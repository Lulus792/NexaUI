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

static size_t fixture(uint8_t bytes[288], int long_loca, int repeated_flags) {
    memset(bytes, 0, 288);
    size_t glyf_offset = long_loca ? 252u : 248u;
    size_t glyf_length = repeated_flags ? 28u : 20u;
    put32(bytes, UINT32_C(0x00010000));
    put16(bytes + 4u, 6);
    record(bytes, 0, NX_SFNT_TAG('h', 'e', 'a', 'd'), 108, 54);
    record(bytes, 1, NX_SFNT_TAG('m', 'a', 'x', 'p'), 164, 32);
    record(bytes, 2, NX_SFNT_TAG('h', 'h', 'e', 'a'), 196, 36);
    record(bytes, 3, NX_SFNT_TAG('h', 'm', 't', 'x'), 232, 8);
    record(bytes, 4, NX_SFNT_TAG('l', 'o', 'c', 'a'), 240,
           long_loca ? 12 : 6);
    record(bytes, 5, NX_SFNT_TAG('g', 'l', 'y', 'f'),
           (uint32_t)glyf_offset, (uint32_t)glyf_length);
    put32(bytes + 108, UINT32_C(0x00010000));
    put32(bytes + 120, UINT32_C(0x5f0f3cf5));
    put16(bytes + 126, 1000);
    put16(bytes + 158, (uint16_t)long_loca);
    put32(bytes + 164, UINT32_C(0x00010000));
    put16(bytes + 168, 2);
    put32(bytes + 196, UINT32_C(0x00010000));
    put16(bytes + 230, 2);
    put16(bytes + 232, 500);
    put16(bytes + 236, 500);
    if (long_loca) put32(bytes + 248, (uint32_t)glyf_length);
    else put16(bytes + 244, (uint16_t)(glyf_length / 2u));
    uint8_t *glyph = bytes + glyf_offset;
    put16(glyph, 1);
    put16(glyph + 6u, 100);
    put16(glyph + 8u, 100);
    put16(glyph + 10u, 2);
    if (repeated_flags) {
        glyph[14] = 0x09; /* On curve, repeat two additional flags. */
        glyph[15] = 2;
        put16(glyph + 16u, 0);
        put16(glyph + 18u, 100);
        put16(glyph + 20u, (uint16_t)-100);
        put16(glyph + 22u, 0);
        put16(glyph + 24u, 0);
        put16(glyph + 26u, 100);
    } else {
        glyph[14] = 0x31; /* (0,0), both coordinates unchanged. */
        glyph[15] = 0x33; /* Positive short x, unchanged y. */
        glyph[16] = 0x27; /* Negative short x, positive short y. */
        glyph[17] = 100;
        glyph[18] = 100;
        glyph[19] = 100;
    }
    return glyf_offset;
}

static void composite_fixture(uint8_t bytes[512]) {
    memset(bytes, 0, 512);
    put32(bytes, UINT32_C(0x00010000));
    put16(bytes + 4u, 6);
    record(bytes, 0, NX_SFNT_TAG('h', 'e', 'a', 'd'), 108, 54);
    record(bytes, 1, NX_SFNT_TAG('m', 'a', 'x', 'p'), 164, 32);
    record(bytes, 2, NX_SFNT_TAG('h', 'h', 'e', 'a'), 196, 36);
    record(bytes, 3, NX_SFNT_TAG('h', 'm', 't', 'x'), 232, 16);
    record(bytes, 4, NX_SFNT_TAG('l', 'o', 'c', 'a'), 248, 20);
    record(bytes, 5, NX_SFNT_TAG('g', 'l', 'y', 'f'), 272, 64);
    put32(bytes + 108, UINT32_C(0x00010000));
    put32(bytes + 120, UINT32_C(0x5f0f3cf5));
    put16(bytes + 126, 1000);
    put16(bytes + 158, 1);
    put32(bytes + 164, UINT32_C(0x00010000));
    put16(bytes + 168, 4);
    put32(bytes + 196, UINT32_C(0x00010000));
    put16(bytes + 230, 4);
    for (size_t i = 0; i < 4u; ++i) put16(bytes + 232u + i * 4u, 500);
    put32(bytes + 248u + 8u, 20);
    put32(bytes + 248u + 12u, 46);
    put32(bytes + 248u + 16u, 64);
    uint8_t *simple = bytes + 272u;
    put16(simple, 1);
    put16(simple + 6u, 100);
    put16(simple + 8u, 100);
    put16(simple + 10u, 2);
    simple[14] = 0x31;
    simple[15] = 0x33;
    simple[16] = 0x27;
    simple[17] = simple[18] = simple[19] = 100;
    uint8_t *composite = simple + 20u;
    put16(composite, UINT16_MAX);
    put16(composite + 6u, 300);
    put16(composite + 8u, 100);
    put16(composite + 10u, 0x0023); /* Words, XY, more components. */
    put16(composite + 12u, 1);
    put16(composite + 18u, 0x0003); /* Words, XY, final component. */
    put16(composite + 20u, 1);
    put16(composite + 22u, 200);
    uint8_t *nested = composite + 26u;
    put16(nested, UINT16_MAX);
    put16(nested + 2u, 0);
    put16(nested + 4u, 200);
    put16(nested + 6u, 150);
    put16(nested + 8u, 300);
    put16(nested + 10u, 0x0043); /* Words, XY, distinct x/y scales. */
    put16(nested + 12u, 2);
    put16(nested + 16u, 200);
    put16(nested + 18u, 8192); /* x scale: 1/2. */
    put16(nested + 20u, 16384); /* y scale: 1. */
    put32(bytes + 248u + 16u, 68);
    put32(bytes + 12u + 5u * 16u + 12u, 68);
}

static int test_composite_outlines(void) {
    uint8_t bytes[512];
    composite_fixture(bytes);
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    nx_glyph_outline_buffer outline = {0};
    CHECK(nx_font_get_glyph_outline(font, 2, &outline) == NX_OK);
    CHECK(outline.point_count == 6 && outline.contour_count == 2);
    nx_glyph_point points[6];
    uint16_t ends[2] = {UINT16_MAX, UINT16_MAX};
    outline.points = points;
    outline.point_capacity = 5;
    outline.contour_ends = ends;
    outline.contour_capacity = 2;
    CHECK(nx_font_get_glyph_outline(font, 2, &outline) == NX_ERROR_LIMIT);
    CHECK(ends[0] == UINT16_MAX && ends[1] == UINT16_MAX);
    outline.point_capacity = 6;
    CHECK(nx_font_get_glyph_outline(font, 2, &outline) == NX_OK);
    CHECK(ends[0] == 2 && ends[1] == 5);
    CHECK(points[0].x == 0 && points[0].y == 0);
    CHECK(points[1].x == 100 && points[1].y == 0);
    CHECK(points[2].x == 0 && points[2].y == 100);
    CHECK(points[3].x == 200 && points[3].y == 0);
    CHECK(points[4].x == 300 && points[4].y == 0);
    CHECK(points[5].x == 200 && points[5].y == 100);
    CHECK(nx_font_get_glyph_outline(font, 3, &outline) == NX_OK);
    CHECK(points[0].x == 0 && points[0].y == 200);
    CHECK(points[1].x == 50 && points[1].y == 200);
    CHECK(points[2].x == 0 && points[2].y == 300);
    CHECK(points[3].x == 100 && points[3].y == 200);
    CHECK(points[4].x == 150 && points[4].y == 200);
    nx_font_close(font);

    composite_fixture(bytes);
    put16(bytes + 272u + 20u + 18u, 0x0001); /* Align child point 0 to parent point 2. */
    put16(bytes + 272u + 20u + 22u, 2);
    font = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    outline = (nx_glyph_outline_buffer){0};
    CHECK(nx_font_get_glyph_outline(font, 2, &outline) == NX_OK);
    outline.points = points;
    outline.point_capacity = 6;
    outline.contour_ends = ends;
    outline.contour_capacity = 2;
    CHECK(nx_font_get_glyph_outline(font, 2, &outline) == NX_OK);
    CHECK(points[3].x == 0 && points[3].y == 100);
    CHECK(points[4].x == 100 && points[4].y == 100);
    nx_font_close(font);

    composite_fixture(bytes);
    put16(bytes + 272u + 20u + 18u, 0x0002); /* Signed byte XY arguments. */
    bytes[272u + 20u + 22u] = 20;
    bytes[272u + 20u + 23u] = (uint8_t)-10;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    outline = (nx_glyph_outline_buffer){0};
    CHECK(nx_font_get_glyph_outline(font, 2, &outline) == NX_OK);
    outline.points = points;
    outline.point_capacity = 6;
    outline.contour_ends = ends;
    outline.contour_capacity = 2;
    CHECK(nx_font_get_glyph_outline(font, 2, &outline) == NX_OK);
    CHECK(points[3].x == 20 && points[3].y == -10);
    nx_font_close(font);

    composite_fixture(bytes);
    put16(bytes + 272u + 20u + 26u + 10u, 0x0843);
    put16(bytes + 272u + 20u + 26u + 14u, 200); /* Scaled x offset. */
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    outline = (nx_glyph_outline_buffer){0};
    CHECK(nx_font_get_glyph_outline(font, 3, &outline) == NX_OK);
    outline.points = points;
    outline.point_capacity = 6;
    outline.contour_ends = ends;
    outline.contour_capacity = 2;
    CHECK(nx_font_get_glyph_outline(font, 3, &outline) == NX_OK);
    CHECK(points[0].x == 100 && points[0].y == 200);
    CHECK(points[4].x == 250 && points[4].y == 200);
    nx_font_close(font);

    composite_fixture(bytes);
    uint8_t *nested = bytes + 272u + 20u + 26u;
    put16(nested + 2u, (uint16_t)-100);
    put16(nested + 6u, 0);
    put16(nested + 10u, 0x0083);
    put16(nested + 18u, 0);      /* xx */
    put16(nested + 20u, 16384);  /* xy */
    put16(nested + 22u, (uint16_t)-16384); /* yx */
    put16(nested + 24u, 0);      /* yy */
    put32(bytes + 248u + 16u, 72);
    put32(bytes + 12u + 5u * 16u + 12u, 72);
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    outline = (nx_glyph_outline_buffer){0};
    CHECK(nx_font_get_glyph_outline(font, 3, &outline) == NX_OK);
    outline.points = points;
    outline.point_capacity = 6;
    outline.contour_ends = ends;
    outline.contour_capacity = 2;
    CHECK(nx_font_get_glyph_outline(font, 3, &outline) == NX_OK);
    CHECK(points[0].x == 0 && points[0].y == 200);
    CHECK(points[1].x == 0 && points[1].y == 300);
    CHECK(points[2].x == -100 && points[2].y == 200);
    nx_font_close(font);

    composite_fixture(bytes);
    uint8_t *composite = bytes + 272u + 20u;
    memmove(composite + 28u, composite + 26u, 22u);
    put16(composite + 18u, 0x0103); /* Composite has an empty instruction block. */
    put16(composite + 26u, 0);
    put32(bytes + 248u + 12u, 48);
    put32(bytes + 248u + 16u, 70);
    put32(bytes + 12u + 5u * 16u + 12u, 70);
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    outline = (nx_glyph_outline_buffer){0};
    CHECK(nx_font_get_glyph_outline(font, 2, &outline) == NX_OK);
    CHECK(outline.point_count == 6 && outline.contour_count == 2);
    nx_font_close(font);
    return 0;
}

static int composite_result(uint8_t bytes[512], uint16_t glyph,
                            nx_result expected) {
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, 512, NULL, &font) == NX_OK);
    nx_glyph_outline_buffer outline = {0};
    CHECK(nx_font_get_glyph_outline(font, glyph, &outline) == expected);
    nx_font_close(font);
    return 0;
}

static int test_malformed_composites(void) {
    uint8_t bytes[512];
    composite_fixture(bytes);
    put16(bytes + 272u + 20u + 12u, 2); /* Direct cycle. */
    CHECK(composite_result(bytes, 2, NX_ERROR_ARGUMENT) == 0);
    composite_fixture(bytes);
    put16(bytes + 272u + 20u + 12u, 3); /* Indirect cycle. */
    CHECK(composite_result(bytes, 2, NX_ERROR_ARGUMENT) == 0);
    composite_fixture(bytes);
    put16(bytes + 272u + 20u + 12u, 4); /* Child ID out of range. */
    CHECK(composite_result(bytes, 2, NX_ERROR_ARGUMENT) == 0);
    composite_fixture(bytes);
    put16(bytes + 272u + 20u + 18u, 0x0001); /* Invalid parent point. */
    put16(bytes + 272u + 20u + 22u, 3);
    CHECK(composite_result(bytes, 2, NX_ERROR_ARGUMENT) == 0);
    composite_fixture(bytes);
    put16(bytes + 272u + 20u + 18u, 0x0001); /* Invalid child point. */
    put16(bytes + 272u + 20u + 24u, 3);
    CHECK(composite_result(bytes, 2, NX_ERROR_ARGUMENT) == 0);
    composite_fixture(bytes);
    put16(bytes + 272u + 20u + 10u, 0x0001); /* First component needs XY. */
    CHECK(composite_result(bytes, 2, NX_ERROR_ARGUMENT) == 0);
    composite_fixture(bytes);
    put16(bytes + 272u + 20u + 18u, 0x0013); /* Reserved flag bit. */
    CHECK(composite_result(bytes, 2, NX_ERROR_ARGUMENT) == 0);
    composite_fixture(bytes);
    put16(bytes + 272u + 20u + 18u, 0x004b); /* Two scale formats. */
    CHECK(composite_result(bytes, 2, NX_ERROR_ARGUMENT) == 0);
    composite_fixture(bytes);
    put16(bytes + 272u + 20u + 18u, 0x1803); /* Conflicting offset flags. */
    CHECK(composite_result(bytes, 2, NX_ERROR_ARGUMENT) == 0);
    composite_fixture(bytes);
    put16(bytes + 272u + 20u + 18u, 0x0103); /* Truncated instructions. */
    put32(bytes + 248u + 12u, 48);
    CHECK(composite_result(bytes, 2, NX_ERROR_ARGUMENT) == 0);
    return 0;
}

static int test_composite_depth_limit(void) {
    uint8_t bytes[2048] = {0};
    put32(bytes, UINT32_C(0x00010000));
    put16(bytes + 4u, 6);
    record(bytes, 0, NX_SFNT_TAG('h', 'e', 'a', 'd'), 108, 54);
    record(bytes, 1, NX_SFNT_TAG('m', 'a', 'x', 'p'), 164, 32);
    record(bytes, 2, NX_SFNT_TAG('h', 'h', 'e', 'a'), 196, 36);
    record(bytes, 3, NX_SFNT_TAG('h', 'm', 't', 'x'), 232, 136);
    record(bytes, 4, NX_SFNT_TAG('l', 'o', 'c', 'a'), 368, 140);
    record(bytes, 5, NX_SFNT_TAG('g', 'l', 'y', 'f'), 512, 614);
    put32(bytes + 108, UINT32_C(0x00010000));
    put32(bytes + 120, UINT32_C(0x5f0f3cf5));
    put16(bytes + 126, 1000);
    put16(bytes + 158, 1);
    put32(bytes + 164, UINT32_C(0x00010000));
    put16(bytes + 168, 34);
    put32(bytes + 196, UINT32_C(0x00010000));
    put16(bytes + 230, 34);
    for (size_t i = 0; i < 34u; ++i)
        put16(bytes + 232u + i * 4u, 500);
    uint8_t *simple = bytes + 512u;
    put16(simple, 1);
    put16(simple + 6u, 100);
    put16(simple + 8u, 100);
    put16(simple + 10u, 2);
    simple[14] = 0x31;
    simple[15] = 0x33;
    simple[16] = 0x27;
    simple[17] = simple[18] = simple[19] = 100;
    put32(bytes + 368u + 4u, 20);
    for (size_t i = 1; i <= 33u; ++i) {
        uint8_t *component = simple + 20u + (i - 1u) * 18u;
        put16(component, UINT16_MAX);
        put16(component + 6u, 100);
        put16(component + 8u, 100);
        put16(component + 10u, 0x0003);
        put16(component + 12u, (uint16_t)(i - 1u));
        put32(bytes + 368u + (i + 1u) * 4u,
              (uint32_t)(20u + i * 18u));
    }
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    nx_glyph_outline_buffer outline = {0};
    CHECK(nx_font_get_glyph_outline(font, 31, &outline) == NX_OK);
    CHECK(outline.point_count == 3 && outline.contour_count == 1);
    CHECK(nx_font_get_glyph_outline(font, 32, &outline) == NX_ERROR_LIMIT);
    nx_font_close(font);
    return 0;
}

typedef struct allocation_tracker {
    unsigned calls, frees, fail_after;
} allocation_tracker;

static void *tracking_allocate(void *user, size_t size) {
    allocation_tracker *tracker = (allocation_tracker *)user;
    if (++tracker->calls > tracker->fail_after) return NULL;
    return malloc(size);
}

static void tracking_deallocate(void *user, void *pointer, size_t size) {
    allocation_tracker *tracker = (allocation_tracker *)user;
    (void)size;
    ++tracker->frees;
    free(pointer);
}

static int test_composite_allocator_failure(void) {
    uint8_t bytes[512];
    composite_fixture(bytes);
    for (unsigned fail_after = 2; fail_after <= 4; ++fail_after) {
        allocation_tracker tracker = {0, 0, fail_after};
        nx_allocator allocator = {&tracker, tracking_allocate, tracking_deallocate};
        nx_font *font = NULL;
        CHECK(nx_font_open(bytes, sizeof(bytes), &allocator, &font) == NX_OK);
        nx_glyph_point points[6];
        uint16_t ends[2] = {UINT16_MAX, UINT16_MAX};
        memset(points, 0x7f, sizeof(points));
        nx_glyph_outline_buffer outline = {0};
        outline.points = points;
        outline.point_capacity = 6;
        outline.contour_ends = ends;
        outline.contour_capacity = 2;
        nx_result result = nx_font_get_glyph_outline(font, 2, &outline);
        CHECK(result == (fail_after == 4 ? NX_OK : NX_ERROR_MEMORY));
        if (fail_after < 4)
            CHECK(ends[0] == UINT16_MAX && ends[1] == UINT16_MAX &&
                  points[0].x == INT32_C(0x7f7f7f7f));
        nx_font_close(font);
        CHECK(tracker.frees == fail_after);
    }
    return 0;
}

static int check_triangle(const uint8_t *bytes, size_t length) {
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, length, NULL, &font) == NX_OK);
    nx_glyph_outline_buffer outline = {0};
    CHECK(nx_font_get_glyph_outline(font, 0, &outline) == NX_OK);
    CHECK(outline.point_count == 0 && outline.contour_count == 0);
    CHECK(nx_font_get_glyph_outline(font, 1, &outline) == NX_OK);
    CHECK(outline.point_count == 3 && outline.contour_count == 1);
    CHECK(outline.bounds.x_min == 0 && outline.bounds.y_min == 0 &&
          outline.bounds.x_max == 100 && outline.bounds.y_max == 100);
    nx_glyph_point points[3];
    uint16_t ends[1] = {UINT16_MAX};
    memset(points, 0x7f, sizeof(points));
    outline.points = points;
    outline.point_capacity = 2;
    outline.contour_ends = ends;
    outline.contour_capacity = 1;
    CHECK(nx_font_get_glyph_outline(font, 1, &outline) == NX_ERROR_LIMIT);
    CHECK(outline.point_count == 3 && ends[0] == UINT16_MAX &&
          points[0].x == INT32_C(0x7f7f7f7f));
    outline.point_capacity = 3;
    CHECK(nx_font_get_glyph_outline(font, 1, &outline) == NX_OK);
    CHECK(ends[0] == 2);
    CHECK(points[0].x == 0 && points[0].y == 0 && points[0].on_curve);
    CHECK(points[1].x == 100 && points[1].y == 0 && points[1].on_curve);
    CHECK(points[2].x == 0 && points[2].y == 100 && points[2].on_curve);
    CHECK(nx_font_get_glyph_outline(font, 2, &outline) == NX_ERROR_ARGUMENT);
    nx_font_close(font);
    return 0;
}

static int test_valid_outlines(void) {
    uint8_t bytes[288];
    fixture(bytes, 0, 0);
    CHECK(check_triangle(bytes, sizeof(bytes)) == 0);
    fixture(bytes, 1, 1);
    CHECK(check_triangle(bytes, sizeof(bytes)) == 0);
    size_t offset = fixture(bytes, 0, 0);
    put16(bytes + offset, 0); /* A header-only, zero-contour glyph. */
    put16(bytes + 244, 5);
    put32(bytes + 12u + 5u * 16u + 12u, 10);
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    nx_glyph_outline_buffer outline = {0};
    CHECK(nx_font_get_glyph_outline(font, 1, &outline) == NX_OK);
    CHECK(outline.point_count == 0 && outline.contour_count == 0);
    nx_font_close(font);
    return 0;
}

static int result_for_glyph(uint8_t bytes[288], nx_result expected) {
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, 288, NULL, &font) == NX_OK);
    nx_glyph_outline_buffer outline = {0};
    CHECK(nx_font_get_glyph_outline(font, 1, &outline) == expected);
    nx_font_close(font);
    return 0;
}

static int test_malformed_and_composite(void) {
    uint8_t bytes[288];
    size_t offset = fixture(bytes, 0, 0);
    put16(bytes + 242, 11); /* loca[1] now follows loca[2]. */
    CHECK(result_for_glyph(bytes, NX_ERROR_ARGUMENT) == 0);
    offset = fixture(bytes, 0, 0);
    put16(bytes + 244, 20); /* End beyond glyf table. */
    CHECK(result_for_glyph(bytes, NX_ERROR_ARGUMENT) == 0);
    offset = fixture(bytes, 0, 0);
    put16(bytes + offset + 10u, 4); /* More points than data. */
    CHECK(result_for_glyph(bytes, NX_ERROR_ARGUMENT) == 0);
    offset = fixture(bytes, 1, 1);
    bytes[offset + 15u] = 3; /* Repeat exceeds point count. */
    CHECK(result_for_glyph(bytes, NX_ERROR_ARGUMENT) == 0);
    offset = fixture(bytes, 0, 0);
    put16(bytes + offset + 12u, 20); /* Instructions exceed glyph. */
    CHECK(result_for_glyph(bytes, NX_ERROR_ARGUMENT) == 0);
    offset = fixture(bytes, 0, 0);
    bytes[offset + 14u] = 0xb1; /* Reserved flag bit. */
    CHECK(result_for_glyph(bytes, NX_ERROR_ARGUMENT) == 0);
    offset = fixture(bytes, 0, 0);
    put16(bytes + offset, UINT16_MAX); /* Truncated composite glyph. */
    put16(bytes + 244u, 5);
    CHECK(result_for_glyph(bytes, NX_ERROR_ARGUMENT) == 0);
    return 0;
}

static int test_glyph_byte_mutations(void) {
    uint8_t bytes[288];
    size_t offset = fixture(bytes, 0, 0);
    for (size_t i = 0; i < 20u; ++i) {
        for (unsigned bit = 0; bit < 8u; ++bit) {
            uint8_t original = bytes[offset + i];
            bytes[offset + i] ^= (uint8_t)(1u << bit);
            nx_font *font = NULL;
            CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
            nx_glyph_outline_buffer outline = {0};
            nx_result result = nx_font_get_glyph_outline(font, 1, &outline);
            CHECK(result == NX_OK || result == NX_ERROR_ARGUMENT ||
                  result == NX_ERROR_UNSUPPORTED);
            nx_font_close(font);
            bytes[offset + i] = original;
        }
    }
    return 0;
}

int main(void) {
    if (test_valid_outlines()) return 1;
    if (test_composite_outlines()) return 1;
    if (test_malformed_composites()) return 1;
    if (test_composite_depth_limit()) return 1;
    if (test_composite_allocator_failure()) return 1;
    if (test_malformed_and_composite()) return 1;
    if (test_glyph_byte_mutations()) return 1;
    puts("glyf tests passed");
    return 0;
}
