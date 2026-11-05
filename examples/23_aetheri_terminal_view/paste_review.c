#include "paste_review.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PREVIEW_COLS=16, PREVIEW_ROWS=8, CACHE_BYTES=65536 };
struct terminal_paste_review {
    terminal_paste_flow *flow;
    const nx_font_collection *fonts;
    ae_term_width width;
    nx_context *context;
    size_t offsets[AE_TERM_PASTE_BYTES+1],count,top;
    int open,initial_frame;
};
static nx_utf8_span span(const char *s){return (nx_utf8_span){s,strlen(s)};}
static int same(terminal_paste_guard a,terminal_paste_guard b) {
    return b.focused==1 && a.identity==b.identity && a.revision==b.revision && a.focus_epoch==b.focus_epoch;
}
static nx_result label(terminal_paste_review *r,const char *text,nx_text_layout **out) {
    return nx_text_layout_create(r->fonts,text,strlen(text),NX_BIDI_AUTO,16,500,out);
}
static nx_result button(terminal_paste_review *r,uint64_t id,float x,const char *name,int disabled,int *clicked) {
    nx_text_layout *layout=NULL;nx_result result=label(r,name,&layout);if(result!=NX_OK)return result;
    nx_control_desc d={0};d.id=id;d.parent_id=3;d.bounds=(nx_rect){x,286,130,36};d.name=span(name);
    d.label_layout=layout;d.label_origin=(nx_point){x+8,294};d.glyph_cache_bytes=CACHE_BYTES;d.disabled=disabled;
    nx_interaction_state state;result=nx_button(r->context,&d,&state);nx_text_layout_destroy(layout);
    if(result==NX_OK)*clicked=state.clicked!=0;
    return result;
}
nx_result terminal_paste_review_create(terminal_paste_flow *flow,const nx_font_collection *fonts,
    ae_term_width width,terminal_paste_review **out) {
    if(!out)return NX_ERROR_ARGUMENT;
    *out=NULL;
    if(!flow || flow->state!=2 || !flow->paste.review || !fonts || !width)return NX_ERROR_ARGUMENT;
    terminal_paste_review *r=calloc(1,sizeof(*r));if(!r)return NX_ERROR_MEMORY;
    r->flow=flow;r->fonts=fonts;r->width=width;r->open=1;r->initial_frame=1;
    size_t at=0;char line[AE_TERM_PREVIEW_BYTES];
    while(at<flow->paste.size) {
        if(r->count>=AE_TERM_PASTE_BYTES){free(r);return NX_ERROR_LIMIT;}
        r->offsets[r->count++]=at;size_t next=ae_terminal_paste_preview(&flow->paste,at,PREVIEW_COLS,width,line);
        if(next<=at || next>flow->paste.size){free(r);return NX_ERROR_STATE;}at=next;
    }
    nx_result result=nx_context_create(NULL,&r->context);if(result!=NX_OK){free(r);return result;}
    *out=r;return NX_OK;
}
void terminal_paste_review_destroy(terminal_paste_review *r){if(r){if(r->open)terminal_paste_reset(r->flow);nx_context_destroy(r->context);free(r);}}
nx_context *terminal_paste_review_context(terminal_paste_review *r){return r?r->context:NULL;}
nx_result terminal_paste_review_frame(terminal_paste_review *r,terminal_paste_guard guard,
    enum terminal_review_result *result,nx_utf8_span *confirmed) {
    if(!result || !confirmed)return NX_ERROR_ARGUMENT;
    *result=TERMINAL_REVIEW_OPEN;*confirmed=(nx_utf8_span){0};
    if(!r || !r->open || r->flow->state!=2)return NX_ERROR_STATE;
    if(!same(r->flow->guard,guard)){terminal_paste_reset(r->flow);r->open=0;*result=TERMINAL_REVIEW_CANCELLED;return NX_OK;}
    nx_result e=nx_begin_frame(r->context,640,360,0);if(e!=NX_OK)return e;
    nx_color foreground;e=nx_theme_color(r->context,NX_COLOR_TEXT,&foreground);if(e!=NX_OK)return e;
    nx_control_desc base={0};base.id=2;base.bounds=(nx_rect){10,10,100,36};base.name=span("Terminal");
    nx_interaction_state state;e=nx_button(r->context,&base,&state);if(e!=NX_OK)return e;
    nx_text_layout *title=NULL;e=label(r,"Review paste",&title);if(e!=NX_OK)return e;
    nx_overlay_desc modal={0};modal.panel.id=3;modal.panel.bounds=(nx_rect){20,20,600,320};
    modal.panel.name=span("Review paste");modal.panel.label_layout=title;modal.panel.label_origin=(nx_point){40,30};
    modal.panel.glyph_cache_bytes=CACHE_BYTES;modal.backdrop_id=4;modal.dismiss_on_escape=1;
    int dismissed=0;e=nx_modal_begin(r->context,&modal,&dismissed);nx_text_layout_destroy(title);if(e!=NX_OK)return e;
    const nx_event *events=NULL;size_t n=0;e=nx_frame_events(r->context,&events,&n);if(e!=NX_OK)return e;
    size_t last=r->count>PREVIEW_ROWS?r->count-PREVIEW_ROWS:0;
    for(size_t i=0;i<n;++i)if(events[i].type==NX_EVENT_KEY_DOWN && !events[i].modifiers) {
        if(events[i].key==NX_KEY_PAGE_DOWN)r->top=r->top+PREVIEW_ROWS>last?last:r->top+PREVIEW_ROWS;
        else if(events[i].key==NX_KEY_PAGE_UP)r->top=r->top>PREVIEW_ROWS?r->top-PREVIEW_ROWS:0;
        else if(events[i].key==NX_KEY_HOME)r->top=0;
        else if(events[i].key==NX_KEY_END)r->top=last;
        else if(events[i].key==NX_KEY_DOWN && r->top<last)++r->top;
        else if(events[i].key==NX_KEY_UP && r->top)--r->top;
    }
    int cancel=0,previous=0,next=0,accept=0;
    e=button(r,10,40,"Cancel",0,&cancel);if(e!=NX_OK)return e;
    e=button(r,11,180,"Previous",r->top==0,&previous);if(e!=NX_OK)return e;
    e=button(r,12,320,"Next",r->top==last,&next);if(e!=NX_OK)return e;
    e=button(r,13,460,"Paste",0,&accept);if(e!=NX_OK)return e;
    if(previous)r->top=r->top>PREVIEW_ROWS?r->top-PREVIEW_ROWS:0;
    if(next)r->top=r->top+PREVIEW_ROWS>last?last:r->top+PREVIEW_ROWS;
    char status[96];int written=snprintf(status,sizeof(status),"Rows %zu-%zu of %zu",r->top+1,
        r->top+PREVIEW_ROWS<r->count?r->top+PREVIEW_ROWS:r->count,r->count);
    if(written<0 || (size_t)written>=sizeof(status))return NX_ERROR_STATE;
    nx_text_layout *status_layout=NULL;e=label(r,status,&status_layout);if(e!=NX_OK)return e;
    e=nx_draw_text_layout(r->context,status_layout,(nx_point){40,55},foreground,CACHE_BYTES);
    nx_text_layout_destroy(status_layout);if(e!=NX_OK)return e;
    nx_semantic_desc summary={0};summary.id=5;summary.parent_id=3;summary.role=NX_ROLE_STATUS;
    summary.bounds=(nx_rect){40,55,400,20};summary.name=span("Preview position");summary.value=span(status);
    e=nx_semantics_add(r->context,&summary,NULL);if(e!=NX_OK)return e;
    const char *note="Paste can run commands. Review all rows.";
    nx_text_layout *note_layout=NULL;
    e=nx_text_layout_create(r->fonts,note,strlen(note),NX_BIDI_AUTO,16,280,&note_layout);if(e!=NX_OK)return e;
    e=nx_draw_text_layout(r->context,note_layout,(nx_point){300,78},foreground,CACHE_BYTES);
    nx_text_layout_destroy(note_layout);if(e!=NX_OK)return e;
    nx_semantic_desc note_sem={0};note_sem.id=6;note_sem.parent_id=3;note_sem.role=NX_ROLE_TEXT;
    note_sem.bounds=(nx_rect){300,78,280,50};note_sem.name=span(note);note_sem.state=NX_SEM_READONLY;
    e=nx_semantics_add(r->context,&note_sem,NULL);if(e!=NX_OK)return e;
    for(size_t i=0;i<PREVIEW_ROWS && r->top+i<r->count;++i) {
        char line[AE_TERM_PREVIEW_BYTES];size_t at=r->offsets[r->top+i];
        if(ae_terminal_paste_preview(&r->flow->paste,at,PREVIEW_COLS,r->width,line)<=at)return NX_ERROR_STATE;
        nx_text_layout *layout=NULL;e=label(r,line,&layout);if(e!=NX_OK)return e;
        float y=78+(float)i*24;e=nx_draw_text_layout(r->context,layout,(nx_point){40,y},foreground,CACHE_BYTES);
        nx_text_layout_destroy(layout);if(e!=NX_OK)return e;
        nx_semantic_desc row={0};row.id=100+i;row.parent_id=3;row.role=NX_ROLE_TEXT;row.bounds=(nx_rect){40,y,520,22};
        row.name=span("Preview row");row.value=span(line);row.state=NX_SEM_READONLY;
        e=nx_semantics_add_readonly_text_snapshot_without_caret(r->context,&row,row.id,NULL);if(e!=NX_OK)return e;
    }
    e=nx_overlay_end(r->context);if(e!=NX_OK)return e;
    if(dismissed || cancel) {*result=TERMINAL_REVIEW_CANCELLED;terminal_paste_reset(r->flow);r->open=0;}
    else if(accept) {e=terminal_paste_confirm(r->flow,guard,1,0,confirmed);if(e!=NX_OK)return e;
        *result=TERMINAL_REVIEW_CONFIRMED;r->open=0;}
    if(!r->open){e=nx_overlay_close(r->context);if(e!=NX_OK)return e;}
    e=nx_end_frame(r->context);if(e!=NX_OK)return e;
    /* A page button is resolved during widget registration. Rebuild once so
       its disabled state and all published rows describe the same page. */
    int initial=r->initial_frame;r->initial_frame=0;
    if(r->open && (previous || next || initial))return terminal_paste_review_frame(r,guard,result,confirmed);
    return NX_OK;
}
