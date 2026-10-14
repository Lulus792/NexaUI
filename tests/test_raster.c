#include "nexaui/nexaui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static nx_glyph_outline_buffer outline_view(nx_glyph_point *points,
                                             size_t point_count,
                                             uint16_t *ends,
                                             size_t contour_count) {
    nx_glyph_outline_buffer outline = {0};
    outline.points = points;
    outline.point_count = outline.point_capacity = point_count;
    outline.contour_ends = ends;
    outline.contour_count = outline.contour_capacity = contour_count;
    return outline;
}

static int test_rectangle_and_headless(void) {
    nx_glyph_point points[] = {
        {0, 0, 1}, {4, 0, 1}, {4, 4, 1}, {0, 4, 1}
    };
    uint16_t ends[] = {3};
    nx_glyph_outline_buffer outline = outline_view(points, 4, ends, 1);
    uint8_t mask[24];
    memset(mask, 0x7f, sizeof(mask));
    CHECK(nx_rasterize_glyph_a8(&outline, 1.0, 0.0, 4.0,
                                 mask, 4, 4, 6, sizeof(mask), NULL) == NX_OK);
    for (size_t y = 0; y < 4u; ++y) {
        for (size_t x = 0; x < 4u; ++x) CHECK(mask[y * 6u + x] == 255);
        CHECK(mask[y * 6u + 4u] == 0x7f && mask[y * 6u + 5u] == 0x7f);
    }
    nx_command command = {0};
    command.type = NX_COMMAND_BITMAP;
    command.rect = (nx_rect){0, 0, 4, 4};
    command.color = (nx_color){1, 0, 0, 1};
    command.bitmap = (nx_bitmap_view){mask, 4, 4, 6, NX_BITMAP_A8};
    uint8_t rgba[4u * 4u * 4u] = {0};
    CHECK(nx_headless_render(&command, 1, rgba, 4, 4, 16,
                             (nx_color){0, 0, 0, 0}) == NX_OK);
    for (size_t i = 0; i < 16u; ++i)
        CHECK(rgba[i * 4u] == 255 && rgba[i * 4u + 1u] == 0 &&
              rgba[i * 4u + 2u] == 0 && rgba[i * 4u + 3u] == 255);
    return 0;
}

static int test_nonzero_hole_and_antialias(void) {
    nx_glyph_point points[] = {
        {0, 0, 1}, {4, 0, 1}, {4, 4, 1}, {0, 4, 1},
        {1, 1, 1}, {1, 3, 1}, {3, 3, 1}, {3, 1, 1}
    };
    uint16_t ends[] = {3, 7};
    nx_glyph_outline_buffer outline = outline_view(points, 8, ends, 2);
    uint8_t mask[16] = {0};
    CHECK(nx_rasterize_glyph_a8(&outline, 1.0, 0.0, 4.0,
                                 mask, 4, 4, 4, sizeof(mask), NULL) == NX_OK);
    CHECK(mask[0] == 255 && mask[3] == 255 && mask[12] == 255);
    CHECK(mask[5] == 0 && mask[6] == 0 &&
          mask[9] == 0 && mask[10] == 0);

    nx_glyph_point half_points[] = {
        {1, 0, 1}, {5, 0, 1}, {5, 4, 1}, {1, 4, 1}
    };
    uint16_t half_end[] = {3};
    outline = outline_view(half_points, 4, half_end, 1);
    uint8_t antialias[6] = {0};
    CHECK(nx_rasterize_glyph_a8(&outline, 0.5, 0.0, 2.0,
                                 antialias, 3, 2, 3,
                                 sizeof(antialias), NULL) == NX_OK);
    for (size_t y = 0; y < 2u; ++y) {
        CHECK(antialias[y * 3u] == 128);
        CHECK(antialias[y * 3u + 1u] == 255);
        CHECK(antialias[y * 3u + 2u] == 128);
    }
    return 0;
}

static int test_implicit_on_curve_points(void) {
    nx_glyph_point implicit_points[] = {
        {0, 0, 1}, {0, 4, 0}, {4, 4, 0}, {4, 0, 1}
    };
    nx_glyph_point explicit_points[] = {
        {0, 0, 1}, {0, 4, 0}, {2, 4, 1}, {4, 4, 0}, {4, 0, 1}
    };
    uint16_t implicit_end[] = {3}, explicit_end[] = {4};
    nx_glyph_outline_buffer implicit = outline_view(implicit_points, 4,
                                                    implicit_end, 1);
    nx_glyph_outline_buffer explicit = outline_view(explicit_points, 5,
                                                    explicit_end, 1);
    uint8_t mask_a[25] = {0}, mask_b[25] = {0};
    CHECK(nx_rasterize_glyph_a8(&implicit, 1.0, 0.0, 4.0,
                                 mask_a, 5, 5, 5, sizeof(mask_a), NULL) == NX_OK);
    CHECK(nx_rasterize_glyph_a8(&explicit, 1.0, 0.0, 4.0,
                                 mask_b, 5, 5, 5, sizeof(mask_b), NULL) == NX_OK);
    CHECK(memcmp(mask_a, mask_b, sizeof(mask_a)) == 0);
    int has_partial = 0, has_full = 0;
    for (size_t i = 0; i < sizeof(mask_a); ++i) {
        has_partial |= mask_a[i] > 0 && mask_a[i] < 255;
        has_full |= mask_a[i] == 255;
    }
    CHECK(has_partial && has_full);

    nx_glyph_point all_off[] = {
        {0, 0, 0}, {0, 4, 0}, {4, 4, 0}, {4, 0, 0}
    };
    nx_glyph_point implied_start[] = {
        {2, 0, 1}, {0, 0, 0}, {0, 2, 1}, {0, 4, 0},
        {2, 4, 1}, {4, 4, 0}, {4, 2, 1}, {4, 0, 0}
    };
    uint16_t all_off_end[] = {3}, implied_start_end[] = {7};
    implicit = outline_view(all_off, 4, all_off_end, 1);
    explicit = outline_view(implied_start, 8, implied_start_end, 1);
    CHECK(nx_rasterize_glyph_a8(&implicit, 1.0, 0.0, 4.0,
                                 mask_a, 5, 5, 5, sizeof(mask_a), NULL) == NX_OK);
    CHECK(nx_rasterize_glyph_a8(&explicit, 1.0, 0.0, 4.0,
                                 mask_b, 5, 5, 5, sizeof(mask_b), NULL) == NX_OK);
    CHECK(memcmp(mask_a, mask_b, sizeof(mask_a)) == 0);
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

static int test_invalid_and_atomic_failures(void) {
    nx_glyph_point points[] = {
        {0, 0, 1}, {2, 0, 1}, {2, 2, 1}, {0, 2, 1}
    };
    uint16_t ends[] = {3};
    nx_glyph_outline_buffer outline = outline_view(points, 4, ends, 1);
    uint8_t mask[4] = {0x7f, 0x7f, 0x7f, 0x7f};
    CHECK(nx_rasterize_glyph_a8(&outline, NAN, 0, 2,
                                 mask, 2, 2, 2, sizeof(mask), NULL) ==
          NX_ERROR_ARGUMENT);
    CHECK(nx_rasterize_glyph_a8(&outline, 1, 0, 2,
                                 mask, 2, 2, 2, 3, NULL) == NX_ERROR_ARGUMENT);
    CHECK(nx_rasterize_glyph_a8(&outline, 1, 0, 2,
                                 mask, 2, 2, 1, sizeof(mask), NULL) ==
          NX_ERROR_ARGUMENT);
    ends[0] = 4;
    CHECK(nx_rasterize_glyph_a8(&outline, 1, 0, 2,
                                 mask, 2, 2, 2, sizeof(mask), NULL) ==
          NX_ERROR_ARGUMENT);
    ends[0] = 3;
    CHECK(nx_rasterize_glyph_a8(&outline, 1, 0, 2,
                                 mask, 4097, 2, 4097, sizeof(mask), NULL) ==
          NX_ERROR_LIMIT);
    for (unsigned fail_after = 0; fail_after < 2u; ++fail_after) {
        tracker state = {0, 0, fail_after};
        nx_allocator allocator = {&state, tracked_allocate, tracked_deallocate};
        CHECK(nx_rasterize_glyph_a8(&outline, 1, 0, 2,
                                     mask, 2, 2, 2, sizeof(mask), &allocator) ==
              NX_ERROR_MEMORY);
        CHECK(state.frees == fail_after);
    }
    for (size_t i = 0; i < sizeof(mask); ++i) CHECK(mask[i] == 0x7f);
    return 0;
}

int main(void) {
    if (test_rectangle_and_headless()) return 1;
    if (test_nonzero_hole_and_antialias()) return 1;
    if (test_implicit_on_curve_points()) return 1;
    if (test_invalid_and_atomic_failures()) return 1;
    puts("raster tests passed");
    return 0;
}
