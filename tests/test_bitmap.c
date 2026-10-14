#include "nexaui/nexaui.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static int pixel_is(const uint8_t *pixels, unsigned width, unsigned x, unsigned y,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    const uint8_t *p = pixels + ((size_t)y * width + x) * 4u;
    return p[0] == r && p[1] == g && p[2] == b && p[3] == a;
}

static int test_rgba_sampling_and_borrowing(void) {
    uint8_t source[24] = {
        255, 0, 0, 255, 0, 255, 0, 128, 9, 9, 9, 9,
        0, 0, 255, 255, 255, 255, 0, 0, 9, 9, 9, 9
    };
    nx_bitmap_view bitmap = {source, 2, 2, 12, NX_BITMAP_RGBA8};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 4, 4, 0) == NX_OK);
    CHECK(nx_draw_bitmap(ctx, (nx_rect){0, 0, 4, 4}, bitmap,
                          (nx_color){1, 1, 1, 1}) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    const nx_command *commands;
    size_t count;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count == 1);
    CHECK(commands[0].bitmap.pixels == source);
    source[0] = 200; /* The command borrows pixels through rendering. */
    uint8_t pixels[4 * 4 * 4] = {0};
    CHECK(nx_headless_render(commands, count, pixels, 4, 4, 16,
                             (nx_color){0, 0, 0, 1}) == NX_OK);
    CHECK(pixel_is(pixels, 4, 0, 0, 200, 0, 0, 255));
    CHECK(pixel_is(pixels, 4, 1, 1, 200, 0, 0, 255));
    CHECK(pixel_is(pixels, 4, 2, 0, 0, 128, 0, 255));
    CHECK(pixel_is(pixels, 4, 0, 3, 0, 0, 255, 255));
    CHECK(pixel_is(pixels, 4, 3, 3, 0, 0, 0, 255));
    nx_context_destroy(ctx);
    return 0;
}

static int test_mask_tint_and_clip(void) {
    uint8_t source[6] = {0, 255, 99, 128, 64, 99};
    nx_bitmap_view bitmap = {source, 2, 2, 3, NX_BITMAP_A8};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 2, 2, 0) == NX_OK);
    CHECK(nx_push_clip(ctx, (nx_rect){1, 0, 1, 2}) == NX_OK);
    CHECK(nx_draw_bitmap(ctx, (nx_rect){0, 0, 2, 2}, bitmap,
                          (nx_color){1, 0, 0, 0.5f}) == NX_OK);
    CHECK(nx_pop_clip(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    const nx_command *commands;
    size_t count;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count == 3);
    uint8_t pixels[16] = {0};
    CHECK(nx_headless_render(commands, count, pixels, 2, 2, 8,
                             (nx_color){0, 0, 0, 0}) == NX_OK);
    CHECK(pixel_is(pixels, 2, 0, 0, 0, 0, 0, 0));
    CHECK(pixel_is(pixels, 2, 0, 1, 0, 0, 0, 0));
    CHECK(pixel_is(pixels, 2, 1, 0, 255, 0, 0, 128));
    CHECK(pixel_is(pixels, 2, 1, 1, 255, 0, 0, 32));
    nx_context_destroy(ctx);
    return 0;
}

static int test_invalid_bitmap_is_atomic(void) {
    uint8_t source[4] = {255, 0, 0, 255};
    nx_bitmap_view bitmap = {source, 1, 1, 4, NX_BITMAP_RGBA8};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 1, 1, 0) == NX_OK);
    bitmap.pixels = NULL;
    CHECK(nx_draw_bitmap(ctx, (nx_rect){0, 0, 1, 1}, bitmap,
                          (nx_color){1, 1, 1, 1}) == NX_ERROR_ARGUMENT);
    bitmap.pixels = source;
    bitmap.stride_bytes = 3;
    CHECK(nx_draw_bitmap(ctx, (nx_rect){0, 0, 1, 1}, bitmap,
                          (nx_color){1, 1, 1, 1}) == NX_ERROR_ARGUMENT);
    bitmap.stride_bytes = 4;
    bitmap.format = (nx_bitmap_format)99;
    CHECK(nx_draw_bitmap(ctx, (nx_rect){0, 0, 1, 1}, bitmap,
                          (nx_color){1, 1, 1, 1}) == NX_ERROR_ARGUMENT);
    CHECK(nx_end_frame(ctx) == NX_OK);
    const nx_command *commands;
    size_t count;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count == 0);
    nx_command malformed = {0};
    malformed.type = NX_COMMAND_BITMAP;
    malformed.rect = (nx_rect){0, 0, 1, 1};
    malformed.color = (nx_color){1, 1, 1, 1};
    malformed.bitmap = bitmap;
    uint8_t pixel[4] = {1, 2, 3, 4};
    CHECK(nx_headless_render(&malformed, 1, pixel, 1, 1, 4,
                             (nx_color){0, 0, 0, 0}) == NX_ERROR_COMMAND);
    CHECK(memcmp(pixel, (uint8_t[]){1, 2, 3, 4}, 4) == 0);
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_rgba_sampling_and_borrowing()) return 1;
    if (test_mask_tint_and_clip()) return 1;
    if (test_invalid_bitmap_is_atomic()) return 1;
    puts("bitmap tests passed");
    return 0;
}
