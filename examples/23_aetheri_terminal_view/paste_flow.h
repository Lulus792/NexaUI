#ifndef NEXAUI_AETHERI_TERMINAL_PASTE_FLOW_H
#define NEXAUI_AETHERI_TERMINAL_PASTE_FLOW_H
#include <nexaui/wayland_clipboard.h>
#include "paste.h"

/* The app advances revision and focus_epoch whenever the target or focus
   changes, including losing and regaining focus. identity changes on replacing
   the terminal/session. The app supplies its current guard at every step. */
typedef struct terminal_paste_guard {
    uint64_t identity,revision,focus_epoch;
    int focused;
} terminal_paste_guard;
typedef struct terminal_paste_transport {
    void *user;
    nx_result (*begin)(void *,size_t,uint32_t,int,uint64_t *);
    nx_result (*take)(void *,uint64_t,char **,size_t *,int *);
    nx_result (*cancel)(void *,uint64_t);
    void (*release)(void *,char *,size_t);
} terminal_paste_transport;
typedef struct terminal_paste_flow {
    terminal_paste_transport transport;
    terminal_paste_guard guard;
    struct ae_terminal_paste paste;
    uint64_t token;
    int state; /* 0 idle, 1 receiving, 2 staged, 3 delivered */
} terminal_paste_flow;
/* A real window transport borrows window through reset; all calls are owner-thread. */
terminal_paste_transport terminal_paste_wayland_transport(nx_window *window);
/* Initialize fresh storage only; reset before reinitializing a live flow. */
nx_result terminal_paste_init(terminal_paste_flow *flow,terminal_paste_transport transport);
nx_result terminal_paste_begin(terminal_paste_flow *flow,terminal_paste_guard guard,uint32_t timeout_ms);
/* Poll after window wakeup. ready=1 on a consumed success or failure. Validation
   uses the original full-snapshot model before anything can be delivered. */
nx_result terminal_paste_poll(terminal_paste_flow *flow,terminal_paste_guard current,
    int *ready,enum ae_term_paste_result *validation);
/* Owner presents the original model's entire navigable preview when review is
   required. A fresh non-repeat confirmation is required for reviewed content;
   the owner may deliver a non-review paste without a second confirmation.
   Borrowed bytes expire at reset; enqueue/copy them before the next call. */
nx_result terminal_paste_confirm(terminal_paste_flow *flow,terminal_paste_guard current,
    int fresh,int repeat,nx_utf8_span *bytes);
/* Cancel the native token if any, and clear the staged text. Idempotent. */
void terminal_paste_reset(terminal_paste_flow *flow);
#endif
