#include "nexaui/nexaui.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", \
    __FILE__, __LINE__, #x); return 1; } } while (0)

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

static void fixture(uint8_t bytes[512]) {
    memset(bytes, 0, 512);
    put32(bytes, UINT32_C(0x00010000));
    put16(bytes + 4u, 7);
    const uint32_t tags[7] = {
        NX_SFNT_TAG('c','m','a','p'), NX_SFNT_TAG('G','P','O','S'),
        NX_SFNT_TAG('h','e','a','d'), NX_SFNT_TAG('h','h','e','a'),
        NX_SFNT_TAG('h','m','t','x'), NX_SFNT_TAG('k','e','r','n'),
        NX_SFNT_TAG('m','a','x','p')
    };
    const uint32_t offsets[7] = {280,368,124,212,248,344,180};
    const uint32_t lengths[7] = {64,144,54,36,32,24,32};
    for (size_t i = 0; i < 7; ++i) {
        put32(bytes + 12u + 16u * i, tags[i]);
        put32(bytes + 20u + 16u * i, offsets[i]);
        put32(bytes + 24u + 16u * i, lengths[i]);
    }
    put32(bytes + 124u, UINT32_C(0x00010000));
    put32(bytes + 136u, UINT32_C(0x5f0f3cf5));
    put16(bytes + 142u, 1000);
    put32(bytes + 180u, UINT32_C(0x00010000));
    put16(bytes + 184u, 8);
    put32(bytes + 212u, UINT32_C(0x00010000));
    put16(bytes + 216u, 800);
    put16(bytes + 218u, (uint16_t)-200);
    put16(bytes + 246u, 8);
    for (size_t i = 0; i < 8; ++i) put16(bytes + 248u + 4u * i, 600);
    uint8_t *cmap = bytes + 280u;
    put16(cmap + 2u, 1);
    put16(cmap + 4u, 3);
    put16(cmap + 6u, 10);
    put32(cmap + 8u, 12);
    uint8_t *sub = cmap + 12u;
    put16(sub, 12);
    put32(sub + 4u, 52);
    put32(sub + 12u, 3);
    const uint32_t mappings[3][2] = {{'A',2},{'T',4},{'V',3}};
    for (size_t i = 0; i < 3; ++i) {
        put32(sub + 16u + 12u * i, mappings[i][0]);
        put32(sub + 20u + 12u * i, mappings[i][0]);
        put32(sub + 24u + 12u * i, mappings[i][1]);
    }
    uint8_t *kern = bytes + 344u;
    put16(kern + 2u, 1);
    put16(kern + 6u, 20);
    put16(kern + 8u, 1);
    put16(kern + 10u, 1);
    put16(kern + 18u, 2);
    put16(kern + 20u, 3);
    put16(kern + 22u, (uint16_t)-100);
    uint8_t *gpos = bytes + 368u;
    put32(gpos, UINT32_C(0x00010000));
    put16(gpos + 4u, 10);
    put16(gpos + 6u, 32);
    put16(gpos + 8u, 58);
    put16(gpos + 10u, 1);
    put32(gpos + 12u, NX_SFNT_TAG('l','a','t','n'));
    put16(gpos + 16u, 8);
    put16(gpos + 18u, 4);
    put16(gpos + 24u, UINT16_MAX);
    put16(gpos + 26u, 2);
    put16(gpos + 28u, 0);
    put16(gpos + 30u, 1);
    put16(gpos + 32u, 2);
    put32(gpos + 34u, NX_SFNT_TAG('p','a','l','t'));
    put16(gpos + 38u, 14);
    put32(gpos + 40u, NX_SFNT_TAG('k','e','r','n'));
    put16(gpos + 44u, 20);
    put16(gpos + 48u, 1);
    put16(gpos + 54u, 1);
    put16(gpos + 56u, 1);
    put16(gpos + 58u, 2);
    put16(gpos + 60u, 6);
    put16(gpos + 62u, 30);
    uint8_t *single_lookup = gpos + 64u;
    put16(single_lookup, 1);
    put16(single_lookup + 4u, 1);
    put16(single_lookup + 6u, 8);
    uint8_t *single = single_lookup + 8u;
    put16(single, 1);
    put16(single + 2u, 10);
    put16(single + 4u, 3);
    put16(single + 6u, 50);
    put16(single + 8u, (uint16_t)-100);
    put16(single + 10u, 1);
    put16(single + 12u, 1);
    put16(single + 14u, 2);
    uint8_t *pair_lookup = gpos + 88u;
    put16(pair_lookup, 2);
    put16(pair_lookup + 4u, 1);
    put16(pair_lookup + 6u, 8);
    uint8_t *pair = pair_lookup + 8u;
    put16(pair, 1);
    put16(pair + 2u, 18);
    put16(pair + 4u, 4);
    put16(pair + 8u, 1);
    put16(pair + 10u, 12);
    put16(pair + 12u, 1);
    put16(pair + 14u, 3);
    put16(pair + 16u, (uint16_t)-50);
    put16(pair + 18u, 1);
    put16(pair + 20u, 1);
    put16(pair + 22u, 2);
}

static nx_result shape(uint8_t bytes[512], const char *text,
                       nx_shaped_glyph *glyphs, size_t *count) {
    nx_font *font = NULL;
    if (nx_font_open(bytes, 512, NULL, &font) != NX_OK)
        return NX_ERROR_ARGUMENT;
    nx_font_collection *collection = NULL;
    nx_result result = nx_font_collection_create(NULL, &collection);
    if (result == NX_OK) result = nx_font_collection_add(collection, font);
    if (result == NX_OK) {
        nx_shape_run runs[8];
        size_t run_count = 0;
        uint8_t paragraph = 0;
        result = nx_shape_text(collection, text, strlen(text), NX_BIDI_LTR,
                               20.0, glyphs, 8, runs, 8, count,
                               &run_count, &paragraph);
    }
    nx_font_collection_destroy(collection);
    nx_font_close(font);
    return result;
}

int main(void) {
    uint8_t bytes[512];
    nx_shaped_glyph glyphs[8] = {0};
    size_t count = 0;
    fixture(bytes);
    CHECK(shape(bytes, "AV", glyphs, &count) == NX_OK);
    CHECK(count == 2 && glyphs[0].glyph_index == 2 &&
          glyphs[1].glyph_index == 3);
    CHECK(glyphs[0].offset_x == 1.0 && glyphs[0].offset_y == -2.0);
    CHECK(glyphs[0].advance_x == 11.0 && glyphs[1].advance_x == 12.0);
    fixture(bytes);
    uint8_t *pair_with_second = bytes + 368u + 96u;
    put16(pair_with_second + 2u, 20);
    put16(pair_with_second + 6u, 1); /* Position both glyphs. */
    put16(pair_with_second + 18u, 25);
    put16(pair_with_second + 20u, 1);
    put16(pair_with_second + 22u, 1);
    put16(pair_with_second + 24u, 2);
    CHECK(shape(bytes, "AV", glyphs, &count) == NX_OK);
    CHECK(glyphs[0].advance_x == 11.0 && glyphs[1].offset_x == 0.5);
    fixture(bytes);
    put32(bytes + 368u + 40u, NX_SFNT_TAG('d','l','i','g'));
    CHECK(shape(bytes, "AV", glyphs, &count) == NX_OK);
    CHECK(glyphs[0].advance_x == 10.0); /* Legacy kern fallback. */
    fixture(bytes);
    put32(bytes + 368u + 12u, NX_SFNT_TAG('D','F','L','T'));
    CHECK(shape(bytes, "A", glyphs, &count) == NX_OK);
    CHECK(glyphs[0].offset_x == 1.0);
    fixture(bytes);
    uint8_t *single = bytes + 368u + 72u;
    put16(single, 2);
    put16(single + 2u, 50);
    put16(single + 6u, 1);
    put16(single + 8u, 100);
    put16(single + 10u, (uint16_t)-50);
    put16(single + 50u, 1);
    put16(single + 52u, 1);
    put16(single + 54u, 2);
    CHECK(shape(bytes, "A", glyphs, &count) == NX_OK);
    CHECK(glyphs[0].offset_x == 2.0 && glyphs[0].offset_y == -1.0);
    fixture(bytes);
    uint8_t *pair = bytes + 368u + 96u;
    memset(pair, 0, 48u);
    put16(pair, 2);
    put16(pair + 2u, 24);
    put16(pair + 4u, 4);
    put16(pair + 8u, 30);
    put16(pair + 10u, 38);
    put16(pair + 12u, 2);
    put16(pair + 14u, 2);
    put16(pair + 22u, (uint16_t)-75); /* Class 1 + class 1. */
    put16(pair + 24u, 1);
    put16(pair + 26u, 1);
    put16(pair + 28u, 2);
    put16(pair + 30u, 1);
    put16(pair + 32u, 2);
    put16(pair + 34u, 1);
    put16(pair + 36u, 1);
    put16(pair + 38u, 2);
    put16(pair + 40u, 1);
    put16(pair + 42u, 3);
    put16(pair + 44u, 3);
    put16(pair + 46u, 1);
    CHECK(shape(bytes, "AV", glyphs, &count) == NX_OK);
    CHECK(glyphs[0].advance_x == 10.5);
    put16(pair + 44u, 2); /* Reversed class range. */
    glyphs[0].glyph_index = 99;
    count = 99;
    CHECK(shape(bytes, "AV", glyphs, &count) == NX_ERROR_ARGUMENT);
    CHECK(glyphs[0].glyph_index == 99 && count == 99);
    fixture(bytes);
    put16(bytes + 368u + 96u + 2u, UINT16_MAX);
    glyphs[0].glyph_index = 99;
    count = 99;
    CHECK(shape(bytes, "AV", glyphs, &count) == NX_ERROR_ARGUMENT);
    CHECK(glyphs[0].glyph_index == 99 && count == 99);
    fixture(bytes);
    put16(bytes + 368u + 4u, 0);
    CHECK(shape(bytes, "A", glyphs, &count) == NX_ERROR_ARGUMENT);
    CHECK(glyphs[0].glyph_index == 99 && count == 99);
    puts("GPOS tests passed");
    return 0;
}
