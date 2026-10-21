#include "nexaui/nexaui.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #condition); \
    exit(EXIT_FAILURE); \
} } while (0)
#include <math.h>
#include <string.h>

nx_result nx_reference_headless_render(const nx_command *, size_t, uint8_t *, uint32_t,
                                      uint32_t, size_t, nx_color);
static uint32_t state = 0x81d423a9u;
static uint32_t random_bits(void) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
static float coordinate(void) {
    return ((float)(random_bits() % 2049u) - 512.0f) / 16;
}
static float channel(void) {
    return (float)(random_bits() % 256u) / 255;
}
static void compare(const nx_command *commands, size_t count, uint32_t w, uint32_t h,
                    nx_color clear, nx_result expected) {
    const size_t padding = 17, guard = 32;
    size_t stride = (size_t)w * 4u + padding, bytes = stride * h;
    uint8_t *before = malloc(bytes + 2 * guard), *candidate = malloc(bytes + 2 * guard);
    CHECK(before && candidate);
    memset(before, 0x95, bytes + 2 * guard);
    memset(candidate, 0x95, bytes + 2 * guard);
    CHECK(nx_reference_headless_render(commands, count, before + guard, w, h, stride, clear) == expected);
    CHECK(nx_headless_render(commands, count, candidate + guard, w, h, stride, clear) == expected);
    CHECK(!memcmp(before, candidate, bytes + 2 * guard));
    for (size_t i = 0; i < guard; ++i)
        CHECK(candidate[i] == 0x95 && candidate[guard + bytes + i] == 0x95);
    for (uint32_t y = 0; y < h; ++y)
        for (size_t i = (size_t)w * 4u; i < stride; ++i)
            CHECK(candidate[guard + (size_t)y * stride + i] == 0x95);
    if (expected != NX_OK)
        for (size_t i = 0; i < bytes + 2 * guard; ++i)
            CHECK(candidate[i] == 0x95);
    free(before);
    free(candidate);
}
int main(void) {
    nx_context *context = NULL;
    CHECK(nx_context_create(NULL, &context) == NX_OK);
    const float radii[] = {0, 0.125f, 0.5f, 1, 2, 7.75f, 8, 16, 1000000};
    const float extents[] = {0, 0.125f, 0.5f, 1, 2, 3, 16, 63.75f};
    const float scales[] = {1, 1.25f, 1.5f, 2};
    /* Tiny-radius rounding is part of the scalar reference's pixel behavior. */
    const float tiny[] = {0, 1.0e-45f, 1.0e-30f, 1.0e-17f, 6.0e-17f, 1.0e-16f,
                          1.0e-14f, 1.17549435e-38f};
    unsigned edge_cases = 0;
    for (unsigned i = 0; i < sizeof(tiny) / sizeof(tiny[0]); ++i)
        for (unsigned shift = 0; shift < 4; ++shift)
            for (unsigned scale = 0; scale < 4; ++scale) {
                CHECK(nx_begin_frame(context, 32, 32, 0) == NX_OK);
                nx_rect box = {(float)(shift % 2) * 0.5f, (float)(shift / 2) * 0.5f,
                               10 * scales[scale], 10 * scales[scale]};
                CHECK(nx_fill_round_rect(context, box, tiny[i] * scales[scale],
                                         (nx_color){1, 1, 1, 0.5f}) == NX_OK);
                CHECK(nx_end_frame(context) == NX_OK);
                const nx_command *edge_commands;
                size_t edge_count;
                CHECK(nx_frame_commands(context, &edge_commands, &edge_count) == NX_OK);
                compare(edge_commands, edge_count, 32, 32, (nx_color){0, 0, 0, 1}, NX_OK);
                ++edge_cases;
            }
    CHECK(edge_cases == 128);
    unsigned cases = 0;
    for (unsigned i = 0; i < 576; ++i) {
        CHECK(nx_begin_frame(context, 96, 80, 0) == NX_OK);
        float scale = scales[i % 4];
        nx_rect box = {((float)(i % 11) - 3) * 0.25f, ((float)(i % 7) - 2) * 0.5f,
                       extents[(i / 9) % 8] * scale, extents[(i / 72) % 8] * scale};
        CHECK(nx_push_clip(context, (nx_rect){0.25f, 0.5f, 79.5f, 67.25f}) == NX_OK);
        CHECK(nx_fill_round_rect(context, box, radii[i % 9] * scale,
                                 (nx_color){0.7f, 0.3f, 0.9f, (float)(i % 3) * 0.5f}) == NX_OK);
        CHECK(nx_pop_clip(context) == NX_OK && nx_end_frame(context) == NX_OK);
        const nx_command *commands;
        size_t count;
        CHECK(nx_frame_commands(context, &commands, &count) == NX_OK);
        compare(commands, count, 96, 80, (nx_color){0.2f, 0.4f, 0.6f, (float)(i % 2)}, NX_OK);
        ++cases;
    }
    uint8_t bitmap[7 * 5 * 4], alpha[7 * 5];
    for (unsigned i = 0; i < sizeof(bitmap); ++i)
        bitmap[i] = (uint8_t)random_bits();
    for (unsigned i = 0; i < sizeof(alpha); ++i)
        alpha[i] = (uint8_t)random_bits();
    for (unsigned i = 0; i < 1024; ++i) {
        CHECK(nx_begin_frame(context, 96, 80, 0) == NX_OK);
        CHECK(nx_push_clip(context, (nx_rect){coordinate(), coordinate(), 64.25f, 49.5f}) == NX_OK);
        for (unsigned j = 0; j < 12; ++j) {
            nx_rect box = {coordinate(), coordinate(), (float)(random_bits() % 1537u) / 16,
                           (float)(random_bits() % 1281u) / 16};
            nx_color color = {channel(), channel(), channel(), channel()};
            switch (j % 6) {
            case 0:
            case 1:
                CHECK(nx_fill_round_rect(context, box, (float)(random_bits() % 1025u) / 16,
                                         color) == NX_OK);
                break;
            case 2:
                CHECK(nx_stroke_rect(context, box, (float)(1u + random_bits() % 65u) / 16,
                                     color) == NX_OK);
                break;
            case 3:
                CHECK(nx_draw_line(context, (nx_point){box.x, box.y},
                                   (nx_point){box.x + box.width, box.y + box.height}, 1.25f,
                                   color) == NX_OK);
                break;
            case 4:
                CHECK(nx_fill_triangle(context, (nx_point){box.x, box.y},
                                       (nx_point){box.x + box.width, box.y},
                                       (nx_point){box.x, box.y + box.height}, color) == NX_OK);
                break;
            default: {
                int rgba = (i + j) % 2 != 0;
                nx_bitmap_view view = {rgba ? bitmap : alpha, 7, 5, rgba ? 28 : 7,
                                       rgba ? NX_BITMAP_RGBA8 : NX_BITMAP_A8};
                CHECK(nx_draw_bitmap(context, box, view, color) == NX_OK);
                break;
            }
            }
        }
        CHECK(nx_pop_clip(context) == NX_OK && nx_end_frame(context) == NX_OK);
        const nx_command *commands;
        size_t count;
        CHECK(nx_frame_commands(context, &commands, &count) == NX_OK);
        compare(commands, count, 96, 80, (nx_color){channel(), channel(), channel(), channel()}, NX_OK);
        ++cases;
    }
    nx_command invalid = {.type = NX_COMMAND_FILL_ROUND_RECT, .rect = {0, 0, 10, 10},
                           .radius = -1, .color = {1, 1, 1, 1}};
    compare(&invalid, 1, 8, 8, (nx_color){0, 0, 0, 0}, NX_ERROR_COMMAND);
    invalid.radius = NAN;
    compare(&invalid, 1, 8, 8, (nx_color){0, 0, 0, 0}, NX_ERROR_COMMAND);
    nx_context_destroy(context);
    CHECK(cases == 1600);
    puts("1600 full corpus +128 tiny-radius byte/stride/guard parity cases and2invalid atomicity cases passed");
    return 0;
}
