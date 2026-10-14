#include "nexaui/nexaui.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", \
    __FILE__, __LINE__, #x); return 1; } } while (0)

static void put16(uint8_t *p, uint16_t x) {
    p[0] = (uint8_t)(x >> 8u); p[1] = (uint8_t)x;
}
static void put32(uint8_t *p, uint32_t x) {
    p[0] = (uint8_t)(x >> 24u); p[1] = (uint8_t)(x >> 16u);
    p[2] = (uint8_t)(x >> 8u); p[3] = (uint8_t)x;
}

static void fixture(uint8_t bytes[522]) {
    memset(bytes, 0, 522);
    put32(bytes, UINT32_C(0x00010000));
    put16(bytes + 4u, 6);
    const uint32_t tags[6] = {
        NX_SFNT_TAG('c','m','a','p'), NX_SFNT_TAG('G','S','U','B'),
        NX_SFNT_TAG('h','e','a','d'), NX_SFNT_TAG('h','h','e','a'),
        NX_SFNT_TAG('h','m','t','x'), NX_SFNT_TAG('m','a','x','p')
    };
    const uint32_t offsets[6] = {264,400,108,196,232,164};
    const uint32_t lengths[6] = {64,122,54,36,32,32};
    for (size_t i = 0; i < 6; ++i) {
        put32(bytes + 12u + i * 16u, tags[i]);
        put32(bytes + 20u + i * 16u, offsets[i]);
        put32(bytes + 24u + i * 16u, lengths[i]);
    }
    put32(bytes + 108u, UINT32_C(0x00010000));
    put32(bytes + 120u, UINT32_C(0x5f0f3cf5));
    put16(bytes + 126u, 1000);
    put32(bytes + 164u, UINT32_C(0x00010000));
    put16(bytes + 168u, 8);
    put32(bytes + 196u, UINT32_C(0x00010000));
    put16(bytes + 200u, 800);
    put16(bytes + 202u, (uint16_t)-200);
    put16(bytes + 230u, 8);
    for (size_t i = 0; i < 8; ++i) put16(bytes + 232u + i * 4u, 600);
    uint8_t *cmap = bytes + 264u;
    put16(cmap + 2u, 1);
    put16(cmap + 4u, 3);
    put16(cmap + 6u, 10);
    put32(cmap + 8u, 12);
    uint8_t *format12 = cmap + 12u;
    put16(format12, 12);
    put32(format12 + 4u, 52);
    put32(format12 + 12u, 3);
    const uint32_t mappings[3][2] = {{'A',5},{'f',3},{'i',4}};
    for (size_t i = 0; i < 3; ++i) {
        put32(format12 + 16u + i * 12u, mappings[i][0]);
        put32(format12 + 20u + i * 12u, mappings[i][0]);
        put32(format12 + 24u + i * 12u, mappings[i][1]);
    }
    uint8_t *gsub = bytes + 400u;
    put32(gsub, UINT32_C(0x00010000));
    put16(gsub + 4u, 10);
    put16(gsub + 6u, 32);
    put16(gsub + 8u, 58);
    uint8_t *scripts = gsub + 10u;
    put16(scripts, 1);
    put32(scripts + 2u, NX_SFNT_TAG('l','a','t','n'));
    put16(scripts + 6u, 8);
    uint8_t *script = scripts + 8u;
    put16(script, 4);
    uint8_t *lang = script + 4u;
    put16(lang + 2u, UINT16_MAX);
    put16(lang + 4u, 2);
    put16(lang + 6u, 0);
    put16(lang + 8u, 1);
    uint8_t *features = gsub + 32u;
    put16(features, 2);
    put32(features + 2u, NX_SFNT_TAG('c','c','m','p'));
    put16(features + 6u, 14);
    put32(features + 8u, NX_SFNT_TAG('l','i','g','a'));
    put16(features + 12u, 20);
    put16(features + 16u, 1);
    put16(features + 22u, 1);
    put16(features + 24u, 1);
    uint8_t *lookups = gsub + 58u;
    put16(lookups, 2);
    put16(lookups + 2u, 6);
    put16(lookups + 4u, 26);
    uint8_t *single = lookups + 6u;
    put16(single, 1);
    put16(single + 4u, 1);
    put16(single + 6u, 8);
    put16(single + 8u, 1);
    put16(single + 10u, 6);
    put16(single + 12u, 1);
    put16(single + 14u, 1);
    put16(single + 16u, 1);
    put16(single + 18u, 5);
    uint8_t *lookup4 = lookups + 26u;
    put16(lookup4, 4);
    put16(lookup4 + 4u, 1);
    put16(lookup4 + 6u, 8);
    uint8_t *ligsub = lookup4 + 8u;
    put16(ligsub, 1);
    put16(ligsub + 2u, 14);
    put16(ligsub + 4u, 1);
    put16(ligsub + 6u, 20);
    put16(ligsub + 14u, 1);
    put16(ligsub + 16u, 1);
    put16(ligsub + 18u, 3);
    put16(ligsub + 20u, 1);
    put16(ligsub + 22u, 4);
    put16(ligsub + 24u, 7);
    put16(ligsub + 26u, 2);
    put16(ligsub + 28u, 4);
}

static int shape(uint8_t bytes[522], const char *text, size_t length,
                 nx_shaped_glyph *glyphs, size_t *count) {
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, 522, NULL, &font) == NX_OK);
    nx_font_collection *collection = NULL;
    CHECK(nx_font_collection_create(NULL, &collection) == NX_OK);
    CHECK(nx_font_collection_add(collection, font) == NX_OK);
    nx_shape_run runs[8];
    size_t run_count = 0;
    uint8_t paragraph = 0;
    nx_result result = nx_shape_text(collection, text, length, NX_BIDI_LTR,
                                     20.0, glyphs, 8, runs, 8, count,
                                     &run_count, &paragraph);
    nx_font_collection_destroy(collection);
    nx_font_close(font);
    return result;
}

int main(void) {
    uint8_t bytes[522];
    fixture(bytes);
    nx_shaped_glyph glyphs[8] = {0};
    size_t count = 0;
    CHECK(shape(bytes, "Afi", 3, glyphs, &count) == NX_OK);
    CHECK(count == 2 && glyphs[0].glyph_index == 6 &&
          glyphs[1].glyph_index == 7);
    CHECK(glyphs[1].byte_start == 1 && glyphs[1].byte_end == 3);
    CHECK(glyphs[0].advance_x == 12.0 && glyphs[1].advance_x == 12.0);
    fixture(bytes);
    put32(bytes + 400u + 10u + 2u, NX_SFNT_TAG('D','F','L','T'));
    CHECK(shape(bytes, "A", 1, glyphs, &count) == NX_OK);
    CHECK(count == 1 && glyphs[0].glyph_index == 6);
    fixture(bytes);
    put32(bytes + 400u + 32u + 8u, NX_SFNT_TAG('d','l','i','g'));
    put16(bytes + 400u + 10u + 8u + 4u + 2u, 1); /* Required feature. */
    CHECK(shape(bytes, "fi", 2, glyphs, &count) == NX_OK);
    CHECK(count == 1 && glyphs[0].glyph_index == 7);
    fixture(bytes);
    put32(bytes + 400u + 32u + 8u, NX_SFNT_TAG('d','l','i','g'));
    CHECK(shape(bytes, "fi", 2, glyphs, &count) == NX_OK);
    CHECK(count == 2 && glyphs[0].glyph_index == 3 &&
          glyphs[1].glyph_index == 4);
    fixture(bytes);
    put16(bytes + 400u + 58u + 26u + 2u, 8);
    CHECK(shape(bytes, "fi", 2, glyphs, &count) == NX_OK);
    CHECK(count == 2 && glyphs[0].glyph_index == 3);
    fixture(bytes);
    put16(bytes + 400u + 10u + 8u + 4u + 4u, 1); /* One active feature. */
    put16(bytes + 400u + 58u, 1); /* One lookup. */
    uint8_t *single = bytes + 400u + 58u + 6u;
    put16(single + 8u, 2);  /* Single substitution format 2. */
    put16(single + 10u, 8); /* Coverage follows the replacement array. */
    put16(single + 12u, 1);
    put16(single + 14u, 6);
    put16(single + 16u, 2); /* Coverage format 2. */
    put16(single + 18u, 1);
    put16(single + 20u, 5);
    put16(single + 22u, 5);
    put16(single + 24u, 0);
    CHECK(shape(bytes, "A", 1, glyphs, &count) == NX_OK);
    CHECK(count == 1 && glyphs[0].glyph_index == 6);
    fixture(bytes);
    put16(bytes + 400u + 58u + 6u + 10u, 0xffffu);
    glyphs[0].glyph_index = 99;
    count = 99;
    CHECK(shape(bytes, "Afi", 3, glyphs, &count) == NX_ERROR_ARGUMENT);
    CHECK(glyphs[0].glyph_index == 99 && count == 99);
    fixture(bytes);
    put16(bytes + 400u + 58u + 26u + 8u + 26u, 0);
    CHECK(shape(bytes, "fi", 2, glyphs, &count) == NX_ERROR_ARGUMENT);
    CHECK(glyphs[0].glyph_index == 99 && count == 99);
    fixture(bytes);
    put16(bytes + 400u + 4u, 0); /* Script list must not alias header. */
    CHECK(shape(bytes, "A", 1, glyphs, &count) == NX_ERROR_ARGUMENT);
    CHECK(glyphs[0].glyph_index == 99 && count == 99);
    puts("GSUB tests passed");
    return 0;
}
