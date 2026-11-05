#ifndef NEXAUI_AETHERI_TERMINAL_IME_ADAPTER_H
#define NEXAUI_AETHERI_TERMINAL_IME_ADAPTER_H
#include <nexaui/nexaui.h>
#include "composition.h"

/* Application supplies a new nonzero epoch for every focus/session/foreground
   change, including loss and regain. This example never owns a PTY. */
typedef struct terminal_ime_guard {
    uint64_t identity,epoch;
    int focused;
} terminal_ime_guard;
typedef struct terminal_ime_adapter {
    terminal_ime_guard guard;
    struct ae_terminal_composition composition;
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
#endif
