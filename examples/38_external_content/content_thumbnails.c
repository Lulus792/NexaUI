#include "content_thumbnails.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void content_thumbnails_clear(content_thumbnails *cache) {
    for (unsigned i=0;i<CONTENT_THUMBNAIL_COUNT;++i) free(cache->cells[i].frame);
    memset(cache->cells,0,sizeof(cache->cells));
}

void content_thumbnails_configure(content_thumbnails *cache, uint64_t generation, double density) {
    density=fmin(4.0,fmax(1.0,density));
    uint32_t width=(uint32_t)ceil(CONTENT_THUMBNAIL_WIDTH*density);
    uint32_t height=(uint32_t)ceil(CONTENT_THUMBNAIL_HEIGHT*density);
    content_request *request=&cache->request;
    if (request->generation==generation && request->width==width && request->height==height) return;
    content_thumbnails_clear(cache);
    *request=(content_request){generation,request->resize+1u,width,height};
}

void content_thumbnails_set_group(content_thumbnails *cache, unsigned page) {
    unsigned first=page-(page-1u)%CONTENT_THUMBNAIL_COUNT;
    if (cache->first==first) return;
    content_thumbnails_clear(cache);
    cache->first=first;
}

void content_thumbnails_open(content_thumbnails *cache, unsigned page) {
    content_thumbnails_set_group(cache,page);
    ++cache->session;
    cache->open=1;
}

unsigned content_thumbnails_missing(const content_thumbnails *cache, unsigned total) {
    if (!cache->open) return 0;
    for (unsigned i=0;i<CONTENT_THUMBNAIL_COUNT && cache->first+i<=total;++i)
        if (!cache->cells[i].page) return cache->first+i;
    return 0;
}

int content_thumbnails_adopt(content_thumbnails *cache, content_thumbnail completed) {
    content_request key=cache->request, got=completed.request;
    if (!cache->first || completed.page<cache->first || completed.page-cache->first>=CONTENT_THUMBNAIL_COUNT ||
        got.generation!=key.generation || got.resize!=key.resize || got.width!=key.width || got.height!=key.height) {
        free(completed.frame);
        return 0;
    }
    size_t bytes=sizeof(content_frame_header)+(size_t)key.width*key.height*4u;
    if (completed.bytes!=bytes) { free(completed.frame);completed.frame=NULL; }
    content_thumbnail *cell=&cache->cells[completed.page-cache->first];
    free(cell->frame);
    *cell=completed;
    return 1;
}

nx_bitmap_view content_thumbnail_pixels(const content_thumbnail *thumbnail) {
    return (nx_bitmap_view){(const uint8_t *)thumbnail->frame+sizeof(content_frame_header),
        thumbnail->request.width,thumbnail->request.height,(size_t)thumbnail->request.width*4u,NX_BITMAP_RGBA8};
}
