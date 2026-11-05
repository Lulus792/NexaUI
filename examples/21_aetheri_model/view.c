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
    float scroll_x, scroll_y, revealed_width, revealed_height;
    unsigned revealed_text_percent;
    int revealed_german;
    uint64_t revealed_focus;
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
    }
    /* Measure at the requested size and wrap whole controls, never shrink
       their text to fit an equal-width column. Coordinates are content-local. */
    const int starts[]={2,6,9}, counts[]={3,2,3}, headings[]={1,5,8};
    nx_rect items[3][3]={0}, groups_rect[3]={0};
    float content_width=width-64, cursor_y=0;
    for(int row=0;row<3;++row) {
        float start_y=cursor_y, x=0;
        cursor_y+=(float)nx_text_layout_height(v->labels[headings[row]])+8;
        for(int i=0;i<counts[row];++i) {
            float item_width=(float)nx_text_layout_width(v->labels[starts[row]+i])+64;
            if(item_width>content_width) item_width=content_width;
            if(x>0 && x+item_width>content_width) {x=0;cursor_y+=48;}
            items[row][i]=(nx_rect){x,cursor_y,item_width,42};
            x+=item_width+8;
        }
        cursor_y+=54;
        groups_rect[row]=(nx_rect){0,start_y,content_width,cursor_y-start_y};
    }
    float close_width=(float)nx_text_layout_width(v->labels[12])+32;
    float title_height=(float)nx_text_layout_height(v->labels[0]);
    int close_below=24+(float)nx_text_layout_width(v->labels[0])+16+close_width>width-24;
    float close_y=close_below?20+title_height+8:16;
    float header_bottom=close_y+50;
    if(header_bottom<20+title_height+8) header_bottom=20+title_height+8;
    nx_theme theme;
    nx_theme_kind kind=m->theme==AE_SETTINGS_LIGHT?NX_THEME_LIGHT:m->theme==AE_SETTINGS_CONTRAST?NX_THEME_HIGH_CONTRAST:NX_THEME_DARK;
    if(nx_theme_preset(kind,&theme)!=NX_OK || nx_context_set_theme(v->context,&theme)!=NX_OK ||
       nx_begin_frame(v->context,width,height,0)!=NX_OK) return 0;
    nx_semantic_desc root={.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,width,height},.name=span(labels[0])};
    nx_color fg, border;
    if(nx_semantics_add(v->context,&root,NULL)!=NX_OK || nx_theme_color(v->context,NX_COLOR_TEXT,&fg)!=NX_OK ||
       nx_theme_color(v->context,NX_COLOR_BORDER,&border)!=NX_OK ||
       nx_draw_text_layout(v->context,v->labels[0],(nx_point){24,20},fg,CACHE)!=NX_OK) return 0;
    uint64_t theme_id=(uint64_t)m->theme+1, lang_id=m->german?2u:1u, size_id=m->text_percent==100?1u:m->text_percent==150?2u:3u;
    uint64_t old_theme=theme_id, old_lang=lang_id, old_size=size_id;
    nx_scroll_area_desc scroll={0}; scroll.id=5; scroll.parent_id=1;
    scroll.name=span(m->german?"Einstellungen":"Settings");
    scroll.viewport=(nx_rect){24,header_bottom,width-48,height-header_bottom-12};
    scroll.content_size=(nx_size){width-48,cursor_y}; scroll.axes=NX_SCROLL_Y;
    uint64_t focus=nx_focus_id(v->context);
    if(focus!=v->revealed_focus || width!=v->revealed_width || height!=v->revealed_height ||
       m->text_percent!=v->revealed_text_percent || m->german!=v->revealed_german) {
        for(int row=0;row<3;++row) for(int i=0;i<counts[row];++i) {
            if(focus==(uint64_t)(10+row*3+i)) {
                float top=items[row][i].y, bottom=top+items[row][i].height;
                if(top<v->scroll_y) v->scroll_y=top;
                else if(bottom>v->scroll_y+scroll.viewport.height) v->scroll_y=bottom-scroll.viewport.height;
            }
        }
        v->revealed_focus=focus; v->revealed_width=width; v->revealed_height=height;
        v->revealed_text_percent=m->text_percent; v->revealed_german=m->german;
    }
    nx_scroll_info scroll_info;
    if(nx_scroll_area_begin(v->context,&scroll,&v->scroll_x,&v->scroll_y,&scroll_info)!=NX_OK) return 0;
    uint64_t *groups[]={&theme_id,&lang_id,&size_id};
    for(int row=0;row<3;++row) {
        float y=scroll.viewport.y-v->scroll_y;
        nx_rect g=groups_rect[row]; g.x+=24; g.y+=y;
        nx_semantic_desc group={.id=(uint64_t)(2+row),.parent_id=5,.role=NX_ROLE_GROUP,
                                .bounds=g,.name=span(labels[headings[row]])};
        if(nx_semantics_add(v->context,&group,NULL)!=NX_OK) return 0;
        if(nx_draw_text_layout(v->context,v->labels[headings[row]],(nx_point){24,g.y},fg,CACHE)!=NX_OK) return 0;
        for(int i=0;i<counts[row];++i) {
            int label=starts[row]+i;
            nx_control_desc control={0}; control.id=(uint64_t)(10+row*3+i); control.parent_id=(uint64_t)(2+row);
            control.bounds=items[row][i]; control.bounds.x+=24; control.bounds.y+=y;
            control.name=span(labels[label]); control.label_layout=v->labels[label];
            control.label_origin=(nx_point){control.bounds.x+48,control.bounds.y+5}; control.glyph_cache_bytes=CACHE;
            nx_interaction_state state;
            if(nx_radio(v->context,&control,(uint64_t)i+1,groups[row],&state)!=NX_OK) return 0;
            /* The preset's raised fill can match the scroll surface. Keep an
               explicit boundary around each mark, including unchecked ones. */
            nx_rect mark={control.bounds.x,control.bounds.y,42,42};
            if(nx_stroke_rect(v->context,mark,2,border)!=NX_OK) return 0;
        }
    }
    if(nx_scroll_area_end(v->context)!=NX_OK) return 0;
    nx_control_desc close={0}; close.id=19; close.parent_id=1;
    close.bounds=(nx_rect){width-24-close_width,close_y,close_width,42}; close.name=span(labels[12]);
    close.label_layout=v->labels[12]; close.label_origin=(nx_point){close.bounds.x+16,close_y+5}; close.glyph_cache_bytes=CACHE;
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
