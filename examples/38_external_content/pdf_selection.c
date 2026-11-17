#include "pdf_selection.h"

typedef struct selection_request {
    pdf_selection *selection;
    nx_utf8_span text;
} selection_request;
typedef struct selection_clipboard {
    pdf_selection *selection;
    nx_window *window;
} selection_clipboard;

static nx_text_document_state document_state(const pdf_selection *selection, nx_utf8_span text) {
    return (nx_text_document_state){text,selection->identity,selection->revision,
        selection->anchor,selection->cursor,NX_TEXT_DOCUMENT_SELECT|NX_TEXT_DOCUMENT_COPY,1};
}

static int scalar_boundary(nx_utf8_span text, size_t offset) {
    return offset <= text.length &&
        (offset == text.length || ((uint8_t)text.data[offset] & 0xc0u) != 0x80u);
}

static nx_result apply_selection(void *user, const nx_text_document_request *request,
    nx_text_document_state *out) {
    selection_request *current=user;
    pdf_selection *selection=current->selection;
    if (request->operation!=NX_TEXT_DOCUMENT_SET_SELECTION) return NX_ERROR_UNSUPPORTED;
    if (request->identity!=selection->identity || request->revision!=selection->revision ||
        request->start_byte>request->end_byte || !scalar_boundary(current->text,request->start_byte) ||
        !scalar_boundary(current->text,request->end_byte)) return NX_ERROR_STATE;
    if (selection->revision==UINT64_MAX) return NX_ERROR_LIMIT;
    selection->anchor=request->start_byte;
    selection->cursor=request->end_byte;
    selection->dragging=0;
    selection->copy_status=PDF_COPY_IDLE;
    ++selection->revision;
    *out=document_state(selection,current->text);
    return NX_OK;
}

static nx_result select_range(pdf_selection *selection, size_t anchor, size_t cursor, int *redraw) {
    if (selection->anchor==anchor && selection->cursor==cursor) return NX_OK;
    if (selection->revision==UINT64_MAX) return NX_ERROR_LIMIT;
    selection->anchor=anchor;
    selection->cursor=cursor;
    selection->copy_status=PDF_COPY_IDLE;
    ++selection->revision;
    *redraw=1;
    return NX_OK;
}

static int inside(nx_rect box, float x, float y) {
    return x>=box.x && y>=box.y && x<box.x+box.width && y<box.y+box.height;
}

/* On drag, whitespace between words resolves to the nearest real word.
   A press in whitespace has no word and clears selection. */
static const nx_text_document_box *word_at(const nx_text_document_box *boxes,
    size_t count, float x, float y, int nearest) {
    const nx_text_document_box *best=NULL;
    double best_distance=0;
    for (size_t i=0;i<count;++i) {
        nx_rect box=boxes[i].bounds;
        if (inside(box,x,y)) return &boxes[i];
        if (!nearest) continue;
        double dx=x<box.x?box.x-x:x>box.x+box.width?x-box.x-box.width:0;
        double dy=y<box.y?box.y-y:y>box.y+box.height?y-box.y-box.height:0;
        double distance=dx*dx+dy*dy;
        if (!best || distance<best_distance) { best=&boxes[i]; best_distance=distance; }
    }
    return best;
}

static nx_result pointer_selection(pdf_selection *selection, const nx_event *event,
    const nx_text_document_box *boxes, size_t count, int *redraw) {
    if (event->type==NX_EVENT_POINTER_DOWN) {
        const nx_text_document_box *word=word_at(boxes,count,event->x,event->y,0);
        selection->dragging=word!=NULL;
        if (!word) return select_range(selection,selection->cursor,selection->cursor,redraw);
        selection->drag_start=word->start_byte;
        selection->drag_end=word->end_byte;
        return select_range(selection,word->start_byte,word->end_byte,redraw);
    }
    const nx_text_document_box *word=word_at(boxes,count,event->x,event->y,1);
    if (event->type==NX_EVENT_POINTER_UP) selection->dragging=0;
    if (!word) return NX_OK;
    return word->start_byte<selection->drag_start ?
        select_range(selection,selection->drag_end,word->start_byte,redraw) :
        select_range(selection,selection->drag_start,word->end_byte,redraw);
}

static size_t grapheme_step(nx_utf8_span text, size_t cursor, int forward) {
    size_t start=0;
    while (start<text.length) {
        size_t next;
        if (nx_grapheme_next(text.data,text.length,start,&next)!=NX_OK) return cursor;
        if ((forward && next>cursor) || (!forward && next>=cursor)) return forward?next:start;
        start=next;
    }
    return text.length;
}

static nx_result copy_to_window(void *user, const char *bytes, size_t length) {
    selection_clipboard *owner=user;
    nx_result result=nx_window_clipboard_set_text(owner->window,bytes,length);
    owner->selection->copy_status=result==NX_OK?PDF_COPY_SUCCEEDED:PDF_COPY_FAILED;
    return result;
}

static nx_result keyboard_selection(pdf_selection *selection, selection_clipboard *clipboard,
    nx_utf8_span text, const nx_event *event, int *redraw) {
    if (event->modifiers==NX_MOD_CONTROL && event->key==NX_KEY_A)
        return select_range(selection,0,text.length,redraw);
    size_t begin=selection->anchor<selection->cursor?selection->anchor:selection->cursor;
    size_t end=selection->anchor>selection->cursor?selection->anchor:selection->cursor;
    if (event->modifiers==NX_MOD_CONTROL && event->key==NX_KEY_C && begin!=end) {
        (void)copy_to_window(clipboard,text.data+begin,end-begin);
        return NX_OK;
    }
    if (event->modifiers & ~NX_MOD_SHIFT) return NX_OK;
    int extend=(event->modifiers&NX_MOD_SHIFT)!=0;
    size_t cursor=selection->cursor;
    switch (event->key) {
    case NX_KEY_HOME: cursor=0; break;
    case NX_KEY_END: cursor=text.length; break;
    case NX_KEY_LEFT: cursor=!extend&&begin!=end?begin:grapheme_step(text,cursor,0); break;
    case NX_KEY_RIGHT: cursor=!extend&&begin!=end?end:grapheme_step(text,cursor,1); break;
    default: return NX_OK;
    }
    return select_range(selection,extend?selection->anchor:cursor,cursor,redraw);
}

nx_result pdf_selection_record(pdf_selection *selection, nx_context *context,
    nx_window *window, nx_semantic_desc node, uint64_t identity,
    int enabled, nx_utf8_span text, const nx_text_document_box *boxes,
    size_t count, int *redraw) {
    if (selection->identity!=identity) {
        selection->identity=identity;
        selection->revision=1;
        selection->anchor=selection->cursor=0;
        selection->dragging=0;
        selection->copy_status=PDF_COPY_IDLE;
    }
    node.name=(nx_utf8_span){"PDF text selection",18};
    static const char description[] = "Drag over words or hold Shift while moving. Ctrl+C copies the selection.";
    node.description=(nx_utf8_span){description,sizeof(description)-1u};
    node.role=NX_ROLE_TEXT_FIELD;
    node.value=(nx_utf8_span){0};
    node.actions=NX_ACTION_FOCUS;
    if (!enabled) node.state|=NX_SEM_DISABLED;
    nx_widget_desc widget={node,NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE};
    nx_text_document_state state=document_state(selection,text);
    selection_request request={selection,text};
    selection_clipboard clipboard_owner={selection,window};
    nx_text_document_clipboard clipboard={.clipboard={&clipboard_owner,NULL,copy_to_window}};
    nx_text_document_result registered;
    nx_result result=nx_text_document_register_with_clipboard(context,&widget,&state,apply_selection,&request,
        &clipboard,&registered);
    if (result!=NX_OK) return result;
    if (registered.interaction.focused && enabled) {
        result=nx_stroke_rect(context,node.bounds,2,(nx_color){0,0,0,1});
        if (result==NX_OK) result=nx_stroke_rect(context,node.bounds,1,(nx_color){1,1,1,1});
        if (result!=NX_OK) return result;
    }
    if (count) {
        nx_text_document_presentation geometry={.boxes=boxes,.box_count=count};
        result=nx_text_document_set_presentation(context,node.id,identity,selection->revision,&geometry);
        if (result!=NX_OK) return result;
    }
    size_t begin=selection->anchor<selection->cursor?selection->anchor:selection->cursor;
    size_t end=selection->anchor>selection->cursor?selection->anchor:selection->cursor;
    if (begin!=end) {
        for (size_t i=0;i<count;++i) {
            if (boxes[i].end_byte<=begin || boxes[i].start_byte>=end) continue;
            result=nx_fill_rect(context,boxes[i].bounds,(nx_color){0.16f,0.79f,0.95f,0.46f});
            if (result!=NX_OK) return result;
        }
    }
    if (!enabled) { selection->dragging=0;return NX_OK; }
    if (selection->take_focus) {
        selection->take_focus=0;
        *redraw=1;
        result=nx_focus_set(context,node.id);
        if (result!=NX_OK) return result;
    }
    const nx_event *events;
    size_t event_count;
    result=nx_frame_events(context,&events,&event_count);
    if (result!=NX_OK) return result;
    for (size_t i=0;i<event_count;++i) {
        const nx_event *event=&events[i];
        if (event->type==NX_EVENT_POINTER_CANCEL || (event->type==NX_EVENT_FOCUS&&!event->focused))
            selection->dragging=0;
        if (event->type==NX_EVENT_POINTER_DOWN && event->button==NX_BUTTON_LEFT &&
            registered.interaction.pointer_pressed && inside(node.bounds,event->x,event->y))
            result=pointer_selection(selection,event,boxes,count,redraw);
        else if (selection->dragging && (event->type==NX_EVENT_POINTER_MOVE ||
            (event->type==NX_EVENT_POINTER_UP && event->button==NX_BUTTON_LEFT)))
            result=pointer_selection(selection,event,boxes,count,redraw);
        else if (event->type==NX_EVENT_KEY_DOWN && nx_focus_id(context)==node.id)
            result=keyboard_selection(selection,&clipboard_owner,text,event,redraw);
        if (result!=NX_OK) return result;
    }
    if (!registered.interaction.held) selection->dragging=0;
    return NX_OK;
}
