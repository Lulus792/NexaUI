#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "nexaui/nexaui.h"
#include "window_internal.h"
#include "renderer.h"

#include <stdio.h>

static const uint8_t color_icon[4u * 4u * 4u] = {
    255, 80, 70, 255, 255, 80, 70, 255, 40, 190, 240, 255, 40, 190, 240, 255,
    255, 80, 70, 255, 255, 255, 255, 128, 255, 255, 255, 128, 40, 190, 240, 255,
    40, 190, 240, 255, 255, 255, 255, 128, 255, 255, 255, 128, 255, 80, 70, 255,
    40, 190, 240, 255, 40, 190, 240, 255, 255, 80, 70, 255, 255, 80, 70, 255
};

static const uint8_t glyph_n[5u * 7u] = {
    255, 0, 0, 0, 255,
    255, 180, 0, 0, 255,
    255, 0, 180, 0, 255,
    255, 0, 255, 0, 255,
    255, 0, 0, 180, 255,
    255, 0, 0, 180, 255,
    255, 0, 0, 0, 255
};

int main(void) {
    nx_window_desc desc = {0};
    desc.title_utf8 = "NexaUI D3D12 clipped shapes";
    desc.width = 720;
    desc.height = 440;
    nx_window *window = NULL;
    if (nx_window_create(&desc, &window) != NX_OK) {
        fputs("Could not create Win32 window\n", stderr);
        return 1;
    }
    nx_renderer_driver renderer = {0};
    if (nx_d3d12_driver_open(window, &renderer) != NX_OK) {
        fputs("Could not create D3D12 renderer\n", stderr);
        nx_window_destroy(window);
        return 1;
    }
    int status = 0;
    ULONGLONG started = GetTickCount64();
    while (GetTickCount64() - started < 3000) {
        if (nx_window_poll(window, 16) != NX_OK) { status = 1; break; }
        nx_window_info info;
        if (nx_window_get_info(window, &info) != NX_OK) {
            status = 1;
            break;
        }
        if (info.should_close) break;
        RECT client;
        if (!GetClientRect(nx_window_native_hwnd(window), &client)) {
            status = 1;
            break;
        }
        nx_render_target target = {(uint32_t)(client.right - client.left),
                                   (uint32_t)(client.bottom - client.top),
                                   info.scale};
        float phase = (float)(GetTickCount64() - started) / 3000.0f;
        nx_color clear = {0.08f, 0.12f, 0.24f, 1};
        nx_command commands[12] = {0};
        commands[0].type = NX_COMMAND_PUSH_CLIP;
        commands[0].rect = (nx_rect){45, 45, 600, 315};
        commands[1].type = NX_COMMAND_FILL_RECT;
        commands[1].rect = (nx_rect){20 + 350 * phase, 55, 180, 110};
        commands[1].color = (nx_color){0.20f, 0.72f, 0.94f, 1};
        commands[2].type = NX_COMMAND_PUSH_CLIP;
        commands[2].rect = (nx_rect){310, 90, 260, 175};
        commands[3].type = NX_COMMAND_FILL_TRIANGLE;
        commands[3].points[0] = (nx_point){390, 75};
        commands[3].points[1] = (nx_point){610, 170};
        commands[3].points[2] = (nx_point){360, 300};
        commands[3].color = (nx_color){0.98f, 0.58f, 0.20f, 0.6f};
        commands[4].type = NX_COMMAND_POP_CLIP;
        commands[5].type = NX_COMMAND_FILL_RECT;
        commands[5].rect = (nx_rect){95, 220, 170, 80};
        commands[5].color = (nx_color){0.42f, 0.83f, 0.39f, 1};
        commands[6].type = NX_COMMAND_POP_CLIP;
        commands[7].type = NX_COMMAND_BITMAP;
        commands[7].rect = (nx_rect){510, 285, 95, 75};
        commands[7].color = (nx_color){1, 1, 1, 0.85f};
        commands[7].bitmap = (nx_bitmap_view){color_icon, 4, 4, 16,
                                                NX_BITMAP_RGBA8};
        commands[8].type = NX_COMMAND_BITMAP;
        commands[8].rect = (nx_rect){400, 285, 55, 75};
        commands[8].color = (nx_color){0.95f, 0.95f, 1, 1};
        commands[8].bitmap = (nx_bitmap_view){glyph_n, 5, 7, 5,
                                                NX_BITMAP_A8};
        commands[9].type = NX_COMMAND_STROKE_RECT;
        commands[9].rect = (nx_rect){285, 195, 94, 92};
        commands[9].thickness = 4;
        commands[9].color = (nx_color){0.98f, 0.82f, 0.32f, 0.85f};
        commands[10].type = NX_COMMAND_LINE;
        commands[10].points[0] = (nx_point){280, 315};
        commands[10].points[1] = (nx_point){395, 220};
        commands[10].thickness = 7;
        commands[10].color = (nx_color){0.91f, 0.34f, 0.64f, 0.8f};
        commands[11].type = NX_COMMAND_FILL_ROUND_RECT;
        commands[11].rect = (nx_rect){90, 340, 175, 52};
        commands[11].radius = 18;
        commands[11].color = (nx_color){0.25f, 0.68f, 0.94f, 0.75f};
        nx_render_outcome outcome;
        if (nx_renderer_run(&renderer, target, commands, 12, clear,
                             &outcome) != NX_OK) {
            status = 1;
            break;
        }
        if (outcome == NX_RENDER_DEVICE_LOST &&
            nx_renderer_recover(&renderer) != NX_OK) {
            status = 1;
            break;
        }
    }
    nx_renderer_close(&renderer);
    nx_window_destroy(window);
    if (status) fputs("Native clear failed\n", stderr);
    return status;
}
