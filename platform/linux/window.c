#include "nexaui/nexaui.h"
#include "x11/backend.h"
#include "x11/window_internal.h"
#include "wayland/backend.h"
#include "wayland/window_internal.h"
#include "window_internal.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef enum nx_linux_backend {
    NX_LINUX_X11,
    NX_LINUX_WAYLAND
} nx_linux_backend;

struct nx_window {
    nx_allocator allocator;
    pthread_t owner_thread;
    nx_linux_backend backend;
    const void *renderer_owner;
    union {
        nx_x11_window *x11;
        nx_wayland_window *wayland;
    } native;
};

static void *nx_linux_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_linux_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static int nx_linux_on_owner_thread(const nx_window *window) {
    return pthread_equal(pthread_self(), window->owner_thread);
}

nx_result nx_window_create(const nx_window_desc *desc, nx_window **out_window) {
    if (!out_window) return NX_ERROR_ARGUMENT;
    *out_window = NULL;
    if (!desc || (desc->allocator.allocate == NULL) !=
                 (desc->allocator.deallocate == NULL)) return NX_ERROR_ARGUMENT;
    nx_allocator allocator = desc->allocator;
    if (!allocator.allocate)
        allocator = (nx_allocator){NULL, nx_linux_allocate, nx_linux_deallocate};
    const char *forced = getenv("NEXAUI_PLATFORM");
    if (forced && strcmp(forced, "wayland") && strcmp(forced, "x11"))
        return NX_ERROR_ARGUMENT;
    int try_wayland = forced ? !strcmp(forced, "wayland") :
                      getenv("WAYLAND_DISPLAY") != NULL;
    nx_result result = NX_ERROR_PLATFORM;
    nx_wayland_window *wayland = NULL;
    nx_x11_window *x11 = NULL;
    nx_linux_backend backend = NX_LINUX_X11;
    if (try_wayland) {
        result = nx_wayland_window_create(desc, &wayland);
        if (result == NX_OK) backend = NX_LINUX_WAYLAND;
        else if (forced || result != NX_ERROR_PLATFORM) return result;
    }
    if (!wayland) {
        result = nx_x11_window_create(desc, &x11);
        if (result != NX_OK) return result;
    }
    nx_window *window = (nx_window *)allocator.allocate(allocator.user,
                                                        sizeof(*window));
    if (!window) {
        if (wayland) nx_wayland_window_destroy(wayland);
        else nx_x11_window_destroy(x11);
        return NX_ERROR_MEMORY;
    }
    *window = (nx_window){0};
    window->allocator = allocator;
    window->owner_thread = pthread_self();
    window->backend = backend;
    if (wayland) window->native.wayland = wayland;
    else window->native.x11 = x11;
    *out_window = window;
    return NX_OK;
}

nx_result nx_window_poll(nx_window *window, uint32_t timeout_ms) {
    if (!window) return NX_ERROR_ARGUMENT;
    return window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_poll(window->native.wayland, timeout_ms) :
        nx_x11_window_poll(window->native.x11, timeout_ms);
}

nx_result nx_window_get_info(nx_window *window, nx_window_info *out_info) {
    if (!window) return NX_ERROR_ARGUMENT;
    return window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_get_info(window->native.wayland, out_info) :
        nx_x11_window_get_info(window->native.x11, out_info);
}

nx_result nx_window_renderer_acquire(nx_window *window, const void *owner) {
    if (!window || !owner) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window) || window->renderer_owner)
        return NX_ERROR_STATE;
    if (window->backend == NX_LINUX_WAYLAND) {
        nx_result result = nx_wayland_window_set_external_renderer(
            window->native.wayland, 1);
        if (result != NX_OK) return result;
    }
    window->renderer_owner = owner;
    return NX_OK;
}

nx_result nx_window_renderer_release(nx_window *window, const void *owner) {
    if (!window || !owner) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window) || window->renderer_owner != owner)
        return NX_ERROR_STATE;
    window->renderer_owner = NULL;
    return window->backend == NX_LINUX_WAYLAND
        ? nx_wayland_window_set_external_renderer(window->native.wayland, 0)
        : NX_OK;
}

nx_result nx_window_set_cursor(nx_window *window, nx_cursor_shape shape) {
    if (!window) return NX_ERROR_ARGUMENT;
    return window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_set_cursor(window->native.wayland, shape) :
        nx_x11_window_set_cursor(window->native.x11, shape);
}

nx_result nx_window_get_reduced_motion(nx_window *window, int *out_enabled) {
    if (!window) return NX_ERROR_ARGUMENT;
    return window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_get_reduced_motion(window->native.wayland,
                                              out_enabled) :
        nx_x11_window_get_reduced_motion(window->native.x11, out_enabled);
}

nx_result nx_window_set_ime_rect(nx_window *window, nx_rect caret) {
    if (!window) return NX_ERROR_ARGUMENT;
    return window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_set_ime_rect(window->native.wayland, caret) :
        nx_x11_window_set_ime_rect(window->native.x11, caret);
}

nx_result nx_window_set_ime_enabled(nx_window *window, int enabled) {
    if (!window || (enabled != 0 && enabled != 1)) return NX_ERROR_ARGUMENT;
    return window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_set_ime_enabled(window->native.wayland, enabled) :
        nx_x11_window_set_ime_enabled(window->native.x11, enabled);
}

nx_result nx_window_set_ime_purpose(nx_window *window,
                                    nx_ime_purpose purpose) {
    if (!window || (purpose != NX_IME_PURPOSE_NORMAL &&
                    purpose != NX_IME_PURPOSE_PASSWORD))
        return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    if (window->backend == NX_LINUX_WAYLAND)
        return nx_wayland_window_set_ime_purpose(window->native.wayland,
                                                 purpose);
    return purpose == NX_IME_PURPOSE_NORMAL ? NX_OK : NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_get_ime_preedit(nx_window *window,
    char *bytes, size_t capacity, size_t *out_length,
    int32_t *out_cursor_begin, int32_t *out_cursor_end) {
    if (!out_length || !out_cursor_begin || !out_cursor_end ||
        (!bytes && capacity)) return NX_ERROR_ARGUMENT;
    *out_length = 0;
    *out_cursor_begin = *out_cursor_end = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    return window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_get_ime_preedit(window->native.wayland,
            bytes, capacity, out_length, out_cursor_begin,
            out_cursor_end) :
        nx_x11_window_get_ime_preedit(window->native.x11,
            bytes, capacity, out_length, out_cursor_begin, out_cursor_end);
}

nx_result nx_window_set_ime_surrounding_text(nx_window *window,
    uint64_t target, uint64_t revision, const char *bytes, size_t length,
    size_t cursor, size_t anchor) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    return window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_set_ime_surrounding_text(window->native.wayland,
            target, revision, bytes, length, cursor, anchor) :
        NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_next_event(nx_window *window, nx_event *out_event,
                                int *available) {
    if (!window) {
        if (out_event) *out_event = (nx_event){0};
        if (available) *available = 0;
        return NX_ERROR_ARGUMENT;
    }
    return window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_next_event(window->native.wayland,
                                      out_event, available) :
        nx_x11_window_next_event(window->native.x11, out_event, available);
}

nx_result nx_window_route_events(nx_window *window, nx_context *context,
                                  size_t *out_count) {
    if (!window) {
        if (out_count) *out_count = 0;
        return NX_ERROR_ARGUMENT;
    }
    return window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_route_events(window->native.wayland,
                                        context, out_count) :
        nx_x11_window_route_events(window->native.x11, context, out_count);
}

nx_result nx_window_next_file_drop(nx_window *window, char *path_utf8,
                                    size_t capacity, size_t *out_length,
                                    nx_file_drop_info *out_info,
                                    int *available) {
    if (!window) {
        if (out_length) *out_length = 0;
        if (out_info) *out_info = (nx_file_drop_info){0};
        if (available) *available = 0;
        return NX_ERROR_ARGUMENT;
    }
    return window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_next_file_drop(window->native.wayland, path_utf8,
                    capacity, out_length, out_info, available) :
        nx_x11_window_next_file_drop(window->native.x11, path_utf8,
                    capacity, out_length, out_info, available);
}

nx_result nx_window_clipboard_set_text(nx_window *window,
                                        const char *bytes, size_t length) {
    if (!window) return NX_ERROR_ARGUMENT;
    return window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_clipboard_set_text(window->native.wayland,
                                              bytes, length) :
        nx_x11_window_clipboard_set_text(window->native.x11, bytes, length);
}

nx_result nx_window_clipboard_get_text(nx_window *window,
                                        char *bytes, size_t capacity,
                                        size_t *out_length) {
    if (!window) {
        if (out_length) *out_length = 0;
        return NX_ERROR_ARGUMENT;
    }
    return window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_clipboard_get_text(window->native.wayland,
                    bytes, capacity, out_length) :
        nx_x11_window_clipboard_get_text(window->native.x11,
                    bytes, capacity, out_length);
}

void nx_window_destroy(nx_window *window) {
    if (!window || !nx_linux_on_owner_thread(window)) return;
    if (window->backend == NX_LINUX_WAYLAND)
        nx_wayland_window_destroy(window->native.wayland);
    else nx_x11_window_destroy(window->native.x11);
    nx_allocator allocator = window->allocator;
    allocator.deallocate(allocator.user, window, sizeof(*window));
}

Display *nx_window_x11_display(nx_window *window) {
    return window && window->backend == NX_LINUX_X11 ?
        nx_x11_window_x11_display(window->native.x11) : NULL;
}

Window nx_window_x11_id(nx_window *window) {
    return window && window->backend == NX_LINUX_X11 ?
        nx_x11_window_x11_id(window->native.x11) : None;
}

#ifdef NEXAUI_X11_TEST_HOOKS
nx_result nx_window_x11_test_ime_edit(nx_window *window,
    const XIMText *text, int first, int removed, int caret) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (window->backend != NX_LINUX_X11) return NX_ERROR_UNSUPPORTED;
    return nx_x11_window_test_ime_edit(window->native.x11, text,
                                        first, removed, caret);
}

nx_result nx_window_x11_test_ime_clear(nx_window *window) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (window->backend != NX_LINUX_X11) return NX_ERROR_UNSUPPORTED;
    return nx_x11_window_test_ime_clear(window->native.x11);
}
#endif

struct wl_display *nx_window_wayland_display(nx_window *window) {
    return window && window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_display(window->native.wayland) : NULL;
}

struct wl_surface *nx_window_wayland_surface(nx_window *window) {
    return window && window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_surface(window->native.wayland) : NULL;
}

unsigned nx_window_wayland_ready(nx_window *window) {
    return window && window->backend == NX_LINUX_WAYLAND ?
        nx_wayland_window_ready(window->native.wayland) : 0u;
}

#ifdef NEXAUI_WAYLAND_TEST_HOOKS
nx_result nx_window_wayland_test_uri_drop(nx_window *window,
                                          const char *text, size_t length,
                                          float x, float y) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_uri_drop(window->native.wayland, text,
                                           length, x, y);
}

nx_result nx_window_wayland_test_clipboard_roundtrip(
    nx_window *window, const char *input, size_t length,
    char *output, size_t capacity, size_t *out_length) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_clipboard_roundtrip(
        window->native.wayland, input, length, output, capacity, out_length);
}

nx_result nx_window_wayland_test_pointer_enter(nx_window *window,
                                                int x, int y) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_pointer_enter(window->native.wayland,
                                                 x, y);
}

nx_result nx_window_wayland_test_focus(nx_window *window, int focused) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_focus(window->native.wayland, focused);
}

nx_result nx_window_wayland_test_key(nx_window *window, char letter, int down) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_key(window->native.wayland, letter, down);
}

nx_result nx_window_wayland_test_repeat_info(nx_window *window,
                                              int rate, int delay_ms) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_repeat_info(window->native.wayland,
                                               rate, delay_ms);
}

nx_result nx_window_wayland_test_output(nx_window *window,
                                        int scale, int entered) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_output(window->native.wayland,
                                         scale, entered);
}

nx_result nx_window_wayland_test_extents(nx_window *window,
                                         int *background_width,
                                         int *background_height,
                                         int *cursor_width,
                                         int *cursor_height) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_extents(window->native.wayland,
        background_width, background_height, cursor_width, cursor_height);
}

nx_result nx_window_wayland_test_ime_commit(nx_window *window,
                                            const char *text, int stale) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_ime_commit(window->native.wayland,
                                             text, stale);
}

nx_result nx_window_wayland_test_ime_preedit(nx_window *window,
    const char *text, int32_t cursor, int stale) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_ime_preedit(window->native.wayland,
        text, cursor, stale);
}

nx_result nx_window_wayland_test_ime_state(nx_window *window,
                                           int *requested, int *activated) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_ime_state(window->native.wayland,
                                            requested, activated);
}
#endif
