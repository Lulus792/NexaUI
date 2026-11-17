#include "pdf_thumbnails.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void pdf_thumbnails_clear(pdf_thumbnails *cache) {
    for (unsigned i=0;i<PDF_THUMBNAIL_COUNT;++i) free(cache->cells[i].frame);
    memset(cache->cells,0,sizeof(cache->cells));
}

void pdf_thumbnails_configure(pdf_thumbnails *cache, uint64_t generation, double density) {
    density=fmin(4.0,fmax(1.0,density));
    uint32_t width=(uint32_t)ceil(PDF_THUMBNAIL_WIDTH*density);
    uint32_t height=(uint32_t)ceil(PDF_THUMBNAIL_HEIGHT*density);
    content_request *request=&cache->request;
    if (request->generation==generation && request->width==width && request->height==height) return;
    pdf_thumbnails_clear(cache);
    *request=(content_request){generation,request->resize+1u,width,height};
}

void pdf_thumbnails_set_group(pdf_thumbnails *cache, unsigned page) {
    unsigned first=page-(page-1u)%PDF_THUMBNAIL_COUNT;
    if (cache->first==first) return;
    pdf_thumbnails_clear(cache);
    cache->first=first;
}

void pdf_thumbnails_open(pdf_thumbnails *cache, unsigned page) {
    pdf_thumbnails_set_group(cache,page);
    ++cache->session;
    cache->open=1;
}

unsigned pdf_thumbnails_missing(const pdf_thumbnails *cache, unsigned total) {
    if (!cache->open) return 0;
    for (unsigned i=0;i<PDF_THUMBNAIL_COUNT && cache->first+i<=total;++i)
        if (!cache->cells[i].page) return cache->first+i;
    return 0;
}

int pdf_thumbnails_adopt(pdf_thumbnails *cache, pdf_thumbnail completed) {
    content_request key=cache->request, got=completed.request;
    if (!cache->first || completed.page<cache->first || completed.page-cache->first>=PDF_THUMBNAIL_COUNT ||
        got.generation!=key.generation || got.resize!=key.resize || got.width!=key.width || got.height!=key.height) {
        free(completed.frame);
        return 0;
    }
    size_t bytes=sizeof(content_frame_header)+(size_t)key.width*key.height*4u;
    if (completed.bytes!=bytes) { free(completed.frame);completed.frame=NULL; }
    pdf_thumbnail *cell=&cache->cells[completed.page-cache->first];
    free(cell->frame);
    *cell=completed;
    return 1;
}

nx_bitmap_view pdf_thumbnail_pixels(const pdf_thumbnail *thumbnail) {
    return (nx_bitmap_view){(const uint8_t *)thumbnail->frame+sizeof(content_frame_header),
        thumbnail->request.width,thumbnail->request.height,(size_t)thumbnail->request.width*4u,NX_BITMAP_RGBA8};
}
