#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "nexaui/nexaui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif
#define REQUIRE(x) do { if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);} } while(0)
static double now(void){
#ifdef _WIN32
    LARGE_INTEGER t,f;REQUIRE(QueryPerformanceFrequency(&f)&&QueryPerformanceCounter(&t));return (double)t.QuadPart/(double)f.QuadPart;
#else
    struct timespec t;REQUIRE(clock_gettime(CLOCK_MONOTONIC,&t)==0);return t.tv_sec+t.tv_nsec*1e-9;
#endif
}
typedef struct metric {size_t calls;double seconds;} metric;
static metric draw_cost[6];
static double draw_begin[6];
static unsigned draw_active[6];
static size_t copied_bytes;
static uint64_t cache_hits,cache_misses;
void nx_cost_begin(unsigned stage){REQUIRE(stage<6&&!draw_active[stage]);draw_active[stage]=1;draw_begin[stage]=now();}
void nx_cost_end(unsigned stage){double end=now();REQUIRE(stage<6&&draw_active[stage]);draw_active[stage]=0;draw_cost[stage].seconds+=end-draw_begin[stage];++draw_cost[stage].calls;}
void nx_cost_bytes(size_t bytes){copied_bytes+=bytes;}
static nx_result measured_raster(const nx_glyph_outline_buffer *outline,double scale,double x,double y,
    uint8_t *pixels,uint32_t width,uint32_t height,size_t stride,size_t size,const nx_allocator *allocator){
    nx_cost_begin(5);nx_result r=nx_rasterize_glyph_a8(outline,scale,x,y,pixels,width,height,stride,size,allocator);nx_cost_end(5);return r;
}
#define nx_rasterize_glyph_a8 measured_raster
#define nx_glyph_cache_destroy measured_cache_destroy
#include "../src/glyph_cache.c"
#undef nx_rasterize_glyph_a8
#undef nx_glyph_cache_destroy
void nx_glyph_cache_destroy(nx_glyph_cache *cache){
    if(cache){nx_glyph_cache_stats stats;REQUIRE(nx_glyph_cache_get_stats(cache,&stats)==NX_OK);cache_hits+=stats.hits;cache_misses+=stats.misses;}
    measured_cache_destroy(cache);
}
static void print_extra(int enabled,const char *variant,unsigned sample){
    for(unsigned i=0;i<6;++i){REQUIRE(!draw_active[i]);printf(",%.6f",enabled?draw_cost[i].seconds*1000:0);}
    printf(",%zu,%llu,%llu,%s,%u\n",enabled?copied_bytes:0,(unsigned long long)(enabled?cache_hits:0),(unsigned long long)(enabled?cache_misses:0),variant,sample);
}
static metric shape_metric,grapheme_metric,break_metric;
#include "font_internal.h"
#include "ot_layout.h"
static metric gsub_metric,gpos_metric;
static nx_result profile_gsub(const nx_font *font,uint32_t script,nx_shaped_glyph *glyphs,nx_ot_glyph_state *state,size_t *count){
    double t=now();nx_result r=nx_gsub_apply_state(font,script,glyphs,state,count);gsub_metric.seconds+=now()-t;++gsub_metric.calls;return r;
}
static nx_result profile_gpos(const nx_font *font,uint32_t script,nx_shaped_glyph *glyphs,nx_ot_glyph_state *state,size_t count,double ppem,uint8_t *pairs){
    double t=now();nx_result r=nx_gpos_apply_state(font,script,glyphs,state,count,ppem,pairs);gpos_metric.seconds+=now()-t;++gpos_metric.calls;return r;
}
#define nx_gsub_apply_state profile_gsub
#define nx_gpos_apply_state profile_gpos
#include "../src/shape.c"
#undef nx_gsub_apply_state
#undef nx_gpos_apply_state
nx_result profile_shape(const nx_font_collection *collection,const char *bytes,size_t length,
    nx_bidi_direction direction,double ppem,nx_shaped_glyph *glyphs,size_t gc,nx_shape_run *runs,
    size_t rc,size_t *ng,size_t *nr,uint8_t *level){
    double t=now();nx_result r=nx_shape_text(collection,bytes,length,direction,ppem,glyphs,gc,runs,rc,ng,nr,level);
    shape_metric.seconds+=now()-t;++shape_metric.calls;return r;
}
nx_result profile_grapheme(const char *bytes,size_t length,size_t start,size_t *next){
    double t=now();nx_result r=nx_grapheme_next(bytes,length,start,next);
    grapheme_metric.seconds+=now()-t;++grapheme_metric.calls;return r;
}
nx_result profile_breaks(const char *bytes,size_t length,uint8_t *out,size_t capacity){
    double t=now();nx_result r=nx_line_breaks(bytes,length,out,capacity);
    break_metric.seconds+=now()-t;++break_metric.calls;return r;
}
/* Exact production layout TU; only its three public callees are timed.
   Shape includes its own nested Unicode/font/OpenType work; never add nested
   shape phases to these top-level disjoint layout-callee totals. */
#define nx_shape_text profile_shape
#define nx_grapheme_next profile_grapheme
#define nx_line_breaks profile_breaks
#include "../src/text_layout.c"
#undef nx_shape_text
#undef nx_grapheme_next
#undef nx_line_breaks

#ifdef NEXAUI_TEXT_LAYOUT_BASELINE_SOURCE
nx_result nx_before_text_layout_create(const nx_font_collection*,const char*,size_t,nx_bidi_direction,double,double,nx_text_layout**);
void nx_before_text_layout_destroy(nx_text_layout*);
#endif

typedef struct heap {size_t live,peak,calls;} heap;
static void *allocate(void *user,size_t size){heap*h=user;void*p=malloc(size);if(p){h->live+=size;++h->calls;if(h->live>h->peak)h->peak=h->live;}return p;}
static void release(void *user,void *p,size_t size){heap*h=user;REQUIRE(h->live>=size);h->live-=size;free(p);}
static uint8_t *load(const char *path,size_t *length){FILE*f=fopen(path,"rb");REQUIRE(f&&fseek(f,0,SEEK_END)==0);long n=ftell(f);REQUIRE(n>0&&fseek(f,0,SEEK_SET)==0);uint8_t*p=malloc((size_t)n);REQUIRE(p&&fread(p,1,(size_t)n,f)==(size_t)n&&fclose(f)==0);*length=(size_t)n;return p;}
static void render(nx_context *ui,float density,uint8_t *pixels){
    const nx_command *commands;size_t count;REQUIRE(nx_frame_commands(ui,&commands,&count)==NX_OK);
    nx_command *scaled=malloc((count?count:1)*sizeof(*scaled));REQUIRE(scaled);
    for(size_t i=0;i<count;++i){scaled[i]=commands[i];scaled[i].rect.x*=density;scaled[i].rect.y*=density;scaled[i].rect.width*=density;scaled[i].rect.height*=density;}
    REQUIRE(nx_headless_render(scaled,count,pixels,(uint32_t)(640*density),(uint32_t)(240*density),(size_t)(640*density)*4,(nx_color){0})==NX_OK);free(scaled);
}
int main(int argc,char **argv){
    if(argc!=2){fputs("usage: nexaui_text_cost_bench FONT.ttf\n",stderr);return 2;}
    static const char *texts[]={"Bergsee.jpg","In NexaCloud suchen ...","Übertragungen · Größe 3,2 MB", "Datei hochgeladen: Projekte/Urlaub2026 – Freigabelink erstellen.","Deutsch: A\xcc\x88pfel; العربية; עברית; ffi AV", "Langer Text mit mehreren Zeilen und mehreren Wörtern. Die Breite begrenzt den Umbruch, die Auswahl bleibt logisch."};
    const float scales[]={1,1.25f,1.5f,2};size_t font_bytes;uint8_t*data=load(argv[1],&font_bytes);nx_font*font=NULL;nx_font_collection*fonts=NULL;
    REQUIRE(nx_font_open(data,font_bytes,NULL,&font)==NX_OK&&nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    heap memory={0};nx_context_desc desc={0};desc.allocator=(nx_allocator){&memory,allocate,release};nx_context*ui=NULL;REQUIRE(nx_context_create(&desc,&ui)==NX_OK);
    puts("kind,text,density,clipped,scaled,iterations,total_ms,shape_ms,grapheme_ms,break_ms,glyphs,commands,allocation_calls,live,peak,gsub_ms,gpos_ms,shape_calls,gsub_calls,gpos_calls,cache_create_ms,cache_get_ms,mask_alloc_copy_ms,placement_command_ms,cache_destroy_ms,raster_nested_ms,copied_bytes,cache_hits,cache_misses,variant,sample");
    for(unsigned c=0;c<6;++c){
      nx_text_layout *saved[2]={0};
#ifdef NEXAUI_TEXT_LAYOUT_BASELINE_SOURCE
      const unsigned order[]={1,0,0,1,1,0,0,1};
#else
      const unsigned order[]={1};
#endif
      for(unsigned sample=0;sample<sizeof(order)/sizeof(order[0]);++sample){
        unsigned current=order[sample];const char *variant=current?"after":"before";
        nx_result (*create)(const nx_font_collection*,const char*,size_t,nx_bidi_direction,double,double,nx_text_layout**)=nx_text_layout_create;
        void (*destroy)(nx_text_layout*)=nx_text_layout_destroy;
#ifdef NEXAUI_TEXT_LAYOUT_BASELINE_SOURCE
        if(!current){create=nx_before_text_layout_create;destroy=nx_before_text_layout_destroy;}
#endif
        shape_metric=(metric){0};grapheme_metric=(metric){0};break_metric=(metric){0};gsub_metric=(metric){0};gpos_metric=(metric){0};
        double t,elapsed=0;size_t glyphs=0;nx_text_layout*layout=NULL;
        for(unsigned k=0;k<12;++k){nx_text_layout*item=NULL;t=now();nx_result r=create(fonts,texts[c],strlen(texts[c]),NX_BIDI_AUTO,14.5,300,&item);elapsed+=now()-t;REQUIRE(r==NX_OK);glyphs=nx_text_layout_glyph_count(item);if(k==11)layout=item;else destroy(item);}
        printf("layout,%u,1,0,0,12,%.6f,%.6f,%.6f,%.6f,%zu,0,0,0,0,%.6f,%.6f,%zu,%zu,%zu",c,elapsed*1000,shape_metric.seconds*1000,grapheme_metric.seconds*1000,break_metric.seconds*1000,glyphs,gsub_metric.seconds*1000,gpos_metric.seconds*1000,shape_metric.calls,gsub_metric.calls,gpos_metric.calls);print_extra(0,variant,sample);
        if(saved[current]){destroy(saved[current]);}
        saved[current]=layout;
      }
      nx_text_layout *layout=saved[1];size_t glyphs=nx_text_layout_glyph_count(layout);double t;
        for(unsigned d=0;d<4;++d)for(unsigned clip=0;clip<2;++clip){
            size_t bytes=(size_t)(640*scales[d])*(size_t)(240*scales[d])*4;
            uint8_t*reference=malloc(bytes),*actual=malloc(bytes);REQUIRE(reference&&actual);
            for(unsigned method=0;method<2;++method){
                memset(draw_cost,0,sizeof(draw_cost));copied_bytes=0;cache_hits=cache_misses=0;
                REQUIRE(nx_context_set_text_raster_scale(ui,scales[d])==NX_OK);
                size_t before=memory.calls;double total=0;
                for(unsigned k=0;k<8;++k){
                    REQUIRE(nx_begin_frame(ui,640,240,0)==NX_OK);
                    if(clip)REQUIRE(nx_push_clip(ui,(nx_rect){600,220,20,10})==NX_OK);
                    t=now();nx_result r=method?nx_draw_text_layout_scaled(ui,layout,(nx_point){10.25f,10.5f},(nx_color){1,1,1,1},scales[d],65536):nx_draw_text_layout(ui,layout,(nx_point){10.25f,10.5f},(nx_color){1,1,1,1},65536);
                    total+=now()-t;REQUIRE(r==NX_OK);if(clip)REQUIRE(nx_pop_clip(ui)==NX_OK);REQUIRE(nx_end_frame(ui)==NX_OK);
                }
                const nx_command*commands;size_t count;REQUIRE(nx_frame_commands(ui,&commands,&count)==NX_OK);
                render(ui,scales[d],method?actual:reference);
                printf("draw,%u,%.2f,%u,%u,8,%.6f,0,0,0,%zu,%zu,%zu,%zu,%zu,0,0,0,0,0",c,scales[d],clip,method,total*1000,glyphs,count,memory.calls-before,memory.live,memory.peak);print_extra(1,"after",0);
            }
            REQUIRE(memcmp(reference,actual,bytes)==0);
            int visible=0;for(size_t i=0;i<bytes;++i)visible|=reference[i]!=0;REQUIRE(visible==!clip);
            if(saved[0]){REQUIRE(nx_begin_frame(ui,640,240,0)==NX_OK);if(clip)REQUIRE(nx_push_clip(ui,(nx_rect){600,220,20,10})==NX_OK);
                REQUIRE(nx_draw_text_layout_scaled(ui,saved[0],(nx_point){10.25f,10.5f},(nx_color){1,1,1,1},scales[d],65536)==NX_OK);
                if(clip){REQUIRE(nx_pop_clip(ui)==NX_OK);}
                REQUIRE(nx_end_frame(ui)==NX_OK);render(ui,scales[d],actual);REQUIRE(memcmp(reference,actual,bytes)==0);}
            free(actual);free(reference);
        }
        nx_text_layout_destroy(layout);fflush(stdout);
#ifdef NEXAUI_TEXT_LAYOUT_BASELINE_SOURCE
        nx_before_text_layout_destroy(saved[0]);
#endif
    }
    nx_context_destroy(ui);REQUIRE(memory.live==0);nx_font_collection_destroy(fonts);nx_font_close(font);free(data);return 0;
}
