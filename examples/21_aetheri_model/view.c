#include "view.h"
#include <stdlib.h>
#include <string.h>

#define CACHE (1024u * 1024u)
struct aetheri_view {
    struct ae_settings *model;
    nx_context *context;
    nx_font *font;
    nx_font_collection *fonts;
    nx_text_layout *labels[13];
    int close_requested;
};
static nx_utf8_span span(const char *s) { return (nx_utf8_span){s, strlen(s)}; }
void aetheri_view_destroy(aetheri_view *v) {
    if (!v) return;
    for (size_t i=0;i<13;++i) nx_text_layout_destroy(v->labels[i]);
    nx_context_destroy(v->context); nx_font_collection_destroy(v->fonts);
    nx_font_close(v->font); free(v);
}
int aetheri_view_create(struct ae_settings *model, const void *font, size_t length, aetheri_view **out) {
    if (!out) return 0;
    *out=NULL;
    if (!model || !font || !length) return 0;
    aetheri_view *v=calloc(1,sizeof(*v)); if (!v) return 0;
    v->model=model;
    if (nx_context_create(NULL,&v->context)!=NX_OK || nx_font_open(font,length,NULL,&v->font)!=NX_OK ||
        nx_font_collection_create(NULL,&v->fonts)!=NX_OK || nx_font_collection_add(v->fonts,v->font)!=NX_OK) {
        aetheri_view_destroy(v); return 0;
    }
    *out=v; return 1;
}
nx_context *aetheri_view_context(aetheri_view *v) { return v?v->context:NULL; }
int aetheri_view_close_requested(const aetheri_view *v) { return v && v->close_requested; }
int aetheri_view_frame(aetheri_view *v, float width, float height, int *changed) {
    if (!v || !changed || width<480 || height<380) return 0;
    *changed=0;
    struct ae_settings *m=v->model;
    if (m->theme<AE_SETTINGS_DARK || m->theme>AE_SETTINGS_CONTRAST ||
        (m->text_percent!=100 && m->text_percent!=150 && m->text_percent!=200)) return 0;
    const char *en[]={"Aetheri Settings", "Appearance", "Dark", "Light", "High contrast", "Language", "English", "German", "Text size", "100%", "150%", "200%", "Close"};
    const char *de[]={"Aetheri Einstellungen", "Darstellung", "Dunkel", "Hell", "Hoher Kontrast", "Sprache", "Englisch", "Deutsch", "Textgr\xc3\xb6\xc3\x9f" "e", "100%", "150%", "200%", "Schlie\xc3\x9f" "en"};
    const char **labels=m->german?de:en;
    for(size_t i=0;i<13;++i) { nx_text_layout_destroy(v->labels[i]); v->labels[i]=NULL; }
    double size=14.0*(double)m->text_percent/100.0;
    for(size_t i=0;i<13;++i) {
        double label_size=i==0?size*1.3:size;
        if(nx_text_layout_create(v->fonts,labels[i],strlen(labels[i]),NX_BIDI_LTR,
                                 label_size,width-64,&v->labels[i])!=NX_OK) return 0;
        if(i==0 || i==12 || (i>=2 && i<=4) || (i>=6 && i<=7) || (i>=9 && i<=11)) {
            int columns=(i>=6 && i<=7)?2:3;
            double available=i==0?width-190:i==12?110:(width-48)/columns-58;
            double actual=nx_text_layout_width(v->labels[i]);
            if(actual>available && available>0) {
                label_size*=0.98*available/actual;
                nx_text_layout_destroy(v->labels[i]); v->labels[i]=NULL;
                if(nx_text_layout_create(v->fonts,labels[i],strlen(labels[i]),NX_BIDI_LTR,
                                         label_size,width-64,&v->labels[i])!=NX_OK) return 0;
            }
        }
    }
    nx_theme theme;
    nx_theme_kind kind=m->theme==AE_SETTINGS_LIGHT?NX_THEME_LIGHT:m->theme==AE_SETTINGS_CONTRAST?NX_THEME_HIGH_CONTRAST:NX_THEME_DARK;
    if(nx_theme_preset(kind,&theme)!=NX_OK || nx_context_set_theme(v->context,&theme)!=NX_OK ||
       nx_begin_frame(v->context,width,height,0)!=NX_OK) return 0;
    nx_semantic_desc root={.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,width,height},.name=span(labels[0])};
    nx_color fg;
    if(nx_semantics_add(v->context,&root,NULL)!=NX_OK || nx_theme_color(v->context,NX_COLOR_TEXT,&fg)!=NX_OK ||
       nx_draw_text_layout(v->context,v->labels[0],(nx_point){24,20},fg,CACHE)!=NX_OK) return 0;
    uint64_t theme_id=(uint64_t)m->theme+1, lang_id=m->german?2u:1u, size_id=m->text_percent==100?1u:m->text_percent==150?2u:3u;
    uint64_t old_theme=theme_id, old_lang=lang_id, old_size=size_id;
    float row_height=(height-80)/3.0f;
    const int starts[]={2,6,9}, counts[]={3,2,3}, headings[]={1,5,8};
    uint64_t *groups[]={&theme_id,&lang_id,&size_id};
    for(int row=0;row<3;++row) {
        float y=70+row*row_height;
        nx_semantic_desc group={.id=(uint64_t)(2+row),.parent_id=1,.role=NX_ROLE_GROUP,
                                .bounds={24,y,width-48,row_height},.name=span(labels[headings[row]])};
        if(nx_semantics_add(v->context,&group,NULL)!=NX_OK) return 0;
        if(nx_draw_text_layout(v->context,v->labels[headings[row]],(nx_point){24,y},fg,CACHE)!=NX_OK) return 0;
        float item_width=(width-48)/(float)counts[row];
        for(int i=0;i<counts[row];++i) {
            int label=starts[row]+i;
            nx_control_desc control={0}; control.id=(uint64_t)(10+row*3+i); control.parent_id=(uint64_t)(2+row);
            control.bounds=(nx_rect){24+i*item_width,y+32,item_width-8,42};
            control.name=span(labels[label]); control.label_layout=v->labels[label];
            control.label_origin=(nx_point){control.bounds.x+48,control.bounds.y+5}; control.glyph_cache_bytes=CACHE;
            nx_interaction_state state;
            if(nx_radio(v->context,&control,(uint64_t)i+1,groups[row],&state)!=NX_OK) return 0;
        }
    }
    nx_control_desc close={0}; close.id=19; close.parent_id=1;
    close.bounds=(nx_rect){width-150,16,126,42}; close.name=span(labels[12]);
    close.label_layout=v->labels[12]; close.label_origin=(nx_point){width-142,21}; close.glyph_cache_bytes=CACHE;
    nx_interaction_state close_state;
    if(nx_button(v->context,&close,&close_state)!=NX_OK) return 0;
    if(close_state.clicked) v->close_requested=1;
    if(nx_end_frame(v->context)!=NX_OK) return 0;
    if(theme_id!=old_theme || lang_id!=old_lang || size_id!=old_size) {
        m->theme=(enum ae_settings_theme)(theme_id-1); m->german=lang_id==2;
        m->text_percent=size_id==1?100u:size_id==2?150u:200u; *changed=1;
    }
    return 1;
}
