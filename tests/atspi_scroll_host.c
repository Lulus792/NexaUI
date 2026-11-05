#include <nexaui/nexaui.h>
#include <nexaui/software.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    nx_window *window = NULL;
    nx_software_view *renderer = NULL;
    nx_context *ctx = NULL;
    float x = 0, y = 0;
    int ok = 0, done = 0;
    const char *private_test = getenv("NEXAUI_TEST_PRIVATE_WAYLAND");
    const char *display = getenv("WAYLAND_DISPLAY");
    if (!private_test || strcmp(private_test, "1") || !display || strcmp(display, "nexaui-test"))
        return 77;
    nx_window_desc window_desc = {"NexaUI scroll test", 240, 180, 0, {0}};
    if (nx_context_create(NULL, &ctx) != NX_OK ||
        nx_window_create(&window_desc, &window) != NX_OK ||
        nx_software_view_open(window, &renderer) != NX_OK) goto cleanup;
    ok = 1;
    while (ok && !done) {
        nx_window_info wi;
        if (nx_window_poll(window, 50) != NX_OK || nx_window_get_info(window, &wi) != NX_OK || wi.should_close) {
            ok = 0; break;
        }
        nx_event event; int available;
        do {
            if (nx_window_next_event(window, &event, &available) != NX_OK ||
                (available && nx_context_queue_event(ctx, &event) != NX_OK)) { ok = 0; break; }
        } while (available);
        size_t routed = 0;
        nx_scroll_area_desc scroll = {0};
        scroll.id = 1; scroll.viewport = (nx_rect){10, 10, 100, 100};
        scroll.name = (nx_utf8_span){"Two-axis scroll", 15};
        scroll.content_size = (nx_size){500, 500}; scroll.axes = NX_SCROLL_X | NX_SCROLL_Y;
        scroll.thumb_x_id = 2; scroll.thumb_y_id = 3; scroll.wheel_step = 40;
        nx_scroll_info info;
        nx_control_desc close = {0}; close.id = 4; close.bounds = (nx_rect){10, 130, 80, 32};
        close.name = (nx_utf8_span){"Close", 5};
        nx_interaction_state state;
        if (!ok || nx_window_route_events(window, ctx, &routed) != NX_OK ||
            nx_begin_frame(ctx, 240, 180, 0) != NX_OK ||
            nx_scroll_area_begin(ctx, &scroll, &x, &y, &info) != NX_OK ||
            nx_scroll_area_end(ctx) != NX_OK || nx_button(ctx, &close, &state) != NX_OK ||
            nx_end_frame(ctx) != NX_OK) { ok = 0; break; }
        if (state.clicked) { done = 1; break; }
        nx_result ax = nx_window_publish_semantics(window, ctx);
        const nx_command *commands = NULL; size_t count = 0; nx_software_view_outcome outcome;
        if ((ax != NX_OK && ax != NX_ERROR_UNSUPPORTED) ||
            nx_frame_commands(ctx, &commands, &count) != NX_OK ||
            nx_software_view_render(renderer, commands, count, (nx_color){0, 0, 0, 1}, &outcome) != NX_OK) ok = 0;
    }
cleanup:
    if (window && nx_window_publish_semantics(window, NULL) != NX_OK) ok = 0;
    if (renderer && nx_software_view_close(&renderer) != NX_OK) ok = 0;
    if (ctx) nx_context_destroy(ctx);
    if (window) nx_window_destroy(window);
    if (ok && done) { printf("SCROLL_CLOSED %.0f %.0f\n", x, y); return 0; }
    fputs("SCROLL_HOST_FAILED\n", stderr); return 1;
}
