#include "wayland_v3_fixture.h"
#include "nexaui/wayland_clipboard.h"
#include <time.h>

static double now_seconds(void) {
    struct timespec t;CHECK(clock_gettime(CLOCK_MONOTONIC,&t)==0);
    return t.tv_sec+t.tv_nsec/1e9;
}
static const char *start_server(test_server *s,pthread_t *thread) {
    s->clipboard_auto_focus=1;s->display=wl_display_create();CHECK(s->display);
    CHECK(wl_global_create(s->display,&wl_compositor_interface,1,s,bind_compositor));
    CHECK(wl_global_create(s->display,&wl_shm_interface,1,s,bind_shm));
    CHECK(wl_global_create(s->display,&wl_seat_interface,5,s,bind_seat));
    CHECK(wl_global_create(s->display,&wl_data_device_manager_interface,3,s,bind_data_manager));
    CHECK(wl_global_create(s->display,&xdg_wm_base_interface,1,s,bind_wm));
    CHECK(wl_global_create(s->display,&zwp_text_input_manager_v3_interface,1,s,bind_manager));
    const char *name=wl_display_add_socket_auto(s->display);CHECK(name);
    CHECK(!pthread_create(thread,NULL,run_server,s));return name;
}
static void drain(nx_window *w) {
    nx_event event;int available;
    do {CHECK(nx_window_next_event(w,&event,&available)==NX_OK);} while(available);
}
int main(void) {
    char runtime[]="/tmp/nexaui-many-transfer-XXXXXX";CHECK(mkdtemp(runtime));
    CHECK(!setenv("XDG_RUNTIME_DIR",runtime,1) && !setenv("NEXAUI_PLATFORM","wayland",1));
    unsetenv("WAYLAND_SOCKET");
    test_server servers[2]={{0}};pthread_t threads[2];nx_window *windows[2]={NULL,NULL};
    nx_window_desc desc={"poll many transfer",240,120,NX_WINDOW_HIDDEN,{0}};
    for(unsigned i=0;i<2;++i) {
        const char *name=start_server(&servers[i],&threads[i]);
        CHECK(!setenv("WAYLAND_DISPLAY",name,1));
        CHECK(nx_window_create(&desc,&windows[i])==NX_OK);
    }
    test_server *peer=&servers[1];atomic_store(&peer->offer_stall,1);atomic_store(&peer->send_offer,1);
    wl_surface_commit(nx_window_wayland_surface(windows[1]));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(windows[1]))>=0);
    uint64_t token=0;double start=now_seconds();
    CHECK(nx_wayland_clipboard_read_begin(windows[1],1024,200,0,&token)==NX_OK && token);
    uint32_t flags=0;int ready=0;char *bytes=NULL;size_t length=0;nx_result result=NX_OK;
    for(unsigned i=0;i<30 && !ready;++i) {
        CHECK(nx_window_poll_many(windows,2,2000,-1,&flags)==NX_OK && !flags);
        drain(windows[0]);drain(windows[1]);
        result=nx_wayland_clipboard_read_take(windows[1],token,&bytes,&length,&ready);
    }
    double elapsed=now_seconds()-start;
    CHECK(ready && result==NX_ERROR_PLATFORM && !bytes && !length && elapsed>=.1 && elapsed<1.5);
    CHECK(atomic_load(&peer->offer_reads)==1 && atomic_load(&peer->stalled_fd)>0);
    CHECK(close(atomic_exchange(&peer->stalled_fd,0)-1)==0);
    CHECK(nx_window_poll_many(&windows[0],1,0,-1,&flags)==NX_OK);
    for(unsigned i=0;i<2;++i)nx_window_destroy(windows[i]);
    for(unsigned i=0;i<2;++i) {
        wl_display_terminate(servers[i].display);CHECK(!pthread_join(threads[i],NULL));
        wl_display_destroy_clients(servers[i].display);wl_display_destroy(servers[i].display);
    }
    CHECK(!rmdir(runtime));puts("second window's stalled transfer deadline woke group");return 0;
}
