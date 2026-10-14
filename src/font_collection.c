#include "nexaui/nexaui.h"

#include <stdlib.h>
#include <string.h>

struct nx_font_collection {
    nx_allocator allocator;
    size_t count;
    const nx_font *fonts[NX_FONT_COLLECTION_MAX_FONTS];
};

static void *nx_collection_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_collection_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

nx_result nx_font_collection_create(const nx_allocator *allocator,
                                    nx_font_collection **out_collection) {
    if (!out_collection) return NX_ERROR_ARGUMENT;
    *out_collection = NULL;
    nx_allocator selected = allocator ? *allocator : (nx_allocator){0};
    if ((selected.allocate == NULL) != (selected.deallocate == NULL))
        return NX_ERROR_ARGUMENT;
    if (!selected.allocate) {
        selected.allocate = nx_collection_allocate;
        selected.deallocate = nx_collection_deallocate;
        selected.user = NULL;
    }
    nx_font_collection *collection =
        selected.allocate(selected.user, sizeof(*collection));
    if (!collection) return NX_ERROR_MEMORY;
    memset(collection, 0, sizeof(*collection));
    collection->allocator = selected;
    *out_collection = collection;
    return NX_OK;
}

void nx_font_collection_destroy(nx_font_collection *collection) {
    if (!collection) return;
    nx_allocator allocator = collection->allocator;
    allocator.deallocate(allocator.user, collection, sizeof(*collection));
}

void nx_font_collection_clear(nx_font_collection *collection) {
    if (!collection) return;
    for (size_t i = 0; i < collection->count; ++i)
        collection->fonts[i] = NULL;
    collection->count = 0;
}

nx_result nx_font_collection_add(nx_font_collection *collection,
                                 const nx_font *font) {
    if (!collection || !font) return NX_ERROR_ARGUMENT;
    for (size_t i = 0; i < collection->count; ++i)
        if (collection->fonts[i] == font) return NX_ERROR_ARGUMENT;
    if (collection->count >= NX_FONT_COLLECTION_MAX_FONTS)
        return NX_ERROR_LIMIT;
    uint16_t probe;
    nx_result result = nx_font_glyph_index(font, 0u, &probe);
    if (result != NX_OK) return result;
    collection->fonts[collection->count++] = font;
    return NX_OK;
}

nx_result nx_font_collection_count(const nx_font_collection *collection,
                                   size_t *out_count) {
    if (!out_count) return NX_ERROR_ARGUMENT;
    *out_count = 0;
    if (!collection) return NX_ERROR_ARGUMENT;
    *out_count = collection->count;
    return NX_OK;
}

nx_result nx_font_collection_font_at(const nx_font_collection *collection,
                                     uint32_t index, const nx_font **out_font) {
    if (!out_font) return NX_ERROR_ARGUMENT;
    *out_font = NULL;
    if (!collection || index >= collection->count) return NX_ERROR_ARGUMENT;
    *out_font = collection->fonts[index];
    return NX_OK;
}

nx_result nx_font_collection_resolve(const nx_font_collection *collection,
                                     uint32_t scalar, nx_font_match *out_match) {
    if (!out_match) return NX_ERROR_ARGUMENT;
    *out_match = (nx_font_match){0};
    out_match->font_index = UINT32_MAX;
    if (!collection || scalar > UINT32_C(0x10ffff) ||
        (scalar >= UINT32_C(0xd800) && scalar <= UINT32_C(0xdfff)))
        return NX_ERROR_ARGUMENT;
    for (size_t i = 0; i < collection->count; ++i) {
        uint16_t glyph = 0;
        nx_result result = nx_font_glyph_index(collection->fonts[i], scalar,
                                                &glyph);
        if (result != NX_OK) return result;
        if (glyph) {
            out_match->font = collection->fonts[i];
            out_match->font_index = (uint32_t)i;
            out_match->glyph_index = glyph;
            break;
        }
    }
    return NX_OK;
}
