#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "nexaui/nexaui.h"
#include "window_internal.h"
#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_COUNT 21u
#define WARMUP_COUNT 5u

static int compare_double(const void *left, const void *right) {
    double a = *(const double *)left, b = *(const double *)right;
    return (a > b) - (a < b);
}

static int run_scene(nx_renderer_driver *driver, nx_render_target target,
                     const nx_command *commands, size_t count,
                     const char *name) {
    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency)) return 1;
    double samples[SAMPLE_COUNT];
    double gpu_samples[SAMPLE_COUNT];
    unsigned gpu_count = 0;
    uint64_t last_timed_frame = 0;
    for (unsigned i = 0; i < WARMUP_COUNT + SAMPLE_COUNT; ++i) {
        LARGE_INTEGER start, end;
        nx_render_outcome outcome = NX_RENDER_SUSPENDED;
        QueryPerformanceCounter(&start);
        nx_result result = nx_renderer_run(driver, target, commands, count,
            (nx_color){0.08f, 0.12f, 0.18f, 1}, &outcome);
        QueryPerformanceCounter(&end);
        if (result != NX_OK || outcome != NX_RENDER_READY) {
            fprintf(stderr, "%s: render failed (%d, %d)\n", name,
                    result, outcome);
            return 1;
        }
        if (i >= WARMUP_COUNT) {
            samples[i - WARMUP_COUNT] =
                1000.0 * (double)(end.QuadPart - start.QuadPart) /
                (double)frequency.QuadPart;
            nx_d3d12_renderer_info frame_info = {0};
            if (nx_d3d12_renderer_get_info(driver->backend.user,
                                            &frame_info) != NX_OK)
                return 1;
            if (frame_info.gpu_timestamp_frequency &&
                frame_info.last_timed_frame > last_timed_frame) {
                gpu_samples[gpu_count++] = frame_info.last_gpu_frame_ms;
                last_timed_frame = frame_info.last_timed_frame;
            }
        }
    }
    qsort(samples, SAMPLE_COUNT, sizeof(samples[0]), compare_double);
    qsort(gpu_samples, gpu_count, sizeof(gpu_samples[0]), compare_double);
    nx_d3d12_renderer_info info = {0};
    if (nx_d3d12_renderer_get_info(driver->backend.user, &info) != NX_OK)
        return 1;
    printf("%-24s n=%zu draws=%llu uploads=%llu copied=%llu px min=%.3f median=%.3f max=%.3f ms\n",
           name, count, (unsigned long long)info.last_draw_calls,
           (unsigned long long)info.last_bitmap_uploads,
           (unsigned long long)info.last_composite_copy_texels,
           samples[0], samples[SAMPLE_COUNT / 2u],
           samples[SAMPLE_COUNT - 1u]);
    if (gpu_count)
        printf("  completed GPU frames: n=%u total min=%.3f median=%.3f max=%.3f ms; latest upload=%.3f render=%.3f ms\n",
               gpu_count, gpu_samples[0], gpu_samples[gpu_count / 2u],
               gpu_samples[gpu_count - 1u], info.last_gpu_upload_ms,
               info.last_gpu_render_ms);
    else
        puts("  GPU timestamps unavailable");
    return 0;
}

int main(void) {
    nx_window_desc desc = {0};
    desc.title_utf8 = "NexaUI D3D12 benchmark";
    desc.width = 640;
    desc.height = 480;
    desc.flags = NX_WINDOW_HIDDEN;
    nx_window *window = NULL;
    if (nx_window_create(&desc, &window) != NX_OK) return 1;
    nx_renderer_driver driver = {0};
    if (nx_d3d12_driver_open(window, &driver) != NX_OK) {
        nx_window_destroy(window);
        return 1;
    }
    RECT client;
    if (!GetClientRect(nx_window_native_hwnd(window), &client)) {
        nx_renderer_close(&driver);
        nx_window_destroy(window);
        return 1;
    }
    nx_render_target target = {(uint32_t)client.right,
                               (uint32_t)client.bottom, 1};
    nx_d3d12_renderer_info info = {0};
    if (nx_d3d12_renderer_get_info(driver.backend.user, &info) != NX_OK) {
        nx_renderer_close(&driver);
        nx_window_destroy(window);
        return 1;
    }
    printf("D3D12 %ux%u, adapter=%s, Present(1), end-to-end wall time\n",
           target.pixel_width, target.pixel_height,
           info.using_warp ? "WARP" : "default");
    nx_command opaque[1000] = {0};
    for (unsigned i = 0; i < 1000; ++i) {
        opaque[i].type = NX_COMMAND_FILL_RECT;
        opaque[i].rect = (nx_rect){(float)((i * 41u) % 620u),
                                   (float)((i * 29u) % 460u), 21, 17};
        opaque[i].color = (nx_color){(float)(i % 7u) / 6.0f,
                                    (float)(i % 11u) / 10.0f, 0.7f, 1};
    }
    nx_command alpha[128] = {0};
    for (unsigned i = 0; i < 128; ++i) {
        alpha[i].type = NX_COMMAND_FILL_RECT;
        alpha[i].rect = (nx_rect){(float)((i * 53u) % 620u),
                                  (float)((i * 37u) % 460u), 25, 21};
        alpha[i].color = (nx_color){0.8f, 0.3f, 0.2f, 0.5f};
    }
    uint8_t image[16u * 16u * 4u];
    for (unsigned y = 0; y < 16; ++y)
        for (unsigned x = 0; x < 16; ++x) {
            size_t p = ((size_t)y * 16u + x) * 4u;
            image[p] = (uint8_t)(x * 17u);
            image[p + 1u] = (uint8_t)(y * 17u);
            image[p + 2u] = 180u;
            image[p + 3u] = (uint8_t)(64u + (x + y) * 6u);
        }
    nx_command bitmaps[128] = {0};
    uint8_t unique_images[128u * sizeof(image)];
    nx_command unique_bitmaps[128] = {0};
    for (unsigned i = 0; i < 128; ++i) {
        bitmaps[i].type = NX_COMMAND_BITMAP;
        bitmaps[i].rect = (nx_rect){(float)((i * 43u) % 620u),
                                    (float)((i * 31u) % 460u), 20, 20};
        bitmaps[i].color = (nx_color){1, 1, 1, 1};
        bitmaps[i].bitmap = (nx_bitmap_view){image, 16, 16, 64,
                                              NX_BITMAP_RGBA8};
        memcpy(unique_images + (size_t)i * sizeof(image), image,
               sizeof(image));
        unique_images[(size_t)i * sizeof(image)] = (uint8_t)i;
        unique_bitmaps[i] = bitmaps[i];
        unique_bitmaps[i].bitmap.pixels =
            unique_images + (size_t)i * sizeof(image);
    }
    int failed = run_scene(&driver, target, opaque, 1000, "opaque rectangles") ||
                 run_scene(&driver, target, alpha, 128, "alpha rectangles") ||
                 run_scene(&driver, target, bitmaps, 128, "shared RGBA8 bitmap") ||
                 run_scene(&driver, target, unique_bitmaps, 128,
                           "unique RGBA8 bitmaps");
    nx_renderer_close(&driver);
    nx_window_destroy(window);
    return failed;
}
