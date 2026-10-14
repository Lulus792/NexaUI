#define _GNU_SOURCE
#include "backend.h"
#include "cursor.h"
#include "drop_queue.h"
#include "event_queue.h"
#include "repeat.h"
#include "uri_list.h"
#include "../../../src/secure_zero.h"
#include "xdg-shell-client-protocol.h"
#include "text-input-unstable-v1-client-protocol.h"
#include "text-input-unstable-v3-client-protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

typedef struct nx_wayland_buffer {
    struct nx_wayland_buffer *next;
    struct nx_wayland_window *window;
    struct wl_buffer *native;
    void *pixels;
    size_t size;
} nx_wayland_buffer;

typedef struct nx_wayland_output {
    struct nx_wayland_output *next;
    struct nx_wayland_window *window;
    struct wl_output *native;
    uint32_t name;
    int scale, entered, removed;
    uint32_t version;
} nx_wayland_output;

typedef struct nx_wayland_clipboard_source {
    struct nx_wayland_clipboard_source *next;
    struct nx_wayland_window *window;
    struct wl_data_source *native;
    char *text;
    size_t length;
    unsigned write_refs;
    int cancelled;
} nx_wayland_clipboard_source;

typedef struct nx_wayland_clipboard_offer {
    struct nx_wayland_clipboard_offer *next;
    struct nx_wayland_window *window;
    struct wl_data_offer *native;
    unsigned mime_types, in_use;
    int obsolete;
} nx_wayland_clipboard_offer;

typedef struct nx_wayland_clipboard_write {
    struct nx_wayland_clipboard_write *next;
    nx_wayland_clipboard_source *source;
    int fd;
    size_t length, offset;
} nx_wayland_clipboard_write;

struct nx_wayland_window {
    struct nx_wayland_window *thread_next;
    nx_allocator allocator;
    pthread_t owner_thread;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_data_device_manager *data_manager;
    struct wl_data_device *data_device;
    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;
    struct zwp_text_input_manager_v1 *text_input_manager;
    struct zwp_text_input_v1 *text_input;
    struct zwp_text_input_manager_v3 *text_input_manager_v3;
    struct zwp_text_input_v3 *text_input_v3;
    char *ime_pending_commit;
    size_t ime_pending_length;
    char *ime_preedit, *ime_pending_preedit;
    size_t ime_preedit_length, ime_pending_preedit_length;
    int32_t ime_preedit_begin, ime_preedit_end;
    int32_t ime_pending_preedit_begin, ime_pending_preedit_end;
    int32_t ime_v1_preedit_cursor;
    int ime_v1_preedit_cursor_set;
    char *ime_surrounding;
    size_t ime_surrounding_length, ime_surrounding_cursor;
    size_t ime_surrounding_anchor;
    uint64_t ime_surrounding_target, ime_surrounding_revision;
    uint32_t ime_surrounding_serial;
    uint32_t ime_pending_delete_before, ime_pending_delete_after;
    int32_t ime_v1_delete_index;
    uint32_t ime_v1_delete_length;
    int ime_v1_delete_pending;
    int ime_v1_delete_invalid;
    int ime_surrounding_sent;
    int ime_surrounding_withdrawn;
    uint32_t ime_serial, ime_activation_serial;
    int ime_requested, ime_activated, ime_entered, ime_caret_set;
    int ime_password;
    int ime_v3_focused;
    nx_rect ime_caret;
    struct xkb_context *xkb_context;
    struct xkb_keymap *keymap;
    struct xkb_state *key_state;
    struct xdg_wm_base *wm_base;
    struct wl_surface *surface;
    struct wl_surface *cursor_surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *toplevel;
    nx_wayland_buffer *buffers;
    nx_wayland_output *outputs;
    nx_wayland_clipboard_source *sources, *owned_source;
    nx_wayland_clipboard_offer *offers, *selection_offer;
    nx_wayland_clipboard_offer *pending_offer, *drag_offer, *drop_offer;
    float drop_x, drop_y;
    nx_platform_drop_queue drop_queue;
    nx_wayland_clipboard_write *writes;
    unsigned write_count;
    unsigned transfer_read_depth;
    uint32_t selection_serial;
    nx_platform_event_queue event_queue;
    int width, height, scale;
    int background_width, background_height, cursor_width, cursor_height;
    int pending_width, pending_height;
    int configured, should_close, hidden, buffer_attached;
    int external_renderer;
    int pointer_inside;
    uint32_t pointer_serial;
    nx_cursor_shape cursor_shape;
    float pointer_x, pointer_y;
    float axis_x, axis_y;
    int axis_steps_x, axis_steps_y;
    int keyboard_focused;
    unsigned char keys_down[256];
    nx_wayland_repeat repeat;
    nx_result error;
};

static _Thread_local nx_wayland_window *nx_wayland_thread_windows;

enum {
    NX_WAYLAND_MIME_UTF8 = 1u,
    NX_WAYLAND_MIME_UTF8_STRING = 2u,
    NX_WAYLAND_MIME_PLAIN = 4u,
    NX_WAYLAND_MIME_URI_LIST = 8u
};

#define NX_WAYLAND_CLIPBOARD_LIMIT (32u * 1024u * 1024u)
#define NX_WAYLAND_CLIPBOARD_UTF8 "text/plain;charset=utf-8"

static void *nx_wayland_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_wayland_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static int nx_wayland_on_owner_thread(const nx_wayland_window *window) {
    return pthread_equal(pthread_self(), window->owner_thread);
}

static uint32_t nx_wayland_modifiers(nx_wayland_window *window);
static nx_result nx_wayland_apply_cursor(nx_wayland_window *window);
static nx_result nx_wayland_attach_background(nx_wayland_window *window);
static nx_result nx_wayland_process_drop(nx_wayland_window *window);
static void nx_wayland_ime_activate(nx_wayland_window *window);
static void nx_wayland_ime_deactivate(nx_wayland_window *window);

static void nx_wayland_refresh_scale(nx_wayland_window *window) {
    int scale = 1;
    for (nx_wayland_output *output = window->outputs; output;
         output = output->next)
        if (output->entered && !output->removed && output->scale > scale)
            scale = output->scale;
    if (window->surface && wl_surface_get_version(window->surface) < 3)
        scale = 1;
    if (scale == window->scale) return;
    if (scale > INT_MAX / window->width ||
        scale > INT_MAX / window->height ||
        window->width * scale > INT_MAX / 4 / (window->height * scale)) {
        window->error = NX_ERROR_LIMIT;
        return;
    }
    window->scale = scale;
    nx_event event = {0};
    event.type = NX_EVENT_RESIZE;
    event.width = (float)window->width;
    event.height = (float)window->height;
    event.scale = (float)scale;
    nx_platform_event_queue_push(&window->event_queue, event);
    if (window->error == NX_OK && window->configured && !window->hidden)
        window->error = nx_wayland_attach_background(window);
    if (window->error == NX_OK)
        window->error = nx_wayland_apply_cursor(window);
}

static void nx_wayland_output_geometry(void *data, struct wl_output *native,
                                       int32_t x, int32_t y, int32_t mm_width,
                                       int32_t mm_height, int32_t subpixel,
                                       const char *make, const char *model,
                                       int32_t transform) {
    (void)data; (void)native; (void)x; (void)y; (void)mm_width;
    (void)mm_height; (void)subpixel; (void)make; (void)model;
    (void)transform;
}

static void nx_wayland_output_mode(void *data, struct wl_output *native,
                                   uint32_t flags, int32_t width,
                                   int32_t height, int32_t refresh) {
    (void)data; (void)native; (void)flags; (void)width;
    (void)height; (void)refresh;
}

static void nx_wayland_output_done(void *data, struct wl_output *native) {
    (void)data; (void)native;
}

static void nx_wayland_output_scale(void *data, struct wl_output *native,
                                    int32_t factor) {
    nx_wayland_output *output = data;
    (void)native;
    if (factor < 1 || output->removed) return;
    output->scale = factor;
    nx_wayland_refresh_scale(output->window);
}

static const struct wl_output_listener nx_wayland_output_listener = {
    .geometry = nx_wayland_output_geometry,
    .mode = nx_wayland_output_mode,
    .done = nx_wayland_output_done,
    .scale = nx_wayland_output_scale
};

static void nx_wayland_surface_enter(void *data, struct wl_surface *surface,
                                     struct wl_output *native) {
    nx_wayland_window *window = data;
    if (surface != window->surface) return;
    for (nx_wayland_output *output = window->outputs; output;
         output = output->next)
        if (output->native == native && !output->removed) {
            output->entered = 1;
            nx_wayland_refresh_scale(window);
            return;
        }
}

static void nx_wayland_surface_leave(void *data, struct wl_surface *surface,
                                     struct wl_output *native) {
    nx_wayland_window *window = data;
    if (surface != window->surface) return;
    for (nx_wayland_output *output = window->outputs; output;
         output = output->next)
        if (output->native == native) {
            output->entered = 0;
            nx_wayland_refresh_scale(window);
            return;
        }
}

static const struct wl_surface_listener nx_wayland_native_surface_listener = {
    .enter = nx_wayland_surface_enter,
    .leave = nx_wayland_surface_leave
};

static uint64_t nx_wayland_now_ns(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
    return (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
}

static ssize_t nx_wayland_write_without_sigpipe(int fd, const void *bytes,
                                                 size_t length) {
    sigset_t blocked, previous, pending;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &blocked, &previous) != 0) {
        errno = EIO;
        return -1;
    }
    sigpending(&pending);
    int already_pending = sigismember(&pending, SIGPIPE);
    ssize_t written = write(fd, bytes, length);
    int saved_errno = errno;
    if (written < 0 && saved_errno == EPIPE && !already_pending) {
        struct timespec zero = {0};
        (void)sigtimedwait(&blocked, NULL, &zero);
    }
    pthread_sigmask(SIG_SETMASK, &previous, NULL);
    errno = saved_errno;
    return written;
}

static void nx_wayland_free_source(nx_wayland_clipboard_source *source);

static void nx_wayland_progress_writes(nx_wayland_window *window) {
    nx_wayland_clipboard_write **slot = &window->writes;
    while (*slot) {
        nx_wayland_clipboard_write *transfer = *slot;
        while (transfer->offset < transfer->length) {
            ssize_t count = nx_wayland_write_without_sigpipe(transfer->fd,
                transfer->source->text + transfer->offset,
                transfer->length - transfer->offset);
            if (count > 0) transfer->offset += (size_t)count;
            else if (count < 0 && errno == EINTR) continue;
            else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                break;
            else transfer->offset = transfer->length;
        }
        if (transfer->offset < transfer->length) {
            slot = &transfer->next;
            continue;
        }
        close(transfer->fd);
        *slot = transfer->next;
        nx_wayland_clipboard_source *source = transfer->source;
        --source->write_refs;
        window->allocator.deallocate(window->allocator.user, transfer,
                                     sizeof(*transfer));
        --window->write_count;
        if (source->cancelled && !source->write_refs)
            nx_wayland_free_source(source);
    }
}

static void nx_wayland_free_source(nx_wayland_clipboard_source *source) {
    nx_wayland_window *window = source->window;
    nx_wayland_clipboard_source **slot = &window->sources;
    while (*slot && *slot != source) slot = &(*slot)->next;
    if (*slot) *slot = source->next;
    if (window->owned_source == source) window->owned_source = NULL;
    if (source->native) wl_data_source_destroy(source->native);
    window->allocator.deallocate(window->allocator.user, source->text,
                                 source->length + 1u);
    window->allocator.deallocate(window->allocator.user, source,
                                 sizeof(*source));
}

static void nx_wayland_source_target(void *data,
                                     struct wl_data_source *native,
                                     const char *mime_type) {
    (void)data; (void)native; (void)mime_type;
}

static void nx_wayland_source_send(void *data,
                                   struct wl_data_source *native,
                                   const char *mime_type, int32_t fd) {
    nx_wayland_clipboard_source *source = data;
    nx_wayland_window *window = source->window;
    (void)native;
    if (!mime_type || (strcmp(mime_type, NX_WAYLAND_CLIPBOARD_UTF8) &&
                       strcmp(mime_type, "text/plain")) ||
        window->write_count >= 4u || !source->length) {
        close(fd);
        return;
    }
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(fd);
        return;
    }
    nx_wayland_clipboard_write *transfer = window->allocator.allocate(
        window->allocator.user, sizeof(*transfer));
    if (!transfer) { close(fd); return; }
    memset(transfer, 0, sizeof(*transfer));
    transfer->source = source;
    transfer->fd = fd;
    transfer->length = source->length;
    ++source->write_refs;
    transfer->next = window->writes;
    window->writes = transfer;
    ++window->write_count;
    nx_wayland_progress_writes(window);
}

static void nx_wayland_source_cancelled(void *data,
                                        struct wl_data_source *native) {
    (void)native;
    nx_wayland_clipboard_source *source = data;
    source->cancelled = 1;
    if (source->window->owned_source == source)
        source->window->owned_source = NULL;
    if (!source->write_refs) nx_wayland_free_source(source);
}

static const struct wl_data_source_listener nx_wayland_source_listener = {
    .target = nx_wayland_source_target,
    .send = nx_wayland_source_send,
    .cancelled = nx_wayland_source_cancelled
};

static void nx_wayland_retire_offer(nx_wayland_clipboard_offer *offer) {
    if (!offer) return;
    offer->obsolete = 1;
    if (offer->in_use) return;
    nx_wayland_window *window = offer->window;
    nx_wayland_clipboard_offer **slot = &window->offers;
    while (*slot && *slot != offer) slot = &(*slot)->next;
    if (*slot) *slot = offer->next;
    wl_data_offer_destroy(offer->native);
    window->allocator.deallocate(window->allocator.user, offer,
                                 sizeof(*offer));
}

static nx_wayland_clipboard_offer *nx_wayland_find_offer(
    nx_wayland_window *window, struct wl_data_offer *native) {
    for (nx_wayland_clipboard_offer *offer = window->offers; offer;
         offer = offer->next)
        if (offer->native == native) return offer;
    return NULL;
}

static void nx_wayland_offer_mime(void *data,
                                  struct wl_data_offer *native,
                                  const char *mime_type) {
    nx_wayland_clipboard_offer *offer = data;
    (void)native;
    if (!strcmp(mime_type, NX_WAYLAND_CLIPBOARD_UTF8))
        offer->mime_types |= NX_WAYLAND_MIME_UTF8;
    else if (!strcmp(mime_type, "UTF8_STRING"))
        offer->mime_types |= NX_WAYLAND_MIME_UTF8_STRING;
    else if (!strcmp(mime_type, "text/plain"))
        offer->mime_types |= NX_WAYLAND_MIME_PLAIN;
    else if (!strcmp(mime_type, "text/uri-list"))
        offer->mime_types |= NX_WAYLAND_MIME_URI_LIST;
}

static const struct wl_data_offer_listener nx_wayland_offer_listener = {
    .offer = nx_wayland_offer_mime
};

static void nx_wayland_device_offer(void *data,
                                    struct wl_data_device *device,
                                    struct wl_data_offer *native) {
    nx_wayland_window *window = data;
    (void)device;
    nx_wayland_clipboard_offer *offer = window->allocator.allocate(
        window->allocator.user, sizeof(*offer));
    if (!offer) {
        wl_data_offer_destroy(native);
        window->error = NX_ERROR_MEMORY;
        return;
    }
    memset(offer, 0, sizeof(*offer));
    offer->window = window;
    offer->native = native;
    if (wl_data_offer_add_listener(native, &nx_wayland_offer_listener,
                                   offer) != 0) {
        wl_data_offer_destroy(native);
        window->allocator.deallocate(window->allocator.user, offer,
                                     sizeof(*offer));
        window->error = NX_ERROR_PLATFORM;
        return;
    }
    if (window->pending_offer) nx_wayland_retire_offer(window->pending_offer);
    offer->next = window->offers;
    window->offers = offer;
    window->pending_offer = offer;
}

static void nx_wayland_device_enter(void *data,
                                    struct wl_data_device *device,
                                    uint32_t serial, struct wl_surface *surface,
                                    wl_fixed_t x, wl_fixed_t y,
                                    struct wl_data_offer *native) {
    nx_wayland_window *window = data;
    (void)device;
    if (window->drag_offer) nx_wayland_retire_offer(window->drag_offer);
    window->drag_offer = nx_wayland_find_offer(window, native);
    if (window->pending_offer == window->drag_offer)
        window->pending_offer = NULL;
    if (window->drag_offer && surface != window->surface) {
        wl_data_offer_accept(window->drag_offer->native, serial, NULL);
        nx_wayland_retire_offer(window->drag_offer);
        window->drag_offer = NULL;
    } else if (window->drag_offer) {
        window->drop_x = (float)wl_fixed_to_double(x);
        window->drop_y = (float)wl_fixed_to_double(y);
        wl_data_offer_accept(window->drag_offer->native, serial,
            window->drag_offer->mime_types & NX_WAYLAND_MIME_URI_LIST ?
                "text/uri-list" : NULL);
    }
}

static void nx_wayland_device_leave(void *data,
                                    struct wl_data_device *device) {
    nx_wayland_window *window = data;
    (void)device;
    nx_wayland_retire_offer(window->drag_offer);
    window->drag_offer = NULL;
}

static void nx_wayland_device_motion(void *data,
                                     struct wl_data_device *device,
                                     uint32_t time, wl_fixed_t x,
                                     wl_fixed_t y) {
    nx_wayland_window *window = data;
    (void)device; (void)time;
    if (window->drag_offer) {
        window->drop_x = (float)wl_fixed_to_double(x);
        window->drop_y = (float)wl_fixed_to_double(y);
    }
}

static void nx_wayland_device_drop(void *data,
                                   struct wl_data_device *device) {
    nx_wayland_window *window = data;
    (void)device;
    if (window->drop_offer) nx_wayland_retire_offer(window->drop_offer);
    window->drop_offer = window->drag_offer;
    window->drag_offer = NULL;
}

static void nx_wayland_device_selection(void *data,
                                        struct wl_data_device *device,
                                        struct wl_data_offer *native) {
    nx_wayland_window *window = data;
    (void)device;
    nx_wayland_clipboard_offer *selected =
        nx_wayland_find_offer(window, native);
    if (window->selection_offer && window->selection_offer != selected)
        nx_wayland_retire_offer(window->selection_offer);
    window->selection_offer = selected;
    window->owned_source = NULL;
    if (window->pending_offer == selected) window->pending_offer = NULL;
}

static const struct wl_data_device_listener nx_wayland_device_listener = {
    .data_offer = nx_wayland_device_offer,
    .enter = nx_wayland_device_enter,
    .leave = nx_wayland_device_leave,
    .motion = nx_wayland_device_motion,
    .drop = nx_wayland_device_drop,
    .selection = nx_wayland_device_selection
};

static void nx_wayland_registry_global(void *data, struct wl_registry *registry,
                                       uint32_t name, const char *interface,
                                       uint32_t version) {
    nx_wayland_window *window = (nx_wayland_window *)data;
    if (!strcmp(interface, wl_compositor_interface.name) &&
        !window->compositor)
        window->compositor = wl_registry_bind(registry, name,
            &wl_compositor_interface, version < 4 ? version : 4);
    else if (!strcmp(interface, xdg_wm_base_interface.name) &&
             !window->wm_base)
        window->wm_base = wl_registry_bind(registry, name,
            &xdg_wm_base_interface, 1);
    else if (!strcmp(interface, wl_shm_interface.name) && !window->shm)
        window->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    else if (!strcmp(interface, wl_seat_interface.name) && !window->seat &&
             version >= 5)
        window->seat = wl_registry_bind(registry, name, &wl_seat_interface, 5);
    else if (!strcmp(interface, zwp_text_input_manager_v1_interface.name) &&
             !window->text_input_manager)
        window->text_input_manager = wl_registry_bind(registry, name,
            &zwp_text_input_manager_v1_interface, 1);
    else if (!strcmp(interface, zwp_text_input_manager_v3_interface.name) &&
             !window->text_input_manager_v3)
        window->text_input_manager_v3 = wl_registry_bind(registry, name,
            &zwp_text_input_manager_v3_interface, 1);
    else if (!strcmp(interface, wl_data_device_manager_interface.name) &&
             !window->data_manager)
        window->data_manager = wl_registry_bind(registry, name,
            &wl_data_device_manager_interface, 1);
    else if (!strcmp(interface, wl_output_interface.name) && version >= 2) {
        nx_wayland_output *output = window->allocator.allocate(
            window->allocator.user, sizeof(*output));
        if (!output) { window->error = NX_ERROR_MEMORY; return; }
        memset(output, 0, sizeof(*output));
        output->window = window;
        output->name = name;
        output->scale = 1;
        output->version = version < 3 ? version : 3;
        output->native = wl_registry_bind(registry, name,
            &wl_output_interface, output->version);
        if (!output->native || wl_output_add_listener(output->native,
                &nx_wayland_output_listener, output) != 0) {
            if (output->native) wl_output_destroy(output->native);
            window->allocator.deallocate(window->allocator.user, output,
                                         sizeof(*output));
            window->error = NX_ERROR_PLATFORM;
            return;
        }
        output->next = window->outputs;
        window->outputs = output;
    }
}

static void nx_wayland_registry_remove(void *data, struct wl_registry *registry,
                                       uint32_t name) {
    (void)registry;
    nx_wayland_window *window = data;
    for (nx_wayland_output *output = window->outputs; output;
         output = output->next)
        if (output->name == name && !output->removed) {
            output->removed = 1;
            output->entered = 0;
            nx_wayland_refresh_scale(window);
            return;
        }
}

static const struct wl_registry_listener nx_wayland_registry_listener = {
    nx_wayland_registry_global, nx_wayland_registry_remove
};

static void nx_wayland_pointer_enter(void *data, struct wl_pointer *pointer,
                                     uint32_t serial, struct wl_surface *surface,
                                     wl_fixed_t x, wl_fixed_t y) {
    nx_wayland_window *window = data;
    (void)pointer;
    if (surface != window->surface) return;
    window->pointer_inside = 1;
    window->pointer_serial = serial;
    window->pointer_x = (float)wl_fixed_to_double(x);
    window->pointer_y = (float)wl_fixed_to_double(y);
    nx_event event = {0};
    event.type = NX_EVENT_POINTER_MOVE;
    event.x = window->pointer_x;
    event.y = window->pointer_y;
    event.modifiers = nx_wayland_modifiers(window);
    nx_platform_event_queue_push(&window->event_queue, event);
    if (window->error == NX_OK)
        window->error = nx_wayland_apply_cursor(window);
}

static void nx_wayland_pointer_leave(void *data, struct wl_pointer *pointer,
                                     uint32_t serial,
                                     struct wl_surface *surface) {
    nx_wayland_window *window = data;
    (void)pointer;
    (void)serial;
    if (surface == window->surface) {
        window->pointer_inside = 0;
        window->axis_x = window->axis_y = 0.0f;
        window->axis_steps_x = window->axis_steps_y = 0;
    }
}

static void nx_wayland_pointer_motion(void *data, struct wl_pointer *pointer,
                                      uint32_t time, wl_fixed_t x,
                                      wl_fixed_t y) {
    nx_wayland_window *window = data;
    (void)pointer;
    (void)time;
    if (!window->pointer_inside) return;
    window->pointer_x = (float)wl_fixed_to_double(x);
    window->pointer_y = (float)wl_fixed_to_double(y);
    nx_event event = {0};
    event.type = NX_EVENT_POINTER_MOVE;
    event.x = window->pointer_x;
    event.y = window->pointer_y;
    event.modifiers = nx_wayland_modifiers(window);
    nx_platform_event_queue_push(&window->event_queue, event);
}

static void nx_wayland_pointer_button(void *data, struct wl_pointer *pointer,
                                      uint32_t serial, uint32_t time,
                                      uint32_t button, uint32_t state) {
    nx_wayland_window *window = data;
    (void)pointer;
    (void)time;
    if (!window->pointer_inside) return;
    if (state == WL_POINTER_BUTTON_STATE_PRESSED)
        window->selection_serial = serial;
    uint32_t logical = 0;
    switch (button) {
    case BTN_LEFT: logical = NX_BUTTON_LEFT; break;
    case BTN_MIDDLE: logical = NX_BUTTON_MIDDLE; break;
    case BTN_RIGHT: logical = NX_BUTTON_RIGHT; break;
    case BTN_SIDE: logical = NX_BUTTON_X1; break;
    case BTN_EXTRA: logical = NX_BUTTON_X2; break;
    default: return;
    }
    nx_event event = {0};
    event.type = state == WL_POINTER_BUTTON_STATE_PRESSED ?
                 NX_EVENT_POINTER_DOWN : NX_EVENT_POINTER_UP;
    event.x = window->pointer_x;
    event.y = window->pointer_y;
    event.button = logical;
    event.modifiers = nx_wayland_modifiers(window);
    nx_platform_event_queue_push(&window->event_queue, event);
}

static void nx_wayland_pointer_axis(void *data, struct wl_pointer *pointer,
                                    uint32_t time, uint32_t axis,
                                    wl_fixed_t value) {
    nx_wayland_window *window = data;
    (void)pointer;
    (void)time;
    float amount = (float)wl_fixed_to_double(value);
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) window->axis_y -= amount;
    else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
        window->axis_x -= amount;
}

static void nx_wayland_pointer_frame(void *data, struct wl_pointer *pointer) {
    nx_wayland_window *window = data;
    (void)pointer;
    if (window->axis_x != 0.0f || window->axis_y != 0.0f ||
        window->axis_steps_x || window->axis_steps_y) {
        nx_event event = {0};
        event.type = NX_EVENT_WHEEL;
        event.x = window->pointer_x;
        event.y = window->pointer_y;
        event.modifiers = nx_wayland_modifiers(window);
        event.delta_x = window->axis_steps_x ?
                        (float)window->axis_steps_x : window->axis_x / 10.0f;
        event.delta_y = window->axis_steps_y ?
                        (float)window->axis_steps_y : window->axis_y / 10.0f;
        nx_platform_event_queue_push(&window->event_queue, event);
    }
    window->axis_x = window->axis_y = 0.0f;
    window->axis_steps_x = window->axis_steps_y = 0;
}

static void nx_wayland_pointer_axis_discrete(void *data,
                                             struct wl_pointer *pointer,
                                             uint32_t axis, int32_t discrete) {
    nx_wayland_window *window = data;
    (void)pointer;
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
        window->axis_steps_y -= discrete;
    else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
        window->axis_steps_x -= discrete;
}

static const struct wl_pointer_listener nx_wayland_pointer_listener = {
    .enter = nx_wayland_pointer_enter,
    .leave = nx_wayland_pointer_leave,
    .motion = nx_wayland_pointer_motion,
    .button = nx_wayland_pointer_button,
    .axis = nx_wayland_pointer_axis,
    .frame = nx_wayland_pointer_frame,
    .axis_discrete = nx_wayland_pointer_axis_discrete
};

static nx_key nx_wayland_key(xkb_keysym_t sym) {
    if (sym >= XKB_KEY_a && sym <= XKB_KEY_z)
        return (nx_key)(NX_KEY_A + sym - XKB_KEY_a);
    if (sym >= XKB_KEY_A && sym <= XKB_KEY_Z)
        return (nx_key)(NX_KEY_A + sym - XKB_KEY_A);
    if (sym >= XKB_KEY_0 && sym <= XKB_KEY_9)
        return (nx_key)(NX_KEY_0 + sym - XKB_KEY_0);
    if (sym >= XKB_KEY_KP_0 && sym <= XKB_KEY_KP_9)
        return (nx_key)(NX_KEY_0 + sym - XKB_KEY_KP_0);
    if (sym >= XKB_KEY_F1 && sym <= XKB_KEY_F12)
        return (nx_key)(NX_KEY_F1 + sym - XKB_KEY_F1);
    switch (sym) {
    case XKB_KEY_Tab: case XKB_KEY_ISO_Left_Tab: return NX_KEY_TAB;
    case XKB_KEY_Return: case XKB_KEY_KP_Enter: return NX_KEY_ENTER;
    case XKB_KEY_Escape: return NX_KEY_ESCAPE;
    case XKB_KEY_space: return NX_KEY_SPACE;
    case XKB_KEY_BackSpace: return NX_KEY_BACKSPACE;
    case XKB_KEY_Delete: return NX_KEY_DELETE;
    case XKB_KEY_Insert: return NX_KEY_INSERT;
    case XKB_KEY_Left: return NX_KEY_LEFT;
    case XKB_KEY_Right: return NX_KEY_RIGHT;
    case XKB_KEY_Up: return NX_KEY_UP;
    case XKB_KEY_Down: return NX_KEY_DOWN;
    case XKB_KEY_Home: return NX_KEY_HOME;
    case XKB_KEY_End: return NX_KEY_END;
    case XKB_KEY_Prior: return NX_KEY_PAGE_UP;
    case XKB_KEY_Next: return NX_KEY_PAGE_DOWN;
    case XKB_KEY_Shift_L: case XKB_KEY_Shift_R: return NX_KEY_SHIFT;
    case XKB_KEY_Control_L: case XKB_KEY_Control_R: return NX_KEY_CONTROL;
    case XKB_KEY_Alt_L: case XKB_KEY_Alt_R: return NX_KEY_ALT;
    case XKB_KEY_Super_L: case XKB_KEY_Super_R: return NX_KEY_SUPER;
    default: return NX_KEY_UNKNOWN;
    }
}

static uint32_t nx_wayland_modifiers(nx_wayland_window *window) {
    if (!window->key_state) return 0;
    uint32_t modifiers = 0;
    if (xkb_state_mod_name_is_active(window->key_state,
            XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0)
        modifiers |= NX_MOD_SHIFT;
    if (xkb_state_mod_name_is_active(window->key_state,
            XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0)
        modifiers |= NX_MOD_CONTROL;
    if (xkb_state_mod_name_is_active(window->key_state,
            XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0)
        modifiers |= NX_MOD_ALT;
    if (xkb_state_mod_name_is_active(window->key_state,
            XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE) > 0)
        modifiers |= NX_MOD_SUPER;
    return modifiers;
}

static int nx_wayland_ime_serial_valid(const nx_wayland_window *window,
                                       uint32_t serial) {
    return window->ime_requested && window->ime_activated &&
           window->ime_entered &&
           (uint32_t)(serial - window->ime_activation_serial) <=
           (uint32_t)(window->ime_serial - window->ime_activation_serial);
}

static int nx_wayland_ime_v1_serial_valid(const nx_wayland_window *window,
                                          uint32_t serial) {
    return nx_wayland_ime_serial_valid(window, serial) &&
           serial == window->ime_serial;
}

static void nx_wayland_ime_clear_pending_commit(nx_wayland_window *window) {
    if (window->ime_pending_commit) {
        if (window->ime_password)
            nx_private_secure_zero(window->ime_pending_commit,
                window->ime_pending_length + 1u);
        window->allocator.deallocate(window->allocator.user,
            window->ime_pending_commit, window->ime_pending_length + 1u);
    }
    window->ime_pending_commit = NULL;
    window->ime_pending_length = 0;
}

static void nx_wayland_ime_clear_pending(nx_wayland_window *window) {
    nx_wayland_ime_clear_pending_commit(window);
    if (window->ime_pending_preedit) {
        if (window->ime_password)
            nx_private_secure_zero(window->ime_pending_preedit,
                window->ime_pending_preedit_length + 1u);
        window->allocator.deallocate(window->allocator.user,
            window->ime_pending_preedit,
            window->ime_pending_preedit_length + 1u);
    }
    window->ime_pending_preedit = NULL;
    window->ime_pending_preedit_length = 0;
    window->ime_pending_preedit_begin = 0;
    window->ime_pending_preedit_end = 0;
    window->ime_pending_delete_before = 0;
    window->ime_pending_delete_after = 0;
}

static void nx_wayland_ime_clear_surrounding(nx_wayland_window *window) {
    if (window->ime_surrounding) {
        if (window->ime_password)
            nx_private_secure_zero(window->ime_surrounding,
                window->ime_surrounding_length + 1u);
        window->allocator.deallocate(window->allocator.user,
            window->ime_surrounding, window->ime_surrounding_length + 1u);
    }
    window->ime_surrounding = NULL;
    window->ime_surrounding_length = 0;
    window->ime_surrounding_cursor = 0;
    window->ime_surrounding_anchor = 0;
    window->ime_surrounding_target = 0;
    window->ime_surrounding_revision = 0;
    window->ime_surrounding_serial = 0;
    window->ime_surrounding_sent = 0;
    window->ime_surrounding_withdrawn = 0;
}

static void nx_wayland_ime_v1_clear_delete(nx_wayland_window *window) {
    window->ime_v1_delete_index = 0;
    window->ime_v1_delete_length = 0;
    window->ime_v1_delete_pending = 0;
    window->ime_v1_delete_invalid = 0;
}

static void nx_wayland_ime_clear_preedit(nx_wayland_window *window,
                                         int notify_change) {
    int changed = window->ime_preedit_length != 0;
    if (window->ime_preedit) {
        if (window->ime_password)
            nx_private_secure_zero(window->ime_preedit,
                window->ime_preedit_length + 1u);
        window->allocator.deallocate(window->allocator.user,
            window->ime_preedit, window->ime_preedit_length + 1u);
    }
    window->ime_preedit = NULL;
    window->ime_preedit_length = 0;
    window->ime_preedit_begin = 0;
    window->ime_preedit_end = 0;
    if (changed && notify_change) {
        nx_event event = {0};
        event.type = NX_EVENT_IME_PREEDIT_CHANGED;
        nx_platform_event_queue_push(&window->event_queue, event);
    }
}

static void nx_wayland_ime_emit_utf8(nx_wayland_window *window,
                                     const char *text, size_t length,
                                     uint64_t target, uint64_t revision) {
    if (nx_utf8_validate(text, length, NULL) != NX_OK) return;
    size_t offset = 0;
    while (offset < length) {
        uint32_t scalar = 0;
        if (nx_utf8_decode(text, length, &offset, &scalar) != NX_OK) break;
        if (scalar < 0x20u || scalar == 0x7fu) continue;
        nx_event event = {0};
        event.type = NX_EVENT_TEXT;
        event.scalar = scalar;
        event.ime_target = target;
        event.ime_revision = revision;
        nx_platform_event_queue_push(&window->event_queue, event);
    }
}

static void nx_wayland_ime_enter(void *data, struct zwp_text_input_v1 *input,
                                 struct wl_surface *surface) {
    nx_wayland_window *window = data;
    (void)input;
    if (surface == window->surface && window->ime_activated)
        window->ime_entered = 1;
}

static void nx_wayland_ime_leave(void *data, struct zwp_text_input_v1 *input) {
    (void)input;
    nx_wayland_window *window = data;
    window->ime_entered = 0;
    window->ime_v1_preedit_cursor_set = 0;
    nx_wayland_ime_v1_clear_delete(window);
    nx_wayland_ime_clear_surrounding(window);
    nx_wayland_ime_clear_preedit(window, 1);
}

static void nx_wayland_ime_modifiers_map(void *data,
    struct zwp_text_input_v1 *input, struct wl_array *map) {
    (void)data; (void)input; (void)map;
}

static void nx_wayland_ime_panel_state(void *data,
    struct zwp_text_input_v1 *input, uint32_t state) {
    (void)data; (void)input; (void)state;
}

static void nx_wayland_ime_preedit(void *data,
    struct zwp_text_input_v1 *input, uint32_t serial,
    const char *value, const char *commit) {
    nx_wayland_window *window = data;
    (void)input; (void)commit;
    int cursor_set = window->ime_v1_preedit_cursor_set;
    int32_t cursor = window->ime_v1_preedit_cursor;
    window->ime_v1_preedit_cursor_set = 0;
    if (!nx_wayland_ime_v1_serial_valid(window, serial) || !value) return;
    size_t length = strnlen(value, 1048577u);
    if (length > 1048576u) {
        window->event_queue.error = NX_ERROR_LIMIT;
        return;
    }
    if (nx_utf8_validate(value, length, NULL) != NX_OK ||
        (cursor_set && (cursor < -1 ||
            (cursor >= 0 && ((size_t)cursor > length ||
             ((size_t)cursor < length &&
              ((unsigned char)value[cursor] & 0xc0u) == 0x80u)))))) {
        window->event_queue.error = NX_ERROR_PLATFORM;
        return;
    }
    if (!cursor_set) cursor = (int32_t)length;
    if (!length) cursor = 0;
    int changed = length != window->ime_preedit_length ||
        cursor != window->ime_preedit_begin ||
        cursor != window->ime_preedit_end ||
        (length && memcmp(value, window->ime_preedit, length) != 0);
    if (!changed) return;
    char *copy = NULL;
    if (length) {
        copy = window->allocator.allocate(window->allocator.user, length + 1u);
        if (!copy) {
            window->event_queue.error = NX_ERROR_MEMORY;
            return;
        }
        memcpy(copy, value, length + 1u);
    }
    nx_wayland_ime_clear_preedit(window, 0);
    window->ime_preedit = copy;
    window->ime_preedit_length = length;
    window->ime_preedit_begin = cursor;
    window->ime_preedit_end = cursor;
    nx_event event = {0};
    event.type = NX_EVENT_IME_PREEDIT_CHANGED;
    nx_platform_event_queue_push(&window->event_queue, event);
}

static void nx_wayland_ime_preedit_styling(void *data,
    struct zwp_text_input_v1 *input, uint32_t index, uint32_t length,
    uint32_t style) {
    (void)data; (void)input; (void)index; (void)length; (void)style;
}

static void nx_wayland_ime_preedit_cursor(void *data,
    struct zwp_text_input_v1 *input, int32_t index) {
    nx_wayland_window *window = data;
    (void)input;
    window->ime_v1_preedit_cursor = index;
    window->ime_v1_preedit_cursor_set = 1;
}

static void nx_wayland_ime_commit(void *data, struct zwp_text_input_v1 *input,
                                   uint32_t serial, const char *text) {
    nx_wayland_window *window = data;
    (void)input;
    int valid = nx_wayland_ime_v1_serial_valid(window, serial) && text &&
                !window->ime_surrounding_withdrawn;
    int snapshot_current = window->ime_surrounding_target &&
        window->ime_surrounding_sent &&
        (uint32_t)(serial - window->ime_surrounding_serial) <=
        (uint32_t)(window->ime_serial - window->ime_surrounding_serial);
    if (window->ime_surrounding_target && !snapshot_current) valid = 0;
    if (window->ime_v1_delete_invalid) valid = 0;
    if (!valid) {
        nx_wayland_ime_v1_clear_delete(window);
        return;
    }
    size_t length = strnlen(text, 1048577u);
    if (length > 1048576u) {
        window->event_queue.error = NX_ERROR_LIMIT;
        nx_wayland_ime_v1_clear_delete(window);
        return;
    }
    if (nx_utf8_validate(text, length, NULL) != NX_OK) {
        nx_wayland_ime_v1_clear_delete(window);
        return;
    }
    if (window->ime_v1_delete_pending && window->ime_v1_delete_length) {
        if (!snapshot_current) {
            nx_wayland_ime_v1_clear_delete(window);
            return;
        }
        size_t start;
        if (window->ime_v1_delete_index < 0) {
            uint64_t before = (uint64_t)-(int64_t)window->ime_v1_delete_index;
            if (before > window->ime_surrounding_cursor) {
                nx_wayland_ime_v1_clear_delete(window);
                return;
            }
            start = window->ime_surrounding_cursor - (size_t)before;
        } else {
            size_t after = (size_t)window->ime_v1_delete_index;
            if (after > window->ime_surrounding_length -
                        window->ime_surrounding_cursor) {
                nx_wayland_ime_v1_clear_delete(window);
                return;
            }
            start = window->ime_surrounding_cursor + after;
        }
        size_t deleted = window->ime_v1_delete_length;
        if (deleted > window->ime_surrounding_length - start ||
            (start < window->ime_surrounding_length &&
             ((unsigned char)window->ime_surrounding[start] & 0xc0u) == 0x80u) ||
            (start + deleted < window->ime_surrounding_length &&
             ((unsigned char)window->ime_surrounding[start + deleted] & 0xc0u) == 0x80u)) {
            nx_wayland_ime_v1_clear_delete(window);
            return;
        }
        nx_event event = {0};
        event.type = NX_EVENT_IME_DELETE_RANGE;
        event.ime_target = window->ime_surrounding_target;
        event.ime_revision = window->ime_surrounding_revision;
        event.ime_index = window->ime_v1_delete_index;
        event.ime_length = window->ime_v1_delete_length;
        nx_platform_event_queue_push(&window->event_queue, event);
    }
    nx_wayland_ime_v1_clear_delete(window);
    if (window->event_queue.error != NX_OK) return;
    window->ime_v1_preedit_cursor_set = 0;
    nx_wayland_ime_clear_preedit(window, 1);
    nx_wayland_ime_emit_utf8(window, text, length,
        snapshot_current ? window->ime_surrounding_target : 0,
        snapshot_current ? window->ime_surrounding_revision : 0);
}

static void nx_wayland_ime_cursor_position(void *data,
    struct zwp_text_input_v1 *input, int32_t index, int32_t anchor) {
    (void)data; (void)input; (void)index; (void)anchor;
}

static void nx_wayland_ime_delete_surrounding(void *data,
    struct zwp_text_input_v1 *input, int32_t index, uint32_t length) {
    nx_wayland_window *window = data;
    (void)input;
    if (window->ime_v1_delete_pending)
        window->ime_v1_delete_invalid = 1;
    window->ime_v1_delete_index = index;
    window->ime_v1_delete_length = length;
    window->ime_v1_delete_pending = 1;
}

static void nx_wayland_ime_keysym(void *data, struct zwp_text_input_v1 *input,
    uint32_t serial, uint32_t time, uint32_t sym, uint32_t state,
    uint32_t modifiers) {
    nx_wayland_window *window = data;
    (void)input; (void)time; (void)modifiers;
    if (!nx_wayland_ime_v1_serial_valid(window, serial)) return;
    nx_key key = nx_wayland_key((xkb_keysym_t)sym);
    if (key == NX_KEY_UNKNOWN) return;
    nx_event event = {0};
    event.type = state == WL_KEYBOARD_KEY_STATE_PRESSED ?
        NX_EVENT_KEY_DOWN : NX_EVENT_KEY_UP;
    event.key = key;
    event.modifiers = nx_wayland_modifiers(window);
    nx_platform_event_queue_push(&window->event_queue, event);
}

static void nx_wayland_ime_language(void *data,
    struct zwp_text_input_v1 *input, uint32_t serial,
    const char *language) {
    (void)data; (void)input; (void)serial; (void)language;
}

static void nx_wayland_ime_direction(void *data,
    struct zwp_text_input_v1 *input, uint32_t serial,
    uint32_t direction) {
    (void)data; (void)input; (void)serial; (void)direction;
}

static const struct zwp_text_input_v1_listener nx_wayland_ime_listener = {
    .enter = nx_wayland_ime_enter,
    .leave = nx_wayland_ime_leave,
    .modifiers_map = nx_wayland_ime_modifiers_map,
    .input_panel_state = nx_wayland_ime_panel_state,
    .preedit_string = nx_wayland_ime_preedit,
    .preedit_styling = nx_wayland_ime_preedit_styling,
    .preedit_cursor = nx_wayland_ime_preedit_cursor,
    .commit_string = nx_wayland_ime_commit,
    .cursor_position = nx_wayland_ime_cursor_position,
    .delete_surrounding_text = nx_wayland_ime_delete_surrounding,
    .keysym = nx_wayland_ime_keysym,
    .language = nx_wayland_ime_language,
    .text_direction = nx_wayland_ime_direction
};

static void nx_wayland_ime_v3_enter(void *data,
    struct zwp_text_input_v3 *input, struct wl_surface *surface) {
    nx_wayland_window *window = data;
    (void)input;
    if (surface != window->surface) return;
    window->ime_v3_focused = 1;
    if (window->ime_requested) nx_wayland_ime_activate(window);
}

static void nx_wayland_ime_v3_leave(void *data,
    struct zwp_text_input_v3 *input, struct wl_surface *surface) {
    nx_wayland_window *window = data;
    (void)input;
    if (surface != window->surface) return;
    window->ime_v3_focused = 0;
    window->ime_activated = 0;
    window->ime_entered = 0;
    nx_wayland_ime_clear_pending(window);
    nx_wayland_ime_clear_preedit(window, 1);
    nx_wayland_ime_clear_surrounding(window);
    window->ime_surrounding_withdrawn = 0;
}

static void nx_wayland_ime_v3_preedit(void *data,
    struct zwp_text_input_v3 *input, const char *text,
    int32_t cursor_begin, int32_t cursor_end) {
    nx_wayland_window *window = data;
    (void)input;
    if (window->ime_pending_preedit)
        window->allocator.deallocate(window->allocator.user,
            window->ime_pending_preedit,
            window->ime_pending_preedit_length + 1u);
    window->ime_pending_preedit = NULL;
    window->ime_pending_preedit_length = 0;
    window->ime_pending_preedit_begin = 0;
    window->ime_pending_preedit_end = 0;
    if (!window->ime_requested || !window->ime_activated ||
        !window->ime_v3_focused || !text) return;
    size_t length = strnlen(text, 1048577u);
    if (length > 1048576u) {
        window->event_queue.error = NX_ERROR_LIMIT;
        return;
    }
    if (!length) return;
    if ((cursor_begin != -1 || cursor_end != -1) &&
        (cursor_begin < 0 || cursor_end < 0 ||
         (size_t)cursor_begin > length || (size_t)cursor_end > length)) {
        window->event_queue.error = NX_ERROR_PLATFORM;
        return;
    }
    if (cursor_begin >= 0 &&
        (((size_t)cursor_begin < length &&
          ((unsigned char)text[cursor_begin] & 0xc0u) == 0x80u) ||
         ((size_t)cursor_end < length &&
          ((unsigned char)text[cursor_end] & 0xc0u) == 0x80u))) {
        window->event_queue.error = NX_ERROR_PLATFORM;
        return;
    }
    if (nx_utf8_validate(text, length, NULL) != NX_OK) {
        window->event_queue.error = NX_ERROR_PLATFORM;
        return;
    }
    char *copy = window->allocator.allocate(window->allocator.user,
                                            length + 1u);
    if (!copy) {
        window->event_queue.error = NX_ERROR_MEMORY;
        return;
    }
    memcpy(copy, text, length + 1u);
    window->ime_pending_preedit = copy;
    window->ime_pending_preedit_length = length;
    window->ime_pending_preedit_begin = cursor_begin;
    window->ime_pending_preedit_end = cursor_end;
}

static void nx_wayland_ime_v3_commit(void *data,
    struct zwp_text_input_v3 *input, const char *text) {
    nx_wayland_window *window = data;
    (void)input;
    nx_wayland_ime_clear_pending_commit(window);
    if (!window->ime_requested || !window->ime_activated ||
        !window->ime_v3_focused || !text) return;
    size_t length = strnlen(text, 1048577u);
    if (length > 1048576u) {
        window->event_queue.error = NX_ERROR_LIMIT;
        return;
    }
    if (!length) return;
    char *copy = window->allocator.allocate(window->allocator.user,
                                            length + 1u);
    if (!copy) {
        window->event_queue.error = NX_ERROR_MEMORY;
        return;
    }
    memcpy(copy, text, length + 1u);
    window->ime_pending_commit = copy;
    window->ime_pending_length = length;
}

static void nx_wayland_ime_v3_delete_surrounding(void *data,
    struct zwp_text_input_v3 *input, uint32_t before, uint32_t after) {
    nx_wayland_window *window = data;
    (void)input;
    window->ime_pending_delete_before = before;
    window->ime_pending_delete_after = after;
}

static void nx_wayland_ime_v3_done(void *data,
    struct zwp_text_input_v3 *input, uint32_t serial) {
    nx_wayland_window *window = data;
    (void)input;
    int valid = nx_wayland_ime_serial_valid(window, serial);
    if (window->ime_surrounding_withdrawn) valid = 0;
    int snapshot_current = window->ime_surrounding_target &&
        window->ime_surrounding_sent &&
        (uint32_t)(serial - window->ime_surrounding_serial) <=
        (uint32_t)(window->ime_serial - window->ime_surrounding_serial);
    if (window->ime_surrounding_target && !snapshot_current) valid = 0;
    int has_delete = window->ime_pending_delete_before ||
                     window->ime_pending_delete_after;
    int delete_valid = !has_delete;
    if (valid && has_delete && snapshot_current) {
        size_t selection_start = window->ime_surrounding_cursor <
            window->ime_surrounding_anchor ?
            window->ime_surrounding_cursor : window->ime_surrounding_anchor;
        size_t selection_end = window->ime_surrounding_cursor >
            window->ime_surrounding_anchor ?
            window->ime_surrounding_cursor : window->ime_surrounding_anchor;
        if (window->ime_pending_delete_before <= selection_start &&
            window->ime_pending_delete_after <=
                window->ime_surrounding_length - selection_end) {
            size_t start = selection_start - window->ime_pending_delete_before;
            size_t end = selection_end + window->ime_pending_delete_after;
            delete_valid =
                (start == window->ime_surrounding_length ||
                 ((unsigned char)window->ime_surrounding[start] & 0xc0u) != 0x80u) &&
                (end == window->ime_surrounding_length ||
                 ((unsigned char)window->ime_surrounding[end] & 0xc0u) != 0x80u);
        }
        if (delete_valid) {
            nx_event event = {0};
            event.type = NX_EVENT_IME_DELETE_SURROUNDING;
            event.ime_target = window->ime_surrounding_target;
            event.ime_revision = window->ime_surrounding_revision;
            event.ime_before = window->ime_pending_delete_before;
            event.ime_after = window->ime_pending_delete_after;
            nx_platform_event_queue_push(&window->event_queue, event);
        }
    }
    if (valid && delete_valid && window->ime_pending_commit &&
        window->event_queue.error == NX_OK)
        nx_wayland_ime_emit_utf8(window, window->ime_pending_commit,
            window->ime_pending_length,
            snapshot_current ? window->ime_surrounding_target : 0,
            snapshot_current ? window->ime_surrounding_revision : 0);
    if (valid && delete_valid) {
        int changed = window->ime_preedit_length !=
                      window->ime_pending_preedit_length ||
            window->ime_preedit_begin != window->ime_pending_preedit_begin ||
            window->ime_preedit_end != window->ime_pending_preedit_end ||
            (window->ime_preedit_length &&
             memcmp(window->ime_preedit, window->ime_pending_preedit,
                    window->ime_preedit_length) != 0);
        nx_wayland_ime_clear_preedit(window, 0);
        window->ime_preedit = window->ime_pending_preedit;
        window->ime_preedit_length = window->ime_pending_preedit_length;
        window->ime_preedit_begin = window->ime_pending_preedit_begin;
        window->ime_preedit_end = window->ime_pending_preedit_end;
        window->ime_pending_preedit = NULL;
        window->ime_pending_preedit_length = 0;
        if (changed) {
            nx_event event = {0};
            event.type = NX_EVENT_IME_PREEDIT_CHANGED;
            nx_platform_event_queue_push(&window->event_queue, event);
        }
    }
    nx_wayland_ime_clear_pending(window);
}

#ifdef ZWP_TEXT_INPUT_V3_ACTION_SINCE_VERSION
static void nx_wayland_ime_v3_action(void *data,
    struct zwp_text_input_v3 *input, uint32_t action, uint32_t serial) {
    (void)data; (void)input; (void)action; (void)serial;
}
#endif

#ifdef ZWP_TEXT_INPUT_V3_LANGUAGE_SINCE_VERSION
static void nx_wayland_ime_v3_language(void *data,
    struct zwp_text_input_v3 *input, const char *language) {
    (void)data; (void)input; (void)language;
}
#endif

#ifdef ZWP_TEXT_INPUT_V3_PREEDIT_HINT_SINCE_VERSION
static void nx_wayland_ime_v3_preedit_hint(void *data,
    struct zwp_text_input_v3 *input, uint32_t start,
    uint32_t end, uint32_t hint) {
    (void)data; (void)input; (void)start; (void)end; (void)hint;
}
#endif

static const struct zwp_text_input_v3_listener nx_wayland_ime_v3_listener = {
    .enter = nx_wayland_ime_v3_enter,
    .leave = nx_wayland_ime_v3_leave,
    .preedit_string = nx_wayland_ime_v3_preedit,
    .commit_string = nx_wayland_ime_v3_commit,
    .delete_surrounding_text = nx_wayland_ime_v3_delete_surrounding,
    .done = nx_wayland_ime_v3_done,
#ifdef ZWP_TEXT_INPUT_V3_ACTION_SINCE_VERSION
    .action = nx_wayland_ime_v3_action,
#endif
#ifdef ZWP_TEXT_INPUT_V3_LANGUAGE_SINCE_VERSION
    .language = nx_wayland_ime_v3_language,
#endif
#ifdef ZWP_TEXT_INPUT_V3_PREEDIT_HINT_SINCE_VERSION
    .preedit_hint = nx_wayland_ime_v3_preedit_hint,
#endif
};

static void nx_wayland_ime_v3_enable(nx_wayland_window *window) {
    nx_wayland_ime_clear_pending(window);
    nx_wayland_ime_clear_preedit(window, 1);
    window->ime_activated = 1;
    window->ime_entered = 1;
    window->ime_activation_serial = ++window->ime_serial;
    window->ime_surrounding_withdrawn = 0;
    zwp_text_input_v3_enable(window->text_input_v3);
    zwp_text_input_v3_set_content_type(window->text_input_v3,
        window->ime_password ? ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA :
                               ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE,
        window->ime_password ? ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD :
                               ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL);
    if (window->ime_caret_set) {
        nx_rect caret = window->ime_caret;
        zwp_text_input_v3_set_cursor_rectangle(window->text_input_v3,
            (int32_t)lround((double)caret.x),
            (int32_t)lround((double)caret.y),
            (int32_t)fmax(1.0, round((double)caret.width)),
            (int32_t)fmax(1.0, round((double)caret.height)));
    }
    window->ime_surrounding_sent = window->ime_surrounding_target != 0;
    if (window->ime_surrounding_sent) {
        zwp_text_input_v3_set_surrounding_text(window->text_input_v3,
            window->ime_surrounding,
            (int32_t)window->ime_surrounding_cursor,
            (int32_t)window->ime_surrounding_anchor);
        window->ime_surrounding_serial = window->ime_serial;
    }
    zwp_text_input_v3_commit(window->text_input_v3);
}

static void nx_wayland_ime_activate(nx_wayland_window *window) {
    if (window->ime_activated) return;
    if (window->text_input_v3) {
        if (!window->ime_v3_focused) return;
        nx_wayland_ime_v3_enable(window);
        return;
    }
    if (!window->text_input || !window->seat || !window->surface ||
        !window->keyboard_focused) return;
    window->ime_activated = 1;
    window->ime_entered = 0;
    window->ime_activation_serial = ++window->ime_serial;
    window->ime_v1_preedit_cursor_set = 0;
    nx_wayland_ime_v1_clear_delete(window);
    nx_wayland_ime_clear_preedit(window, 1);
    zwp_text_input_v1_activate(window->text_input, window->seat,
                                window->surface);
    zwp_text_input_v1_set_content_type(window->text_input,
        window->ime_password ? ZWP_TEXT_INPUT_V1_CONTENT_HINT_SENSITIVE_DATA :
                               ZWP_TEXT_INPUT_V1_CONTENT_HINT_NONE,
        window->ime_password ? ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PASSWORD :
                               ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL);
    if (window->ime_caret_set) {
        nx_rect caret = window->ime_caret;
        zwp_text_input_v1_set_cursor_rectangle(window->text_input,
            (int32_t)lround((double)caret.x),
            (int32_t)lround((double)caret.y),
            (int32_t)fmax(1.0, round((double)caret.width)),
            (int32_t)fmax(1.0, round((double)caret.height)));
    }
    window->ime_surrounding_sent = window->ime_surrounding_target != 0;
    if (window->ime_surrounding_sent) {
        zwp_text_input_v1_set_surrounding_text(window->text_input,
            window->ime_surrounding,
            (uint32_t)window->ime_surrounding_cursor,
            (uint32_t)window->ime_surrounding_anchor);
        window->ime_surrounding_serial = window->ime_serial;
    }
    zwp_text_input_v1_commit_state(window->text_input, window->ime_serial);
}

static void nx_wayland_ime_deactivate(nx_wayland_window *window) {
    if (!window->ime_activated) {
        if (window->text_input_v3 || window->text_input)
            nx_wayland_ime_clear_surrounding(window);
        nx_wayland_ime_v1_clear_delete(window);
        return;
    }
    if (window->text_input_v3) {
        window->ime_activated = 0;
        window->ime_entered = 0;
        ++window->ime_serial;
        nx_wayland_ime_clear_pending(window);
        nx_wayland_ime_clear_preedit(window, 1);
        nx_wayland_ime_clear_surrounding(window);
        if (window->ime_v3_focused) {
            zwp_text_input_v3_disable(window->text_input_v3);
            zwp_text_input_v3_commit(window->text_input_v3);
        }
        return;
    }
    if (!window->text_input || !window->seat) return;
    window->ime_activated = 0;
    window->ime_entered = 0;
    window->ime_v1_preedit_cursor_set = 0;
    nx_wayland_ime_v1_clear_delete(window);
    nx_wayland_ime_clear_preedit(window, 1);
    nx_wayland_ime_clear_surrounding(window);
    ++window->ime_serial;
    zwp_text_input_v1_reset(window->text_input);
    zwp_text_input_v1_deactivate(window->text_input, window->seat);
}

static void nx_wayland_keyboard_keymap(void *data,
                                       struct wl_keyboard *keyboard,
                                       uint32_t format, int fd,
                                       uint32_t size) {
    nx_wayland_window *window = data;
    (void)keyboard;
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 ||
        size == 0 || size > 8u * 1024u * 1024u) {
        close(fd);
        window->error = NX_ERROR_PLATFORM;
        return;
    }
    void *text = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (text == MAP_FAILED) {
        window->error = NX_ERROR_PLATFORM;
        return;
    }
    if (((const char *)text)[size - 1] != '\0') {
        munmap(text, size);
        window->error = NX_ERROR_PLATFORM;
        return;
    }
    struct xkb_keymap *keymap = xkb_keymap_new_from_string(
        window->xkb_context, text, XKB_KEYMAP_FORMAT_TEXT_V1,
        XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(text, size);
    if (!keymap) { window->error = NX_ERROR_PLATFORM; return; }
    struct xkb_state *state = xkb_state_new(keymap);
    if (!state) {
        xkb_keymap_unref(keymap);
        window->error = NX_ERROR_MEMORY;
        return;
    }
    if (window->key_state) xkb_state_unref(window->key_state);
    if (window->keymap) xkb_keymap_unref(window->keymap);
    window->keymap = keymap;
    window->key_state = state;
    memset(window->keys_down, 0, sizeof(window->keys_down));
    nx_wayland_repeat_cancel(&window->repeat);
}

static void nx_wayland_keyboard_enter(void *data,
                                      struct wl_keyboard *keyboard,
                                      uint32_t serial,
                                      struct wl_surface *surface,
                                      struct wl_array *keys) {
    nx_wayland_window *window = data;
    (void)keyboard;
    (void)serial;
    if (surface != window->surface) return;
    window->keyboard_focused = 1;
    if (window->ime_requested) nx_wayland_ime_activate(window);
    memset(window->keys_down, 0, sizeof(window->keys_down));
    nx_wayland_repeat_cancel(&window->repeat);
    uint32_t *key;
    wl_array_for_each(key, keys) {
        if (*key < sizeof(window->keys_down)) window->keys_down[*key] = 1;
    }
    nx_event event = {0};
    event.type = NX_EVENT_FOCUS;
    event.focused = 1;
    nx_platform_event_queue_push(&window->event_queue, event);
}

static void nx_wayland_keyboard_leave(void *data,
                                      struct wl_keyboard *keyboard,
                                      uint32_t serial,
                                      struct wl_surface *surface) {
    nx_wayland_window *window = data;
    (void)keyboard;
    (void)serial;
    if (surface != window->surface) return;
    window->keyboard_focused = 0;
    nx_wayland_ime_deactivate(window);
    window->selection_serial = 0;
    memset(window->keys_down, 0, sizeof(window->keys_down));
    nx_wayland_repeat_cancel(&window->repeat);
    nx_event event = {0};
    event.type = NX_EVENT_FOCUS;
    event.focused = 0;
    nx_platform_event_queue_push(&window->event_queue, event);
}

static void nx_wayland_emit_key(nx_wayland_window *window, uint32_t key,
                                int down, int repeat) {
    xkb_keycode_t code = key + 8u;
    nx_key logical = nx_wayland_key(
        xkb_state_key_get_one_sym(window->key_state, code));
    uint32_t modifiers = nx_wayland_modifiers(window);
    if (logical != NX_KEY_UNKNOWN) {
        nx_event event = {0};
        event.type = down ? NX_EVENT_KEY_DOWN : NX_EVENT_KEY_UP;
        event.key = logical;
        event.repeat = repeat;
        event.modifiers = modifiers;
        nx_platform_event_queue_push(&window->event_queue, event);
    }
    if (down && !window->ime_entered &&
        !(modifiers & (NX_MOD_CONTROL | NX_MOD_ALT | NX_MOD_SUPER))) {
        uint32_t scalar = xkb_state_key_get_utf32(window->key_state, code);
        if (scalar >= 0x20u && scalar != 0x7fu &&
            scalar <= 0x10ffffu &&
            (scalar < 0xd800u || scalar > 0xdfffu)) {
            nx_event event = {0};
            event.type = NX_EVENT_TEXT;
            event.scalar = scalar;
            nx_platform_event_queue_push(&window->event_queue, event);
        }
    }
}

static void nx_wayland_keyboard_key(void *data, struct wl_keyboard *keyboard,
                                    uint32_t serial, uint32_t time,
                                    uint32_t key, uint32_t state) {
    nx_wayland_window *window = data;
    (void)keyboard;
    (void)time;
    if (!window->keyboard_focused || !window->key_state ||
        key > UINT32_MAX - 8u) return;
    int down = state == WL_KEYBOARD_KEY_STATE_PRESSED;
    if (down) window->selection_serial = serial;
    int repeat = down && key < sizeof(window->keys_down) &&
                 window->keys_down[key];
    if (key < sizeof(window->keys_down)) window->keys_down[key] = down;
    nx_wayland_emit_key(window, key, down, repeat);
    if (down && !repeat && window->keymap &&
        xkb_keymap_key_repeats(window->keymap, key + 8u))
        nx_wayland_repeat_press(&window->repeat, key, 1,
                                 nx_wayland_now_ns());
    else if (!down)
        nx_wayland_repeat_release(&window->repeat, key);
}

static void nx_wayland_emit_due_repeats(nx_wayland_window *window) {
    if (!window->keyboard_focused || !window->key_state) {
        nx_wayland_repeat_cancel(&window->repeat);
        return;
    }
    unsigned count = nx_wayland_repeat_take_due(&window->repeat,
                                                 nx_wayland_now_ns(), 8);
    for (unsigned i = 0; i < count; ++i)
        nx_wayland_emit_key(window, window->repeat.key, 1, 1);
}

static void nx_wayland_keyboard_modifiers(void *data,
                                          struct wl_keyboard *keyboard,
                                          uint32_t serial, uint32_t depressed,
                                          uint32_t latched, uint32_t locked,
                                          uint32_t group) {
    nx_wayland_window *window = data;
    (void)keyboard;
    (void)serial;
    if (window->key_state)
        xkb_state_update_mask(window->key_state, depressed, latched,
                              locked, 0, 0, group);
}

static void nx_wayland_keyboard_repeat_info(void *data,
                                            struct wl_keyboard *keyboard,
                                            int32_t rate, int32_t delay) {
    nx_wayland_window *window = data;
    (void)keyboard;
    if (rate < 0 || delay < 0) {
        window->error = NX_ERROR_PLATFORM;
        return;
    }
    nx_wayland_repeat_info(&window->repeat, rate, delay,
                            nx_wayland_now_ns());
}

static const struct wl_keyboard_listener nx_wayland_keyboard_listener = {
    .keymap = nx_wayland_keyboard_keymap,
    .enter = nx_wayland_keyboard_enter,
    .leave = nx_wayland_keyboard_leave,
    .key = nx_wayland_keyboard_key,
    .modifiers = nx_wayland_keyboard_modifiers,
    .repeat_info = nx_wayland_keyboard_repeat_info
};

static void nx_wayland_seat_capabilities(void *data, struct wl_seat *seat,
                                         uint32_t capabilities) {
    nx_wayland_window *window = data;
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !window->pointer) {
        window->pointer = wl_seat_get_pointer(seat);
        if (!window->pointer || wl_pointer_add_listener(window->pointer,
                &nx_wayland_pointer_listener, window) != 0)
            window->error = NX_ERROR_PLATFORM;
    } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) &&
               window->pointer) {
        wl_pointer_release(window->pointer);
        window->pointer = NULL;
        window->pointer_inside = 0;
        window->selection_serial = 0;
    }
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !window->keyboard) {
        window->keyboard = wl_seat_get_keyboard(seat);
        if (!window->keyboard || wl_keyboard_add_listener(window->keyboard,
                &nx_wayland_keyboard_listener, window) != 0)
            window->error = NX_ERROR_PLATFORM;
    } else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) &&
               window->keyboard) {
        wl_keyboard_release(window->keyboard);
        window->keyboard = NULL;
        window->keyboard_focused = 0;
        nx_wayland_ime_deactivate(window);
        window->selection_serial = 0;
        memset(window->keys_down, 0, sizeof(window->keys_down));
        nx_wayland_repeat_cancel(&window->repeat);
    }
}

static void nx_wayland_seat_name(void *data, struct wl_seat *seat,
                                  const char *name) {
    (void)data;
    (void)seat;
    (void)name;
}

static const struct wl_seat_listener nx_wayland_seat_listener = {
    nx_wayland_seat_capabilities, nx_wayland_seat_name
};

static void nx_wayland_ping(void *data, struct xdg_wm_base *wm_base,
                            uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener nx_wayland_wm_listener = {
    nx_wayland_ping
};

static void nx_wayland_buffer_release(void *data, struct wl_buffer *native) {
    nx_wayland_buffer *buffer = (nx_wayland_buffer *)data;
    nx_wayland_window *window = buffer->window;
    nx_wayland_buffer **slot = &window->buffers;
    while (*slot && *slot != buffer) slot = &(*slot)->next;
    if (*slot) *slot = buffer->next;
    wl_buffer_destroy(native);
    munmap(buffer->pixels, buffer->size);
    window->allocator.deallocate(window->allocator.user, buffer,
                                 sizeof(*buffer));
}

static const struct wl_buffer_listener nx_wayland_buffer_listener = {
    nx_wayland_buffer_release
};

static nx_result nx_wayland_create_buffer(nx_wayland_window *window,
                                           int width, int height,
                                           uint32_t format,
                                           nx_wayland_buffer **out_buffer) {
    *out_buffer = NULL;
    if (width <= 0 || height <= 0 || width > INT_MAX / 4 / height)
        return NX_ERROR_LIMIT;
    int stride = width * 4;
    int size = stride * height;
    nx_wayland_buffer *buffer = window->allocator.allocate(
        window->allocator.user, sizeof(*buffer));
    if (!buffer) return NX_ERROR_MEMORY;
    memset(buffer, 0, sizeof(*buffer));
    buffer->window = window;
    buffer->size = (size_t)size;
    char name[] = "/tmp/nexaui-wayland-XXXXXX";
    int fd = mkstemp(name);
    if (fd < 0) goto platform_fail;
    unlink(name);
    if (ftruncate(fd, (off_t)size) != 0) goto fd_fail;
    buffer->pixels = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, 0);
    if (buffer->pixels == MAP_FAILED) {
        buffer->pixels = NULL;
        goto fd_fail;
    }
    struct wl_shm_pool *pool = wl_shm_create_pool(window->shm, fd, size);
    close(fd);
    if (!pool) goto platform_fail;
    buffer->native = wl_shm_pool_create_buffer(pool, 0, width,
                                                height, stride, format);
    wl_shm_pool_destroy(pool);
    if (!buffer->native) goto platform_fail;
    if (wl_buffer_add_listener(buffer->native, &nx_wayland_buffer_listener,
                               buffer) != 0) goto platform_fail;
    buffer->next = window->buffers;
    window->buffers = buffer;
    *out_buffer = buffer;
    return NX_OK;
fd_fail:
    close(fd);
platform_fail:
    if (buffer->native) wl_buffer_destroy(buffer->native);
    if (buffer->pixels) munmap(buffer->pixels, buffer->size);
    window->allocator.deallocate(window->allocator.user, buffer,
                                 sizeof(*buffer));
    return NX_ERROR_PLATFORM;
}

static nx_result nx_wayland_attach_background(nx_wayland_window *window) {
    if (window->external_renderer) {
        if (wl_surface_get_version(window->surface) >= 3)
            wl_surface_set_buffer_scale(window->surface, window->scale);
        return NX_OK;
    }
    nx_wayland_buffer *buffer = NULL;
    if (window->scale > INT_MAX / window->width ||
        window->scale > INT_MAX / window->height) return NX_ERROR_LIMIT;
    int width = window->width * window->scale;
    int height = window->height * window->scale;
    nx_result result = nx_wayland_create_buffer(window, width,
                        height, WL_SHM_FORMAT_XRGB8888, &buffer);
    if (result != NX_OK) return result;
    uint32_t *pixels = (uint32_t *)buffer->pixels;
    for (size_t i = 0; i < buffer->size / 4u; ++i)
        pixels[i] = 0xfff6f7f9u;
    if (wl_surface_get_version(window->surface) >= 3)
        wl_surface_set_buffer_scale(window->surface, window->scale);
    wl_surface_attach(window->surface, buffer->native, 0, 0);
    wl_surface_damage(window->surface, 0, 0, window->width, window->height);
    wl_surface_commit(window->surface);
    window->buffer_attached = 1;
    window->background_width = width;
    window->background_height = height;
    return NX_OK;
}

static nx_result nx_wayland_apply_cursor(nx_wayland_window *window) {
    if (!window->pointer || !window->pointer_inside) return NX_OK;
    if (!window->cursor_surface)
        window->cursor_surface = wl_compositor_create_surface(
            window->compositor);
    if (!window->cursor_surface) return NX_ERROR_PLATFORM;
    nx_wayland_buffer *buffer = NULL;
    if (window->scale > INT_MAX / NX_WAYLAND_CURSOR_SIZE)
        return NX_ERROR_LIMIT;
    int dimension = NX_WAYLAND_CURSOR_SIZE * window->scale;
    nx_result result = nx_wayland_create_buffer(window,
        dimension, dimension,
        WL_SHM_FORMAT_ARGB8888, &buffer);
    if (result != NX_OK) return result;
    int hotspot_x = 0, hotspot_y = 0;
    uint32_t pixels[NX_WAYLAND_CURSOR_SIZE * NX_WAYLAND_CURSOR_SIZE];
    nx_wayland_cursor_render(window->cursor_shape, pixels,
                             &hotspot_x, &hotspot_y);
    uint32_t *target = buffer->pixels;
    for (int y = 0; y < dimension; ++y)
        for (int x = 0; x < dimension; ++x)
            target[(size_t)y * (size_t)dimension + (size_t)x] =
                pixels[(y / window->scale) * NX_WAYLAND_CURSOR_SIZE +
                       x / window->scale];
    wl_pointer_set_cursor(window->pointer, window->pointer_serial,
                           window->cursor_surface, hotspot_x, hotspot_y);
    if (wl_surface_get_version(window->cursor_surface) >= 3)
        wl_surface_set_buffer_scale(window->cursor_surface, window->scale);
    wl_surface_attach(window->cursor_surface, buffer->native, 0, 0);
    wl_surface_damage(window->cursor_surface, 0, 0,
                       NX_WAYLAND_CURSOR_SIZE, NX_WAYLAND_CURSOR_SIZE);
    wl_surface_commit(window->cursor_surface);
    window->cursor_width = window->cursor_height = dimension;
    return NX_OK;
}

static void nx_wayland_toplevel_configure(void *data,
                                          struct xdg_toplevel *toplevel,
                                          int32_t width, int32_t height,
                                          struct wl_array *states) {
    nx_wayland_window *window = (nx_wayland_window *)data;
    (void)toplevel;
    (void)states;
    window->pending_width = width;
    window->pending_height = height;
}

static void nx_wayland_toplevel_close(void *data,
                                      struct xdg_toplevel *toplevel) {
    nx_wayland_window *window = (nx_wayland_window *)data;
    (void)toplevel;
    window->should_close = 1;
}

static const struct xdg_toplevel_listener nx_wayland_toplevel_listener = {
    nx_wayland_toplevel_configure, nx_wayland_toplevel_close, NULL, NULL
};

static void nx_wayland_surface_configure(void *data,
                                         struct xdg_surface *xdg_surface,
                                         uint32_t serial) {
    nx_wayland_window *window = (nx_wayland_window *)data;
    xdg_surface_ack_configure(xdg_surface, serial);
    int width = window->pending_width > 0 ? window->pending_width :
                window->width;
    int height = window->pending_height > 0 ? window->pending_height :
                 window->height;
    int resized = width != window->width || height != window->height;
    if (resized) {
        window->width = width;
        window->height = height;
        xdg_surface_set_window_geometry(window->xdg_surface, 0, 0,
                                         width, height);
        nx_event event = {0};
        event.type = NX_EVENT_RESIZE;
        event.width = (float)width;
        event.height = (float)height;
        event.scale = (float)window->scale;
        nx_platform_event_queue_push(&window->event_queue, event);
    }
    window->configured = 1;
    if (!window->hidden && !window->external_renderer && window->error == NX_OK) {
        if (resized || !window->buffer_attached)
            window->error = nx_wayland_attach_background(window);
        else wl_surface_commit(window->surface);
    }
}

static const struct xdg_surface_listener nx_wayland_surface_listener = {
    nx_wayland_surface_configure
};

nx_result nx_wayland_window_create(const nx_window_desc *desc,
                                   nx_wayland_window **out_window) {
    if (!out_window) return NX_ERROR_ARGUMENT;
    *out_window = NULL;
    if (!desc || !isfinite(desc->width) || !isfinite(desc->height) ||
        desc->width <= 0 || desc->height <= 0 ||
        (desc->flags & ~NX_WINDOW_HIDDEN)) return NX_ERROR_ARGUMENT;
    double width = ceil((double)desc->width);
    double height = ceil((double)desc->height);
    if (width > UINT16_MAX || height > UINT16_MAX) return NX_ERROR_LIMIT;
    const char *title = desc->title_utf8 ? desc->title_utf8 : "NexaUI";
    if (nx_utf8_validate(title, strlen(title), NULL) != NX_OK)
        return NX_ERROR_ARGUMENT;
    nx_allocator allocator = desc->allocator;
    if ((allocator.allocate == NULL) != (allocator.deallocate == NULL))
        return NX_ERROR_ARGUMENT;
    if (!allocator.allocate)
        allocator = (nx_allocator){NULL, nx_wayland_allocate,
                                   nx_wayland_deallocate};
    nx_wayland_window *window = allocator.allocate(allocator.user,
                                                   sizeof(*window));
    if (!window) return NX_ERROR_MEMORY;
    memset(window, 0, sizeof(*window));
    window->allocator = allocator;
    window->owner_thread = pthread_self();
    window->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!window->xkb_context) {
        allocator.deallocate(allocator.user, window, sizeof(*window));
        return NX_ERROR_MEMORY;
    }
    window->width = (int)width;
    window->height = (int)height;
    window->scale = 1;
    window->hidden = (desc->flags & NX_WINDOW_HIDDEN) != 0;
    nx_result result = nx_platform_event_queue_init(&window->event_queue,
                                                     allocator, 64u);
    if (result != NX_OK) goto fail;
    nx_platform_drop_queue_init(&window->drop_queue, allocator);
    window->display = wl_display_connect(NULL);
    if (!window->display) { result = NX_ERROR_PLATFORM; goto fail; }
    window->registry = wl_display_get_registry(window->display);
    if (!window->registry || wl_registry_add_listener(window->registry,
            &nx_wayland_registry_listener, window) != 0 ||
        wl_display_roundtrip(window->display) < 0 ||
        !window->compositor || !window->wm_base || !window->shm ||
        window->error != NX_OK) {
        result = window->error != NX_OK ? window->error : NX_ERROR_PLATFORM;
        goto fail;
    }
    if (xdg_wm_base_add_listener(window->wm_base,
            &nx_wayland_wm_listener, window) != 0 ||
        (window->seat && wl_seat_add_listener(window->seat,
            &nx_wayland_seat_listener, window) != 0)) {
        result = NX_ERROR_PLATFORM;
        goto fail;
    }
    if (window->data_manager && window->seat) {
        window->data_device = wl_data_device_manager_get_data_device(
            window->data_manager, window->seat);
        if (!window->data_device || wl_data_device_add_listener(
                window->data_device, &nx_wayland_device_listener,
                window) != 0) {
            result = NX_ERROR_PLATFORM;
            goto fail;
        }
    }
    window->surface = wl_compositor_create_surface(window->compositor);
    if (window->surface && wl_surface_add_listener(window->surface,
            &nx_wayland_native_surface_listener, window) != 0) {
        result = NX_ERROR_PLATFORM;
        goto fail;
    }
    if (window->text_input_manager_v3 && window->seat) {
        window->text_input_v3 = zwp_text_input_manager_v3_get_text_input(
            window->text_input_manager_v3, window->seat);
        if (!window->text_input_v3 || zwp_text_input_v3_add_listener(
                window->text_input_v3, &nx_wayland_ime_v3_listener,
                window) != 0) {
            result = NX_ERROR_PLATFORM;
            goto fail;
        }
    } else if (window->text_input_manager && window->seat) {
        window->text_input = zwp_text_input_manager_v1_create_text_input(
            window->text_input_manager);
        if (!window->text_input || zwp_text_input_v1_add_listener(
                window->text_input, &nx_wayland_ime_listener, window) != 0) {
            result = NX_ERROR_PLATFORM;
            goto fail;
        }
    }
    if (window->surface)
        window->xdg_surface = xdg_wm_base_get_xdg_surface(window->wm_base,
                                                            window->surface);
    if (window->xdg_surface)
        window->toplevel = xdg_surface_get_toplevel(window->xdg_surface);
    if (!window->surface || !window->xdg_surface || !window->toplevel ||
        xdg_surface_add_listener(window->xdg_surface,
            &nx_wayland_surface_listener, window) != 0 ||
        xdg_toplevel_add_listener(window->toplevel,
            &nx_wayland_toplevel_listener, window) != 0) {
        result = NX_ERROR_PLATFORM;
        goto fail;
    }
    xdg_toplevel_set_title(window->toplevel, title);
    xdg_toplevel_set_app_id(window->toplevel, "nexaui");
    xdg_surface_set_window_geometry(window->xdg_surface, 0, 0,
                                     window->width, window->height);
    wl_surface_commit(window->surface);
    if (wl_display_roundtrip(window->display) < 0 || !window->configured ||
        window->error != NX_OK) {
        result = window->error != NX_OK ? window->error : NX_ERROR_PLATFORM;
        goto fail;
    }
    *out_window = window;
    window->thread_next = nx_wayland_thread_windows;
    nx_wayland_thread_windows = window;
    return NX_OK;
fail:
    nx_wayland_window_destroy(window);
    return result;
}

nx_result nx_wayland_window_poll(nx_wayland_window *window,
                                  uint32_t timeout_ms) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    if (window->error != NX_OK) return window->error;
    if (wl_display_dispatch_pending(window->display) < 0)
        return NX_ERROR_PLATFORM;
    if (window->error != NX_OK) return window->error;
    nx_result drop_result = nx_wayland_process_drop(window);
    if (drop_result != NX_OK) return window->error = drop_result;
    nx_wayland_progress_writes(window);
    while (wl_display_prepare_read(window->display) != 0) {
        if (wl_display_dispatch_pending(window->display) < 0)
            return NX_ERROR_PLATFORM;
    }
    int needs_write = 0;
    if (wl_display_flush(window->display) < 0) {
        if (errno != EAGAIN) {
            wl_display_cancel_read(window->display);
            return NX_ERROR_PLATFORM;
        }
        needs_write = 1;
    }
    struct pollfd fds[5] = {{wl_display_get_fd(window->display),
                        (short)(POLLIN | (needs_write ? POLLOUT : 0)), 0}};
    nfds_t fd_count = 1;
    for (nx_wayland_clipboard_write *transfer = window->writes;
         transfer && fd_count < 5; transfer = transfer->next)
        fds[fd_count++] = (struct pollfd){transfer->fd, POLLOUT, 0};
    int timeout = timeout_ms > INT_MAX ? INT_MAX : (int)timeout_ms;
    timeout = nx_wayland_repeat_timeout(&window->repeat,
                                        nx_wayland_now_ns(), timeout);
    int ready = poll(fds, fd_count, timeout);
    if (ready > 0 && (fds[0].revents & POLLOUT) &&
        wl_display_flush(window->display) < 0 && errno != EAGAIN) {
        wl_display_cancel_read(window->display);
        return NX_ERROR_PLATFORM;
    }
    if (ready > 0 && (fds[0].revents & POLLIN)) {
        if (wl_display_read_events(window->display) < 0)
            return NX_ERROR_PLATFORM;
    } else {
        wl_display_cancel_read(window->display);
        if (ready < 0 && errno != EINTR) return NX_ERROR_PLATFORM;
        if (ready > 0 && (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)))
            return NX_ERROR_PLATFORM;
    }
    if (ready > 0) nx_wayland_progress_writes(window);
    if (wl_display_dispatch_pending(window->display) < 0)
        return NX_ERROR_PLATFORM;
    nx_wayland_progress_writes(window);
    if (window->error != NX_OK) return window->error;
    drop_result = nx_wayland_process_drop(window);
    if (drop_result != NX_OK) return window->error = drop_result;
    nx_wayland_emit_due_repeats(window);
    return window->event_queue.error;
}

nx_result nx_wayland_window_get_info(nx_wayland_window *window,
                                      nx_window_info *out_info) {
    if (!window || !out_info) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    *out_info = (nx_window_info){(float)window->width,
                                 (float)window->height, (float)window->scale,
                                 window->should_close};
    return NX_OK;
}

nx_result nx_wayland_window_set_external_renderer(nx_wayland_window *window,
                                                  int enabled) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    if (enabled && window->error != NX_OK) return window->error;
    window->external_renderer = enabled != 0;
    if (!enabled && window->configured && !window->hidden && window->error == NX_OK)
        window->error = nx_wayland_attach_background(window);
    return window->error;
}

nx_result nx_wayland_window_set_cursor(nx_wayland_window *window,
                                        nx_cursor_shape shape) {
    if (!window || shape < NX_CURSOR_DEFAULT || shape > NX_CURSOR_BUSY)
        return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    if (window->error != NX_OK) return window->error;
    if (shape == window->cursor_shape) return NX_OK;
    nx_cursor_shape previous = window->cursor_shape;
    window->cursor_shape = shape;
    nx_result result = nx_wayland_apply_cursor(window);
    if (result != NX_OK) window->cursor_shape = previous;
    return result;
}

nx_result nx_wayland_window_get_reduced_motion(nx_wayland_window *window,
                                                 int *out_enabled) {
    if (!out_enabled) return NX_ERROR_ARGUMENT;
    *out_enabled = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_wayland_window_set_ime_rect(nx_wayland_window *window,
                                          nx_rect caret) {
    if (!window || !isfinite(caret.x) || !isfinite(caret.y) ||
        !isfinite(caret.width) || !isfinite(caret.height) ||
        caret.width <= 0 || caret.height <= 0) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    if (!window->text_input && !window->text_input_v3)
        return NX_ERROR_UNSUPPORTED;
    double x = round((double)caret.x);
    double y = round((double)caret.y);
    double width = round((double)caret.width);
    double height = round((double)caret.height);
    if (x < INT32_MIN || x > INT32_MAX || y < INT32_MIN || y > INT32_MAX ||
        width > INT32_MAX || height > INT32_MAX) return NX_ERROR_LIMIT;
    window->ime_caret = caret;
    window->ime_caret_set = 1;
    if (window->ime_activated) {
        if (window->text_input_v3) {
            zwp_text_input_v3_set_cursor_rectangle(window->text_input_v3,
                (int32_t)x, (int32_t)y, (int32_t)fmax(1.0, width),
                (int32_t)fmax(1.0, height));
            zwp_text_input_v3_commit(window->text_input_v3);
            ++window->ime_serial;
        } else {
            zwp_text_input_v1_set_cursor_rectangle(window->text_input,
                (int32_t)x, (int32_t)y, (int32_t)fmax(1.0, width),
                (int32_t)fmax(1.0, height));
            zwp_text_input_v1_commit_state(window->text_input,
                                           ++window->ime_serial);
        }
    }
    return NX_OK;
}

nx_result nx_wayland_window_set_ime_enabled(nx_wayland_window *window,
                                             int enabled) {
    if (!window || (enabled != 0 && enabled != 1)) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    if (!window->text_input && !window->text_input_v3)
        return NX_ERROR_UNSUPPORTED;
    window->ime_requested = enabled;
    if (enabled) nx_wayland_ime_activate(window);
    else nx_wayland_ime_deactivate(window);
    return NX_OK;
}

nx_result nx_wayland_window_set_ime_purpose(nx_wayland_window *window,
                                            nx_ime_purpose purpose) {
    if (!window || (purpose != NX_IME_PURPOSE_NORMAL &&
                    purpose != NX_IME_PURPOSE_PASSWORD))
        return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    if (!window->text_input && !window->text_input_v3)
        return NX_ERROR_UNSUPPORTED;
    int password = purpose == NX_IME_PURPOSE_PASSWORD;
    if (window->ime_password == password) return NX_OK;
    if (window->ime_activated) nx_wayland_ime_deactivate(window);
    window->ime_password = password;
    if (window->ime_requested) nx_wayland_ime_activate(window);
    return NX_OK;
}

nx_result nx_wayland_window_get_ime_preedit(nx_wayland_window *window,
    char *bytes, size_t capacity, size_t *out_length,
    int32_t *out_cursor_begin, int32_t *out_cursor_end) {
    if (!out_length || !out_cursor_begin || !out_cursor_end ||
        (!bytes && capacity)) return NX_ERROR_ARGUMENT;
    *out_length = 0;
    *out_cursor_begin = *out_cursor_end = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    if (!window->text_input_v3 && !window->text_input)
        return NX_ERROR_UNSUPPORTED;
    *out_length = window->ime_preedit_length;
    *out_cursor_begin = window->ime_preedit_begin;
    *out_cursor_end = window->ime_preedit_end;
    if (!bytes) return NX_OK;
    if (capacity <= window->ime_preedit_length) return NX_ERROR_LIMIT;
    if (window->ime_preedit_length)
        memcpy(bytes, window->ime_preedit, window->ime_preedit_length);
    bytes[window->ime_preedit_length] = '\0';
    return NX_OK;
}

nx_result nx_wayland_window_set_ime_surrounding_text(nx_wayland_window *window,
    uint64_t target, uint64_t revision, const char *bytes, size_t length,
    size_t cursor, size_t anchor) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    if (!window->text_input_v3 && !window->text_input)
        return NX_ERROR_UNSUPPORTED;
    if (window->ime_password && target && length)
        return NX_ERROR_ARGUMENT;
    if (!target) {
        if (revision || bytes || length || cursor || anchor)
            return NX_ERROR_ARGUMENT;
        if (!window->ime_surrounding_target) return NX_OK;
        int sent = window->ime_activated && window->ime_surrounding_sent;
        nx_wayland_ime_clear_surrounding(window);
        if (sent) {
            window->ime_surrounding_withdrawn = 1;
            if (window->text_input_v3) {
                zwp_text_input_v3_set_surrounding_text(window->text_input_v3,
                    "", 0, 0);
                zwp_text_input_v3_commit(window->text_input_v3);
                ++window->ime_serial;
            } else {
                zwp_text_input_v1_set_surrounding_text(window->text_input,
                    "", 0, 0);
                zwp_text_input_v1_commit_state(window->text_input,
                    ++window->ime_serial);
            }
        }
        return NX_OK;
    }
    if (!revision || !bytes || length > 4000u ||
        cursor > length || anchor > length ||
        memchr(bytes, '\0', length) ||
        nx_utf8_validate(bytes, length, NULL) != NX_OK ||
        (cursor < length &&
         ((unsigned char)bytes[cursor] & 0xc0u) == 0x80u) ||
        (anchor < length &&
         ((unsigned char)bytes[anchor] & 0xc0u) == 0x80u))
        return NX_ERROR_ARGUMENT;
    if (target == window->ime_surrounding_target &&
        revision == window->ime_surrounding_revision &&
        length == window->ime_surrounding_length &&
        cursor == window->ime_surrounding_cursor &&
        anchor == window->ime_surrounding_anchor &&
        !memcmp(bytes, window->ime_surrounding, length))
        return NX_OK;
    char *copy = window->allocator.allocate(window->allocator.user,
                                            length + 1u);
    if (!copy) return NX_ERROR_MEMORY;
    memcpy(copy, bytes, length);
    copy[length] = '\0';
    int changed_target = target != window->ime_surrounding_target;
    int reenable = window->text_input_v3 && window->ime_activated &&
        (target != window->ime_surrounding_target ||
         !window->ime_surrounding_sent ||
         (length && !window->ime_surrounding_length));
    nx_wayland_ime_clear_surrounding(window);
    window->ime_surrounding = copy;
    window->ime_surrounding_length = length;
    window->ime_surrounding_cursor = cursor;
    window->ime_surrounding_anchor = anchor;
    window->ime_surrounding_target = target;
    window->ime_surrounding_revision = revision;
    window->ime_surrounding_withdrawn = 0;
    if (reenable) nx_wayland_ime_v3_enable(window);
    else if (window->text_input_v3 && window->ime_activated) {
        zwp_text_input_v3_set_text_change_cause(window->text_input_v3,
            ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER);
        zwp_text_input_v3_set_surrounding_text(window->text_input_v3,
            copy, (int32_t)cursor, (int32_t)anchor);
        zwp_text_input_v3_commit(window->text_input_v3);
        window->ime_surrounding_serial = ++window->ime_serial;
        window->ime_surrounding_sent = 1;
    } else if (window->text_input && window->ime_activated) {
        if (changed_target) {
            nx_wayland_ime_v1_clear_delete(window);
            nx_wayland_ime_clear_preedit(window, 1);
            zwp_text_input_v1_reset(window->text_input);
        }
        zwp_text_input_v1_set_surrounding_text(window->text_input,
            copy, (uint32_t)cursor, (uint32_t)anchor);
        zwp_text_input_v1_commit_state(window->text_input,
            ++window->ime_serial);
        window->ime_surrounding_serial = window->ime_serial;
        window->ime_surrounding_sent = 1;
    }
    return NX_OK;
}

nx_result nx_wayland_window_next_event(nx_wayland_window *window,
                                        nx_event *out_event, int *available) {
    if (!out_event || !available) return NX_ERROR_ARGUMENT;
    *out_event = (nx_event){0};
    *available = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    return nx_platform_event_queue_next(&window->event_queue,
                                         out_event, available);
}

nx_result nx_wayland_window_route_events(nx_wayland_window *window,
                                          nx_context *context,
                                          size_t *out_count) {
    if (!out_count) return NX_ERROR_ARGUMENT;
    *out_count = 0;
    if (!window || !context) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    return nx_platform_event_queue_route(&window->event_queue,
                                          context, out_count);
}

nx_result nx_wayland_window_next_file_drop(nx_wayland_window *window,
                                            char *path_utf8, size_t capacity,
                                            size_t *out_length,
                                            nx_file_drop_info *out_info,
                                            int *available) {
    if (!out_length || !out_info || !available || (!path_utf8 && capacity))
        return NX_ERROR_ARGUMENT;
    *out_length = 0;
    *out_info = (nx_file_drop_info){0};
    *available = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    return nx_platform_drop_queue_next(&window->drop_queue, path_utf8,
                                        capacity, out_length, out_info,
                                        available);
}

nx_result nx_wayland_window_clipboard_set_text(nx_wayland_window *window,
                                                const char *bytes,
                                                size_t length) {
    if (!window || (!bytes && length) ||
        (length && memchr(bytes, 0, length)) ||
        nx_utf8_validate(bytes, length, NULL) != NX_OK)
        return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    if (!window->data_manager || !window->data_device)
        return NX_ERROR_UNSUPPORTED;
    if (!window->selection_serial) return NX_ERROR_STATE;
    if (length > NX_WAYLAND_CLIPBOARD_LIMIT) return NX_ERROR_LIMIT;
    unsigned live_sources = 0;
    for (nx_wayland_clipboard_source *current = window->sources; current;
         current = current->next)
        if (++live_sources >= 4u) return NX_ERROR_LIMIT;
    nx_wayland_clipboard_source *source = window->allocator.allocate(
        window->allocator.user, sizeof(*source));
    if (!source) return NX_ERROR_MEMORY;
    memset(source, 0, sizeof(*source));
    source->window = window;
    source->text = window->allocator.allocate(window->allocator.user,
                                              length + 1u);
    if (!source->text) {
        window->allocator.deallocate(window->allocator.user, source,
                                     sizeof(*source));
        return NX_ERROR_MEMORY;
    }
    if (length) memcpy(source->text, bytes, length);
    source->text[length] = '\0';
    source->length = length;
    source->native = wl_data_device_manager_create_data_source(
        window->data_manager);
    if (!source->native || wl_data_source_add_listener(source->native,
            &nx_wayland_source_listener, source) != 0) {
        if (source->native) wl_data_source_destroy(source->native);
        window->allocator.deallocate(window->allocator.user, source->text,
                                     length + 1u);
        window->allocator.deallocate(window->allocator.user, source,
                                     sizeof(*source));
        return NX_ERROR_PLATFORM;
    }
    wl_data_source_offer(source->native, NX_WAYLAND_CLIPBOARD_UTF8);
    wl_data_source_offer(source->native, "text/plain");
    source->next = window->sources;
    window->sources = source;
    window->owned_source = source;
    wl_data_device_set_selection(window->data_device, source->native,
                                  window->selection_serial);
    return NX_OK;
}

static nx_result nx_wayland_clipboard_copy(const char *text, size_t length,
                                           char *bytes, size_t capacity,
                                           size_t *out_length) {
    if ((length && memchr(text, 0, length)) ||
        nx_utf8_validate(text, length, NULL) != NX_OK)
        return NX_ERROR_STATE;
    *out_length = length;
    if (!bytes && !capacity) return NX_OK;
    if (capacity <= length) return NX_ERROR_LIMIT;
    if (length) memcpy(bytes, text, length);
    bytes[length] = '\0';
    return NX_OK;
}

static nx_result nx_wayland_read_fd(nx_wayland_window *window, int read_fd,
                                    char **out_bytes, size_t *out_length,
                                    size_t *out_capacity) {
    *out_bytes = NULL;
    *out_length = *out_capacity = 0;
    char *collected = NULL;
    size_t used = 0, allocated = 0;
    nx_result result = NX_ERROR_PLATFORM;
    uint64_t started = nx_wayland_now_ns();
    uint64_t deadline = started + 5000000000ull;
    ++window->transfer_read_depth;
    if (started) for (;;) {
        for (nx_wayland_window *candidate = nx_wayland_thread_windows;
             candidate; candidate = candidate->thread_next) {
            nx_result pumped = nx_wayland_window_poll(candidate, 0);
            if (pumped != NX_OK) { result = pumped; goto done; }
        }
        char chunk[8192];
        ssize_t count = read(read_fd, chunk, sizeof(chunk));
        if (count == 0) {
            result = NX_OK;
            break;
        }
        if (count > 0) {
            size_t needed = used + (size_t)count;
            if (needed > NX_WAYLAND_CLIPBOARD_LIMIT) {
                result = NX_ERROR_LIMIT;
                break;
            }
            if (needed > allocated) {
                size_t grown = allocated ? allocated * 2u : 8192u;
                if (grown < needed) grown = needed;
                if (grown > NX_WAYLAND_CLIPBOARD_LIMIT)
                    grown = NX_WAYLAND_CLIPBOARD_LIMIT;
                char *replacement = window->allocator.allocate(
                    window->allocator.user, grown);
                if (!replacement) { result = NX_ERROR_MEMORY; break; }
                if (used) memcpy(replacement, collected, used);
                if (collected) window->allocator.deallocate(
                    window->allocator.user, collected, allocated);
                collected = replacement;
                allocated = grown;
            }
            memcpy(collected + used, chunk, (size_t)count);
            used = needed;
            continue;
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            break;
        uint64_t now = nx_wayland_now_ns();
        if (!now || now >= deadline) break;
        struct pollfd pipe_fd = {read_fd, POLLIN, 0};
        (void)poll(&pipe_fd, 1, 10);
    }
done:
    --window->transfer_read_depth;
    close(read_fd);
    if (result == NX_OK) {
        *out_bytes = collected;
        *out_length = used;
        *out_capacity = allocated;
    } else if (collected)
        window->allocator.deallocate(window->allocator.user,
                                     collected, allocated);
    return result;
}

static nx_result nx_wayland_clipboard_read_fd(nx_wayland_window *window,
                                              int read_fd, char *bytes,
                                              size_t capacity,
                                              size_t *out_length) {
    char *collected = NULL;
    size_t length = 0, allocated = 0;
    nx_result result = nx_wayland_read_fd(window, read_fd, &collected,
                                          &length, &allocated);
    if (result == NX_OK)
        result = nx_wayland_clipboard_copy(collected, length, bytes,
                                           capacity, out_length);
    if (collected) window->allocator.deallocate(window->allocator.user,
                                                 collected, allocated);
    return result;
}

static nx_result nx_wayland_process_drop(nx_wayland_window *window) {
    if (window->transfer_read_depth) return NX_OK;
    nx_wayland_clipboard_offer *offer = window->drop_offer;
    window->drop_offer = NULL;
    if (!offer) return NX_OK;
    if (!(offer->mime_types & NX_WAYLAND_MIME_URI_LIST)) {
        nx_wayland_retire_offer(offer);
        return NX_OK;
    }
    int pipe_fds[2];
    if (pipe2(pipe_fds, O_CLOEXEC) != 0) {
        nx_wayland_retire_offer(offer);
        return NX_ERROR_PLATFORM;
    }
    int flags = fcntl(pipe_fds[0], F_GETFL);
    if (flags < 0 || fcntl(pipe_fds[0], F_SETFL,
                            flags | O_NONBLOCK) < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        nx_wayland_retire_offer(offer);
        return NX_ERROR_PLATFORM;
    }
    ++offer->in_use;
    float drop_x = window->drop_x, drop_y = window->drop_y;
    char local_host[256] = {0};
    if (gethostname(local_host, sizeof(local_host)) != 0)
        local_host[0] = '\0';
    local_host[sizeof(local_host) - 1u] = '\0';
    const char *host = local_host[0] ? local_host : NULL;
    wl_data_offer_receive(offer->native, "text/uri-list", pipe_fds[1]);
    close(pipe_fds[1]);
    char *payload = NULL;
    size_t length = 0, capacity = 0;
    nx_result result = nx_wayland_read_fd(window, pipe_fds[0], &payload,
                                           &length, &capacity);
    if (result == NX_OK)
        result = nx_platform_uri_list_append(&window->drop_queue, payload,
            length, drop_x, drop_y, host);
    if (payload) window->allocator.deallocate(window->allocator.user,
                                               payload, capacity);
    --offer->in_use;
    nx_wayland_retire_offer(offer);
    return result;
}

nx_result nx_wayland_window_clipboard_get_text(nx_wayland_window *window,
                                                char *bytes, size_t capacity,
                                                size_t *out_length) {
    if (!out_length || (!bytes && capacity)) return NX_ERROR_ARGUMENT;
    *out_length = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    if (!window->data_device) return NX_ERROR_UNSUPPORTED;
    if (window->owned_source)
        return nx_wayland_clipboard_copy(window->owned_source->text,
            window->owned_source->length, bytes, capacity, out_length);
    nx_wayland_clipboard_offer *offer = window->selection_offer;
    if (!offer) return NX_ERROR_STATE;
    const char *mime = offer->mime_types & NX_WAYLAND_MIME_UTF8 ?
        NX_WAYLAND_CLIPBOARD_UTF8 :
        offer->mime_types & NX_WAYLAND_MIME_UTF8_STRING ? "UTF8_STRING" :
        offer->mime_types & NX_WAYLAND_MIME_PLAIN ? "text/plain" : NULL;
    if (!mime) return NX_ERROR_STATE;
    int pipe_fds[2];
    if (pipe2(pipe_fds, O_CLOEXEC) != 0)
        return NX_ERROR_PLATFORM;
    int read_flags = fcntl(pipe_fds[0], F_GETFL);
    if (read_flags < 0 ||
        fcntl(pipe_fds[0], F_SETFL, read_flags | O_NONBLOCK) < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return NX_ERROR_PLATFORM;
    }
    ++offer->in_use;
    wl_data_offer_receive(offer->native, mime, pipe_fds[1]);
    close(pipe_fds[1]);
    nx_result result = nx_wayland_clipboard_read_fd(window, pipe_fds[0],
                                                     bytes, capacity,
                                                     out_length);
    --offer->in_use;
    if (offer->obsolete) nx_wayland_retire_offer(offer);
    return result;
}

void nx_wayland_window_destroy(nx_wayland_window *window) {
    if (!window || !nx_wayland_on_owner_thread(window)) return;
    /* Another reader of this display can have decoded default-queue new-id
       events without invoking our listeners. Adopt those objects while their
       parents/listener data are alive, then destroy them below. This reads no
       new socket data and does not wait for the compositor. */
    if (window->display) (void)wl_display_dispatch_pending(window->display);
    nx_wayland_window **thread_slot = &nx_wayland_thread_windows;
    while (*thread_slot && *thread_slot != window)
        thread_slot = &(*thread_slot)->thread_next;
    if (*thread_slot) *thread_slot = window->thread_next;
    while (window->writes) {
        nx_wayland_clipboard_write *transfer = window->writes;
        window->writes = transfer->next;
        close(transfer->fd);
        --transfer->source->write_refs;
        window->allocator.deallocate(window->allocator.user, transfer,
                                     sizeof(*transfer));
    }
    while (window->sources) nx_wayland_free_source(window->sources);
    while (window->offers) {
        nx_wayland_clipboard_offer *offer = window->offers;
        window->offers = offer->next;
        wl_data_offer_destroy(offer->native);
        window->allocator.deallocate(window->allocator.user, offer,
                                     sizeof(*offer));
    }
    nx_platform_drop_queue_destroy(&window->drop_queue);
    nx_wayland_buffer *buffer = window->buffers;
    while (buffer) {
        nx_wayland_buffer *next = buffer->next;
        if (buffer->native) wl_buffer_destroy(buffer->native);
        if (buffer->pixels) munmap(buffer->pixels, buffer->size);
        window->allocator.deallocate(window->allocator.user, buffer,
                                     sizeof(*buffer));
        buffer = next;
    }
    if (window->toplevel) xdg_toplevel_destroy(window->toplevel);
    if (window->xdg_surface) xdg_surface_destroy(window->xdg_surface);
    nx_wayland_ime_deactivate(window);
    nx_wayland_ime_clear_pending(window);
    nx_wayland_ime_clear_preedit(window, 0);
    if (window->text_input_v3)
        zwp_text_input_v3_destroy(window->text_input_v3);
    if (window->text_input_manager_v3)
        zwp_text_input_manager_v3_destroy(window->text_input_manager_v3);
    if (window->text_input) zwp_text_input_v1_destroy(window->text_input);
    if (window->text_input_manager)
        zwp_text_input_manager_v1_destroy(window->text_input_manager);
    if (window->surface) wl_surface_destroy(window->surface);
    if (window->cursor_surface) wl_surface_destroy(window->cursor_surface);
    if (window->wm_base) xdg_wm_base_destroy(window->wm_base);
    if (window->data_device) wl_data_device_destroy(window->data_device);
    if (window->data_manager)
        wl_data_device_manager_destroy(window->data_manager);
    if (window->pointer) wl_pointer_release(window->pointer);
    if (window->keyboard) wl_keyboard_release(window->keyboard);
    if (window->seat) wl_seat_release(window->seat);
    if (window->shm) wl_shm_destroy(window->shm);
    if (window->compositor) wl_compositor_destroy(window->compositor);
    nx_wayland_output *output = window->outputs;
    while (output) {
        nx_wayland_output *next = output->next;
        if (output->native) {
            if (output->version >= 3) wl_output_release(output->native);
            else wl_output_destroy(output->native);
        }
        window->allocator.deallocate(window->allocator.user, output,
                                     sizeof(*output));
        output = next;
    }
    if (window->registry) wl_registry_destroy(window->registry);
    if (window->display) wl_display_disconnect(window->display);
    if (window->key_state) xkb_state_unref(window->key_state);
    if (window->keymap) xkb_keymap_unref(window->keymap);
    if (window->xkb_context) xkb_context_unref(window->xkb_context);
    nx_platform_event_queue_destroy(&window->event_queue);
    nx_allocator allocator = window->allocator;
    allocator.deallocate(allocator.user, window, sizeof(*window));
}

struct wl_display *nx_wayland_window_display(nx_wayland_window *window) {
    return window ? window->display : NULL;
}

struct wl_surface *nx_wayland_window_surface(nx_wayland_window *window) {
    return window ? window->surface : NULL;
}

unsigned nx_wayland_window_ready(nx_wayland_window *window) {
    if (!window) return 0;
    return (window->buffer_attached ? 1u : 0u) |
           (window->pointer ? 2u : 0u) |
           (window->key_state ? 4u : 0u) |
           (window->data_device ? 8u : 0u);
}

#ifdef NEXAUI_WAYLAND_TEST_HOOKS
nx_result nx_wayland_window_test_uri_drop(nx_wayland_window *window,
                                          const char *text, size_t length,
                                          float x, float y) {
    if (!window || (!text && length)) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    char local_host[256] = {0};
    if (gethostname(local_host, sizeof(local_host)) != 0)
        local_host[0] = '\0';
    local_host[sizeof(local_host) - 1u] = '\0';
    return nx_platform_uri_list_append(&window->drop_queue, text, length,
                                        x, y, local_host[0] ? local_host : NULL);
}

nx_result nx_wayland_window_test_clipboard_roundtrip(
    nx_wayland_window *window, const char *input, size_t length,
    char *output, size_t capacity, size_t *out_length) {
    if (!window || (!input && length) || !out_length ||
        (!output && capacity)) return NX_ERROR_ARGUMENT;
    *out_length = 0;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    nx_wayland_clipboard_source *source = window->allocator.allocate(
        window->allocator.user, sizeof(*source));
    if (!source) return NX_ERROR_MEMORY;
    memset(source, 0, sizeof(*source));
    source->window = window;
    source->text = window->allocator.allocate(window->allocator.user,
                                              length + 1u);
    if (!source->text) {
        window->allocator.deallocate(window->allocator.user, source,
                                     sizeof(*source));
        return NX_ERROR_MEMORY;
    }
    if (length) memcpy(source->text, input, length);
    source->text[length] = '\0';
    source->length = length;
    int pipe_fds[2];
    if (pipe2(pipe_fds, O_CLOEXEC | O_NONBLOCK) != 0) {
        window->allocator.deallocate(window->allocator.user, source->text,
                                     length + 1u);
        window->allocator.deallocate(window->allocator.user, source,
                                     sizeof(*source));
        return NX_ERROR_PLATFORM;
    }
    source->next = window->sources;
    window->sources = source;
    nx_wayland_source_send(source, NULL, NX_WAYLAND_CLIPBOARD_UTF8,
                            pipe_fds[1]);
    nx_wayland_source_cancelled(source, NULL);
    return nx_wayland_clipboard_read_fd(window, pipe_fds[0], output,
                                        capacity, out_length);
}

nx_result nx_wayland_window_test_output(nx_wayland_window *window,
                                        int scale, int entered) {
    if (!window || scale < 1 || !nx_wayland_on_owner_thread(window))
        return NX_ERROR_ARGUMENT;
    nx_wayland_output *output = window->outputs;
    if (!output || output->removed) return NX_ERROR_STATE;
    nx_wayland_output_scale(output, output->native, scale);
    if (entered)
        nx_wayland_surface_enter(window, window->surface, output->native);
    else
        nx_wayland_surface_leave(window, window->surface, output->native);
    return window->error != NX_OK ? window->error : window->event_queue.error;
}

nx_result nx_wayland_window_test_extents(nx_wayland_window *window,
                                         int *background_width,
                                         int *background_height,
                                         int *cursor_width,
                                         int *cursor_height) {
    if (!window || !background_width || !background_height ||
        !cursor_width || !cursor_height) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    *background_width = window->background_width;
    *background_height = window->background_height;
    *cursor_width = window->cursor_width;
    *cursor_height = window->cursor_height;
    return NX_OK;
}

nx_result nx_wayland_window_test_pointer_enter(nx_wayland_window *window,
                                                int x, int y) {
    if (!window || !window->pointer) return NX_ERROR_STATE;
    nx_wayland_pointer_enter(window, window->pointer, 0, window->surface,
                              wl_fixed_from_int(x), wl_fixed_from_int(y));
    return window->error;
}

nx_result nx_wayland_window_test_focus(nx_wayland_window *window, int focused) {
    if (!window || !window->keyboard || !window->key_state)
        return NX_ERROR_STATE;
    if (focused) {
        struct wl_array keys = {0};
        nx_wayland_keyboard_enter(window, window->keyboard, 0,
                                   window->surface, &keys);
    } else nx_wayland_keyboard_leave(window, window->keyboard, 0,
                                     window->surface);
    return window->event_queue.error;
}

nx_result nx_wayland_window_test_key(nx_wayland_window *window,
                                     char letter, int down) {
    if (!window || !window->keymap || !window->key_state ||
        letter < 'a' || letter > 'z') return NX_ERROR_ARGUMENT;
    for (xkb_keycode_t code = xkb_keymap_min_keycode(window->keymap);
         code <= xkb_keymap_max_keycode(window->keymap); ++code) {
        if (code < 8 || xkb_state_key_get_one_sym(window->key_state, code) !=
                         (xkb_keysym_t)(XKB_KEY_a + letter - 'a')) continue;
        nx_wayland_keyboard_key(window, window->keyboard, 0, 0,
                                 code - 8u, down ?
                                 WL_KEYBOARD_KEY_STATE_PRESSED :
                                 WL_KEYBOARD_KEY_STATE_RELEASED);
        return window->event_queue.error;
    }
    return NX_ERROR_STATE;
}

nx_result nx_wayland_window_test_repeat_info(nx_wayland_window *window,
                                              int rate, int delay_ms) {
    if (!window || !window->keyboard) return NX_ERROR_STATE;
    nx_wayland_keyboard_repeat_info(window, window->keyboard, rate,
                                     delay_ms);
    return window->error;
}

nx_result nx_wayland_window_test_ime_commit(nx_wayland_window *window,
                                             const char *text, int stale) {
    if (!window || !text) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    if (window->text_input_v3 && window->ime_requested &&
        !window->ime_v3_focused)
        nx_wayland_ime_v3_enter(window, window->text_input_v3,
                                 window->surface);
    if (!window->ime_activated) return NX_ERROR_STATE;
    if (window->text_input_v3) {
        nx_wayland_ime_v3_commit(window, window->text_input_v3, text);
        nx_wayland_ime_v3_done(window, window->text_input_v3,
            stale ? window->ime_activation_serial - 1u :
                    window->ime_serial);
        return window->event_queue.error;
    }
    if (!window->text_input) return NX_ERROR_STATE;
    nx_wayland_ime_enter(window, window->text_input, window->surface);
    nx_wayland_ime_commit(window, window->text_input,
        stale ? window->ime_activation_serial - 1u : window->ime_serial,
        text);
    return window->event_queue.error;
}

nx_result nx_wayland_window_test_ime_preedit(nx_wayland_window *window,
    const char *text, int32_t cursor, int stale) {
    if (!window || !text) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    if (window->text_input_v3 || !window->text_input)
        return NX_ERROR_UNSUPPORTED;
    if (!window->ime_activated) return NX_ERROR_STATE;
    nx_wayland_ime_enter(window, window->text_input, window->surface);
    nx_wayland_ime_preedit_cursor(window, window->text_input, cursor);
    nx_wayland_ime_preedit(window, window->text_input,
        stale ? window->ime_activation_serial - 1u : window->ime_serial,
        text, "");
    return window->event_queue.error;
}

nx_result nx_wayland_window_test_ime_state(nx_wayland_window *window,
                                            int *requested, int *activated) {
    if (!window || !requested || !activated) return NX_ERROR_ARGUMENT;
    if (!nx_wayland_on_owner_thread(window)) return NX_ERROR_STATE;
    *requested = window->ime_requested;
    *activated = window->ime_activated;
    return NX_OK;
}
#endif
