#include "nexaui/nexaui.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define NX_PAINT_BYTES (64u * 1024u * 1024u)
struct nx_paint_image {
    nx_allocator allocator;
    nx_bitmap_view bitmap;
    nx_rect bounds;
    size_t bytes;
};
static void *paint_alloc(void *u,size_t n) { (void)u; return malloc(n); }
static void paint_free(void *u,void *p,size_t n) { (void)u;(void)n;free(p); }
static int color_ok(nx_color c) {
    return isfinite(c.r)&&isfinite(c.g)&&isfinite(c.b)&&isfinite(c.a)&&
        c.r>=0&&c.r<=1&&c.g>=0&&c.g<=1&&c.b>=0&&c.b<=1&&c.a>=0&&c.a<=1;
}
static int point_ok(nx_point p) {
    return isfinite(p.x)&&isfinite(p.y)&&fabsf(p.x)<=1048576&&fabsf(p.y)<=1048576;
}
static int shape_ok(float w,float h,float r,float scale) {
    return isfinite(w)&&isfinite(h)&&isfinite(r)&&isfinite(scale)&&
        w>0&&h>0&&w<=1048576&&h<=1048576&&r>=0&&scale>=.125f&&scale<=8;
}
static uint8_t byte(double x) { return (uint8_t)floor(fmax(0,fmin(1,x))*255+.5); }
static int inside(double x,double y,double w,double h,double r) {
    if(x<0||y<0||x>=w||y>=h) return 0;
    double dx=fmax(fmax(r-x,x-(w-r)),0),dy=fmax(fmax(r-y,y-(h-r)),0);
    return dx*dx+dy*dy<=r*r;
}
static nx_color paint_sample(const nx_paint *p,double x,double y) {
    if(p->kind==NX_PAINT_SOLID) return p->color;
    double dx=(double)p->to.x-p->from.x,dy=(double)p->to.y-p->from.y;
    double length=dx*dx+dy*dy;
    double t=length==0?1:p->kind==NX_PAINT_LINEAR?
        ((x-p->from.x)*dx+(y-p->from.y)*dy)/length:
        hypot(x-p->from.x,y-p->from.y)/sqrt(length);
    t=fmax(0,fmin(1,t)); size_t i=1;
    while(i+1<p->stop_count && t>p->stops[i].position) ++i;
    const nx_color_stop *a=&p->stops[i-1],*b=&p->stops[i];
    t=(t-a->position)/(b->position-a->position);
    double alpha=a->color.a*(1-t)+b->color.a*t;
    nx_color c={0,0,0,(float)alpha};
    if(alpha>0) {
        c.r=(float)((a->color.r*a->color.a*(1-t)+b->color.r*b->color.a*t)/alpha);
        c.g=(float)((a->color.g*a->color.a*(1-t)+b->color.g*b->color.a*t)/alpha);
        c.b=(float)((a->color.b*a->color.a*(1-t)+b->color.b*b->color.a*t)/alpha);
    }
    return c;
}
static int paint_ok(const nx_paint *p) {
    if(p->kind==NX_PAINT_SOLID) return color_ok(p->color);
    if((p->kind!=NX_PAINT_LINEAR&&p->kind!=NX_PAINT_RADIAL)||
        !point_ok(p->from)||!point_ok(p->to)||!p->stops||p->stop_count<2||p->stop_count>16)
        return 0;
    if(p->stops[0].position!=0||p->stops[p->stop_count-1].position!=1) return 0;
    for(size_t i=0;i<p->stop_count;++i)
        if(!isfinite(p->stops[i].position)||!color_ok(p->stops[i].color)||
            (i&&p->stops[i].position<=p->stops[i-1].position)) return 0;
    return 1;
}
/* Zero outside the padded domain. Running sums bound each pass by O(pixels),
   independent of blur radius; three horizontal/vertical pairs approximate a
   Gaussian using the explicitly documented box kernel. */
static void box_pass(const float *src,float *dst,uint32_t w,uint32_t h,unsigned r,int vertical) {
    uint32_t outer=vertical?w:h,inner=vertical?h:w;
    for(uint32_t line=0;line<outer;++line) {
        double sum=0;
        for(uint32_t j=0;j<=r&&j<inner;++j)
            sum+=src[vertical?(size_t)j*w+line:(size_t)line*w+j];
        for(uint32_t j=0;j<inner;++j) {
            dst[vertical?(size_t)j*w+line:(size_t)line*w+j]=(float)(sum/(2*r+1));
            if(j>=r) sum-=src[vertical?(size_t)(j-r)*w+line:(size_t)line*w+j-r];
            if((size_t)j+r+1<inner)
                sum+=src[vertical?(size_t)(j+r+1)*w+line:(size_t)line*w+j+r+1];
        }
    }
}
static nx_result create_image(const nx_surface_image_desc *d,unsigned blur,nx_point offset,
    const nx_allocator *allocator,nx_paint_image **out) {
    if(!out||!d||!shape_ok(d->width,d->height,d->radius,d->scale)||!paint_ok(&d->paint)||
        !point_ok(offset)||(allocator&&(!!allocator->allocate!=!!allocator->deallocate)))
        return NX_ERROR_ARGUMENT;
    unsigned padding=blur?3*blur+1:0;
    double wd=ceil((double)d->width*d->scale)+2*padding;
    double hd=ceil((double)d->height*d->scale)+2*padding;
    if(wd>4096||hd>4096||wd*hd>4194304) return NX_ERROR_LIMIT;
    uint32_t w=(uint32_t)wd,h=(uint32_t)hd;
    size_t pixels=(size_t)w*h,bytes=pixels*4,peak=sizeof(nx_paint_image)+bytes;
    if(blur) peak+=pixels*sizeof(float)*2;
    if(!d->byte_budget||peak>d->byte_budget||peak>NX_PAINT_BYTES) return NX_ERROR_LIMIT;
    nx_allocator a=allocator&&allocator->allocate?*allocator:(nx_allocator){NULL,paint_alloc,paint_free};
    nx_paint_image *image=a.allocate(a.user,sizeof(*image));
    if(!image) return NX_ERROR_MEMORY;
    memset(image,0,sizeof(*image));image->allocator=a;image->bytes=bytes;
    uint8_t *rgba=a.allocate(a.user,bytes);
    if(!rgba) {nx_paint_image_destroy(image);return NX_ERROR_MEMORY;}
    image->bitmap=(nx_bitmap_view){rgba,w,h,(size_t)w*4,NX_BITMAP_RGBA8};
    image->bounds=(nx_rect){offset.x-padding/d->scale,offset.y-padding/d->scale,w/d->scale,h/d->scale};
    float *mask=NULL,*temp=NULL;
    if(blur) {
        mask=a.allocate(a.user,pixels*sizeof(float));temp=a.allocate(a.user,pixels*sizeof(float));
        if(!mask||!temp) {
            if(mask) a.deallocate(a.user,mask,pixels*sizeof(float));
            if(temp) a.deallocate(a.user,temp,pixels*sizeof(float));
            nx_paint_image_destroy(image);return NX_ERROR_MEMORY;
        }
    }
    double radius=fmin(d->radius,fmin(d->width,d->height)*.5);
    int row_constant=d->paint.kind==NX_PAINT_SOLID||
        (d->paint.kind==NX_PAINT_LINEAR&&d->paint.from.x==d->paint.to.x)||
        (d->paint.from.x==d->paint.to.x&&d->paint.from.y==d->paint.to.y);
    for(uint32_t y=0;y<h;++y) {
        double ymin=(y+.25-padding)/d->scale,ymax=(y+.75-padding)/d->scale;
        nx_color row={0};uint8_t row_rgb[3]={0};
        if(row_constant){row=paint_sample(&d->paint,(.5-padding)/d->scale,(y+.5-padding)/d->scale);
            row_rgb[0]=byte(row.r);row_rgb[1]=byte(row.g);row_rgb[2]=byte(row.b);}
        for(uint32_t x=0;x<w;++x) {
            double xmin=(x+.25-padding)/d->scale,xmax=(x+.75-padding)/d->scale;
            double coverage=0;
            /* The farthest Cartesian sample bounds all four original samples.
               Keep their exact arithmetic on partial/curved boundary pixels. */
            if(xmin>=0&&ymin>=0&&xmax<d->width&&ymax<d->height){
                double dx=fmax(fmax(radius-xmin,xmax-(d->width-radius)),0);
                double dy=fmax(fmax(radius-ymin,ymax-(d->height-radius)),0);
                if(dx*dx+dy*dy<=radius*radius)coverage=1;
            }
            if(coverage==0)for(unsigned sy=0;sy<2;++sy)for(unsigned sx=0;sx<2;++sx)
                coverage+=inside((x+.25+.5*sx-padding)/d->scale,
                    (y+.25+.5*sy-padding)/d->scale,d->width,d->height,radius)*.25;
            size_t i=(size_t)y*w+x;
            nx_color c=row_constant?row:paint_sample(&d->paint,(x+.5-padding)/d->scale,(y+.5-padding)/d->scale);
            rgba[4*i]=row_constant?row_rgb[0]:byte(c.r);rgba[4*i+1]=row_constant?row_rgb[1]:byte(c.g);
            rgba[4*i+2]=row_constant?row_rgb[2]:byte(c.b);rgba[4*i+3]=byte(c.a*coverage);
            if(mask) mask[i]=(float)coverage;
        }
    }
    if(mask) {
        for(unsigned pass=0;pass<3;++pass) {box_pass(mask,temp,w,h,blur,0);box_pass(temp,mask,w,h,blur,1);}
        for(size_t i=0;i<pixels;++i) rgba[4*i+3]=byte(d->paint.color.a*mask[i]);
        a.deallocate(a.user,mask,pixels*sizeof(float));a.deallocate(a.user,temp,pixels*sizeof(float));
    }
    *out=image;return NX_OK;
}
nx_result nx_surface_image_create(const nx_surface_image_desc *d,const nx_allocator *a,nx_paint_image **out) {
    return create_image(d,0,(nx_point){0,0},a,out);
}
nx_result nx_shadow_image_create(const nx_shadow_image_desc *d,const nx_allocator *a,nx_paint_image **out) {
    if(!d||!isfinite(d->softness)||d->softness<0||!isfinite(d->scale)||d->scale<=0)
        return NX_ERROR_ARGUMENT;
    double radius=ceil((double)d->softness*d->scale);
    if(radius>64) return NX_ERROR_LIMIT;
    nx_surface_image_desc surface={d->width,d->height,d->radius,d->scale,{0},d->byte_budget};
    surface.paint.kind=NX_PAINT_SOLID;surface.paint.color=d->color;
    return create_image(&surface,(unsigned)radius,d->offset,a,out);
}
nx_result nx_paint_image_view(const nx_paint_image *image,nx_bitmap_view *view,nx_rect *bounds) {
    if(!image||!view||!bounds) return NX_ERROR_ARGUMENT;
    *view=image->bitmap;*bounds=image->bounds;return NX_OK;
}
nx_result nx_draw_paint_image(nx_context *ctx,const nx_paint_image *image,nx_point origin,nx_color tint) {
    if(!image||!point_ok(origin)) return NX_ERROR_ARGUMENT;
    nx_rect r=image->bounds;r.x+=origin.x;r.y+=origin.y;
    return nx_draw_bitmap(ctx,r,image->bitmap,tint);
}
void nx_paint_image_destroy(nx_paint_image *image) {
    if(!image) return;
    nx_allocator a=image->allocator;
    if(image->bitmap.pixels) a.deallocate(a.user,(void *)image->bitmap.pixels,image->bytes);
    a.deallocate(a.user,image,sizeof(*image));
}
