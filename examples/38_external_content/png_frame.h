#ifndef EXAMPLE_CONTENT_PNG_FRAME_H
#define EXAMPLE_CONTENT_PNG_FRAME_H

#include "surface.h"

#define CONTENT_PNG_ENCODED_LIMIT (4u * 1024u * 1024u)
#define CONTENT_PNG_PIXEL_LIMIT (4u * 1024u * 1024u)
#define CONTENT_PNG_SOURCE_DIMENSION 2048u

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

/* Also return the actual fitted image rectangle in physical frame pixels. */
nx_result content_png_frame_fit(const void *encoded, size_t encoded_size, content_request request,
    uint64_t sequence, void **out, size_t *out_size, content_png_fit *fit);

#endif
