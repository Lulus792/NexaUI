#ifndef CONTENT_PDF_THUMBNAILS_H
#define CONTENT_PDF_THUMBNAILS_H

#include "surface.h"

#define PDF_THUMBNAIL_COUNT 3u
#define PDF_THUMBNAIL_WIDTH 104u
#define PDF_THUMBNAIL_HEIGHT 80u

typedef struct pdf_thumbnail {
    content_request request;
    unsigned page;
    void *frame;
    size_t bytes;
} pdf_thumbnail;

typedef struct pdf_thumbnails {
    content_request request;
    unsigned first;
    uint64_t session;
    int open;
    pdf_thumbnail cells[PDF_THUMBNAIL_COUNT];
} pdf_thumbnails;

/* Each cached frame is owned by the UI thread. Call only when the previous
   synchronous software render has returned. */
void pdf_thumbnails_clear(pdf_thumbnails *cache);
void pdf_thumbnails_configure(pdf_thumbnails *cache, uint64_t generation, double density);
void pdf_thumbnails_set_group(pdf_thumbnails *cache, unsigned page);
void pdf_thumbnails_open(pdf_thumbnails *cache, unsigned page);
unsigned pdf_thumbnails_missing(const pdf_thumbnails *cache, unsigned total);
/* Transfers or frees the supplied frame, including stale completions. */
int pdf_thumbnails_adopt(pdf_thumbnails *cache, pdf_thumbnail completed);
nx_bitmap_view pdf_thumbnail_pixels(const pdf_thumbnail *thumbnail);

#endif
