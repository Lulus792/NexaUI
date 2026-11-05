#ifndef NEXAUI_AETHERI_TERMINAL_IME_ADAPTER_H
#define NEXAUI_AETHERI_TERMINAL_IME_ADAPTER_H
#include <nexaui/nexaui.h>
#include "composition.h"
#include "paste.h"

/* Application supplies a new nonzero epoch for every focus/session/foreground
   change, including loss and regain. This example never owns a PTY. */
typedef struct terminal_ime_guard {
    uint64_t identity,epoch;
    int focused;
} terminal_ime_guard;
typedef struct terminal_ime_adapter {
    terminal_ime_guard guard;
    struct ae_terminal_composition composition;
    struct ae_terminal_paste commit;
    char pending[AE_TERM_PASTE_BYTES];
    size_t pending_length;
    int receiving;
} terminal_ime_adapter;

/* Bind only after native focus is established and publish the same identity
   and epoch via nx_window_set_ime_surrounding_text with empty context. */
nx_result terminal_ime_bind(terminal_ime_adapter *ime,terminal_ime_guard guard);
void terminal_ime_reset(terminal_ime_adapter *ime);
/* Call only for a current native preedit notification. Native byte offsets
   become Aetheri scalar offsets; failed conversion leaves the draft intact.
   If the owner guard changed, clear the draft and reject the stale event. */
nx_result terminal_ime_preedit(terminal_ime_adapter *ime,terminal_ime_guard current,
    const char *bytes,size_t length,int32_t begin,int32_t end);
/* Deliver Wayland IME text events in order. Complete commits are validated by
   the original Aetheri whole-snapshot policy before bytes are returned; no
   output is produced for partial, filtered, stale or rejected commits.
   The returned span is borrowed until the next commit event, interruption,
   bind or reset. Copy it into the app-owned PTY queue before then. */
nx_result terminal_ime_commit_event(terminal_ime_adapter *ime,terminal_ime_guard current,
    const nx_event *event,int *ready,nx_utf8_span *out);
/* Discard an incomplete commit on any interleaved event, queue failure,
   transport loss or owner transition. Never flush a partial commit. */
void terminal_ime_interrupt(terminal_ime_adapter *ime);
#endif
