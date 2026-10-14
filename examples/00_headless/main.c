#include "nexaui/nexaui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define WIDTH 320u
#define HEIGHT 180u

static int draw(nx_context *ctx) {
    static const uint8_t icon[8 * 8] = {
        0, 0, 0, 255, 255, 0, 0, 0,
        0, 0, 0, 255, 255, 0, 0, 0,
        0, 0, 0, 255, 255, 0, 0, 0,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        0, 0, 0, 255, 255, 0, 0, 0,
        0, 0, 0, 255, 255, 0, 0, 0,
        0, 0, 0, 255, 255, 0, 0, 0
    };
    const nx_color panel = {0.12f, 0.15f, 0.21f, 1};
    const nx_color accent = {0.28f, 0.67f, 0.95f, 1};
    const nx_color track = {0.23f, 0.27f, 0.34f, 1};
    if (nx_begin_frame(ctx, WIDTH, HEIGHT, 1.0 / 60.0) != NX_OK) return 0;
    if (nx_fill_round_rect(ctx, (nx_rect){16, 16, 288, 148}, 12, panel) != NX_OK) return 0;
    if (nx_stroke_rect(ctx, (nx_rect){23, 23, 274, 134}, 1,
                       (nx_color){0.32f, 0.37f, 0.46f, 1}) != NX_OK) return 0;
    if (nx_fill_rect(ctx, (nx_rect){32, 34, 132, 8}, accent) != NX_OK) return 0;
    if (nx_draw_bitmap(ctx, (nx_rect){268, 31, 16, 16},
                       (nx_bitmap_view){icon, 8, 8, 8, NX_BITMAP_A8},
                       accent) != NX_OK) return 0;
    if (nx_fill_rect(ctx, (nx_rect){32, 66, 256, 14}, track) != NX_OK) return 0;
    if (nx_fill_rect(ctx, (nx_rect){32, 66, 190, 14}, accent) != NX_OK) return 0;
    if (nx_push_clip(ctx, (nx_rect){32, 102, 256, 42}) != NX_OK) return 0;
    for (int i = 0; i < 20; ++i) {
        float h = (float)((i * 29 + 11) % 37) + 8.0f;
        if (nx_fill_rect(ctx, (nx_rect){32.0f + (float)i * 13.0f, 144.0f - h, 8, h},
                         accent) != NX_OK) return 0;
    }
    if (nx_draw_line(ctx, (nx_point){38, 135}, (nx_point){275, 110}, 2,
                     (nx_color){1, 0.72f, 0.29f, 1}) != NX_OK) return 0;
    if (nx_fill_triangle(ctx, (nx_point){274, 105}, (nx_point){282, 109},
                         (nx_point){274, 113}, accent) != NX_OK) return 0;
    if (nx_pop_clip(ctx) != NX_OK) return 0;
    return nx_end_frame(ctx) == NX_OK;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "nexaui.ppm";
    nx_context *ctx = NULL;
    uint8_t *pixels = NULL;
    FILE *file = NULL;
    int status = 1;
    if (nx_context_create(NULL, &ctx) != NX_OK || !draw(ctx)) goto cleanup;
    const nx_command *commands = NULL;
    size_t count = 0;
    if (nx_frame_commands(ctx, &commands, &count) != NX_OK) goto cleanup;
    pixels = (uint8_t *)malloc((size_t)WIDTH * HEIGHT * 4u);
    if (!pixels) goto cleanup;
    if (nx_headless_render(commands, count, pixels, WIDTH, HEIGHT, WIDTH * 4u,
                           (nx_color){0.06f, 0.08f, 0.12f, 1}) != NX_OK) goto cleanup;
    file = fopen(path, "wb");
    if (!file) goto cleanup;
    if (fprintf(file, "P6\n%u %u\n255\n", WIDTH, HEIGHT) < 0) goto cleanup;
    for (size_t i = 0; i < (size_t)WIDTH * HEIGHT; ++i)
        if (fwrite(pixels + i * 4u, 1, 3, file) != 3) goto cleanup;
    status = 0;
cleanup:
    if (file && fclose(file) != 0) status = 1;
    free(pixels);
    nx_context_destroy(ctx);
    if (status) fprintf(stderr, "Failed to write %s\n", path);
    return status;
}
