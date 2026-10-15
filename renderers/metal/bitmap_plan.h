#ifndef NEXAUI_METAL_BITMAP_PLAN_H
#define NEXAUI_METAL_BITMAP_PLAN_H
#include "renderer_backend.h"

#define NX_MTL_BITMAP_LIMIT 4096u
#define NX_MTL_ATLAS_AXIS 8192u
#define NX_MTL_ATLAS_BYTES (64u * 1024u * 1024u)
typedef struct nx_mtl_bitmap_entry { nx_bitmap_view view; uint32_t x, y; } nx_mtl_bitmap_entry;
typedef struct nx_mtl_bitmap_plan {
    nx_mtl_bitmap_entry entries[NX_MTL_BITMAP_LIMIT];
    uint32_t count, width, height;
} nx_mtl_bitmap_plan;

/* Fixed bounded metadata, no per-frame allocation. Source views are borrowed
   only for build/copy/encoding; no GPU work may read the prior upload storage. */
nx_result nx_mtl_bitmaps_build(nx_mtl_bitmap_plan *plan, const nx_command *commands,
    size_t count, uint32_t previous_width, uint32_t previous_height);
uint32_t nx_mtl_bitmap_find(const nx_mtl_bitmap_plan *plan, nx_bitmap_view view);
/* Copy only a successfully built, unmodified plan with valid borrowed pixels.
   Capacity covers stride*height; row padding remains untouched. */
nx_result nx_mtl_bitmaps_copy(const nx_mtl_bitmap_plan *plan, uint8_t *pixels,
    size_t capacity, size_t stride);
#endif
