#include <nexaui/nexaui.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "search_font_fixture.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)

typedef struct heap {size_t live,peak,calls,fail;int bad;} heap;
static void *allocate(void *u,size_t n){heap*h=u;if(++h->calls==h->fail)return NULL;
    void*p=malloc(n);if(p){h->live+=n;if(h->live>h->peak)h->peak=h->live;}return p;}
static void release(void*u,void*p,size_t n){heap*h=u;if(n>h->live)h->bad=1;else h->live-=n;free(p);}

static int render_scaled(nx_context *ui,float density,uint8_t *pixels,unsigned w,unsigned h){
    const nx_command *commands;size_t count;
    CHECK(nx_frame_commands(ui,&commands,&count)==NX_OK&&count<64);
    nx_command scaled[64];
    for(size_t i=0;i<count;++i){scaled[i]=commands[i];
        scaled[i].rect.x*=density;scaled[i].rect.y*=density;
        scaled[i].rect.width*=density;scaled[i].rect.height*=density;}
    CHECK(nx_headless_render(scaled,count,pixels,w,h,(size_t)w*4,(nx_color){0})==NX_OK);
    return 0;
}

/* Independent physical-position reference, using the original shaped glyph
   positions and cache masks. No stamp offsets/bounds enter this reference. */
static int reference(const nx_text_layout *layout,float density,uint8_t *pixels,unsigned w,unsigned h){
    nx_glyph_cache *cache=NULL;nx_text_glyph glyph;
    CHECK(nx_text_layout_glyph_at(layout,0,&glyph)==NX_OK);
    CHECK(nx_glyph_cache_create(glyph.shape.font,65536,NULL,&cache)==NX_OK);
    nx_command commands[16]={0};size_t count=nx_text_layout_glyph_count(layout);CHECK(count<=16);
    for(size_t i=0;i<count;++i){nx_glyph_mask_view mask;
        CHECK(nx_text_layout_glyph_at(layout,i,&glyph)==NX_OK);
        CHECK(nx_glyph_cache_get_fractional(cache,glyph.shape.glyph_index,
            nx_text_layout_pixels_per_em(layout)*density,&mask)==NX_OK);
        commands[i].type=NX_COMMAND_BITMAP;commands[i].color=(nx_color){1,1,1,1};
        commands[i].rect=(nx_rect){(float)((8+glyph.x+glyph.shape.offset_x)*density+mask.x_offset),
            (float)((4+glyph.baseline_y-glyph.shape.offset_y)*density+mask.y_offset),
            (float)mask.width,(float)mask.height};
        commands[i].bitmap=(nx_bitmap_view){mask.pixels,mask.width,mask.height,mask.stride_bytes,NX_BITMAP_A8};
    }
    CHECK(nx_headless_render(commands,count,pixels,w,h,(size_t)w*4,(nx_color){0})==NX_OK);
    nx_glyph_cache_destroy(cache);return 0;
}

static nx_result sample(void *u,size_t index,nx_series_point *point){
    *point=((nx_series_point*)u)[index];return NX_OK;
}
static int fractional_widgets(nx_context *ui,const nx_font_collection *fonts){
    nx_text_field *text=NULL;nx_numeric_field *number=NULL;nx_interaction_state state;int valid,submitted;
    CHECK(nx_text_field_create("010",3,4096,&text)==NX_OK&&nx_numeric_field_create(1,4096,&number)==NX_OK);
    nx_text_field_desc td={0};td.id=1;td.bounds=(nx_rect){0,0,150,35};
    td.name=(nx_utf8_span){"Text",4};td.fonts=fonts;td.pixels_per_em=19.25;td.glyph_cache_bytes=65536;
    nx_numeric_field_desc nd={0};nd.id=2;nd.bounds=(nx_rect){0,40,150,35};nd.name=(nx_utf8_span){"Number",6};
    nd.fonts=fonts;nd.pixels_per_em=19.25;nd.glyph_cache_bytes=65536;nd.maximum=10;
    nx_plot_desc pd={0};nx_series_envelope scratch[200];nx_plot_state ps={0};
    pd.id=3;pd.bounds=(nx_rect){160,0,180,100};pd.name=(nx_utf8_span){"Plot",4};
    pd.fonts=fonts;pd.pixels_per_em=12.5;pd.glyph_cache_bytes=65536;
    pd.x_max=pd.y_max=1;pd.scratch=scratch;pd.scratch_capacity=200;
    nx_series_point values[]={{0,0},{1,1}};pd.series=(nx_series_view){2,values,sample};
    CHECK(nx_begin_frame(ui,400,240,0)==NX_OK);
    CHECK(nx_text_field_widget(ui,&td,text,&state,&valid,&submitted)==NX_OK&&valid);
    CHECK(nx_numeric_field_widget(ui,&nd,number,&state,&valid)==NX_OK&&valid);
    CHECK(nx_plot(ui,&pd,&ps)==NX_OK&&nx_end_frame(ui)==NX_OK);
    nx_text_field_destroy(text);nx_numeric_field_destroy(number);return 0;
}

static int density_pixels(void){
    const float scales[]={.125f,1,1.25f,1.5f,2,8};
    nx_text_stamp *stamps[6]={0};uint8_t *expected[6]={0};unsigned widths[6],heights[6];
    uint8_t bytes[512];font_fixture(bytes);nx_font *font=NULL;nx_font_collection *fonts=NULL;
    nx_text_layout *layout=NULL,*blank=NULL;nx_context *ui=NULL;
    CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    CHECK(nx_text_layout_create(fonts,"010\n1",5,NX_BIDI_LTR,19.25,80,&layout)==NX_OK);
    CHECK(nx_text_layout_create(fonts,"",0,NX_BIDI_LTR,19.25,80,&blank)==NX_OK);
    CHECK(nx_context_create(NULL,&ui)==NX_OK);
    CHECK(!fractional_widgets(ui,fonts));
    double width=nx_text_layout_width(layout),height=nx_text_layout_height(layout);
    size_t lines=nx_text_layout_line_count(layout);nx_text_caret caret,before;
    CHECK(nx_text_layout_caret_at(layout,1,&before)==NX_OK);
    for(unsigned k=0;k<6;++k){float density=scales[k];nx_bitmap_view view;nx_point offset;nx_rect bounds;
        widths[k]=(unsigned)(80*density);heights[k]=(unsigned)(64*density);
        size_t size=(size_t)widths[k]*heights[k]*4;expected[k]=malloc(size);uint8_t *actual=malloc(size);
        CHECK(expected[k]&&actual);
        CHECK(!reference(layout,density,expected[k],widths[k],heights[k]));
        CHECK(nx_begin_frame(ui,80,64,0)==NX_OK);
        CHECK(nx_draw_text_layout_scaled(ui,layout,(nx_point){8,4},(nx_color){1,1,1,1},density,65536)==NX_OK);
        CHECK(nx_end_frame(ui)==NX_OK&&!render_scaled(ui,density,actual,widths[k],heights[k]));
        CHECK(memcmp(actual,expected[k],size)==0);free(actual);
        CHECK(nx_context_set_text_raster_scale(ui,density)==NX_OK);
        actual=malloc(size);CHECK(actual&&nx_begin_frame(ui,80,64,0)==NX_OK);
        CHECK(nx_draw_text_layout(ui,layout,(nx_point){8,4},(nx_color){1,1,1,1},65536)==NX_OK);
        CHECK(nx_end_frame(ui)==NX_OK&&!render_scaled(ui,density,actual,widths[k],heights[k]));
        CHECK(memcmp(actual,expected[k],size)==0);free(actual);
        CHECK(nx_text_stamp_create_scaled(layout,density,65536,NULL,&stamps[k])==NX_OK);
        CHECK(nx_text_stamp_view(stamps[k],&view,&offset)==NX_OK&&view.width&&view.height);
        CHECK(nx_text_stamp_bounds(stamps[k],&bounds)==NX_OK&&bounds.x==offset.x&&bounds.y==offset.y);
        CHECK(fabs(bounds.width*density-view.width)<.001&&fabs(bounds.height*density-view.height)<.001);
        nx_text_stamp *empty=NULL;CHECK(nx_text_stamp_create_scaled(blank,density,65536,NULL,&empty)==NX_OK);
        CHECK(nx_text_stamp_bounds(empty,&bounds)==NX_OK&&!bounds.width&&!bounds.height);
        nx_text_stamp_destroy(empty);
        CHECK(nx_text_layout_width(layout)==width&&nx_text_layout_height(layout)==height&&nx_text_layout_line_count(layout)==lines);
        CHECK(nx_text_layout_caret_at(layout,1,&caret)==NX_OK);
        CHECK(caret.byte_offset==before.byte_offset&&caret.line_index==before.line_index&&
            caret.x==before.x&&caret.top==before.top&&caret.height==before.height&&caret.affinity==before.affinity);
    }
    /* All source/layout objects are gone before these cached stamps render. */
    nx_text_layout_destroy(blank);nx_text_layout_destroy(layout);nx_font_collection_destroy(fonts);nx_font_close(font);
    memset(bytes,0,sizeof(bytes));
    for(unsigned k=0;k<6;++k){size_t size=(size_t)widths[k]*heights[k]*4;uint8_t *actual=malloc(size);CHECK(actual);
        CHECK(nx_begin_frame(ui,80,64,0)==NX_OK);
        CHECK(nx_label_stamp(ui,1,0,(nx_rect){8,4,60,48},stamps[k],(nx_point){8,4},(nx_color){1,1,1,1})==NX_OK);
        CHECK(nx_end_frame(ui)==NX_OK&&!render_scaled(ui,scales[k],actual,widths[k],heights[k]));
        CHECK(memcmp(actual,expected[k],size)==0);free(actual);free(expected[k]);nx_text_stamp_destroy(stamps[k]);
        uint32_t index;nx_semantic_view semantic;
        CHECK(nx_semantics_find(ui,1,&index)==NX_OK&&nx_semantics_get(ui,index,&semantic)==NX_OK);
        CHECK(semantic.name.length==5&&!memcmp(semantic.name.data,"010\n1",5));
    }
    nx_context_destroy(ui);return 0;
}

static int failures_and_budget(void){
    uint8_t bytes[512];font_fixture(bytes);put16(bytes+272,0); /* zero advance: overlapping ink */
    nx_font *font=NULL;nx_font_collection *fonts=NULL;nx_text_layout *layout=NULL;
    CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    CHECK(nx_text_layout_create(fonts,"010",3,NX_BIDI_LTR,19.25,80,&layout)==NX_OK);
    size_t failures=0;
    for(size_t failure=1;failure<200;++failure){heap h={0};h.fail=failure;nx_allocator a={&h,allocate,release};
        nx_text_stamp *stamp=(nx_text_stamp*)(uintptr_t)1;
        nx_result r=nx_text_stamp_create_scaled(layout,1.5f,65536,&a,&stamp);
        if(r==NX_ERROR_MEMORY){CHECK(!stamp);++failures;}
        else{CHECK(r==NX_OK);nx_text_stamp_destroy(stamp);}
        CHECK(!h.live&&!h.bad);if(r==NX_OK)break;
    }CHECK(failures>=8&&failures<198);
    int direct_success=0;
    for(size_t failure=1;failure<200;++failure){heap h={0};nx_context_desc desc={0};
        desc.allocator=(nx_allocator){&h,allocate,release};nx_context *ui=NULL;
        CHECK(nx_context_create(&desc,&ui)==NX_OK&&nx_begin_frame(ui,80,64,0)==NX_OK);
        CHECK(nx_fill_rect(ui,(nx_rect){0,0,1,1},(nx_color){1,0,0,1})==NX_OK);
        size_t live=h.live;h.fail=h.calls+failure;
        nx_result r=nx_draw_text_layout_scaled(ui,layout,(nx_point){0},(nx_color){1,1,1,1},1.5f,65536);
        CHECK(r==NX_OK||r==NX_ERROR_MEMORY);
        if(r!=NX_OK)CHECK(h.live==live);
        h.fail=0;CHECK(nx_end_frame(ui)==NX_OK);const nx_command *commands;size_t count;
        CHECK(nx_frame_commands(ui,&commands,&count)==NX_OK);
        CHECK(count==(r==NX_OK?4:1));
        nx_context_destroy(ui);CHECK(!h.live&&!h.bad);if(r==NX_OK){direct_success=1;break;}
    }CHECK(direct_success);
    const float invalid[]={0,.124f,8.01f,NAN,INFINITY};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);++i){nx_text_stamp *stamp=(nx_text_stamp*)(uintptr_t)1;
        CHECK(nx_text_stamp_create_scaled(layout,invalid[i],65536,NULL,&stamp)==NX_ERROR_ARGUMENT&&!stamp);}
    nx_context *ui=NULL;CHECK(nx_context_create(NULL,&ui)==NX_OK&&nx_begin_frame(ui,80,64,0)==NX_OK);
    CHECK(nx_fill_rect(ui,(nx_rect){0,0,1,1},(nx_color){1,0,0,1})==NX_OK);
    CHECK(nx_draw_text_layout_scaled(ui,layout,(nx_point){0},(nx_color){1,1,1,1},NAN,65536)==NX_ERROR_ARGUMENT);
    CHECK(nx_end_frame(ui)==NX_OK);const nx_command *commands;size_t count;
    CHECK(nx_frame_commands(ui,&commands,&count)==NX_OK&&count==1);nx_context_destroy(ui);
    nx_text_layout_destroy(layout);
    char *many=malloc(8000);CHECK(many);memset(many,'0',8000);
    CHECK(nx_text_layout_create(fonts,many,8000,NX_BIDI_LTR,1000,10000,&layout)==NX_OK);free(many);
    heap h={0};nx_allocator a={&h,allocate,release};nx_text_stamp *stamp=NULL;
    CHECK(nx_text_stamp_create_scaled(layout,1,65536,&a,&stamp)==NX_ERROR_LIMIT&&!stamp);
    CHECK(h.peak>60u*1024u*1024u&&h.peak<=64u*1024u*1024u&&!h.live&&!h.bad);
    printf("construction cap: peak %zu bytes, all released; %zu constructor allocation failures checked\n",h.peak,failures);
    nx_text_layout_destroy(layout);nx_font_collection_destroy(fonts);nx_font_close(font);return 0;
}
int main(void){if(density_pixels()||failures_and_budget())return 1;puts("text density pixels, lifetime, OOM and budget passed");return 0;}
