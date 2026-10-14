#ifndef NEXAUI_WAYLAND_WINDOW_INTERNAL_H
#define NEXAUI_WAYLAND_WINDOW_INTERNAL_H

#include "nexaui/nexaui.h"
#include <wayland-client.h>

/* Private native handles for platform integration tests and renderers. */
struct wl_display *nx_window_wayland_display(nx_window *window);
struct wl_surface *nx_window_wayland_surface(nx_window *window);
/* Bit 0: buffer, bit 1: pointer, bit 2: keymap, bit 3: data device. */
unsigned nx_window_wayland_ready(nx_window *window);
#ifdef NEXAUI_WAYLAND_TEST_HOOKS
nx_result nx_window_wayland_test_uri_drop(nx_window *window,
                                          const char *text, size_t length,
                                          float x, float y);
nx_result nx_window_wayland_test_clipboard_roundtrip(
    nx_window *window, const char *input, size_t length,
    char *output, size_t capacity, size_t *out_length);
nx_result nx_window_wayland_test_pointer_enter(nx_window *window,
                                                int x, int y);
nx_result nx_window_wayland_test_focus(nx_window *window, int focused);
nx_result nx_window_wayland_test_key(nx_window *window,
                                     char letter, int down);
nx_result nx_window_wayland_test_repeat_info(nx_window *window,
                                              int rate, int delay_ms);
nx_result nx_window_wayland_test_output(nx_window *window,
                                        int scale, int entered);
nx_result nx_window_wayland_test_extents(nx_window *window,
                                         int *background_width,
                                         int *background_height,
                                         int *cursor_width,
                                         int *cursor_height);
nx_result nx_window_wayland_test_ime_commit(nx_window *window,
                                            const char *text, int stale);
nx_result nx_window_wayland_test_ime_preedit(nx_window *window,
    const char *text, int32_t cursor, int stale);
nx_result nx_window_wayland_test_ime_state(nx_window *window,
                                           int *requested, int *activated);
#endif

#endif
