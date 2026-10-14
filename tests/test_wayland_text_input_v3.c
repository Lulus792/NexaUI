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
    int configured, enabled, pending_done;
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

static void surface_commit(struct wl_client *client,
                           struct wl_resource *resource) {
    test_server *server = wl_resource_get_user_data(resource);
    (void)client;
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
    xdg_toplevel_send_configure(server->toplevel, 240, 120, &states);
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

static void *run_server(void *data) {
    wl_display_run(((test_server *)data)->display);
    return NULL;
}

int main(void) {
    test_server server = {0};
    char runtime[] = "/tmp/nexaui-wayland-v3-XXXXXX";
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
                           &zwp_text_input_manager_v3_interface, 1,
                           &server, bind_manager));
    const char *socket_name = wl_display_add_socket_auto(server.display);
    CHECK(socket_name);
    char socket_path[256];
    CHECK(snprintf(socket_path, sizeof(socket_path), "%s/%s", runtime,
                   socket_name) > 0);
    CHECK(setenv("WAYLAND_DISPLAY", socket_name, 1) == 0);
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, run_server, &server) == 0);

    nx_window_desc desc = {0};
    desc.width = 240;
    desc.height = 120;
    desc.flags = NX_WINDOW_HIDDEN;
    nx_window *window = NULL;
    CHECK(nx_window_create(&desc, &window) == NX_OK && window);
    CHECK(nx_window_set_ime_rect(window, (nx_rect){12, 34, 2, 18}) == NX_OK);
    CHECK(nx_window_set_ime_enabled(window, 1) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.enable_commits) == 1);
    CHECK(atomic_load(&server.caret_ok) == 1);
    size_t preedit_length = 99;
    int32_t preedit_begin = -2, preedit_end = -2;
    CHECK(nx_window_get_ime_preedit(window, NULL, 0, &preedit_length,
        &preedit_begin, &preedit_end) == NX_OK);
    CHECK(preedit_length == 0 && preedit_begin == 0 && preedit_end == 0);
    nx_event event;
    int available = 0;
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    CHECK(!available);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(nx_window_get_ime_preedit(window, NULL, 0, &preedit_length,
        &preedit_begin, &preedit_end) == NX_OK);
    CHECK(preedit_length == 4 && preedit_begin == 2 && preedit_end == 2);
    char preedit[5] = {0};
    CHECK(nx_window_get_ime_preedit(window, preedit, 4, &preedit_length,
        &preedit_begin, &preedit_end) == NX_ERROR_LIMIT);
    CHECK(nx_window_get_ime_preedit(window, preedit, sizeof(preedit),
        &preedit_length, &preedit_begin, &preedit_end) == NX_OK);
    CHECK(strcmp(preedit, "na\xc3\xaf") == 0);
    const uint32_t expected[] = {'c', 'a', 'f', 0xe9};
    for (size_t i = 0; i < 4; ++i) {
        available = 0;
        CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
        CHECK(available && event.type == NX_EVENT_TEXT &&
              event.scalar == expected[i]);
    }
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    CHECK(available && event.type == NX_EVENT_IME_PREEDIT_CHANGED);
    uint8_t font_bytes[512];
    font_fixture(font_bytes);
    nx_font *font = NULL;
    nx_font_collection *fonts = NULL;
    nx_text_field *field = NULL;
    nx_context *context = NULL;
    CHECK(nx_font_open(font_bytes, sizeof(font_bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &fonts) == NX_OK);
    CHECK(nx_font_collection_add(fonts, font) == NX_OK);
    CHECK(nx_text_field_create("x", 1, 1024, &field) == NX_OK);
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
    const nx_command *commands = NULL;
    size_t command_count = 0, line_count = 0;
    CHECK(nx_frame_commands(context, &commands, &command_count) == NX_OK);
    for (size_t i = 0; i < command_count; ++i)
        if (commands[i].type == NX_COMMAND_LINE) ++line_count;
    CHECK(line_count >= 2); /* Composition underline and insertion caret. */
    nx_utf8_span draft;
    CHECK(nx_text_field_text(field, &draft) == NX_OK);
    CHECK(draft.length == 1 && draft.data[0] == 'x');
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(nx_window_get_ime_preedit(window, NULL, 0, &preedit_length,
        &preedit_begin, &preedit_end) == NX_OK);
    CHECK(preedit_length == 4 && preedit_begin == -1 && preedit_end == -1);
    size_t routed = 0;
    CHECK(nx_window_route_events(window, context, &routed) == NX_OK);
    CHECK(routed >= 1);
    CHECK(nx_begin_frame(context, 160, 60, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &field_desc, field, &interaction,
        &valid, &submitted) == NX_OK && interaction.focused);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_frame_commands(context, &commands, &command_count) == NX_OK);
    line_count = 0;
    for (size_t i = 0; i < command_count; ++i)
        if (commands[i].type == NX_COMMAND_LINE) ++line_count;
    CHECK(line_count == 1); /* The IME hid the caret, not the composition. */
    CHECK(nx_text_field_set_text(field, "abcd", 4) == NX_OK);
    CHECK(nx_begin_frame(context, 160, 60, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &field_desc, field, &interaction,
        &valid, &submitted) == NX_OK && interaction.focused);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.field_surrounding_ok));
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(nx_window_route_events(window, context, &routed) == NX_OK);
    CHECK(routed >= 2);
    CHECK(nx_begin_frame(context, 160, 60, 0) == NX_OK);
    const nx_event *frame_events = NULL;
    size_t frame_event_count = 0;
    CHECK(nx_frame_events(context, &frame_events, &frame_event_count) == NX_OK);
    nx_event stale_delete = {0}, stale_commit = {0};
    for (size_t i = 0; i < frame_event_count; ++i) {
        if (frame_events[i].type == NX_EVENT_IME_DELETE_SURROUNDING)
            stale_delete = frame_events[i];
        if (frame_events[i].type == NX_EVENT_TEXT &&
            frame_events[i].ime_target == 81)
            stale_commit = frame_events[i];
    }
    CHECK(stale_delete.ime_target == 81 && stale_commit.ime_target == 81);
    CHECK(nx_text_field_widget(context, &field_desc, field, &interaction,
        &valid, &submitted) == NX_OK && interaction.focused);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_text_field_text(field, &draft) == NX_OK);
    CHECK(draft.length == 4 && memcmp(draft.data, "abc!", 4) == 0);
    CHECK(nx_context_queue_event(context, &stale_delete) == NX_OK);
    CHECK(nx_context_queue_event(context, &stale_commit) == NX_OK);
    CHECK(nx_begin_frame(context, 160, 60, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &field_desc, field, &interaction,
        &valid, &submitted) == NX_OK && interaction.focused);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_text_field_text(field, &draft) == NX_OK);
    CHECK(draft.length == 4 && memcmp(draft.data, "abc!", 4) == 0);
    CHECK(nx_window_set_ime_surrounding_text(window, 91, 7,
        "ab\xc3\xa9" "cd", 6, 6, 6) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.surrounding_ok));
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    if (available && event.type == NX_EVENT_IME_PREEDIT_CHANGED)
        CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    CHECK(available && event.type == NX_EVENT_IME_DELETE_SURROUNDING &&
          event.ime_target == 91 && event.ime_revision == 7 &&
          event.ime_before == 1 && event.ime_after == 0);
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    CHECK(available && event.type == NX_EVENT_TEXT && event.scalar == '!' &&
          event.ime_target == 91 && event.ime_revision == 7);
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    CHECK(!available);
    CHECK(nx_window_set_ime_surrounding_text(window, 91, 7,
        "ab\xc3\xa9" "cd", 6, 3, 6) == NX_ERROR_ARGUMENT);
    CHECK(nx_window_set_ime_surrounding_text(window, 0, 0,
        NULL, 0, 0, 0) == NX_OK);
    nx_context_destroy(context);
    nx_text_field_destroy(field);
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    CHECK(nx_window_set_ime_enabled(window, 0) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.disable_commits) == 1);
    CHECK(nx_window_get_ime_preedit(window, NULL, 0, &preedit_length,
        &preedit_begin, &preedit_end) == NX_OK && preedit_length == 0);
    CHECK(atomic_load(&server.normal_content) > 0);
    CHECK(nx_window_set_ime_purpose(window, NX_IME_PURPOSE_PASSWORD) == NX_OK);
    CHECK(nx_window_set_ime_surrounding_text(window, 92, 1,
        "secret", 6, 6, 6) == NX_ERROR_ARGUMENT);
    CHECK(nx_window_set_ime_surrounding_text(window, 92, 1,
        "", 0, 0, 0) == NX_OK);
    CHECK(nx_window_set_ime_enabled(window, 1) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.password_content) > 0 &&
          server.surrounding_length == 0);
    int normal_before = atomic_load(&server.normal_content);
    CHECK(nx_window_set_ime_purpose(window, NX_IME_PURPOSE_NORMAL) == NX_OK);
    CHECK(nx_window_set_ime_surrounding_text(window, 93, 2,
        "abc", 3, 3, 3) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.normal_content) > normal_before &&
          server.surrounding_length == 3 &&
          memcmp(server.surrounding, "abc", 3) == 0);
    CHECK(nx_window_set_ime_enabled(window, 0) == NX_OK);
    nx_window_destroy(window);
    wl_display_terminate(server.display);
    CHECK(pthread_join(thread, NULL) == 0);
    wl_display_destroy(server.display);
    CHECK(unlink(socket_path) == 0 || access(socket_path, F_OK) != 0);
    CHECK(rmdir(runtime) == 0);
    return 0;
}
