#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include "wayland/window_internal.h"
#include "xdg-shell-server-protocol.h"
#include "text-input-unstable-v1-server-protocol.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server.h>

#include "fixture_font.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    exit(1); \
} } while (0)

typedef struct test_server {
    struct wl_display *display;
    struct wl_resource *surface, *xdg_surface, *toplevel;
    struct wl_resource *keyboard, *text_input;
    int configured, enabled, state_commits, keyboard_entered;
    uint32_t serial;
    atomic_int activation_count, deactivation_count, caret_ok;
    atomic_int normal_content, password_content;
    char surrounding[64];
    uint32_t surrounding_cursor, surrounding_anchor;
    atomic_int surrounding_count;
    int field_commit_sent;
} test_server;

static void resource_destroy(struct wl_client *client,
    struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void surface_commit(struct wl_client *client,
    struct wl_resource *resource) {
    test_server *server = wl_resource_get_user_data(resource);
    (void)client;
    if (server->configured || !server->xdg_surface || !server->toplevel)
        return;
    server->configured = 1;
    struct wl_array states, keys;
    wl_array_init(&states);
    xdg_toplevel_send_configure(server->toplevel, 240, 120, &states);
    wl_array_release(&states);
    xdg_surface_send_configure(server->xdg_surface, 1);
    if (server->keyboard && !server->keyboard_entered) {
        wl_array_init(&keys);
        wl_keyboard_send_enter(server->keyboard, 2, server->surface, &keys);
        wl_array_release(&keys);
        server->keyboard_entered = 1;
    }
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
    struct wl_resource *resource = wl_resource_create(client,
        &wl_compositor_interface, version, id);
    CHECK(resource);
    wl_resource_set_implementation(resource, &compositor_implementation,
        data, NULL);
}

static void bind_shm(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
    struct wl_resource *resource = wl_resource_create(client,
        &wl_shm_interface, version, id);
    CHECK(resource);
    wl_resource_set_user_data(resource, data);
    wl_shm_send_format(resource, WL_SHM_FORMAT_ARGB8888);
}

static void get_keyboard(struct wl_client *client,
    struct wl_resource *resource, uint32_t id) {
    test_server *server = wl_resource_get_user_data(resource);
    server->keyboard = wl_resource_create(client, &wl_keyboard_interface, 1, id);
    CHECK(server->keyboard);
    wl_resource_set_implementation(server->keyboard, NULL, server, NULL);
    if (server->configured && !server->keyboard_entered) {
        struct wl_array keys;
        wl_array_init(&keys);
        wl_keyboard_send_enter(server->keyboard, 2, server->surface, &keys);
        wl_array_release(&keys);
        server->keyboard_entered = 1;
    }
}

static const struct wl_seat_interface seat_implementation = {
    .get_keyboard = get_keyboard,
    .release = resource_destroy
};

static void bind_seat(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
    struct wl_resource *resource = wl_resource_create(client,
        &wl_seat_interface, version, id);
    CHECK(resource);
    wl_resource_set_implementation(resource, &seat_implementation, data, NULL);
    wl_seat_send_capabilities(resource, WL_SEAT_CAPABILITY_KEYBOARD);
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
    server->toplevel = wl_resource_create(client,
        &xdg_toplevel_interface, 1, id);
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
    (void)client; (void)resource; (void)x; (void)y;
    CHECK(width == 240 && height == 120);
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
    server->xdg_surface = wl_resource_create(client,
        &xdg_surface_interface, 1, id);
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
    struct wl_resource *resource = wl_resource_create(client,
        &xdg_wm_base_interface, version, id);
    CHECK(resource);
    wl_resource_set_implementation(resource, &wm_implementation, data, NULL);
}

static void text_activate(struct wl_client *client,
    struct wl_resource *resource, struct wl_resource *seat,
    struct wl_resource *surface) {
    test_server *server = wl_resource_get_user_data(resource);
    (void)client; (void)seat;
    CHECK(surface == server->surface);
    server->enabled = 1;
    atomic_fetch_add(&server->activation_count, 1);
    zwp_text_input_v1_send_enter(resource, surface);
}

static void text_deactivate(struct wl_client *client,
    struct wl_resource *resource, struct wl_resource *seat) {
    test_server *server = wl_resource_get_user_data(resource);
    (void)client; (void)seat;
    server->enabled = 0;
    atomic_fetch_add(&server->deactivation_count, 1);
    zwp_text_input_v1_send_leave(resource);
}

static void text_content_type(struct wl_client *client,
    struct wl_resource *resource, uint32_t hint, uint32_t purpose) {
    test_server *server = wl_resource_get_user_data(resource);
    (void)client;
    if (hint == ZWP_TEXT_INPUT_V1_CONTENT_HINT_NONE &&
        purpose == ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL)
        atomic_fetch_add(&server->normal_content, 1);
    else if (hint == ZWP_TEXT_INPUT_V1_CONTENT_HINT_SENSITIVE_DATA &&
             purpose == ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PASSWORD)
        atomic_fetch_add(&server->password_content, 1);
    else CHECK(0);
}

static void text_reset(struct wl_client *client,
    struct wl_resource *resource) {
    (void)client; (void)resource;
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
    struct wl_resource *resource, const char *text,
    uint32_t cursor, uint32_t anchor) {
    test_server *server = wl_resource_get_user_data(resource);
    (void)client;
    CHECK(strlen(text) < sizeof(server->surrounding));
    strcpy(server->surrounding, text);
    server->surrounding_cursor = cursor;
    server->surrounding_anchor = anchor;
    atomic_fetch_add(&server->surrounding_count, 1);
}

static void text_commit_state(struct wl_client *client,
    struct wl_resource *resource, uint32_t serial) {
    test_server *server = wl_resource_get_user_data(resource);
    (void)client;
    CHECK(server->enabled);
    server->serial = serial;
    if (++server->state_commits == 1) {
        zwp_text_input_v1_send_preedit_cursor(resource, 2);
        zwp_text_input_v1_send_preedit_string(resource, serial,
            "na\xc3\xaf", "");
    } else if (server->state_commits == 2) {
        zwp_text_input_v1_send_commit_string(resource, serial,
            "caf\xc3\xa9");
    } else if (server->state_commits == 3) {
        zwp_text_input_v1_send_preedit_cursor(resource, 1);
        zwp_text_input_v1_send_preedit_string(resource, serial - 1u,
            "stale", "");
    } else if (server->state_commits == 4) {
        CHECK(strcmp(server->surrounding, "ab\xc3\xa9" "cd") == 0);
        CHECK(server->surrounding_cursor == 6 &&
              server->surrounding_anchor == 6);
        zwp_text_input_v1_send_delete_surrounding_text(resource, -1, 1);
        zwp_text_input_v1_send_commit_string(resource, serial, "!");
    } else if (server->state_commits == 5) {
        CHECK(server->surrounding[0] == '\0');
        zwp_text_input_v1_send_delete_surrounding_text(resource, -1, 1);
        zwp_text_input_v1_send_commit_string(resource, serial, "x");
    } else if (server->state_commits == 6) {
        CHECK(strcmp(server->surrounding, "ab\xc3\xa9" "cd") == 0);
        CHECK(server->surrounding_cursor == 4 &&
              server->surrounding_anchor == 4);
        zwp_text_input_v1_send_delete_surrounding_text(resource, -2, 1);
        zwp_text_input_v1_send_commit_string(resource, serial, "z");
    } else if (strcmp(server->surrounding, "q") == 0) {
        zwp_text_input_v1_send_delete_surrounding_text(resource, 0, 1);
        zwp_text_input_v1_send_commit_string(resource, serial, "\xc0");
    } else if (!server->field_commit_sent &&
               strcmp(server->surrounding, "abcd") == 0) {
        CHECK(server->surrounding_cursor == 4 &&
              server->surrounding_anchor == 4);
        server->field_commit_sent = 1;
        zwp_text_input_v1_send_delete_surrounding_text(resource, -1, 1);
        zwp_text_input_v1_send_commit_string(resource, serial, "!");
    }
}

static const struct zwp_text_input_v1_interface text_implementation = {
    .activate = text_activate,
    .deactivate = text_deactivate,
    .reset = text_reset,
    .set_content_type = text_content_type,
    .set_cursor_rectangle = text_cursor_rectangle,
    .set_surrounding_text = text_surrounding,
    .commit_state = text_commit_state
};

static void create_text_input(struct wl_client *client,
    struct wl_resource *resource, uint32_t id) {
    test_server *server = wl_resource_get_user_data(resource);
    server->text_input = wl_resource_create(client,
        &zwp_text_input_v1_interface, 1, id);
    CHECK(server->text_input);
    wl_resource_set_implementation(server->text_input,
        &text_implementation, server, NULL);
}

static const struct zwp_text_input_manager_v1_interface manager_implementation = {
    .create_text_input = create_text_input
};

static void bind_manager(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
    struct wl_resource *resource = wl_resource_create(client,
        &zwp_text_input_manager_v1_interface, version, id);
    CHECK(resource);
    wl_resource_set_implementation(resource,
        &manager_implementation, data, NULL);
}

static void *run_server(void *data) {
    wl_display_run(((test_server *)data)->display);
    return NULL;
}

int main(void) {
    test_server server = {0};
    char runtime[] = "/tmp/nexaui-wayland-v1-XXXXXX";
    CHECK(mkdtemp(runtime));
    CHECK(setenv("XDG_RUNTIME_DIR", runtime, 1) == 0);
    CHECK(setenv("NEXAUI_PLATFORM", "wayland", 1) == 0);
    server.display = wl_display_create();
    CHECK(server.display);
    CHECK(wl_global_create(server.display, &wl_compositor_interface, 1,
        &server, bind_compositor));
    CHECK(wl_global_create(server.display, &wl_shm_interface, 1,
        &server, bind_shm));
    CHECK(wl_global_create(server.display, &wl_seat_interface, 5,
        &server, bind_seat));
    CHECK(wl_global_create(server.display, &xdg_wm_base_interface, 1,
        &server, bind_wm));
    CHECK(wl_global_create(server.display,
        &zwp_text_input_manager_v1_interface, 1, &server, bind_manager));
    const char *socket_name = wl_display_add_socket_auto(server.display);
    CHECK(socket_name);
    char socket_path[256];
    CHECK(snprintf(socket_path, sizeof(socket_path), "%s/%s",
        runtime, socket_name) > 0);
    CHECK(setenv("WAYLAND_DISPLAY", socket_name, 1) == 0);
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, run_server, &server) == 0);

    nx_window_desc desc = {0};
    desc.width = 240;
    desc.height = 120;
    desc.flags = NX_WINDOW_HIDDEN;
    nx_window *window = NULL;
    CHECK(nx_window_create(&desc, &window) == NX_OK && window);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(nx_window_set_ime_rect(window, (nx_rect){12, 34, 2, 18}) == NX_OK);
    CHECK(nx_window_set_ime_enabled(window, 1) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.activation_count) == 1);
    CHECK(atomic_load(&server.caret_ok));
    size_t length = 0;
    int32_t begin = 0, end = 0;
    char preedit[5] = {0};
    CHECK(nx_window_get_ime_preedit(window, preedit, sizeof(preedit),
        &length, &begin, &end) == NX_OK);
    CHECK(length == 4 && begin == 2 && end == 2 &&
        strcmp(preedit, "na\xc3\xaf") == 0);
    nx_event event;
    int available = 0, preedit_events = 0;
    do {
        CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
        if (available && event.type == NX_EVENT_IME_PREEDIT_CHANGED)
            ++preedit_events;
    } while (available);
    CHECK(preedit_events == 1);

    CHECK(nx_window_set_ime_rect(window, (nx_rect){13, 34, 2, 18}) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(nx_window_get_ime_preedit(window, NULL, 0,
        &length, &begin, &end) == NX_OK && length == 0);
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    CHECK(available && event.type == NX_EVENT_IME_PREEDIT_CHANGED);
    const uint32_t expected[] = {'c', 'a', 'f', 0xe9};
    for (size_t i = 0; i < 4; ++i) {
        CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
        CHECK(available && event.type == NX_EVENT_TEXT &&
              event.scalar == expected[i]);
    }
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK &&
          !available);
    CHECK(nx_window_set_ime_rect(window, (nx_rect){14, 34, 2, 18}) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(nx_window_get_ime_preedit(window, NULL, 0,
        &length, &begin, &end) == NX_OK && length == 0);
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK &&
          !available);
    CHECK(nx_window_set_ime_surrounding_text(window, 91, 7,
        "ab\xc3\xa9" "cd", 6, 6, 6) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.surrounding_count) == 1);
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    CHECK(available && event.type == NX_EVENT_IME_DELETE_RANGE &&
          event.ime_target == 91 && event.ime_revision == 7 &&
          event.ime_index == -1 && event.ime_length == 1);
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    CHECK(available && event.type == NX_EVENT_TEXT && event.scalar == '!' &&
          event.ime_target == 91 && event.ime_revision == 7);
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK &&
          !available);
    CHECK(nx_window_set_ime_surrounding_text(window, 0, 0,
        NULL, 0, 0, 0) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.surrounding_count) == 2);
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK &&
          !available);
    CHECK(nx_window_set_ime_surrounding_text(window, 91, 8,
        "ab\xc3\xa9" "cd", 6, 4, 4) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.surrounding_count) == 3);
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK &&
          !available);
    CHECK(nx_window_set_ime_surrounding_text(window, 91, 9,
        "q", 1, 0, 0) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.surrounding_count) == 4);
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK &&
          !available);
    uint8_t font_bytes[512];
    font_fixture(font_bytes);
    nx_font *font = NULL;
    nx_font_collection *fonts = NULL;
    nx_text_field *field = NULL;
    nx_context *context = NULL;
    CHECK(nx_font_open(font_bytes, sizeof(font_bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &fonts) == NX_OK);
    CHECK(nx_font_collection_add(fonts, font) == NX_OK);
    CHECK(nx_text_field_create("abcd", 4, 1024, &field) == NX_OK);
    CHECK(nx_context_create(NULL, &context) == NX_OK);
    nx_text_field_desc field_desc = {0};
    field_desc.id = 81;
    field_desc.bounds = (nx_rect){0, 0, 120, 28};
    field_desc.name = (nx_utf8_span){"Editor", 6};
    field_desc.fonts = fonts;
    field_desc.pixels_per_em = 16;
    field_desc.glyph_cache_bytes = 4096;
    field_desc.ime_window = window;
    nx_interaction_state interaction;
    int valid = 0, submitted = 0;
    CHECK(nx_begin_frame(context, 160, 60, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &field_desc, field, &interaction,
        &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_focus_set(context, 81) == NX_OK);
    CHECK(nx_begin_frame(context, 160, 60, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &field_desc, field, &interaction,
        &valid, &submitted) == NX_OK && interaction.focused);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.surrounding_count) == 5);
    size_t routed = 0;
    CHECK(nx_window_route_events(window, context, &routed) == NX_OK);
    CHECK(routed == 2);
    CHECK(nx_begin_frame(context, 160, 60, 0) == NX_OK);
    const nx_event *frame_events = NULL;
    size_t frame_event_count = 0;
    CHECK(nx_frame_events(context, &frame_events, &frame_event_count) == NX_OK);
    CHECK(frame_event_count == 2 &&
          frame_events[0].type == NX_EVENT_IME_DELETE_RANGE &&
          frame_events[1].type == NX_EVENT_TEXT);
    nx_event stale_delete = frame_events[0], stale_commit = frame_events[1];
    CHECK(nx_text_field_widget(context, &field_desc, field, &interaction,
        &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(context) == NX_OK);
    nx_utf8_span draft;
    CHECK(nx_text_field_text(field, &draft) == NX_OK);
    CHECK(draft.length == 4 && memcmp(draft.data, "abc!", 4) == 0);
    CHECK(nx_context_queue_event(context, &stale_delete) == NX_OK);
    CHECK(nx_context_queue_event(context, &stale_commit) == NX_OK);
    CHECK(nx_begin_frame(context, 160, 60, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &field_desc, field, &interaction,
        &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_text_field_text(field, &draft) == NX_OK);
    CHECK(draft.length == 4 && memcmp(draft.data, "abc!", 4) == 0);
    nx_context_destroy(context);
    nx_text_field_destroy(field);
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    CHECK(nx_window_set_ime_enabled(window, 0) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.deactivation_count) == 1);
    CHECK(atomic_load(&server.normal_content) > 0);
    CHECK(nx_window_set_ime_purpose(window, NX_IME_PURPOSE_PASSWORD) == NX_OK);
    CHECK(nx_window_set_ime_surrounding_text(window, 92, 1,
        "secret", 6, 6, 6) == NX_ERROR_ARGUMENT);
    CHECK(nx_window_set_ime_surrounding_text(window, 92, 1,
        "", 0, 0, 0) == NX_OK);
    CHECK(nx_window_set_ime_enabled(window, 1) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.password_content) > 0 &&
          server.surrounding[0] == '\0');
    int normal_before = atomic_load(&server.normal_content);
    CHECK(nx_window_set_ime_purpose(window, NX_IME_PURPOSE_NORMAL) == NX_OK);
    CHECK(nx_window_set_ime_surrounding_text(window, 93, 2,
        "abc", 3, 3, 3) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.normal_content) > normal_before &&
          strcmp(server.surrounding, "abc") == 0);
    CHECK(nx_window_set_ime_enabled(window, 0) == NX_OK);
    nx_window_destroy(window);
    wl_display_terminate(server.display);
    CHECK(pthread_join(thread, NULL) == 0);
    wl_display_destroy(server.display);
    CHECK(unlink(socket_path) == 0 || access(socket_path, F_OK) != 0);
    CHECK(rmdir(runtime) == 0);
    return 0;
}
