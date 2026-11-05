#include "review.h"
#include "files/model.h"
#include <stdlib.h>
#include <string.h>
struct rename_review { rename_review_owner *owner; nx_font_collection *fonts; nx_context *ctx; int open; };
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
    char old_display[AE_FILES_LABEL+8],new_display[AE_FILES_LABEL+8];
    const char *old=ae_files_rename_old(r->owner->operation),*next=ae_files_rename_new(r->owner->operation);
    format_name(old,old_name);format_name(next,new_name);
    memcpy(old_display,"From: ",6);memcpy(old_display+6,old_name,strlen(old_name)+1);
    memcpy(new_display,"To: ",4);memcpy(new_display+4,new_name,strlen(new_name)+1);
    enum ae_files_rename_result operation_status=ae_files_rename_status(r->owner->operation);
    const char *message=r->owner->submitted && operation_status==AE_FILES_RENAME_PREPARED ?
        "Confirmation requested; awaiting result" : status(operation_status);
    const char *strings[]={"Rename entry",old_display,new_display,message,"Cancel","Confirm"};
    const char *values[]={strings[0],old_name,new_name,strings[3]};
    nx_text_layout *layouts[6]={0};nx_result e=NX_OK;
    for(unsigned i=0;i<6 && e==NX_OK;++i){e=nx_text_layout_create(r->fonts,strings[i],strlen(strings[i]),NX_BIDI_AUTO,16,360,&layouts[i]);
        if(e==NX_OK && (nx_text_layout_height(layouts[i])>60 || nx_text_layout_width(layouts[i])>360))e=NX_ERROR_LIMIT;}
    if(e!=NX_OK)goto done;
    e=nx_begin_frame(r->ctx,400,300,0);if(e!=NX_OK)goto done;
    nx_color color;e=nx_theme_color(r->ctx,NX_COLOR_TEXT,&color);
    nx_control_desc background={0};background.id=20;background.bounds=(nx_rect){0,0,400,300};background.name=span("Files");
    nx_interaction_state state;if(e==NX_OK)e=nx_button(r->ctx,&background,&state);
    nx_overlay_desc modal={0};modal.panel.id=1;modal.panel.bounds=(nx_rect){8,4,384,292};
    modal.panel.name=span("Rename review");modal.backdrop_id=21;modal.dismiss_on_escape=1;
    int dismissed=0,overlay_open=0;
    if(e==NX_OK){e=nx_modal_begin(r->ctx,&modal,&dismissed);if(e==NX_OK)overlay_open=1;}
    nx_rect boxes[]={{20,14,360,26},{20,48,360,60},{20,112,360,60},{20,180,360,60}};
    const char *names[]={"Rename entry","Original name","New name","Rename status"};
    for(unsigned i=0;e==NX_OK && i<4;++i){nx_semantic_desc item={0};item.id=2+i;item.parent_id=1;
        item.role=i==3?NX_ROLE_STATUS:NX_ROLE_TEXT;item.bounds=boxes[i];item.name=span(names[i]);item.value=span(values[i]);
        e=nx_semantics_add(r->ctx,&item,NULL);if(e==NX_OK)e=nx_push_clip(r->ctx,boxes[i]);
        if(e==NX_OK){e=nx_draw_text_layout(r->ctx,layouts[i],(nx_point){boxes[i].x,boxes[i].y},color,1024*1024);
            nx_result pop=nx_pop_clip(r->ctx);if(e==NX_OK)e=pop;}}
    int cancel=0,confirm=0;
    for(unsigned i=0;e==NX_OK && i<2;++i){nx_control_desc b={0};b.id=10+i;b.parent_id=1;
        b.bounds=(nx_rect){20+i*130,250,120,36};b.name=span(strings[4+i]);b.label_layout=layouts[4+i];
        b.label_origin=(nx_point){b.bounds.x+8,b.bounds.y+8};b.glyph_cache_bytes=1024*1024;
        b.disabled=i && (r->owner->submitted || ae_files_rename_status(r->owner->operation)!=AE_FILES_RENAME_PREPARED);
        e=nx_button(r->ctx,&b,&state);if(e==NX_OK){if(i)confirm=state.clicked!=0;else cancel=state.clicked!=0;}}
    if(overlay_open){nx_result end=nx_overlay_end(r->ctx);if(e==NX_OK)e=end;}
    if(e==NX_OK && (cancel || dismissed)){*action=REVIEW_CANCEL;r->open=0;e=nx_overlay_close(r->ctx);}
    else if(e==NX_OK && confirm){*action=REVIEW_CONFIRM;r->owner->submitted=1;}
    {nx_result end=nx_end_frame(r->ctx);if(e==NX_OK)e=end;}
done:
    for(unsigned i=0;i<6;++i)if(layouts[i])nx_text_layout_destroy(layouts[i]);
    return e;
}
