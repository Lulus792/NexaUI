#ifndef NEXAUI_X11_BACKEND_H
#define NEXAUI_X11_BACKEND_H

#include "nexaui/nexaui.h"
#include "../accessibility.h"
#include <X11/Xlib.h>

typedef struct nx_x11_window nx_x11_window;

nx_result nx_x11_window_create(const nx_window_desc *desc,
                               nx_x11_window **out_window);
nx_result nx_x11_window_poll(nx_x11_window *window, uint32_t timeout_ms);
nx_result nx_x11_window_poll_with_fd(nx_x11_window *window, uint32_t timeout_ms, int fd, int writable);
void nx_x11_window_ax_geometry(nx_x11_window *window, nx_linux_ax_geometry *out);
int nx_x11_window_ax_focus(nx_x11_window *window);
nx_result nx_x11_window_get_info(nx_x11_window *window, nx_window_info *out_info);
nx_result nx_x11_window_set_cursor(nx_x11_window *window, nx_cursor_shape shape);
nx_result nx_x11_window_get_reduced_motion(nx_x11_window *window,
                                            int *out_enabled);
nx_result nx_x11_window_set_ime_rect(nx_x11_window *window, nx_rect caret);
nx_result nx_x11_window_set_ime_enabled(nx_x11_window *window, int enabled);
nx_result nx_x11_window_get_ime_preedit(nx_x11_window *window,
    char *bytes, size_t capacity, size_t *out_length,
    int32_t *out_cursor_begin, int32_t *out_cursor_end);
nx_result nx_x11_window_next_event(nx_x11_window *window, nx_event *out_event,
                                    int *available);
nx_result nx_x11_window_route_events(nx_x11_window *window, nx_context *context,
                                      size_t *out_count);
nx_result nx_x11_window_next_file_drop(nx_x11_window *window, char *path_utf8,
                                        size_t capacity, size_t *out_length,
                                        nx_file_drop_info *out_info,
                                        int *available);
nx_result nx_x11_window_clipboard_set_text(nx_x11_window *window,
                                            const char *bytes, size_t length);
nx_result nx_x11_window_clipboard_get_text(nx_x11_window *window,
                                            char *bytes, size_t capacity,
                                            size_t *out_length);
void nx_x11_window_destroy(nx_x11_window *window);
Display *nx_x11_window_x11_display(nx_x11_window *window);
Window nx_x11_window_x11_id(nx_x11_window *window);
#ifdef NEXAUI_X11_TEST_HOOKS
nx_result nx_x11_window_test_ime_edit(nx_x11_window *window,
    const XIMText *text, int first, int removed, int caret);
nx_result nx_x11_window_test_ime_clear(nx_x11_window *window);
#endif

#endif
