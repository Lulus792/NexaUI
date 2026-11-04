#ifndef NEXAUI_WAYLAND_V3_FIXTURE_H
#define NEXAUI_WAYLAND_V3_FIXTURE_H
#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include "wayland/window_internal.h"
#include "xdg-shell-server-protocol.h"
#include "text-input-unstable-v3-server-protocol.h"
#include "fixture_font.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    exit(1); \
} } while (0)

typedef struct test_server {
    struct wl_display *display;
    struct wl_resource *surface, *xdg_surface, *toplevel, *text_input;
    struct wl_resource *data_device;
    atomic_int send_offer;
    atomic_int offer_reads, offer_once;
    int configured, enabled, pending_done, width, height;
    uint32_t commit_serial;
    char surrounding[4001];
    size_t surrounding_length;
    int32_t surrounding_cursor, surrounding_anchor;
    atomic_int enable_commits, disable_commits, caret_ok, surrounding_ok;
    atomic_int field_surrounding_ok;
    atomic_int normal_content, password_content;
} test_server;

static void resource_destroy(struct wl_client *client,
                             struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}
static void offer_receive(struct wl_client *client,struct wl_resource *resource,
                          const char *mime,int32_t fd) {
    (void)client;(void)mime;test_server *server=wl_resource_get_user_data(resource);
    int previous=atomic_fetch_add(&server->offer_reads,1);
    if(!atomic_load(&server->offer_once) || previous==0) {
        const char payload[]="Once";
        if(write(fd,payload,sizeof(payload)-1u)!=(ssize_t)(sizeof(payload)-1u))abort();
    }
    close(fd);
}

static void surface_commit(struct wl_client *client,
                           struct wl_resource *resource) {
    test_server *server = wl_resource_get_user_data(resource);
    if (atomic_exchange(&server->send_offer, 0)) {
        struct wl_resource *offer = wl_resource_create(client, &wl_data_offer_interface, 3, 0);
        CHECK(offer && server->data_device);
        static const struct wl_data_offer_interface implementation = {.receive = offer_receive, .destroy = resource_destroy};
        wl_resource_set_implementation(offer, &implementation, server, NULL);
        wl_data_device_send_data_offer(server->data_device, offer);
        wl_data_offer_send_offer(offer, "text/plain;charset=utf-8");
        wl_data_device_send_selection(server->data_device, offer);
    }
    if (server->configured) {
        if (server->pending_done) {
            server->pending_done = 0;
            zwp_text_input_v3_send_done(server->text_input,
                                         server->commit_serial);
        }
        return;
    }
    if (!server->xdg_surface || !server->toplevel)
        return;
    server->configured = 1;
    struct wl_array states;
    wl_array_init(&states);
    xdg_toplevel_send_configure(server->toplevel,
        server->width ? server->width : 240, server->height ? server->height : 120, &states);
    wl_array_release(&states);
    xdg_surface_send_configure(server->xdg_surface, 1);
    if (server->text_input)
        zwp_text_input_v3_send_enter(server->text_input, server->surface);
}

static const struct wl_surface_interface surface_implementation = {
    .destroy = resource_destroy,
    .commit = surface_commit
};

static void create_surface(struct wl_client *client,
                           struct wl_resource *resource, uint32_t id) {
    test_server *server = wl_resource_get_user_data(resource);
    server->surface = wl_resource_create(client, &wl_surface_interface, 1, id);
    CHECK(server->surface);
    wl_resource_set_implementation(server->surface,
                                   &surface_implementation, server, NULL);
}

static const struct wl_compositor_interface compositor_implementation = {
    .create_surface = create_surface
};

static void bind_compositor(struct wl_client *client, void *data,
                            uint32_t version, uint32_t id) {
    struct wl_resource *resource = wl_resource_create(
        client, &wl_compositor_interface, version, id);
    CHECK(resource);
    wl_resource_set_implementation(resource, &compositor_implementation,
                                   data, NULL);
}

static void bind_shm(struct wl_client *client, void *data,
                     uint32_t version, uint32_t id) {
    struct wl_resource *resource = wl_resource_create(
        client, &wl_shm_interface, version, id);
    CHECK(resource);
    wl_resource_set_user_data(resource, data);
    wl_shm_send_format(resource, WL_SHM_FORMAT_ARGB8888);
}

static const struct wl_seat_interface seat_implementation = {
    .release = resource_destroy
};

static void bind_seat(struct wl_client *client, void *data,
                      uint32_t version, uint32_t id) {
    struct wl_resource *resource = wl_resource_create(
        client, &wl_seat_interface, version, id);
    CHECK(resource);
    wl_resource_set_implementation(resource, &seat_implementation, data, NULL);
    wl_seat_send_capabilities(resource, 0);
}

static void toplevel_set_string(struct wl_client *client,
    struct wl_resource *resource, const char *value) {
    (void)client; (void)resource; (void)value;
}

static const struct xdg_toplevel_interface toplevel_implementation = {
    .destroy = resource_destroy,
    .set_title = toplevel_set_string,
    .set_app_id = toplevel_set_string
};

static void xdg_get_toplevel(struct wl_client *client,
                             struct wl_resource *resource, uint32_t id) {
    test_server *server = wl_resource_get_user_data(resource);
    server->toplevel = wl_resource_create(client, &xdg_toplevel_interface,
                                           1, id);
    CHECK(server->toplevel);
    wl_resource_set_implementation(server->toplevel,
        &toplevel_implementation, server, NULL);
}

static void xdg_ack_configure(struct wl_client *client,
                              struct wl_resource *resource, uint32_t serial) {
    (void)client; (void)resource;
    CHECK(serial == 1);
}

static void xdg_set_geometry(struct wl_client *client,
    struct wl_resource *resource, int32_t x, int32_t y,
    int32_t width, int32_t height) {
    (void)client; (void)x; (void)y;
    test_server *server=wl_resource_get_user_data(resource);
    CHECK(width == (server->width ? server->width : 240) &&
          height == (server->height ? server->height : 120));
}

static const struct xdg_surface_interface xdg_surface_implementation = {
    .destroy = resource_destroy,
    .get_toplevel = xdg_get_toplevel,
    .set_window_geometry = xdg_set_geometry,
    .ack_configure = xdg_ack_configure
};

static void xdg_get_surface(struct wl_client *client,
                            struct wl_resource *resource, uint32_t id,
                            struct wl_resource *surface) {
    test_server *server = wl_resource_get_user_data(resource);
    CHECK(surface == server->surface);
    server->xdg_surface = wl_resource_create(client, &xdg_surface_interface,
                                              1, id);
    CHECK(server->xdg_surface);
    wl_resource_set_implementation(server->xdg_surface,
        &xdg_surface_implementation, server, NULL);
}

static const struct xdg_wm_base_interface wm_implementation = {
    .destroy = resource_destroy,
    .get_xdg_surface = xdg_get_surface
};

static void bind_wm(struct wl_client *client, void *data,
                    uint32_t version, uint32_t id) {
    struct wl_resource *resource = wl_resource_create(
        client, &xdg_wm_base_interface, version, id);
    CHECK(resource);
    wl_resource_set_implementation(resource, &wm_implementation, data, NULL);
}

static void text_enable(struct wl_client *client,
                        struct wl_resource *resource) {
    (void)client;
    ((test_server *)wl_resource_get_user_data(resource))->enabled = 1;
}

static void text_disable(struct wl_client *client,
                         struct wl_resource *resource) {
    (void)client;
    ((test_server *)wl_resource_get_user_data(resource))->enabled = 0;
}

static void text_content_type(struct wl_client *client,
    struct wl_resource *resource, uint32_t hint, uint32_t purpose) {
    test_server *server = wl_resource_get_user_data(resource);
    (void)client;
    if (hint == ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE &&
        purpose == ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL)
        atomic_fetch_add(&server->normal_content, 1);
    else if (hint == ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA &&
             purpose == ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD)
        atomic_fetch_add(&server->password_content, 1);
    else CHECK(0);
}

static void text_cursor_rectangle(struct wl_client *client,
    struct wl_resource *resource, int32_t x, int32_t y,
    int32_t width, int32_t height) {
    test_server *server = wl_resource_get_user_data(resource);
    (void)client;
    if (x == 12 && y == 34 && width == 2 && height == 18)
        atomic_store(&server->caret_ok, 1);
}

static void text_surrounding(struct wl_client *client,
    struct wl_resource *resource, const char *value,
    int32_t cursor, int32_t anchor) {
    test_server *server = wl_resource_get_user_data(resource);
    (void)client;
    size_t length = strlen(value);
    CHECK(length <= 4000);
    memcpy(server->surrounding, value, length + 1u);
    server->surrounding_length = length;
    server->surrounding_cursor = cursor;
    server->surrounding_anchor = anchor;
    if (length == 6 && memcmp(value, "ab\xc3\xa9" "cd", 6) == 0 &&
        cursor == 6 && anchor == 6)
        atomic_store(&server->surrounding_ok, 1);
    if (length == 4 && memcmp(value, "abcd", 4) == 0 &&
        cursor == 4 && anchor == 4)
        atomic_store(&server->field_surrounding_ok, 1);
}

static void text_change_cause(struct wl_client *client,
    struct wl_resource *resource, uint32_t cause) {
    (void)client; (void)resource;
    CHECK(cause == ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER);
}

static void text_commit(struct wl_client *client,
                        struct wl_resource *resource) {
    test_server *server = wl_resource_get_user_data(resource);
    (void)client;
    ++server->commit_serial;
    if (server->enabled) {
        atomic_fetch_add(&server->enable_commits, 1);
        if (server->commit_serial == 1) {
            zwp_text_input_v3_send_commit_string(resource, "caf\xc3\xa9");
            zwp_text_input_v3_send_preedit_string(resource, "na\xc3\xaf", 2, 2);
        } else if (atomic_load(&server->field_surrounding_ok) &&
                   server->surrounding_length == 4 &&
                   memcmp(server->surrounding, "abcd", 4) == 0) {
            zwp_text_input_v3_send_delete_surrounding_text(resource, 1, 0);
            zwp_text_input_v3_send_commit_string(resource, "!");
            zwp_text_input_v3_send_preedit_string(resource, "", 0, 0);
        } else if (atomic_load(&server->surrounding_ok)) {
            zwp_text_input_v3_send_delete_surrounding_text(resource, 1, 0);
            zwp_text_input_v3_send_commit_string(resource, "!");
            zwp_text_input_v3_send_preedit_string(resource, "", 0, 0);
        } else {
            zwp_text_input_v3_send_preedit_string(resource, "na\xc3\xaf", -1, -1);
        }
        server->pending_done = 1;
    } else {
        atomic_fetch_add(&server->disable_commits, 1);
    }
}

static const struct zwp_text_input_v3_interface text_implementation = {
    .destroy = resource_destroy,
    .enable = text_enable,
    .disable = text_disable,
    .set_content_type = text_content_type,
    .set_cursor_rectangle = text_cursor_rectangle,
    .set_surrounding_text = text_surrounding,
    .set_text_change_cause = text_change_cause,
    .commit = text_commit
};

static void get_text_input(struct wl_client *client,
    struct wl_resource *resource, uint32_t id, struct wl_resource *seat) {
    test_server *server = wl_resource_get_user_data(resource);
    (void)seat;
    server->text_input = wl_resource_create(client,
        &zwp_text_input_v3_interface, 1, id);
    CHECK(server->text_input);
    wl_resource_set_implementation(server->text_input,
        &text_implementation, server, NULL);
}

static const struct zwp_text_input_manager_v3_interface manager_implementation = {
    .destroy = resource_destroy,
    .get_text_input = get_text_input
};

static void bind_manager(struct wl_client *client, void *data,
                         uint32_t version, uint32_t id) {
    struct wl_resource *resource = wl_resource_create(client,
        &zwp_text_input_manager_v3_interface, version, id);
    CHECK(resource);
    wl_resource_set_implementation(resource, &manager_implementation,
                                   data, NULL);
}

static void get_data_device(struct wl_client *client, struct wl_resource *manager,
                             uint32_t id, struct wl_resource *seat) {
    (void)seat;
    test_server *s = wl_resource_get_user_data(manager);
    s->data_device = wl_resource_create(client, &wl_data_device_interface, 3, id);
    CHECK(s->data_device);
    static const struct wl_data_device_interface implementation = {.release = resource_destroy};
    wl_resource_set_implementation(s->data_device, &implementation, s, NULL);
}
static void bind_data_manager(struct wl_client *client, void *data,
                               uint32_t version, uint32_t id) {
    struct wl_resource *manager = wl_resource_create(client, &wl_data_device_manager_interface, version, id);
    CHECK(manager);
    static const struct wl_data_device_manager_interface implementation = {.get_data_device = get_data_device};
    wl_resource_set_implementation(manager, &implementation, data, NULL);
}
typedef struct window_heap { size_t calls, live; } window_heap;
static void *window_allocate(void *user, size_t bytes) {
    window_heap *h = user; void *p = malloc(bytes);
    if (p) { ++h->calls; ++h->live; } return p;
}
static void window_free(void *user, void *p, size_t bytes) {
    (void)bytes; window_heap *h = user;
    CHECK(h->live); --h->live; free(p);
}

static void *run_server(void *data) {
    wl_display_run(((test_server *)data)->display);
    return NULL;
}


#endif
