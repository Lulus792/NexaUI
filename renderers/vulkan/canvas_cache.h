#ifndef NEXAUI_VULKAN_CANVAS_CACHE_H
#define NEXAUI_VULKAN_CANVAS_CACHE_H
#include "nexaui/nexaui.h"
#include <string.h>

/* Optional CPU proof of the queue-owned canvas content. Canonical metadata
   excludes padding/unused fields/borrowed addresses; image rows are owned bytes.
   No hash collisions or reads through expired image pointers. Preflight first. */
#define NX_VK_CANVAS_CACHE_BYTES (32u * 1024u * 1024u)
typedef struct nx_vk_canvas_cache {
    unsigned char *data;
    size_t capacity, length, count;
    double scale;
    nx_color clear;
    int valid, variant;
} nx_vk_canvas_cache;

static void nx_vk_canvas_cache_key(nx_command *k, const nx_command *c) {
    memset(k, 0, sizeof(*k));
    k->type = c->type;
    if (c->type == NX_COMMAND_POP_CLIP) return;
    if (c->type == NX_COMMAND_LINE || c->type == NX_COMMAND_FILL_TRIANGLE) {
        unsigned count = c->type == NX_COMMAND_LINE ? 2u : 3u;
        for (unsigned i = 0; i < count; ++i) {
            k->points[i].x = c->points[i].x; k->points[i].y = c->points[i].y;
        }
    } else {
        k->rect.x = c->rect.x; k->rect.y = c->rect.y;
        k->rect.width = c->rect.width; k->rect.height = c->rect.height;
    }
    if (c->type == NX_COMMAND_PUSH_CLIP) return;
    k->color.r = c->color.r; k->color.g = c->color.g;
    k->color.b = c->color.b; k->color.a = c->color.a;
    if (c->type == NX_COMMAND_LINE || c->type == NX_COMMAND_STROKE_RECT)
        k->thickness = c->thickness;
    if (c->type == NX_COMMAND_FILL_ROUND_RECT) k->radius = c->radius;
    if (c->type == NX_COMMAND_BITMAP) {
        k->bitmap.width = c->bitmap.width; k->bitmap.height = c->bitmap.height;
        k->bitmap.stride_bytes = c->bitmap.stride_bytes; k->bitmap.format = c->bitmap.format;
    }
}

static void nx_vk_canvas_cache_drop(nx_vk_canvas_cache *c, nx_allocator a) {
    if (c->data) a.deallocate(a.user, c->data, c->capacity);
    memset(c, 0, sizeof(*c));
}
static size_t nx_vk_canvas_cache_size(const nx_command *commands, size_t count) {
    if (!count || count > NX_VK_CANVAS_CACHE_BYTES / sizeof(*commands)) return 0;
    size_t bytes = count * sizeof(*commands);
    for (size_t i = 0; i < count; ++i) {
        if (commands[i].type < NX_COMMAND_FILL_RECT || commands[i].type > NX_COMMAND_BITMAP) return 0;
        if (commands[i].type != NX_COMMAND_BITMAP) continue;
        const nx_bitmap_view *b = &commands[i].bitmap;
        if (b->format != NX_BITMAP_RGBA8 && b->format != NX_BITMAP_A8) return 0;
        size_t pixel = b->format == NX_BITMAP_RGBA8 ? 4u : 1u;
        if (b->width > NX_VK_CANVAS_CACHE_BYTES / pixel) return 0;
        size_t row = (size_t)b->width * pixel;
        if (row > NX_VK_CANVAS_CACHE_BYTES ||
            b->height > (NX_VK_CANVAS_CACHE_BYTES - bytes) / (row ? row : 1u)) return 0;
        bytes += row * b->height;
    }
    return bytes;
}
static int nx_vk_canvas_cache_match(const nx_vk_canvas_cache *c,
    const nx_command *commands, size_t count, double scale, nx_color clear, int variant) {
    if (!c->valid || c->count != count || c->variant != variant ||
        memcmp(&c->scale, &scale, sizeof(scale)) ||
        memcmp(&c->clear, &clear, sizeof(clear))) return 0;
    size_t bytes = nx_vk_canvas_cache_size(commands, count);
    if (!bytes || bytes != c->length) return 0;
    size_t at = count * sizeof(*commands);
    for (size_t i = 0; i < count; ++i) {
        nx_command key;
        nx_vk_canvas_cache_key(&key, &commands[i]);
        if (memcmp(c->data + i * sizeof(key), &key, sizeof(key))) return 0;
    }
    for (size_t i = 0; i < count; ++i) if (commands[i].type == NX_COMMAND_BITMAP) {
        const nx_bitmap_view *b = &commands[i].bitmap;
        size_t row = (size_t)b->width * (b->format == NX_BITMAP_RGBA8 ? 4u : 1u);
        for (uint32_t y = 0; y < b->height; ++y) {
            if (memcmp(c->data + at, b->pixels + (size_t)y * b->stride_bytes, row)) return 0;
            at += row;
        }
    }
    return 1;
}
static int nx_vk_canvas_cache_capture(nx_vk_canvas_cache *c, nx_allocator a,
    const nx_command *commands, size_t count, double scale, nx_color clear, int variant) {
    c->valid = 0;
    size_t bytes = nx_vk_canvas_cache_size(commands, count);
    if (!bytes) { nx_vk_canvas_cache_drop(c, a); return 0; }
    if (c->capacity < bytes) {
        /* The old proof is already invalid: release before allocating so even
           replacement stays within the32MiB retained/peak CPU limit. */
        nx_vk_canvas_cache_drop(c, a);
        c->data = a.allocate(a.user, bytes);
        if (!c->data) return 0; /* Optional optimization: render normally. */
        c->capacity = bytes;
    }
    size_t at = count * sizeof(*commands);
    for (size_t i = 0; i < count; ++i) {
        nx_command key;
        nx_vk_canvas_cache_key(&key, &commands[i]);
        memcpy(c->data + i * sizeof(key), &key, sizeof(key));
    }
    for (size_t i = 0; i < count; ++i) if (commands[i].type == NX_COMMAND_BITMAP) {
        const nx_bitmap_view *b = &commands[i].bitmap;
        size_t row = (size_t)b->width * (b->format == NX_BITMAP_RGBA8 ? 4u : 1u);
        for (uint32_t y = 0; y < b->height; ++y) {
            memcpy(c->data + at, b->pixels + (size_t)y * b->stride_bytes, row);
            at += row;
        }
    }
    c->length = bytes; c->count = count; c->scale = scale;
    c->clear = clear; c->variant = variant;
    return 1; /* Caller marks valid only for a successfully submitted canvas. */
}
#endif
