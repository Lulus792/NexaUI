#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>

#include "nexaui/nexaui.h"
#include "window_internal.h"
#include "renderer.h"

#include <stdio.h>
#include <string.h>

typedef struct demo_window {
    nx_window *window;
    nx_context *ui;
    nx_renderer_driver renderer;
    nx_color clear;
    nx_color accent;
    unsigned presses;
    unsigned text_count;
    unsigned frames;
    int a_down, b_down;
} demo_window;

#define CHECK_STEP(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 0; \
} } while (0)

static void close_window(demo_window *slot) {
    nx_renderer_close(&slot->renderer);
    nx_context_destroy(slot->ui);
    nx_window_destroy(slot->window);
    slot->ui = NULL;
    slot->window = NULL;
}

static int open_window(demo_window *slot, const char *title,
                       nx_color clear, nx_color accent, int hidden) {
    nx_window_desc desc = {0};
    desc.title_utf8 = title;
    desc.width = 420;
    desc.height = 260;
    desc.flags = hidden ? NX_WINDOW_HIDDEN : 0;
    slot->clear = clear;
    slot->accent = accent;
    CHECK_STEP(nx_window_create(&desc, &slot->window) == NX_OK);
    CHECK_STEP(nx_context_create(NULL, &slot->ui) == NX_OK);
    CHECK_STEP(nx_d3d12_driver_open(slot->window, &slot->renderer) == NX_OK);
    return 1;
}

static int draw_window(demo_window *slot, nx_window_info info) {
    const nx_event *events = NULL;
    const nx_command *commands = NULL;
    size_t routed = 0, event_count = 0, command_count = 0;
    nx_input_snapshot input;
    nx_render_outcome outcome;
    RECT client;

    CHECK_STEP(nx_window_route_events(slot->window, slot->ui, &routed) == NX_OK);
    CHECK_STEP(nx_begin_frame(slot->ui, info.width, info.height, 1.0 / 60.0) == NX_OK);
    CHECK_STEP(nx_frame_events(slot->ui, &events, &event_count) == NX_OK);
    CHECK_STEP(nx_frame_input(slot->ui, &input) == NX_OK);
    for (size_t i = 0; i < event_count; ++i) {
        if (events[i].type == NX_EVENT_TEXT) ++slot->text_count;
        if (events[i].type == NX_EVENT_POINTER_DOWN) ++slot->presses;
    }
    slot->a_down = nx_key_down(slot->ui, NX_KEY_A);
    slot->b_down = nx_key_down(slot->ui, NX_KEY_B);

    float width = info.width;
    float height = info.height;
    float fill = 25.0f + (float)(slot->presses % 8u) * 30.0f;
    if (width > 70 && height > 40) {
        CHECK_STEP(nx_fill_rect(slot->ui, (nx_rect){20, 20, width - 40, height - 40},
                                (nx_color){0.16f, 0.18f, 0.23f, 1}) == NX_OK);
        CHECK_STEP(nx_fill_rect(slot->ui, (nx_rect){35, 42, width - 70, 62},
                                slot->accent) == NX_OK);
        CHECK_STEP(nx_fill_rect(slot->ui, (nx_rect){35, 135, fill, 26},
                                (nx_color){0.97f, 0.78f, 0.28f, 1}) == NX_OK);
        CHECK_STEP(nx_fill_rect(slot->ui,
                                (nx_rect){35, 180, 18.0f + (float)(slot->text_count % 20u) * 15.0f, 18},
                                (nx_color){0.48f, 0.88f, 0.66f, 1}) == NX_OK);
        if (input.focused)
            CHECK_STEP(nx_stroke_rect(slot->ui,
                                      (nx_rect){22, 22, width - 44, height - 44},
                                      2, slot->accent) == NX_OK);
    }
    CHECK_STEP(nx_end_frame(slot->ui) == NX_OK);
    CHECK_STEP(nx_frame_commands(slot->ui, &commands, &command_count) == NX_OK);
    CHECK_STEP(GetClientRect(nx_window_native_hwnd(slot->window), &client));

    nx_render_target target = {(uint32_t)(client.right - client.left),
                               (uint32_t)(client.bottom - client.top), info.scale};
    CHECK_STEP(nx_renderer_run(&slot->renderer, target, commands, command_count,
                               slot->clear, &outcome) == NX_OK);
    if (outcome == NX_RENDER_DEVICE_LOST)
        CHECK_STEP(nx_renderer_recover(&slot->renderer) == NX_OK);
    ++slot->frames;
    (void)routed;
    return 1;
}

static int tick(demo_window slots[2], uint32_t timeout_ms) {
    nx_window *pump = slots[0].window ? slots[0].window : slots[1].window;
    if (!pump || nx_window_poll(pump, timeout_ms) != NX_OK) return 0;
    for (unsigned i = 0; i < 2; ++i) {
        if (!slots[i].window) continue;
        nx_window_info info;
        if (nx_window_get_info(slots[i].window, &info) != NX_OK) return 0;
        if (info.should_close) {
            close_window(&slots[i]);
        } else if (info.width > 0 && info.height > 0 &&
                   !draw_window(&slots[i], info)) {
            return 0;
        }
    }
    return 1;
}

static int smoke(demo_window slots[2]) {
    HWND first = nx_window_native_hwnd(slots[0].window);
    HWND second = nx_window_native_hwnd(slots[1].window);
    CHECK_STEP(first && second && first != second);
    CHECK_STEP(PostMessageW(first, WM_SETFOCUS, 0, 0));
    CHECK_STEP(PostMessageW(first, WM_KEYDOWN, 'A', 0));
    CHECK_STEP(PostMessageW(second, WM_SETFOCUS, 0, 0));
    CHECK_STEP(PostMessageW(second, WM_KEYDOWN, 'B', 0));
    CHECK_STEP(tick(slots, 0));
    CHECK_STEP(slots[0].a_down && !slots[0].b_down);
    CHECK_STEP(slots[1].b_down && !slots[1].a_down);
    CHECK_STEP(slots[0].frames && slots[1].frames);

    SendMessageW(first, WM_CLOSE, 0, 0);
    CHECK_STEP(tick(slots, 0));
    CHECK_STEP(!slots[0].window && slots[1].window);
    unsigned previous_text_count = slots[1].text_count;
    CHECK_STEP(PostMessageW(second, WM_CHAR, 'Z', 0));
    CHECK_STEP(tick(slots, 0));
    CHECK_STEP(slots[1].text_count == previous_text_count + 1 &&
               slots[1].frames >= 3);
    puts("two native windows rendered; input isolated; survivor rendered after close");
    return 1;
}

int main(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--smoke") != 0)) {
        fputs("usage: nexaui_multi_window_example [--smoke]\n", stderr);
        return 2;
    }
    const int hidden = argc == 2;
    demo_window slots[2] = {0};
    int ok = open_window(&slots[0], "NexaUI multi-window A",
                         (nx_color){0.07f, 0.12f, 0.22f, 1},
                         (nx_color){0.22f, 0.64f, 0.95f, 1}, hidden) &&
             open_window(&slots[1], "NexaUI multi-window B",
                         (nx_color){0.19f, 0.09f, 0.16f, 1},
                         (nx_color){0.94f, 0.36f, 0.57f, 1}, hidden);
    if (ok && hidden) {
        ok = smoke(slots);
    } else if (ok) {
        while (slots[0].window || slots[1].window) {
            if (!tick(slots, 16)) { ok = 0; break; }
        }
    }
    close_window(&slots[0]);
    close_window(&slots[1]);
    if (!ok) fputs("multi-window example failed\n", stderr);
    return ok ? 0 : 1;
}
