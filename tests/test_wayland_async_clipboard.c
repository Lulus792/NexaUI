#include "wayland_v3_fixture.h"
#include "text_value.h"
#include "accessibility.h"
#include "nexaui/wayland_clipboard.h"
#include <time.h>
#include <xkbcommon/xkbcommon.h>

static void send_keymap(struct wl_resource *keyboard) {
    struct xkb_context *context=xkb_context_new(XKB_CONTEXT_NO_FLAGS);CHECK(context);
    struct xkb_keymap *keymap=xkb_keymap_new_from_names(context,NULL,XKB_KEYMAP_COMPILE_NO_FLAGS);CHECK(keymap);
    char *text=xkb_keymap_get_as_string(keymap,XKB_KEYMAP_FORMAT_TEXT_V1);CHECK(text);
    char path[]="/tmp/nexaui-keymap-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0 && unlink(path)==0);
    size_t length=strlen(text)+1;CHECK(write(fd,text,length)==(ssize_t)length);
    wl_keyboard_send_keymap(keyboard,WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,fd,(uint32_t)length);
    close(fd);free(text);xkb_keymap_unref(keymap);xkb_context_unref(context);
}

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

typedef struct public_wrong_thread {
    nx_window *window; uint64_t token; nx_result begin, take, cancel;
    uint64_t output_token; char *bytes; size_t length; int ready;
} public_wrong_thread;
static void *probe_public_wrong_thread(void *arg) {
    public_wrong_thread *probe=arg;
    probe->output_token=1; probe->bytes=(char *)1; probe->length=1; probe->ready=1;
    probe->begin=nx_wayland_clipboard_read_begin(probe->window,1024,1000,0,&probe->output_token);
    probe->take=nx_wayland_clipboard_read_take(probe->window,probe->token,&probe->bytes,&probe->length,&probe->ready);
    probe->cancel=nx_wayland_clipboard_read_cancel(probe->window,probe->token);
    return NULL;
}

int main(int argc, char **argv) {
    CHECK(argc==1 || (argc==2 && (!strcmp(argv[1],"--focus-loss") || !strcmp(argv[1],"--keyboard-removed") || !strcmp(argv[1],"--seat-removed") || !strcmp(argv[1],"--pointer-removed") || !strcmp(argv[1],"--pointer-serial") || !strcmp(argv[1],"--disconnect"))));
    int focus_loss=argc==2;
    int keyboard_removed=argc==2 && !strcmp(argv[1],"--keyboard-removed");
    int seat_removed=argc==2 && !strcmp(argv[1],"--seat-removed");
    int pointer_serial=argc==2 && !strcmp(argv[1],"--pointer-serial");
    int disconnect=argc==2 && !strcmp(argv[1],"--disconnect");
    int pointer_removed=argc==2 && (!strcmp(argv[1],"--pointer-removed") || pointer_serial || disconnect);
    test_server server = {0}; char runtime[] = "/tmp/nexaui-async-XXXXXX";
    server.keyboard_enabled=focus_loss;
    server.clipboard_auto_focus=!focus_loss;
    server.pointer_enabled=pointer_removed;
    if(pointer_removed)server.keyboard_bound=send_keymap;
    CHECK(mkdtemp(runtime) && setenv("XDG_RUNTIME_DIR", runtime, 1) == 0);
    server.display = wl_display_create(); CHECK(server.display);
    CHECK(wl_global_create(server.display, &wl_compositor_interface, 1, &server, bind_compositor));
    if(pointer_removed) CHECK(wl_display_init_shm(server.display)==0);
    else CHECK(wl_global_create(server.display, &wl_shm_interface, 1, &server, bind_shm));
    server.seat_global=wl_global_create(server.display, &wl_seat_interface, 5, &server, bind_seat);
    CHECK(server.seat_global);
    CHECK(wl_global_create(server.display, &wl_data_device_manager_interface, 3, &server, bind_data_manager));
    CHECK(wl_global_create(server.display, &xdg_wm_base_interface, 1, &server, bind_wm));
    CHECK(wl_global_create(server.display, &zwp_text_input_manager_v3_interface, 1, &server, bind_manager));
    const char *name = wl_display_add_socket_auto(server.display); CHECK(name);
    char path[256]; CHECK(snprintf(path, sizeof(path), "%s/%s", runtime, name) > 0);
    CHECK(setenv("WAYLAND_DISPLAY", name, 1) == 0); unsetenv("WAYLAND_SOCKET");
    pthread_t thread; CHECK(pthread_create(&thread, NULL, run_server, &server) == 0);
    window_heap allocations = {0};
    nx_window_desc desc = {"async clipboard", 240, 120, disconnect?0:NX_WINDOW_HIDDEN,
                           {&allocations, window_allocate, window_free}};
    nx_window *window = NULL; CHECK(nx_window_create(&desc, &window) == NX_OK);
    if(focus_loss) {
        atomic_store(&server.keyboard_enter_requested,1);publish_offer(&server,window);
        atomic_store(&server.keyboard_leave_requested,1);
        wl_surface_commit(nx_window_wayland_surface(window));
        CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
        uint64_t token=0;size_t length=0;
        CHECK(nx_wayland_clipboard_read_begin(window,1024,2000,0,&token)==NX_ERROR_STATE);
        CHECK(nx_window_clipboard_get_text(window,NULL,0,&length)==NX_ERROR_STATE);
        CHECK(atomic_load(&server.offer_reads)==0);
        atomic_store(&server.keyboard_enter_requested,1);publish_offer(&server,window);
        atomic_store(&server.offer_stall,1);
        CHECK(nx_wayland_clipboard_read_begin(window,1024,2000,0,&token)==NX_OK);
        for(unsigned i=0;i<20 && !atomic_load(&server.stalled_fd);++i)CHECK(nx_window_poll(window,100)==NX_OK);
        CHECK(atomic_load(&server.stalled_fd));
        nx_event event;int event_available=0;
        do {CHECK(nx_window_next_event(window,&event,&event_available)==NX_OK);} while(event_available);
        if(pointer_removed) {
            nx_context *context=NULL;CHECK(nx_context_create(NULL,&context)==NX_OK);
            atomic_store(&server.pointer_press_requested,1);
            wl_surface_commit(nx_window_wayland_surface(window));
            CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
            size_t count=0;CHECK(nx_window_route_events(window,context,&count)==NX_OK);
            CHECK(nx_begin_frame(context,240,120,0)==NX_OK);
            nx_input_snapshot input;CHECK(nx_frame_input(context,&input)==NX_OK);
            CHECK((input.buttons_down & (NX_BUTTON_LEFT|NX_BUTTON_RIGHT))==(NX_BUTTON_LEFT|NX_BUTTON_RIGHT));
            CHECK(nx_end_frame(context)==NX_OK);
            if(!pointer_serial) {
                atomic_store(&server.keyboard_press_requested,1);
                wl_surface_commit(nx_window_wayland_surface(window));
                CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
            }
            if(disconnect) {
                nx_linux_ax_geometry geometry;
                nx_linux_window_ax_geometry(window,&geometry);
                CHECK(geometry.visible && geometry.focused);
                atomic_store(&server.disconnect_requested,1);
                wl_surface_commit(nx_window_wayland_surface(window));
                CHECK(wl_display_flush(nx_window_wayland_display(window))>=0);
                double start=now_seconds();
                CHECK(nx_window_poll(window,1000)==NX_ERROR_PLATFORM && now_seconds()-start<1.5);
                CHECK(!nx_linux_window_clipboard_async_current(window,token));
                nx_window_status status;
                CHECK(nx_window_get_status(window,&status)==NX_ERROR_PLATFORM);
                nx_window_info info;
                CHECK(nx_window_get_info(window,&info)==NX_ERROR_PLATFORM);
                nx_pixel_size pixels={99,99};
                CHECK(nx_window_get_pixel_size(window,&pixels)==NX_ERROR_PLATFORM && !pixels.width && !pixels.height);
                nx_linux_window_ax_geometry(window,&geometry);
                CHECK(!geometry.visible && !geometry.focused);
                CHECK(nx_window_route_events(window,context,&count)==NX_OK);
                CHECK(nx_begin_frame(context,240,120,0)==NX_OK);
                CHECK(nx_frame_input(context,&input)==NX_OK && input.buttons_down==0);
                const nx_event *events=NULL;size_t event_count=0;int cancelled=0,focus_lost=0;
                CHECK(nx_frame_events(context,&events,&event_count)==NX_OK);
                for(size_t i=0;i<event_count;++i) {
                    if(events[i].type==NX_EVENT_POINTER_CANCEL)cancelled=1;
                    if(events[i].type==NX_EVENT_FOCUS && !events[i].focused)focus_lost=1;
                }
                CHECK(cancelled && focus_lost && nx_end_frame(context)==NX_OK);
                CHECK(nx_window_poll(window,0)==NX_ERROR_PLATFORM);
                close_peer(&server);nx_context_destroy(context);nx_window_destroy(window);CHECK(allocations.live==0);
                wl_display_terminate(server.display);CHECK(pthread_join(thread,NULL)==0);wl_display_destroy(server.display);
                CHECK(unlink(path)==0 || access(path,F_OK)!=0);CHECK(rmdir(runtime)==0);
                puts("Disconnected Wayland window cancels input and reports a terminal error");return 0;
            }
            atomic_store(&server.pointer_remove_requested,1);
            wl_surface_commit(nx_window_wayland_surface(window));
            CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
            CHECK(nx_window_poll(window,0)==NX_OK && nx_linux_window_clipboard_async_current(window,token));
            CHECK(nx_window_route_events(window,context,&count)==NX_OK);
            CHECK(nx_begin_frame(context,240,120,0)==NX_OK);
            CHECK(nx_frame_input(context,&input)==NX_OK && input.buttons_down==0);
            CHECK((input.buttons_released & (NX_BUTTON_LEFT|NX_BUTTON_RIGHT))==(NX_BUTTON_LEFT|NX_BUTTON_RIGHT));
            const nx_event *events=NULL;size_t event_count=0;int cancelled=0;
            CHECK(nx_frame_events(context,&events,&event_count)==NX_OK);
            for(size_t i=0;i<event_count;++i)if(events[i].type==NX_EVENT_POINTER_CANCEL)cancelled=1;
            CHECK(cancelled && nx_end_frame(context)==NX_OK);
            close_peer(&server);
            char *bytes=NULL;int ready=0;
            for(unsigned i=0;i<20 && !ready;++i) {
                CHECK(nx_window_poll(window,100)==NX_OK);
                CHECK(nx_wayland_clipboard_read_take(window,token,&bytes,&length,&ready)==NX_OK);
            }
            CHECK(ready && length==4 && !memcmp(bytes,"Once",4));nx_wayland_clipboard_read_release(bytes,length);
            CHECK(nx_window_clipboard_set_text(window,"Owned",5)==(pointer_serial?NX_ERROR_STATE:NX_OK));
            CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
            CHECK(atomic_load(&server.selection_serial_seen)==(pointer_serial?0:7));
            nx_context_destroy(context);nx_window_destroy(window);CHECK(allocations.live==0);
            wl_display_terminate(server.display);CHECK(pthread_join(thread,NULL)==0);wl_display_destroy(server.display);
            CHECK(unlink(path)==0 || access(path,F_OK)!=0);CHECK(rmdir(runtime)==0);
            puts("Pointer removal cancels buttons without cancelling clipboard");return 0;
        }
        if(seat_removed)atomic_store(&server.seat_remove_requested,1);
        else if(keyboard_removed)atomic_store(&server.keyboard_remove_requested,1);
        else atomic_store(&server.keyboard_leave_requested,1);
        wl_surface_commit(nx_window_wayland_surface(window));
        CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
        CHECK(!nx_linux_window_clipboard_async_current(window,token));
        char *stale=(char *)1; size_t stale_length=1; int stale_ready=1;
        CHECK(nx_wayland_clipboard_read_take(window,token,&stale,&stale_length,&stale_ready)==NX_ERROR_STATE &&
              !stale && !stale_length && !stale_ready);
        CHECK(nx_wayland_clipboard_read_begin(window,1024,2000,0,&token)==NX_ERROR_STATE);
        if(seat_removed)CHECK(nx_window_poll(window,0)==NX_ERROR_PLATFORM);
        int saw_focus_loss=0;
        do {CHECK(nx_window_next_event(window,&event,&event_available)==NX_OK);
            if(event_available && event.type==NX_EVENT_FOCUS && !event.focused)saw_focus_loss=1;
        } while(event_available);
        CHECK(saw_focus_loss);
        close_peer(&server);
        if(!keyboard_removed && !seat_removed) {
            /* Re-entering without a fresh offer must not resurrect stale data. */
            atomic_store(&server.keyboard_enter_requested,1);
            wl_surface_commit(nx_window_wayland_surface(window));
            CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
            CHECK(nx_wayland_clipboard_read_begin(window,1024,2000,0,&token)==NX_ERROR_STATE && !token);
            atomic_store(&server.keyboard_leave_requested,1);
            wl_surface_commit(nx_window_wayland_surface(window));
            CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
            /* Wayland selection can arrive before keyboard.enter. Until enter,
               neither the public async nor synchronous reader may use it. */
            atomic_store(&server.offer_stall,0);publish_offer(&server,window);
            int reads_before=atomic_load(&server.offer_reads);
            CHECK(nx_wayland_clipboard_read_begin(window,1024,2000,0,&token)==NX_ERROR_STATE && !token);
            CHECK(nx_window_clipboard_get_text(window,NULL,0,&length)==NX_ERROR_STATE);
            char *private_bytes=(char *)1; size_t private_length=1;
            CHECK(nx_linux_window_clipboard_fetch_text(window,&private_bytes,&private_length)==NX_ERROR_STATE &&
                  !private_bytes && !private_length);
            CHECK(atomic_load(&server.offer_reads)==reads_before);
            atomic_store(&server.keyboard_enter_requested,1);
            wl_surface_commit(nx_window_wayland_surface(window));
            CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
            CHECK(nx_wayland_clipboard_read_begin(window,1024,2000,0,&token)==NX_OK);
            char *bytes=NULL;int ready=0;
            for(unsigned i=0;i<20 && !ready;++i) {
                CHECK(nx_window_poll(window,100)==NX_OK);
                CHECK(nx_wayland_clipboard_read_take(window,token,&bytes,&length,&ready)==NX_OK);
            }
            CHECK(ready && length==4 && !memcmp(bytes,"Once",4));nx_wayland_clipboard_read_release(bytes,length);
        }
        nx_window_destroy(window);CHECK(allocations.live==0);
        wl_display_terminate(server.display);CHECK(pthread_join(thread,NULL)==0);wl_display_destroy(server.display);
        CHECK(unlink(path)==0 || access(path,F_OK)!=0);CHECK(rmdir(runtime)==0);
        puts("Wayland selection invalidated on focus loss and restored on reentry");return 0;
    }
    atomic_store(&server.offer_stall, 1); publish_offer(&server, window);
    uint64_t first = 0, other = 0; char *bytes = NULL; size_t length = 0; int ready = 0;
    CHECK(nx_wayland_clipboard_read_begin(window, NX_WAYLAND_CLIPBOARD_READ_MAX_BYTES+1u, 2000, 0, &other)==NX_ERROR_ARGUMENT && !other);
    CHECK(nx_wayland_clipboard_read_begin(window, 1024, 0, 0, &other)==NX_ERROR_ARGUMENT && !other);
    CHECK(nx_wayland_clipboard_read_begin(window, 1024, 2000, 2, &other) == NX_ERROR_ARGUMENT && !other);
    CHECK(nx_wayland_clipboard_read_begin(window, 1024, 2000, 0, &first) == NX_OK && first);
    public_wrong_thread probe={.window=window,.token=first}; pthread_t other_thread;
    CHECK(!pthread_create(&other_thread,NULL,probe_public_wrong_thread,&probe));
    CHECK(!pthread_join(other_thread,NULL));
    CHECK(probe.begin==NX_ERROR_STATE && probe.take==NX_ERROR_STATE && probe.cancel==NX_ERROR_STATE &&
          !probe.output_token && !probe.bytes && !probe.length && !probe.ready);
    CHECK(nx_linux_window_clipboard_async_cancel_active(window) == NX_ERROR_STATE);
    CHECK(nx_linux_window_clipboard_async_start(window, 1024, 2000, 0, &other) == NX_ERROR_LIMIT);
    CHECK(nx_wayland_clipboard_read_take(window, first+1, &bytes, &length, &ready) == NX_ERROR_STATE && !bytes && !length && !ready);
    CHECK(nx_wayland_clipboard_read_take(window, first, &bytes, &length, &ready) == NX_OK && !ready);
    double begin = now_seconds();
    for (unsigned i = 0; i < 20 && !atomic_load(&server.stalled_fd); ++i)
        CHECK(nx_window_poll(window, 100) == NX_OK);
    CHECK(atomic_load(&server.stalled_fd) && now_seconds() - begin < 1.5);
    CHECK(nx_wayland_clipboard_read_take(window, first, &bytes, &length, &ready) == NX_OK && !ready);
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
    CHECK(nx_wayland_clipboard_read_take(window, first, &bytes, &length, &ready) == NX_OK && !ready);
    /* Replacing the selection must not free the in-flight offer. */
    publish_offer(&server, window);
    close_peer(&server);
    for (unsigned i = 0; i < 20 && !ready; ++i) {
        CHECK(nx_window_poll(window, 100) == NX_OK);
        CHECK(nx_wayland_clipboard_read_take(window, first, &bytes, &length, &ready) == NX_OK);
    }
    CHECK(ready && length == 4 && !memcmp(bytes, "Once", 4)); nx_wayland_clipboard_read_release(bytes,length);
    CHECK(nx_wayland_clipboard_read_take(window, first, &bytes, &length, &ready) == NX_ERROR_STATE);

    publish_offer(&server, window); uint64_t second = 0;
    CHECK(nx_wayland_clipboard_read_begin(window, 1024, 100, 1, &second) == NX_OK && second != first);
    CHECK(nx_wayland_clipboard_read_cancel(window, first) == NX_ERROR_STATE);
    nx_result result = NX_OK; ready = 0; begin = now_seconds();
    for (unsigned i = 0; i < 30 && !ready; ++i) {
        CHECK(nx_window_poll(window, 300) == NX_OK);
        result = nx_wayland_clipboard_read_take(window, second, &bytes, &length, &ready);
    }
    CHECK(ready && result == NX_ERROR_PLATFORM && now_seconds() - begin < 1.5);
    close_peer(&server);
    atomic_store(&server.offer_stall, 0); publish_offer(&server, window);
    uint64_t small = 0; ready = 0;
    CHECK(nx_wayland_clipboard_read_begin(window, 3, 2000, 1, &small) == NX_OK);
    for (unsigned i = 0; i < 20 && !ready; ++i) {
        CHECK(nx_window_poll(window, 100) == NX_OK);
        result = nx_wayland_clipboard_read_take(window, small, &bytes, &length, &ready);
    }
    CHECK(ready && result == NX_ERROR_LIMIT && !bytes);
    atomic_store(&server.offer_invalid, 1); publish_offer(&server, window);
    uint64_t invalid = 0; ready = 0;
    CHECK(nx_wayland_clipboard_read_begin(window, 1024, 2000, 1, &invalid) == NX_OK);
    for (unsigned i = 0; i < 20 && !ready; ++i) {
        CHECK(nx_window_poll(window, 100) == NX_OK);
        result = nx_wayland_clipboard_read_take(window, invalid, &bytes, &length, &ready);
    }
    CHECK(ready && result == NX_ERROR_STATE && !bytes);
    atomic_store(&server.offer_invalid, 0); atomic_store(&server.offer_stall, 1);
    publish_offer(&server, window); uint64_t third = 0;
    CHECK(nx_wayland_clipboard_read_begin(window, 1024, 2000, 1, &third) == NX_OK);
    for (unsigned i = 0; i < 20 && !atomic_load(&server.stalled_fd); ++i)
        CHECK(nx_window_poll(window, 100) == NX_OK);
    CHECK(atomic_load(&server.stalled_fd));
    CHECK(nx_wayland_clipboard_read_cancel(window, third) == NX_OK);
    CHECK(nx_wayland_clipboard_read_cancel(window, third) == NX_ERROR_STATE);
    close_peer(&server);
    publish_offer(&server, window); uint64_t last = 0;
    CHECK(nx_wayland_clipboard_read_begin(window, 1024, 2000, 1, &last) == NX_OK);
    for (unsigned i = 0; i < 20 && !atomic_load(&server.stalled_fd); ++i)
        CHECK(nx_window_poll(window, 100) == NX_OK);
    CHECK(atomic_load(&server.stalled_fd)); nx_window_destroy(window); CHECK(allocations.live == 0); close_peer(&server);
    wl_display_terminate(server.display); CHECK(pthread_join(thread, NULL) == 0);
    wl_display_destroy(server.display);
    CHECK(unlink(path) == 0 || access(path, F_OK) != 0); CHECK(rmdir(runtime) == 0);
    puts("Asynchronous Wayland clipboard polling and cancellation verified"); return 0;
}
