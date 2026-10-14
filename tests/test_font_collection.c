#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static void put16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8u);
    p[1] = (uint8_t)value;
}

static void put32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24u);
    p[1] = (uint8_t)(value >> 16u);
    p[2] = (uint8_t)(value >> 8u);
    p[3] = (uint8_t)value;
}

static void fixture(uint8_t bytes[68], uint32_t scalar, uint16_t glyph) {
    memset(bytes, 0, 68);
    put32(bytes, UINT32_C(0x00010000));
    put16(bytes + 4u, 1);
    put32(bytes + 12u, NX_SFNT_TAG('c', 'm', 'a', 'p'));
    put32(bytes + 20u, 28);
    put32(bytes + 24u, 40);
    uint8_t *cmap = bytes + 28u;
    put16(cmap + 2u, 1);
    put16(cmap + 4u, 3);
    put16(cmap + 6u, 10);
    put32(cmap + 8u, 12);
    uint8_t *format12 = cmap + 12u;
    put16(format12, 12);
    put32(format12 + 4u, 28);
    put32(format12 + 12u, 1);
    put32(format12 + 16u, scalar);
    put32(format12 + 20u, scalar);
    put32(format12 + 24u, glyph);
}

static int test_order_missing_and_clear(void) {
    uint8_t latin[68], greek[68], alternate[68];
    fixture(latin, 'A', 1);
    fixture(greek, 0x03a9u, 2);
    fixture(alternate, 'A', 3);
    nx_font *latin_font = NULL, *greek_font = NULL, *alternate_font = NULL;
    CHECK(nx_font_open(latin, sizeof(latin), NULL, &latin_font) == NX_OK);
    CHECK(nx_font_open(greek, sizeof(greek), NULL, &greek_font) == NX_OK);
    CHECK(nx_font_open(alternate, sizeof(alternate), NULL,
                       &alternate_font) == NX_OK);
    nx_font_collection *collection = NULL;
    CHECK(nx_font_collection_create(NULL, &collection) == NX_OK);
    CHECK(nx_font_collection_add(collection, latin_font) == NX_OK);
    CHECK(nx_font_collection_add(collection, alternate_font) == NX_OK);
    CHECK(nx_font_collection_add(collection, greek_font) == NX_OK);
    CHECK(nx_font_collection_add(collection, latin_font) == NX_ERROR_ARGUMENT);
    size_t count = 0;
    CHECK(nx_font_collection_count(collection, &count) == NX_OK && count == 3);
    const nx_font *at = NULL;
    CHECK(nx_font_collection_font_at(collection, 2, &at) == NX_OK &&
          at == greek_font);
    CHECK(nx_font_collection_font_at(collection, 3, &at) == NX_ERROR_ARGUMENT &&
          at == NULL);
    nx_font_match match;
    CHECK(nx_font_collection_resolve(collection, 'A', &match) == NX_OK);
    CHECK(match.font == latin_font && match.font_index == 0 &&
          match.glyph_index == 1);
    CHECK(nx_font_collection_resolve(collection, 0x03a9u, &match) == NX_OK);
    CHECK(match.font == greek_font && match.font_index == 2 &&
          match.glyph_index == 2);
    CHECK(nx_font_collection_resolve(collection, 'Z', &match) == NX_OK);
    CHECK(match.font == NULL && match.font_index == UINT32_MAX &&
          match.glyph_index == 0);
    CHECK(nx_font_collection_resolve(collection, 0xd800u, &match) ==
          NX_ERROR_ARGUMENT);
    CHECK(match.font == NULL && match.font_index == UINT32_MAX);
    nx_font_collection_clear(collection);
    CHECK(nx_font_collection_count(collection, &count) == NX_OK && count == 0);
    CHECK(nx_font_collection_resolve(collection, 'A', &match) == NX_OK &&
          match.font == NULL);
    CHECK(nx_font_collection_add(collection, alternate_font) == NX_OK);
    CHECK(nx_font_collection_add(collection, latin_font) == NX_OK);
    CHECK(nx_font_collection_resolve(collection, 'A', &match) == NX_OK &&
          match.font == alternate_font && match.glyph_index == 3);
    nx_font_collection_destroy(collection);
    nx_font_close(alternate_font);
    nx_font_close(greek_font);
    nx_font_close(latin_font);
    return 0;
}

static int test_unsupported_and_capacity(void) {
    uint8_t bytes[68], unsupported[28] = {0};
    fixture(bytes, 'A', 1);
    put32(unsupported, UINT32_C(0x00010000));
    put16(unsupported + 4u, 1);
    put32(unsupported + 12u, NX_SFNT_TAG('n', 'a', 'm', 'e'));
    put32(unsupported + 20u, 28);
    nx_font *no_cmap = NULL;
    CHECK(nx_font_open(unsupported, sizeof(unsupported), NULL,
                       &no_cmap) == NX_OK);
    nx_font_collection *collection = NULL;
    CHECK(nx_font_collection_create(NULL, &collection) == NX_OK);
    CHECK(nx_font_collection_add(collection, no_cmap) ==
          NX_ERROR_UNSUPPORTED);
    nx_font_close(no_cmap);
    nx_font *fonts[NX_FONT_COLLECTION_MAX_FONTS + 1u] = {0};
    for (size_t i = 0; i < NX_FONT_COLLECTION_MAX_FONTS + 1u; ++i) {
        CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &fonts[i]) == NX_OK);
        CHECK(nx_font_collection_add(collection, fonts[i]) ==
              (i < NX_FONT_COLLECTION_MAX_FONTS ? NX_OK : NX_ERROR_LIMIT));
    }
    size_t count = 0;
    CHECK(nx_font_collection_count(collection, &count) == NX_OK &&
          count == NX_FONT_COLLECTION_MAX_FONTS);
    nx_font_collection_destroy(collection);
    for (size_t i = 0; i < NX_FONT_COLLECTION_MAX_FONTS + 1u; ++i)
        nx_font_close(fonts[i]);
    return 0;
}

typedef struct tracker {
    unsigned calls, frees, fail_after;
} tracker;

static void *tracked_allocate(void *user, size_t size) {
    tracker *state = (tracker *)user;
    if (++state->calls > state->fail_after) return NULL;
    return malloc(size);
}

static void tracked_deallocate(void *user, void *pointer, size_t size) {
    tracker *state = (tracker *)user;
    (void)size;
    ++state->frees;
    free(pointer);
}

static int test_allocator_and_arguments(void) {
    tracker state = {0, 0, 0};
    nx_allocator allocator = {&state, tracked_allocate, tracked_deallocate};
    nx_font_collection *collection = NULL;
    CHECK(nx_font_collection_create(&allocator, &collection) ==
          NX_ERROR_MEMORY && collection == NULL);
    state = (tracker){0, 0, 1};
    CHECK(nx_font_collection_create(&allocator, &collection) == NX_OK);
    CHECK(nx_font_collection_add(collection, NULL) == NX_ERROR_ARGUMENT);
    nx_font_match match;
    CHECK(nx_font_collection_resolve(collection, 0x110000u, &match) ==
          NX_ERROR_ARGUMENT);
    nx_font_collection_destroy(collection);
    CHECK(state.frees == 1);
    CHECK(nx_font_collection_create(NULL, NULL) == NX_ERROR_ARGUMENT);
    return 0;
}

int main(void) {
    if (test_order_missing_and_clear()) return 1;
    if (test_unsupported_and_capacity()) return 1;
    if (test_allocator_and_arguments()) return 1;
    puts("font collection tests passed");
    return 0;
}
