#include "nexaui/nexaui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static nx_color red(void) { return (nx_color){1, 0, 0, 1}; }
static nx_color clear(void) { return (nx_color){0, 0, 0, 0}; }

static int painted(const uint8_t *pixels, unsigned width, unsigned x, unsigned y) {
    return pixels[(y * width + x) * 4u + 3u] != 0;
}

static int test_stroke_and_round_rect(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 5, 5, 0) == NX_OK);
    CHECK(nx_stroke_rect(ctx, (nx_rect){0, 0, 5, 5}, 1, red()) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    const nx_command *commands;
    size_t count;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count == 1);
    uint8_t pixels[5 * 5 * 4] = {0};
    CHECK(nx_headless_render(commands, count, pixels, 5, 5, 20, clear()) == NX_OK);
    CHECK(painted(pixels, 5, 0, 0) && painted(pixels, 5, 4, 4));
    CHECK(!painted(pixels, 5, 2, 2) && !painted(pixels, 5, 1, 1));
    CHECK(nx_begin_frame(ctx, 5, 5, 0) == NX_OK);
    CHECK(nx_fill_round_rect(ctx, (nx_rect){0, 0, 5, 5}, 2, red()) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count == 1);
    CHECK(nx_headless_render(commands, count, pixels, 5, 5, 20, clear()) == NX_OK);
    CHECK(!painted(pixels, 5, 0, 0) && !painted(pixels, 5, 4, 4));
    CHECK(painted(pixels, 5, 1, 0) && painted(pixels, 5, 2, 0));
    CHECK(painted(pixels, 5, 2, 2));
    nx_context_destroy(ctx);
    return 0;
}

static int test_line_clip_and_degenerate(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 5, 5, 0) == NX_OK);
    CHECK(nx_push_clip(ctx, (nx_rect){2, 0, 2, 5}) == NX_OK);
    CHECK(nx_draw_line(ctx, (nx_point){1.5f, 2.5f},
                       (nx_point){3.5f, 2.5f}, 1, red()) == NX_OK);
    CHECK(nx_pop_clip(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    const nx_command *commands;
    size_t count;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count == 3);
    uint8_t pixels[5 * 5 * 4] = {0};
    CHECK(nx_headless_render(commands, count, pixels, 5, 5, 20, clear()) == NX_OK);
    CHECK(!painted(pixels, 5, 1, 2));
    CHECK(painted(pixels, 5, 2, 2) && painted(pixels, 5, 3, 2));
    CHECK(!painted(pixels, 5, 2, 1) && !painted(pixels, 5, 4, 2));
    CHECK(nx_begin_frame(ctx, 5, 5, 0) == NX_OK);
    CHECK(nx_draw_line(ctx, (nx_point){2.5f, 2.5f},
                       (nx_point){2.5f, 2.5f}, 1, red()) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK);
    CHECK(nx_headless_render(commands, count, pixels, 5, 5, 20, clear()) == NX_OK);
    CHECK(painted(pixels, 5, 2, 2) && !painted(pixels, 5, 1, 2));
    nx_context_destroy(ctx);
    return 0;
}

static int test_triangle_tiling(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 4, 4, 0) == NX_OK);
    nx_color half = {1, 0, 0, 0.5f};
    CHECK(nx_fill_triangle(ctx, (nx_point){0, 0}, (nx_point){4, 0},
                           (nx_point){0, 4}, half) == NX_OK);
    CHECK(nx_fill_triangle(ctx, (nx_point){0, 4}, (nx_point){4, 4},
                           (nx_point){4, 0}, half) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    const nx_command *commands;
    size_t count;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count == 2);
    uint8_t pixels[4 * 4 * 4] = {0};
    CHECK(nx_headless_render(commands, count, pixels, 4, 4, 16, clear()) == NX_OK);
    for (unsigned i = 0; i < 16; ++i) {
        CHECK(pixels[i * 4u] == 255 && pixels[i * 4u + 3u] == 128);
    }
    nx_context_destroy(ctx);
    return 0;
}

static int test_invalid_shapes_are_atomic(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 2, 2, 0) == NX_OK);
    CHECK(nx_stroke_rect(ctx, (nx_rect){0, 0, 2, 2}, 0, red()) == NX_ERROR_ARGUMENT);
    CHECK(nx_fill_round_rect(ctx, (nx_rect){0, 0, 2, 2}, NAN, red()) == NX_ERROR_ARGUMENT);
    CHECK(nx_draw_line(ctx, (nx_point){NAN, 0}, (nx_point){1, 1}, 1,
                       red()) == NX_ERROR_ARGUMENT);
    CHECK(nx_fill_triangle(ctx, (nx_point){0, 0}, (nx_point){1, INFINITY},
                           (nx_point){1, 1}, red()) == NX_ERROR_ARGUMENT);
    CHECK(nx_end_frame(ctx) == NX_OK);
    const nx_command *commands;
    size_t count;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count == 0);
    nx_command malformed = {0};
    malformed.type = NX_COMMAND_LINE;
    malformed.color = red();
    malformed.thickness = -1;
    uint8_t pixels[16];
    memset(pixels, 17, sizeof(pixels));
    CHECK(nx_headless_render(&malformed, 1, pixels, 2, 2, 8,
                             clear()) == NX_ERROR_COMMAND);
    for (unsigned i = 0; i < sizeof(pixels); ++i) CHECK(pixels[i] == 17);
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_stroke_and_round_rect()) return 1;
    if (test_line_clip_and_degenerate()) return 1;
    if (test_triangle_tiling()) return 1;
    if (test_invalid_shapes_are_atomic()) return 1;
    puts("shape tests passed");
    return 0;
}
