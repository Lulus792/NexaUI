#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include "renderer.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now(void) {
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) return -1;
    return (double)time.tv_sec + (double)time.tv_nsec / 1000000000.0;
}

int main(int argc, char **argv) {
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    if (argc > 1 && !smoke) {
        fputs("Usage: nexaui_native_clear [--smoke]\n", stderr);
        return 1;
    }
    nx_window *window = NULL;
    nx_window_desc desc = {"NexaUI Vulkan shapes", 640, 400, 0, {0}};
    nx_result result = nx_window_create(&desc, &window);
    if (result != NX_OK) {
        const char *platform = getenv("NEXAUI_PLATFORM");
        int required = getenv("NEXAUI_REQUIRE_VULKAN") &&
            (!platform || !strcmp(platform, "x11") || getenv("NEXAUI_REQUIRE_VULKAN_WAYLAND"));
        if (smoke && result == NX_ERROR_PLATFORM && !required) return 77;
        fprintf(stderr, "Could not create a native Linux window: %d\n", (int)result);
        return 1;
    }
    nx_renderer_driver renderer = {0};
    result = nx_vulkan_driver_open(window, &renderer);
    if (result != NX_OK) {
        fprintf(stderr, "Could not create the Vulkan renderer: %d\n", (int)result);
        nx_window_destroy(window);
        return 1;
    }
    double started = now(), duration = smoke ? 0.25 : 3.0;
    int status = started < 0 ? 1 : 0;
    unsigned frames = 0;
    while (!status) {
        double current = now();
        if (current < 0) { status = 1; break; }
        if (current - started >= duration) break;
        if (nx_window_poll(window, 16) != NX_OK) { status = 1; break; }
        nx_window_info info;
        if (nx_window_get_info(window, &info) != NX_OK) { status = 1; break; }
        if (info.should_close) break;
        double width = ceil((double)info.width * info.scale);
        double height = ceil((double)info.height * info.scale);
        if (width > UINT32_MAX || height > UINT32_MAX) { status = 1; break; }
        nx_render_target target = {(uint32_t)width, (uint32_t)height, info.scale};
        float phase = (float)((current - started) / duration);
        nx_color clear = {0.08f + 0.14f * phase, 0.16f, 0.28f + 0.32f * phase, 1};
        nx_command commands[3] = {{0}};
        commands[0].type = NX_COMMAND_FILL_RECT;
        commands[0].rect = (nx_rect){24, 24, info.width * 0.6f, info.height * 0.45f};
        commands[0].color = (nx_color){0.16f, 0.65f, 0.88f, 1};
        commands[1].type = NX_COMMAND_FILL_RECT;
        commands[1].rect = (nx_rect){48 + phase * 80, 56, 110, 60};
        commands[1].color = (nx_color){0.95f, 0.65f, 0.18f, 1};
        commands[2].type = NX_COMMAND_FILL_TRIANGLE;
        commands[2].points[0] = (nx_point){info.width * 0.5f, info.height * 0.2f};
        commands[2].points[1] = (nx_point){info.width * 0.85f, info.height * 0.75f};
        commands[2].points[2] = (nx_point){info.width * 0.4f, info.height * 0.7f};
        commands[2].color = (nx_color){0.75f, 0.25f, 0.4f, 1};
        nx_render_outcome outcome;
        result = nx_renderer_run(&renderer, target, commands, 3, clear, &outcome);
        if (result != NX_OK) { status = 1; break; }
        if (outcome == NX_RENDER_DEVICE_LOST) {
            if (nx_renderer_recover(&renderer) != NX_OK) { status = 1; break; }
        } else if (outcome == NX_RENDER_READY) ++frames;
    }
    result = nx_renderer_close(&renderer);
    for (unsigned retry = 0; result == NX_ERROR_LIMIT && retry < 3u; ++retry) {
        (void)nx_window_poll(window, 16);
        result = nx_renderer_close(&renderer);
    }
    if (result != NX_OK) {
        fprintf(stderr, "Renderer retirement is incomplete: %d\n", (int)result);
        return 1; /* Keep the borrowed window alive until process teardown. */
    }
    nx_window_destroy(window);
    if (smoke && !frames) status = 1;
    if (!status) printf("Presented %u Vulkan frames\n", frames);
    return status;
}
