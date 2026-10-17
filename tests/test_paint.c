#include "nexaui/nexaui.h"
#include "paint_scene.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
typedef struct heap {size_t calls,fail,bytes,live;} heap;
static void *allocate(void *u,size_t n) {heap *h=u;if(++h->calls==h->fail)return NULL;
    void *p=malloc(n);if(p){h->bytes+=n;++h->live;}return p;}
static void release(void *u,void *p,size_t n) {heap *h=u;h->bytes-=n;--h->live;free(p);}
static int gradients(void) {
    nx_color_stop stops[]={{0,{1,0,0,1}},{1,{0,0,1,1}}};
    nx_surface_image_desc d={10,8,0,1,{NX_PAINT_LINEAR,{0},{0,0},{10,0},stops,2},100000};
    nx_paint_image *image=NULL;nx_bitmap_view b;nx_rect r;
    CHECK(nx_surface_image_create(&d,NULL,&image)==NX_OK);
    CHECK(nx_paint_image_view(image,&b,&r)==NX_OK&&b.width==10&&b.height==8);
    CHECK(b.pixels[0]==242&&b.pixels[2]==13&&b.pixels[3]==255);
    CHECK(b.pixels[4*9]==13&&b.pixels[4*9+2]==242);
    nx_paint_image_destroy(image);image=NULL;
    stops[1].color.a=0;
    CHECK(nx_surface_image_create(&d,NULL,&image)==NX_OK);
    CHECK(nx_paint_image_view(image,&b,&r)==NX_OK);
    CHECK(b.pixels[4*5]==255&&b.pixels[4*5+2]==0&&b.pixels[4*5+3]==115);
    nx_paint_image_destroy(image);image=NULL;
    d.paint.to=d.paint.from;
    CHECK(nx_surface_image_create(&d,NULL,&image)==NX_OK);
    CHECK(nx_paint_image_view(image,&b,&r)==NX_OK&&b.pixels[3]==0);
    nx_paint_image_destroy(image);image=NULL;
    stops[1].color.a=1;d.paint.kind=NX_PAINT_RADIAL;d.paint.from=(nx_point){.5f,.5f};d.paint.to=(nx_point){4.5f,.5f};
    CHECK(nx_surface_image_create(&d,NULL,&image)==NX_OK);
    CHECK(nx_paint_image_view(image,&b,&r)==NX_OK&&b.pixels[0]==255&&b.pixels[2]==0&&b.pixels[4*4+2]==255);
    nx_paint_image_destroy(image);
    d.radius=100;d.scale=2;
    CHECK(nx_surface_image_create(&d,NULL,&image)==NX_OK);
    CHECK(nx_paint_image_view(image,&b,&r)==NX_OK&&b.width==20&&b.pixels[3]==0);
    nx_paint_image_destroy(image);
    nx_paint_image *sentinel=(nx_paint_image *)(uintptr_t)1;image=sentinel;
    stops[1].position=0;CHECK(nx_surface_image_create(&d,NULL,&image)==NX_ERROR_ARGUMENT&&image==sentinel);
    stops[1].position=1;d.width=NAN;CHECK(nx_surface_image_create(&d,NULL,&image)==NX_ERROR_ARGUMENT);
    d.width=1e6f;CHECK(nx_surface_image_create(&d,NULL,&image)==NX_ERROR_LIMIT);
    d.width=10;d.byte_budget=1;CHECK(nx_surface_image_create(&d,NULL,&image)==NX_ERROR_LIMIT);
    d.byte_budget=100000;
    for(size_t fail=1;fail<=2;++fail) {
        heap h={0};h.fail=fail;nx_allocator a={&h,allocate,release};
        CHECK(nx_surface_image_create(&d,&a,&image)==NX_ERROR_MEMORY&&image==sentinel&&!h.bytes&&!h.live);
        h.fail=0;CHECK(nx_surface_image_create(&d,&a,&image)==NX_OK);
        nx_paint_image_destroy(image);CHECK(!h.live&&!h.bytes);image=sentinel;
    }
    return 0;
}
static int shadows(void) {
    nx_shadow_image_desc d={20,10,3,1,2,{4,-3},{0,1,1,1},1000000};
    nx_paint_image *image=NULL;nx_bitmap_view b;nx_rect r;
    CHECK(nx_shadow_image_create(&d,NULL,&image)==NX_OK);
    CHECK(nx_paint_image_view(image,&b,&r)==NX_OK&&b.width==34&&b.height==24&&r.x==-3&&r.y==-10);
    CHECK(b.pixels[3]==0);
    unsigned distinct[256]={0},count=0;
    for(uint32_t x=0;x<b.width;++x) distinct[b.pixels[(12*b.width+x)*4+3]]=1;
    for(unsigned x=0;x<256;++x) count+=distinct[x];
    CHECK(count>8); /* Genuine soft edge, not a flat translucent rectangle. */
    CHECK(b.pixels[(12*b.width+5)*4+3]>0&&b.pixels[(12*b.width+17)*4+3]>200);
    nx_paint_image_destroy(image);image=(nx_paint_image *)(uintptr_t)1;
    for(size_t fail=1;fail<=4;++fail) {
        heap h={0};h.fail=fail;nx_allocator a={&h,allocate,release};
        CHECK(nx_shadow_image_create(&d,&a,&image)==NX_ERROR_MEMORY&&image==(nx_paint_image *)(uintptr_t)1&&!h.live&&!h.bytes);
        h.fail=0;CHECK(nx_shadow_image_create(&d,&a,&image)==NX_OK);
        nx_paint_image_destroy(image);CHECK(!h.live&&!h.bytes);image=(nx_paint_image *)(uintptr_t)1;
    }
    d.softness=65;CHECK(nx_shadow_image_create(&d,NULL,&image)==NX_ERROR_LIMIT);
    d.softness=NAN;CHECK(nx_shadow_image_create(&d,NULL,&image)==NX_ERROR_ARGUMENT);
    return 0;
}
static int scenes(const char *path) {
    const float density[]={1,1.25f,1.5f,2};
    for(unsigned s=0;s<4;++s) {
        nx_context *ui=NULL;nx_paint_image *images[3]={0};
        CHECK(nx_context_create(NULL,&ui)==NX_OK&&paint_scene(ui,images,density[s])==0);
        const nx_command *commands;size_t count;
        CHECK(nx_frame_commands(ui,&commands,&count)==NX_OK);
        uint8_t rgba[180*140*4];
        CHECK(nx_headless_render(commands,count,rgba,180,140,180*4,(nx_color){.01f,.025f,.05f,1})==NX_OK);
        CHECK(rgba[(25*180+30)*4+2]>20&&rgba[(0*180+0)*4+2]==13);
        if(path&&s==0){FILE *f=fopen(path,"wb");CHECK(f);fprintf(f,"P6\n180 140\n255\n");
            for(size_t i=0;i<180*140;++i)CHECK(fwrite(rgba+4*i,1,3,f)==3);CHECK(!fclose(f));}
        paint_scene_free(images);nx_context_destroy(ui);
    }
    return 0;
}
static int icon_matrix(const char *path) {
    nx_context *ui=NULL;CHECK(nx_context_create(NULL,&ui)==NX_OK);
    CHECK(NX_ICON_SUN==28&&NX_ICON_SHIELD==29);
    CHECK(nx_begin_frame(ui,720,288,0)==NX_OK);
    const float sizes[]={16,24,32,48};
    for(unsigned theme=0;theme<4;++theme){
        nx_theme t;CHECK(nx_theme_preset((nx_theme_kind)(theme+1),&t)==NX_OK);
        CHECK(nx_fill_rect(ui,(nx_rect){0,theme*72.f,720,72},t.colors[NX_COLOR_BACKGROUND])==NX_OK);
        for(unsigned size=0;size<4;++size)for(unsigned icon=0;icon<3;++icon)
            CHECK(nx_draw_icon(ui,(nx_icon)(NX_ICON_SHIELD+icon),
                (nx_rect){size*180.f+12+icon*52.f,theme*72.f+12,sizes[size],sizes[size]},
                sizes[size]/16,t.colors[NX_COLOR_ACCENT])==NX_OK);
    }
    CHECK(nx_draw_icon(ui,NX_ICON_SHIELD,(nx_rect){0,0,24,24},NAN,(nx_color){1,1,1,1})==NX_ERROR_ARGUMENT);
    CHECK(nx_end_frame(ui)==NX_OK);const nx_command *commands;size_t count;
    CHECK(nx_frame_commands(ui,&commands,&count)==NX_OK);
    uint8_t *rgba=malloc(720*288*4);CHECK(rgba);
    CHECK(nx_headless_render(commands,count,rgba,720,288,720*4,(nx_color){0,0,0,1})==NX_OK);
    if(path){FILE *f=fopen(path,"wb");CHECK(f);fprintf(f,"P6\n720 288\n255\n");
        for(size_t i=0;i<720*288;++i)CHECK(fwrite(rgba+4*i,1,3,f)==3);CHECK(!fclose(f));}
    free(rgba);nx_context_destroy(ui);return 0;
}
int main(int argc,char **argv) {
    CHECK(!gradients()&&!shadows()&&!scenes(argc>1?argv[1]:NULL)&&!icon_matrix(argc>2?argv[2]:NULL));
    clock_t begin=clock();nx_paint_image *image=NULL;
    nx_shadow_image_desc d={640,360,12,2,4,{0,4},{0,.5f,1,.6f},32u*1024u*1024u};
    CHECK(nx_shadow_image_create(&d,NULL,&image)==NX_OK);
    printf("1280x720 soft-card build CPU: %.3f ms; draw reuses immutable pixels\n",1000.*(clock()-begin)/CLOCKS_PER_SEC);
    nx_paint_image_destroy(image);return 0;
}
