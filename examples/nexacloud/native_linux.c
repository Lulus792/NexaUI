#define _POSIX_C_SOURCE 200809L
#include "scene.h"
#include "renderer.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef NEXAUI_VULKAN_TEST_HOOKS
static int cloud_vk_profile_active;
#endif
static double now(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return -1;
    return (double)t.tv_sec + (double)t.tv_nsec / 1.e9;
}

static int frame(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer, double dt) {
    nx_window_info info; size_t routed;
    if (nx_window_poll(window, 0) != NX_OK || nx_window_get_info(window, &info) != NX_OK) return 0;
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    if (cloud_vk_profile_active) {
        /* Hold the workload's input constant without warping the real cursor.
           Native polling still handles window size/close; physical UI events
           cannot change measured hover/selection between comparison blocks. */
        nx_event event; int available;
        do { if (nx_window_next_event(window,&event,&available) != NX_OK) return 0; } while (available);
        event=(nx_event){0}; event.type=NX_EVENT_POINTER_MOVE; event.x=event.y=-1000;
        if (nx_context_queue_event(cloud_context(scene),&event) != NX_OK) return 0;
    } else
#endif
    if (nx_window_route_events(window, cloud_context(scene), &routed) != NX_OK) return 0;
    if (info.should_close) return 1;
    if (info.width <= 0 || info.height <= 0) return nx_window_poll(window, 16) == NX_OK;
    if (!cloud_frame(scene, info.width, info.height, dt) ||
        nx_window_set_cursor(window, cloud_cursor(scene)) != NX_OK) return 0;
    nx_result accessibility=nx_window_publish_semantics(window,cloud_context(scene));
    if (accessibility!=NX_OK && accessibility!=NX_ERROR_UNSUPPORTED) return 0;
    const nx_command *commands; size_t count;
    if (nx_frame_commands(cloud_context(scene), &commands, &count) != NX_OK) return 0;
    double width = ceil((double)info.width * info.scale), height = ceil((double)info.height * info.scale);
    if (width > UINT32_MAX || height > UINT32_MAX) return 0;
    nx_render_target target = {(uint32_t)width, (uint32_t)height, info.scale};
    nx_render_outcome outcome;
    nx_result result = nx_renderer_run(renderer, target, commands, count, (nx_color){0,0,0,1}, &outcome);
    if (result != NX_OK) { fprintf(stderr, "Vulkan frame: %d\n", result); return 0; }
    if (outcome == NX_RENDER_SUSPENDED) return nx_window_poll(window, 4) == NX_OK;
    return outcome != NX_RENDER_DEVICE_LOST || nx_renderer_recover(renderer) == NX_OK;
}

static int close_renderer(nx_window *window, nx_renderer_driver *renderer) {
    nx_result result = nx_renderer_close(renderer);
    for (unsigned i = 0; result == NX_ERROR_LIMIT && i < 4; ++i) {
        if (nx_window_poll(window, 16) != NX_OK) return 0;
        result = nx_renderer_close(renderer);
    }
    if (result != NX_OK) fprintf(stderr, "Renderer retirement incomplete: %d\n", result);
    return result == NX_OK;
}

#ifdef NEXAUI_VULKAN_TEST_HOOKS
static int cloud_vk_profile_enabled;
static int cloud_vk_profile_ring;
static int capture(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer, const char *path) {
    nx_window_info info; nx_vulkan_renderer_info gpu;
    const nx_command *commands; size_t count;
    if (nx_window_get_info(window, &info) != NX_OK ||
        nx_vulkan_renderer_get_info(renderer->backend.user, &gpu) != NX_OK ||
        nx_frame_commands(cloud_context(scene), &commands, &count) != NX_OK ||
        !gpu.pixel_width || !gpu.pixel_height || count > SIZE_MAX / sizeof(nx_command)) return 0;
    if ((uint64_t)gpu.pixel_width * gpu.pixel_height > SIZE_MAX / 4u) return 0;
    size_t bytes = (size_t)gpu.pixel_width * gpu.pixel_height * 4u;
    uint8_t *pixels = malloc(bytes), *reference = malloc(bytes), *row = malloc((size_t)gpu.pixel_width * 3u);
    nx_command *scaled = malloc((count ? count : 1) * sizeof(*scaled));
    int okay = pixels && reference && scaled && row;
    if (okay) okay = nx_vulkan_test_render_readback(renderer->backend.user, commands, count,
        info.scale, (nx_color){0,0,0,1}, pixels, (size_t)gpu.pixel_width * 4u) == NX_OK;
    for (size_t i = 0; okay && i < count; ++i) {
        scaled[i] = commands[i];
        if (scaled[i].type == NX_COMMAND_FILL_TRIANGLE || scaled[i].type == NX_COMMAND_LINE) {
            unsigned points = scaled[i].type == NX_COMMAND_LINE ? 2u : 3u;
            for (unsigned j = 0; j < points; ++j) {
                scaled[i].points[j].x = (float)(scaled[i].points[j].x * info.scale);
                scaled[i].points[j].y = (float)(scaled[i].points[j].y * info.scale);
            }
        } else if (scaled[i].type != NX_COMMAND_POP_CLIP) {
            scaled[i].rect.x = (float)(scaled[i].rect.x * info.scale);
            scaled[i].rect.y = (float)(scaled[i].rect.y * info.scale);
            scaled[i].rect.width = (float)(scaled[i].rect.width * info.scale);
            scaled[i].rect.height = (float)(scaled[i].rect.height * info.scale);
        }
        if (scaled[i].type == NX_COMMAND_STROKE_RECT || scaled[i].type == NX_COMMAND_LINE)
            scaled[i].thickness = (float)(scaled[i].thickness * info.scale);
        if (scaled[i].type == NX_COMMAND_FILL_ROUND_RECT) scaled[i].radius = (float)(scaled[i].radius * info.scale);
    }
    if (okay) okay = nx_headless_render(scaled, count, reference, gpu.pixel_width, gpu.pixel_height,
        (size_t)gpu.pixel_width * 4u, (nx_color){0,0,0,1}) == NX_OK;
    if (okay && memcmp(pixels, reference, bytes)) {
        for (size_t i = 0; i < bytes; ++i) if (pixels[i] != reference[i]) {
            fprintf(stderr, "NexaCloud GPU mismatch at (%zu,%zu) channel %zu: GPU=%u CPU=%u\n",
                (i / 4u) % gpu.pixel_width, i / (gpu.pixel_width * 4u), i % 4u, pixels[i], reference[i]); break;
        }
        okay = 0;
    }
    FILE *file = okay ? fopen(path, "wb") : NULL;
    if (!file) okay = 0;
    if (okay) okay = fprintf(file, "P6\n%u %u\n255\n", gpu.pixel_width, gpu.pixel_height) > 0;
    size_t row_bytes = (size_t)gpu.pixel_width * 3u;
    for (uint32_t y = 0; okay && y < gpu.pixel_height; ++y) {
        for (uint32_t x = 0; x < gpu.pixel_width; ++x)
            memcpy(row + (size_t)x * 3u, pixels + ((size_t)y * gpu.pixel_width + x) * 4u, 3);
        okay = fwrite(row, 1, row_bytes, file) == row_bytes;
    }
    if (file && fclose(file)) okay = 0;
    if (okay) printf("GPU/reference equal: %ux%u, %zu commands, %u bitmaps, atlas %ux%u\n",
        gpu.pixel_width, gpu.pixel_height, count, gpu.unique_bitmaps, gpu.atlas_width, gpu.atlas_height);
    free(row); free(scaled); free(reference); free(pixels); return okay;
}

static int reflow_frame(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer) {
    return frame(window, scene, renderer, 0);
}
#include "native_reflow.h"
#include "profile_linux.h"

static int queue_click(cloud_scene *scene, float x, float y) {
    nx_event event = {0}; event.type = NX_EVENT_POINTER_DOWN; event.button = NX_BUTTON_LEFT;
    event.x = x; event.y = y;
    if (nx_context_queue_event(cloud_context(scene), &event) != NX_OK) return 0;
    event.type = NX_EVENT_POINTER_UP;
    return nx_context_queue_event(cloud_context(scene), &event) == NX_OK;
}

static int smoke(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer, const char *prefix) {
    char path[1024];
    /* Normalized events exercise the common scene on both backends. This is
       not evidence of an installed compositor's physical input/IME behavior. */
    for (unsigned i = 0; i < 2; ++i) if (!frame(window, scene, renderer, 0)) return 0;
    uint32_t index; nx_semantic_view view;
    if (nx_semantics_find(cloud_context(scene), 11, &index) != NX_OK ||
        nx_semantics_get(cloud_context(scene), index, &view) != NX_OK) return 0;
    if (snprintf(path, sizeof(path), "%s-linux-overview.ppm", prefix) >= (int)sizeof(path) ||
        !capture(window, scene, renderer, path) ||
        !vk_profile_compare(window,scene,renderer,prefix,"overview-100") ||
        !queue_click(scene, view.bounds.x + view.bounds.width * .5f, view.bounds.y + view.bounds.height * .5f) ||
        !frame(window, scene, renderer, 0) || !frame(window, scene, renderer, 0)) return 0;
    if (nx_semantics_find(cloud_context(scene), 1000, &index) != NX_OK ||
        nx_semantics_get(cloud_context(scene), index, &view) != NX_OK || !(view.state & NX_SEM_SELECTED)) return 0;
    if (snprintf(path, sizeof(path), "%s-linux-files.ppm", prefix) >= (int)sizeof(path) ||
        !capture(window, scene, renderer, path) || !vk_profile_compare(window,scene,renderer,prefix,"files-100") ||
        nx_focus_set(cloud_context(scene), 1001) != NX_OK) return 0;
    nx_event event = {0}; event.type = NX_EVENT_KEY_DOWN; event.key = NX_KEY_SPACE;
    if (nx_context_queue_event(cloud_context(scene), &event) != NX_OK) return 0;
    event.type = NX_EVENT_KEY_UP;
    if (nx_context_queue_event(cloud_context(scene), &event) != NX_OK ||
        !frame(window, scene, renderer, 0) || !frame(window, scene, renderer, 0)) return 0;
    return nx_semantics_find(cloud_context(scene), 1001, &index) == NX_OK &&
        nx_semantics_get(cloud_context(scene), index, &view) == NX_OK && (view.state & NX_SEM_SELECTED);
}
#endif

int main(int argc, char **argv) {
    int testing = argc == 4 && (!strcmp(argv[1], "--smoke") || !strcmp(argv[1], "--profile") || !strcmp(argv[1], "--profile-ring"));
    if (argc != 2 && !testing) {
        fputs("usage: nexaui_nexacloud_native FONT.ttf | --smoke FONT.ttf OUTPUT_PREFIX | --profile FONT.ttf OUTPUT_PREFIX | --profile-ring FONT.ttf OUTPUT_PREFIX\n", stderr); return 2;
    }
    cloud_scene *scene = cloud_create(argv[testing ? 2 : 1]);
    if (!scene) { fputs("Cannot open the TrueType font.\n", stderr); return 1; }
    nx_window *window = NULL; nx_renderer_driver renderer = {0};
    nx_window_desc desc = {"NexaCloud — NexaUI", 1440, 1040, 0, {0}};
    nx_result created = nx_window_create(&desc, &window);
    const char *backend = getenv("NEXAUI_PLATFORM");
    int required = getenv("NEXAUI_REQUIRE_VULKAN") &&
        (!backend || strcmp(backend, "wayland") || getenv("NEXAUI_REQUIRE_VULKAN_WAYLAND"));
    if (testing && created == NX_ERROR_PLATFORM && !required) { cloud_destroy(scene); return 77; }
    int okay = created == NX_OK && nx_vulkan_driver_open(window, &renderer) == NX_OK;
    if (okay) cloud_set_window(scene, window);
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    cloud_vk_profile_ring = testing && !strcmp(argv[1],"--profile-ring");
    cloud_vk_profile_enabled = cloud_vk_profile_ring || (testing && !strcmp(argv[1],"--profile"));
    if (okay && testing) okay = smoke(window, scene, &renderer, argv[3]);
    if (okay && testing) {
        if (!close_renderer(window, &renderer)) return 1; /* Retain scene/window until process exits. */
        cloud_set_window(scene, NULL); nx_window_destroy(window); window = NULL;
        desc.width = 820; desc.height = 700;
        okay = nx_window_create(&desc, &window) == NX_OK && nx_vulkan_driver_open(window, &renderer) == NX_OK;
        if (okay) {
            cloud_set_window(scene, window);
            char path[1024];
            okay = frame(window, scene, &renderer, 0) && frame(window, scene, &renderer, 0) &&
                snprintf(path, sizeof(path), "%s-linux-compact.ppm", argv[3]) < (int)sizeof(path) &&
                capture(window, scene, &renderer, path) && native_reflow(window, scene, &renderer, argv[3], "linux");
            if (okay) okay = nx_focus_set(cloud_context(scene),11)==NX_OK && reflow_key(scene,NX_KEY_ENTER) &&
                reflow_frames(window,scene,&renderer) && vk_profile_compare(window,scene,&renderer,argv[3],"files-200");
        }
    }
#else
    if (testing) okay = 0;
#endif
    double previous = now();
    while (okay && !testing) {
        nx_window_info info; double current = now();
        if (current < 0 || previous < 0 || nx_window_get_info(window, &info) != NX_OK) { okay = 0; break; }
        if (info.should_close) break;
        double dt = current - previous; previous = current;
        okay = frame(window, scene, &renderer, dt > .1 ? .1 : dt);
    }
    if (!close_renderer(window, &renderer)) return 1;
    cloud_set_window(scene, NULL); cloud_destroy(scene); nx_window_destroy(window);
    return okay ? 0 : 1;
}
