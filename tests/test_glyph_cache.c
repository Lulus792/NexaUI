#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8u);
    p[1] = (uint8_t)v;
}

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24u);
    p[1] = (uint8_t)(v >> 16u);
    p[2] = (uint8_t)(v >> 8u);
    p[3] = (uint8_t)v;
}

static void record(uint8_t *bytes, unsigned index, uint32_t tag,
                   uint32_t offset, uint32_t length) {
    uint8_t *entry = bytes + 12u + index * 16u;
    put32(entry, tag);
    put32(entry + 8u, offset);
    put32(entry + 12u, length);
}

static void fixture(uint8_t bytes[384]) {
    memset(bytes, 0, 384);
    put32(bytes, UINT32_C(0x00010000));
    put16(bytes + 4u, 6);
    record(bytes, 0, NX_SFNT_TAG('h', 'e', 'a', 'd'), 108, 54);
    record(bytes, 1, NX_SFNT_TAG('m', 'a', 'x', 'p'), 164, 32);
    record(bytes, 2, NX_SFNT_TAG('h', 'h', 'e', 'a'), 196, 36);
    record(bytes, 3, NX_SFNT_TAG('h', 'm', 't', 'x'), 232, 16);
    record(bytes, 4, NX_SFNT_TAG('l', 'o', 'c', 'a'), 248, 20);
    record(bytes, 5, NX_SFNT_TAG('g', 'l', 'y', 'f'), 272, 60);
    put32(bytes + 108, UINT32_C(0x00010000));
    put32(bytes + 120, UINT32_C(0x5f0f3cf5));
    put16(bytes + 126, 1000);
    put16(bytes + 158, 1);
    put32(bytes + 164, UINT32_C(0x00010000));
    put16(bytes + 168, 4);
    put32(bytes + 196, UINT32_C(0x00010000));
    put16(bytes + 230, 4);
    for (size_t i = 0; i < 4u; ++i)
        put16(bytes + 232u + i * 4u, 500);
    put32(bytes + 248u + 8u, 20);
    put32(bytes + 248u + 12u, 40);
    put32(bytes + 248u + 16u, 60);
    uint8_t *glyph = bytes + 272u;
    put16(glyph, 1);
    put16(glyph + 6u, 100);
    put16(glyph + 8u, 100);
    put16(glyph + 10u, 2);
    glyph[14] = 0x31;
    glyph[15] = 0x33;
    glyph[16] = 0x27;
    glyph[17] = glyph[18] = glyph[19] = 100;
    memcpy(glyph + 20u, glyph, 20u);
    memcpy(glyph + 40u, glyph, 20u);
}

static int test_cache_hits_and_empty_glyph(void) {
    uint8_t bytes[384];
    fixture(bytes);
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    nx_glyph_cache *cache = NULL;
    CHECK(nx_glyph_cache_create(font, 65536, NULL, &cache) == NX_OK);
    nx_glyph_mask_view first, second;
    CHECK(nx_glyph_cache_get(cache, 1, 20, &first) == NX_OK);
    CHECK(first.pixels && first.width == 4 && first.height == 4 &&
          first.stride_bytes == 4 && first.x_offset == -1 &&
          first.y_offset == -3 && first.advance_x == 10.0);
    unsigned coverage_sum = 0;
    for (size_t i = 0; i < 16u; ++i) coverage_sum += first.pixels[i];
    CHECK(coverage_sum > 0);
    CHECK(nx_glyph_cache_get(cache, 1, 20, &second) == NX_OK);
    CHECK(second.pixels == first.pixels);
    nx_glyph_cache_stats stats;
    CHECK(nx_glyph_cache_get_stats(cache, &stats) == NX_OK);
    CHECK(stats.hits == 1 && stats.misses == 1 && stats.entry_count == 1 &&
          stats.used_bytes > 16 && stats.used_bytes <= stats.max_bytes);
    CHECK(nx_glyph_cache_get(cache, 0, 20, &second) == NX_OK);
    CHECK(second.pixels == NULL && second.width == 0 && second.height == 0 &&
          second.advance_x == 10.0);
    CHECK(nx_glyph_cache_get(cache, 1, 21, &second) == NX_OK);
    CHECK(nx_glyph_cache_get_stats(cache, &stats) == NX_OK);
    CHECK(stats.entry_count == 3 && stats.misses == 3);
    nx_glyph_cache_clear(cache);
    CHECK(nx_glyph_cache_get_stats(cache, &stats) == NX_OK);
    CHECK(stats.entry_count == 0 && stats.used_bytes == 0 &&
          stats.hits == 1 && stats.misses == 3);
    CHECK(nx_glyph_cache_get(cache, 1, 20, &second) == NX_OK);
    CHECK(nx_glyph_cache_get_stats(cache, &stats) == NX_OK);
    CHECK(stats.misses == 4);
    nx_glyph_cache_destroy(cache);
    nx_font_close(font);
    return 0;
}

static int test_lru_and_limits(void) {
    uint8_t bytes[384];
    fixture(bytes);
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    nx_glyph_cache *cache = NULL;
    nx_glyph_mask_view mask;
    nx_glyph_cache_stats stats;
    CHECK(nx_glyph_cache_create(font, 65536, NULL, &cache) == NX_OK);
    CHECK(nx_glyph_cache_get(cache, 1, 20, &mask) == NX_OK);
    CHECK(nx_glyph_cache_get_stats(cache, &stats) == NX_OK);
    size_t entry_size = stats.used_bytes;
    nx_glyph_cache_destroy(cache);

    CHECK(nx_glyph_cache_create(font, entry_size * 2u, NULL, &cache) == NX_OK);
    CHECK(nx_glyph_cache_get(cache, 1, 20, &mask) == NX_OK);
    CHECK(nx_glyph_cache_get(cache, 2, 20, &mask) == NX_OK);
    CHECK(nx_glyph_cache_get(cache, 1, 20, &mask) == NX_OK); /* Touch MRU. */
    CHECK(nx_glyph_cache_get(cache, 3, 20, &mask) == NX_OK); /* Evict glyph 2. */
    CHECK(nx_glyph_cache_get_stats(cache, &stats) == NX_OK);
    CHECK(stats.entry_count == 2 && stats.used_bytes == stats.max_bytes &&
          stats.hits == 1 && stats.misses == 3 && stats.evictions == 1);
    CHECK(nx_glyph_cache_get(cache, 1, 20, &mask) == NX_OK);
    CHECK(nx_glyph_cache_get(cache, 2, 20, &mask) == NX_OK);
    CHECK(nx_glyph_cache_get_stats(cache, &stats) == NX_OK);
    CHECK(stats.hits == 2 && stats.misses == 4 && stats.evictions == 2);
    CHECK(nx_glyph_cache_get(cache, 4, 20, &mask) == NX_ERROR_ARGUMENT);
    CHECK(nx_glyph_cache_get(cache, 1, 0, &mask) == NX_ERROR_ARGUMENT);
    CHECK(nx_glyph_cache_get(cache, 1, 20, NULL) == NX_ERROR_ARGUMENT);
    nx_glyph_cache_destroy(cache);

    CHECK(nx_glyph_cache_create(font, entry_size - 1u, NULL, &cache) == NX_OK);
    CHECK(nx_glyph_cache_get(cache, 1, 20, &mask) == NX_ERROR_LIMIT);
    CHECK(mask.pixels == NULL);
    CHECK(nx_glyph_cache_get_stats(cache, &stats) == NX_OK);
    CHECK(stats.used_bytes == 0 && stats.entry_count == 0);
    nx_glyph_cache_destroy(cache);
    nx_font_close(font);
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

static int test_allocator_failure(void) {
    uint8_t bytes[384];
    fixture(bytes);
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    for (unsigned fail_after = 0; fail_after <= 6u; ++fail_after) {
        tracker state = {0, 0, fail_after};
        nx_allocator allocator = {&state, tracked_allocate, tracked_deallocate};
        nx_glyph_cache *cache = NULL;
        nx_result result = nx_glyph_cache_create(font, 65536,
                                                  &allocator, &cache);
        if (fail_after == 0u) {
            CHECK(result == NX_ERROR_MEMORY && cache == NULL);
        } else {
            CHECK(result == NX_OK && cache);
            nx_glyph_mask_view mask;
            result = nx_glyph_cache_get(cache, 1, 20, &mask);
            CHECK(result == (fail_after == 6u ? NX_OK : NX_ERROR_MEMORY));
            if (fail_after < 6u)
                CHECK(mask.pixels == NULL && mask.width == 0);
            nx_glyph_cache_stats stats;
            CHECK(nx_glyph_cache_get_stats(cache, &stats) == NX_OK);
            CHECK(stats.entry_count == (fail_after == 6u ? 1u : 0u));
            nx_glyph_cache_destroy(cache);
        }
        CHECK(state.frees == fail_after);
    }
    nx_font_close(font);
    return 0;
}

int main(void) {
    if (test_cache_hits_and_empty_glyph()) return 1;
    if (test_lru_and_limits()) return 1;
    if (test_allocator_failure()) return 1;
    puts("glyph cache tests passed");
    return 0;
}
