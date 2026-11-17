#define _POSIX_C_SOURCE 200809L
#include "pdf_export.h"
#include "pdf_frame.h"
#include "png_frame.h"

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int valid_rect(nx_rect rect) {
    return isfinite(rect.x) && isfinite(rect.y) && isfinite(rect.width) && isfinite(rect.height) &&
        rect.width>0 && rect.height>0;
}

nx_result content_pdf_export_render(const char *renderer, int crop, int file, unsigned page,
    pdf_export_view view, nx_bitmap_view *out) {
    if (!out || !view.width || !view.height || !isfinite(view.scale) || view.scale<=0 ||
        !valid_rect(view.viewport) || !valid_rect(view.page)) return NX_ERROR_ARGUMENT;
    double w=ceil(view.width*view.scale), h=ceil(view.height*view.scale);
    if (w>NX_VIEWPORT_MAX_DIMENSION || h>NX_VIEWPORT_MAX_DIMENSION || w*h>CONTENT_PNG_PIXEL_LIMIT/4u)
        return NX_ERROR_LIMIT;
    uint32_t width=(uint32_t)w, height=(uint32_t)h;
    double fit=fmin(w/view.viewport.width,h/view.viewport.height);
    double cw=view.viewport.width*fit, ch=view.viewport.height*fit;
    double cx=(w-cw)*.5, cy=(h-ch)*.5;
    double px=cx+((double)view.page.x-view.viewport.x)*fit;
    double py=cy+((double)view.page.y-view.viewport.y)*fit;
    double pw=view.page.width*fit, ph=view.page.height*fit;
    if (fabs(px)>FLT_MAX || fabs(py)>FLT_MAX || pw>FLT_MAX || ph>FLT_MAX) return NX_ERROR_LIMIT;
    nx_rect clip={(float)cx,(float)cy,(float)cw,(float)ch};
    nx_rect image={(float)px,(float)py,(float)pw,(float)ph};
    if (!valid_rect(clip) || !valid_rect(image)) return NX_ERROR_LIMIT;
    unsigned edge=(unsigned)ceil(fmin(CONTENT_PDF_RASTER_EDGE,fmax(pw,ph)));
    nx_bitmap_view raster;
    nx_result result=content_pdf_render_detail(renderer,crop,file,page,edge,&raster);
    if (result!=NX_OK) return result;
    nx_command commands[]={
        {.type=NX_COMMAND_PUSH_CLIP,.rect=clip},
        {.type=NX_COMMAND_BITMAP,.rect=image,.color={1,1,1,1},.bitmap=raster},
        {.type=NX_COMMAND_POP_CLIP}
    };
    size_t stride=(size_t)width*4u;
    uint8_t *pixels=malloc(stride*height);
    result=pixels?nx_headless_render(commands,sizeof(commands)/sizeof(commands[0]),
        pixels,width,height,stride,(nx_color){1,1,1,1}):NX_ERROR_MEMORY;
    free((void *)raster.pixels);
    if (result==NX_OK) *out=(nx_bitmap_view){pixels,width,height,stride,NX_BITMAP_RGBA8};
    else free(pixels);
    return result;
}

nx_result content_pdf_export_png(const char *path, nx_bitmap_view image) {
    if (!path || path[0]!='/' || !image.pixels || image.format!=NX_BITMAP_RGBA8 || !image.width || !image.height)
        return NX_ERROR_ARGUMENT;
    if (image.width>NX_VIEWPORT_MAX_DIMENSION || image.height>NX_VIEWPORT_MAX_DIMENSION ||
        (size_t)image.width*4u>CONTENT_PNG_PIXEL_LIMIT/image.height) return NX_ERROR_LIMIT;
    if (image.stride_bytes!=(size_t)image.width*4u) return NX_ERROR_ARGUMENT;
    char temporary[PATH_MAX];
    int chars=snprintf(temporary,sizeof(temporary),"%s.tmp.XXXXXX",path);
    if (chars<0 || (size_t)chars>=sizeof(temporary)) return NX_ERROR_LIMIT;
    int fd=mkstemp(temporary);
    if (fd<0) return NX_ERROR_PLATFORM;
    FILE *stream=fdopen(fd,"wb");
    nx_result result=NX_ERROR_PLATFORM;
    if (stream) {
        png_image png={.version=PNG_IMAGE_VERSION,.width=image.width,.height=image.height,.format=PNG_FORMAT_RGBA};
        int written=png_image_write_to_stdio(&png,stream,0,image.pixels,0,NULL);
        png_image_free(&png);
        if (fclose(stream)==0 && written) {
            if (!link(temporary,path)) result=NX_OK;
            else if (errno==EEXIST) result=NX_ERROR_STATE;
        }
    } else close(fd);
    if (unlink(temporary) && result==NX_OK) result=NX_ERROR_PLATFORM;
    return result;
}
