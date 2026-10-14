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

static void fixture(uint8_t bytes[376], int alternate) {
    memset(bytes, 0, 376);
    put32(bytes, UINT32_C(0x00010000));
    put16(bytes + 4u, 6);
    const uint32_t tags[6] = {
        NX_SFNT_TAG('c','m','a','p'), NX_SFNT_TAG('h','e','a','d'),
        NX_SFNT_TAG('h','h','e','a'), NX_SFNT_TAG('h','m','t','x'),
        NX_SFNT_TAG('k','e','r','n'), NX_SFNT_TAG('m','a','x','p')
    };
    const uint32_t offsets[6] = {264,108,196,232,352,164};
    const uint32_t lengths[6] = {88,54,36,32,24,32};
    for (size_t i = 0; i < 6; ++i) {
        put32(bytes + 12u + 16u * i, tags[i]);
        put32(bytes + 20u + 16u * i, offsets[i]);
        put32(bytes + 24u + 16u * i, lengths[i]);
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
    put16(bytes + 236u, 300); /* Space in the primary font. */
    if (alternate) put16(bytes + 244u, 200); /* Accent glyph. */
    put16(bytes + 266u, 1);
    put16(bytes + 268u, 3);
    put16(bytes + 270u, 10);
    put32(bytes + 272u, 12);
    uint8_t *sub = bytes + 276u;
    put16(sub, 12);
    put32(sub + 4u, alternate ? 52u : 64u);
    put32(sub + 12u, alternate ? 3u : 4u);
    const uint32_t primary[4][2] = {
        {0x0020,1},{0x0041,2},{0x0056,3},{0x05d0,5}
    };
    const uint32_t secondary[3][2] = {
        {0x0041,2},{0x0301,3},{0x03a9,4}
    };
    for (size_t i = 0; i < (alternate ? 3u : 4u); ++i) {
        uint32_t scalar = alternate ? secondary[i][0] : primary[i][0];
        uint32_t glyph = alternate ? secondary[i][1] : primary[i][1];
        put32(sub + 16u + i * 12u, scalar);
        put32(sub + 20u + i * 12u, scalar);
        put32(sub + 24u + i * 12u, glyph);
    }
    if (!alternate) {
        put16(bytes + 354u, 1);
        put16(bytes + 358u, 20);
        put16(bytes + 360u, 1);
        put16(bytes + 362u, 1);
        put16(bytes + 370u, 2);
        put16(bytes + 372u, 3);
        put16(bytes + 374u, (uint16_t)-100);
    }
}

int main(void) {
    uint8_t primary[376], secondary[376];
    fixture(primary, 0);
    fixture(secondary, 1);
    nx_font *a = NULL, *b = NULL;
    CHECK(nx_font_open(primary, sizeof(primary), NULL, &a) == NX_OK);
    CHECK(nx_font_open(secondary, sizeof(secondary), NULL, &b) == NX_OK);
    nx_font_collection *collection = NULL;
    CHECK(nx_font_collection_create(NULL, &collection) == NX_OK);
    CHECK(nx_font_collection_add(collection, a) == NX_OK);
    CHECK(nx_font_collection_add(collection, b) == NX_OK);
    nx_shaped_glyph glyphs[8] = {0};
    nx_shape_run runs[8] = {0};
    size_t glyph_count = 0, run_count = 0;
    uint8_t paragraph = 255;
    CHECK(nx_shape_text(collection, "AV", 2, NX_BIDI_AUTO, 20.0,
                        NULL, 0, NULL, 0, &glyph_count, &run_count,
                        &paragraph) == NX_ERROR_LIMIT);
    CHECK(glyph_count == 2 && run_count == 1 && paragraph == 0);
    CHECK(nx_shape_text(collection, "AV", 2, NX_BIDI_AUTO, 20.0,
                        glyphs, 8, runs, 8, &glyph_count, &run_count,
                        &paragraph) == NX_OK);
    CHECK(glyphs[0].glyph_index == 2 && glyphs[1].glyph_index == 3);
    CHECK(glyphs[0].advance_x == 10.0 && glyphs[1].advance_x == 12.0);
    CHECK(runs[0].script_tag == NX_SFNT_TAG('L','a','t','n'));
    glyphs[0].glyph_index = 99;
    runs[0].glyph_count = 99;
    CHECK(nx_shape_text(collection, "AV", 2, NX_BIDI_AUTO, 20.0,
                        glyphs, 1, runs, 1, &glyph_count, &run_count,
                        &paragraph) == NX_ERROR_LIMIT);
    CHECK(glyph_count == 2 && run_count == 1 &&
          glyphs[0].glyph_index == 99 && runs[0].glyph_count == 99);
    CHECK(nx_shape_text(collection, "A\xcc\x81", 3, NX_BIDI_LTR, 20.0,
                        glyphs, 8, runs, 8, &glyph_count, &run_count,
                        &paragraph) == NX_OK);
    CHECK(glyph_count == 2 && run_count == 1);
    CHECK(glyphs[0].font == b && glyphs[1].font == b);
    CHECK(glyphs[1].advance_x == 0.0 && glyphs[1].offset_x == -8.0);
    CHECK(glyphs[1].cluster_byte_start == 0 && glyphs[1].script_tag ==
          NX_SFNT_TAG('L','a','t','n'));
    CHECK(nx_shape_text(collection, "A\xd7\x90\xce\xa9", 5,
                        NX_BIDI_LTR, 20.0, glyphs, 8, runs, 8,
                        &glyph_count, &run_count, &paragraph) == NX_OK);
    CHECK(glyph_count == 3 && run_count == 3);
    CHECK(glyphs[0].bidi_level == 0 && glyphs[1].bidi_level == 1 &&
          glyphs[2].bidi_level == 0);
    CHECK(runs[0].script_tag == NX_SFNT_TAG('L','a','t','n') &&
          runs[1].script_tag == NX_SFNT_TAG('H','e','b','r') &&
          runs[2].script_tag == NX_SFNT_TAG('G','r','e','k'));
    CHECK(glyphs[0].font == a && glyphs[1].font == a &&
          glyphs[2].font == b);
    CHECK(nx_shape_text(collection, "A\xef\xb8\x8f", 4,
                        NX_BIDI_LTR, 20.0, glyphs, 8, runs, 8,
                        &glyph_count, &run_count, &paragraph) == NX_OK);
    CHECK(glyph_count == 1 && glyphs[0].scalar == 'A');
    CHECK(nx_shape_text(collection, "Z", 1, NX_BIDI_LTR, 20.0,
                        glyphs, 8, runs, 8, &glyph_count, &run_count,
                        &paragraph) == NX_OK);
    CHECK(glyph_count == 1 && glyphs[0].font == a &&
          glyphs[0].glyph_index == 0);
    glyphs[0].glyph_index = 99;
    glyph_count = run_count = 99;
    paragraph = 99;
    CHECK(nx_shape_text(collection, "\xc0\xaf", 2, NX_BIDI_AUTO, 20.0,
                        glyphs, 8, runs, 8, &glyph_count, &run_count,
                        &paragraph) == NX_ERROR_ARGUMENT);
    CHECK(glyphs[0].glyph_index == 99 && glyph_count == 99 &&
          run_count == 99 && paragraph == 99);
    nx_font_collection_destroy(collection);
    nx_font_close(b);
    nx_font_close(a);
    puts("shape tests passed");
    return 0;
}
