#include "nexaui/nexaui.h"

#include <stdio.h>
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

static void format4(uint8_t *subtable) {
    put16(subtable, 4);
    put16(subtable + 2, 44);
    put16(subtable + 6, 6); /* Three segments. */
    put16(subtable + 8, 4);
    put16(subtable + 10, 1);
    put16(subtable + 12, 2);
    put16(subtable + 14, 0x42);
    put16(subtable + 16, 0x62);
    put16(subtable + 18, 0xffff);
    put16(subtable + 22, 0x41);
    put16(subtable + 24, 0x61);
    put16(subtable + 26, 0xffff);
    put16(subtable + 28, (uint16_t)-64); /* A -> 1, B -> 2. */
    put16(subtable + 30, 1);             /* glyphIdArray + 1. */
    put16(subtable + 32, 1);             /* Sentinel -> zero. */
    put16(subtable + 34, 0);
    put16(subtable + 36, 4); /* Relative to this idRangeOffset word. */
    put16(subtable + 38, 0);
    put16(subtable + 40, 3); /* a -> 4 after delta. */
    put16(subtable + 42, 0); /* b remains missing. */
}

static void format12(uint8_t *subtable) {
    put16(subtable, 12);
    put32(subtable + 4, 52);
    put32(subtable + 12, 3);
    put32(subtable + 16, 0x41);
    put32(subtable + 20, 0x42);
    put32(subtable + 24, 10);
    put32(subtable + 28, 0x61);
    put32(subtable + 32, 0x62);
    put32(subtable + 36, 12);
    put32(subtable + 40, 0x1f600);
    put32(subtable + 44, 0x1f601);
    put32(subtable + 48, 20);
}

static void font4(uint8_t bytes[84]) {
    memset(bytes, 0, 84);
    put32(bytes, UINT32_C(0x00010000));
    put16(bytes + 4, 1);
    put32(bytes + 12, NX_SFNT_TAG('c', 'm', 'a', 'p'));
    put32(bytes + 20, 28);
    put32(bytes + 24, 56);
    uint8_t *cmap = bytes + 28;
    put16(cmap + 2, 1);
    put16(cmap + 4, 3);
    put16(cmap + 6, 1);
    put32(cmap + 8, 12);
    format4(cmap + 12);
}

static void font12(uint8_t bytes[144]) {
    memset(bytes, 0, 144);
    put32(bytes, UINT32_C(0x00010000));
    put16(bytes + 4, 1);
    put32(bytes + 12, NX_SFNT_TAG('c', 'm', 'a', 'p'));
    put32(bytes + 20, 28);
    put32(bytes + 24, 116);
    uint8_t *cmap = bytes + 28;
    put16(cmap + 2, 2);
    put16(cmap + 4, 3);
    put16(cmap + 6, 1);
    put32(cmap + 8, 20);
    put16(cmap + 12, 3);
    put16(cmap + 14, 10);
    put32(cmap + 16, 64);
    format4(cmap + 20);
    format12(cmap + 64);
}

static int glyph_is(nx_font *font, uint32_t scalar, uint16_t expected) {
    uint16_t glyph = UINT16_MAX;
    return nx_font_glyph_index(font, scalar, &glyph) == NX_OK && glyph == expected;
}

static int test_format4(void) {
    uint8_t bytes[84];
    font4(bytes);
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(glyph_is(font, 'A', 1) && glyph_is(font, 'B', 2));
    CHECK(glyph_is(font, 'a', 4) && glyph_is(font, 'b', 0));
    CHECK(glyph_is(font, 'C', 0) && glyph_is(font, 0x1f600, 0));
    uint16_t glyph = 99;
    CHECK(nx_font_glyph_index(font, 0xd800, &glyph) == NX_ERROR_ARGUMENT &&
          glyph == 0);
    CHECK(nx_font_glyph_index(font, 0x110000, &glyph) == NX_ERROR_ARGUMENT);
    nx_font_close(font);
    return 0;
}

static int test_format12_preference(void) {
    uint8_t bytes[144];
    font12(bytes);
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(glyph_is(font, 'A', 10) && glyph_is(font, 'B', 11));
    CHECK(glyph_is(font, 'a', 12) && glyph_is(font, 'b', 13));
    CHECK(glyph_is(font, 0x1f600, 20) && glyph_is(font, 0x1f601, 21));
    CHECK(glyph_is(font, 0x1f602, 0));
    nx_font_close(font);
    font12(bytes);
    put16(bytes + 28 + 12, 0); /* Unicode platform full repertoire. */
    put16(bytes + 28 + 14, 4);
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(glyph_is(font, 0x1f600, 20));
    nx_font_close(font);
    return 0;
}

static int test_malformed_and_fallback(void) {
    uint8_t bytes[144];
    nx_font *font = NULL;
    uint16_t glyph = 99;
    font12(bytes);
    put32(bytes + 28 + 64 + 28, 0x40); /* Groups now overlap. */
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(glyph_is(font, 'A', 1)); /* Valid BMP table is retained. */
    CHECK(glyph_is(font, 0x1f600, 0));
    nx_font_close(font);

    uint8_t short_font[84];
    font4(short_font);
    put16(short_font + 28 + 12 + 36, 3); /* Odd range offset. */
    CHECK(nx_font_open(short_font, sizeof(short_font), NULL, &font) == NX_OK);
    CHECK(nx_font_glyph_index(font, 'a', &glyph) == NX_ERROR_ARGUMENT && glyph == 0);
    nx_font_close(font);
    font4(short_font);
    put16(short_font + 28 + 12 + 2, 42); /* Last glyph-array word is outside. */
    CHECK(nx_font_open(short_font, sizeof(short_font), NULL, &font) == NX_OK);
    CHECK(nx_font_glyph_index(font, 'a', &glyph) == NX_ERROR_ARGUMENT);
    nx_font_close(font);
    font4(short_font);
    put32(short_font + 28 + 8, UINT32_MAX);
    CHECK(nx_font_open(short_font, sizeof(short_font), NULL, &font) == NX_OK);
    CHECK(nx_font_glyph_index(font, 'A', &glyph) == NX_ERROR_ARGUMENT);
    nx_font_close(font);
    font4(short_font);
    put32(short_font + 28 + 8, 4); /* Subtable overlaps encoding records. */
    CHECK(nx_font_open(short_font, sizeof(short_font), NULL, &font) == NX_OK);
    CHECK(nx_font_glyph_index(font, 'A', &glyph) == NX_ERROR_ARGUMENT);
    nx_font_close(font);
    font4(short_font);
    put32(short_font + 12, NX_SFNT_TAG('h', 'e', 'a', 'd'));
    CHECK(nx_font_open(short_font, sizeof(short_font), NULL, &font) == NX_OK);
    CHECK(nx_font_glyph_index(font, 'A', &glyph) == NX_ERROR_UNSUPPORTED);
    nx_font_close(font);
    return 0;
}

static int test_glyph_count_bounds(void) {
    uint8_t base[84], bytes[106] = {0};
    font4(base);
    memcpy(bytes, base, 12);
    put16(bytes + 4, 2);
    put32(bytes + 12, NX_SFNT_TAG('c', 'm', 'a', 'p'));
    put32(bytes + 20, 44);
    put32(bytes + 24, 56);
    put32(bytes + 28, NX_SFNT_TAG('m', 'a', 'x', 'p'));
    put32(bytes + 36, 100);
    put32(bytes + 40, 6);
    memcpy(bytes + 44, base + 28, 56);
    put32(bytes + 100, UINT32_C(0x00005000));
    put16(bytes + 104, 4);
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(glyph_is(font, 'A', 1));
    uint16_t glyph = 99;
    CHECK(nx_font_glyph_index(font, 'a', &glyph) == NX_ERROR_ARGUMENT && glyph == 0);
    nx_font_close(font);

    uint8_t base12[144], bytes12[166] = {0};
    font12(base12);
    memcpy(bytes12, base12, 12);
    put16(bytes12 + 4, 2);
    put32(bytes12 + 12, NX_SFNT_TAG('c', 'm', 'a', 'p'));
    put32(bytes12 + 20, 44);
    put32(bytes12 + 24, 116);
    put32(bytes12 + 28, NX_SFNT_TAG('m', 'a', 'x', 'p'));
    put32(bytes12 + 36, 160);
    put32(bytes12 + 40, 6);
    memcpy(bytes12 + 44, base12 + 28, 116);
    put32(bytes12 + 160, UINT32_C(0x00005000));
    put16(bytes12 + 164, 21); /* Format 12 reaches glyph 21: invalid. */
    CHECK(nx_font_open(bytes12, sizeof(bytes12), NULL, &font) == NX_OK);
    CHECK(glyph_is(font, 'A', 1) && glyph_is(font, 0x1f600, 0));
    nx_font_close(font);
    return 0;
}

int main(void) {
    if (test_format4()) return 1;
    if (test_format12_preference()) return 1;
    if (test_malformed_and_fallback()) return 1;
    if (test_glyph_count_bounds()) return 1;
    puts("cmap tests passed");
    return 0;
}
