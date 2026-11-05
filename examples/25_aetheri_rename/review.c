#include "review.h"
#include "files/model.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct rename_review { rename_review_owner *owner; nx_font_collection *fonts; nx_context *ctx; int open,seen_bottom; float scroll_x,scroll_y; };
static nx_utf8_span span(const char *s){return (nx_utf8_span){s,strlen(s)};}
/* Literal bytes are never interpreted as markup or bidi controls. Escaping
   backslashes makes the ASCII representation reversible and unambiguous. */
static void format_name(const char *source,char out[AE_FILES_LABEL+1]) {
    static const char hex[]="0123456789ABCDEF";size_t n=0;
    for(const unsigned char *p=(const unsigned char *)source;*p;++p){unsigned c=*p;
        if(c>=32 && c<127 && c!='\\')out[n++]=(char)c;
        else {out[n++]='\\';out[n++]='x';out[n++]=hex[c>>4];out[n++]=hex[c&15];}}
    out[n]=0;
}
/* Wrap only between complete displayed bytes, never within a \\xNN escape.
   The original unwrapped representation remains the semantic value. */
static void wrap_name(const char *prefix,const char *name,char out[AE_FILES_LABEL*2+16]) {
    size_t used=strlen(prefix),column=used;memcpy(out,prefix,used);
    while(*name){size_t token=*name=='\\'?4u:1u;
        if(column+token>24){out[used++]='\n';column=0;}
        memcpy(out+used,name,token);used+=token;name+=token;column+=token;}
    out[used]=0;
}
static const char *status(enum ae_files_rename_result r) {
    switch(r){case AE_FILES_RENAME_PREPARED:return "Ready for confirmation";
    case AE_FILES_RENAME_DONE:return "Rename completed";
    case AE_FILES_RENAME_CONFLICT:return "Destination already exists";
    case AE_FILES_RENAME_UNKNOWN:return "Result unknown; check the original operation";
    case AE_FILES_RENAME_CHANGED:return "Source changed; rename not attempted";
    case AE_FILES_RENAME_DENIED:return "Rename denied";
    default:return "Rename unavailable";}
}
nx_result rename_review_owner_init(rename_review_owner *owner,struct ae_files_rename *operation) {
    if(!owner || !operation)return NX_ERROR_ARGUMENT;
    owner->operation=operation;owner->submitted=0;return NX_OK;
}
nx_result rename_review_create(rename_review_owner *owner,nx_font_collection *fonts,rename_review **out) {
    if(!out)return NX_ERROR_ARGUMENT;
    *out=NULL;if(!owner || !owner->operation || !fonts)return NX_ERROR_ARGUMENT;
    rename_review *r=calloc(1,sizeof(*r));if(!r)return NX_ERROR_MEMORY;
    nx_result e=nx_context_create(NULL,&r->ctx);if(e!=NX_OK){free(r);return e;}
    nx_control_desc background={0};background.id=20;background.bounds=(nx_rect){0,0,400,300};background.name=span("Files");
    nx_interaction_state state;e=nx_begin_frame(r->ctx,400,300,0);
    if(e==NX_OK)e=nx_button(r->ctx,&background,&state);
    if(e==NX_OK)e=nx_end_frame(r->ctx);
    if(e==NX_OK)e=nx_focus_set(r->ctx,20);
    if(e!=NX_OK){nx_context_destroy(r->ctx);free(r);return e;}
    r->owner=owner;r->fonts=fonts;r->open=1;*out=r;return NX_OK;
}
void rename_review_destroy(rename_review *r){if(r){nx_context_destroy(r->ctx);free(r);}}
nx_context *rename_review_context(rename_review *r){return r?r->ctx:NULL;}
nx_result rename_review_frame(rename_review *r,rename_review_action *action) {
    if(!r || !action)return NX_ERROR_ARGUMENT;
    *action=REVIEW_NONE;
    if(!r->open) {
        nx_result e=nx_begin_frame(r->ctx,400,300,0);if(e!=NX_OK)return e;
        nx_control_desc background={0};background.id=20;background.bounds=(nx_rect){0,0,400,300};background.name=span("Files");
        nx_interaction_state state;e=nx_button(r->ctx,&background,&state);
        if(e==NX_OK && state.clicked)*action=REVIEW_BACKGROUND;
        nx_result end=nx_end_frame(r->ctx);return e==NX_OK?end:e;
    }
    char old_name[AE_FILES_LABEL+1],new_name[AE_FILES_LABEL+1];
    char old_display[AE_FILES_LABEL*2+16],new_display[AE_FILES_LABEL*2+16];
    const char *old=ae_files_rename_old(r->owner->operation),*next=ae_files_rename_new(r->owner->operation);
    format_name(old,old_name);format_name(next,new_name);
    wrap_name("From: ",old_name,old_display);wrap_name("To: ",new_name,new_display);
    enum ae_files_rename_result operation_status=ae_files_rename_status(r->owner->operation);
    const char *message=r->owner->submitted && operation_status==AE_FILES_RENAME_PREPARED ?
        "Confirmation requested; awaiting result" : status(operation_status);
    const char *strings[]={"Rename entry",old_display,new_display,message,"Cancel","Confirm","Previous","Next"};
    nx_text_layout *layouts[8]={0};nx_result e=NX_OK;
    for(unsigned i=0;i<3 && e==NX_OK;++i){float width=i?340:360;
        e=nx_text_layout_create(r->fonts,strings[i],strlen(strings[i]),NX_BIDI_AUTO,16,width,&layouts[i]);
        if(e==NX_OK && nx_text_layout_width(layouts[i])>width)e=NX_ERROR_LIMIT;}
    if(e!=NX_OK)goto done;
    float old_height=(float)nx_text_layout_height(layouts[1]),new_height=(float)nx_text_layout_height(layouts[2]);
    float content_height=old_height+new_height+16;
    int reviewed=r->seen_bottom || content_height<=110;
    if(!reviewed && operation_status==AE_FILES_RENAME_PREPARED && !r->owner->submitted)
        strings[3]="Scroll to review both names";
    for(unsigned i=3;i<8 && e==NX_OK;++i){float width=i==3?360:(i>=6?80:104),height=i==3?40:24;
        e=nx_text_layout_create(r->fonts,strings[i],strlen(strings[i]),NX_BIDI_AUTO,16,width,&layouts[i]);
        if(e==NX_OK && (nx_text_layout_height(layouts[i])>height || nx_text_layout_width(layouts[i])>width))e=NX_ERROR_LIMIT;}
    if(e!=NX_OK)goto done;
    e=nx_begin_frame(r->ctx,400,300,0);if(e!=NX_OK)goto done;
    nx_color color;e=nx_theme_color(r->ctx,NX_COLOR_TEXT,&color);
    nx_control_desc background={0};background.id=20;background.bounds=(nx_rect){0,0,400,300};background.name=span("Files");
    nx_interaction_state state;if(e==NX_OK)e=nx_button(r->ctx,&background,&state);
    nx_overlay_desc modal={0};modal.panel.id=1;modal.panel.bounds=(nx_rect){8,4,384,292};
    modal.panel.name=span("Rename review");modal.backdrop_id=21;modal.dismiss_on_escape=1;
    int dismissed=0,overlay_open=0;
    if(e==NX_OK){e=nx_modal_begin(r->ctx,&modal,&dismissed);if(e==NX_OK)overlay_open=1;}
    nx_semantic_desc title={0};title.id=2;title.parent_id=1;title.role=NX_ROLE_TEXT;
    title.bounds=(nx_rect){20,14,360,26};title.name=span("Rename entry");title.value=span(strings[0]);
    if(e==NX_OK)e=nx_semantics_add(r->ctx,&title,NULL);
    if(e==NX_OK)e=nx_draw_text_layout(r->ctx,layouts[0],(nx_point){20,14},color,1024*1024);
    nx_scroll_area_desc scroll={0};scroll.id=30;scroll.parent_id=1;scroll.thumb_y_id=31;
    scroll.name=span("Names to review");scroll.viewport=(nx_rect){20,48,360,110};
    scroll.content_size=(nx_size){360,content_height};scroll.axes=NX_SCROLL_Y;
    nx_scroll_info info={0};int scrolling=0;
    if(e==NX_OK){e=nx_scroll_area_begin(r->ctx,&scroll,&r->scroll_x,&r->scroll_y,&info);if(e==NX_OK)scrolling=1;}
    const char *names[]={"Original name","New name"};const char *values[]={old_name,new_name};
    for(unsigned i=0;e==NX_OK && i<2;++i){nx_semantic_desc item={0};item.id=3+i;item.parent_id=30;item.role=NX_ROLE_TEXT;
        item.bounds=(nx_rect){28,52-r->scroll_y+(i?old_height+8:0),340,i?new_height:old_height};
        item.name=span(names[i]);item.value=span(values[i]);e=nx_semantics_add(r->ctx,&item,NULL);
        if(e==NX_OK)e=nx_draw_text_layout(r->ctx,layouts[1+i],(nx_point){item.bounds.x,item.bounds.y},color,1024*1024);}
    if(scrolling){nx_result end=nx_scroll_area_end(r->ctx);if(e==NX_OK)e=end;}
    nx_semantic_desc status_node={0};status_node.id=5;status_node.parent_id=1;status_node.role=NX_ROLE_STATUS;
    status_node.bounds=(nx_rect){20,162,360,40};status_node.name=span("Rename status");status_node.value=span(strings[3]);
    if(e==NX_OK)e=nx_semantics_add(r->ctx,&status_node,NULL);
    if(e==NX_OK)e=nx_draw_text_layout(r->ctx,layouts[3],(nx_point){20,162},color,1024*1024);
    int cancel=0,confirm=0;
    for(unsigned i=0;e==NX_OK && i<2;++i){nx_control_desc b={0};b.id=10+i;b.parent_id=1;
        b.bounds=(nx_rect){20+i*130,250,120,36};b.name=span(strings[4+i]);b.label_layout=layouts[4+i];
        b.label_origin=(nx_point){b.bounds.x+8,b.bounds.y+8};b.glyph_cache_bytes=1024*1024;
        b.disabled=i && (!reviewed || r->owner->submitted || ae_files_rename_status(r->owner->operation)!=AE_FILES_RENAME_PREPARED);
        e=nx_button(r->ctx,&b,&state);if(e==NX_OK){if(i)confirm=state.clicked!=0;else cancel=state.clicked!=0;}}
    if(e==NX_OK && scrolling && info.max_y>0){
        for(unsigned i=0;i<2 && e==NX_OK;++i){nx_control_desc page={0};page.id=32+i;page.parent_id=1;
            page.bounds=(nx_rect){180+i*100,207,90,32};page.name=span(i?"Next page":"Previous page");
            page.label_layout=layouts[6+i];page.label_origin=(nx_point){page.bounds.x+5,page.bounds.y+6};
            page.glyph_cache_bytes=1024*1024;page.disabled=i?r->scroll_y>=info.max_y:r->scroll_y<=0;
            e=nx_button(r->ctx,&page,&state);
            if(e==NX_OK && state.clicked)r->scroll_y=i?fminf(info.max_y,r->scroll_y+110):fmaxf(0,r->scroll_y-110);
        }
    }
    if(overlay_open){nx_result end=nx_overlay_end(r->ctx);if(e==NX_OK)e=end;}
    if(e==NX_OK && scrolling && info.offset_y>=info.max_y)r->seen_bottom=1;
    if(e==NX_OK && (cancel || dismissed)){*action=REVIEW_CANCEL;r->open=0;e=nx_overlay_close(r->ctx);}
    else if(e==NX_OK && confirm){*action=REVIEW_CONFIRM;r->owner->submitted=1;}
    {nx_result end=nx_end_frame(r->ctx);if(e==NX_OK)e=end;}
done:
    for(unsigned i=0;i<8;++i)if(layouts[i])nx_text_layout_destroy(layouts[i]);
    return e;
}
