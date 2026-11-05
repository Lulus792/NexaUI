#ifndef NEXAUI_AETHERI_TERMINAL_IME_OVERLAY_H
#define NEXAUI_AETHERI_TERMINAL_IME_OVERLAY_H
#include "ime_adapter.h"
#include "view.h"

typedef struct terminal_ime_overlay {
    const terminal_ime_adapter *ime; /* borrowed through frame */
    terminal_ime_guard current;
    /* Optional stable nonzero identity for a separate uncommitted text object.
       Must differ from the terminal text identity; change when the session is
       replaced. Zero omits it;
       the owner must omit it for sensitive/password input. */
    uint64_t semantic_identity;
    nx_utf8_span semantic_name; /* optional borrowed localized label */
} terminal_ime_overlay;
/* Candidate area in logical pixels, including a horizontally scrolled draft.
   The host must re-query after composition, caret, size, focus or scale changes. */
nx_result terminal_ime_overlay_caret(const terminal_ime_overlay *overlay,
    const aetheri_terminal_view_desc *desc,nx_rect *caret);
/* Translate grid-local geometry into logical window coordinates and publish it
   on the owner's native window. No device-scale multiplication. Call only for
   the focused current terminal after its latest layout; the host owns focus,
   window and surface lifetime and must republish when the geometry changes. */
nx_result terminal_ime_overlay_publish_caret(const terminal_ime_overlay *overlay,
    const aetheri_terminal_view_desc *desc,nx_window *window,nx_point grid_origin);
/* Callback for aetheri_terminal_view_frame_with_overlay. Transient preedit is
   painted over the grid and never inserted into the read-only text snapshot. */
nx_result terminal_ime_overlay_draw(void *overlay,nx_context *context,
    const aetheri_terminal_view_desc *desc);
#endif
