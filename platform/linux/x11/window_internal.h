#ifndef NEXAUI_X11_WINDOW_INTERNAL_H
#define NEXAUI_X11_WINDOW_INTERNAL_H

#include "nexaui/nexaui.h"
#include <X11/Xlib.h>

/* Private native handles for platform integration tests and renderers. */
Display *nx_window_x11_display(nx_window *window);
Window nx_window_x11_id(nx_window *window);
#ifdef NEXAUI_X11_TEST_HOOKS
nx_result nx_window_x11_test_ime_edit(nx_window *window,
    const XIMText *text, int first, int removed, int caret);
nx_result nx_window_x11_test_ime_clear(nx_window *window);
#endif

#endif
