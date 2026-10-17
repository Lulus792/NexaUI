#ifndef NEXAUI_INTERNAL_H
#define NEXAUI_INTERNAL_H

#include "nexaui/nexaui.h"
#include <math.h>

/* Physical-coordinate recording for stamp construction; keeps the original
   shaped layout and line breaks rather than creating a scaled replacement. */
nx_result nx_draw_text_layout_pixels(nx_context *context,
    const nx_text_layout *layout, float raster_scale, size_t cache_bytes);

static inline int nx_valid_rect(nx_rect r) {
    return isfinite(r.x) && isfinite(r.y) && isfinite(r.width) &&
           isfinite(r.height) && r.width >= 0.0f && r.height >= 0.0f &&
           isfinite((double)r.x + (double)r.width) &&
           isfinite((double)r.y + (double)r.height);
}

static inline int nx_valid_color(nx_color c) {
    return isfinite(c.r) && isfinite(c.g) && isfinite(c.b) && isfinite(c.a) &&
           c.r >= 0.0f && c.r <= 1.0f && c.g >= 0.0f && c.g <= 1.0f &&
           c.b >= 0.0f && c.b <= 1.0f && c.a >= 0.0f && c.a <= 1.0f;
}

static inline int nx_valid_bitmap_view(nx_bitmap_view bitmap) {
    if (!bitmap.pixels || !bitmap.width || !bitmap.height ||
        (bitmap.format != NX_BITMAP_RGBA8 && bitmap.format != NX_BITMAP_A8))
        return 0;
    size_t channels = bitmap.format == NX_BITMAP_RGBA8 ? 4u : 1u;
    if (bitmap.width > SIZE_MAX / channels) return 0;
    size_t row_bytes = (size_t)bitmap.width * channels;
    return bitmap.stride_bytes >= row_bytes &&
           (size_t)(bitmap.height - 1u) <= (SIZE_MAX - row_bytes) / bitmap.stride_bytes;
}

#endif
