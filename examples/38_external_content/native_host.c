#define _GNU_SOURCE
#include "native_worker.h"
#include <nexaui/software.h>
#include <limits.h>
#include <stdio.h>
#include "native_input.h"
#include "pdf_selection.h"

enum input_phase { INPUT_IDLE, INPUT_PENDING, INPUT_ACTIVE, INPUT_RELEASED, INPUT_FOCUS_LOST, INPUT_DISCONNECTED };
static const char *input_name(enum input_phase phase) {
    static const char *names[]={"Scaled frame ready.","Release keys to enter content.",
        "Input active: Ctrl+Shift+F12 exits.","Input released.","Input focus lost.","Input disconnected; pixels retained."};
    return names[(unsigned)phase];
}
enum phase { PREPARING, WAITING, REPLACED, RELEASED, OLD_REJECTED, FRESH_PENDING, READY,
             BAD_PENDING, BAD_REJECTED, RECOVERY_PENDING, RECOVERED, SCALED_WAIT, SCALED_PENDING,
             SCALED_READY, FAILED, PDF_PAGE1_SCALED, PDF_PAGE_PENDING, PDF_HIT_READY, PDF_HIT_FAILED };
static int pdf_hit_page(enum phase phase) {
    return phase==PDF_HIT_READY || phase==PDF_HIT_FAILED;
}
static int pdf_can_navigate(enum phase phase) {
    return phase==READY || phase==BAD_REJECTED || phase==RECOVERED ||
        phase==SCALED_READY || phase==PDF_PAGE1_SCALED || pdf_hit_page(phase);
}

static const char *phase_name(enum phase phase) {
    static const char *names[] = {"Preparing old frame.","Waiting for frame.","Replacement waiting.",
        "Old frame completing.","Old frame rejected.","Fresh frame pending.","Frame ready.",
        "Bad frame pending.","Frame format unavailable.","Recovery frame pending.","Frame recovered.",
        "Waiting for scaled frame.","Scaled frame pending.","Scaled frame ready.","Producer unavailable.",
        "PDF page 1 of 2 at 2x.","Rendering PDF page.","PDF page ready.","PDF rendering failed; page retained."};
    return names[(unsigned)phase];
}
static uint8_t *read_font(const char *path, size_t *bytes) {
    FILE *file = fopen(path, "rb"); if (!file) return NULL;
    uint8_t *data = NULL;
    if (!fseek(file, 0, SEEK_END)) {
        long length = ftell(file);
        if (length > 0 && length <= 16 * 1024 * 1024 && !fseek(file, 0, SEEK_SET)) {
            data = malloc((size_t)length);
            if (data && fread(data, 1, (size_t)length, file) != (size_t)length) { free(data); data = NULL; }
            if (data) *bytes = (size_t)length;
        }
    }
    fclose(file); return data;
}
static nx_result label(nx_context *context, const nx_font_collection *fonts,
    uint64_t id, const char *text, nx_rect bounds, double em) {
    nx_text_layout *layout = NULL;
    nx_result result = nx_text_layout_create(fonts, text, strlen(text), NX_BIDI_AUTO, em, bounds.width, &layout);
    if (result == NX_OK) {
        nx_point origin = {bounds.x,bounds.y};
        nx_color color = {0.05f,0.07f,0.1f,1};
        result = id ? nx_label(context,id,1,bounds,layout,origin,color,65536) :
            nx_draw_text_layout(context,layout,origin,color,65536);
    }
    nx_text_layout_destroy(layout); return result;
}
static nx_result button(nx_context *context, const nx_font_collection *fonts, uint64_t id,
    const char *text, float x, float y, float width, int enabled, unsigned *action) {
    nx_text_layout *layout = NULL;
    nx_result result = nx_text_layout_create(fonts, text, strlen(text), NX_BIDI_AUTO, 14, width - 12, &layout);
    if (result == NX_OK) {
        nx_control_desc desc = {.id=id,.parent_id=1,.bounds={x,y,width,32},.name={text,strlen(text)},
            .label_layout=layout,.label_origin={x+6,y+8},.glyph_cache_bytes=65536,.disabled=!enabled};
        nx_interaction_state state;
        result = nx_button(context, &desc, &state);
        if (result == NX_OK && state.clicked && enabled) *action = (unsigned)id;
    }
    nx_text_layout_destroy(layout); return result;
}
static nx_result input_hint(nx_context *context,const nx_font_collection *fonts) {
    return label(context,fonts,3,"Physical input fixture; exit: Ctrl+Shift+F12",(nx_rect){16,122,448,14},11);
}
typedef struct pdf_text_view {
    uint64_t generation, resize, sequence, identity;
    int selectable;
} pdf_text_view;
enum { PDF_SELECT_BUTTON=36, PDF_TEXT_NODE=101 };

static nx_rect word_bounds(nx_rect view, nx_rect word) {
    return (nx_rect){view.x + word.x * view.width, view.y + word.y * view.height,
        word.width * view.width, word.height * view.height};
}

static nx_result pdf_text_semantics(nx_context *context, content_surface *surface,
    const nx_viewport_desc *viewport, pdf_text_view *text_view, pdf_selection *selection,
    nx_window *window, int enabled, int *redraw) {
    content_surface_status status;
    nx_result result = content_surface_get_status(surface, &status);
    if (result != NX_OK) return result;
    if (text_view->generation != status.request.generation || text_view->resize != status.request.resize ||
        text_view->sequence != status.sequence || text_view->selectable!=selection->active) {
        if (text_view->identity == UINT64_MAX) return NX_ERROR_LIMIT;
        *text_view = (pdf_text_view){status.request.generation,status.request.resize,status.sequence,
            text_view->identity+1,selection->active};
    }
    nx_utf8_span text;
    result = content_surface_get_text(surface, &text);
    if (result != NX_OK || !text.length) return result;
    nx_semantic_desc node = {.id=PDF_TEXT_NODE, .parent_id=viewport->id, .role=NX_ROLE_TEXT, .bounds=viewport->bounds,
        .name={"PDF page text",13}, .value=text, .state=NX_SEM_READONLY};
    const content_frame_word *words;
    size_t word_count;
    result = content_surface_get_words(surface, &words, &word_count);
    if (result != NX_OK) return result;
    nx_text_document_box boxes[CONTENT_FRAME_WORD_LIMIT];
    for (size_t i = 0; i < word_count; ++i)
        boxes[i] = (nx_text_document_box){words[i].start_byte, words[i].end_byte,
            word_bounds(viewport->bounds,words[i].bounds)};
    if (selection->active) return pdf_selection_record(selection,context,window,node,text_view->identity,
        enabled,text,boxes,word_count,redraw);
    result = nx_semantics_add_readonly_text_snapshot_without_caret(context,&node,text_view->identity,NULL);
    if (result != NX_OK || !word_count) return result;
    nx_text_document_presentation presentation = {.boxes=boxes, .box_count=word_count};
    return nx_text_document_set_presentation(context, node.id, text_view->identity, 0, &presentation);
}

static nx_result pdf_link_widgets(nx_context *context, content_surface *surface,
    const nx_viewport_desc *viewport, uint64_t frame_identity, int enabled, unsigned *destination) {
    const content_frame_link *links;
    size_t count;
    nx_result result = content_surface_get_links(surface,&links,&count);
    if (result != NX_OK || !count) return result;
    if (frame_identity >= (UINT64_MAX-1024u)/CONTENT_PDF_LINK_LIMIT) return NX_ERROR_LIMIT;
    uint64_t first_id=1024u+frame_identity*CONTENT_PDF_LINK_LIMIT;
    for (size_t i=0;i<count;++i) {
        char title[64];
        snprintf(title,sizeof(title),"PDF link to page %u",links[i].target_page);
        nx_rect bounds=word_bounds(viewport->bounds,links[i].bounds);
        nx_widget_desc widget = {
            .semantic={.id=first_id+i,.parent_id=viewport->id,.role=NX_ROLE_BUTTON,
                .bounds=bounds,.name={title,strlen(title)},.state=enabled?0:NX_SEM_DISABLED,
                .actions=NX_ACTION_FOCUS|NX_ACTION_PRESS},
            .flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE};
        nx_interaction_state state;
        result=nx_widget_register(context,&widget,&state);
        if (result != NX_OK) return result;
        if (enabled && state.clicked) *destination=links[i].target_page;
        if (enabled && (state.focused || state.hovered)) {
            result=nx_stroke_rect(context,bounds,2,(nx_color){0,0,0,1});
            if (result==NX_OK) result=nx_stroke_rect(context,bounds,1,(nx_color){1,1,1,1});
            if (result != NX_OK) return result;
        }
    }
    return NX_OK;
}

enum { PDF_FIND_FIELD = 32, PDF_NEXT_HIT = 34 };
typedef struct pdf_document_view {
    content_pdf_index *index;
    unsigned page, pages, counted_page;
    int index_finished;
    content_pdf_hits hits;
    size_t counted_length;
    char counted_query[CONTENT_PDF_QUERY_LIMIT];
} pdf_document_view;

static int find_visible(const nx_text_field *field, enum phase phase, float width) {
    return field && phase >= READY && phase != FAILED && width >= 256;
}

static int find_query_valid(nx_utf8_span text, void *user) {
    (void)user;
    return text.length <= CONTENT_PDF_QUERY_LIMIT;
}

static nx_result pdf_find_shortcut(nx_context *context, int leased, int *find_pressed, int *redraw) {
    const nx_event *events;
    size_t count;
    nx_result result = nx_frame_events(context,&events,&count);
    if (result != NX_OK) return result;
    for (size_t i = 0; i < count; ++i) {
        const nx_event *event = &events[i];
        if (event->type == NX_EVENT_FOCUS && !event->focused) *find_pressed = 0;
        /* Activate the field after F is released. Activating a v1 input
           method mid-chord can deliver the old release to the new editor. */
        if (event->type == NX_EVENT_KEY_DOWN && event->key == NX_KEY_F &&
            event->modifiers == NX_MOD_CONTROL && nx_focus_id(context)!=PDF_FIND_FIELD)
            *find_pressed = 1;
        if (event->type == NX_EVENT_KEY_UP && event->key == NX_KEY_F && *find_pressed) {
            *find_pressed = 0;
            *redraw = 1;
            return nx_focus_set(context,PDF_FIND_FIELD);
        }
        if (event->type == NX_EVENT_KEY_DOWN && event->key == NX_KEY_ESCAPE &&
            nx_focus_id(context)==PDF_FIND_FIELD) {
            *redraw = 1;
            return nx_focus_set(context,leased?100u:15u);
        }
    }
    return NX_OK;
}

static nx_result pdf_find_field(nx_context *context, nx_window *window,
    const nx_font_collection *fonts, nx_text_field *field) {
    nx_text_field_desc desc = {.id=PDF_FIND_FIELD, .parent_id=1, .bounds={104,48,144,32},
        .name={"Find in PDF",11}, .fonts=fonts, .pixels_per_em=14, .glyph_cache_bytes=65536,
        .validate=find_query_valid, .ime_window=window};
    nx_result result = nx_text_clipboard_for_window(window,&desc.clipboard);
    if (result != NX_OK) return result;
    result = label(context,fonts,0,"Find in PDF",(nx_rect){16,57,88,18},13);
    if (result != NX_OK) return result;
    nx_interaction_state interaction;
    int valid, submitted;
    static const char description[] = "Case-sensitive search. Next hit moves to another matching page.";
    return nx_text_field_widget_described(context,&desc,field,
        (nx_utf8_span){description,sizeof(description)-1u},&interaction,&valid,&submitted);
}

static nx_result pdf_status_label(nx_context *context, const nx_font_collection *fonts,
    uint64_t id, nx_rect bounds, const char *message) {
    nx_semantic_desc status = {.id=id,.parent_id=1,.role=NX_ROLE_STATUS,.bounds=bounds,
        .name={message,strlen(message)}};
    nx_result result = nx_semantics_add(context,&status,NULL);
    return result == NX_OK ? label(context,fonts,0,message,bounds,11) : result;
}

#ifdef CONTENT_NATIVE_PDF
static nx_result pdf_document_find(nx_context *context, const nx_font_collection *fonts,
    const nx_text_field *field, pdf_document_view *document, float width) {
    nx_utf8_span query;
    nx_result result = nx_text_field_text(field,&query);
    if (result != NX_OK) return result;
    if (!query.length || query.length > CONTENT_PDF_QUERY_LIMIT) {
        document->counted_length = 0;
        document->hits = (content_pdf_hits){0};
        return NX_OK;
    }
    const char *message = document->index_finished ? "Document search unavailable" : "Indexing document";
    char summary[64];
    if (document->index && document->page && document->page <= document->pages) {
        if (document->counted_page != document->page || document->counted_length != query.length ||
            memcmp(document->counted_query,query.data,query.length)) {
            result = content_pdf_index_find(document->index,query,document->page,&document->hits);
            if (result != NX_OK) return result;
            document->counted_page = document->page;
            document->counted_length = query.length;
            memcpy(document->counted_query,query.data,query.length);
        }
        snprintf(summary,sizeof(summary),"%zu %s in document",document->hits.total,
            document->hits.total==1?"match":"matches");
        message = summary;
    } else document->hits = (content_pdf_hits){0};
    float split = width * .58f;
    return pdf_status_label(context,fonts,35,(nx_rect){16+split,122,width-split,14},message);
}
#endif

static nx_result pdf_find_results(nx_context *context, const nx_font_collection *fonts,
    nx_text_field *field, const content_surface *surface, nx_rect view) {
    nx_utf8_span query, text = {0};
    nx_result result = nx_text_field_text(field,&query);
    if (result != NX_OK || !query.length) return result;
    const content_frame_word *words = NULL;
    size_t word_count = 0;
    if (surface) {
        result = content_surface_get_text(surface,&text);
        if (result != NX_OK) return result;
        result = content_surface_get_words(surface,&words,&word_count);
        if (result != NX_OK) return result;
    }
    const char *message = "Page text unavailable";
    char summary[96];
    if (query.length > CONTENT_PDF_QUERY_LIMIT) {
        message = "Search text exceeds 128 bytes";
    } else if (text.length) {
        size_t matches = 0, next_word = 0, highlighted = 0;
        /* Both inputs are validated UTF-8. A literal match begins and ends
           at scalar boundaries. Use non-overlapping occurrences. */
        for (size_t start = 0; query.length <= text.length - start;) {
            if (memcmp(text.data + start,query.data,query.length)) {
                ++start;
                continue;
            }
            ++matches;
            size_t end = start + query.length;
            while (next_word < word_count && words[next_word].start_byte < end) {
                const content_frame_word *word = &words[next_word++];
                if (word->end_byte <= start) continue;
                result = nx_fill_rect(context,word_bounds(view,word->bounds),(nx_color){1,0.73f,0.14f,0.42f});
                if (result != NX_OK) return result;
                ++highlighted;
            }
            start = end;
        }
        snprintf(summary,sizeof(summary),"%zu %s on page%s",matches,matches==1?"match":"matches",
            matches && !highlighted ? " (highlight unavailable)" : "");
        message = summary;
    }
    return pdf_status_label(context,fonts,33,(nx_rect){16,122,view.width*.58f,14},message);
}

static int frame(nx_context *context, nx_window_info info, const nx_font_collection *fonts,
    content_surface *surface, double scale, enum phase phase, int focus, unsigned *action,
    nx_result *content_result, int *leased, int input_case, enum input_phase input_phase, int png_case, int pdf_case,
    pdf_text_view *text_view, nx_window *window, nx_text_field *query, pdf_document_view *document,
    unsigned *linked_page, pdf_selection *selection, int *redraw) {
    *action = 0; *leased = 0; *linked_page=0;
    if (nx_begin_frame(context, info.width, info.height, 0) != NX_OK) return 0;
    nx_semantic_desc root = {.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,info.width,info.height},
        .name={"External CPU content fixture",28}};
    int input_controls=input_case&&phase==SCALED_READY;
    const char *status=png_case&&phase==BAD_REJECTED?"PNG decoding failed; image retained.":phase_name(phase);
    char pdf_status[64];
    if (pdf_case) {
        if (phase==READY) status="PDF page 1 of 2.";
        if (phase==BAD_REJECTED) status="PDF rendering failed; page retained.";
        if (phase==RECOVERED) status="PDF page 2 of 2.";
        if (phase==SCALED_READY) status="PDF page 2 of 2 at 2x.";
        if (phase==PDF_HIT_READY) {
            const char *zoom=scale==2?" at 2x":"";
            if (document->pages)
                snprintf(pdf_status,sizeof(pdf_status),"PDF page %u of %u%s.",document->page,document->pages,zoom);
            else snprintf(pdf_status,sizeof(pdf_status),"PDF page %u%s.",document->page,zoom);
            status=pdf_status;
        }
    }
    int search = find_visible(query,phase,info.width);
    int hit_page = pdf_case&&pdf_hit_page(phase);
    if (nx_semantics_add(context, &root, NULL) != NX_OK || label(context, fonts, 2,
        input_controls?input_name(input_phase):status, (nx_rect){16,12,info.width-32,24}, 15) != NX_OK ||
        (search && pdf_find_field(context,window,fonts,query)!=NX_OK) ||
        (!search && (button(context,fonts,input_controls?16u:10u,input_controls?"Enter content":"Replace tab",16,48,112,
            input_controls?input_phase!=INPUT_DISCONNECTED:phase==WAITING,action)!=NX_OK ||
        button(context,fonts,input_controls?17u:11u,input_controls?"Release input":"Release old",136,48,112,
            input_controls?input_phase==INPUT_ACTIVE||input_phase==INPUT_PENDING:phase==REPLACED,action)!=NX_OK)) ||
        button(context,fonts,input_controls?18u:12u,input_controls?"Disconnect":
            pdf_case&&(phase==BAD_REJECTED||phase==PDF_PAGE1_SCALED||hit_page)?"Next page":"Publish frame",256,48,128,
            input_controls?input_phase!=INPUT_DISCONNECTED:phase==OLD_REJECTED||phase==BAD_REJECTED||
            phase==SCALED_WAIT||phase==PDF_PAGE1_SCALED||(hit_page&&document->page<document->pages),action)!=NX_OK ||
        button(context,fonts,13,pdf_case&&(phase==SCALED_READY||hit_page)?"Previous page":"Bad frame",16,88,112,
            phase==READY||(pdf_case&&phase==SCALED_READY)||(hit_page&&document->page>1u),action)!=NX_OK ||
        button(context,fonts,14,"Scale 2x",136,88,112,phase==RECOVERED||(hit_page&&scale==1),action)!=NX_OK ||
        button(context,fonts,15,"Close",256,88,pdf_case&&info.width>=480?112:128,1,action)!=NX_OK) return 0;
    if (pdf_case&&info.width>=480&&pdf_can_navigate(phase)) {
        nx_utf8_span text;
        if (content_surface_get_text(surface,&text)!=NX_OK ||
            button(context,fonts,PDF_SELECT_BUTTON,selection->active?"Done":"Select text",376,88,88,
                text.length!=0,action)!=NX_OK) return 0;
    }
    if(input_controls&&input_hint(context,fonts)!=NX_OK) return 0;
#ifdef CONTENT_NATIVE_PDF
    if (search && pdf_document_find(context,fonts,query,document,info.width-32)!=NX_OK) return 0;
    if (search && document->hits.next_page && info.width>=480) {
        if (button(context,fonts,PDF_NEXT_HIT,"Next hit",392,48,72,pdf_can_navigate(phase),action)!=NX_OK) return 0;
    }
#endif
    const char *name=pdf_case?"PDF page image":"Fixture frame";
    nx_viewport_desc desc = {.id=100,.parent_id=1,.bounds={16,136,info.width>32?info.width-32:0,info.height>152?info.height-152:0},
        .name={name,strlen(name)},.scale=scale};
    nx_interaction_state interaction;
    *content_result = content_surface_draw(surface, context, &desc, &interaction);
    *leased = *content_result == NX_OK && desc.bounds.width>0 && desc.bounds.height>0;
    if (pdf_case && *leased && pdf_text_semantics(context,surface,&desc,text_view,selection,window,
        pdf_can_navigate(phase),redraw)!=NX_OK) return 0;
    if (pdf_case&&*leased&&selection->active&&selection->copy_status!=PDF_COPY_IDLE&&info.width>=480 &&
        pdf_status_label(context,fonts,37,(nx_rect){296,15,info.width-312,16},
            selection->copy_status==PDF_COPY_SUCCEEDED?"Selection copied":"Copy unavailable")!=NX_OK) return 0;
    if (*content_result != NX_OK && nx_fill_rect(context, desc.bounds,
        (nx_color){227.0f/255.0f,231.0f/255.0f,239.0f/255.0f,1}) != NX_OK) return 0;
    if (search && pdf_find_results(context,fonts,query,*leased?surface:NULL,desc.bounds)!=NX_OK) return 0;
    if (pdf_case && *leased && pdf_link_widgets(context,surface,&desc,text_view->identity,
        pdf_can_navigate(phase)&&!selection->active,linked_page)!=NX_OK) return 0;
    if (focus) {
        uint64_t id = phase==WAITING?10u:phase==REPLACED?11u:
            phase==OLD_REJECTED||phase==BAD_REJECTED||phase==SCALED_WAIT?12u:
            phase==READY?13u:phase==RECOVERED?14u:15u;
        if (pdf_case&&phase==SCALED_READY) id=13u;
        if (phase==PDF_PAGE1_SCALED) id=12u;
        if (hit_page&&*leased) id=100u;
        if (nx_focus_id(context)!=PDF_FIND_FIELD && nx_focus_set(context,id)!=NX_OK) return 0;
    }
    return nx_end_frame(context)==NX_OK;
}

int main(void) {
    const char *isolated=getenv("NEXAUI_TEST_PRIVATE_WAYLAND"),*display=getenv("WAYLAND_DISPLAY");
    const char *path=getenv("NEXAUI_CONTENT_TEST_FONT"),*test_case=getenv("NEXAUI_CONTENT_TEST_CASE");
    if (!isolated||strcmp(isolated,"1")||!display||strcmp(display,"nexaui-test")||
        !getenv("NEXAUI_TEST_ATSPI_ADDRESS")||!path||!test_case||
        (strcmp(test_case,"full")&&strcmp(test_case,"png")&&strcmp(test_case,"pdf")&&strcmp(test_case,"input")&&
        strcmp(test_case,"pending-close")&&strcmp(test_case,"rejected-close"))) return 2;
    int png_case=!strcmp(test_case,"png");
    int pdf_case=!strcmp(test_case,"pdf");
    int pdf_select_case=pdf_case&&getenv("NEXAUI_CONTENT_TEST_PDF_SELECT");
    int pdf_compose=pdf_case&&getenv("NEXAUI_CONTENT_PDF_COMPOSE");
    const char *encoded_dir=(png_case||pdf_case)?getenv("NEXAUI_CONTENT_TEST_ENCODED"):NULL;
    const char *pdf_renderer=pdf_case?getenv("NEXAUI_CONTENT_TEST_PDF_RENDERER"):NULL;
    const char *pdf_reader=pdf_case?getenv("NEXAUI_CONTENT_TEST_PDF_READER"):NULL;
    const char *pdf_link_reader=pdf_case?getenv("NEXAUI_CONTENT_TEST_PDF_LINK_READER"):NULL;
#ifndef CONTENT_NATIVE_PNG
    if (png_case) return 2;
#endif
#ifndef CONTENT_NATIVE_PDF
    if (pdf_case) return 2;
#endif
    if (((png_case||pdf_case)&&!encoded_dir)||(pdf_case&&(!pdf_renderer||!pdf_reader))) return 2;
    int result=1,leased=0,worker_ready=0;
    nx_snapshot_exchange *exchange=NULL; content_surface *surface=NULL;
    nx_window *window=NULL; nx_software_view *software=NULL; nx_context *context=NULL;
    nx_font *font=NULL; nx_font_collection *fonts=NULL;
    content_input *input=NULL; native_input_receiver receiver={.generation=1};
    int input_case=!strcmp(test_case,"input"); enum input_phase input_phase=INPUT_IDLE;
    native_worker worker={0}; worker.wake[0]=worker.wake[1]=-1;
    size_t bytes=0; uint8_t *data=read_font(path,&bytes);
    enum phase phase=PREPARING,last_phase=FAILED;
    unsigned frames=0,action=0,linked_page=0; int need_frame=1,initial_job=0; double scale=1;
    unsigned nav_completion=0,pending_page=0,scaled_search_completion=0; enum phase nav_target=FAILED;
    uint64_t nav_sequence=0;
    pdf_document_view document={0};
    pdf_text_view text_view={0};
    pdf_selection selection={0};
    nx_text_field *query=NULL;
    int logged_preedit=0,find_pressed=0;
    uint64_t logged_generation=0,logged_resize=0,logged_sequence=0;
    const nx_window_desc desc={.title_utf8="External CPU content fixture",.width=480,.height=360};
    if (!data||nx_font_open(data,bytes,NULL,&font)!=NX_OK||nx_font_collection_create(NULL,&fonts)!=NX_OK||
        nx_font_collection_add(fonts,font)!=NX_OK||nx_snapshot_exchange_create(NULL,&exchange)!=NX_OK||
        content_surface_create(exchange,1,4u*1024u*1024u,NULL,&surface)!=NX_OK||
        nx_context_create(NULL,&context)!=NX_OK||nx_window_create(&desc,&window)!=NX_OK||
        (pdf_case&&nx_text_field_create("",0,4096,&query)!=NX_OK)||
        nx_software_view_open(window,&software)!=NX_OK||
        native_worker_open(&worker,exchange,encoded_dir,pdf_renderer,pdf_reader,pdf_link_reader)) goto done;
    worker_ready=1;
    if(input_case&&content_input_create(1,NX_KEY_F12,NX_MOD_CONTROL|NX_MOD_SHIFT,native_input_receive,&receiver,&input)!=NX_OK) goto done;
    for (;;) {
        uint32_t ready=0; size_t routed=0; nx_window_info info;
        if (nx_window_poll_with_fd_events(window,need_frame?0:200,worker.wake[0],NX_WINDOW_FD_READABLE,&ready)!=NX_OK||
            nx_window_get_info(window,&info)!=NX_OK) goto done;
        if(input) {
            /* Drain native events first; pending native AX actions still use
               route_events below, in their ordinary native-then-AX order. */
            for(;;) {
                nx_event event,host; int available=0,route=0;
                if(nx_window_next_event(window,&event,&available)!=NX_OK) goto done;
                if(!available) break;
                ++routed;
                if(event.type==NX_EVENT_FOCUS||event.type==NX_EVENT_KEY_DOWN||event.type==NX_EVENT_KEY_UP||event.type==NX_EVENT_POINTER_CANCEL) {
                    printf("INPUT_NATIVE type=%d key=%d focus=%d repeat=%d\n",(int)event.type,(int)event.key,event.focused,event.repeat);fflush(stdout);
                }
                content_input_status before,after;
                if(content_input_get_status(input,&before)!=NX_OK||
                    content_input_event(input,&event,&host,&route)!=NX_OK||
                    content_input_get_status(input,&after)!=NX_OK) goto done;
                if(route&&nx_context_queue_event(context,&host)!=NX_OK) goto done;
                if(before.active&&!after.active) {
                    input_phase=event.type==NX_EVENT_FOCUS&&!event.focused?INPUT_FOCUS_LOST:INPUT_RELEASED;
                    if(nx_focus_set(context,15)!=NX_OK) goto done;
                    need_frame=1;
                }
                /* Some backends deliver pointer cancellation before FocusOut.
                   Preserve the actual focus-loss reason after that release. */
                if(event.type==NX_EVENT_FOCUS&&!event.focused&&phase==SCALED_READY&&input_phase!=INPUT_DISCONNECTED) {
                    input_phase=INPUT_FOCUS_LOST;need_frame=1;
                }
            }
        }
        size_t ax_routed=0;
        if(nx_window_route_events(window,context,&ax_routed)!=NX_OK) goto done;
        routed+=ax_routed;
        if (ready) {
            char wake[64]; ssize_t count;
            do { count=read(worker.wake[0],wake,sizeof(wake)); } while (count>0||(count<0&&errno==EINTR));
            if (count<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK) goto done;
        }
        native_worker_state state=native_state(&worker);
#ifdef CONTENT_NATIVE_PDF
        if (pdf_case) {
            native_pdf_index_result available = native_take_pdf_index(&worker);
            if (available.generation) {
                content_surface_status current;
                if (content_surface_get_status(surface,&current)!=NX_OK) {
                    content_pdf_index_destroy(available.index);
                    goto done;
                }
                if (available.generation==current.request.generation) {
                    content_pdf_index_destroy(document.index);
                    document.index=available.index;
                    document.index_finished=1;
                    document.pages=content_pdf_index_pages(available.index);
                    document.counted_length=0;
                    need_frame=1;
                } else {
                    if (available.index) {
                        printf("PDF_INDEX stale generation=%llu current=%llu\n",
                            (unsigned long long)available.generation,
                            (unsigned long long)current.request.generation);
                        fflush(stdout);
                    }
                    content_pdf_index_destroy(available.index);
                }
            }
        }
#endif
        if (phase==PREPARING&&state.prepared) { phase=WAITING; need_frame=1; }
        int decode_rejected=(png_case||pdf_case)&&state.completed==3&&state.result==NX_ERROR_ARGUMENT;
        int navigation_error=pdf_case&&(phase==PDF_PAGE_PENDING||phase==PDF_HIT_FAILED)&&
            state.completed==nav_completion&&state.result!=NX_OK;
        if (state.result!=NX_OK&&!decode_rejected&&!navigation_error&&phase!=FAILED) { phase=FAILED; need_frame=1; }
        if (info.should_close) { result=0; break; }
        if (!need_frame&&!routed&&!ready) continue;
        need_frame=0;
        enum phase drawn=phase; nx_result content_result=NX_ERROR_STATE;
        if (++frames>150||!frame(context,info,fonts,surface,scale,phase,phase!=last_phase,&action,
            &content_result,&leased,input_case,input_phase,png_case,pdf_case,&text_view,window,query,&document,
            &linked_page,&selection,&need_frame)) goto done;
        int search_focused=nx_focus_id(context)==PDF_FIND_FIELD;
        if (find_visible(query,drawn,info.width)) {
            if (pdf_find_shortcut(context,leased,&find_pressed,&need_frame)!=NX_OK) goto done;
        } else find_pressed=0;
        if (phase!=last_phase) need_frame=1;
        last_phase=drawn;
        if (!search_focused&&nx_key_pressed(context,NX_KEY_ESCAPE)) {
            if (!selection.active || nx_focus_id(context)!=PDF_TEXT_NODE) { result=0; break; }
            selection.active=selection.dragging=selection.take_focus=0;
            if (nx_focus_set(context,leased?100u:15u)!=NX_OK) goto done;
            need_frame=1;
        }
        if (pdf_compose&&!logged_preedit&&search_focused) {
            size_t bytes=0;
            int32_t begin=0,end=0;
            if (nx_window_get_ime_preedit(window,NULL,0,&bytes,&begin,&end)==NX_OK&&bytes) {
                puts("PDF_FIND_PREEDIT");
                fflush(stdout);
                logged_preedit=1;
            }
        }
        if (phase==RELEASED&&state.completed==1) {
            phase=content_result==NX_ERROR_STATE?OLD_REJECTED:FAILED; need_frame=1;
        }
        if ((phase==FRESH_PENDING&&state.completed==2)||(phase==RECOVERY_PENDING&&state.completed==4)||
            (phase==SCALED_PENDING&&state.completed==(scaled_search_completion?scaled_search_completion:5u))) {
            if (content_result!=NX_OK) phase=FAILED;
            else {
                if (pdf_case && phase!=SCALED_PENDING) document.page=phase==FRESH_PENDING?1u:2u;
                phase=phase==FRESH_PENDING?READY:phase==RECOVERY_PENDING?RECOVERED:
                    scaled_search_completion?PDF_HIT_READY:SCALED_READY;
            }
            scaled_search_completion=0;
            need_frame=1;
        }
        if (phase==BAD_PENDING&&state.completed==3) {
            int rejected=(png_case||pdf_case)?decode_rejected&&content_result==NX_OK:content_result==NX_ERROR_UNSUPPORTED;
            phase=rejected?BAD_REJECTED:FAILED; need_frame=1;
        }
        if (phase==PDF_PAGE_PENDING&&state.completed==nav_completion) {
            content_surface_status accepted;
            if (content_surface_get_status(surface,&accepted)!=NX_OK) goto done;
            if (state.result==NX_OK && content_result==NX_OK && accepted.sequence==nav_sequence) {
                document.page=pending_page;
                phase=nav_target;
            } else phase=content_result==NX_OK?PDF_HIT_FAILED:FAILED;
            need_frame=1;
        }
        if (nx_window_publish_semantics(window,context)!=NX_OK) goto done;
        const nx_command *commands=NULL; size_t count=0; nx_software_view_outcome outcome=NX_SOFTWARE_VIEW_READY;
        if (nx_frame_commands(context,&commands,&count)!=NX_OK||
            nx_software_view_render(software,commands,count,(nx_color){1,1,1,1},&outcome)!=NX_OK) goto done;
        /* The presenter borrows commands only until return. Discard this
           stream after completing its lease; BUSY/SUSPENDED rebuild later. */
        if (leased) { if (content_surface_complete(surface)!=NX_OK) goto done; leased=0; }
        if (outcome!=NX_SOFTWARE_VIEW_READY) need_frame=1;
        content_surface_status status;
        if (content_surface_get_status(surface,&status)!=NX_OK) goto done;
        if (status.request.generation!=logged_generation||status.request.resize!=logged_resize||status.sequence!=logged_sequence) {
            printf("CONTENT_STATE generation=%llu resize=%llu width=%u height=%u sequence=%llu copied=%llu copy_bytes=%zu\n",
                (unsigned long long)status.request.generation,(unsigned long long)status.request.resize,
                status.request.width,status.request.height,(unsigned long long)status.sequence,
                (unsigned long long)status.copied_frames,status.last_copy_bytes);
            fflush(stdout);
            logged_generation=status.request.generation;logged_resize=status.request.resize;logged_sequence=status.sequence;
        }
        if (!initial_job&&status.request.width&&status.request.height) {
            if (!native_schedule(&worker,(native_job){.request=status.request,.sequence=1,.held=1})) goto done;
            initial_job=1;
        }
        if (action==15) { result=0; break; }
        if (action==PDF_SELECT_BUTTON) {
            selection.active=!selection.active;
            selection.take_focus=selection.active;
            selection.dragging=0;
            need_frame=1;
        } else if (action==10&&phase==WAITING) {
            if (content_surface_destroy(&surface)!=NX_OK||content_surface_create(exchange,2,4u*1024u*1024u,NULL,&surface)!=NX_OK) goto done;
#ifdef CONTENT_NATIVE_PDF
            content_pdf_index_destroy(document.index);
            document=(pdf_document_view){0};
#endif
            if(input) { if(content_input_rebind(input,2)!=NX_OK) goto done; receiver.generation=2; }
            selection=(pdf_selection){0};
            phase=REPLACED; need_frame=1;
        } else if (action==11&&phase==REPLACED) { native_release(&worker); phase=RELEASED; need_frame=1; }
        else if (action==12&&(phase==OLD_REJECTED||phase==BAD_REJECTED||phase==SCALED_WAIT)) {
            native_job job={.request=status.request,.sequence=phase==BAD_REJECTED?3u:1u,
                .kind=phase==BAD_REJECTED?3u:phase==SCALED_WAIT?4u:1u,
                .page=scaled_search_completion?document.page:0};
            if (!native_schedule(&worker,job)) goto done;
            phase=phase==OLD_REJECTED?FRESH_PENDING:phase==BAD_REJECTED?RECOVERY_PENDING:SCALED_PENDING; need_frame=1;
        } else if (action==13&&phase==READY) {
            if (!native_schedule(&worker,(native_job){.request=status.request,.sequence=2,.kind=2})) goto done;
            phase=BAD_PENDING; need_frame=1;
        } else if (action==14&&(phase==RECOVERED||(pdf_case&&pdf_hit_page(phase)))) {
            if (state.completed==UINT_MAX) goto done;
            if (pdf_case&&pdf_hit_page(phase)) scaled_search_completion=state.completed+1;
            scale=2; phase=SCALED_WAIT; need_frame=1;
        }
        else if (pdf_case&&((action==13&&phase==SCALED_READY)||(action==12&&phase==PDF_PAGE1_SCALED)||
            (pdf_hit_page(phase)&&((action==12&&document.page<document.pages)||(action==13&&document.page>1u)))||
            (action==PDF_NEXT_HIT&&document.hits.next_page&&content_result==NX_OK)||
            (linked_page&&content_result==NX_OK))) {
            if (status.sequence==UINT64_MAX||state.completed==UINT_MAX) goto done;
            int from_hits=pdf_hit_page(phase);
            nav_target=linked_page||action==PDF_NEXT_HIT||from_hits?PDF_HIT_READY:action==13?PDF_PAGE1_SCALED:SCALED_READY;
            pending_page=linked_page?linked_page:action==PDF_NEXT_HIT?document.hits.next_page:
                action==13?document.page-1u:document.page+1u;
            nav_sequence=status.sequence+1;
            native_job job={.request=status.request,.sequence=nav_sequence,.kind=5,.page=pending_page};
            if (!native_schedule(&worker,job)) goto done;
            nav_completion=state.completed+1; phase=PDF_PAGE_PENDING; need_frame=1;
        }
        if(input) {
            if(action==16&&phase==SCALED_READY&&input_phase!=INPUT_DISCONNECTED&&input_phase!=INPUT_ACTIVE) {
                input_phase=INPUT_PENDING;
                if(nx_focus_set(context,100)!=NX_OK) goto done;
                need_frame=1;
            } else if(action==17&&(input_phase==INPUT_ACTIVE||input_phase==INPUT_PENDING)) {
                if(content_input_release(input)!=NX_OK||nx_focus_set(context,15)!=NX_OK) goto done;
                input_phase=INPUT_RELEASED;need_frame=1;
            } else if(action==18&&input_phase!=INPUT_DISCONNECTED) {
                if(content_input_release(input)!=NX_OK||nx_focus_set(context,15)!=NX_OK) goto done;
                input_phase=INPUT_DISCONNECTED;need_frame=1;
            }
            content_input_target target={.bounds={16,136,info.width>32?info.width-32:0,info.height>152?info.height-152:0},
                .width=status.request.width,.height=status.request.height,
                .available=phase==SCALED_READY&&input_phase!=INPUT_DISCONNECTED,
                .focused=nx_focus_id(context)==100};
            content_input_status before,after;
            if(content_input_get_status(input,&before)!=NX_OK||content_input_set_target(input,&target)!=NX_OK||
                content_input_get_status(input,&after)!=NX_OK) goto done;
            if(before.active&&!after.active) { input_phase=INPUT_RELEASED;need_frame=1; }
            if(input_phase==INPUT_PENDING) {
                nx_result armed=content_input_arm(input);
                printf("INPUT_ARM result=%d focus=%llu window=%d keys=%zu buttons=%u draining=%zu/%u\n",(int)armed,
                    (unsigned long long)nx_focus_id(context),after.window_focused,after.physical_keys,after.physical_buttons,
                    after.draining_keys,after.draining_buttons);fflush(stdout);
                if(armed==NX_OK) { input_phase=INPUT_ACTIVE;need_frame=1; }
                else if(armed!=NX_ERROR_STATE) goto done;
            }
        }
    }
done:
    if(content_input_close(&input)!=NX_OK) result=1;
    if (leased&&content_surface_complete(surface)!=NX_OK) result=1;
    if (window&&nx_window_publish_semantics(window,NULL)!=NX_OK) result=1;
    if (software&&nx_software_view_close(&software)!=NX_OK) result=1;
    if (content_surface_destroy(&surface)!=NX_OK) result=1;
    nx_context_destroy(context);
    nx_text_field_destroy(query);
#ifdef CONTENT_NATIVE_PDF
    content_pdf_index_destroy(document.index);
#endif
    if (native_worker_close(&worker)) result=1;
    if (exchange&&nx_snapshot_exchange_destroy(exchange)!=NX_OK) result=1;
    if (window) nx_window_destroy(window);
    nx_font_collection_destroy(fonts); nx_font_close(font); free(data);
    unsigned expected=(!strcmp(test_case,"full")||input_case||png_case)?5u:!strcmp(test_case,"rejected-close")?1u:0u;
    if (pdf_case) expected=pdf_select_case?4u:pdf_link_reader?10u:8u;
    unsigned published=expected-(unsigned)(png_case||pdf_case);
    unsigned discarded=!strcmp(test_case,"pending-close")?1u:0u;
    if (!worker_ready||worker.completed!=expected||worker.publications!=published||worker.discarded!=discarded||
        atomic_load(&worker.disposals)!=published||((expected==5u)&&phase!=SCALED_READY)||
        (pdf_case&&(pdf_select_case?(phase!=RECOVERED||document.page!=2u):
            (phase!=PDF_HIT_READY||document.page!=1u)))||
        (!strcmp(test_case,"rejected-close")&&phase!=OLD_REJECTED)||(!strcmp(test_case,"pending-close")&&phase!=WAITING)) result=1;
    if(input_case) {
        if(receiver.downs!=6||receiver.ups!=6||receiver.pointer_downs!=3||receiver.pointer_ups!=3||
            receiver.enters!=3||receiver.exits!=3||receiver.texts||receiver.buttons||native_input_keys(&receiver)||
            receiver.focused||input_phase!=INPUT_DISCONNECTED) result=1;
        printf("INPUT_EXIT result=%d down=%u up=%u pdown=%u pup=%u enter=%u exit=%u text=%u keys=%u buttons=%u\n",
            result,receiver.downs,receiver.ups,receiver.pointer_downs,receiver.pointer_ups,receiver.enters,receiver.exits,
            receiver.texts,native_input_keys(&receiver),receiver.buttons);
    }
    printf("CONTENT_EXIT result=%d publications=%u discarded=%u disposals=%u frames=%u completed=%u\n",
        result,worker.publications,worker.discarded,atomic_load(&worker.disposals),frames,worker.completed);
    return result;
}
