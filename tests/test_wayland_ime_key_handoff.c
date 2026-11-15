#include "wayland_v3_fixture.h"
#include <xkbcommon/xkbcommon.h>

static void keymap(struct wl_resource *keyboard) {
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    CHECK(context);
    struct xkb_rule_names names = {.layout = "us"};
    struct xkb_keymap *map = xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    CHECK(map);
    char *text = xkb_keymap_get_as_string(map, XKB_KEYMAP_FORMAT_TEXT_V1);
    CHECK(text);
    size_t length = strlen(text) + 1u;
    CHECK(length <= UINT32_MAX);
    char path[] = "/tmp/nexaui-handoff-keymap-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0 && unlink(path) == 0);
    CHECK(write(fd, text, length) == (ssize_t)length);
    wl_keyboard_send_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, (uint32_t)length);
    close(fd);
    free(text);
    xkb_keymap_unref(map);
    xkb_context_unref(context);
}

static void text_committed(test_server *server, struct wl_resource *input) {
    (void)input;
    if (server->enabled) atomic_fetch_add(&server->enable_commits, 1);
}

static void produce(test_server *server, struct wl_client *client) {
    (void)client;
    int command = atomic_exchange(&server->numeric_commit, 0);
    if (!command) return;
    CHECK(server->keyboard);
    if (command == 1) {
        struct wl_array keys;
        wl_array_init(&keys);
        wl_keyboard_send_enter(server->keyboard, 1, server->surface, &keys);
        wl_keyboard_send_modifiers(server->keyboard, 2, 0, 0, 0, 0);
        wl_keyboard_send_repeat_info(server->keyboard, 0, 0);
        wl_array_release(&keys);
    } else {
        CHECK(command == 2 || command == 3);
        wl_keyboard_send_key(server->keyboard, 3, 0, KEY_TAB,
            command == 2 ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);
    }
}

static void send(test_server *server, nx_window *window, int command) {
    atomic_store(&server->numeric_commit, command);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
}

static void take(nx_window *window, nx_event_type type, int repeat) {
    nx_event event;
    int available = 0;
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK && available);
    CHECK(event.type == type && event.key == NX_KEY_TAB && event.repeat == repeat);
}

static void empty(nx_window *window) {
    nx_event event;
    int available = 1;
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK && !available);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    int activation = !strcmp(argv[1], "activation");
    int deactivation = !strcmp(argv[1], "deactivation");
    int forwarded = !strcmp(argv[1], "forwarded");
    int normal = !strcmp(argv[1], "normal");
    CHECK(activation || deactivation || forwarded || normal);
    test_server server = {0};
    server.keyboard_enabled = 1;
    server.keyboard_bound = keymap;
    server.text_committed = text_committed;
    server.surface_committed = produce;
    char runtime[] = "/tmp/nexaui-ime-handoff-XXXXXX";
    CHECK(mkdtemp(runtime));
    CHECK(setenv("XDG_RUNTIME_DIR", runtime, 1) == 0);
    CHECK(setenv("NEXAUI_PLATFORM", "wayland", 1) == 0);
    server.display = wl_display_create();
    CHECK(server.display);
    CHECK(wl_global_create(server.display, &wl_compositor_interface, 1, &server, bind_compositor));
    CHECK(wl_global_create(server.display, &wl_shm_interface, 1, &server, bind_shm));
    CHECK(wl_global_create(server.display, &wl_seat_interface, 5, &server, bind_seat));
    CHECK(wl_global_create(server.display, &wl_data_device_manager_interface, 3, &server, bind_data_manager));
    CHECK(wl_global_create(server.display, &xdg_wm_base_interface, 1, &server, bind_wm));
    CHECK(wl_global_create(server.display, &zwp_text_input_manager_v3_interface, 1, &server, bind_manager));
    const char *socket_name = wl_display_add_socket_auto(server.display);
    CHECK(socket_name);
    char socket_path[256];
    CHECK(snprintf(socket_path, sizeof(socket_path), "%s/%s", runtime, socket_name) > 0);
    CHECK(setenv("WAYLAND_DISPLAY", socket_name, 1) == 0);
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, run_server, &server) == 0);
    window_heap heap = {0};
    nx_window_desc desc = {0};
    desc.width = 240;
    desc.height = 120;
    desc.flags = NX_WINDOW_HIDDEN;
    desc.allocator = (nx_allocator){&heap, window_allocate, window_free};
    nx_window *window = NULL;
    CHECK(nx_window_create(&desc, &window) == NX_OK);
    send(&server, window, 1);
    nx_event event;
    int available = 0;
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK && available && event.type == NX_EVENT_FOCUS);
    empty(window);
    if (deactivation) {
        CHECK(nx_window_set_ime_enabled(window, 1) == NX_OK);
        CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    }
    send(&server, window, 2);
    take(window, NX_EVENT_KEY_DOWN, 0);
    empty(window);
    if (!normal) {
        CHECK(nx_window_set_ime_enabled(window, !deactivation) == NX_OK);
        CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
        empty(window); /* A mode transition alone does not release a held key. */
    }
    if (forwarded) {
        send(&server, window, 3);
        take(window, NX_EVENT_KEY_UP, 0);
        empty(window);
    }
    /* The nonconforming missing release reproduces the observed provider
       handoff. Only the following real press reconciles that stale state. */
    send(&server, window, 2);
    if (activation || deactivation) take(window, NX_EVENT_KEY_UP, 0);
    take(window, NX_EVENT_KEY_DOWN, normal);
    empty(window);
    send(&server, window, 3);
    take(window, NX_EVENT_KEY_UP, 0);
    empty(window);
    nx_window_destroy(window);
    CHECK(!heap.live);
    wl_display_terminate(server.display);
    CHECK(pthread_join(thread, NULL) == 0);
    wl_display_destroy(server.display);
    CHECK(unlink(socket_path) == 0 || access(socket_path, F_OK) != 0);
    CHECK(rmdir(runtime) == 0);
    printf("IME key handoff: %s passed; fresh press and forwarded releases preserved\n", argv[1]);
    return 0;
}
