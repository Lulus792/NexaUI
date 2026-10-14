#ifndef NEXAUI_WAYLAND_BACKEND_H
#define NEXAUI_WAYLAND_BACKEND_H

#include "nexaui/nexaui.h"
#include <wayland-client.h>

typedef struct nx_wayland_window nx_wayland_window;
nx_result nx_wayland_window_set_external_renderer(nx_wayland_window *window,
                                                  int enabled);

nx_result nx_wayland_window_create(const nx_window_desc *desc,
                                   nx_wayland_window **out_window);
nx_result nx_wayland_window_poll(nx_wayland_window *window, uint32_t timeout_ms);
nx_result nx_wayland_window_get_info(nx_wayland_window *window,
                                     nx_window_info *out_info);
nx_result nx_wayland_window_set_cursor(nx_wayland_window *window,
                                       nx_cursor_shape shape);
nx_result nx_wayland_window_get_reduced_motion(nx_wayland_window *window,
                                                int *out_enabled);
nx_result nx_wayland_window_set_ime_rect(nx_wayland_window *window,
                                         nx_rect caret);
nx_result nx_wayland_window_set_ime_enabled(nx_wayland_window *window,
                                            int enabled);
nx_result nx_wayland_window_set_ime_purpose(nx_wayland_window *window,
                                            nx_ime_purpose purpose);
nx_result nx_wayland_window_get_ime_preedit(nx_wayland_window *window,
    char *bytes, size_t capacity, size_t *out_length,
    int32_t *out_cursor_begin, int32_t *out_cursor_end);
nx_result nx_wayland_window_set_ime_surrounding_text(nx_wayland_window *window,
    uint64_t target, uint64_t revision, const char *bytes, size_t length,
    size_t cursor, size_t anchor);
nx_result nx_wayland_window_next_event(nx_wayland_window *window,
                                       nx_event *out_event, int *available);
nx_result nx_wayland_window_route_events(nx_wayland_window *window,
                                         nx_context *context,
                                         size_t *out_count);
nx_result nx_wayland_window_next_file_drop(nx_wayland_window *window,
                                           char *path_utf8, size_t capacity,
                                           size_t *out_length,
                                           nx_file_drop_info *out_info,
                                           int *available);
nx_result nx_wayland_window_clipboard_set_text(nx_wayland_window *window,
                                               const char *bytes,
                                               size_t length);
nx_result nx_wayland_window_clipboard_get_text(nx_wayland_window *window,
                                               char *bytes, size_t capacity,
                                               size_t *out_length);
void nx_wayland_window_destroy(nx_wayland_window *window);
struct wl_display *nx_wayland_window_display(nx_wayland_window *window);
struct wl_surface *nx_wayland_window_surface(nx_wayland_window *window);
unsigned nx_wayland_window_ready(nx_wayland_window *window);
#ifdef NEXAUI_WAYLAND_TEST_HOOKS
nx_result nx_wayland_window_test_uri_drop(nx_wayland_window *window,
                                          const char *text, size_t length,
                                          float x, float y);
nx_result nx_wayland_window_test_clipboard_roundtrip(
    nx_wayland_window *window, const char *input, size_t length,
    char *output, size_t capacity, size_t *out_length);
nx_result nx_wayland_window_test_pointer_enter(nx_wayland_window *window,
                                                int x, int y);
nx_result nx_wayland_window_test_focus(nx_wayland_window *window, int focused);
nx_result nx_wayland_window_test_key(nx_wayland_window *window,
                                     char letter, int down);
nx_result nx_wayland_window_test_repeat_info(nx_wayland_window *window,
                                              int rate, int delay_ms);
nx_result nx_wayland_window_test_output(nx_wayland_window *window,
                                        int scale, int entered);
nx_result nx_wayland_window_test_extents(nx_wayland_window *window,
                                         int *background_width,
                                         int *background_height,
                                         int *cursor_width,
                                         int *cursor_height);
nx_result nx_wayland_window_test_ime_commit(nx_wayland_window *window,
                                            const char *text, int stale);
nx_result nx_wayland_window_test_ime_preedit(nx_wayland_window *window,
    const char *text, int32_t cursor, int stale);
nx_result nx_wayland_window_test_ime_state(nx_wayland_window *window,
                                           int *requested, int *activated);
#endif

#endif
