#ifndef NEXAUI_TEST_TRANSFER_ICON_SCENE_H
#define NEXAUI_TEST_TRANSFER_ICON_SCENE_H
#include "nexaui/nexaui.h"
#include <string.h>

/* Equal subpixel phase in every column/row at 1/1.25/1.5/2x. The entire
   90x70 scene fits the native fixture's 180x140 target even at 2x. */
static nx_rect transfer_icon_bounds(unsigned column,unsigned row) {
    return (nx_rect){4.25f+28.f*column,4.5f+32.f*row,24,24};
}
static nx_rect transfer_icon_outer_clip(void) {
    return (nx_rect){5.5f,34.25f,74.75f,28.25f};
}
static nx_rect transfer_icon_inner_clip(unsigned column) {
    nx_rect r=transfer_icon_bounds(column,1);
    return (nx_rect){r.x+3.25f,r.y+4.25f,16.5f,15.5f};
}
static int transfer_icon_scene_sized(nx_context *ui,nx_theme_kind kind,float opacity,
                                    float size,nx_color *background) {
    nx_theme theme;
    if(nx_theme_preset(kind,&theme)!=NX_OK || nx_context_set_theme(ui,&theme)!=NX_OK ||
       nx_begin_frame(ui,90,70,0)!=NX_OK)return 1;
    *background=theme.colors[NX_COLOR_SURFACE];
    nx_color ink=theme.colors[NX_COLOR_TEXT];ink.a*=opacity;
    if(nx_fill_rect(ui,(nx_rect){0,0,90,70},*background)!=NX_OK)return 1;
    const nx_icon icons[]={NX_ICON_TRANSFER,NX_ICON_UPLOAD,NX_ICON_DOWNLOAD};
    for(unsigned row=0;row<2;++row) {
        if(row && nx_push_clip(ui,transfer_icon_outer_clip())!=NX_OK)return 1;
        for(unsigned column=0;column<3;++column) {
            if(row && nx_push_clip(ui,transfer_icon_inner_clip(column))!=NX_OK)return 1;
            nx_rect bounds=transfer_icon_bounds(column,row);bounds.width=bounds.height=size;
            if(nx_draw_icon(ui,icons[column],bounds,1.5f,ink)!=NX_OK)return 1;
            if(row && nx_pop_clip(ui)!=NX_OK)return 1;
        }
        if(row && nx_pop_clip(ui)!=NX_OK)return 1;
    }
    return nx_end_frame(ui)!=NX_OK;
}
static int transfer_icon_scene(nx_context *ui,nx_theme_kind kind,float opacity,
                              nx_color *background) {
    return transfer_icon_scene_sized(ui,kind,opacity,24,background);
}
static void transfer_icon_scale(const nx_command *source,size_t count,float scale,
                                nx_command *target) {
    for(size_t i=0;i<count;++i) {
        target[i]=source[i];nx_command *c=&target[i];
        c->rect.x*=scale;c->rect.y*=scale;c->rect.width*=scale;c->rect.height*=scale;
        c->thickness*=scale;c->radius*=scale;
        for(unsigned j=0;j<3;++j){c->points[j].x*=scale;c->points[j].y*=scale;}
    }
}
#endif
