#include "surface.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct content_surface {
    nx_snapshot_exchange *exchange;
    nx_viewport_surface *viewport;
    nx_allocator allocator;
    content_surface_status status;
    size_t max_bytes;
    uint8_t *pixels;
    uint8_t *metadata;
    size_t text_bytes, word_count, link_count;
    nx_command *mapped;
    size_t mapped_capacity;
    int leased;
};

static void *allocate(void *user, size_t bytes) { (void)user; return malloc(bytes); }
static void deallocate(void *user, void *data, size_t bytes) {
    (void)user; (void)bytes; free(data);
}
static void clear_metadata(content_surface *surface) {
    if (surface->metadata)
        surface->allocator.deallocate(surface->allocator.user, surface->metadata,
            surface->text_bytes + surface->word_count * sizeof(content_frame_word) +
            surface->link_count * sizeof(content_frame_link));
    surface->metadata = NULL;
    surface->text_bytes = 0;
    surface->word_count = 0;
    surface->link_count = 0;
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
    clear_metadata(surface);
    surface->status.staging_bytes = bytes;
    surface->status.sequence = 0;
    surface->status.last_copy_bytes = 0;
    surface->status.image = (content_image_area){0};
    ++surface->status.request.resize;
    surface->status.request.width = width;
    surface->status.request.height = height;
    return NX_OK;
}

static int unit_bounds(nx_rect box) {
    return box.x >= 0 && box.y >= 0 && box.width > 0 && box.height > 0 &&
        box.x + box.width <= 1 && box.y + box.height <= 1;
}

static int valid_words(const char *text, size_t text_bytes, const uint8_t *words, size_t count) {
    uint32_t previous_end = 0;
    for (size_t i = 0; i < count; ++i) {
        content_frame_word word;
        memcpy(&word, words + i * sizeof(word), sizeof(word));
        if (word.start_byte < previous_end || word.start_byte >= word.end_byte || word.end_byte > text_bytes ||
            ((uint8_t)text[word.start_byte] & 0xc0u) == 0x80u ||
            (word.end_byte < text_bytes && ((uint8_t)text[word.end_byte] & 0xc0u) == 0x80u) ||
            !unit_bounds(word.bounds))
            return 0;
        previous_end = word.end_byte;
    }
    return 1;
}

static int valid_links(const uint8_t *links, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        content_frame_link link;
        memcpy(&link,links + i * sizeof(link),sizeof(link));
        if (!unit_bounds(link.bounds) ||
            (link.target_page ? link.uri[0]!=0 : !content_pdf_web_uri(link.uri))) return 0;
    }
    return 1;
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
    content_image_area image=header.image;
    if (image.width || image.height || image.x || image.y || image.source_width || image.source_height) {
        if (!image.width || !image.height || !image.source_width || !image.source_height ||
            image.x>=request->width || image.y>=request->height ||
            image.width>request->width-image.x || image.height>request->height-image.y) return NX_ERROR_ARGUMENT;
    }
    if (header.format != NX_BITMAP_RGBA8) return NX_ERROR_UNSUPPORTED;
    if (request->height > SIZE_MAX / header.stride) return NX_ERROR_LIMIT;
    size_t payload = (size_t)request->height * header.stride;
    if (payload > surface->max_bytes) return NX_ERROR_LIMIT;
    size_t body = view->bytes - sizeof(header);
    if (header.text_bytes > CONTENT_FRAME_TEXT_LIMIT || header.word_count > CONTENT_FRAME_WORD_LIMIT ||
        header.link_count > CONTENT_PDF_LINK_LIMIT)
        return NX_ERROR_LIMIT;
    size_t word_bytes = (size_t)header.word_count * sizeof(content_frame_word);
    size_t link_bytes = (size_t)header.link_count * sizeof(content_frame_link);
    size_t metadata_bytes = word_bytes + link_bytes + header.text_bytes;
    if (body < payload || body - payload != metadata_bytes) return NX_ERROR_ARGUMENT;
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
    const char *text = (const char *)(pixels + payload);
    const uint8_t *words = (const uint8_t *)text + header.text_bytes;
    const uint8_t *links = words + word_bytes;
    uint8_t *metadata = NULL;
    if (metadata_bytes) {
        if (memchr(text, 0, header.text_bytes) || nx_utf8_validate(text, header.text_bytes, NULL) != NX_OK)
            return NX_ERROR_ARGUMENT;
        if (!valid_words(text, header.text_bytes, words, header.word_count)) return NX_ERROR_ARGUMENT;
        if (!valid_links(links,header.link_count)) return NX_ERROR_ARGUMENT;
        metadata = surface->allocator.allocate(surface->allocator.user, metadata_bytes);
        if (!metadata) return NX_ERROR_MEMORY;
        memcpy(metadata, words, word_bytes);
        memcpy(metadata + word_bytes, links, link_bytes);
        memcpy(metadata + word_bytes + link_bytes, text, header.text_bytes);
    }
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
    clear_metadata(surface);
    surface->metadata = metadata;
    surface->text_bytes = header.text_bytes;
    surface->word_count = header.word_count;
    surface->link_count = header.link_count;
    surface->status.sequence = header.sequence;
    surface->status.image = image;
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
    if (surface->mapped) allocator.deallocate(allocator.user,surface->mapped,
        surface->mapped_capacity*sizeof(*surface->mapped));
    clear_metadata(surface);
    allocator.deallocate(allocator.user, surface, sizeof(*surface));
    *out = NULL;
    return NX_OK;
}

nx_result content_surface_draw(content_surface *surface, nx_context *context,
    const nx_viewport_desc *desc, nx_interaction_state *out) {
    if (!surface) return NX_ERROR_ARGUMENT;
    nx_result result=nx_viewport(context,surface->viewport,desc,out);
    if (result==NX_OK) surface->leased=desc->bounds.width>0&&desc->bounds.height>0;
    return result;
}

nx_result content_surface_complete(content_surface *surface) {
    if (!surface) return NX_ERROR_ARGUMENT;
    nx_result result=nx_viewport_surface_complete(surface->viewport);
    if (result==NX_OK) surface->leased=0;
    return result;
}

nx_result content_surface_map_commands(content_surface *surface,
    const nx_command *commands, size_t count, nx_rect image,
    const nx_bitmap_view *replacement, const nx_command **out) {
    if (!surface || !commands || !out || count>SIZE_MAX/sizeof(nx_command) ||
        !isfinite(image.x) || !isfinite(image.y) || !isfinite(image.width) || !isfinite(image.height) ||
        !(image.width>0 && image.height>0)) return NX_ERROR_ARGUMENT;
    if (replacement && (!replacement->pixels || replacement->format!=NX_BITMAP_RGBA8 ||
        !replacement->width || !replacement->height || replacement->width>NX_VIEWPORT_MAX_DIMENSION ||
        replacement->height>NX_VIEWPORT_MAX_DIMENSION || replacement->stride_bytes<(size_t)replacement->width*4u ||
        replacement->height>SIZE_MAX/replacement->stride_bytes)) return NX_ERROR_ARGUMENT;
    if (!surface->leased) return NX_ERROR_STATE;
    if (commands==surface->mapped) return NX_ERROR_ARGUMENT;
    size_t target=count;
    for (size_t i=0;i<count;++i) {
        if (commands[i].type!=NX_COMMAND_BITMAP || commands[i].bitmap.pixels!=surface->pixels) continue;
        if (target!=count) return NX_ERROR_ARGUMENT;
        target=i;
    }
    if (target==count) return NX_ERROR_ARGUMENT;
    if (surface->mapped_capacity<count) {
        nx_allocator allocator=surface->allocator;
        nx_command *replacement=allocator.allocate(allocator.user,count*sizeof(*replacement));
        if (!replacement) return NX_ERROR_MEMORY;
        if (surface->mapped) allocator.deallocate(allocator.user,surface->mapped,
            surface->mapped_capacity*sizeof(*surface->mapped));
        surface->mapped=replacement;
        surface->mapped_capacity=count;
    }
    memcpy(surface->mapped,commands,count*sizeof(*commands));
    surface->mapped[target].rect=image;
    if (replacement) surface->mapped[target].bitmap=*replacement;
    *out=surface->mapped;
    return NX_OK;
}

nx_result content_surface_get_status(const content_surface *surface, content_surface_status *out) {
    if (!surface || !out) return NX_ERROR_ARGUMENT;
    *out = surface->status;
    return NX_OK;
}

nx_result content_surface_get_text(const content_surface *surface, nx_utf8_span *out) {
    if (!surface || !out) return NX_ERROR_ARGUMENT;
    const char *text = surface->metadata ?
        (const char *)(surface->metadata + surface->word_count * sizeof(content_frame_word) +
            surface->link_count * sizeof(content_frame_link)) : NULL;
    *out = (nx_utf8_span){text, surface->text_bytes};
    return NX_OK;
}

nx_result content_surface_get_links(const content_surface *surface,
    const content_frame_link **links, size_t *count) {
    if (!surface || !links || !count) return NX_ERROR_ARGUMENT;
    *count = surface->link_count;
    *links = surface->link_count ? (const content_frame_link *)(surface->metadata +
        surface->word_count * sizeof(content_frame_word)) : NULL;
    return NX_OK;
}

nx_result content_surface_get_words(const content_surface *surface,
    const content_frame_word **words, size_t *count) {
    if (!surface || !words || !count) return NX_ERROR_ARGUMENT;
    *count = surface->word_count;
    *words = surface->word_count ? (const content_frame_word *)surface->metadata : NULL;
    return NX_OK;
}
