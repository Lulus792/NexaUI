#ifndef EXAMPLE_CONTENT_PNG_FRAME_H
#define EXAMPLE_CONTENT_PNG_FRAME_H

#include "surface.h"

#define CONTENT_PNG_ENCODED_LIMIT (4u * 1024u * 1024u)
#define CONTENT_PNG_PIXEL_LIMIT (4u * 1024u * 1024u)
#define CONTENT_PNG_SOURCE_DIMENSION 2048u

/* Return an owned tightly packed RGBA8 raster. Free out->pixels on success. */
nx_result content_png_bitmap(const void *encoded, size_t encoded_size, nx_bitmap_view *out);

/* Decode a borrowed PNG into one owned content frame. Scale to fit the
   physical viewport with nearest-neighbor sampling and transparent margins.
   The decoded source and output pixels each have a 4 MiB staging limit.
   Assign both outputs only on success. Free the result or transfer it to
   nx_snapshot_exchange with a free-compatible disposer. */
nx_result content_png_frame(const void *encoded, size_t encoded_size, content_request request,
    uint64_t sequence, void **out, size_t *out_size);

typedef struct content_png_fit {
    uint32_t source_width, source_height;
    uint32_t x, y, width, height;
} content_png_fit;

/* Fit a borrowed packed RGBA8 decode. The caller retains the original. */
nx_result content_png_fit_bitmap(nx_bitmap_view bitmap,content_request request,
    uint64_t sequence,void **out,size_t *out_size,content_png_fit *fit);

/* Also return the actual fitted image rectangle in physical frame pixels. */
nx_result content_png_frame_fit(const void *encoded, size_t encoded_size, content_request request,
    uint64_t sequence, void **out, size_t *out_size, content_png_fit *fit);

#endif
