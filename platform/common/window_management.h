#ifndef NEXAUI_WINDOW_MANAGEMENT_H
#define NEXAUI_WINDOW_MANAGEMENT_H

#include "nexaui/nexaui.h"
#include "event_queue.h"

#define NX_WINDOW_FLAGS_ALL (NX_WINDOW_HIDDEN | NX_WINDOW_CLIENT_CHROME | NX_WINDOW_STATE_EVENTS)
#define NX_WINDOW_STATES_ALL (NX_WINDOW_STATE_MINIMIZED | NX_WINDOW_STATE_MAXIMIZED | NX_WINDOW_STATE_FULLSCREEN | NX_WINDOW_STATE_ACTIVE)
#define NX_WINDOW_CAPS_NATIVE (NX_WINDOW_CAP_CLIENT_CHROME | NX_WINDOW_CAP_MINIMIZE | NX_WINDOW_CAP_MAXIMIZE | NX_WINDOW_CAP_RESTORE | NX_WINDOW_CAP_MOVE | NX_WINDOW_CAP_RESIZE | NX_WINDOW_CAP_RESTORE_MINIMIZED)

static inline int nx_window_edge_valid(nx_window_resize_edge edge) {
    return edge >= NX_WINDOW_RESIZE_TOP_LEFT && edge <= NX_WINDOW_RESIZE_LEFT;
}

static inline void nx_window_state_event(nx_platform_event_queue *queue,
    uint32_t flags, uint32_t *previous, uint32_t current) {
    if (*previous == current) return;
    *previous = current;
    if (flags & NX_WINDOW_STATE_EVENTS) {
        nx_event event = {0};
        event.type = NX_EVENT_WINDOW_STATE;
        event.window_state = current;
        nx_platform_event_queue_push(queue, event);
    }
}

#endif
