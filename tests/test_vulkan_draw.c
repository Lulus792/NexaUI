#include "renderer.h"
#include "wayland/window_internal.h"
#include "x11/window_internal.h"
#include <X11/Xutil.h>

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static nx_command rect(float x, float y, float width, float height, nx_color color) {
    nx_command command = {0};
    command.type = NX_COMMAND_FILL_RECT;
    command.rect = (nx_rect){x, y, width, height};
    command.color = color;
    return command;
}

static nx_command triangle(nx_point a, nx_point b, nx_point c, nx_color color) {
    nx_command command = {0};
    command.type = NX_COMMAND_FILL_TRIANGLE;
    command.points[0] = a;
    command.points[1] = b;
    command.points[2] = c;
    command.color = color;
    return command;
}

static int compare(nx_window *window, nx_vulkan_renderer *renderer,
    const nx_command *commands, size_t count, double scale, nx_color clear) {
    int presented = 0;
    for (unsigned retry = 0; retry < 10 && !presented; ++retry) {
        CHECK(nx_window_poll(window, 10) == NX_OK);
        CHECK(nx_vulkan_render(renderer, commands, count, scale, clear, &presented) == NX_OK);
    }
    CHECK(presented);
    nx_vulkan_renderer_info info;
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK);
    CHECK(info.drawing_supported && info.readback_supported && info.pixel_width && info.pixel_height);
    size_t stride = (size_t)info.pixel_width * 4u + 17u;
    size_t bytes = stride * info.pixel_height;
    uint8_t *expected = malloc(bytes), *actual = malloc(bytes);
    nx_command *scaled = malloc((count ? count : 1) * sizeof(*scaled));
    CHECK(expected && actual && scaled);
    for (size_t i = 0; i < count; ++i) {
        scaled[i] = commands[i];
        if (scaled[i].type != NX_COMMAND_FILL_TRIANGLE && scaled[i].type != NX_COMMAND_LINE &&
            scaled[i].type != NX_COMMAND_POP_CLIP) {
            scaled[i].rect.x = (float)(scaled[i].rect.x * scale);
            scaled[i].rect.y = (float)(scaled[i].rect.y * scale);
            scaled[i].rect.width = (float)(scaled[i].rect.width * scale);
            scaled[i].rect.height = (float)(scaled[i].rect.height * scale);
        } else if (scaled[i].type != NX_COMMAND_POP_CLIP) {
            for (unsigned j = 0; j < 3; ++j) {
                scaled[i].points[j].x = (float)(scaled[i].points[j].x * scale);
                scaled[i].points[j].y = (float)(scaled[i].points[j].y * scale);
            }
        }
        if (scaled[i].type == NX_COMMAND_STROKE_RECT || scaled[i].type == NX_COMMAND_LINE)
            scaled[i].thickness = (float)(scaled[i].thickness * scale);
        if (scaled[i].type == NX_COMMAND_FILL_ROUND_RECT)
            scaled[i].radius = (float)(scaled[i].radius * scale);
    }
    memset(expected, 0xa7, bytes);
    memset(actual, 0xa7, bytes);
    CHECK(nx_headless_render(scaled, count, expected, info.pixel_width,
                             info.pixel_height, stride, clear) == NX_OK);
    CHECK(nx_vulkan_test_render_readback(renderer, commands, count, scale,
                                         clear, actual, stride) == NX_OK);
    int matches = memcmp(expected, actual, bytes) == 0;
    if (!matches) {
        for (size_t i = 0; i < bytes; ++i) if (actual[i] != expected[i]) {
            fprintf(stderr, "scale=%.2f count=%zu at (%zu,%zu) channel %zu: GPU=%u CPU=%u\n",
                scale, count, (i % stride) / 4u, i / stride, (i % stride) % 4u,
                actual[i], expected[i]);
            break;
        }
    }
    free(scaled);
    free(actual);
    free(expected);
    CHECK(matches);
    return 0;
}

static int test_draws(nx_window *window) {
    nx_vulkan_renderer *renderer = NULL;
    CHECK(nx_vulkan_renderer_create(window, NULL, &renderer) == NX_OK);
    const nx_color clear = {0.5f / 255.0f, 82.5f / 255.0f, 190.5f / 255.0f, 92.5f / 255.0f};
    nx_command commands[] = {
        rect(-9, 3, 46, 23, (nx_color){0.5f, 0.25f, 0.75f, 1}),
        rect(12.5f, 18.25f, 30.75f, 21.5f, (nx_color){0.2f, 0.8f, 0.1f, 1}),
        triangle((nx_point){16, 12}, (nx_point){67, 18}, (nx_point){30, 56},
                 (nx_color){1, 0, 0, 1}),
        triangle((nx_point){75, 7}, (nx_point){58, 48}, (nx_point){104, 38},
                 (nx_color){0, 0, 1, 1}),
        triangle((nx_point){-12, 38}, (nx_point){32, 78}, (nx_point){-9, 109},
                 (nx_color){1, 1, 0, 1}),
        /* Shared diagonal, both windings, half-pixel top-left edges. */
        triangle((nx_point){40.5f, 52.5f}, (nx_point){68.5f, 52.5f}, (nx_point){40.5f, 80.5f},
                 (nx_color){0, 1, 1, 1}),
        triangle((nx_point){68.5f, 80.5f}, (nx_point){40.5f, 80.5f}, (nx_point){68.5f, 52.5f},
                 (nx_color){1, 0, 1, 1}),
        triangle((nx_point){4, 4}, (nx_point){4, 4}, (nx_point){4, 4}, (nx_color){1,1,1,1}),
        rect(6, 6, 0, 19, (nx_color){1,1,1,1}),
        rect(0.50001f, 0.49999f, 3, 2, (nx_color){1,1,1,1})
    };
    int presented = 1;
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_PIPELINE_MEMORY) == NX_OK);
    CHECK(nx_vulkan_render(renderer, commands, 10, 1, clear, &presented) == NX_ERROR_MEMORY && !presented);
    nx_vulkan_renderer_info info;
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK && !info.has_acquired_image);
    CHECK(!info.vertex_capacity_bytes && !info.presented_frames);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_DRAW_MEMORY) == NX_OK);
    CHECK(nx_vulkan_render(renderer, commands, 10, 1, clear, &presented) == NX_ERROR_MEMORY && !presented);
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK && !info.has_acquired_image && !info.vertex_capacity_bytes);
    const double scales[] = {1, 1.5, 2};
    for (unsigned i = 0; i < 3; ++i) CHECK(compare(window, renderer, commands, 10, scales[i], clear) == 0);
    nx_command fractional = triangle((nx_point){10.50001f, 10.5f},
        (nx_point){50.50001f, 10.5f}, (nx_point){10.50001f, 50.5f}, (nx_color){1,1,1,1});
    CHECK(compare(window, renderer, &fractional, 1, 1, clear) == 0);
    nx_command distant = triangle((nx_point){-1000000.0f, -1000000.0625f},
        (nx_point){1000000.0f, 999999.9375f}, (nx_point){-1000000.0f, 999999.9375f},
        (nx_color){0,1,1,1});
    CHECK(compare(window, renderer, &distant, 1, 1, clear) == 0);
    for (unsigned i = 0; i < 24; ++i) {
        float offset = (float)i / 1000.0f;
        nx_command skewed = triangle((nx_point){3.1401f + offset, 2.7182f - offset},
            (nx_point){152.319f - offset, 11.1781f + offset},
            (nx_point){35.409f + offset, 105.079f - offset}, (nx_color){1,0,1,1});
        CHECK(compare(window, renderer, &skewed, 1, scales[i % 3u], clear) == 0);
    }
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK);
    uint64_t allocations = info.vertex_buffer_allocations, capacity = info.vertex_capacity_bytes;
    CHECK(allocations == 1 && capacity && info.last_submitted_vertices);
    nx_command many[256];
    for (unsigned i = 0; i < 256; ++i) {
        float x = (float)((i * 17u) % 145u) - 12.5f;
        float y = (float)((i * 31u) % 109u) - 8.25f;
        nx_color color = {(i % 17u) / 16.0f, (i % 23u) / 22.0f, (i % 29u) / 28.0f, 1};
        many[i] = i % 2 ? rect(x, y, 15.25f, 11.5f, color)
            : triangle((nx_point){x,y}, (nx_point){x+13,y+3}, (nx_point){x+4,y+17}, color);
    }
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_DRAW_MEMORY) == NX_OK);
    CHECK(nx_vulkan_render(renderer, many, 256, 1, clear, &presented) == NX_ERROR_MEMORY && !presented);
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK &&
        info.vertex_buffer_allocations == allocations && info.vertex_capacity_bytes == capacity && !info.has_acquired_image);
    CHECK(compare(window, renderer, commands, 10, 1, clear) == 0);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_VERTEX_MEMORY) == NX_OK);
    CHECK(nx_vulkan_render(renderer, many, 256, 1, clear, &presented) == NX_ERROR_MEMORY);
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK &&
          info.vertex_buffer_allocations == allocations && info.vertex_capacity_bytes == capacity &&
          !info.has_acquired_image);
    CHECK(compare(window, renderer, commands, 10, 1, clear) == 0); /* Old upload still usable. */
    CHECK(compare(window, renderer, many, 256, 1, clear) == 0);
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK && info.vertex_buffer_allocations == 2);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_SUBMISSION_WAIT_TIMEOUT) == NX_OK);
    CHECK(nx_vulkan_render(renderer, commands, 10, 1, clear, &presented) == NX_OK && presented);
    CHECK(nx_vulkan_render(renderer, many, 256, 1, clear, &presented) == NX_OK && !presented);
    CHECK(nx_vulkan_render(renderer, many, 256, 1, clear, &presented) == NX_OK && !presented);
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK &&
          info.vertex_buffer_allocations == 2 && !info.has_acquired_image);
    CHECK(compare(window, renderer, many, 256, 1, clear) == 0);
    for (unsigned i = 0; i < 16; ++i) CHECK(compare(window, renderer, many, 256, 1, clear) == 0);
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK && info.vertex_buffer_allocations == 2);
    CHECK(nx_vulkan_test_invalidate(renderer, 0) == NX_OK);
    CHECK(compare(window, renderer, commands, 10, 1, clear) == 0);
    if (nx_window_wayland_display(window)) {
        CHECK(nx_window_wayland_test_output(window, 2, 1) == NX_OK);
        CHECK(compare(window, renderer, commands, 10, 2, clear) == 0);
        CHECK(nx_window_wayland_test_output(window, 1, 0) == NX_OK);
    }
    CHECK(nx_vulkan_renderer_recover(renderer) == NX_OK);
    CHECK(compare(window, renderer, commands, 10, 1, clear) == 0);
    CHECK(nx_vulkan_renderer_destroy(renderer) == NX_OK);
    return 0;
}

static nx_command clip(float x, float y, float width, float height) {
    nx_command c = rect(x, y, width, height, (nx_color){0});
    c.type = NX_COMMAND_PUSH_CLIP; return c;
}

static unsigned channel(unsigned long pixel, unsigned long mask) {
    unsigned shift = 0;
    while (!(mask & 1u)) { mask >>= 1u; ++shift; }
    return (unsigned)((((pixel >> shift) & mask) * 255u + mask / 2u) / mask);
}

static int displayed_pixels(nx_window *window, nx_vulkan_renderer *renderer,
                             const nx_command *commands, size_t count, nx_color clear) {
    Display *display = nx_window_x11_display(window);
    if (!display) return 0;
    nx_vulkan_renderer_info info;
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK);
    size_t bytes = (size_t)info.pixel_width * info.pixel_height * 4u;
    uint8_t *expected = malloc(bytes); CHECK(expected);
    CHECK(nx_headless_render(commands, count, expected, info.pixel_width,
        info.pixel_height, (size_t)info.pixel_width * 4u, clear) == NX_OK);
    int matches = 0;
    for (unsigned retry = 0; retry < 20 && !matches; ++retry) {
        CHECK(nx_window_poll(window, 10) == NX_OK);
        XSync(display, False);
        XImage *image = XGetImage(display, nx_window_x11_id(window), 0, 0,
            info.pixel_width, info.pixel_height, AllPlanes, ZPixmap);
        CHECK(image && image->red_mask && image->green_mask && image->blue_mask);
        matches = 1;
        for (uint32_t y = 0; y < info.pixel_height && matches; ++y)
            for (uint32_t x = 0; x < info.pixel_width; ++x) {
                unsigned long p = XGetPixel(image, (int)x, (int)y);
                const uint8_t *e = expected + ((size_t)y * info.pixel_width + x) * 4u;
                if (channel(p, image->red_mask) != e[0] || channel(p, image->green_mask) != e[1] ||
                    channel(p, image->blue_mask) != e[2]) { matches = 0; break; }
            }
        XDestroyImage(image);
    }
    free(expected);
    CHECK(matches); /* Includes the final canvas-to-swapchain copy and presentation. */
    return 0;
}

static int test_clips_and_alpha(nx_window *window) {
    nx_vulkan_renderer *renderer = NULL;
    CHECK(nx_vulkan_renderer_create(window, NULL, &renderer) == NX_OK);
    nx_command pop = {0}; pop.type = NX_COMMAND_POP_CLIP;
    nx_command commands[] = {
        rect(-10, -10, 200, 160, (nx_color){.3f,.6f,.9f,.13f}),
        clip(7.5f, 10.25f, 71.25f, 63.5f),
        rect(0, 0, 100, 100, (nx_color){.9f,.2f,.1f,.37f}),
        clip(17.125f, 18.75f, 55.5f, 40.5f),
        triangle((nx_point){9.3f,11.7f}, (nx_point){86.5f,22.5f}, (nx_point){30.1f,78.7f},
            (nx_color){.7f,.3f,.8f,.61f}),
        rect(18, 19, 43, 29, (nx_color){.2f,.9f,.4f,.5f/255}),
        rect(13, 13, 61, 51, (nx_color){1,1,1,.499f/255}), pop,
        rect(3.5f, 40.5f, 89, 36, (nx_color){.1f,.5f,.9f,254.499f/255}),
        clip(22, 12, 0, 50), rect(0, 0, 120, 90, (nx_color){1,0,0,1}), pop, pop,
        triangle((nx_point){40.5f,52.5f}, (nx_point){68.5f,52.5f}, (nx_point){40.5f,80.5f},
            (nx_color){0,1,1,.51f}),
        triangle((nx_point){68.5f,80.5f}, (nx_point){40.5f,80.5f}, (nx_point){68.5f,52.5f},
            (nx_color){1,0,1,.51f}),
        rect(0, 0, 8, 8, (nx_color){.4f,.3f,.8f,254.5f/255}),
        rect(0, 0, 8, 8, (nx_color){1,1,1,0})
    };
    const double scales[] = {1, 1.5, 2};
    const nx_color backgrounds[] = {{.2f,.4f,.7f,0}, {.7f,.3f,.1f,.26f}, {.1f,.2f,.3f,1}};
    for (unsigned i = 0; i < 3; ++i)
        CHECK(compare(window, renderer, commands, sizeof(commands)/sizeof(*commands), scales[i], backgrounds[i]) == 0);
    nx_command deep[2 * NX_MAX_CLIP_DEPTH + 1];
    for (unsigned i = 0; i < NX_MAX_CLIP_DEPTH; ++i) deep[i] = clip(i * .25f, i * .125f, 100, 80);
    deep[NX_MAX_CLIP_DEPTH] = rect(0, 0, 130, 100, (nx_color){1,.1f,.3f,.43f});
    for (unsigned i = 0; i < NX_MAX_CLIP_DEPTH; ++i) deep[NX_MAX_CLIP_DEPTH + 1 + i] = pop;
    CHECK(compare(window, renderer, deep, sizeof(deep)/sizeof(*deep), 1.5, backgrounds[1]) == 0);
    nx_command empty[] = {clip(-20, -20, 3, 3), pop};
    CHECK(compare(window, renderer, empty, 2, 1, backgrounds[0]) == 0);
    CHECK(compare(window, renderer, NULL, 0, 1, backgrounds[2]) == 0);
    nx_command many[96];
    for (unsigned i = 0; i < 96; ++i) {
        float x = (float)((i * 17u) % 151u) - 9.25f, y = (float)((i * 29u) % 107u) - 8.5f;
        nx_color c = {(i % 19u)/18.0f, (i % 23u)/22.0f, (i % 29u)/28.0f, (i % 31u)/30.0f};
        many[i] = i % 2 ? rect(x,y,37.75f,22.5f,c) :
            triangle((nx_point){x,y},(nx_point){x+49.25f,y+8.5f},(nx_point){x+7.75f,y+34.25f},c);
    }
    for (unsigned i = 0; i < 3; ++i) CHECK(compare(window, renderer, many, 96, scales[i], backgrounds[i]) == 0);
    CHECK(compare(window, renderer, commands, sizeof(commands)/sizeof(*commands), 1, backgrounds[2]) == 0);
    CHECK(displayed_pixels(window, renderer, commands, sizeof(commands)/sizeof(*commands), backgrounds[2]) == 0);
    CHECK(nx_vulkan_test_invalidate(renderer, 0) == NX_OK);
    CHECK(compare(window, renderer, commands, sizeof(commands)/sizeof(*commands), 1, backgrounds[1]) == 0);
    CHECK(nx_vulkan_renderer_recover(renderer) == NX_OK);
    CHECK(compare(window, renderer, commands, sizeof(commands)/sizeof(*commands), 2, backgrounds[0]) == 0);
    nx_vulkan_renderer_info info;
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK);
    size_t stride = (size_t)info.pixel_width * 4u + 9u;
    size_t bytes = stride * info.pixel_height;
    uint8_t *pending = malloc(bytes); CHECK(pending);
    memset(pending, 0x6b, bytes);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_SUBMISSION_WAIT_TIMEOUT) == NX_OK);
    CHECK(nx_vulkan_test_render_readback(renderer, commands, sizeof(commands)/sizeof(*commands),
        1, backgrounds[1], pending, stride) == NX_ERROR_LIMIT);
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK && info.pending_readback);
    for (size_t i = 0; i < bytes; ++i) CHECK(pending[i] == 0x6b);
    free(pending);
    int presented = 0;
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_SUBMISSION_WAIT_TIMEOUT) == NX_OK);
    for (unsigned retry = 0; retry < 2; ++retry) {
        CHECK(nx_vulkan_render(renderer, commands, sizeof(commands)/sizeof(*commands), 1, backgrounds[1], &presented) == NX_OK && !presented);
        CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK && info.pending_readback);
    }
    CHECK(nx_vulkan_render(renderer, commands, sizeof(commands)/sizeof(*commands), 1, backgrounds[1], &presented) == NX_OK && presented);
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK && !info.pending_readback);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_RETIRE_TIMEOUT) == NX_OK);
    CHECK(nx_vulkan_renderer_destroy(renderer) == NX_ERROR_LIMIT);
    CHECK(nx_vulkan_renderer_destroy(renderer) == NX_OK);
    return 0;
}

typedef struct block { void *pointer; size_t bytes; } block;
typedef struct heap { block blocks[64]; size_t live; } heap;
static void *tracked_allocate(void *user, size_t bytes) {
    heap *h = user;
    if (h->live == 64) abort();
    void *p = malloc(bytes);
    if (p) h->blocks[h->live++] = (block){p, bytes};
    return p;
}
static void tracked_free(void *user, void *pointer, size_t bytes) {
    heap *h = user; size_t i = 0;
    while (i < h->live && h->blocks[i].pointer != pointer) ++i;
    if (i == h->live || h->blocks[i].bytes != bytes) abort();
    h->blocks[i] = h->blocks[--h->live]; free(pointer);
}
static int test_owned_storage(nx_window *window) {
    heap h = {0}; nx_allocator allocator = {&h, tracked_allocate, tracked_free};
    nx_vulkan_renderer *renderer;
    CHECK(nx_vulkan_renderer_create(window, &allocator, &renderer) == NX_OK);
    nx_command commands[80];
    for (unsigned i = 0; i < 80; ++i) commands[i] = rect(i, i % 71u, 12, 9, (nx_color){1,.3f,.7f,.4f});
    int presented;
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_DRAW_MEMORY) == NX_OK);
    CHECK(nx_vulkan_render(renderer, commands, 1, 1, (nx_color){0}, &presented) == NX_ERROR_MEMORY && !presented);
    CHECK(compare(window, renderer, commands, 1, 1, (nx_color){0}) == 0);
    CHECK(compare(window, renderer, commands, 80, 1.5, (nx_color){0}) == 0);
    CHECK(nx_vulkan_test_invalidate(renderer, 0) == NX_OK);
    CHECK(compare(window, renderer, commands, 1, 1, (nx_color){0}) == 0);
    CHECK(nx_vulkan_renderer_recover(renderer) == NX_OK);
    CHECK(compare(window, renderer, commands, 80, 1, (nx_color){0}) == 0);
    CHECK(nx_vulkan_renderer_destroy(renderer) == NX_OK && h.live == 0);
    return 0;
}

static nx_command round_rect(float x, float y, float w, float h, float radius, nx_color color) {
    nx_command c = rect(x, y, w, h, color);
    c.type = NX_COMMAND_FILL_ROUND_RECT; c.radius = radius; return c;
}
static nx_command stroke_rect(float x, float y, float w, float h, float thickness, nx_color color) {
    nx_command c = rect(x, y, w, h, color);
    c.type = NX_COMMAND_STROKE_RECT; c.thickness = thickness; return c;
}
static nx_command line(float ax, float ay, float bx, float by, float thickness, nx_color color) {
    nx_command c = {0}; c.type = NX_COMMAND_LINE;
    c.points[0] = (nx_point){ax, ay}; c.points[1] = (nx_point){bx, by};
    c.thickness = thickness; c.color = color; return c;
}

static int test_shapes(nx_window *window) {
    nx_vulkan_renderer *renderer;
    CHECK(nx_vulkan_renderer_create(window, NULL, &renderer) == NX_OK);
    nx_command commands[] = {
        round_rect(3.25f, 4.5f, 59.5f, 41.25f, 12.5f, (nx_color){.2f,.7f,1,.63f}),
        stroke_rect(12.5f, 10.5f, 62.25f, 47.5f, 1.5f, (nx_color){1,.2f,.7f,.47f}),
        clip(5.5f, 8.25f, 138.25f, 85.5f),
        line(-30, 11.5f, 120, 100.5f, 5.5f, (nx_color){.4f,1,.2f,.6f}),
        line(70.5f, 22.5f, 70.5f, 22.5f, 19, (nx_color){1,.7f,.2f,1}),
        line(90.5f, 4, 90.5f, 105, 1, (nx_color){1,1,1,.25f}),
        line(3, 62.5f, 155, 62.5f, 1, (nx_color){.2f,1,1,.7f}),
        round_rect(90.5f, 55.5f, 27, 18, 1000, (nx_color){1,.2f,1,.3f}),
        round_rect(2, 76, 31, 25, 0, (nx_color){.3f,.1f,1,1}),
        stroke_rect(41, 77, 31, 25, 40, (nx_color){.8f,.5f,1,.8f}),
        round_rect(4, 4, 0, 30, 10, (nx_color){1,1,1,1}),
        line(150, 5, 130, 90, .125f, (nx_color){1,1,1,1}),
        { .type = NX_COMMAND_POP_CLIP }
    };
    const double scales[] = {1, 1.5, 2};
    for (unsigned i = 0; i < 3; ++i)
        CHECK(compare(window, renderer, commands, sizeof(commands)/sizeof(*commands), scales[i], (nx_color){.03f,.07f,.11f,.26f}) == 0);
    /* Exact 3-4-5 circle boundaries, reversed segments and near-half-pixel
       coordinates must not depend on float multiply/add contraction. */
    nx_command boundaries[] = {
        line(20.5f, 20.5f, 20.5f, 20.5f, 10, (nx_color){1,0,1,1}),
        round_rect(40.5f, 10.5f, 20, 20, 5, (nx_color){0,1,1,.7f}),
        line(12.50001f, 36.5f, 110.50001f, 36.5f, 10, (nx_color){1,1,0,.4f}),
        stroke_rect(10.50001f, 12.5f, 30, 30, .00002f, (nx_color){1,1,1,1}),
        line(110.50001f, 61.5f, 12.50001f, 61.5f, 10, (nx_color){0,1,0,.5f})
    };
    CHECK(compare(window, renderer, boundaries, sizeof(boundaries)/sizeof(*boundaries), 1, (nx_color){0}) == 0);
    nx_command distant[] = {
        line(-1000000, -1000000.0625f, 1000000, 999999.9375f, 1.25f, (nx_color){1,1,1,.45f}),
        line(1000000, 80.5f, -1000000, 80.5f, 1, (nx_color){1,0,1,.65f}),
        round_rect(-1000000, 12.5f, 1000050, 55.25f, 17.5f, (nx_color){1,.4f,.2f,.5f}),
        stroke_rect(-1000000, 22.5f, 1000080, 46.5f, .5f, (nx_color){.1f,1,.2f,.4f})
    };
    CHECK(compare(window, renderer, distant, sizeof(distant)/sizeof(*distant), 1, (nx_color){0}) == 0);
    nx_command many[120];
    for (unsigned i = 0; i < 120; ++i) {
        float x = (float)((i * 31u) % 170u) - 9.25f;
        float y = (float)((i * 19u) % 130u) - 8.75f;
        nx_color color = {(i % 17u)/16.f, (i % 23u)/22.f, (i % 29u)/28.f, (i % 11u)/10.f};
        many[i] = i % 3u == 0 ? round_rect(x, y, 32.25f, 21.75f, (i % 13u) * .75f, color) :
            i % 3u == 1 ? stroke_rect(x, y, 29.75f, 22.5f, .25f + (i % 7u), color) :
            line(x, y, x + 19.25f, y - 13.75f, .5f + (i % 7u) * .75f, color);
    }
    for (unsigned i = 0; i < 3; ++i) CHECK(compare(window, renderer, many, 120, scales[i], (nx_color){0,0,0,1}) == 0);
    for (unsigned i = 0; i < 24; ++i) {
        float offset = (float)i / 1000.f;
        nx_command skew[] = {
            line(3.1401f + offset, 2.7182f - offset, 152.319f - offset, 105.079f + offset,
                 1.001f + offset, (nx_color){1,.3f,.7f,.6f}),
            round_rect(4.50001f + offset, 10.49999f - offset, 64.319f, 43.7182f, 21.1781f,
                       (nx_color){.1f,1,.8f,.4f})
        };
        CHECK(compare(window, renderer, skew, 2, scales[i % 3], (nx_color){0}) == 0);
    }
    CHECK(nx_vulkan_test_invalidate(renderer, 0) == NX_OK);
    CHECK(compare(window, renderer, commands, sizeof(commands)/sizeof(*commands), 1, (nx_color){0}) == 0);
    CHECK(nx_vulkan_renderer_recover(renderer) == NX_OK);
    CHECK(compare(window, renderer, many, 120, 1.5, (nx_color){0}) == 0);
    CHECK(nx_vulkan_renderer_destroy(renderer) == NX_OK);
    return 0;
}

static nx_command bitmap(float x, float y, float w, float h, nx_bitmap_view view, nx_color tint) {
    nx_command c = rect(x, y, w, h, tint); c.type = NX_COMMAND_BITMAP; c.bitmap = view; return c;
}

static int test_bitmaps(nx_window *window) {
    heap h = {0}; nx_allocator allocator = {&h, tracked_allocate, tracked_free};
    nx_vulkan_renderer *renderer;
    CHECK(nx_vulkan_renderer_create(window, &allocator, &renderer) == NX_OK);
    uint8_t rgba[32] = {255,0,0,255, 0,255,0,128, 0,0,255,0, 17,18,19,20,
        255,255,0,64, 0,255,255,255, 255,255,255,0, 21,22,23,24};
    uint8_t mask[10] = {0,64,255,19,20, 128,255,32,21,22};
    nx_bitmap_view image = {rgba, 3, 2, 16, NX_BITMAP_RGBA8};
    nx_bitmap_view coverage = {mask, 3, 2, 5, NX_BITMAP_A8};
    nx_command first = bitmap(3.5f, 4.25f, 91.5f, 57.25f, image, (nx_color){1,.5f,.75f,.8f});
    const nx_vulkan_test_fault faults[] = {NX_VK_TEST_BITMAP_RECORD_MEMORY, NX_VK_TEST_ATLAS_MEMORY, NX_VK_TEST_BITMAP_UPLOAD_MEMORY};
    int presented;
    for (unsigned i = 0; i < 3; ++i) {
        CHECK(nx_vulkan_test_fail_next(renderer, faults[i]) == NX_OK);
        CHECK(nx_vulkan_render(renderer, &first, 1, 1, (nx_color){0}, &presented) == NX_ERROR_MEMORY && !presented);
        nx_vulkan_renderer_info info;
        CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK && !info.has_acquired_image && !info.presented_frames);
    }
    CHECK(compare(window, renderer, &first, 1, 1, (nx_color){0}) == 0);
    nx_glyph_point glyph_points[8] = {{0,0,1},{4,0,1},{4,4,1},{0,4,1}, {1,1,1},{1,3,1},{3,3,1},{3,1,1}};
    uint16_t ends[2] = {3,7};
    nx_glyph_outline_buffer outline = {0};
    outline.points = glyph_points; outline.point_count = outline.point_capacity = 8;
    outline.contour_ends = ends; outline.contour_count = outline.contour_capacity = 2;
    uint8_t glyph[64] = {0};
    CHECK(nx_rasterize_glyph_a8(&outline, 2, 0, 8, glyph, 8, 8, 8, sizeof(glyph), NULL) == NX_OK);
    CHECK(glyph[0] == 255 && glyph[27] == 0);
    nx_command commands[] = {
        clip(9.5f, 6.25f, 137, 93), first,
        bitmap(32.25f, 22.75f, 81.5f, 55.5f, coverage, (nx_color){.8f,.2f,.1f,.7f}),
        round_rect(62, 39, 64, 28, 8, (nx_color){.1f,.8f,.4f,.5f}),
        bitmap(70.5f, 50.5f, 61, 41, image, (nx_color){1,1,1,1}),
        {.type = NX_COMMAND_POP_CLIP},
        bitmap(-8.25f, 90.5f, 21.5f, 20.25f, image, (nx_color){1,1,1,1}),
        bitmap(112, 9, 32, 32, (nx_bitmap_view){glyph,8,8,8,NX_BITMAP_A8}, (nx_color){.9f,.8f,1,.8f}),
        bitmap(0, 0, 160, 112, image, (nx_color){1,1,1,0}),
        bitmap(0, 0, 0, 20, coverage, (nx_color){1,1,1,1})
    };
    const double scales[] = {1, 1.5, 2};
    const nx_color clears[] = {{0},{.03f,.07f,.11f,.26f},{.04f,.08f,.12f,1}};
    for (unsigned i = 0; i < 3; ++i) for (unsigned j = 0; j < 3; ++j)
        CHECK(compare(window, renderer, commands, sizeof(commands)/sizeof(*commands), scales[i], clears[j]) == 0);
    CHECK(compare(window, renderer, commands, sizeof(commands)/sizeof(*commands), 1, clears[2]) == 0);
    CHECK(displayed_pixels(window, renderer, commands, sizeof(commands)/sizeof(*commands), clears[2]) == 0);
    rgba[3] = 0; mask[1] = 255;
    CHECK(compare(window, renderer, commands, sizeof(commands)/sizeof(*commands), 1, clears[1]) == 0);
    CHECK(compare(window, renderer, NULL, 0, 1, clears[0]) == 0);
    nx_command clipped[] = {clip(-20,-20,1,1), first, {.type = NX_COMMAND_POP_CLIP}};
    CHECK(compare(window, renderer, clipped, 3, 1, clears[1]) == 0);
    CHECK(compare(window, renderer, commands, sizeof(commands)/sizeof(*commands), 1, clears[2]) == 0);
    for (unsigned i = 0; i < 24; ++i) {
        float offset = (float)i / 1000.f;
        nx_command edges[] = {
            bitmap(10.50001f + offset, 6.49999f - offset, 99.00001f, 60.00001f, image, (nx_color){1,1,1,1}),
            bitmap(-1000000, 55.5f, 1000050, 33.75f, coverage, (nx_color){.2f,.8f,1,.6f}),
            bitmap(110.25f + offset, 50.5f, .3f, .7f, image, (nx_color){1,1,1,1})
        };
        CHECK(compare(window, renderer, edges, 3, scales[i % 3], clears[i % 3]) == 0);
    }
    uint8_t sources[40][4]; nx_command many[80];
    for (unsigned i = 0; i < 40; ++i) {
        sources[i][0] = (uint8_t)(i * 23u); sources[i][1] = (uint8_t)(i * 31u);
        sources[i][2] = (uint8_t)(i * 47u); sources[i][3] = (uint8_t)(i * 17u);
        many[i] = bitmap((i * 17u) % 140u, (i * 31u) % 100u, 21.5f, 19.25f,
            (nx_bitmap_view){sources[i],1,1,4,NX_BITMAP_RGBA8}, (nx_color){.8f,.4f,.9f,.8f});
        many[i + 40] = many[i];
    }
    nx_vulkan_renderer_info before, after;
    CHECK(nx_vulkan_renderer_get_info(renderer, &before) == NX_OK);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_BITMAP_RECORD_MEMORY) == NX_OK);
    CHECK(nx_vulkan_render(renderer, many, 80, 1, clears[0], &presented) == NX_ERROR_MEMORY && !presented);
    CHECK(compare(window, renderer, commands, sizeof(commands)/sizeof(*commands), 1, clears[0]) == 0);
    CHECK(compare(window, renderer, many, 80, 1.5, clears[1]) == 0);
    CHECK(nx_vulkan_renderer_get_info(renderer, &after) == NX_OK && after.unique_bitmaps == 40);
    CHECK(after.atlas_allocations == before.atlas_allocations && after.bitmap_upload_allocations == before.bitmap_upload_allocations);
    /* Larger dimensions force both GPU storage classes to grow; each failure
       leaves the previously usable stream and upload allocations intact. */
    uint8_t *large = malloc(2049u * 3u); CHECK(large);
    for (size_t i = 0; i < 2049u * 3u; ++i) large[i] = (uint8_t)(i * 37u);
    nx_command growing = bitmap(2.25f, 1.75f, 150, 100, (nx_bitmap_view){large,2049,3,2049,NX_BITMAP_A8}, (nx_color){.8f,.4f,1,.7f});
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_ATLAS_MEMORY) == NX_OK);
    CHECK(nx_vulkan_render(renderer, &growing, 1, 1, clears[0], &presented) == NX_ERROR_MEMORY && !presented);
    CHECK(compare(window, renderer, &first, 1, 1, clears[0]) == 0);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_BITMAP_UPLOAD_MEMORY) == NX_OK);
    CHECK(nx_vulkan_render(renderer, &growing, 1, 1, clears[0], &presented) == NX_ERROR_MEMORY && !presented);
    CHECK(compare(window, renderer, &first, 1, 1, clears[0]) == 0);
    CHECK(compare(window, renderer, &growing, 1, 1, clears[1]) == 0);
    CHECK(nx_vulkan_renderer_get_info(renderer, &before) == NX_OK && before.atlas_width >= 2049);
    for (unsigned i = 0; i < 4; ++i) CHECK(compare(window, renderer, &growing, 1, 1.5, clears[2]) == 0);
    CHECK(nx_vulkan_renderer_get_info(renderer, &after) == NX_OK && after.atlas_allocations == before.atlas_allocations &&
          after.bitmap_upload_allocations == before.bitmap_upload_allocations);
    uint8_t *budget_pixels = calloc(1024u * 1024u + 17u, 1); CHECK(budget_pixels);
    nx_command over_budget[17];
    for (unsigned i = 0; i < 17; ++i) over_budget[i] = bitmap(0,0,10,10,
        (nx_bitmap_view){budget_pixels+i,1024,1024,1024,NX_BITMAP_A8}, (nx_color){1,1,1,1});
    CHECK(nx_vulkan_render(renderer, over_budget, 17, 1, clears[0], &presented) == NX_ERROR_LIMIT && !presented);
    free(budget_pixels);
    CHECK(nx_vulkan_renderer_get_info(renderer, &after) == NX_OK && !after.has_acquired_image &&
          after.atlas_allocations == before.atlas_allocations && after.bitmap_upload_allocations == before.bitmap_upload_allocations);
    CHECK(compare(window, renderer, &growing, 1, 1, clears[1]) == 0);
    /* Successful submission stages borrowed bytes. Retire asynchronously after
       freeing the command/source, without exposing them to the GPU. */
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_SUBMISSION_WAIT_TIMEOUT) == NX_OK);
    CHECK(nx_vulkan_render(renderer, &growing, 1, 1, clears[1], &presented) == NX_OK && presented);
    free(large); growing = (nx_command){0};
    for (unsigned i = 0; i < 2; ++i) {
        CHECK(nx_vulkan_render(renderer, many, 80, 1, clears[0], &presented) == NX_OK && !presented);
        CHECK(nx_vulkan_renderer_get_info(renderer, &after) == NX_OK &&
              after.atlas_allocations == before.atlas_allocations && after.bitmap_upload_allocations == before.bitmap_upload_allocations);
    }
    CHECK(compare(window, renderer, many, 80, 2, clears[1]) == 0);
    CHECK(nx_vulkan_test_invalidate(renderer, 0) == NX_OK);
    CHECK(compare(window, renderer, commands, sizeof(commands)/sizeof(*commands), 1, clears[0]) == 0);
    CHECK(nx_vulkan_renderer_recover(renderer) == NX_OK);
    CHECK(compare(window, renderer, many, 80, 1, clears[1]) == 0);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_RETIRE_TIMEOUT) == NX_OK);
    CHECK(nx_vulkan_renderer_destroy(renderer) == NX_ERROR_LIMIT);
    CHECK(nx_vulkan_renderer_destroy(renderer) == NX_OK && !h.live);
    return 0;
}

static int test_preflight_and_driver(nx_window *window) {
    nx_renderer_driver driver = {0};
    CHECK(nx_vulkan_driver_open(window, &driver) == NX_OK);
    nx_vulkan_renderer *renderer = driver.backend.user;
    nx_window_info window_info;
    CHECK(nx_window_get_info(window, &window_info) == NX_OK);
    nx_render_target target = {(uint32_t)ceil(window_info.width * window_info.scale),
        (uint32_t)ceil(window_info.height * window_info.scale), window_info.scale};
    nx_command commands[2] = {rect(8, 8, 28, 20, (nx_color){0,1,0,1}), {0}};
    commands[1] = commands[0];
    static const uint8_t source_pixel[4] = {255,255,255,255};
    commands[1].type = NX_COMMAND_BITMAP;
    commands[1].bitmap = (nx_bitmap_view){source_pixel, 1, 1, 4, NX_BITMAP_RGBA8};
    nx_render_outcome outcome;
    commands[1].bitmap.stride_bytes = 1;
    CHECK(nx_renderer_run(&driver, target, commands, 2, (nx_color){0,0,0,1}, &outcome) == NX_ERROR_COMMAND);
    nx_vulkan_renderer_info before, after;
    CHECK(nx_vulkan_renderer_get_info(renderer, &before) == NX_OK &&
          !before.presented_frames && !before.has_acquired_image && !before.vertex_capacity_bytes);
    commands[1] = commands[0];
    commands[1].type = NX_COMMAND_POP_CLIP;
    CHECK(nx_renderer_run(&driver, target, commands, 2, (nx_color){0,0,0,1}, &outcome) == NX_ERROR_COMMAND);
    int presented;
    CHECK(nx_vulkan_render(renderer, NULL, 1, 1, (nx_color){0,0,0,1}, &presented) == NX_ERROR_ARGUMENT);
    CHECK(nx_vulkan_render(renderer, commands, 1, NAN, (nx_color){0,0,0,1}, &presented) == NX_ERROR_ARGUMENT);
    CHECK(nx_vulkan_render(renderer, commands, 1, DBL_MAX, (nx_color){0,0,0,1}, &presented) == NX_ERROR_LIMIT);
    CHECK(nx_vulkan_render(renderer, commands, (size_t)UINT32_MAX, 1,
                            (nx_color){0,0,0,1}, &presented) == NX_ERROR_LIMIT);
    nx_command enormous = triangle((nx_point){1.0e30f, 0}, (nx_point){0,1},
                                   (nx_point){0,0}, (nx_color){1,1,1,1});
    CHECK(nx_vulkan_render(renderer, &enormous, 1, 1,
                            (nx_color){0,0,0,1}, &presented) == NX_ERROR_LIMIT);
    enormous = line(-1.0e20f, 0, 1.0e20f, 1, 1, (nx_color){1,1,1,1});
    CHECK(nx_vulkan_render(renderer, &enormous, 1, 1, (nx_color){0}, &presented) == NX_ERROR_LIMIT);
    enormous = round_rect(-1.0e30f, 0, 1.0e30f, 20, 1, (nx_color){1,1,1,1});
    CHECK(nx_vulkan_render(renderer, &enormous, 1, 1, (nx_color){0}, &presented) == NX_ERROR_LIMIT);
    enormous = bitmap(0,0,10,10, (nx_bitmap_view){source_pixel,16385,1,65540,NX_BITMAP_RGBA8}, (nx_color){1,1,1,1});
    CHECK(nx_vulkan_render(renderer, &enormous, 1, 1, (nx_color){0}, &presented) == NX_ERROR_LIMIT);
    CHECK(nx_renderer_run(&driver, target, commands, 1, (nx_color){0,0,0,1}, &outcome) == NX_OK);
    CHECK(outcome == NX_RENDER_READY);
    CHECK(nx_vulkan_renderer_get_info(renderer, &after) == NX_OK &&
          after.presented_frames > before.presented_frames && after.last_submitted_vertices == 6);
    CHECK(nx_vulkan_test_invalidate(renderer, 1) == NX_OK);
    CHECK(nx_renderer_run(&driver, target, commands, 1, (nx_color){0,0,0,1}, &outcome) == NX_OK &&
          outcome == NX_RENDER_DEVICE_LOST);
    CHECK(nx_renderer_recover(&driver) == NX_OK);
    CHECK(nx_renderer_run(&driver, target, commands, 1, (nx_color){0,0,0,1}, &outcome) == NX_OK &&
          outcome == NX_RENDER_READY);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_RETIRE_TIMEOUT) == NX_OK);
    CHECK(nx_renderer_close(&driver) == NX_ERROR_LIMIT && driver.open);
    CHECK(nx_renderer_close(&driver) == NX_OK && !driver.open);
    return 0;
}

static int test_batches(nx_window *window) {
    nx_vulkan_renderer *renderer = NULL;
    CHECK(nx_vulkan_renderer_create(window,NULL,&renderer) == NX_OK);
    const uint8_t mask[]={0,51,127,255,172,1};
    nx_command stream[]={
        rect(0,0,31,29,(nx_color){1,0,0,1}),
        {.type=NX_COMMAND_PUSH_CLIP,.rect={2.5f,3.5f,80,70}},
        rect(1,2,29,21,(nx_color){0,1,0,1}),
        rect(5.5f,7.5f,57,45,(nx_color){.2f,.3f,.9f,.5f}),
        round_rect(8,6,50,45,7,(nx_color){.6f,.9f,.1f,1}),
        {.type=NX_COMMAND_POP_CLIP},
        bitmap(3,4,61,73,(nx_bitmap_view){mask,3,2,3,NX_BITMAP_A8},(nx_color){.9f,.2f,.7f,.8f}),
        triangle((nx_point){7.5f,8.5f},(nx_point){98.5f,11.5f},(nx_point){41.5f,83.5f},(nx_color){.1f,.4f,.7f,1}),
        line(-4,4,100,79,2.5f,(nx_color){.2f,.9f,.6f,.7f})
    };
    const double scales[]={1,1.25,1.5,2};
    nx_vulkan_renderer_info before,after;
    CHECK(nx_vulkan_test_set_batching(renderer,2) == NX_ERROR_ARGUMENT);
    for (unsigned i=0;i<4;++i) {
        CHECK(nx_vulkan_test_set_batching(renderer,0) == NX_OK);
        CHECK(compare(window,renderer,stream,9,scales[i],(nx_color){.1f,.2f,.3f,.4f}) == 0);
        CHECK(nx_vulkan_renderer_get_info(renderer,&before) == NX_OK);
        CHECK(!before.last_frame.batched && before.last_frame.draw_records == 7 && before.last_frame.draw_calls == 7);
        CHECK(nx_vulkan_test_set_batching(renderer,1) == NX_OK);
        CHECK(compare(window,renderer,stream,9,scales[i],(nx_color){.1f,.2f,.3f,.4f}) == 0);
        CHECK(nx_vulkan_renderer_get_info(renderer,&after) == NX_OK);
        CHECK(after.last_frame.batched && after.last_frame.input_commands == 9 && after.last_frame.draw_records == 7 && after.last_frame.draw_calls == 4);
        CHECK(after.last_frame.vertex_upload_bytes == 7u*84u && after.last_frame.history_copies == 3 &&
            after.last_frame.history_copy_bytes == before.last_frame.history_copy_bytes && after.last_frame.render_passes == before.last_frame.render_passes);
        CHECK(after.last_frame.cpu_timing_valid && after.last_frame.submission == after.submitted_frames);
    }
    enum {COUNT=10000};
    nx_command *many=calloc(COUNT,sizeof(*many)); CHECK(many);
    for (unsigned i=0;i<COUNT;++i) many[i]=rect((float)((i*7)%151)+.5f,(float)((i*11)%103)+.5f,4,4,
        (nx_color){(i%7)/6.f,(i%11)/10.f,(i%13)/12.f,1});
    CHECK(nx_vulkan_renderer_get_info(renderer,&before) == NX_OK);
    CHECK(nx_vulkan_test_fail_next(renderer,NX_VK_TEST_VERTEX_MEMORY) == NX_OK);
    int presented;
    CHECK(nx_vulkan_render(renderer,many,COUNT,1,(nx_color){0},&presented) == NX_ERROR_MEMORY && !presented);
    CHECK(nx_vulkan_renderer_get_info(renderer,&after) == NX_OK && after.vertex_capacity_bytes == before.vertex_capacity_bytes);
    CHECK(after.submitted_frames == before.submitted_frames && after.last_frame.submission == before.last_frame.submission);
    CHECK(compare(window,renderer,many,COUNT,1,(nx_color){0}) == 0);
    CHECK(nx_vulkan_renderer_get_info(renderer,&before) == NX_OK && before.last_frame.draw_records == COUNT && before.last_frame.draw_calls == 1);
    CHECK(before.last_frame.vertex_upload_bytes == COUNT*84u && before.last_submitted_vertices == COUNT*6u);
    CHECK(compare(window,renderer,many,COUNT,1,(nx_color){0}) == 0);
    CHECK(nx_vulkan_renderer_get_info(renderer,&after) == NX_OK && after.vertex_buffer_allocations == before.vertex_buffer_allocations);
    free(many);
    CHECK(nx_vulkan_renderer_destroy(renderer) == NX_OK);
    puts("Vulkan ordered instance batches: 10000 opaque records in one draw, mixed alpha/bitmap clips, history boundaries and rollback passed.");
    return 0;
}
static int test_independent_windows(nx_window *first) {
    nx_window *second = NULL;
    nx_window_desc desc = {"NexaUI independent Vulkan renderer", 128, 96, 0, {0}};
    CHECK(nx_window_create(&desc, &second) == NX_OK);
    nx_vulkan_renderer *a = NULL, *b = NULL;
    CHECK(nx_vulkan_renderer_create(first, NULL, &a) == NX_OK);
    CHECK(nx_vulkan_renderer_create(second, NULL, &b) == NX_OK);
    nx_command command = rect(7, 9, 47, 31, (nx_color){1,0,0,.45f});
    CHECK(compare(first, a, &command, 1, 1, (nx_color){0,0,0,1}) == 0);
    command.color = (nx_color){0,0,1,.65f};
    CHECK(compare(second, b, &command, 1, 1, (nx_color){1,1,1,1}) == 0);
    CHECK(nx_vulkan_renderer_destroy(b) == NX_OK);
    nx_window_destroy(second);
    command.color = (nx_color){0,1,0,.35f};
    CHECK(compare(first, a, &command, 1, 1, (nx_color){0,0,0,1}) == 0);
    CHECK(nx_vulkan_renderer_destroy(a) == NX_OK);
    return 0;
}

int main(int argc, char **argv) {
    const char *backend = getenv("NEXAUI_PLATFORM");
    if (!backend) return 1;
    int required = getenv("NEXAUI_REQUIRE_VULKAN") &&
        (!strcmp(backend, "x11") || getenv("NEXAUI_REQUIRE_VULKAN_WAYLAND"));
    nx_window *window = NULL;
    nx_window_desc desc = {"NexaUI Vulkan drawing test", 160, 112, 0, {0}};
    nx_result result = nx_window_create(&desc, &window);
    if (result == NX_ERROR_PLATFORM && !required) return 77;
    CHECK(result == NX_OK);
    if (argc == 2) {
        int selected = !strcmp(argv[1], "alpha") ? test_clips_and_alpha(window) :
            !strcmp(argv[1], "storage") ? test_owned_storage(window) :
            !strcmp(argv[1], "shapes") ? test_shapes(window) :
            !strcmp(argv[1], "bitmaps") ? test_bitmaps(window) :
            !strcmp(argv[1], "batches") ? test_batches(window) : 1;
        nx_window_destroy(window); return selected;
    }
    int status = test_draws(window);
    if (!status) status = test_clips_and_alpha(window);
    if (!status) status = test_shapes(window);
    if (!status) status = test_bitmaps(window);
    if (!status) status = test_owned_storage(window);
    if (!status) status = test_preflight_and_driver(window);
    if (!status) status = test_independent_windows(window);
    if (!status) status = test_batches(window);
    nx_window_destroy(window);
    return status;
}
