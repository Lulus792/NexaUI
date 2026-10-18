#include "transfer_icon_scene.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define WIDTH 180u
#define HEIGHT 140u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
static const uint8_t *pixel(const uint8_t *image,unsigned x,unsigned y) {
    return image+((size_t)y*WIDTH+x)*4;
}
static int distinct(const uint8_t *image,float scale,float size,unsigned column) {
    nx_rect r=transfer_icon_bounds(0,0);unsigned x=(unsigned)(r.x*scale),y=(unsigned)(r.y*scale);
    unsigned delta=(unsigned)(28*scale)*column,width=(unsigned)ceilf(size*scale);
    for(unsigned row=0;row<width;++row)
        if(memcmp(pixel(image,x,y+row),pixel(image,x+delta,y+row),width*4))return 1;
    return 0;
}
static int ink_near(const uint8_t *image,float scale,float size,float x,float y,int radius) {
    nx_rect r=transfer_icon_bounds(0,0);
    int cx=(int)((r.x+x*size/24)*scale),cy=(int)((r.y+y*size/24)*scale);
    for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx)
        if(memcmp(pixel(image,(unsigned)(cx+dx),(unsigned)(cy+dy)),image,4))return 1;
    return 0;
}
static int inside(nx_rect r,float scale,unsigned x,unsigned y) {
    double px=x+.5,py=y+.5;
    return px>=(double)r.x*scale && py>=(double)r.y*scale &&
           px<(double)(r.x+r.width)*scale && py<(double)(r.y+r.height)*scale;
}
static int verify(const uint8_t *image,const uint8_t *unclipped,float scale,float size) {
    CHECK(distinct(image,scale,size,1)); /* Before R02, Transfer is byte-equal to Upload. */
    CHECK(distinct(image,scale,size,2));
    /* Visual landmarks, independent of command count/order: two shafts,
       upper-left head, lower-right head; opposite heads remain absent. */
    CHECK(ink_near(image,scale,size,8,12.5f,1));
    CHECK(ink_near(image,scale,size,16,8.5f,1));
    CHECK(ink_near(image,scale,size,5.5f,6.5f,1));
    CHECK(ink_near(image,scale,size,18.5f,14.5f,1));
    CHECK(!ink_near(image,scale,size,5.5f,14.5f,0));
    CHECK(!ink_near(image,scale,size,18.5f,6.5f,0));
    /* Independently crop an un-clipped render of the identical physical
       geometry. This avoids assuming float translation invariance at26px. */
    for(unsigned y=(unsigned)floorf(34*scale);y<(unsigned)ceilf(66*scale);++y)
        for(unsigned x=0;x<(unsigned)ceilf(90*scale);++x) {
            int visible=0;
            if(inside(transfer_icon_outer_clip(),scale,x,y))
                for(unsigned column=0;column<3;++column)
                    visible|=inside(transfer_icon_inner_clip(column),scale,x,y);
            const uint8_t *expected=visible?pixel(unclipped,x,y):image;
            CHECK(!memcmp(pixel(image,x,y),expected,4));
        }
    return 0;
}
int main(void) {
    const float scales[]={1,1.25f,1.5f,2};
    nx_context *ui=NULL;CHECK(nx_context_create(NULL,&ui)==NX_OK);
    uint8_t *image=malloc(WIDTH*HEIGHT*4),*unclipped=malloc(WIDTH*HEIGHT*4);CHECK(image&&unclipped);
    for(unsigned size=24;size<=26;size+=2)for(unsigned theme=NX_THEME_DARK;theme<=NX_THEME_MIDNIGHT;++theme)
        for(unsigned density=0;density<4;++density)for(unsigned alpha=0;alpha<2;++alpha) {
            nx_color clear;
            CHECK(!(size==24?transfer_icon_scene(ui,(nx_theme_kind)theme,alpha?.55f:1,&clear):
                transfer_icon_scene_sized(ui,(nx_theme_kind)theme,alpha?.55f:1,(float)size,&clear)));
            const nx_command *commands;size_t count;
            CHECK(nx_frame_commands(ui,&commands,&count)==NX_OK);
            nx_command *scaled=malloc(count*sizeof(*scaled));CHECK(scaled);
            transfer_icon_scale(commands,count,scales[density],scaled);
            CHECK(nx_headless_render(scaled,count,image,WIDTH,HEIGHT,WIDTH*4,clear)==NX_OK);
            size_t visible_count=0;
            for(size_t i=0;i<count;++i)
                if(scaled[i].type!=NX_COMMAND_PUSH_CLIP&&scaled[i].type!=NX_COMMAND_POP_CLIP)
                    scaled[visible_count++]=scaled[i];
            CHECK(nx_headless_render(scaled,visible_count,unclipped,WIDTH,HEIGHT,WIDTH*4,clear)==NX_OK);
            int result=verify(image,unclipped,scales[density],(float)size);free(scaled);
            if(result)fprintf(stderr,"transfer size=%u theme=%u density=%.2f opacity=%.2f\n",size,theme,scales[density],alpha?.55f:1);
            CHECK(!result);
        }
    free(unclipped);free(image);nx_context_destroy(ui);
    puts("Transfer icon: 64 size/theme/density/alpha frames, both arrowheads and independent clip pixels passed");return 0;
}
