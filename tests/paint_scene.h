#ifndef NEXAUI_TEST_PAINT_SCENE_H
#define NEXAUI_TEST_PAINT_SCENE_H
#include "nexaui/nexaui.h"
static nx_result paint_plot_sample(void *user,size_t index,nx_series_point *point) {
    *point=((const nx_series_point *)user)[index];return NX_OK;
}
/* Authored reusable scene; resources survive every renderer/readback call. */
static int paint_scene(nx_context *ui,nx_paint_image *images[3],float density) {
    nx_color_stop stops[]={{0,{0,.65f,1,.9f}},{.4f,{.1f,.15f,.4f,.7f}},{1,{.6f,0,1,0}}};
    nx_surface_image_desc d={83.25f,48.5f,17,density,{0},4u*1024u*1024u};
    d.paint=(nx_paint){NX_PAINT_LINEAR,{0},{0,0},{83,48},stops,3};
    if(nx_surface_image_create(&d,NULL,&images[0])!=NX_OK) return 1;
    d.paint.kind=NX_PAINT_RADIAL;d.paint.from=(nx_point){40,24};d.paint.to=(nx_point){82,24};
    if(nx_surface_image_create(&d,NULL,&images[1])!=NX_OK) return 1;
    nx_shadow_image_desc shadow={55,27,12,density,2.5f,{3,-2},{0,.8f,1,.8f},4u*1024u*1024u};
    if(nx_shadow_image_create(&shadow,NULL,&images[2])!=NX_OK) return 1;
    if(nx_begin_frame(ui,180,140,0)!=NX_OK) return 1;
    if(nx_push_clip(ui,(nx_rect){2.5f,3.25f,167,128})!=NX_OK) return 1;
    if(nx_draw_paint_image(ui,images[2],(nx_point){32,28},(nx_color){1,1,1,1})!=NX_OK) return 1;
    if(nx_draw_paint_image(ui,images[0],(nx_point){9.25f,10.5f},(nx_color){1,1,1,1})!=NX_OK) return 1;
    if(nx_push_clip(ui,(nx_rect){52.5f,31.25f,89,72})!=NX_OK) return 1;
    if(nx_draw_paint_image(ui,images[1],(nx_point){58.5f,45.25f},(nx_color){.8f,1,1,.7f})!=NX_OK) return 1;
    if(nx_pop_clip(ui)!=NX_OK) return 1;
    nx_series_point download[]={{0,1},{1,3},{2,2},{3,6},{4,2},{5,4}};
    nx_series_point upload[]={{0,.5},{1,1},{2,.5},{3,2},{4,1},{5,.5}};
    nx_series_view views[]={{6,download,paint_plot_sample},{6,upload,paint_plot_sample}};
    nx_plot_layer layers[]={{{"Download",8},&views[0],1,{0,.6f,1,1},{0,.4f,1,.8f},{0,.2f,1,0},0,1},
        {{"Upload",6},&views[1],1,{.8f,.1f,1,1},{.7f,.1f,1,.8f},{.4f,0,.7f,0},0,1}};
    nx_series_envelope scratch[320];nx_plot_state state={0};nx_plot_multi_desc plot={0};
    plot.plot.id=1;plot.plot.bounds=(nx_rect){4,35,150,60};plot.plot.name=(nx_utf8_span){"Rates",5};
    plot.plot.x_min=0;plot.plot.x_max=5;plot.plot.y_min=0;plot.plot.y_max=8;
    plot.plot.scratch=scratch;plot.plot.scratch_capacity=320;
    plot.layers=layers;plot.layer_count=2;plot.x_unit=(nx_utf8_span){"s",1};plot.y_unit=(nx_utf8_span){"MB/s",4};
    plot.raster_scale=density;
    if(nx_plot_multi(ui,&plot,&state)!=NX_OK) return 1;
    for(int i=0;i<3;++i)
        if(nx_draw_icon(ui,(nx_icon)(NX_ICON_SHIELD+i),(nx_rect){12+i*42.f,102,24,24},1.4f,(nx_color){0,.9f,1,1})!=NX_OK) return 1;
    if(nx_draw_icon(ui,NX_ICON_TRANSFER,(nx_rect){138,102,24,24},1.4f,(nx_color){0,.9f,1,1})!=NX_OK)return 1;
    return nx_pop_clip(ui)!=NX_OK||nx_end_frame(ui)!=NX_OK;
}
static void paint_scene_free(nx_paint_image *images[3]) {
    for(unsigned i=0;i<3;++i) nx_paint_image_destroy(images[i]);
}
#endif
