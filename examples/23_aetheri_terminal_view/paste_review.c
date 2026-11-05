#include "paste_review.h"
#include <math.h>
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
    float window_width,window_height,ppem;
    int german;
    int open,initial_frame,settling;
};
static nx_utf8_span span(const char *s){return (nx_utf8_span){s,strlen(s)};}
static int same(terminal_paste_guard a,terminal_paste_guard b) {
    return b.focused==1 && a.identity==b.identity && a.revision==b.revision && a.focus_epoch==b.focus_epoch;
}
static nx_result label(terminal_paste_review *r,const char *text,float max_width,nx_text_layout **out) {
    return nx_text_layout_create(r->fonts,text,strlen(text),NX_BIDI_AUTO,r->ppem,max_width,out);
}
/* Status uses individually positioned ASCII glyphs, so its maximum extent is
   established by the widest supported glyph rather than every possible page. */
static nx_result status_metrics(const nx_font_collection *fonts,float ppem,float width,int german,
                                float advances[128],float *height) {
    const char *alphabet=german?"0123456789 Zeiln-/":"0123456789 Rows-/";
    const char *longest=german?"Zeilen 4096-4096/4096":"Rows 4096-4096/4096";
    memset(advances,0,128*sizeof(*advances));float digit_width=0;*height=0;
    for(const char *c=alphabet;*c;++c) {
        nx_text_layout *layout=NULL;
        nx_result e=nx_text_layout_create(fonts,c,1,NX_BIDI_AUTO,ppem,width,&layout);if(e!=NX_OK)return e;
        float glyph_width=(float)nx_text_layout_width(layout),glyph_height=(float)nx_text_layout_height(layout);
        advances[(unsigned char)*c]=glyph_width;
        if(*c>='0' && *c<='9' && glyph_width>digit_width)digit_width=glyph_width;
        if(glyph_height>*height)*height=glyph_height;
        nx_text_layout_destroy(layout);
    }
    for(unsigned c='0';c<='9';++c)advances[c]=digit_width;
    advances[' ']=digit_width;
    float total=0;for(const unsigned char *c=(const unsigned char *)longest;*c;++c)total+=advances[*c];
    return digit_width>0 && *height>0 && total<=width?NX_OK:NX_ERROR_LIMIT;
}
/* Validate a candidate completely before accepting it. A later page must not
   discover an oversized glyph after the old frame has already been lost. */
static nx_result preflight(const terminal_paste_review *r,float window_width,float window_height,float ppem,int german) {
    float inner_width=window_width-56,line_height=ceilf(ppem*1.5f);
    float button_height=fmaxf(36,line_height+8),min_button=fmaxf(130,ppem*6.25f);
    unsigned columns=inner_width>=4*min_button+24?4:2,button_rows=columns==4?1:2;
    float button_width=(inner_width-8*(columns-1))/columns;
    float button_y=window_height-24-(button_rows*button_height+(button_rows-1)*8);
    const char *title=german?"Einfügen prüfen":"Review paste";
    const char *note=german?"Kann Befehle ausführen":"Paste may run commands";
    const char *buttons_de[]={"Abbrechen","Zurück","Weiter","Einfügen"};
    const char *buttons_en[]={"Cancel","Previous","Next","Paste"};
    const char *texts[]={title,note};float heights[2]={0};nx_text_layout *layout=NULL;
    nx_result result=NX_OK;
    for(unsigned i=0;i<2;++i) {
        result=nx_text_layout_create(r->fonts,texts[i],strlen(texts[i]),NX_BIDI_AUTO,ppem,inner_width,&layout);
        if(result!=NX_OK)return result;
        heights[i]=(float)nx_text_layout_height(layout);
        if(nx_text_layout_width(layout)>inner_width)result=NX_ERROR_LIMIT;
        nx_text_layout_destroy(layout);layout=NULL;if(result!=NX_OK)return result;
    }
    float status_advances[128],status_height=0;
    result=status_metrics(r->fonts,ppem,inner_width,german,status_advances,&status_height);if(result!=NX_OK)return result;
    float preview_y=22+heights[0]+4+status_height+4+heights[1]+8;
    float available=button_y-preview_y-8;
    size_t visible=available>0?(size_t)floorf(available/line_height):0;
    if(visible>PREVIEW_ROWS)visible=PREVIEW_ROWS;
    if(!visible || button_width<min_button)return NX_ERROR_LIMIT;
    for(unsigned i=0;i<4;++i) {
        const char *name=german?buttons_de[i]:buttons_en[i];
        result=nx_text_layout_create(r->fonts,name,strlen(name),NX_BIDI_AUTO,ppem,button_width-16,&layout);
        if(result!=NX_OK)return result;
        if(nx_text_layout_width(layout)>button_width-16 || nx_text_layout_height(layout)>button_height-4)result=NX_ERROR_LIMIT;
        nx_text_layout_destroy(layout);layout=NULL;if(result!=NX_OK)return result;
    }
    struct { uint64_t hash;size_t index;int valid; } cache[512]={{0}};
    for(size_t i=0;i<r->count;++i) {
        char text[AE_TERM_PREVIEW_BYTES];size_t at=r->offsets[i];
        if(ae_terminal_paste_preview(&r->flow->paste,at,PREVIEW_COLS,r->width,text)<=at)return NX_ERROR_STATE;
        uint64_t hash=UINT64_C(14695981039346656037);
        for(const unsigned char *p=(const unsigned char *)text;*p;++p)hash=(hash^*p)*UINT64_C(1099511628211);
        size_t slot=(size_t)(hash%512u);
        if(cache[slot].valid && cache[slot].hash==hash) {
            char prior[AE_TERM_PREVIEW_BYTES];size_t previous=r->offsets[cache[slot].index];
            if(ae_terminal_paste_preview(&r->flow->paste,previous,PREVIEW_COLS,r->width,prior)<=previous)return NX_ERROR_STATE;
            if(!strcmp(prior,text))continue;
        }
        result=nx_text_layout_create(r->fonts,text,strlen(text),NX_BIDI_AUTO,ppem,inner_width,&layout);
        if(result!=NX_OK)return result;
        if(nx_text_layout_width(layout)>inner_width || nx_text_layout_height(layout)>line_height)result=NX_ERROR_LIMIT;
        nx_text_layout_destroy(layout);layout=NULL;if(result!=NX_OK)return result;
        cache[slot].valid=1;cache[slot].hash=hash;cache[slot].index=i;
    }
    return NX_OK;
}
static nx_result button(terminal_paste_review *r,uint64_t id,nx_rect bounds,const char *name,int disabled,int *clicked) {
    nx_text_layout *layout=NULL;nx_result result=label(r,name,bounds.width-16,&layout);if(result!=NX_OK)return result;
    if(nx_text_layout_width(layout)>bounds.width-16 || nx_text_layout_height(layout)>bounds.height-4) {
        nx_text_layout_destroy(layout);return NX_ERROR_LIMIT;
    }
    nx_control_desc d={0};d.id=id;d.parent_id=3;d.bounds=bounds;d.name=span(name);
    d.label_layout=layout;d.label_origin=(nx_point){bounds.x+8,bounds.y+(bounds.height-(float)nx_text_layout_height(layout))*0.5f};d.glyph_cache_bytes=CACHE_BYTES;d.disabled=disabled;
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
    r->window_width=640;r->window_height=360;r->ppem=16;
    size_t at=0;char line[AE_TERM_PREVIEW_BYTES];
    while(at<flow->paste.size) {
        if(r->count>=AE_TERM_PASTE_BYTES){free(r);return NX_ERROR_LIMIT;}
        r->offsets[r->count++]=at;size_t next=ae_terminal_paste_preview(&flow->paste,at,PREVIEW_COLS,width,line);
        if(next<=at || next>flow->paste.size){free(r);return NX_ERROR_STATE;}at=next;
    }
    nx_result result=preflight(r,r->window_width,r->window_height,r->ppem,r->german);
    if(result!=NX_OK){free(r);return result;}
    result=nx_context_create(NULL,&r->context);if(result!=NX_OK){free(r);return result;}
    *out=r;return NX_OK;
}
void terminal_paste_review_destroy(terminal_paste_review *r){if(r){if(r->open)terminal_paste_reset(r->flow);nx_context_destroy(r->context);free(r);}}
nx_context *terminal_paste_review_context(terminal_paste_review *r){return r?r->context:NULL;}
nx_result terminal_paste_review_configure(terminal_paste_review *r,float width,float height,float ppem,int german) {
    if(!r)return NX_ERROR_ARGUMENT;
    if(!r->open)return NX_ERROR_STATE;
    if(!isfinite(width) || !isfinite(height) || !isfinite(ppem) || width<400 || height<320 ||
       width>8192 || height>8192 || ppem<16 || ppem>32 || (german!=0 && german!=1)) {
        r->settling=1;return NX_ERROR_ARGUMENT;
    }
    if(r->window_width==width && r->window_height==height && r->ppem==ppem && r->german==german)return NX_OK;
    r->settling=1;
    nx_result checked=preflight(r,width,height,ppem,german);if(checked!=NX_OK)return checked;
    r->window_width=width;r->window_height=height;r->ppem=ppem;r->german=german;return NX_OK;
}
nx_result terminal_paste_review_frame(terminal_paste_review *r,terminal_paste_guard guard,
    enum terminal_review_result *result,nx_utf8_span *confirmed) {
    if(!result || !confirmed)return NX_ERROR_ARGUMENT;
    *result=TERMINAL_REVIEW_OPEN;*confirmed=(nx_utf8_span){0};
    if(!r || !r->open || r->flow->state!=2)return NX_ERROR_STATE;
    if(!same(r->flow->guard,guard)){terminal_paste_reset(r->flow);r->open=0;*result=TERMINAL_REVIEW_CANCELLED;return NX_OK;}
    const char *title_text=r->german?"Einfügen prüfen":"Review paste";
    const char *note=r->german?"Kann Befehle ausführen":"Paste may run commands";
    const char *cancel_text=r->german?"Abbrechen":"Cancel";
    const char *previous_text=r->german?"Zurück":"Previous";
    const char *next_text=r->german?"Weiter":"Next";
    const char *accept_text=r->german?"Einfügen":"Paste";
    float inner_width=r->window_width-56,inner_x=28,line_height=ceilf(r->ppem*1.5f);
    float button_height=fmaxf(36,line_height+8),min_button=fmaxf(130,r->ppem*6.25f);
    unsigned button_columns=inner_width>=4*min_button+24?4:2;
    unsigned button_rows=button_columns==4?1:2;
    float button_width=(inner_width-8*(button_columns-1))/button_columns;
    float button_y=r->window_height-24-(button_rows*button_height+(button_rows-1)*8);
    nx_text_layout *title=NULL,*note_layout=NULL;
    nx_result e=label(r,title_text,inner_width,&title);if(e!=NX_OK)return e;
    e=label(r,note,inner_width,&note_layout);if(e!=NX_OK){nx_text_layout_destroy(title);return e;}
    float status_advances[128],status_budget=0;
    e=status_metrics(r->fonts,r->ppem,inner_width,r->german,status_advances,&status_budget);
    if(e!=NX_OK){nx_text_layout_destroy(title);nx_text_layout_destroy(note_layout);return e;}
    float title_y=22,status_y=title_y+(float)nx_text_layout_height(title)+4;
    float note_y=status_y+status_budget+4;
    float note_height=(float)nx_text_layout_height(note_layout);
    float preview_y=note_y+note_height+8;
    float available=button_y-preview_y-8;
    size_t visible=available>0?(size_t)floorf(available/line_height):0;
    if(visible>PREVIEW_ROWS)visible=PREVIEW_ROWS;
    if(!visible || button_width<min_button || nx_text_layout_width(title)>inner_width ||
       nx_text_layout_width(note_layout)>inner_width) {
        nx_text_layout_destroy(title);nx_text_layout_destroy(note_layout);return NX_ERROR_LIMIT;
    }
    nx_text_layout_destroy(title);title=NULL;
    nx_text_layout_destroy(note_layout);note_layout=NULL;
    nx_rect buttons[4];for(unsigned i=0;i<4;++i)
        buttons[i]=(nx_rect){inner_x+(i%button_columns)*(button_width+8),
            button_y+(i/button_columns)*(button_height+8),button_width,button_height};
    e=nx_begin_frame(r->context,r->window_width,r->window_height,0);
    if(e!=NX_OK)return e;
    nx_color foreground;e=nx_theme_color(r->context,NX_COLOR_TEXT,&foreground);if(e!=NX_OK)return e;
    nx_control_desc base={0};base.id=2;base.bounds=(nx_rect){10,10,100,36};base.name=span("Terminal");
    nx_interaction_state state;e=nx_button(r->context,&base,&state);if(e!=NX_OK)return e;
    e=label(r,title_text,inner_width,&title);if(e!=NX_OK)return e;
    nx_overlay_desc modal={0};modal.panel.id=3;modal.panel.bounds=(nx_rect){12,12,r->window_width-24,r->window_height-24};
    modal.panel.name=span(title_text);modal.panel.label_layout=title;modal.panel.label_origin=(nx_point){inner_x,title_y};
    modal.panel.glyph_cache_bytes=CACHE_BYTES;modal.backdrop_id=4;modal.dismiss_on_escape=1;
    int dismissed=0;e=nx_modal_begin(r->context,&modal,&dismissed);nx_text_layout_destroy(title);if(e!=NX_OK)return e;
    const nx_event *events=NULL;size_t n=0;e=nx_frame_events(r->context,&events,&n);if(e!=NX_OK)return e;
    size_t last=r->count>visible?r->count-visible:0;if(r->top>last)r->top=last;
    for(size_t i=0;i<n;++i)if(events[i].type==NX_EVENT_KEY_DOWN && !events[i].modifiers) {
        if(events[i].key==NX_KEY_PAGE_DOWN)r->top=r->top+visible>last?last:r->top+visible;
        else if(events[i].key==NX_KEY_PAGE_UP)r->top=r->top>visible?r->top-visible:0;
        else if(events[i].key==NX_KEY_HOME)r->top=0;
        else if(events[i].key==NX_KEY_END)r->top=last;
        else if(events[i].key==NX_KEY_DOWN && r->top<last)++r->top;
        else if(events[i].key==NX_KEY_UP && r->top)--r->top;
    }
    int cancel=0,previous=0,next=0,accept=0;
    e=button(r,10,buttons[0],cancel_text,0,&cancel);if(e!=NX_OK)return e;
    e=button(r,11,buttons[1],previous_text,r->top==0,&previous);if(e!=NX_OK)return e;
    e=button(r,12,buttons[2],next_text,r->top==last,&next);if(e!=NX_OK)return e;
    e=button(r,13,buttons[3],accept_text,0,&accept);if(e!=NX_OK)return e;
    if(previous)r->top=r->top>visible?r->top-visible:0;
    if(next)r->top=r->top+visible>last?last:r->top+visible;
    char status[96];int written=snprintf(status,sizeof(status),r->german?"Zeilen %zu-%zu/%zu":"Rows %zu-%zu/%zu",r->top+1,
        r->top+visible<r->count?r->top+visible:r->count,r->count);
    if(written<0 || (size_t)written>=sizeof(status))return NX_ERROR_STATE;
    float status_x=inner_x;
    for(int i=0;i<written;++i) {
        char character[2]={status[i],0};nx_text_layout *glyph=NULL;
        e=label(r,character,inner_width,&glyph);if(e!=NX_OK)return e;
        unsigned char code=(unsigned char)status[i];
        if(code>=128 || nx_text_layout_width(glyph)>status_advances[code] || nx_text_layout_height(glyph)>status_budget) {
            nx_text_layout_destroy(glyph);return NX_ERROR_LIMIT;
        }
        e=nx_draw_text_layout(r->context,glyph,(nx_point){status_x,status_y},foreground,CACHE_BYTES);
        status_x+=status_advances[code];
        nx_text_layout_destroy(glyph);if(e!=NX_OK)return e;
    }
    nx_semantic_desc summary={0};summary.id=5;summary.parent_id=3;summary.role=NX_ROLE_STATUS;
    summary.bounds=(nx_rect){inner_x,status_y,inner_width,status_budget};summary.name=span(r->german?"Vorschauposition":"Preview position");summary.value=span(status);
    e=nx_semantics_add(r->context,&summary,NULL);if(e!=NX_OK)return e;
    e=label(r,note,inner_width,&note_layout);if(e!=NX_OK)return e;
    e=nx_draw_text_layout(r->context,note_layout,(nx_point){inner_x,note_y},foreground,CACHE_BYTES);
    nx_text_layout_destroy(note_layout);if(e!=NX_OK)return e;
    nx_semantic_desc note_sem={0};note_sem.id=6;note_sem.parent_id=3;note_sem.role=NX_ROLE_TEXT;
    note_sem.bounds=(nx_rect){inner_x,note_y,inner_width,note_height};note_sem.name=span(note);note_sem.state=NX_SEM_READONLY;
    e=nx_semantics_add(r->context,&note_sem,NULL);if(e!=NX_OK)return e;
    for(size_t i=0;i<visible && r->top+i<r->count;++i) {
        char line[AE_TERM_PREVIEW_BYTES];size_t at=r->offsets[r->top+i];
        if(ae_terminal_paste_preview(&r->flow->paste,at,PREVIEW_COLS,r->width,line)<=at)return NX_ERROR_STATE;
        nx_text_layout *layout=NULL;e=label(r,line,inner_width,&layout);if(e!=NX_OK)return e;
        if(nx_text_layout_width(layout)>inner_width || nx_text_layout_height(layout)>line_height) {
            nx_text_layout_destroy(layout);return NX_ERROR_LIMIT;
        }
        float y=preview_y+(float)i*line_height;e=nx_draw_text_layout(r->context,layout,(nx_point){inner_x,y},foreground,CACHE_BYTES);
        nx_text_layout_destroy(layout);if(e!=NX_OK)return e;
        nx_semantic_desc row={0};row.id=100+i;row.parent_id=3;row.role=NX_ROLE_TEXT;row.bounds=(nx_rect){inner_x,y,inner_width,line_height};
        row.name=span(r->german?"Vorschauzeile":"Preview row");row.value=span(line);row.state=NX_SEM_READONLY;
        e=nx_semantics_add_readonly_text_snapshot_without_caret(r->context,&row,row.id,NULL);if(e!=NX_OK)return e;
    }
    e=nx_overlay_end(r->context);if(e!=NX_OK)return e;
    if(dismissed || cancel) {*result=TERMINAL_REVIEW_CANCELLED;terminal_paste_reset(r->flow);r->open=0;}
    else if(accept && !r->settling) {e=terminal_paste_confirm(r->flow,guard,1,0,confirmed);if(e!=NX_OK)return e;
        *result=TERMINAL_REVIEW_CONFIRMED;r->open=0;}
    if(r->settling) {
        nx_input_snapshot input;e=nx_frame_input(r->context,&input);if(e!=NX_OK)return e;
        if(!input.buttons_down && !nx_key_down(r->context,NX_KEY_ENTER) && !nx_key_down(r->context,NX_KEY_SPACE))r->settling=0;
    }
    if(!r->open){e=nx_overlay_close(r->context);if(e!=NX_OK)return e;}
    e=nx_end_frame(r->context);if(e!=NX_OK)return e;
    /* A page button is resolved during widget registration. Rebuild once so
       its disabled state and all published rows describe the same page. */
    int initial=r->initial_frame;r->initial_frame=0;
    if(r->open && (previous || next || initial))return terminal_paste_review_frame(r,guard,result,confirmed);
    return NX_OK;
}
