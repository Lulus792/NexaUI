#ifndef NEXAUI_AETHERI_TERMINAL_IME_OVERLAY_H
#define NEXAUI_AETHERI_TERMINAL_IME_OVERLAY_H
#include "ime_adapter.h"
#include "view.h"

typedef struct terminal_ime_overlay {
    const terminal_ime_adapter *ime; /* borrowed through frame */
    terminal_ime_guard current;
} terminal_ime_overlay;
/* Candidate area in logical pixels, including a horizontally scrolled draft.
   The host must re-query after composition, caret, size, focus or scale changes. */
nx_result terminal_ime_overlay_caret(const terminal_ime_overlay *overlay,
    const aetheri_terminal_view_desc *desc,nx_rect *caret);
/* Callback for aetheri_terminal_view_frame_with_overlay. Transient preedit is
   painted over the grid and never inserted into the read-only text snapshot. */
nx_result terminal_ime_overlay_draw(void *overlay,nx_context *context,
    const aetheri_terminal_view_desc *desc);
#endif
