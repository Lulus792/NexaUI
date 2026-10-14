#include "nexaui/nexaui.h"

#include <stdio.h>

int main(void) {
    nx_window *window = NULL;
    nx_context *ui = NULL;
    nx_window_desc desc = {0};
    desc.title_utf8 = "NexaUI native window";
    desc.width = 800;
    desc.height = 500;
    if (nx_window_create(&desc, &window) != NX_OK) {
        fputs("Window creation failed\n", stderr);
        return 1;
    }
    if (nx_context_create(NULL, &ui) != NX_OK) {
        nx_window_destroy(window);
        return 1;
    }
    nx_window_info info;
    for (;;) {
        if (nx_window_poll(window, 100) != NX_OK) {
            fputs("Window event processing failed\n", stderr);
            nx_context_destroy(ui);
            nx_window_destroy(window);
            return 1;
        }
        nx_event event;
        int available;
        do {
            if (nx_window_next_event(window, &event, &available) != NX_OK) {
                nx_context_destroy(ui);
                nx_window_destroy(window);
                return 1;
            }
            if (available) {
                if (nx_context_queue_event(ui, &event) != NX_OK) {
                    nx_context_destroy(ui);
                    nx_window_destroy(window);
                    return 1;
                }
                if (event.type == NX_EVENT_KEY_DOWN)
                    printf("key down: %d\n", (int)event.key);
                if (event.type == NX_EVENT_TEXT)
                    printf("text scalar: U+%04X\n", event.scalar);
                if (event.type == NX_EVENT_POINTER_DOWN)
                    printf("pointer down: %.1f, %.1f\n", event.x, event.y);
            }
        } while (available);
        if (nx_window_get_info(window, &info) != NX_OK) {
            nx_context_destroy(ui);
            nx_window_destroy(window);
            return 1;
        }
        if (info.width > 0 && info.height > 0) {
            if (nx_begin_frame(ui, info.width, info.height, 0.1) != NX_OK ||
                nx_end_frame(ui) != NX_OK) {
                nx_context_destroy(ui);
                nx_window_destroy(window);
                return 1;
            }
        }
        if (info.should_close) break;
    }
    nx_context_destroy(ui);
    nx_window_destroy(window);
    return 0;
}
