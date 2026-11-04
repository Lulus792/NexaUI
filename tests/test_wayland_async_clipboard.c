#include "wayland_v3_fixture.h"
#include "text_value.h"
#include <time.h>

static double now_seconds(void) {
    struct timespec t; CHECK(clock_gettime(CLOCK_MONOTONIC, &t) == 0);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
static void publish_offer(test_server *server, nx_window *window) {
    atomic_store(&server->send_offer, 1);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
}
static void close_peer(test_server *server) {
    int fd = atomic_exchange(&server->stalled_fd, 0);
    CHECK(fd > 0 && close(fd - 1) == 0);
}

int main(int argc, char **argv) {
    CHECK(argc==1 || (argc==2 && (!strcmp(argv[1],"--focus-loss") || !strcmp(argv[1],"--keyboard-removed"))));
    int focus_loss=argc==2;
    int keyboard_removed=argc==2 && !strcmp(argv[1],"--keyboard-removed");
    test_server server = {0}; char runtime[] = "/tmp/nexaui-async-XXXXXX";
    server.keyboard_enabled=focus_loss;
    CHECK(mkdtemp(runtime) && setenv("XDG_RUNTIME_DIR", runtime, 1) == 0);
    server.display = wl_display_create(); CHECK(server.display);
    CHECK(wl_global_create(server.display, &wl_compositor_interface, 1, &server, bind_compositor));
    CHECK(wl_global_create(server.display, &wl_shm_interface, 1, &server, bind_shm));
    CHECK(wl_global_create(server.display, &wl_seat_interface, 5, &server, bind_seat));
    CHECK(wl_global_create(server.display, &wl_data_device_manager_interface, 3, &server, bind_data_manager));
    CHECK(wl_global_create(server.display, &xdg_wm_base_interface, 1, &server, bind_wm));
    CHECK(wl_global_create(server.display, &zwp_text_input_manager_v3_interface, 1, &server, bind_manager));
    const char *name = wl_display_add_socket_auto(server.display); CHECK(name);
    char path[256]; CHECK(snprintf(path, sizeof(path), "%s/%s", runtime, name) > 0);
    CHECK(setenv("WAYLAND_DISPLAY", name, 1) == 0); unsetenv("WAYLAND_SOCKET");
    pthread_t thread; CHECK(pthread_create(&thread, NULL, run_server, &server) == 0);
    window_heap allocations = {0};
    nx_window_desc desc = {"async clipboard", 240, 120, NX_WINDOW_HIDDEN,
                           {&allocations, window_allocate, window_free}};
    nx_window *window = NULL; CHECK(nx_window_create(&desc, &window) == NX_OK);
    if(focus_loss) {
        atomic_store(&server.keyboard_enter_requested,1);publish_offer(&server,window);
        atomic_store(&server.keyboard_leave_requested,1);
        wl_surface_commit(nx_window_wayland_surface(window));
        CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
        uint64_t token=0;size_t length=0;
        CHECK(nx_linux_window_clipboard_async_start(window,1024,2000,0,&token)==NX_ERROR_STATE);
        CHECK(nx_window_clipboard_get_text(window,NULL,0,&length)==NX_ERROR_STATE);
        CHECK(atomic_load(&server.offer_reads)==0);
        atomic_store(&server.keyboard_enter_requested,1);publish_offer(&server,window);
        atomic_store(&server.offer_stall,1);
        CHECK(nx_linux_window_clipboard_async_start(window,1024,2000,0,&token)==NX_OK);
        for(unsigned i=0;i<20 && !atomic_load(&server.stalled_fd);++i)CHECK(nx_window_poll(window,100)==NX_OK);
        CHECK(atomic_load(&server.stalled_fd));
        nx_event event;int event_available=0;
        do {CHECK(nx_window_next_event(window,&event,&event_available)==NX_OK);} while(event_available);
        if(keyboard_removed)atomic_store(&server.keyboard_remove_requested,1);
        else atomic_store(&server.keyboard_leave_requested,1);
        wl_surface_commit(nx_window_wayland_surface(window));
        CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
        CHECK(!nx_linux_window_clipboard_async_current(window,token));
        CHECK(nx_linux_window_clipboard_async_start(window,1024,2000,0,&token)==NX_ERROR_STATE);
        int saw_focus_loss=0;
        do {CHECK(nx_window_next_event(window,&event,&event_available)==NX_OK);
            if(event_available && event.type==NX_EVENT_FOCUS && !event.focused)saw_focus_loss=1;
        } while(event_available);
        CHECK(saw_focus_loss);
        close_peer(&server);
        if(!keyboard_removed) {
            atomic_store(&server.offer_stall,0);atomic_store(&server.keyboard_enter_requested,1);publish_offer(&server,window);
            CHECK(nx_linux_window_clipboard_async_start(window,1024,2000,0,&token)==NX_OK);
            char *bytes=NULL;int ready=0;
            for(unsigned i=0;i<20 && !ready;++i) {
                CHECK(nx_window_poll(window,100)==NX_OK);
                CHECK(nx_linux_window_clipboard_async_take(window,token,&bytes,&length,&ready)==NX_OK);
            }
            CHECK(ready && length==4 && !memcmp(bytes,"Once",4));free(bytes);
        }
        nx_window_destroy(window);CHECK(allocations.live==0);
        wl_display_terminate(server.display);CHECK(pthread_join(thread,NULL)==0);wl_display_destroy(server.display);
        CHECK(unlink(path)==0 || access(path,F_OK)!=0);CHECK(rmdir(runtime)==0);
        puts("Wayland selection invalidated on focus loss and restored on reentry");return 0;
    }
    atomic_store(&server.offer_stall, 1); publish_offer(&server, window);
    uint64_t first = 0, other = 0; char *bytes = NULL; size_t length = 0; int ready = 0;
    CHECK(nx_linux_window_clipboard_async_start(window, 1024, 2000, 0, &first) == NX_OK && first);
    CHECK(nx_linux_window_clipboard_async_start(window, 1024, 2000, 0, &other) == NX_ERROR_LIMIT);
    CHECK(nx_linux_window_clipboard_async_take(window, first, &bytes, &length, &ready) == NX_OK && !ready);
    double begin = now_seconds();
    for (unsigned i = 0; i < 20 && !atomic_load(&server.stalled_fd); ++i)
        CHECK(nx_window_poll(window, 100) == NX_OK);
    CHECK(atomic_load(&server.stalled_fd) && now_seconds() - begin < 1.5);
    CHECK(nx_linux_window_clipboard_async_take(window, first, &bytes, &length, &ready) == NX_OK && !ready);
    /* Native protocol work still progresses while the clipboard owner stalls. */
    CHECK(nx_window_set_ime_rect(window, (nx_rect){12, 34, 2, 18}) == NX_OK);
    CHECK(nx_window_set_ime_enabled(window, 1) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0 && atomic_load(&server.enable_commits) > 0);
    wl_surface_commit(nx_window_wayland_surface(window));
    size_t preedit_length = 0; int32_t preedit_begin = 0, preedit_end = 0;
    for (unsigned i = 0; i < 20 && !preedit_length; ++i) {
        CHECK(nx_window_poll(window, 100) == NX_OK);
        CHECK(nx_window_get_ime_preedit(window, NULL, 0, &preedit_length,
                                                &preedit_begin, &preedit_end) == NX_OK);
    }
    CHECK(preedit_length == 4 && preedit_begin == 2 && preedit_end == 2);
    CHECK(nx_linux_window_clipboard_async_take(window, first, &bytes, &length, &ready) == NX_OK && !ready);
    /* Replacing the selection must not free the in-flight offer. */
    publish_offer(&server, window);
    close_peer(&server);
    for (unsigned i = 0; i < 20 && !ready; ++i) {
        CHECK(nx_window_poll(window, 100) == NX_OK);
        CHECK(nx_linux_window_clipboard_async_take(window, first, &bytes, &length, &ready) == NX_OK);
    }
    CHECK(ready && length == 4 && !memcmp(bytes, "Once", 4)); free(bytes);
    CHECK(nx_linux_window_clipboard_async_take(window, first, &bytes, &length, &ready) == NX_ERROR_STATE);

    publish_offer(&server, window); uint64_t second = 0;
    CHECK(nx_linux_window_clipboard_async_start(window, 1024, 100, 1, &second) == NX_OK && second != first);
    CHECK(nx_linux_window_clipboard_async_cancel(window, first) == NX_ERROR_STATE);
    nx_result result = NX_OK; ready = 0; begin = now_seconds();
    for (unsigned i = 0; i < 30 && !ready; ++i) {
        CHECK(nx_window_poll(window, 300) == NX_OK);
        result = nx_linux_window_clipboard_async_take(window, second, &bytes, &length, &ready);
    }
    CHECK(ready && result == NX_ERROR_PLATFORM && now_seconds() - begin < 1.5);
    close_peer(&server);
    atomic_store(&server.offer_stall, 0); publish_offer(&server, window);
    uint64_t small = 0; ready = 0;
    CHECK(nx_linux_window_clipboard_async_start(window, 3, 2000, 1, &small) == NX_OK);
    for (unsigned i = 0; i < 20 && !ready; ++i) {
        CHECK(nx_window_poll(window, 100) == NX_OK);
        result = nx_linux_window_clipboard_async_take(window, small, &bytes, &length, &ready);
    }
    CHECK(ready && result == NX_ERROR_LIMIT && !bytes);
    atomic_store(&server.offer_invalid, 1); publish_offer(&server, window);
    uint64_t invalid = 0; ready = 0;
    CHECK(nx_linux_window_clipboard_async_start(window, 1024, 2000, 1, &invalid) == NX_OK);
    for (unsigned i = 0; i < 20 && !ready; ++i) {
        CHECK(nx_window_poll(window, 100) == NX_OK);
        result = nx_linux_window_clipboard_async_take(window, invalid, &bytes, &length, &ready);
    }
    CHECK(ready && result == NX_ERROR_STATE && !bytes);
    atomic_store(&server.offer_invalid, 0); atomic_store(&server.offer_stall, 1);
    publish_offer(&server, window); uint64_t third = 0;
    CHECK(nx_linux_window_clipboard_async_start(window, 1024, 2000, 1, &third) == NX_OK);
    for (unsigned i = 0; i < 20 && !atomic_load(&server.stalled_fd); ++i)
        CHECK(nx_window_poll(window, 100) == NX_OK);
    CHECK(atomic_load(&server.stalled_fd));
    CHECK(nx_linux_window_clipboard_async_cancel(window, third) == NX_OK);
    close_peer(&server);
    publish_offer(&server, window); uint64_t last = 0;
    CHECK(nx_linux_window_clipboard_async_start(window, 1024, 2000, 1, &last) == NX_OK);
    for (unsigned i = 0; i < 20 && !atomic_load(&server.stalled_fd); ++i)
        CHECK(nx_window_poll(window, 100) == NX_OK);
    CHECK(atomic_load(&server.stalled_fd)); nx_window_destroy(window); CHECK(allocations.live == 0); close_peer(&server);
    wl_display_terminate(server.display); CHECK(pthread_join(thread, NULL) == 0);
    wl_display_destroy(server.display);
    CHECK(unlink(path) == 0 || access(path, F_OK) != 0); CHECK(rmdir(runtime) == 0);
    puts("Asynchronous Wayland clipboard polling and cancellation verified"); return 0;
}
