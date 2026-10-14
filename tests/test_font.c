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

static void *incomplete_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void fixture(uint8_t bytes[106]) {
    memset(bytes, 0, 106);
    put32(bytes, UINT32_C(0x00010000));
    put16(bytes + 4, 2);
    put32(bytes + 12, NX_SFNT_TAG('m', 'a', 'x', 'p'));
    put32(bytes + 16, UINT32_C(0x12345678));
    put32(bytes + 20, 100);
    put32(bytes + 24, 6);
    put32(bytes + 28, NX_SFNT_TAG('h', 'e', 'a', 'd'));
    put32(bytes + 32, UINT32_C(0xabcdef01));
    put32(bytes + 36, 44);
    put32(bytes + 40, 54);
    bytes[44] = 0x12;
    bytes[100] = 0x34;
}

static void metrics_fixture(uint8_t bytes[210]) {
    memset(bytes, 0, 210);
    put32(bytes, UINT32_C(0x00010000));
    put16(bytes + 4, 4);
    const uint32_t tags[4] = {
        NX_SFNT_TAG('h', 'm', 't', 'x'), NX_SFNT_TAG('m', 'a', 'x', 'p'),
        NX_SFNT_TAG('h', 'e', 'a', 'd'), NX_SFNT_TAG('h', 'h', 'e', 'a')
    };
    const uint32_t offsets[4] = {200, 132, 76, 164};
    const uint32_t lengths[4] = {10, 32, 54, 36};
    for (unsigned i = 0; i < 4; ++i) {
        put32(bytes + 12 + i * 16, tags[i]);
        put32(bytes + 20 + i * 16, offsets[i]);
        put32(bytes + 24 + i * 16, lengths[i]);
    }
    put32(bytes + 76, UINT32_C(0x00010000));
    put32(bytes + 88, UINT32_C(0x5f0f3cf5));
    put16(bytes + 94, 1000);
    put16(bytes + 126, 0);
    put32(bytes + 132, UINT32_C(0x00010000));
    put16(bytes + 136, 3);
    put32(bytes + 164, UINT32_C(0x00010000));
    put16(bytes + 168, 800);
    put16(bytes + 170, (uint16_t)-200);
    put16(bytes + 172, 50);
    put16(bytes + 198, 2);
    put16(bytes + 200, 500);
    put16(bytes + 202, 10);
    put16(bytes + 204, 600);
    put16(bytes + 206, (uint16_t)-20);
    put16(bytes + 208, 30);
}

static int test_directory(void) {
    uint8_t bytes[106];
    fixture(bytes);
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK && font);
    size_t count = 0;
    CHECK(nx_font_table_count(font, &count) == NX_OK && count == 2);
    nx_bytes_view table;
    uint32_t tag = 0;
    CHECK(nx_font_table_at(font, 0, &tag, &table) == NX_OK);
    CHECK(tag == NX_SFNT_TAG('h', 'e', 'a', 'd') && table.data == bytes + 44 &&
          table.length == 54 && table.checksum == UINT32_C(0xabcdef01));
    CHECK(nx_font_find_table(font, NX_SFNT_TAG('m', 'a', 'x', 'p'), &table) == NX_OK);
    CHECK(table.data == bytes + 100 && table.length == 6 &&
          table.checksum == UINT32_C(0x12345678));
    CHECK(nx_font_find_table(font, NX_SFNT_TAG('c', 'm', 'a', 'p'), &table)
          == NX_ERROR_ARGUMENT && !table.data);
    CHECK(nx_font_table_at(font, 2, &tag, &table) == NX_ERROR_ARGUMENT &&
          tag == 0 && !table.data);
    nx_font_close(font);
    return 0;
}

static int test_malformed(void) {
    uint8_t bytes[106];
    nx_font *font = NULL;
    fixture(bytes);
    CHECK(nx_font_open(bytes, 11, NULL, &font) == NX_ERROR_ARGUMENT && !font);
    CHECK(nx_font_open(bytes, 43, NULL, &font) == NX_ERROR_ARGUMENT && !font);
    put32(bytes, 0);
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_ERROR_ARGUMENT);
    fixture(bytes);
    put16(bytes + 4, 0);
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_ERROR_ARGUMENT);
    fixture(bytes);
    put16(bytes + 4, NX_SFNT_MAX_TABLES + 1u);
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_ERROR_LIMIT);
    fixture(bytes);
    put32(bytes + 20, 40);
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_ERROR_ARGUMENT);
    fixture(bytes);
    put32(bytes + 20, 107);
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_ERROR_ARGUMENT);
    fixture(bytes);
    put32(bytes + 24, UINT32_MAX);
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_ERROR_ARGUMENT);
    fixture(bytes);
    put32(bytes + 28, NX_SFNT_TAG('m', 'a', 'x', 'p'));
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_ERROR_ARGUMENT);
    fixture(bytes);
    nx_allocator incomplete = {0};
    incomplete.allocate = incomplete_allocate;
    CHECK(nx_font_open(bytes, sizeof(bytes), &incomplete, &font) == NX_ERROR_ARGUMENT);
    return 0;
}

static int test_metrics(void) {
    uint8_t bytes[210];
    metrics_fixture(bytes);
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    nx_font_metrics metrics;
    CHECK(nx_font_get_metrics(font, &metrics) == NX_OK);
    CHECK(metrics.units_per_em == 1000 && metrics.glyph_count == 3 &&
          metrics.number_of_hmetrics == 2 && metrics.ascender == 800 &&
          metrics.descender == -200 && metrics.line_gap == 50 &&
          metrics.index_to_loc_format == 0);
    uint16_t advance;
    int16_t lsb;
    CHECK(nx_font_glyph_hmetrics(font, 0, &advance, &lsb) == NX_OK &&
          advance == 500 && lsb == 10);
    CHECK(nx_font_glyph_hmetrics(font, 1, &advance, &lsb) == NX_OK &&
          advance == 600 && lsb == -20);
    CHECK(nx_font_glyph_hmetrics(font, 2, &advance, &lsb) == NX_OK &&
          advance == 600 && lsb == 30);
    CHECK(nx_font_glyph_hmetrics(font, 3, &advance, &lsb) == NX_ERROR_ARGUMENT &&
          advance == 0 && lsb == 0);
    nx_font_close(font);

    metrics_fixture(bytes);
    put16(bytes + 94, 0);
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_get_metrics(font, &metrics) == NX_ERROR_ARGUMENT &&
          metrics.units_per_em == 0);
    nx_font_close(font);
    metrics_fixture(bytes);
    put32(bytes + 24, 9); /* hmtx is too short for the final LSB. */
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_get_metrics(font, &metrics) == NX_ERROR_ARGUMENT);
    nx_font_close(font);
    metrics_fixture(bytes);
    put32(bytes + 12 + 16 + 12, 6); /* maxp v1.0 needs its full header. */
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_get_metrics(font, &metrics) == NX_ERROR_ARGUMENT);
    nx_font_close(font);
    metrics_fixture(bytes);
    put16(bytes + 198, 4); /* More long metrics than glyphs. */
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_get_metrics(font, &metrics) == NX_ERROR_ARGUMENT);
    nx_font_close(font);
    return 0;
}

typedef struct tracking { unsigned calls, frees, fail_after; } tracking;
static void *tracked_allocate(void *user, size_t size) {
    tracking *t = (tracking *)user;
    ++t->calls;
    if (t->calls > t->fail_after) return NULL;
    return malloc(size);
}
static void tracked_deallocate(void *user, void *pointer, size_t size) {
    tracking *t = (tracking *)user;
    (void)size;
    ++t->frees;
    free(pointer);
}

static int test_allocations_and_mutations(void) {
    uint8_t bytes[106];
    fixture(bytes);
    for (unsigned fail_after = 0; fail_after < 3; ++fail_after) {
        tracking t = {0, 0, fail_after};
        nx_allocator allocator = {&t, tracked_allocate, tracked_deallocate};
        nx_font *font = NULL;
        nx_result result = nx_font_open(bytes, sizeof(bytes), &allocator, &font);
        CHECK(result == (fail_after < 2 ? NX_ERROR_MEMORY : NX_OK));
        nx_font_close(font);
        CHECK(t.frees == fail_after);
    }
    /* Deterministic mutation smoke: every header/directory byte is flipped. */
    for (size_t i = 0; i < 44; ++i) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            uint8_t modified = bytes[i];
            bytes[i] ^= (uint8_t)(1u << bit);
            nx_font *font = NULL;
            nx_result result = nx_font_open(bytes, sizeof(bytes), NULL, &font);
            CHECK(result == NX_OK || result == NX_ERROR_ARGUMENT ||
                  result == NX_ERROR_LIMIT);
            nx_font_close(font);
            bytes[i] = modified;
        }
    }
    return 0;
}

int main(void) {
    if (test_directory()) return 1;
    if (test_malformed()) return 1;
    if (test_metrics()) return 1;
    if (test_allocations_and_mutations()) return 1;
    puts("font directory tests passed");
    return 0;
}
