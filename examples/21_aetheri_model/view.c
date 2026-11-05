#include "view.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CACHE (1024u * 1024u)
struct aetheri_view {
    struct ae_settings *model;
    nx_context *context;
    nx_font *font;
    nx_font_collection *fonts;
    nx_text_layout *labels[15];
    nx_text_layout *displays[AE_SETTINGS_DISPLAYS];
    int close_requested;
    float scroll_x, scroll_y, revealed_width, revealed_height;
    unsigned revealed_text_percent;
    int revealed_german;
    uint64_t revealed_focus;
};
static nx_utf8_span span(const char *s) { return (nx_utf8_span){s, strlen(s)}; }
/* The shared layout deliberately preserves unbreakable words even when wider
   than its limit. In a narrow Settings viewport, add emergency line breaks at
   grapheme boundaries; semantic names continue to use the original bytes. */
static int small_text_layout(nx_font_collection *fonts,const char *text,size_t length,
                             double size,double width,nx_text_layout **out) {
    if(nx_text_layout_create(fonts,text,length,NX_BIDI_LTR,size,width,out)!=NX_OK) return 0;
    if(nx_text_layout_width(*out)<=width) return 1;
    nx_text_layout_destroy(*out); *out=NULL;
    if(length>(SIZE_MAX-1)/2) return 0;
    char *wrapped=malloc(length*2+1); if(!wrapped) return 0;
    size_t line=0,pos=0,written=0;
    while(pos<length) {
        size_t next;
        nx_text_layout *probe=NULL;
        if(nx_grapheme_next(text,length,pos,&next)!=NX_OK ||
           nx_text_layout_create(fonts,text+line,next-line,NX_BIDI_LTR,size,width,&probe)!=NX_OK) {
            free(wrapped); return 0;
        }
        int overflow=nx_text_layout_width(probe)>width;
        nx_text_layout_destroy(probe);
        if(overflow) {
            if(pos==line) {free(wrapped);return 0;}
            wrapped[written++]='\n'; line=pos;
        }
        memcpy(wrapped+written,text+pos,next-pos); written+=next-pos; pos=next;
    }
    nx_result result=nx_text_layout_create(fonts,wrapped,written,NX_BIDI_LTR,size,width,out);
    free(wrapped);
    if(result!=NX_OK) return 0;
    if(nx_text_layout_width(*out)>width) {nx_text_layout_destroy(*out);*out=NULL;return 0;}
    return 1;
}
void aetheri_view_destroy(aetheri_view *v) {
    if (!v) return;
    for (size_t i=0;i<15;++i) nx_text_layout_destroy(v->labels[i]);
    for (size_t i=0;i<AE_SETTINGS_DISPLAYS;++i) nx_text_layout_destroy(v->displays[i]);
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
    if (!v || !changed || width<128 || height<150) return 0;
    *changed=0;
    struct ae_settings *m=v->model;
    if (m->count>AE_SETTINGS_DISPLAYS || (m->count && m->selected>=m->count) ||
        m->theme<AE_SETTINGS_DARK || m->theme>AE_SETTINGS_CONTRAST ||
        (m->text_percent!=100 && m->text_percent!=150 && m->text_percent!=200)) return 0;
    const char *en[]={"Aetheri Settings", "Appearance", "Dark", "Light", "High contrast", "Language", "English", "German", "Text size", "100%", "150%", "200%", "Close"};
    const char *de[]={"Aetheri Einstellungen", "Darstellung", "Dunkel", "Hell", "Hoher Kontrast", "Sprache", "Englisch", "Deutsch", "Textgr\xc3\xb6\xc3\x9f" "e", "100%", "150%", "200%", "Schlie\xc3\x9f" "en"};
    const char **labels=m->german?de:en;
    for(size_t i=0;i<15;++i) { nx_text_layout_destroy(v->labels[i]); v->labels[i]=NULL; }
    for(size_t i=0;i<AE_SETTINGS_DISPLAYS;++i) { nx_text_layout_destroy(v->displays[i]); v->displays[i]=NULL; }
    int small=width<320 || height<240;
    float content_x=small?16:24, content_width=width-(small?32:64);
    double size=14.0*(double)m->text_percent/100.0;
    for(size_t i=0;i<13;++i) {
        double label_size=i==0?size*1.3:size;
        int option=(i>=2 && i<=4) || (i>=6 && i<=7) || (i>=9 && i<=11);
        float wrap=small?(i==12?content_width-32:content_width):width-(option?160:64);
        if(small ? !small_text_layout(v->fonts,labels[i],strlen(labels[i]),label_size,wrap,&v->labels[i]) :
           nx_text_layout_create(v->fonts,labels[i],strlen(labels[i]),NX_BIDI_LTR,
                                 label_size,wrap,&v->labels[i])!=NX_OK) return 0;
    }
    const char *display_heading=m->german?"Anzeigen":"Displays";
    const char *status=m->stale ? (m->german?"Anzeigedaten veraltet":"Display data is stale") :
        !m->refreshes ? (m->german?"Noch keine Anzeigedaten":"Display data not yet observed") :
        !m->count ? (m->german?"Keine Anzeigen beobachtet":"No displays observed") :
        (m->german?"Beobachtete Anzeigen":"Observed displays");
    if(small ? (!small_text_layout(v->fonts,display_heading,strlen(display_heading),size,content_width,&v->labels[13]) ||
                !small_text_layout(v->fonts,status,strlen(status),size,content_width,&v->labels[14])) :
       (nx_text_layout_create(v->fonts,display_heading,strlen(display_heading),NX_BIDI_LTR,size,content_width,&v->labels[13])!=NX_OK ||
        nx_text_layout_create(v->fonts,status,strlen(status),NX_BIDI_LTR,size,content_width,&v->labels[14])!=NX_OK)) return 0;
    char display_names[AE_SETTINGS_DISPLAYS][192]; nx_rect display_rects[AE_SETTINGS_DISPLAYS]={0};
    for(size_t i=0;i<m->count;++i) {
        const struct ae_settings_display *d=&m->displays[i];
        if(!d->id || !memchr(d->name,0,sizeof(d->name))) return 0;
        const char *selected=i==m->selected ? (m->german?" (ausgew\xc3\xa4hlt)":" (selected)") : "";
        int written=d->mode_known ? snprintf(display_names[i],sizeof(display_names[i]),"%s (%d x %d)%s",d->name,d->width,d->height,selected) :
                                   snprintf(display_names[i],sizeof(display_names[i]),"%s%s",d->name,selected);
        if(written<0 || (size_t)written>=sizeof(display_names[i])) return 0;
        if(small ? !small_text_layout(v->fonts,display_names[i],(size_t)written,size,content_width-16,&v->displays[i]) :
           nx_text_layout_create(v->fonts,display_names[i],(size_t)written,NX_BIDI_LTR,size,width-88,&v->displays[i])!=NX_OK) return 0;
    }
    /* Measure at the requested size and wrap whole controls, never shrink
       their text to fit an equal-width column. Coordinates are content-local. */
    const int starts[]={2,6,9}, counts[]={3,2,3}, headings[]={1,5,8};
    nx_rect items[3][3]={0}, groups_rect[3]={0};
    int compact=width<400 || small;
    float cursor_y=compact?(float)nx_text_layout_height(v->labels[0])+12:0;
    for(int row=0;row<3;++row) {
        float start_y=cursor_y, x=0, line_height=0;
        cursor_y+=(float)nx_text_layout_height(v->labels[headings[row]])+8;
        for(int i=0;i<counts[row];++i) {
            if(small) {
                items[row][i]=(nx_rect){0,cursor_y,content_width,42};
                cursor_y+=46+(float)nx_text_layout_height(v->labels[starts[row]+i])+12;
                continue;
            }
            float item_height=(float)nx_text_layout_height(v->labels[starts[row]+i])+10;
            if(item_height<42) item_height=42;
            float item_width=(float)nx_text_layout_width(v->labels[starts[row]+i])+item_height+22;
            if(item_width>content_width) item_width=content_width;
            if(x>0 && x+item_width>content_width) {x=0;cursor_y+=line_height+6;line_height=0;}
            items[row][i]=(nx_rect){x,cursor_y,item_width,item_height};
            if(item_height>line_height) line_height=item_height;
            x+=item_width+8;
        }
        if(!small) cursor_y+=line_height+12;
        groups_rect[row]=(nx_rect){0,start_y,content_width,cursor_y-start_y};
    }
    float displays_y=cursor_y;
    cursor_y+=(float)nx_text_layout_height(v->labels[13])+8;
    float status_y=cursor_y;
    cursor_y+=(float)nx_text_layout_height(v->labels[14])+8;
    for(size_t i=0;i<m->count;++i) {
        if(small) {
            display_rects[i]=(nx_rect){0,cursor_y,content_width,42};
            cursor_y+=46+(float)nx_text_layout_height(v->displays[i])+8;
            continue;
        }
        float item_height=(float)nx_text_layout_height(v->displays[i])+16;
        if(item_height<42) item_height=42;
        display_rects[i]=(nx_rect){0,cursor_y,content_width,item_height};
        cursor_y+=item_height+8;
    }
    float displays_end=cursor_y;
    float close_width=(float)nx_text_layout_width(v->labels[12])+32;
    float close_height=(float)nx_text_layout_height(v->labels[12])+10;
    if(close_height<42) close_height=42;
    nx_rect small_close={0,cursor_y,close_width,close_height};
    if(small_close.width>content_width) small_close.width=content_width;
    if(small) cursor_y+=close_height+8;
    float title_height=(float)nx_text_layout_height(v->labels[0]);
    int close_below=24+(float)nx_text_layout_width(v->labels[0])+16+close_width>width-24;
    float close_y=!compact && close_below?20+title_height+8:16;
    float header_bottom=close_y+50;
    if(!compact && header_bottom<20+title_height+8) header_bottom=20+title_height+8;
    nx_theme theme;
    nx_theme_kind kind=m->theme==AE_SETTINGS_LIGHT?NX_THEME_LIGHT:m->theme==AE_SETTINGS_CONTRAST?NX_THEME_HIGH_CONTRAST:NX_THEME_DARK;
    if(nx_theme_preset(kind,&theme)!=NX_OK || nx_context_set_theme(v->context,&theme)!=NX_OK ||
       nx_begin_frame(v->context,width,height,0)!=NX_OK) return 0;
    nx_semantic_desc root={.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,width,height},.name=span(labels[0])};
    nx_color fg, border;
    if(nx_semantics_add(v->context,&root,NULL)!=NX_OK || nx_theme_color(v->context,NX_COLOR_TEXT,&fg)!=NX_OK ||
       nx_theme_color(v->context,NX_COLOR_BORDER,&border)!=NX_OK) return 0;
    if(!compact && nx_draw_text_layout(v->context,v->labels[0],(nx_point){24,20},fg,CACHE)!=NX_OK) return 0;
    uint64_t theme_id=(uint64_t)m->theme+1, lang_id=m->german?2u:1u, size_id=m->text_percent==100?1u:m->text_percent==150?2u:3u;
    uint64_t old_theme=theme_id, old_lang=lang_id, old_size=size_id;
    nx_scroll_area_desc scroll={0}; scroll.id=5; scroll.parent_id=1;
    scroll.name=span(m->german?"Einstellungen":"Settings");
    scroll.viewport=small?(nx_rect){8,8,width-16,height-16}:(nx_rect){24,header_bottom,width-48,height-header_bottom-12};
    scroll.content_size=(nx_size){scroll.viewport.width,cursor_y}; scroll.axes=NX_SCROLL_Y;
    uint64_t focus=nx_focus_id(v->context);
    if(focus!=v->revealed_focus || width!=v->revealed_width || height!=v->revealed_height ||
       m->text_percent!=v->revealed_text_percent || m->german!=v->revealed_german) {
        for(int row=0;row<3;++row) for(int i=0;i<counts[row];++i) {
            if(focus==(uint64_t)(10+row*3+i)) {
                float top=items[row][i].y, bottom=top+items[row][i].height;
                if(small) bottom=top+fminf(scroll.viewport.height,46+(float)nx_text_layout_height(v->labels[starts[row]+i]));
                if(top<v->scroll_y) v->scroll_y=top;
                else if(bottom>v->scroll_y+scroll.viewport.height) v->scroll_y=bottom-scroll.viewport.height;
            }
        }
        for(size_t i=0;i<m->count;++i) if(focus==(UINT64_C(0x100000000)+m->displays[i].id)) {
            float top=display_rects[i].y, bottom=top+display_rects[i].height;
            if(small) bottom=top+fminf(scroll.viewport.height,46+(float)nx_text_layout_height(v->displays[i]));
            if(top<v->scroll_y) v->scroll_y=top;
            else if(bottom>v->scroll_y+scroll.viewport.height) v->scroll_y=bottom-scroll.viewport.height;
        }
        if(small && focus==19) {
            float top=small_close.y, bottom=top+small_close.height;
            if(top<v->scroll_y) v->scroll_y=top;
            else if(bottom>v->scroll_y+scroll.viewport.height) v->scroll_y=bottom-scroll.viewport.height;
        }
        v->revealed_focus=focus; v->revealed_width=width; v->revealed_height=height;
        v->revealed_text_percent=m->text_percent; v->revealed_german=m->german;
    }
    if(small && focus) {
        const nx_event *events=NULL; size_t count=0;
        if(nx_frame_events(v->context,&events,&count)!=NX_OK) return 0;
        for(size_t i=0;i<count;++i) if(events[i].type==NX_EVENT_KEY_DOWN && !events[i].modifiers) {
            if(events[i].key==NX_KEY_PAGE_DOWN) v->scroll_y+=scroll.viewport.height*0.5f;
            else if(events[i].key==NX_KEY_PAGE_UP) v->scroll_y=fmaxf(0,v->scroll_y-scroll.viewport.height*0.5f);
        }
    }
    nx_scroll_info scroll_info;
    if(nx_scroll_area_begin(v->context,&scroll,&v->scroll_x,&v->scroll_y,&scroll_info)!=NX_OK) return 0;
    if(compact && nx_draw_text_layout(v->context,v->labels[0],
        (nx_point){content_x,scroll.viewport.y-v->scroll_y},fg,CACHE)!=NX_OK) return 0;
    uint64_t *groups[]={&theme_id,&lang_id,&size_id};
    for(int row=0;row<3;++row) {
        float y=scroll.viewport.y-v->scroll_y;
        nx_rect g=groups_rect[row]; g.x+=content_x; g.y+=y;
        nx_semantic_desc group={.id=(uint64_t)(2+row),.parent_id=5,.role=NX_ROLE_GROUP,
                                .bounds=g,.name=span(labels[headings[row]])};
        if(nx_semantics_add(v->context,&group,NULL)!=NX_OK) return 0;
        if(nx_draw_text_layout(v->context,v->labels[headings[row]],(nx_point){content_x,g.y},fg,CACHE)!=NX_OK) return 0;
        for(int i=0;i<counts[row];++i) {
            int label=starts[row]+i;
            nx_control_desc control={0}; control.id=(uint64_t)(10+row*3+i); control.parent_id=(uint64_t)(2+row);
            control.bounds=items[row][i]; control.bounds.x+=content_x; control.bounds.y+=y;
            control.name=span(labels[label]); control.label_layout=small?NULL:v->labels[label];
            control.label_origin=(nx_point){control.bounds.x+control.bounds.height+6,control.bounds.y+5}; control.glyph_cache_bytes=CACHE;
            nx_interaction_state state;
            if(nx_radio(v->context,&control,(uint64_t)i+1,groups[row],&state)!=NX_OK) return 0;
            if(small && nx_draw_text_layout(v->context,v->labels[label],
                (nx_point){content_x,control.bounds.y+46},fg,CACHE)!=NX_OK) return 0;
            /* The preset's raised fill can match the scroll surface. Keep an
               explicit boundary around each mark, including unchecked ones. */
            nx_rect mark={control.bounds.x,control.bounds.y,control.bounds.height,control.bounds.height};
            if(nx_stroke_rect(v->context,mark,2,border)!=NX_OK) return 0;
        }
    }
    float dy=scroll.viewport.y-v->scroll_y;
    nx_semantic_desc displays_group={.id=6,.parent_id=5,.role=NX_ROLE_GROUP,
        .bounds={content_x,dy+displays_y,content_width,displays_end-displays_y},.name=span(display_heading)};
    nx_semantic_desc display_status={.id=7,.parent_id=6,.role=NX_ROLE_STATUS,
        .bounds={content_x,dy+status_y,content_width,(float)nx_text_layout_height(v->labels[14])},.name=span(status)};
    if(nx_semantics_add(v->context,&displays_group,NULL)!=NX_OK ||
       nx_semantics_add(v->context,&display_status,NULL)!=NX_OK ||
       nx_draw_text_layout(v->context,v->labels[13],(nx_point){content_x,dy+displays_y},fg,CACHE)!=NX_OK ||
       nx_draw_text_layout(v->context,v->labels[14],(nx_point){content_x,dy+status_y},fg,CACHE)!=NX_OK) return 0;
    uint32_t chosen_display=0;
    for(size_t i=0;i<m->count;++i) {
        nx_control_desc control={0}; control.id=UINT64_C(0x100000000)+m->displays[i].id; control.parent_id=6;
        control.bounds=display_rects[i]; control.bounds.x+=content_x; control.bounds.y+=dy;
        control.name=span(display_names[i]); control.disabled=m->stale;
        control.label_layout=small?NULL:v->displays[i]; control.label_origin=(nx_point){control.bounds.x+8,control.bounds.y+8};
        control.glyph_cache_bytes=CACHE; nx_interaction_state state;
        if(nx_button(v->context,&control,&state)!=NX_OK) return 0;
        if(small && nx_draw_text_layout(v->context,v->displays[i],
            (nx_point){content_x,control.bounds.y+46},fg,CACHE)!=NX_OK) return 0;
        if(state.clicked) chosen_display=m->displays[i].id;
    }
    if(!small && nx_scroll_area_end(v->context)!=NX_OK) return 0;
    nx_control_desc close={0}; close.id=19; close.parent_id=small?5:1;
    close.bounds=small?small_close:(nx_rect){width-24-close_width,close_y,close_width,42};
    if(small) {close.bounds.x+=content_x;close.bounds.y+=dy;}
    close.name=span(labels[12]);
    close.label_layout=v->labels[12]; close.label_origin=(nx_point){close.bounds.x+16,close.bounds.y+5}; close.glyph_cache_bytes=CACHE;
    nx_interaction_state close_state;
    if(nx_button(v->context,&close,&close_state)!=NX_OK) return 0;
    if(close_state.clicked) v->close_requested=1;
    if(small && nx_scroll_area_end(v->context)!=NX_OK) return 0;
    if(nx_end_frame(v->context)!=NX_OK) return 0;
    if(theme_id!=old_theme || lang_id!=old_lang || size_id!=old_size) {
        m->theme=(enum ae_settings_theme)(theme_id-1); m->german=lang_id==2;
        m->text_percent=size_id==1?100u:size_id==2?150u:200u; *changed=1;
    }
    if(chosen_display && !m->stale && m->displays[m->selected].id!=chosen_display) {
        if(!ae_settings_select(m,chosen_display)) return 0;
        *changed=1;
    }
    return 1;
}
