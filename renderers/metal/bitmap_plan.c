#include "bitmap_plan.h"
#include <math.h>
#include <string.h>

uint32_t nx_mtl_bitmap_find(const nx_mtl_bitmap_plan *p, nx_bitmap_view view) {
    for (uint32_t i = 0; i < p->count; ++i) {
        nx_bitmap_view a = p->entries[i].view;
        if (a.pixels == view.pixels && a.width == view.width && a.height == view.height &&
            a.stride_bytes == view.stride_bytes && a.format == view.format) return i;
    }
    return p->count;
}
static uint32_t grow(uint32_t need, uint32_t previous) {
    uint32_t size = previous ? previous : 1;
    while (size < need) size *= 2;
    return size;
}
nx_result nx_mtl_bitmaps_build(nx_mtl_bitmap_plan *p, const nx_command *commands,
    size_t count, uint32_t previous_width, uint32_t previous_height) {
    if (!p) return NX_ERROR_ARGUMENT;
    p->count = p->width = p->height = 0;
    if (previous_width > NX_MTL_ATLAS_AXIS || previous_height > NX_MTL_ATLAS_AXIS ||
        (previous_width && (previous_width & (previous_width-1))) ||
        (previous_height && (previous_height & (previous_height-1)))) return NX_ERROR_ARGUMENT;
    nx_result result = nx_render_validate_commands(commands,count);
    if (result != NX_OK) return result;
    uint32_t width = previous_width > 1024u ? previous_width : 1024u;
    for (size_t i = 0; i < count; ++i) {
        const nx_command *c = &commands[i];
        if (c->type != NX_COMMAND_BITMAP || !c->rect.width || !c->rect.height || floorf(c->color.a*255.f+.5f) == 0) continue;
        nx_bitmap_view v = c->bitmap;
        if (v.width > NX_MTL_ATLAS_AXIS || v.height > NX_MTL_ATLAS_AXIS ||
            (uint64_t)v.width*v.height > NX_MTL_ATLAS_BYTES/4u) return NX_ERROR_LIMIT;
        if (nx_mtl_bitmap_find(p,v) < p->count) continue;
        if (p->count == NX_MTL_BITMAP_LIMIT) return NX_ERROR_LIMIT;
        p->entries[p->count++] = (nx_mtl_bitmap_entry){v,0,0};
        if (v.width > width) width = grow(v.width,width);
    }
    if (!p->count) return NX_OK;
    uint32_t height;
    for (;;) {
        uint32_t x = 0, y = 0, row = 0;
        for (uint32_t i = 0; i < p->count; ++i) {
            nx_mtl_bitmap_entry *e = &p->entries[i];
            if (e->view.width > width-x) { x = 0; y += row; row = 0; }
            e->x = x; e->y = y; x += e->view.width;
            if (e->view.height > row) row = e->view.height;
        }
        height = y+row; /* At most 4096*8192, bounded well within uint32_t. */
        if (height <= NX_MTL_ATLAS_AXIS) break;
        if (width == NX_MTL_ATLAS_AXIS) return NX_ERROR_LIMIT;
        width *= 2;
    }
    height = grow(height,previous_height);
    if ((uint64_t)width*height > NX_MTL_ATLAS_BYTES/4u) return NX_ERROR_LIMIT;
    p->width = width; p->height = height; return NX_OK;
}
nx_result nx_mtl_bitmaps_copy(const nx_mtl_bitmap_plan *p, uint8_t *pixels,
    size_t capacity, size_t stride) {
    if (!p || p->count > NX_MTL_BITMAP_LIMIT) return NX_ERROR_ARGUMENT;
    if (!p->count) return NX_OK;
    if (!pixels || !p->width || !p->height || p->width > NX_MTL_ATLAS_AXIS || p->height > NX_MTL_ATLAS_AXIS ||
        stride < (size_t)p->width*4u || stride > SIZE_MAX/p->height || capacity < stride*p->height)
        return NX_ERROR_ARGUMENT;
    /* Padding stays untouched, both for callers and GPU stride diagnostics. */
    for (uint32_t y = 0; y < p->height; ++y) memset(pixels+(size_t)y*stride,0,(size_t)p->width*4u);
    for (uint32_t i = 0; i < p->count; ++i) {
        const nx_mtl_bitmap_entry *e = &p->entries[i];
        for (uint32_t y = 0; y < e->view.height; ++y) {
            uint8_t *dst = pixels+(size_t)(e->y+y)*stride+(size_t)e->x*4u;
            const uint8_t *src = e->view.pixels+(size_t)y*e->view.stride_bytes;
            if (e->view.format == NX_BITMAP_RGBA8) memcpy(dst,src,(size_t)e->view.width*4u);
            else for (uint32_t x = 0; x < e->view.width; ++x) {
                dst[x*4u] = dst[x*4u+1] = dst[x*4u+2] = 255;
                dst[x*4u+3] = src[x];
            }
        }
    }
    return NX_OK;
}
