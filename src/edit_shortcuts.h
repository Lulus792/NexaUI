#ifndef NEXAUI_EDIT_SHORTCUTS_H
#define NEXAUI_EDIT_SHORTCUTS_H

#include "nexaui/nexaui.h"

/* Keep native modifier identity in public events. Ctrl remains a compatible
   explicit editing binding; macOS additionally accepts its primary Command.
   Alt/AltGr and mixed Control+Command are not primary shortcuts. */
static inline int nx_edit_primary(uint32_t modifiers) {
    uint32_t chord = modifiers & (NX_MOD_CONTROL | NX_MOD_ALT | NX_MOD_SUPER);
    if (chord == NX_MOD_CONTROL) return 1;
#if defined(__APPLE__)
    if (chord == NX_MOD_SUPER) return 1;
#endif
    return 0;
}

#endif
