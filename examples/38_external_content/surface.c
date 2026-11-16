#include "surface.h"
#include <stdlib.h>
#include <string.h>

struct content_surface {
    nx_snapshot_exchange *exchange;
    nx_viewport_surface *viewport;
    nx_allocator allocator;
    content_surface_status status;
    size_t max_bytes;
    uint8_t *pixels;
};

static void *allocate(void *user, size_t bytes) { (void)user; return malloc(bytes); }
static void deallocate(void *user, void *data, size_t bytes) {
    (void)user; (void)bytes; free(data);
}

static nx_result resize_source(void *user, uint32_t width, uint32_t height) {
    content_surface *surface = user;
    if (surface->status.request.resize == UINT64_MAX) return NX_ERROR_LIMIT;
    if (width > NX_VIEWPORT_MAX_DIMENSION || height > NX_VIEWPORT_MAX_DIMENSION)
        return NX_ERROR_LIMIT;
    size_t row = (size_t)width * 4u;
    if (height && row > surface->max_bytes / height) return NX_ERROR_LIMIT;
    size_t bytes = row * height;
    uint8_t *replacement = bytes ? surface->allocator.allocate(surface->allocator.user, bytes) : NULL;
    if (bytes && !replacement) return NX_ERROR_MEMORY;
    if (surface->pixels)
        surface->allocator.deallocate(surface->allocator.user, surface->pixels, surface->status.staging_bytes);
    surface->pixels = replacement;
    surface->status.staging_bytes = bytes;
    surface->status.sequence = 0;
    surface->status.last_copy_bytes = 0;
    ++surface->status.request.resize;
    surface->status.request.width = width;
    surface->status.request.height = height;
    return NX_OK;
}

static nx_result stage(content_surface *surface, const nx_snapshot_view *view) {
    if (view->bytes < sizeof(content_frame_header)) return NX_ERROR_ARGUMENT;
    content_frame_header header;
    memcpy(&header, view->data, sizeof(header));
    const content_request *request = &surface->status.request;
    if (header.request.generation != request->generation || header.request.resize != request->resize)
        return NX_ERROR_STATE;
    if (header.request.width != request->width || header.request.height != request->height ||
        !header.sequence || header.stride < (size_t)request->width * 4u) return NX_ERROR_ARGUMENT;
    if (header.format != NX_BITMAP_RGBA8) return NX_ERROR_UNSUPPORTED;
    if (request->height > SIZE_MAX / header.stride) return NX_ERROR_LIMIT;
    size_t payload = (size_t)request->height * header.stride;
    if (payload > surface->max_bytes) return NX_ERROR_LIMIT;
    if (view->bytes - sizeof(header) != payload) return NX_ERROR_ARGUMENT;
    if (header.damage_base) {
        if (header.damage_base >= header.sequence ||
            !header.damage_width || !header.damage_height ||
            header.damage_x >= request->width || header.damage_y >= request->height ||
            header.damage_width > request->width - header.damage_x ||
            header.damage_height > request->height - header.damage_y)
            return NX_ERROR_ARGUMENT;
    } else if (header.damage_x || header.damage_y || header.damage_width || header.damage_height) {
        return NX_ERROR_ARGUMENT;
    }
    if (header.sequence < surface->status.sequence) return NX_ERROR_STATE;
    /* A sequence denotes one immutable frame. Repeated observations reuse
       its staged pixels; a producer must increase it for changed content. */
    if (header.sequence == surface->status.sequence) return NX_OK;
    if (surface->status.copied_frames == UINT64_MAX) return NX_ERROR_LIMIT;
    const uint8_t *pixels = (const uint8_t *)view->data + sizeof(header);
    size_t row = (size_t)request->width * 4u;
    /* Publication can replace an unseen snapshot. A mismatched base requires
       the complete current image even when its rectangle is valid. */
    int partial = header.damage_base && header.damage_base == surface->status.sequence;
    uint32_t first_y = partial ? header.damage_y : 0;
    uint32_t rows = partial ? header.damage_height : request->height;
    size_t offset = partial ? (size_t)header.damage_x * 4u : 0;
    size_t copy_bytes = partial ? (size_t)header.damage_width * 4u : row;
    for (uint32_t y = first_y; y < first_y + rows; ++y)
        memcpy(surface->pixels + (size_t)y * row + offset,
               pixels + (size_t)y * header.stride + offset, copy_bytes);
    surface->status.sequence = header.sequence;
    surface->status.last_copy_bytes = copy_bytes * rows;
    ++surface->status.copied_frames;
    return NX_OK;
}

static nx_result acquire_source(void *user, nx_bitmap_view *out) {
    content_surface *surface = user;
    nx_snapshot_view view;
    nx_result result = nx_snapshot_exchange_acquire(surface->exchange, &view);
    if (result != NX_OK) return result;
    if (!view.data) return NX_ERROR_STATE;
    result = stage(surface, &view);
    /* Snapshot borrow ends inside the callback, before UI end_frame. Only
       our bounded staging buffer is leased to the later renderer. */
    nx_result released = nx_snapshot_exchange_release(surface->exchange, &view);
    if (released != NX_OK) return released;
    if (result == NX_OK)
        *out = (nx_bitmap_view){surface->pixels, surface->status.request.width,
            surface->status.request.height, (size_t)surface->status.request.width * 4u, NX_BITMAP_RGBA8};
    return result;
}

static void release_source(void *user) { (void)user; }

nx_result content_surface_create(nx_snapshot_exchange *exchange, uint64_t generation,
    size_t max_bytes, const nx_allocator *allocator, content_surface **out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out = NULL;
    if (!exchange || !generation || !max_bytes || max_bytes > CONTENT_FRAME_LIMIT ||
        (allocator && (!allocator->allocate || !allocator->deallocate))) return NX_ERROR_ARGUMENT;
    nx_allocator selected = allocator ? *allocator : (nx_allocator){NULL, allocate, deallocate};
    content_surface *surface = selected.allocate(selected.user, sizeof(*surface));
    if (!surface) return NX_ERROR_MEMORY;
    memset(surface, 0, sizeof(*surface));
    surface->allocator = selected;
    surface->exchange = exchange;
    surface->max_bytes = max_bytes;
    surface->status.request.generation = generation;
    nx_viewport_source source = {surface, resize_source, acquire_source, release_source};
    nx_result result = nx_viewport_surface_create(&source, &selected, &surface->viewport);
    if (result != NX_OK) { selected.deallocate(selected.user, surface, sizeof(*surface)); return result; }
    *out = surface;
    return NX_OK;
}

nx_result content_surface_destroy(content_surface **out) {
    if (!out) return NX_ERROR_ARGUMENT;
    if (!*out) return NX_OK;
    content_surface *surface = *out;
    nx_result result = nx_viewport_surface_destroy(surface->viewport);
    if (result != NX_OK) return result;
    nx_allocator allocator = surface->allocator;
    if (surface->pixels) allocator.deallocate(allocator.user, surface->pixels, surface->status.staging_bytes);
    allocator.deallocate(allocator.user, surface, sizeof(*surface));
    *out = NULL;
    return NX_OK;
}

nx_result content_surface_draw(content_surface *surface, nx_context *context,
    const nx_viewport_desc *desc, nx_interaction_state *out) {
    if (!surface) return NX_ERROR_ARGUMENT;
    return nx_viewport(context, surface->viewport, desc, out);
}

nx_result content_surface_complete(content_surface *surface) {
    return surface ? nx_viewport_surface_complete(surface->viewport) : NX_ERROR_ARGUMENT;
}

nx_result content_surface_get_status(const content_surface *surface, content_surface_status *out) {
    if (!surface || !out) return NX_ERROR_ARGUMENT;
    *out = surface->status;
    return NX_OK;
}
