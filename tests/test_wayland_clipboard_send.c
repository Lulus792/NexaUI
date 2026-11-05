#define _GNU_SOURCE
#include "wayland_v3_fixture.h"
#include <errno.h>
#include <fcntl.h>
#include <time.h>

static int readers[8];
static atomic_int batches;

static double seconds(void) {
    struct timespec t; CHECK(clock_gettime(CLOCK_MONOTONIC, &t) == 0);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static void request_selection(test_server *server, struct wl_resource *source) {
    (void)server;
    if (!source) return;
    int batch = atomic_load(&batches);
    CHECK(batch < 2);
    for (int i = 0; i < 4; ++i) {
        int fds[2]; CHECK(pipe2(fds, O_CLOEXEC | O_NONBLOCK) == 0);
        readers[batch * 4 + i] = fds[0];
        wl_data_source_send_send(source, "text/plain;charset=utf-8", fds[1]);
        close(fds[1]);
    }
    atomic_store(&batches, batch + 1);
}

static void poll_window(nx_window *window, int many, uint32_t timeout) {
    if (many) {
        nx_window *windows[] = {window}; uint32_t events = 0;
        CHECK(nx_window_poll_many(windows, 1, timeout, -1, &events) == NX_OK);
    } else CHECK(nx_window_poll(window, timeout) == NX_OK);
}

static void drain_events(nx_window *window) {
    nx_event event; int available;
    do { CHECK(nx_window_next_event(window, &event, &available) == NX_OK); }
    while (available);
}

int main(int argc, char **argv) {
    CHECK(argc == 1 || (argc == 2 && !strcmp(argv[1], "--many")));
    int many = argc == 2;
    test_server server = {0}; char runtime[] = "/tmp/nexaui-send-XXXXXX";
    server.keyboard_enabled = 1; server.pointer_enabled = 1;
    server.selection_changed = request_selection;
    CHECK(mkdtemp(runtime) && setenv("XDG_RUNTIME_DIR", runtime, 1) == 0);
    server.display = wl_display_create(); CHECK(server.display);
    CHECK(wl_global_create(server.display, &wl_compositor_interface, 1, &server, bind_compositor));
    CHECK(wl_display_init_shm(server.display) == 0);
    CHECK(wl_global_create(server.display, &wl_seat_interface, 5, &server, bind_seat));
    CHECK(wl_global_create(server.display, &wl_data_device_manager_interface, 3, &server, bind_data_manager));
    CHECK(wl_global_create(server.display, &xdg_wm_base_interface, 1, &server, bind_wm));
    const char *name = wl_display_add_socket_auto(server.display); CHECK(name);
    char path[256]; CHECK(snprintf(path, sizeof(path), "%s/%s", runtime, name) > 0);
    CHECK(setenv("WAYLAND_DISPLAY", name, 1) == 0); unsetenv("WAYLAND_SOCKET");
    pthread_t thread; CHECK(pthread_create(&thread, NULL, run_server, &server) == 0);
    window_heap allocations = {0};
    nx_window_desc desc = {"clipboard sender", 240, 120, NX_WINDOW_HIDDEN,
                           {&allocations, window_allocate, window_free}};
    nx_window *window = NULL; CHECK(nx_window_create(&desc, &window) == NX_OK);
    atomic_store(&server.keyboard_enter_requested, 1);
    atomic_store(&server.pointer_press_requested, 1);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    size_t length = 1024u * 1024u;
    char *text = malloc(length); CHECK(text); memset(text, 'x', length);
    CHECK(nx_window_clipboard_set_text(window, text, length) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&batches) == 1);
    double start = seconds();
    poll_window(window, many, 0); drain_events(window);
    /* Progress midway must not extend the total transfer deadline. */
    struct timespec delay = {2, 0}; CHECK(nanosleep(&delay, NULL) == 0);
    char buffer[8192];
    for (int i = 0; i < 4; ++i) CHECK(read(readers[i], buffer, sizeof(buffer)) > 0);
    poll_window(window, many, 0); drain_events(window);
    /* Do not drain again until the fixed deadline has woken the owner. */
    while (seconds() - start < 4.9) {
        poll_window(window, many, 7000); drain_events(window);
    }
    CHECK(seconds() - start >= 4.0 && seconds() - start < 7.0);
    for (int i = 0; i < 4; ++i) {
        ssize_t n;
        do { n = read(readers[i], buffer, sizeof(buffer)); } while (n > 0);
        CHECK(n == 0 && close(readers[i]) == 0);
    }
    CHECK(nx_window_clipboard_set_text(window, text, length) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&batches) == 2);
    poll_window(window, many, 0);
    for (int i = 4; i < 8; ++i) {
        CHECK(read(readers[i], buffer, sizeof(buffer)) > 0);
        CHECK(close(readers[i]) == 0);
    }
    free(text); nx_window_destroy(window); CHECK(allocations.live == 0);
    wl_display_terminate(server.display); CHECK(pthread_join(thread, NULL) == 0);
    wl_display_destroy(server.display);
    CHECK(unlink(path) == 0 || access(path, F_OK) != 0); CHECK(rmdir(runtime) == 0);
    puts("Stalled clipboard sends expire and free slots; progress does not extend the deadline");
    return 0;
}
