#define _POSIX_C_SOURCE 200809L
#include "renderer.h"
#include "window_internal.h"
#include "x11/window_internal.h"
#include "wayland/window_internal.h"

#include <X11/Xutil.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

typedef struct allocation { void *pointer; size_t size; } allocation;
typedef struct tracker { allocation blocks[32]; size_t live, calls, fail_at; } tracker;

static void *allocate(void *user, size_t size) {
    tracker *track = user;
    if (++track->calls == track->fail_at) return NULL;
    if (track->live == 32u) abort();
    void *pointer = malloc(size);
    if (!pointer) abort();
    track->blocks[track->live++] = (allocation){pointer, size};
    return pointer;
}

static void deallocate(void *user, void *pointer, size_t size) {
    tracker *track = user;
    size_t i = 0;
    while (i < track->live && track->blocks[i].pointer != pointer) ++i;
    if (i == track->live || track->blocks[i].size != size) abort();
    track->blocks[i] = track->blocks[--track->live];
    free(pointer);
}

static int show_frame(nx_window *window, nx_vulkan_renderer *renderer, nx_color color) {
    for (unsigned i = 0; i < 10; ++i) {
        CHECK(nx_window_poll(window, 0) == NX_OK);
        int presented = 0;
        CHECK(nx_vulkan_clear_present(renderer, color, &presented) == NX_OK);
        if (presented) return 0;
        CHECK(nx_window_poll(window, 10) == NX_OK);
    }
    return 1;
}

static int read_pixels(nx_window *window, nx_vulkan_renderer *renderer) {
    nx_vulkan_renderer_info info;
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK);
    CHECK(info.pixel_width && info.pixel_height && info.readback_supported);
    size_t stride = (size_t)info.pixel_width * 4u + 13u;
    size_t bytes = stride * info.pixel_height;
    uint8_t *actual = malloc(bytes), *expected = malloc(bytes);
    CHECK(actual && expected);
    const uint8_t colors[][4] = {{17, 83, 201, 255}, {243, 31, 5, 93}, {0, 255, 127, 0}};
    for (unsigned i = 0; i < 3; ++i) {
        nx_color clear = {colors[i][0] / 255.0f, colors[i][1] / 255.0f,
                          colors[i][2] / 255.0f, colors[i][3] / 255.0f};
        memset(actual, 0xa5, bytes);
        memset(expected, 0xa5, bytes);
        CHECK(nx_headless_render(NULL, 0, expected, info.pixel_width,
                                 info.pixel_height, stride, clear) == NX_OK);
        CHECK(nx_vulkan_test_readback(renderer, clear, actual, stride) == NX_OK);
        CHECK(memcmp(actual, expected, bytes) == 0);
    }
    free(expected);
    free(actual);
    CHECK(show_frame(window, renderer, (nx_color){17 / 255.0f, 83 / 255.0f, 201 / 255.0f, 1}) == 0);
    return 0;
}

static unsigned channel(unsigned long pixel, unsigned long mask) {
    if (!mask) return 0;
    while (!(mask & 1u)) { mask >>= 1u; pixel >>= 1u; }
    return (unsigned)(((pixel & mask) * 255u + mask / 2u) / mask);
}

static int inspect_x11_window(nx_window *window) {
    Display *display = nx_window_x11_display(window);
    if (!display) return 0;
    for (unsigned i = 0; i < 50u; ++i) {
        XSync(display, False);
        XImage *image = XGetImage(display, nx_window_x11_id(window),
                                  5, 5, 1, 1, AllPlanes, ZPixmap);
        CHECK(image);
        unsigned long pixel = XGetPixel(image, 0, 0);
        int match = channel(pixel, image->red_mask) == 17u &&
                    channel(pixel, image->green_mask) == 83u &&
                    channel(pixel, image->blue_mask) == 201u;
        XDestroyImage(image);
        if (match) return 0;
        CHECK(nx_window_poll(window, 10) == NX_OK);
    }
    fprintf(stderr, "Presented X11 window pixels did not match the clear\n");
    return 1;
}

typedef struct thread_state { nx_window *window; nx_vulkan_renderer *renderer; int passed; } thread_state;

static void *wrong_thread(void *user) {
    thread_state *state = user;
    nx_vulkan_renderer_info info;
    nx_vulkan_renderer *other = NULL;
    int presented = 1;
    state->passed = nx_vulkan_renderer_get_info(state->renderer, &info) == NX_ERROR_STATE &&
        !info.pixel_width && nx_vulkan_clear_present(state->renderer,
            (nx_color){0, 0, 0, 1}, &presented) == NX_ERROR_STATE && !presented &&
        nx_vulkan_renderer_create(state->window, NULL, &other) == NX_ERROR_STATE && !other;
    if (nx_vulkan_renderer_destroy(state->renderer) != NX_ERROR_STATE) state->passed = 0;
    return NULL;
}

static int test_driver(nx_window *window) {
    nx_renderer_driver driver = {0};
    CHECK(nx_vulkan_driver_open(window, &driver) == NX_OK);
    nx_window_info window_info;
    CHECK(nx_window_get_info(window, &window_info) == NX_OK);
    nx_render_target target = {(uint32_t)ceil(window_info.width * window_info.scale),
        (uint32_t)ceil(window_info.height * window_info.scale), window_info.scale};
    nx_render_outcome outcome;
    CHECK(nx_renderer_run(&driver, (nx_render_target){0, 0, 1}, NULL, 0,
                          (nx_color){0, 0, 0, 1}, &outcome) == NX_OK);
    CHECK(outcome == NX_RENDER_SUSPENDED);
    nx_command unsupported = {0};
    static const uint8_t source_pixel[4] = {255,255,255,255};
    unsupported.type = NX_COMMAND_BITMAP;
    unsupported.bitmap = (nx_bitmap_view){source_pixel, 1, 1, 4, NX_BITMAP_RGBA8};
    unsupported.rect = (nx_rect){0, 0, 10, 10};
    unsupported.color = (nx_color){1, 1, 1, 1};
    CHECK(nx_renderer_run(&driver, target, &unsupported, 1,
                          (nx_color){0, 0, 0, 1}, &outcome) == NX_ERROR_UNSUPPORTED);
    nx_vulkan_renderer_info before, after;
    CHECK(nx_vulkan_renderer_get_info(driver.backend.user, &before) == NX_OK);
    CHECK(!before.presented_frames);
    CHECK(nx_renderer_run(&driver, target, NULL, 0,
                          (nx_color){0.1f, 0.2f, 0.3f, 1}, &outcome) == NX_OK);
    CHECK(outcome == NX_RENDER_READY);
    CHECK(nx_vulkan_test_invalidate(driver.backend.user, 0) == NX_OK);
    CHECK(nx_renderer_run(&driver, target, NULL, 0,
                          (nx_color){0.1f, 0.2f, 0.3f, 1}, &outcome) == NX_OK);
    CHECK(outcome == NX_RENDER_READY);
    CHECK(nx_vulkan_renderer_get_info(driver.backend.user, &after) == NX_OK);
    CHECK(after.swapchain_generation > before.swapchain_generation);
    CHECK(nx_vulkan_test_invalidate(driver.backend.user, 1) == NX_OK);
    CHECK(nx_renderer_run(&driver, target, NULL, 0,
                          (nx_color){0, 0, 0, 1}, &outcome) == NX_OK);
    CHECK(outcome == NX_RENDER_DEVICE_LOST);
    CHECK(nx_renderer_recover(&driver) == NX_OK);
    CHECK(nx_renderer_run(&driver, target, NULL, 0,
                          (nx_color){0, 0, 0, 1}, &outcome) == NX_OK);
    CHECK(outcome == NX_RENDER_READY);
    CHECK(nx_vulkan_test_fail_next(driver.backend.user, NX_VK_TEST_RETIRE_TIMEOUT) == NX_OK);
    CHECK(nx_renderer_close(&driver) == NX_ERROR_LIMIT);
    CHECK(driver.open && driver.closing && driver.backend.user);
    CHECK(nx_renderer_run(&driver, target, NULL, 0,
                         (nx_color){0, 0, 0, 1}, &outcome) == NX_ERROR_STATE);
    CHECK(nx_renderer_close(&driver) == NX_OK);
    CHECK(!driver.open);
    return 0;
}

static int test_renderer(nx_window *window) {
    tracker track = {0};
    nx_allocator allocator = {&track, allocate, deallocate};
    nx_vulkan_renderer *renderer = NULL;
    CHECK(nx_vulkan_renderer_create(window, &allocator, &renderer) == NX_OK);
    size_t allocation_calls = track.calls;
    nx_vulkan_renderer_info info;
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK);
    CHECK(info.device.has_surface && !info.presented_frames && !info.image_count);
    CHECK(info.device.present_fences != NX_VK_PRESENT_FENCES_NONE);
    nx_vulkan_renderer *duplicate = NULL;
    CHECK(nx_vulkan_renderer_create(window, NULL, &duplicate) == NX_ERROR_STATE && !duplicate);
    int token = 0;
    CHECK(nx_window_renderer_acquire(window, &token) == NX_ERROR_STATE);
    CHECK(nx_window_renderer_release(window, &token) == NX_ERROR_STATE);
    int presented = 9;
    CHECK(nx_vulkan_clear_present(renderer, (nx_color){NAN, 0, 0, 1}, &presented) == NX_ERROR_ARGUMENT);
    CHECK(!presented);
    pthread_t thread;
    thread_state state = {window, renderer, 0};
    CHECK(pthread_create(&thread, NULL, wrong_thread, &state) == 0);
    CHECK(pthread_join(thread, NULL) == 0 && state.passed);
    for (unsigned i = 0; i < 32u; ++i)
        CHECK(show_frame(window, renderer, (nx_color){0.2f, 0.3f, 0.7f, 1}) == 0);
    CHECK(track.calls == allocation_calls); /* Reuse NexaUI frame bookkeeping. */
    CHECK(read_pixels(window, renderer) == 0);
    CHECK(inspect_x11_window(window) == 0);
    if (nx_window_wayland_display(window)) {
        int bg_width, bg_height, cursor_width, cursor_height;
        CHECK(nx_window_wayland_test_extents(window, &bg_width, &bg_height,
                                             &cursor_width, &cursor_height) == NX_OK);
        CHECK(nx_window_wayland_test_output(window, 2, 1) == NX_OK);
        int unchanged_width, unchanged_height;
        CHECK(nx_window_wayland_test_extents(window, &unchanged_width, &unchanged_height,
                                             &cursor_width, &cursor_height) == NX_OK);
        CHECK(unchanged_width == bg_width && unchanged_height == bg_height);
        CHECK(show_frame(window, renderer, (nx_color){0, 0, 1, 1}) == 0);
        CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK);
        nx_window_info native;
        CHECK(nx_window_get_info(window, &native) == NX_OK && native.scale == 2);
        CHECK(info.pixel_width == (uint32_t)ceil(native.width * 2));
        CHECK(read_pixels(window, renderer) == 0);
        CHECK(nx_window_wayland_test_output(window, 1, 0) == NX_OK);
    } else {
        Display *display = nx_window_x11_display(window);
        XResizeWindow(display, nx_window_x11_id(window), 320, 200);
        XSync(display, False);
        nx_window_info native = {0};
        for (unsigned i = 0; i < 80; ++i) {
            CHECK(nx_window_poll(window, 25) == NX_OK);
            CHECK(nx_window_get_info(window, &native) == NX_OK);
            if (native.width == 320 && native.height == 200) break;
        }
        if (native.width != 320 || native.height != 200)
            fprintf(stderr, "X11 resize resulted in %.0fx%.0f\n", native.width, native.height);
        CHECK(native.width == 320 && native.height == 200);
        CHECK(show_frame(window, renderer, (nx_color){0, 1, 0, 1}) == 0);
        CHECK(read_pixels(window, renderer) == 0);
        CHECK(inspect_x11_window(window) == 0);
    }
    CHECK(nx_vulkan_renderer_recover(renderer) == NX_OK);
    CHECK(show_frame(window, renderer, (nx_color){1, 0, 0, 1}) == 0);
    CHECK(read_pixels(window, renderer) == 0);
    CHECK(nx_vulkan_renderer_get_info(renderer, &info) == NX_OK);
    printf("%s: %s, %ux%u, %u images, %llu presents, graphics=%u present=%u\n",
        getenv("NEXAUI_PLATFORM"), info.device.name, info.pixel_width,
        info.pixel_height, info.image_count, (unsigned long long)info.presented_frames,
        info.device.graphics_queue_family, info.device.present_queue_family);
    CHECK(nx_vulkan_renderer_destroy(renderer) == NX_OK);
    CHECK(!track.live);
    /* Fail each bookkeeping allocation, then reacquire the same window. */
    for (size_t fail = 1; fail <= allocation_calls; ++fail) {
        track = (tracker){0};
        track.fail_at = fail;
        CHECK(nx_vulkan_renderer_create(window, &allocator, &renderer) == NX_ERROR_MEMORY);
        CHECK(!renderer && !track.live);
    }
    CHECK(test_driver(window) == 0);
    return 0;
}

static int test_retirement_failures(nx_window *window) {
    nx_vulkan_renderer *renderer = NULL;
    nx_vulkan_renderer_info before, after;
    const nx_color clear = {0.1f, 0.2f, 0.3f, 1};
    CHECK(nx_vulkan_renderer_create(window, NULL, &renderer) == NX_OK);
    CHECK(show_frame(window, renderer, clear) == 0);
    CHECK(nx_vulkan_renderer_get_info(renderer, &before) == NX_OK);
    CHECK(before.pending_present_fences && before.device.present_fences);
    CHECK(nx_vulkan_test_invalidate(renderer, 0) == NX_OK);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_RETIRE_TIMEOUT) == NX_OK);
    int presented = 1;
    CHECK(nx_vulkan_clear_present(renderer, clear, &presented) == NX_OK && !presented);
    CHECK(nx_vulkan_renderer_get_info(renderer, &after) == NX_OK);
    CHECK(after.swapchain_generation == before.swapchain_generation &&
          after.image_count == before.image_count && after.pending_present_fences);
    CHECK(show_frame(window, renderer, clear) == 0);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_PRESENT_WAIT_TIMEOUT) == NX_OK);
    int held = 0;
    for (unsigned i = 0; i < 128u; ++i) {
        CHECK(nx_vulkan_clear_present(renderer, clear, &presented) == NX_OK);
        CHECK(nx_vulkan_renderer_get_info(renderer, &before) == NX_OK);
        if (!presented && before.has_acquired_image) { held = 1; break; }
    }
    CHECK(held);
    CHECK(nx_vulkan_test_invalidate(renderer, 0) == NX_OK);
    CHECK(show_frame(window, renderer, clear) == 0);
    CHECK(nx_vulkan_renderer_get_info(renderer, &after) == NX_OK);
    CHECK(!after.has_acquired_image && after.swapchain_generation == before.swapchain_generation);
    CHECK(show_frame(window, renderer, clear) == 0);
    CHECK(nx_vulkan_renderer_get_info(renderer, &after) == NX_OK);
    CHECK(after.swapchain_generation > before.swapchain_generation && after.completed_present_fences);
    CHECK(nx_vulkan_test_invalidate(renderer, 0) == NX_OK);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_REBUILD_MEMORY) == NX_OK);
    CHECK(nx_vulkan_clear_present(renderer, clear, &presented) == NX_ERROR_MEMORY);
    CHECK(nx_vulkan_renderer_get_info(renderer, &after) == NX_OK && !after.image_count);
    CHECK(show_frame(window, renderer, clear) == 0);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_PRESENT_OOM) == NX_OK);
    CHECK(nx_vulkan_clear_present(renderer, clear, &presented) == NX_ERROR_MEMORY && !presented);
    CHECK(nx_vulkan_renderer_get_info(renderer, &before) == NX_OK && before.has_acquired_image);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_RETIRE_TIMEOUT) == NX_OK);
    CHECK(nx_vulkan_renderer_recover(renderer) == NX_ERROR_LIMIT);
    CHECK(nx_vulkan_renderer_get_info(renderer, &after) == NX_OK &&
          after.swapchain_generation == before.swapchain_generation);
    CHECK(nx_vulkan_renderer_recover(renderer) == NX_OK);
    CHECK(show_frame(window, renderer, clear) == 0);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_PRESENT_OUT_OF_DATE) == NX_OK);
    CHECK(nx_vulkan_clear_present(renderer, clear, &presented) == NX_OK && !presented);
    CHECK(nx_vulkan_renderer_get_info(renderer, &before) == NX_OK &&
          !before.has_acquired_image && before.pending_present_fences);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_RETIRE_TIMEOUT) == NX_OK);
    CHECK(nx_vulkan_renderer_destroy(renderer) == NX_ERROR_LIMIT);
    CHECK(nx_vulkan_renderer_get_info(renderer, &after) == NX_OK && after.closing &&
          after.image_count == before.image_count);
    CHECK(nx_vulkan_clear_present(renderer, clear, &presented) == NX_ERROR_STATE);
    CHECK(nx_vulkan_renderer_recover(renderer) == NX_ERROR_STATE);
    nx_vulkan_renderer *other = NULL;
    CHECK(nx_vulkan_renderer_create(window, NULL, &other) == NX_ERROR_STATE && !other);
    CHECK(nx_vulkan_renderer_destroy(renderer) == NX_OK);
    for (int policy = NX_VULKAN_RETIRE_COMPATIBLE; policy <= NX_VULKAN_RETIRE_LEGACY; ++policy) {
        nx_vulkan_renderer_desc desc = {0};
        desc.retirement = (nx_vulkan_retirement_policy)policy;
        CHECK(nx_vulkan_renderer_create_config(window, &desc, &renderer) == NX_OK);
        CHECK(show_frame(window, renderer, clear) == 0);
        CHECK(nx_vulkan_renderer_get_info(renderer, &after) == NX_OK);
        CHECK((after.device.present_fences == NX_VK_PRESENT_FENCES_NONE) ==
              (policy == NX_VULKAN_RETIRE_LEGACY));
        CHECK(nx_vulkan_renderer_destroy(renderer) == NX_OK);
    }
    CHECK(nx_vulkan_renderer_create(window, NULL, &renderer) == NX_OK);
    CHECK(nx_vulkan_test_fail_next(renderer, NX_VK_TEST_SUBMISSION_WAIT_TIMEOUT) == NX_OK);
    /* The readback submits real GPU work before its two waits time out.
       Its private storage must survive the call and be retired on retry. */
    nx_window_info native;
    CHECK(nx_window_get_info(window, &native) == NX_OK);
    size_t stride = (size_t)ceil(native.width * native.scale) * 4u;
    unsigned char *pixels = malloc(stride * (size_t)ceil(native.height * native.scale));
    CHECK(pixels);
    nx_result result = nx_vulkan_test_readback(renderer, clear, pixels, stride);
    free(pixels);
    CHECK(result == NX_ERROR_LIMIT);
    CHECK(nx_vulkan_renderer_get_info(renderer, &after) == NX_OK && after.pending_readback);
    CHECK(read_pixels(window, renderer) == 0);
    CHECK(nx_vulkan_renderer_get_info(renderer, &after) == NX_OK && !after.pending_readback);
    CHECK(nx_vulkan_renderer_destroy(renderer) == NX_OK);
    return 0;
}

int main(void) {
    const char *backend = getenv("NEXAUI_PLATFORM");
    if (!backend) return 1;
    int required = getenv("NEXAUI_REQUIRE_VULKAN") &&
        (!strcmp(backend, "x11") || getenv("NEXAUI_REQUIRE_VULKAN_WAYLAND"));
    nx_window *window = NULL;
    nx_window_desc desc = {"NexaUI Vulkan presentation test", 128, 96, 0, {0}};
    nx_result result = nx_window_create(&desc, &window);
    if (result == NX_ERROR_PLATFORM && !required) return 77;
    if (result != NX_OK) {
        fprintf(stderr, "%s window creation failed: %d\n", backend, (int)result);
        return 1;
    }
    int status = test_renderer(window);
    if (!status) status = test_retirement_failures(window);
    nx_window_destroy(window);
    return status;
}
