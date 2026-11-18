#include "jpeg_frame.h"
#include "png_frame.h"
#include <libexif/exif-data.h>
#include <turbojpeg.h>
#include <stdlib.h>
#include <string.h>

#define JPEG_SOURCE_PIXELS (64 * 1024 * 1024)

static unsigned orientation(const void *encoded,size_t size) {
    unsigned value=1;
    ExifData *data=exif_data_new_from_data(encoded,(unsigned int)size);
    if (data) {
        ExifEntry *entry=exif_content_get_entry(data->ifd[EXIF_IFD_0],EXIF_TAG_ORIENTATION);
        if (entry && entry->format==EXIF_FORMAT_SHORT && entry->components==1 && entry->data && entry->size>=2) {
            unsigned tag=exif_get_short(entry->data,exif_data_get_byte_order(data));
            if (tag>=1 && tag<=8) value=tag;
        }
        exif_data_unref(data);
    }
    return value;
}

static tjscalingfactor fit_factor(int width,int height) {
    int count=0;
    const tjscalingfactor *factors=tj3GetScalingFactors(&count);
    tjscalingfactor best={0,1};
    for (int i=0;i<count;++i) {
        tjscalingfactor f=factors[i];
        if (f.num>f.denom || f.num*best.denom<=best.num*f.denom) continue;
        unsigned w=(unsigned)TJSCALED(width,f),h=(unsigned)TJSCALED(height,f);
        if (w<=CONTENT_PNG_SOURCE_DIMENSION && h<=CONTENT_PNG_SOURCE_DIMENSION &&
            w*4u<=CONTENT_PNG_PIXEL_LIMIT/h) best=f;
    }
    return best;
}

static uint8_t *oriented(const uint8_t *pixels,uint32_t width,uint32_t height,unsigned tag) {
    static const struct { int swap, flip_x, flip_y; } transforms[]={
        {0,0,0}, {0,1,0}, {0,1,1}, {0,0,1}, {1,0,0}, {1,1,0}, {1,1,1}, {1,0,1}
    };
    int swap=transforms[tag-1].swap;
    uint32_t w=swap?height:width,h=swap?width:height;
    uint8_t *rotated=malloc((size_t)w*h*4u);
    if (!rotated) return NULL;
    for (uint32_t y=0;y<height;++y) {
        for (uint32_t x=0;x<width;++x) {
            uint32_t dx=swap?y:x,dy=swap?x:y;
            if (transforms[tag-1].flip_x) dx=w-1u-dx;
            if (transforms[tag-1].flip_y) dy=h-1u-dy;
            memcpy(rotated+((size_t)dy*w+dx)*4u,pixels+((size_t)y*width+x)*4u,4u);
        }
    }
    return rotated;
}

nx_result content_jpeg_bitmap(const void *encoded,size_t size,nx_bitmap_view *out,
    content_jpeg_size *original) {
    if (!encoded || !size || !out || !original) return NX_ERROR_ARGUMENT;
    if (size>CONTENT_PNG_ENCODED_LIMIT) return NX_ERROR_LIMIT;
    tjhandle jpeg=tj3Init(TJINIT_DECOMPRESS);
    if (!jpeg) return NX_ERROR_MEMORY;
    uint8_t *pixels=NULL;
    nx_result result=NX_ERROR_PLATFORM;
    if (tj3Set(jpeg,TJPARAM_STOPONWARNING,1) || tj3Set(jpeg,TJPARAM_SCANLIMIT,128) ||
        tj3Set(jpeg,TJPARAM_MAXMEMORY,64) || tj3Set(jpeg,TJPARAM_MAXPIXELS,JPEG_SOURCE_PIXELS) ||
        tj3DecompressHeader(jpeg,encoded,size)) goto done;
    int width=tj3Get(jpeg,TJPARAM_JPEGWIDTH),height=tj3Get(jpeg,TJPARAM_JPEGHEIGHT);
    if (width<=0 || height<=0) goto done;
    result=NX_ERROR_LIMIT;
    if (width>JPEG_SOURCE_PIXELS/height) goto done;
    int colorspace=tj3Get(jpeg,TJPARAM_COLORSPACE);
    if (tj3Get(jpeg,TJPARAM_PRECISION)!=8 || tj3Get(jpeg,TJPARAM_LOSSLESS) ||
        (colorspace!=TJCS_RGB && colorspace!=TJCS_YCbCr && colorspace!=TJCS_GRAY)) {
        result=NX_ERROR_UNSUPPORTED;
        goto done;
    }
    tjscalingfactor factor=fit_factor(width,height);
    if (!factor.num) goto done;
    result=NX_ERROR_PLATFORM;
    if (tj3SetScalingFactor(jpeg,factor)) goto done;
    uint32_t w=(uint32_t)TJSCALED(width,factor),h=(uint32_t)TJSCALED(height,factor);
    pixels=malloc((size_t)w*h*4u);
    if (!pixels) { result=NX_ERROR_MEMORY; goto done; }
    if (tj3Decompress8(jpeg,encoded,size,pixels,0,TJPF_RGBA)) goto done;

    unsigned tag=orientation(encoded,size);
    if (tag!=1) {
        uint8_t *replacement=oriented(pixels,w,h,tag);
        if (!replacement) { result=NX_ERROR_MEMORY; goto done; }
        free(pixels);
        pixels=replacement;
    }
    if (tag>=5) {
        uint32_t previous_w=w;
        w=h;
        h=previous_w;
    }
    *out=(nx_bitmap_view){pixels,w,h,(size_t)w*4u,NX_BITMAP_RGBA8};
    *original=tag>=5?(content_jpeg_size){(uint32_t)height,(uint32_t)width}:
        (content_jpeg_size){(uint32_t)width,(uint32_t)height};
    pixels=NULL;
    result=NX_OK;
done:
    free(pixels);
    tj3Destroy(jpeg);
    return result;
}
