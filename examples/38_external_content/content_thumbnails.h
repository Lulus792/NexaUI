#ifndef CONTENT_THUMBNAILS_H
#define CONTENT_THUMBNAILS_H

#include "surface.h"

#define CONTENT_THUMBNAIL_COUNT 3u
#define CONTENT_THUMBNAIL_WIDTH 104u
#define CONTENT_THUMBNAIL_HEIGHT 80u

typedef struct content_thumbnail {
    content_request request;
    unsigned page;
    void *frame;
    size_t bytes;
} content_thumbnail;

typedef struct content_thumbnails {
    content_request request;
    unsigned first;
    uint64_t session;
    int open;
    content_thumbnail cells[CONTENT_THUMBNAIL_COUNT];
} content_thumbnails;

/* Each cached frame is owned by the UI thread. Call only when the previous
   synchronous software render has returned. */
void content_thumbnails_clear(content_thumbnails *cache);
void content_thumbnails_configure(content_thumbnails *cache, uint64_t generation, double density);
void content_thumbnails_set_group(content_thumbnails *cache, unsigned page);
void content_thumbnails_open(content_thumbnails *cache, unsigned page);
unsigned content_thumbnails_missing(const content_thumbnails *cache, unsigned total);
/* Transfers or frees the supplied frame, including stale completions. */
int content_thumbnails_adopt(content_thumbnails *cache, content_thumbnail completed);
nx_bitmap_view content_thumbnail_pixels(const content_thumbnail *thumbnail);

#endif
