#include "view.h"
#include <stdlib.h>
#include <string.h>

struct aetheri_desktop_view {
    struct ae_desktop_model *model;
    nx_context *context;
    nx_font *font;
    nx_font_collection *fonts;
    float scroll_x,scroll_y;
    uint64_t model_revision;
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
int aetheri_desktop_view_frame(aetheri_desktop_view *v,float width,float height,uint64_t revision,aetheri_desktop_intent *intent) {
    if(!intent)return 0;
    *intent=(aetheri_desktop_intent){0};
    if(!v || !revision || revision<v->model_revision || width<320 || height<84 || (v->model->search && height<240) ||
       v->model->count>AE_DESKTOP_WINDOWS)return 0;
    aetheri_desktop_intent pending={0};
    const char *labels[]={v->model->german?"Suchen":"Search",v->model->german?"System":"Settings","Terminal"};
    nx_text_layout *layouts[3]={0},*result_layouts[AE_DESKTOP_RESULTS]={0};int okay=0;
    struct ae_desktop_result results[AE_DESKTOP_RESULTS];size_t result_count=0;
    float result_y[AE_DESKTOP_RESULTS]={0},result_height[AE_DESKTOP_RESULTS]={0},content_height=0;
    for(int i=0;i<3;++i)
        if(nx_text_layout_create(v->fonts,labels[i],strlen(labels[i]),NX_BIDI_LTR,16,width,&layouts[i])!=NX_OK)goto done;
    if(v->model->search) {
        result_count=ae_desktop_model_results(v->model,results);
        if(result_count>AE_DESKTOP_RESULTS)goto done;
        for(size_t i=0;i<result_count;++i) {
            if(nx_text_layout_create(v->fonts,results[i].label,strlen(results[i].label),NX_BIDI_LTR,16,width-64,&result_layouts[i])!=NX_OK)goto done;
            float h=(float)nx_text_layout_height(result_layouts[i])+16;
            if(h<44)h=44;
            result_y[i]=content_height;result_height[i]=h;content_height+=h+8;
        }
    }
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
    if(v->model->search) {
        nx_scroll_area_desc scroll={0};scroll.id=30;scroll.parent_id=1;
        const char *heading=v->model->german?"Suchergebnisse":"Search results";
        scroll.name=(nx_utf8_span){heading,strlen(heading)};
        scroll.viewport=(nx_rect){16,16,width-32,height-104};
        scroll.content_size=(nx_size){width-32,content_height};scroll.axes=NX_SCROLL_Y;
        nx_scroll_info info;
        if(nx_scroll_area_begin(v->context,&scroll,&v->scroll_x,&v->scroll_y,&info)!=NX_OK)goto done;
        for(size_t i=0;i<result_count;++i) {
            nx_control_desc d={0};
            d.id=results[i].window ? UINT64_C(0x100000000)+results[i].window : UINT64_C(0x200000000)+(uint64_t)results[i].app;
            d.parent_id=30;d.bounds=(nx_rect){24,scroll.viewport.y-v->scroll_y+result_y[i],width-48,result_height[i]};
            d.name=(nx_utf8_span){results[i].label,strlen(results[i].label)};d.label_layout=result_layouts[i];
            d.label_origin=(nx_point){d.bounds.x+8,d.bounds.y+8};d.glyph_cache_bytes=1024u*1024u;
            d.disabled=results[i].app && v->model->pending[results[i].app];
            nx_interaction_state state;if(nx_button(v->context,&d,&state)!=NX_OK)goto done;
            if(state.clicked) {
                pending.kind=results[i].window?AETHERI_DESKTOP_ACTIVATE:AETHERI_DESKTOP_LAUNCH;
                pending.window=results[i].window;pending.app=results[i].app;
            }
        }
        if(nx_scroll_area_end(v->context)!=NX_OK)goto done;
    }
    if(nx_end_frame(v->context)!=NX_OK)goto done;
    if(!v->model_revision || revision==v->model_revision)*intent=pending;
    v->model_revision=revision;okay=1;
done:
    for(int i=0;i<3;++i)nx_text_layout_destroy(layouts[i]);
    for(size_t i=0;i<AE_DESKTOP_RESULTS;++i)nx_text_layout_destroy(result_layouts[i]);
    return okay;
}
