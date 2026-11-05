#include "view.h"
#include <stdlib.h>
#include <string.h>

struct aetheri_desktop_view {
    struct ae_desktop_model *model;
    nx_context *context;
    nx_font *font;
    nx_font_collection *fonts;
};
void aetheri_desktop_view_destroy(aetheri_desktop_view *v) {
    if(!v)return;
    nx_context_destroy(v->context);nx_font_collection_destroy(v->fonts);nx_font_close(v->font);free(v);
}
int aetheri_desktop_view_create(struct ae_desktop_model *model,const void *font,size_t length,aetheri_desktop_view **out) {
    if(!out)return 0;
    *out=NULL;
    if(!model || !font || !length)return 0;
    aetheri_desktop_view *v=calloc(1,sizeof(*v));if(!v)return 0;v->model=model;
    if(nx_context_create(NULL,&v->context)!=NX_OK || nx_font_open(font,length,NULL,&v->font)!=NX_OK ||
       nx_font_collection_create(NULL,&v->fonts)!=NX_OK || nx_font_collection_add(v->fonts,v->font)!=NX_OK) {
        aetheri_desktop_view_destroy(v);return 0;
    }
    *out=v;return 1;
}
nx_context *aetheri_desktop_view_context(aetheri_desktop_view *v) {return v?v->context:NULL;}
int aetheri_desktop_view_frame(aetheri_desktop_view *v,float width,float height,aetheri_desktop_intent *intent) {
    if(!v || !intent || width<320 || height<84)return 0;
    *intent=(aetheri_desktop_intent){0};aetheri_desktop_intent pending={0};
    const char *labels[]={v->model->german?"Suchen":"Search",v->model->german?"System":"Settings","Terminal"};
    nx_text_layout *layouts[3]={0};int okay=0;
    for(int i=0;i<3;++i)
        if(nx_text_layout_create(v->fonts,labels[i],strlen(labels[i]),NX_BIDI_LTR,16,width,&layouts[i])!=NX_OK)goto done;
    if(nx_begin_frame(v->context,width,height,0)!=NX_OK)goto done;
    nx_semantic_desc root={0};root.id=1;root.role=NX_ROLE_WINDOW;root.bounds=(nx_rect){0,0,width,height};
    root.name=(nx_utf8_span){"Aetheri Desktop",15};
    if(nx_semantics_add(v->context,&root,NULL)!=NX_OK)goto done;
    float button_width=(width-48)/3;
    for(int i=0;i<3;++i) {
        nx_control_desc d={0};d.id=(uint64_t)(10+i);d.parent_id=1;
        d.bounds=(nx_rect){8+(float)i*(button_width+8),height-70,button_width,44};
        d.name=(nx_utf8_span){labels[i],strlen(labels[i])};d.label_layout=layouts[i];
        d.label_origin=(nx_point){d.bounds.x+8,d.bounds.y+8};d.glyph_cache_bytes=1024u*1024u;
        d.disabled=i>0 && v->model->pending[i==1?AE_DESKTOP_SETTINGS:AE_DESKTOP_TERMINAL];
        nx_interaction_state state;if(nx_button(v->context,&d,&state)!=NX_OK)goto done;
        if(state.clicked) {
            pending.kind=i?AETHERI_DESKTOP_LAUNCH:AETHERI_DESKTOP_SEARCH;
            pending.app=i==1?AE_DESKTOP_SETTINGS:i==2?AE_DESKTOP_TERMINAL:AE_DESKTOP_APP_NONE;
        }
    }
    if(nx_end_frame(v->context)!=NX_OK)goto done;
    *intent=pending;okay=1;
done:
    for(int i=0;i<3;++i)nx_text_layout_destroy(layouts[i]);
    return okay;
}
