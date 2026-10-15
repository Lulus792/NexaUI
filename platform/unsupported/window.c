#include "nexaui/nexaui.h"

nx_result nx_window_create(const nx_window_desc *desc, nx_window **out_window) {
    (void)desc;
    if (!out_window) return NX_ERROR_ARGUMENT;
    *out_window = NULL;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_poll(nx_window *window, uint32_t timeout_ms) {
    (void)window;
    (void)timeout_ms;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_get_info(nx_window *window, nx_window_info *out_info) {
    (void)window;
    (void)out_info;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_set_cursor(nx_window *window, nx_cursor_shape shape) {
    if (!window || shape < NX_CURSOR_DEFAULT || shape > NX_CURSOR_BUSY)
        return NX_ERROR_ARGUMENT;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_get_reduced_motion(nx_window *window, int *out_enabled) {
    (void)window;
    if (!out_enabled) return NX_ERROR_ARGUMENT;
    *out_enabled = 0;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_set_ime_rect(nx_window *window, nx_rect caret) {
    (void)window;
    (void)caret;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_set_ime_enabled(nx_window *window, int enabled) {
    (void)window;
    (void)enabled;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_set_ime_purpose(nx_window *window,
                                    nx_ime_purpose purpose) {
    (void)window; (void)purpose;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_get_ime_preedit(nx_window *window,
    char *bytes, size_t capacity, size_t *out_length,
    int32_t *out_cursor_begin, int32_t *out_cursor_end) {
    (void)window; (void)bytes; (void)capacity;
    if (!out_length || !out_cursor_begin || !out_cursor_end ||
        (!bytes && capacity)) return NX_ERROR_ARGUMENT;
    *out_length = 0;
    *out_cursor_begin = *out_cursor_end = 0;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_set_ime_surrounding_text(nx_window *window,
    uint64_t target, uint64_t revision, const char *bytes, size_t length,
    size_t cursor, size_t anchor) {
    (void)window; (void)target; (void)revision; (void)bytes;
    (void)length; (void)cursor; (void)anchor;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_next_event(nx_window *window, nx_event *out_event, int *available) {
    (void)window;
    if (!out_event || !available) return NX_ERROR_ARGUMENT;
    *out_event = (nx_event){0};
    *available = 0;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_next_file_drop(nx_window *window,
                                   char *path_utf8, size_t capacity,
                                   size_t *out_length,
                                   nx_file_drop_info *out_info,
                                   int *available) {
    (void)window;
    if (!out_length || !out_info || !available || (!path_utf8 && capacity))
        return NX_ERROR_ARGUMENT;
    *out_length = 0;
    *out_info = (nx_file_drop_info){0};
    *available = 0;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_route_events(nx_window *window, nx_context *context,
                                 size_t *out_count) {
    (void)window;
    (void)context;
    if (!out_count) return NX_ERROR_ARGUMENT;
    *out_count = 0;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_clipboard_set_text(nx_window *window,
                                        const char *bytes, size_t length) {
    (void)window;
    if (!bytes && length) return NX_ERROR_ARGUMENT;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_clipboard_get_text(nx_window *window,
                                        char *bytes, size_t capacity,
                                        size_t *out_length) {
    (void)window;
    if (!out_length || (!bytes && capacity)) return NX_ERROR_ARGUMENT;
    *out_length = 0;
    return NX_ERROR_UNSUPPORTED;
}

void nx_window_destroy(nx_window *window) {
    (void)window;
}

nx_result nx_window_publish_semantics(nx_window *window, const nx_context *context) {
    (void)context;
    return window ? NX_ERROR_UNSUPPORTED : NX_ERROR_ARGUMENT;
}
