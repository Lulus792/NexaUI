#include "wayland_v3_fixture.h"

/* The allocator sees still-owned storage immediately before free. Identify a
   copy while its original bytes are live, then require the entire allocation,
   including its terminator, to be overwritten when the protocol retires it. */
typedef struct storage_allocation {
    void *pointer;
    size_t bytes;
    int watched;
} storage_allocation;

typedef struct storage_heap {
    window_heap base;
    storage_allocation allocations[256];
    size_t wiped;
} storage_heap;

static void *storage_allocate(void *user, size_t bytes) {
    storage_heap *heap = user;
    void *pointer = window_allocate(&heap->base, bytes);
    if (!pointer) return NULL;
    for (size_t i = 0; i < 256; ++i) {
        if (!heap->allocations[i].pointer) {
            heap->allocations[i] = (storage_allocation){pointer, bytes, 0};
            return pointer;
        }
    }
    CHECK(0 && "fixture allocation table exhausted");
    return NULL;
}

static void storage_free(void *user, void *pointer, size_t bytes) {
    storage_heap *heap = user;
    for (size_t i = 0; i < 256; ++i) {
        storage_allocation *allocation = &heap->allocations[i];
        if (allocation->pointer != pointer) continue;
        CHECK(allocation->bytes == bytes);
        if (allocation->watched) {
            const unsigned char *data = pointer;
            for (size_t j = 0; j < bytes; ++j)
                CHECK(data[j] == 0 && "retired password IME allocation must be overwritten");
            ++heap->wiped;
        }
        *allocation = (storage_allocation){0};
        window_free(&heap->base, pointer, bytes);
        return;
    }
    CHECK(0 && "unknown fixture allocation");
}

static void watch(storage_heap *heap, const char *text) {
    size_t bytes = strlen(text) + 1u, found = 0;
    for (size_t i = 0; i < 256; ++i) {
        storage_allocation *allocation = &heap->allocations[i];
        if (allocation->pointer && allocation->bytes == bytes &&
            !memcmp(allocation->pointer, text, bytes)) {
            CHECK(!allocation->watched);
            allocation->watched = 1;
            ++found;
        }
    }
    CHECK(found == 1);
}

static const char first[] = "synthetic-preedit-\xc3\xa9-one";
static const char second[] = "synthetic-preedit-\xc3\xa1-two";
static const char committed[] = "synthetic-commit-\xc3\xbc";

static void text_committed(test_server *server, struct wl_resource *input) {
    (void)input;
    if (server->enabled) atomic_fetch_add(&server->enable_commits, 1);
}

static void produce(test_server *server, struct wl_client *client) {
    (void)client;
    int command = atomic_exchange(&server->numeric_commit, 0);
    struct wl_resource *input = server->text_input;
    if (!command) return;
    CHECK(input);
    switch (command) {
    case 1: zwp_text_input_v3_send_preedit_string(input, first, 0, 0); break;
    case 2: zwp_text_input_v3_send_preedit_string(input, second, 0, 0); break;
    case 3: zwp_text_input_v3_send_preedit_string(input, NULL, 0, 0); break;
    case 4: zwp_text_input_v3_send_preedit_string(input, "\xc0", 0, 0); break;
    case 5: zwp_text_input_v3_send_commit_string(input, committed); break;
    case 6: zwp_text_input_v3_send_done(input, server->commit_serial); break;
    case 7: zwp_text_input_v3_send_done(input, server->commit_serial - 1u); break;
    case 8: zwp_text_input_v3_send_leave(input, server->surface); break;
    case 9: zwp_text_input_v3_send_enter(input, server->surface); break;
    default: CHECK(0 && "unknown fixture command");
    }
}

static void send(test_server *server, nx_window *window, int command) {
    atomic_store(&server->numeric_commit, command);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
}

static void expect_preedit(nx_window *window, const char *expected) {
    char bytes[128];
    size_t length = 0;
    int32_t begin = -1, end = -1;
    CHECK(nx_window_get_ime_preedit(window, bytes, sizeof(bytes), &length,
                                  &begin, &end) == NX_OK);
    CHECK(length == strlen(expected) && !memcmp(bytes, expected, length + 1u));
    CHECK(begin == 0 && end == 0);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    int replace = !strcmp(argv[1], "replace");
    int empty = !strcmp(argv[1], "empty");
    int invalid = !strcmp(argv[1], "invalid");
    int lifecycle = !strcmp(argv[1], "lifecycle");
    CHECK(replace || empty || invalid || lifecycle);
    test_server server = {0};
    server.text_committed = text_committed;
    server.surface_committed = produce;
    char runtime[] = "/tmp/nexaui-ime-storage-XXXXXX";
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
    storage_heap heap = {0};
    nx_window_desc desc = {0};
    desc.width = 240;
    desc.height = 120;
    desc.flags = NX_WINDOW_HIDDEN;
    desc.allocator = (nx_allocator){&heap, storage_allocate, storage_free};
    nx_window *window = NULL;
    CHECK(nx_window_create(&desc, &window) == NX_OK);
    CHECK(nx_window_set_ime_purpose(window, NX_IME_PURPOSE_PASSWORD) == NX_OK);
    CHECK(nx_window_set_ime_enabled(window, 1) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.password_content) > 0);
    send(&server, window, 1);
    watch(&heap, first);
    expect_preedit(window, ""); /* A pending wire reply is not yet visible. */
    if (replace) {
        send(&server, window, 2);
        CHECK(heap.wiped == 1);
        watch(&heap, second);
        send(&server, window, 6);
        CHECK(heap.wiped == 1); /* done transfers the still-live allocation. */
        expect_preedit(window, second);
        send(&server, window, 3);
        send(&server, window, 6);
        CHECK(heap.wiped == 2);
        expect_preedit(window, "");
    } else if (empty || invalid) {
        send(&server, window, invalid ? 4 : 3);
        CHECK(heap.wiped == 1);
        expect_preedit(window, "");
        if (invalid) {
            nx_event event;
            int available = 1;
            CHECK(nx_window_next_event(window, &event, &available) == NX_ERROR_PLATFORM);
            CHECK(!available);
        }
    } else {
        send(&server, window, 6);
        CHECK(heap.wiped == 0);
        expect_preedit(window, first);
        send(&server, window, 2);
        watch(&heap, second);
        send(&server, window, 5);
        watch(&heap, committed);
        send(&server, window, 8);
        CHECK(heap.wiped == 3);
        expect_preedit(window, "");
        send(&server, window, 9);
        CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
        send(&server, window, 1);
        watch(&heap, first);
        send(&server, window, 7);
        CHECK(heap.wiped == 4);
        send(&server, window, 1);
        watch(&heap, first);
        CHECK(nx_window_set_ime_purpose(window, NX_IME_PURPOSE_NORMAL) == NX_OK);
        CHECK(heap.wiped == 5);
        CHECK(nx_window_set_ime_purpose(window, NX_IME_PURPOSE_PASSWORD) == NX_OK);
        CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
        send(&server, window, 1);
        watch(&heap, first);
        send(&server, window, 5);
        watch(&heap, committed);
        CHECK(nx_window_set_ime_enabled(window, 0) == NX_OK);
        CHECK(heap.wiped == 7);
        CHECK(nx_window_set_ime_enabled(window, 1) == NX_OK);
        CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
        send(&server, window, 1);
        watch(&heap, first);
        send(&server, window, 5);
        watch(&heap, committed);
    }
    nx_window_destroy(window);
    CHECK(!heap.base.live);
    if (lifecycle) CHECK(heap.wiped == 9);
    wl_display_terminate(server.display);
    CHECK(pthread_join(thread, NULL) == 0);
    wl_display_destroy(server.display);
    CHECK(unlink(socket_path) == 0 || access(socket_path, F_OK) != 0);
    CHECK(rmdir(runtime) == 0);
    printf("Password IME storage: %s passed; %zu whole allocations overwritten\n",
           argv[1], heap.wiped);
    return 0;
}
