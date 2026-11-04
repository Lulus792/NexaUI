#include "wayland_v3_fixture.h"
#include "text_value.h"
#include <time.h>

static double now_seconds(void) {
    struct timespec t; CHECK(clock_gettime(CLOCK_MONOTONIC, &t) == 0);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
typedef struct drop_heap { window_heap heap; nx_window *window; int armed, reentered; } drop_heap;
static void *drop_allocate(void *user, size_t size) {
    drop_heap *heap=user;
    if(heap->armed) {
        heap->armed=0; ++heap->reentered;
        CHECK(nx_window_poll(heap->window,0)==NX_OK);
        size_t length=0; nx_file_drop_info info={0}; int available=0;
        CHECK(nx_window_next_file_drop(heap->window,NULL,0,&length,&info,&available)==NX_ERROR_STATE);
    }
    return window_allocate(&heap->heap,size);
}
static void drop_free(void *user, void *pointer, size_t size) {
    drop_heap *heap=user; window_free(&heap->heap,pointer,size);
}

int main(int argc, char **argv) {
    CHECK(argc==1 || (argc==2 && (!strcmp(argv[1],"--timeout") || !strcmp(argv[1],"--destroy"))));
    int timeout=argc==2 && !strcmp(argv[1],"--timeout");
    int destroy=argc==2 && !strcmp(argv[1],"--destroy");
    test_server server = {0}; char runtime[] = "/tmp/nexaui-drop-XXXXXX";
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
    drop_heap allocations = {0};
    nx_window_desc desc = {"async drop", 240, 120, NX_WINDOW_HIDDEN,
                           {&allocations, drop_allocate, drop_free}};
    nx_window *window = NULL; CHECK(nx_window_create(&desc, &window) == NX_OK);
    allocations.window=window;
    atomic_store(&server.offer_stall, 1); atomic_store(&server.send_drop, 1);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    double start = now_seconds();
    for (unsigned i=0; i<20 && !atomic_load(&server.stalled_fd); ++i)
        CHECK(nx_window_poll(window, 100) == NX_OK);
    CHECK(atomic_load(&server.stalled_fd) && now_seconds()-start < 1.5);
    if (destroy) {
        nx_window_destroy(window); window=NULL; CHECK(allocations.heap.live==0);
    } else if (timeout) {
        nx_result result=NX_OK;
        while (result==NX_OK && now_seconds()-start<6.5) result=nx_window_poll(window,300);
        CHECK(result==NX_ERROR_PLATFORM && now_seconds()-start>=4.0 && now_seconds()-start<6.5);
    } else {
    CHECK(nx_window_set_ime_rect(window, (nx_rect){12,34,2,18}) == NX_OK);
    CHECK(nx_window_set_ime_enabled(window, 1) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0 && atomic_load(&server.enable_commits)>0);
    /* A pending drop must not monopolize the independent clipboard reader. */
    atomic_store(&server.offer_stall,0); atomic_store(&server.send_offer,1);
    atomic_store(&server.send_drop,2);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    uint64_t token=0; char *clipboard=NULL; size_t clipboard_length=0; int ready=0;
    CHECK(nx_linux_window_clipboard_async_start(window,1024,2000,0,&token)==NX_OK);
    for(unsigned i=0;i<20 && !ready;++i) {
        CHECK(nx_window_poll(window,100)==NX_OK);
        CHECK(nx_linux_window_clipboard_async_take(window,token,&clipboard,&clipboard_length,&ready)==NX_OK);
    }
    CHECK(ready && clipboard_length==4 && !memcmp(clipboard,"Once",4)); free(clipboard);
    int has_event=0; nx_event event;
    do {CHECK(nx_window_next_event(window,&event,&has_event)==NX_OK);} while(has_event);
    int fd=atomic_exchange(&server.stalled_fd,0); CHECK(fd>0 && close(fd-1)==0);
    allocations.armed=1;
    char value[64]; size_t length=0; int available=0; nx_file_drop_info info={0};
    for (unsigned i=0;i<20 && !available;++i) {
        CHECK(nx_window_poll(window,100)==NX_OK);
        CHECK(nx_window_next_file_drop(window,value,sizeof(value),&length,&info,&available)==NX_OK);
    }
    CHECK(available && !strcmp(value,"/tmp/one file") && info.x==20.5f && info.y==30.25f && allocations.reentered==1);
    available=0;
    for(unsigned i=0;i<20 && !available;++i) {
        CHECK(nx_window_poll(window,100)==NX_OK);
        CHECK(nx_window_next_file_drop(window,value,sizeof(value),&length,&info,&available)==NX_OK);
    }
    CHECK(available && !strcmp(value,"/tmp/one file") && info.x==50.5f && info.y==30.25f);
    int woke=0;
    do {CHECK(nx_window_next_event(window,&event,&has_event)==NX_OK);if(has_event && event.type==NX_EVENT_WAKE)woke=1;} while(has_event);
    CHECK(woke);
    }
    if(window) nx_window_destroy(window);
    CHECK(allocations.heap.live==0);
    int remaining=atomic_exchange(&server.stalled_fd,0); if(remaining)CHECK(close(remaining-1)==0);
    wl_display_terminate(server.display); CHECK(pthread_join(thread,NULL)==0);
    wl_display_destroy(server.display); CHECK(unlink(path)==0 || access(path,F_OK)!=0); CHECK(rmdir(runtime)==0);
    puts("Asynchronous Wayland drop remains responsive and completes"); return 0;
}
