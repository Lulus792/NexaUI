#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "nexaui/nexaui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);}}while(0)
typedef struct heap {size_t calls,live,peak;} heap;
static void *allocate(void *user,size_t size){heap *h=user;void*p=malloc(size);if(p){++h->calls;h->live+=size;if(h->live>h->peak)h->peak=h->live;}return p;}
static void release(void *user,void *p,size_t size){heap*h=user;REQUIRE(h->live>=size);h->live-=size;free(p);}
static double now(void){
#ifdef _WIN32
    LARGE_INTEGER value,frequency;REQUIRE(QueryPerformanceFrequency(&frequency)&&QueryPerformanceCounter(&value));
    return (double)value.QuadPart/frequency.QuadPart;
#else
    struct timespec value;REQUIRE(clock_gettime(CLOCK_MONOTONIC,&value)==0);return value.tv_sec+value.tv_nsec*1e-9;
#endif
}
static uint64_t bytes_hash(const uint8_t *p,size_t n,uint64_t hash){for(size_t i=0;i<n;++i){hash^=p[i];hash*=UINT64_C(1099511628211);}return hash;}
static uint64_t bitmap_hash(nx_bitmap_view b){uint64_t hash=UINT64_C(1469598103934665603);
    for(uint32_t y=0;y<b.height;++y)hash=bytes_hash(b.pixels+(size_t)y*b.stride_bytes,(size_t)b.width*(b.format==NX_BITMAP_RGBA8?4:1),hash);return hash;}
typedef struct paint_api {
    const char *name;
    nx_result (*surface)(const nx_surface_image_desc*,const nx_allocator*,nx_paint_image**);
    nx_result (*shadow)(const nx_shadow_image_desc*,const nx_allocator*,nx_paint_image**);
    nx_result (*view)(const nx_paint_image*,nx_bitmap_view*,nx_rect*);
    void (*destroy)(nx_paint_image*);
} paint_api;
#ifdef NEXAUI_PAINT_BASELINE_PATH
nx_result nx_before_surface_create(const nx_surface_image_desc*,const nx_allocator*,nx_paint_image**);
nx_result nx_before_shadow_create(const nx_shadow_image_desc*,const nx_allocator*,nx_paint_image**);
nx_result nx_before_paint_view(const nx_paint_image*,nx_bitmap_view*,nx_rect*);
void nx_before_paint_destroy(nx_paint_image*);
#endif
static void snapshot(const char *prefix,const char *variant,const char *name,nx_bitmap_view b){
    if(!prefix)return;char path[1024];int n=snprintf(path,sizeof(path),"%s-%s-%s.rgba",prefix,variant,name);
    REQUIRE(n>0&&(size_t)n<sizeof(path));FILE*f=fopen(path,"wb");REQUIRE(f);
    for(uint32_t y=0;y<b.height;++y)REQUIRE(fwrite(b.pixels+(size_t)y*b.stride_bytes,1,(size_t)b.width*4,f)==(size_t)b.width*4);
    REQUIRE(fclose(f)==0);
}
static void paint_case(FILE *csv,const paint_api *api,unsigned which,int iteration,const char *prefix){
    static const char *names[]={"linear-320x160-1","linear-320x160-2","radial-128x128-2","shadow-640x360-2","glow-48x48-2"};
    nx_color_stop stops[]={{0,{.01f,.16f,.26f,.9f}},{.43f,{.01f,.07f,.13f,.8f}},{1,{0,.02f,.05f,.2f}}};
    nx_surface_image_desc d={320,160,12,which==0?1.f:2.f,{NX_PAINT_LINEAR,{0},{0,0},{0,160},stops,3},64u*1024u*1024u};
    nx_shadow_image_desc shadow={640,360,12,2,4,{0,4},{0,.5f,1,.6f},64u*1024u*1024u};
    if(which==2){d.width=d.height=128;d.radius=64;d.paint.kind=NX_PAINT_RADIAL;d.paint.from=(nx_point){64,64};d.paint.to=(nx_point){128,64};}
    if(which==4){shadow.width=shadow.height=48;shadow.radius=24;shadow.softness=6;}
    heap h={0};nx_allocator allocator={&h,allocate,release};nx_paint_image *image=NULL;
    double begin=now();nx_result result=which<3?api->surface(&d,&allocator,&image):api->shadow(&shadow,&allocator,&image);
    double ms=(now()-begin)*1000;REQUIRE(result==NX_OK);
    nx_bitmap_view bitmap;nx_rect bounds;REQUIRE(api->view(image,&bitmap,&bounds)==NX_OK);
    uint64_t hash=bitmap_hash(bitmap);
    if(iteration==0)snapshot(prefix,api->name,names[which],bitmap);
    if(iteration>=0)fprintf(csv,"create,%s,%s,%d,%.9f,%zu,%zu,%zu,1,%llu\n",api->name,names[which],iteration,ms,h.calls,h.peak,h.live,(unsigned long long)hash);
    api->destroy(image);REQUIRE(h.live==0);
}
static void compare_images(const paint_api *a,nx_paint_image *left,const paint_api *b,nx_paint_image *right){
    nx_bitmap_view x,y;nx_rect xb,yb;REQUIRE(a->view(left,&x,&xb)==NX_OK&&b->view(right,&y,&yb)==NX_OK);
    REQUIRE(x.width==y.width&&x.height==y.height&&x.format==y.format&&memcmp(&xb,&yb,sizeof(xb))==0);
    for(uint32_t row=0;row<x.height;++row)REQUIRE(memcmp(x.pixels+(size_t)row*x.stride_bytes,y.pixels+(size_t)row*y.stride_bytes,(size_t)x.width*4)==0);
}
static void compare_matrix(const paint_api *before,const paint_api *current){
    const float scales[]={.125f,.5f,1,1.25f,1.5f,2,4,8};
    nx_color_stop stops[]={{0,{.7f,.1f,.2f,1}},{.43f,{.1f,.7f,.2f,.35f}},{1,{.1f,.2f,.9f,0}}};
    heap h={0};nx_allocator allocator={&h,allocate,release};
    unsigned cases=0;
    for(unsigned i=0;i<128;++i){
        nx_surface_image_desc d={1+(i*17%73)+.375f,1+(i*29%61)+.625f,i%5==0?10000.f:(float)(i%19),scales[i%8],{0},64u*1024u*1024u};
        d.paint=(nx_paint){(nx_paint_kind)(i%3),{.2f,.4f,.6f,.8f},{-3.75f,.5f},{d.width+1.25f,d.height-.75f},stops,3};
        if(i%4==0)d.paint.to.x=d.paint.from.x;
        if(i%11==0)d.paint.to=d.paint.from;
        nx_paint_image *left=NULL,*right=NULL;
        REQUIRE(before->surface(&d,&allocator,&left)==NX_OK&&current->surface(&d,&allocator,&right)==NX_OK);
        compare_images(before,left,current,right);before->destroy(left);current->destroy(right);++cases;
        if(i%3==0){nx_shadow_image_desc shadow={d.width,d.height,d.radius,d.scale,.25f+.5f*(i%10),{-2.25f,3.5f},{.2f,.7f,1,.65f},64u*1024u*1024u};
            REQUIRE(before->shadow(&shadow,&allocator,&left)==NX_OK&&current->shadow(&shadow,&allocator,&right)==NX_OK);
            compare_images(before,left,current,right);before->destroy(left);current->destroy(right);++cases;}
        REQUIRE(!h.live);
    }
    printf("Paired geometry/color matrix: %u complete RGBA buffers and bounds identical\n",cases);
}
static nx_result sample(void *user,size_t index,nx_series_point *point){*point=((const nx_series_point*)user)[index];return NX_OK;}
typedef struct plot_api {
    const char *name;
    nx_result (*create)(const nx_context_desc*,nx_context**);
    nx_result (*begin)(nx_context*,float,float,double);
    nx_result (*plot)(nx_context*,const nx_plot_multi_desc*,nx_plot_state*);
    nx_result (*end)(nx_context*);
    nx_result (*commands)(const nx_context*,const nx_command**,size_t*);
    void (*destroy)(nx_context*);
} plot_api;
#ifdef NEXAUI_CORE_BASELINE_PATH
nx_result nx_before_core_context_create(const nx_context_desc*,nx_context**);
nx_result nx_before_core_begin_frame(nx_context*,float,float,double);
nx_result nx_before_core_plot_multi(nx_context*,const nx_plot_multi_desc*,nx_plot_state*);
nx_result nx_before_core_end_frame(nx_context*);
nx_result nx_before_core_frame_commands(const nx_context*,const nx_command**,size_t*);
void nx_before_core_context_destroy(nx_context*);
#endif
static void compare_frames(const nx_command *left,size_t a,const nx_command *right,size_t b){
    REQUIRE(a==b);
    for(size_t i=0;i<a;++i){const nx_command*x=&left[i],*y=&right[i];
        REQUIRE(x->type==y->type&&memcmp(&x->rect,&y->rect,sizeof(x->rect))==0&&
            memcmp(&x->color,&y->color,sizeof(x->color))==0&&memcmp(x->points,y->points,sizeof(x->points))==0&&
            x->radius==y->radius&&x->thickness==y->thickness);
        if(x->type==NX_COMMAND_BITMAP){REQUIRE(x->bitmap.width==y->bitmap.width&&x->bitmap.height==y->bitmap.height&&x->bitmap.format==y->bitmap.format);
            for(uint32_t row=0;row<x->bitmap.height;++row)REQUIRE(memcmp(x->bitmap.pixels+(size_t)row*x->bitmap.stride_bytes,
                y->bitmap.pixels+(size_t)row*y->bitmap.stride_bytes,(size_t)x->bitmap.width*4)==0);}
    }
}
static void plot_case(FILE *csv,float scale,unsigned scenario,const char *prefix){
    static const char *kinds[]={"rates","gaps-negative","empty","zero","sparse"};
    nx_series_point values[2][601];nx_series_view views[2];
    for(unsigned k=0;k<2;++k){for(unsigned i=0;i<601;++i)values[k][i]=(nx_series_point){i,20+10*sin(i*.08+k)+5*sin(i*.17)};views[k]=(nx_series_view){601,values[k],sample};}
    for(unsigned k=0;k<2;++k){
        if(scenario==1)for(unsigned i=0;i<601;++i)values[k][i].y-=25;
        if(scenario==2)views[k].count=0;
        if(scenario==3)for(unsigned i=0;i<601;++i)values[k][i].y=0;
        if(scenario==4){for(unsigned i=0;i<601;++i)values[k][i].y=1+.05*(i%3);views[k].count=11;views[k].user=&values[k][590];}
    }
    nx_plot_layer layers[]={{{"Download",8},&views[0],1,{0,.6f,1,1},{0,.4f,1,.8f},{0,.2f,1,0},0,1},
        {{"Upload",6},&views[1],1,{.8f,.1f,1,1},{.7f,.1f,1,.8f},{.4f,0,.7f,0},0,1}};
    nx_series_envelope scratch[2560];nx_plot_multi_desc d={0};d.plot.id=1;d.plot.bounds=(nx_rect){0,0,640,280};
    d.plot.name=(nx_utf8_span){"Rates",5};d.plot.x_max=600;d.plot.y_max=50;d.plot.scratch=scratch;d.plot.scratch_capacity=2560;
    d.layers=layers;d.layer_count=2;d.raster_scale=scale;d.x_unit=(nx_utf8_span){"s",1};d.y_unit=(nx_utf8_span){"MB/s",4};
    nx_series_view gaps[2][2];
    if(scenario==1){d.plot.y_min=-50;for(unsigned k=0;k<2;++k){gaps[k][0]=(nx_series_view){200,values[k],sample};gaps[k][1]=(nx_series_view){201,&values[k][400],sample};layers[k].segments=gaps[k];layers[k].segment_count=2;}}
    const plot_api apis[]={
#ifdef NEXAUI_CORE_BASELINE_PATH
        {"before",nx_before_core_context_create,nx_before_core_begin_frame,nx_before_core_plot_multi,nx_before_core_end_frame,nx_before_core_frame_commands,nx_before_core_context_destroy},
#endif
        {"current",nx_context_create,nx_begin_frame,nx_plot_multi,nx_end_frame,nx_frame_commands,nx_context_destroy}};
    size_t variants=sizeof(apis)/sizeof(*apis);heap heaps[2]={{0}};nx_context *contexts[2]={0};nx_plot_state states[2]={{0}};
    const nx_command *frames[2]={0};size_t counts[2]={0};
    for(size_t k=0;k<variants;++k){nx_context_desc context={0};context.allocator=(nx_allocator){&heaps[k],allocate,release};REQUIRE(apis[k].create(&context,&contexts[k])==NX_OK);}
    for(int iteration=-4;iteration<32;++iteration){
        for(size_t step=0;step<variants;++step){size_t k=(iteration&1)?variants-1-step:step;heap*h=&heaps[k];
            size_t calls=h->calls;h->peak=h->live;double begin=now();
            REQUIRE(apis[k].begin(contexts[k],640,280,0)==NX_OK&&apis[k].plot(contexts[k],&d,&states[k])==NX_OK&&apis[k].end(contexts[k])==NX_OK);
            double ms=(now()-begin)*1000;REQUIRE(apis[k].commands(contexts[k],&frames[k],&counts[k])==NX_OK);
            uint64_t hash=UINT64_C(1469598103934665603);unsigned index=0;
            for(size_t i=0;i<counts[k];++i)if(frames[k][i].type==NX_COMMAND_BITMAP){uint64_t part=bitmap_hash(frames[k][i].bitmap);hash=bytes_hash((const uint8_t*)&part,sizeof(part),hash);
                if(iteration==0){char name[80];snprintf(name,sizeof(name),"plot-%s-%.0f-layer-%u",kinds[scenario],scale,index++);snapshot(prefix,apis[k].name,name,frames[k][i].bitmap);}}
            if(iteration>=0)fprintf(csv,"frame,%s,plot-%s-640x280-%.0f,%d,%.9f,%zu,%zu,%zu,%zu,%llu\n",apis[k].name,kinds[scenario],scale,iteration,ms,h->calls-calls,h->peak,h->live,counts[k],(unsigned long long)hash);
        }
        if(variants==2)compare_frames(frames[0],counts[0],frames[1],counts[1]);
    }
    for(size_t k=0;k<variants;++k){apis[k].destroy(contexts[k]);REQUIRE(!heaps[k].live);}
}
static void cached_case(FILE *csv){
    nx_surface_image_desc d={320,160,12,2,{NX_PAINT_SOLID,{.01f,.05f,.1f,1},{0},{0},NULL,0},4u*1024u*1024u};nx_paint_image *image=NULL;
    REQUIRE(nx_surface_image_create(&d,NULL,&image)==NX_OK);heap h={0};nx_context_desc context={0};context.allocator=(nx_allocator){&h,allocate,release};nx_context*ui=NULL;REQUIRE(nx_context_create(&context,&ui)==NX_OK);
    for(int sample_index=-2;sample_index<16;++sample_index){size_t calls=h.calls;h.peak=h.live;double begin=now();
        for(unsigned frame=0;frame<128;++frame){REQUIRE(nx_begin_frame(ui,640,360,0)==NX_OK);for(unsigned k=0;k<32;++k)REQUIRE(nx_draw_paint_image(ui,image,(nx_point){(float)k,0},(nx_color){1,1,1,1})==NX_OK);REQUIRE(nx_end_frame(ui)==NX_OK);}
        double ms=(now()-begin)*1000/128.;if(sample_index>=0)fprintf(csv,"frame,current,cached-32-paints,%d,%.9f,%zu,%zu,%zu,32,0\n",sample_index,ms,h.calls-calls,h.peak,h.live);
    }
    nx_context_destroy(ui);nx_paint_image_destroy(image);REQUIRE(!h.live);
}
int main(int argc,char **argv){
    REQUIRE(argc==2||argc==3);FILE*csv=fopen(argv[1],"wb");REQUIRE(csv);const char *prefix=argc>2?argv[2]:NULL;
    fputs("phase,variant,case,iteration,ms,allocations,peak_bytes,retained_bytes,commands,checksum\n",csv);
    const paint_api apis[]={
#ifdef NEXAUI_PAINT_BASELINE_PATH
        {"before",nx_before_surface_create,nx_before_shadow_create,nx_before_paint_view,nx_before_paint_destroy},
#endif
        {"current",nx_surface_image_create,nx_shadow_image_create,nx_paint_image_view,nx_paint_image_destroy}};
    size_t count=sizeof(apis)/sizeof(*apis);
    if(count==2)compare_matrix(&apis[0],&apis[1]);
    for(unsigned which=0;which<5;++which)for(int iteration=-2;iteration<16;++iteration)for(size_t step=0;step<count;++step){size_t index=(iteration&1)?count-1-step:step;paint_case(csv,&apis[index],which,iteration,prefix);}
    cached_case(csv);plot_case(csv,1,0,prefix);for(unsigned scenario=0;scenario<5;++scenario)plot_case(csv,2,scenario,prefix);REQUIRE(fclose(csv)==0);return 0;
}
