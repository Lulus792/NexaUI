#include "renderer.h"
#include "wayland/window_internal.h"

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
        if (scaled[i].type == NX_COMMAND_FILL_RECT) {
            scaled[i].rect.x = (float)(scaled[i].rect.x * scale);
            scaled[i].rect.y = (float)(scaled[i].rect.y * scale);
            scaled[i].rect.width = (float)(scaled[i].rect.width * scale);
            scaled[i].rect.height = (float)(scaled[i].rect.height * scale);
        } else {
            for (unsigned j = 0; j < 3; ++j) {
                scaled[i].points[j].x = (float)(scaled[i].points[j].x * scale);
                scaled[i].points[j].y = (float)(scaled[i].points[j].y * scale);
            }
        }
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
    commands[1].color.a = 0.5f;
    nx_render_outcome outcome;
    CHECK(nx_renderer_run(&driver, target, commands, 2, (nx_color){0,0,0,1}, &outcome) == NX_ERROR_UNSUPPORTED);
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

static int test_independent_windows(nx_window *first) {
    nx_window *second = NULL;
    nx_window_desc desc = {"NexaUI independent Vulkan renderer", 128, 96, 0, {0}};
    CHECK(nx_window_create(&desc, &second) == NX_OK);
    nx_vulkan_renderer *a = NULL, *b = NULL;
    CHECK(nx_vulkan_renderer_create(first, NULL, &a) == NX_OK);
    CHECK(nx_vulkan_renderer_create(second, NULL, &b) == NX_OK);
    nx_command command = rect(7, 9, 47, 31, (nx_color){1,0,0,1});
    CHECK(compare(first, a, &command, 1, 1, (nx_color){0,0,0,1}) == 0);
    command.color = (nx_color){0,0,1,1};
    CHECK(compare(second, b, &command, 1, 1, (nx_color){1,1,1,1}) == 0);
    CHECK(nx_vulkan_renderer_destroy(b) == NX_OK);
    nx_window_destroy(second);
    command.color = (nx_color){0,1,0,1};
    CHECK(compare(first, a, &command, 1, 1, (nx_color){0,0,0,1}) == 0);
    CHECK(nx_vulkan_renderer_destroy(a) == NX_OK);
    return 0;
}

int main(void) {
    const char *backend = getenv("NEXAUI_PLATFORM");
    if (!backend) return 1;
    int required = getenv("NEXAUI_REQUIRE_VULKAN") &&
        (!strcmp(backend, "x11") || getenv("NEXAUI_REQUIRE_VULKAN_WAYLAND"));
    nx_window *window = NULL;
    nx_window_desc desc = {"NexaUI Vulkan drawing test", 160, 112, 0, {0}};
    nx_result result = nx_window_create(&desc, &window);
    if (result == NX_ERROR_PLATFORM && !required) return 77;
    CHECK(result == NX_OK);
    int status = test_draws(window);
    if (!status) status = test_preflight_and_driver(window);
    if (!status) status = test_independent_windows(window);
    nx_window_destroy(window);
    return status;
}
