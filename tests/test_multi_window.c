#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>

#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

typedef struct fail_state { unsigned allocations; } fail_state;

static void *fail_allocate(void *user, size_t size) {
    fail_state *state = (fail_state *)user;
    if (++state->allocations > 2) return NULL;
    return malloc(size);
}

static void fail_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static int discard_events(nx_window *window) {
    nx_event event;
    int available;
    do {
        CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    } while (available);
    return 0;
}

int main(void) {
    nx_window_desc desc = {0};
    desc.width = 240;
    desc.height = 120;
    desc.flags = NX_WINDOW_HIDDEN;
    nx_window *first = NULL, *second = NULL;
    desc.title_utf8 = "NexaUI multi-window first";
    CHECK(nx_window_create(&desc, &first) == NX_OK);
    desc.title_utf8 = "NexaUI multi-window second";
    CHECK(nx_window_create(&desc, &second) == NX_OK);
    HWND first_hwnd = FindWindowW(L"NexaUIWindowClass", L"NexaUI multi-window first");
    HWND second_hwnd = FindWindowW(L"NexaUIWindowClass", L"NexaUI multi-window second");
    CHECK(first_hwnd && second_hwnd && first_hwnd != second_hwnd);
    BOOL system_animations = TRUE;
    int reduced = -1;
    CHECK(SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0,
                                &system_animations, 0));
    CHECK(nx_window_get_reduced_motion(first, &reduced) == NX_OK &&
          reduced == !system_animations);
    CHECK(nx_window_get_reduced_motion(second, &reduced) == NX_OK &&
          reduced == !system_animations);
    CHECK(nx_window_get_reduced_motion(first, NULL) == NX_ERROR_ARGUMENT);
    CHECK(discard_events(first) == 0 && discard_events(second) == 0);

    nx_context *first_ctx = NULL, *second_ctx = NULL;
    CHECK(nx_context_create(NULL, &first_ctx) == NX_OK);
    CHECK(nx_context_create(NULL, &second_ctx) == NX_OK);
    size_t routed = 99;
    CHECK(nx_window_route_events(NULL, first_ctx, &routed) == NX_ERROR_ARGUMENT &&
          routed == 0);
    CHECK(nx_window_route_events(first, NULL, &routed) == NX_ERROR_ARGUMENT &&
          routed == 0);
    CHECK(nx_window_route_events(first, first_ctx, NULL) == NX_ERROR_ARGUMENT);

    CHECK(PostMessageW(first_hwnd, WM_SETFOCUS, 0, 0));
    CHECK(PostMessageW(first_hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(12, 24)));
    CHECK(PostMessageW(first_hwnd, WM_KEYDOWN, 'A', 0));
    CHECK(PostMessageW(second_hwnd, WM_SETFOCUS, 0, 0));
    CHECK(PostMessageW(second_hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(45, 60)));
    CHECK(PostMessageW(second_hwnd, WM_KEYDOWN, 'B', 0));
    CHECK(nx_window_poll(first, 0) == NX_OK);
    CHECK(nx_window_route_events(second, second_ctx, &routed) == NX_OK && routed >= 3);
    CHECK(nx_window_route_events(first, first_ctx, &routed) == NX_OK && routed >= 3);
    CHECK(nx_begin_frame(first_ctx, 240, 120, 0) == NX_OK);
    CHECK(nx_begin_frame(second_ctx, 240, 120, 0) == NX_OK);
    nx_input_snapshot first_input, second_input;
    CHECK(nx_frame_input(first_ctx, &first_input) == NX_OK);
    CHECK(nx_frame_input(second_ctx, &second_input) == NX_OK);
    CHECK(first_input.focused && second_input.focused);
    CHECK(first_input.pointer_x < second_input.pointer_x);
    CHECK(nx_key_down(first_ctx, NX_KEY_A) && !nx_key_down(first_ctx, NX_KEY_B));
    CHECK(nx_key_down(second_ctx, NX_KEY_B) && !nx_key_down(second_ctx, NX_KEY_A));
    CHECK(nx_end_frame(first_ctx) == NX_OK && nx_end_frame(second_ctx) == NX_OK);

    CHECK(PostMessageW(first_hwnd, WM_KILLFOCUS, 0, 0));
    CHECK(nx_window_poll(second, 0) == NX_OK);
    CHECK(nx_window_route_events(first, first_ctx, &routed) == NX_OK && routed == 1);
    CHECK(nx_window_route_events(second, second_ctx, &routed) == NX_OK && routed == 0);
    CHECK(nx_begin_frame(first_ctx, 240, 120, 0) == NX_OK);
    CHECK(nx_begin_frame(second_ctx, 240, 120, 0) == NX_OK);
    CHECK(nx_frame_input(first_ctx, &first_input) == NX_OK && !first_input.focused);
    CHECK(nx_frame_input(second_ctx, &second_input) == NX_OK && second_input.focused);
    CHECK(!nx_key_down(first_ctx, NX_KEY_A) && nx_key_released(first_ctx, NX_KEY_A));
    CHECK(nx_key_down(second_ctx, NX_KEY_B));
    CHECK(nx_end_frame(first_ctx) == NX_OK && nx_end_frame(second_ctx) == NX_OK);

    fail_state state = {0};
    nx_context_desc failing_desc = {0};
    failing_desc.allocator = (nx_allocator){&state, fail_allocate, fail_deallocate};
    nx_context *failing_ctx = NULL;
    CHECK(nx_context_create(&failing_desc, &failing_ctx) == NX_OK);
    CHECK(PostMessageW(first_hwnd, WM_CHAR, 'X', 0));
    CHECK(nx_window_poll(first, 0) == NX_OK);
    CHECK(nx_window_route_events(first, failing_ctx, &routed) == NX_ERROR_MEMORY &&
          routed == 0);
    CHECK(nx_window_route_events(first, first_ctx, &routed) == NX_OK && routed == 1);
    CHECK(nx_begin_frame(first_ctx, 240, 120, 0) == NX_OK);
    const nx_event *events = NULL;
    size_t count = 0;
    CHECK(nx_frame_events(first_ctx, &events, &count) == NX_OK && count == 1 &&
          events[0].type == NX_EVENT_TEXT && events[0].scalar == 'X');
    CHECK(nx_end_frame(first_ctx) == NX_OK);
    nx_context_destroy(failing_ctx);

    SendMessageW(first_hwnd, WM_CLOSE, 0, 0);
    nx_window_info info;
    CHECK(nx_window_get_info(first, &info) == NX_OK && info.should_close);
    CHECK(nx_window_get_info(second, &info) == NX_OK && !info.should_close);
    nx_window_destroy(first);
    CHECK(PostMessageW(second_hwnd, WM_CHAR, 'Y', 0));
    CHECK(nx_window_poll(second, 0) == NX_OK);
    CHECK(nx_window_route_events(second, second_ctx, &routed) == NX_OK && routed == 1);
    CHECK(nx_begin_frame(second_ctx, 240, 120, 0) == NX_OK);
    CHECK(nx_frame_events(second_ctx, &events, &count) == NX_OK && count == 1 &&
          events[0].type == NX_EVENT_TEXT && events[0].scalar == 'Y');
    CHECK(nx_end_frame(second_ctx) == NX_OK);
    nx_context_destroy(first_ctx);
    nx_context_destroy(second_ctx);
    nx_window_destroy(second);
    puts("multi-window routing tests passed");
    return 0;
}
