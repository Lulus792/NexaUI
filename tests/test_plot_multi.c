#include "nexaui/nexaui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
static nx_result sample(void *u,size_t i,nx_series_point *p){*p=((nx_series_point *)u)[i];return NX_OK;}
typedef struct heap {size_t calls,fail,bytes;} heap;
static void *alloc(void *u,size_t n){heap*h=u;if(++h->calls==h->fail)return NULL;void*p=malloc(n);if(p)h->bytes+=n;return p;}
static void release(void*u,void*p,size_t n){heap*h=u;h->bytes-=n;free(p);}
static int run(const char *path) {
    nx_series_point a[]={{0,3},{2,3}},b[]={{8,-2},{10,-2}},c[]={{0,1},{10,1}};
    nx_series_view segments[]={{2,a,sample},{2,b,sample}},upload={2,c,sample};
    nx_plot_layer layers[]={{{"Download",8},segments,2,{0,.7f,1,1},{0,.6f,1,.8f},{0,0,1,.1f},0,1},
        {{"Upload",6},&upload,1,{1,0,1,1},{1,0,1,.7f},{.3f,0,1,.1f},0,1},
        {{"Reference",9},&upload,1,{1,1,0,1},{0},{0},0,0}};
    nx_series_envelope scratch[1024];nx_plot_multi_desc d={0};
    d.plot.id=1;d.plot.bounds=(nx_rect){0,0,160,100};d.plot.name=(nx_utf8_span){"Rates",5};
    d.plot.x_min=0;d.plot.x_max=10;d.plot.y_min=-4;d.plot.y_max=4;
    d.plot.scratch=scratch;d.plot.scratch_capacity=1024;
    d.layers=layers;d.layer_count=3;d.x_unit=(nx_utf8_span){"s",1};d.y_unit=(nx_utf8_span){"MB/s",4};d.raster_scale=1;
    heap h={0};nx_context_desc context={0};context.allocator=(nx_allocator){&h,alloc,release};
    nx_context *ui=NULL;nx_plot_state state={0};CHECK(nx_context_create(&context,&ui)==NX_OK);
    CHECK(nx_begin_frame(ui,160,100,0)==NX_OK);
    CHECK(nx_plot_multi(ui,&d,&state)==NX_OK);CHECK(nx_end_frame(ui)==NX_OK);
    const nx_command *commands;size_t count;CHECK(nx_frame_commands(ui,&commands,&count)==NX_OK);
    size_t masks=0;const nx_command *first=NULL;
    for(size_t i=0;i<count;++i)if(commands[i].type==NX_COMMAND_BITMAP){++masks;if(!first)first=&commands[i];}
    CHECK(masks==2&&first);
    nx_bitmap_view bitmap=first->bitmap;
    unsigned filled=0;for(uint32_t y=0;y<bitmap.height;++y){
        CHECK(bitmap.pixels[((size_t)y*bitmap.width+bitmap.width/2)*4+3]==0);
        if(bitmap.pixels[((size_t)y*bitmap.width+2)*4+3])++filled;
    }CHECK(filled>8); /* Explicit data gap stays transparent; negative segment fills below zero. */
    uint8_t rgba[160*100*4];CHECK(nx_headless_render(commands,count,rgba,160,100,160*4,(nx_color){0,0,0,1})==NX_OK);
    if(path){FILE*f=fopen(path,"wb");CHECK(f);fprintf(f,"P6\n160 100\n255\n");
        for(size_t i=0;i<160*100;++i)CHECK(fwrite(rgba+4*i,1,3,f)==3);CHECK(!fclose(f));}
    uint32_t index;nx_semantic_view v;
    CHECK(nx_semantics_find(ui,1,&index)==NX_OK&&nx_semantics_get(ui,index,&v)==NX_OK);
    CHECK(v.description.length>30&&strstr(v.description.data,"Download")&&strstr(v.description.data,"MB/s"));
    CHECK(nx_focus_set(ui,1)==NX_OK);nx_event e={0};e.type=NX_EVENT_KEY_DOWN;e.key=NX_KEY_RIGHT;
    CHECK(nx_context_queue_event(ui,&e)==NX_OK&&nx_begin_frame(ui,160,100,0)==NX_OK);
    CHECK(nx_plot_multi(ui,&d,&state)==NX_OK&&state.cursor_valid&&state.cursor_x>0);CHECK(nx_end_frame(ui)==NX_OK);
    e=(nx_event){0};e.type=NX_EVENT_POINTER_MOVE;e.x=80;e.y=45;
    CHECK(nx_context_queue_event(ui,&e)==NX_OK&&nx_begin_frame(ui,160,100,0)==NX_OK);
    CHECK(nx_plot_multi(ui,&d,&state)==NX_OK&&state.cursor_x>4&&state.cursor_x<7);CHECK(nx_end_frame(ui)==NX_OK);
    for(size_t failure=1;failure<=2;++failure){
        CHECK(nx_begin_frame(ui,160,100,0)==NX_OK);size_t before=h.bytes;
        nx_plot_state saved=state;h.fail=h.calls+failure;
        CHECK(nx_plot_multi(ui,&d,&state)==NX_ERROR_MEMORY&&memcmp(&state,&saved,sizeof(state))==0);
        CHECK(h.bytes==before&&nx_semantics_find(ui,1,&index)!=NX_OK);h.fail=0;
        CHECK(nx_plot_multi(ui,&d,&state)==NX_OK);CHECK(nx_end_frame(ui)==NX_OK);
    }
    CHECK(nx_begin_frame(ui,160,100,0)==NX_OK);
    d.plot.scratch_capacity=1;CHECK(nx_plot_multi(ui,&d,&state)==NX_ERROR_LIMIT);
    d.plot.scratch_capacity=1024;d.raster_scale=8;d.plot.bounds.width=2000;
    CHECK(nx_plot_multi(ui,&d,&state)==NX_ERROR_LIMIT);
    d.plot.bounds.width=160;d.raster_scale=1;layers[0].baseline=NAN;
    CHECK(nx_plot_multi(ui,&d,&state)==NX_ERROR_ARGUMENT);layers[0].baseline=0;
    a[1].y=NAN;CHECK(nx_plot_multi(ui,&d,&state)==NX_ERROR_ARGUMENT);a[1].y=3;
    upload.count=1000001;CHECK(nx_plot_multi(ui,&d,&state)==NX_ERROR_LIMIT);upload.count=2;
    d.hide_fills=1;CHECK(nx_plot_multi(ui,&d,&state)==NX_OK);CHECK(nx_end_frame(ui)==NX_OK);
    CHECK(nx_frame_commands(ui,&commands,&count)==NX_OK);
    for(size_t i=0;i<count;++i)CHECK(commands[i].type!=NX_COMMAND_BITMAP);
    nx_context_destroy(ui);CHECK(!h.bytes);return 0;
}
int main(int argc,char **argv){return run(argc>1?argv[1]:NULL);}
