#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include "scene.h"
#include "window_internal.h"
#include "renderer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int frame(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer) {
    nx_window_info info; size_t routed;
    if (nx_window_poll(window, 0) != NX_OK || nx_window_get_info(window, &info) != NX_OK ||
        nx_window_route_events(window, cloud_context(scene), &routed) != NX_OK) return 0;
    if (info.should_close) return 1;
    if (info.width <= 0 || info.height <= 0) return nx_window_poll(window, 16) == NX_OK;
    if (!cloud_frame(scene, info.width, info.height, 1.0 / 60.0)) return 0;
    if (nx_window_publish_semantics(window,cloud_context(scene)) != NX_OK) return 0;
    if (nx_window_set_cursor(window, cloud_cursor(scene)) != NX_OK) return 0;
    const nx_command *commands; size_t count;
    if (nx_frame_commands(cloud_context(scene), &commands, &count) != NX_OK) return 0;
    RECT client;
    if (!GetClientRect(nx_window_native_hwnd(window), &client)) return 0;
    nx_render_target target = {(uint32_t)client.right, (uint32_t)client.bottom, info.scale};
    nx_render_outcome outcome;
    nx_result result = nx_renderer_run(renderer, target, commands, count,
        (nx_color){0, 0, 0, 1}, &outcome);
    if (result != NX_OK) { fprintf(stderr, "D3D12 frame: %d\n", result); return 0; }
    return outcome != NX_RENDER_DEVICE_LOST || nx_renderer_recover(renderer) == NX_OK;
}

#ifdef NEXAUI_D3D12_TEST_HOOKS
static int capture(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer, const char *path) {
    RECT client; nx_window_info info;
    if (!GetClientRect(nx_window_native_hwnd(window), &client) ||
        nx_window_get_info(window, &info) != NX_OK) return 0;
    uint32_t w = (uint32_t)client.right, h = (uint32_t)client.bottom;
    const nx_command *commands; size_t count;
    if (nx_frame_commands(cloud_context(scene), &commands, &count) != NX_OK) return 0;
    uint8_t *pixels = malloc((size_t)w * h * 4u);
    if (!pixels) return 0;
    int okay = nx_d3d12_test_render_readback(renderer->backend.user, commands, count,
        info.scale, (nx_color){0,0,0,1}, pixels, w * 4u) == NX_OK;
    FILE *file = okay ? fopen(path, "wb") : NULL;
    if (!file) okay = 0;
    if (okay) okay = fprintf(file, "P6\n%u %u\n255\n", w, h) > 0;
    for (size_t i = 0; okay && i < (size_t)w * h; ++i) okay = fwrite(pixels + i * 4u, 1, 3, file) == 3;
    if (file && fclose(file) != 0) okay = 0;
    free(pixels); return okay;
}

static int reflow_frame(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer) {
    return frame(window, scene, renderer);
}
#include "native_reflow.h"

static int smoke(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer, const char *prefix) {
    char path[1024]; nx_window_info info;
    if (!frame(window, scene, renderer) || nx_window_get_info(window, &info) != NX_OK) return 0;
    snprintf(path, sizeof(path), "%s-native-overview.ppm", prefix);
    if (!capture(window, scene, renderer, path)) return 0;
    HWND hwnd = nx_window_native_hwnd(window);
    LPARAM pos = MAKELPARAM((int)(100 * info.scale), (int)(195 * info.scale));
    if (!PostMessageW(hwnd, WM_MOUSEMOVE, 0, pos) ||
        !PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, pos) ||
        !PostMessageW(hwnd, WM_LBUTTONUP, 0, pos) || !frame(window, scene, renderer) ||
        !frame(window, scene, renderer)) return 0;
    uint32_t index; nx_semantic_view view;
    if (nx_semantics_find(cloud_context(scene), 1000, &index) != NX_OK ||
        nx_semantics_get(cloud_context(scene), index, &view) != NX_OK ||
        !(view.state & NX_SEM_SELECTED)) return 0;
    snprintf(path, sizeof(path), "%s-native-files.ppm", prefix);
    if (!capture(window, scene, renderer, path) ||
        nx_focus_set(cloud_context(scene), 1001) != NX_OK ||
        !PostMessageW(hwnd, WM_KEYDOWN, VK_SPACE, 0) ||
        !PostMessageW(hwnd, WM_KEYUP, VK_SPACE, 0) ||
        !frame(window, scene, renderer) || !frame(window, scene, renderer)) return 0;
    if (nx_semantics_find(cloud_context(scene), 1001, &index) != NX_OK ||
        nx_semantics_get(cloud_context(scene), index, &view) != NX_OK ||
        !(view.state & NX_SEM_SELECTED)) return 0;
    if (!SetWindowPos(hwnd, NULL, 0, 0, 820, 700, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) ||
        !frame(window, scene, renderer)) return 0;
    snprintf(path, sizeof(path), "%s-native-compact.ppm", prefix);
    if (!capture(window, scene, renderer, path)) return 0;
    LARGE_INTEGER frequency, start, end;
    if (!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&start)) return 0;
    for (unsigned i = 0; i < 30; ++i) if (!frame(window, scene, renderer)) return 0;
    if (!QueryPerformanceCounter(&end)) return 0;
    nx_d3d12_renderer_info gpu;
    if (nx_d3d12_renderer_get_info(renderer->backend.user, &gpu) != NX_OK) return 0;
    printf("30 warm compact frames: %.3f ms mean wall time (includes event polling); "
        "last GPU frame %.3f ms, %llu draw calls, WARP=%d\n",
        1000.0 * (double)(end.QuadPart - start.QuadPart) / (double)frequency.QuadPart / 30.0,
        gpu.last_gpu_frame_ms, (unsigned long long)gpu.last_draw_calls, gpu.using_warp);
    return native_reflow(window, scene, renderer, prefix, "native");
}
#endif

int main(int argc, char **argv) {
    int testing = argc == 4 && strcmp(argv[1], "--smoke") == 0;
    if (argc != 2 && !testing) {
        fputs("usage: nexaui_nexacloud_native FONT.ttf | --smoke FONT.ttf OUTPUT_PREFIX\n", stderr); return 2;
    }
    cloud_scene *scene = cloud_create(argv[testing ? 2 : 1]);
    nx_window *window = NULL; nx_renderer_driver renderer = {0};
    nx_window_desc desc = {0}; desc.title_utf8 = "NexaCloud — NexaUI";
    desc.width = 1440; desc.height = 1040; desc.flags = testing ? NX_WINDOW_HIDDEN : 0;
    int okay = scene && nx_window_create(&desc, &window) == NX_OK &&
        nx_d3d12_driver_open(window, &renderer) == NX_OK;
    if (okay) cloud_set_window(scene, window);
#ifdef NEXAUI_D3D12_TEST_HOOKS
    if (okay && testing) okay = smoke(window, scene, &renderer, argv[3]);
#else
    if (testing) okay = 0;
#endif
    while (okay && !testing) {
        nx_window_info info;
        if (nx_window_get_info(window, &info) != NX_OK) { okay = 0; break; }
        if (info.should_close) break;
        okay = frame(window, scene, &renderer);
    }
    nx_renderer_close(&renderer); cloud_destroy(scene); nx_window_destroy(window);
    return okay ? 0 : 1;
}
