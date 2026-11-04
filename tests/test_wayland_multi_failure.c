#include "wayland_v3_fixture.h"

static const char *start_server(test_server *server, pthread_t *thread, const char *socket) {
    server->display=wl_display_create();CHECK(server->display);
    CHECK(wl_global_create(server->display,&wl_compositor_interface,1,server,bind_compositor));
    CHECK(wl_global_create(server->display,&wl_shm_interface,1,server,bind_shm));
    server->seat_global=wl_global_create(server->display,&wl_seat_interface,5,server,bind_seat);
    CHECK(server->seat_global);
    CHECK(wl_global_create(server->display,&wl_data_device_manager_interface,3,server,bind_data_manager));
    CHECK(wl_global_create(server->display,&xdg_wm_base_interface,1,server,bind_wm));
    CHECK(wl_global_create(server->display,&zwp_text_input_manager_v3_interface,1,server,bind_manager));
    CHECK(wl_display_add_socket(server->display,socket)==0);
    CHECK(pthread_create(thread,NULL,run_server,server)==0);
    return socket;
}

int main(void) {
    char runtime[]="/tmp/nexaui-multi-failure-XXXXXX";
    CHECK(mkdtemp(runtime) && setenv("XDG_RUNTIME_DIR",runtime,1)==0);
    CHECK(setenv("NEXAUI_PLATFORM","wayland",1)==0);
    unsetenv("WAYLAND_SOCKET");
    test_server healthy={0}, failed={0};pthread_t healthy_thread,failed_thread;
    const char *healthy_socket=start_server(&healthy,&healthy_thread,"healthy");
    char healthy_path[256],failed_path[256];
    CHECK(snprintf(healthy_path,sizeof(healthy_path),"%s/%s",runtime,healthy_socket)>0);
    CHECK(setenv("WAYLAND_DISPLAY",healthy_socket,1)==0);
    window_heap healthy_heap={0},failed_heap={0};
    nx_window_desc desc={"healthy",240,120,NX_WINDOW_HIDDEN,{&healthy_heap,window_allocate,window_free}};
    nx_window *first=NULL,*second=NULL;
    CHECK(nx_window_create(&desc,&first)==NX_OK);
    const char *failed_socket=start_server(&failed,&failed_thread,"failed");
    CHECK(snprintf(failed_path,sizeof(failed_path),"%s/%s",runtime,failed_socket)>0);
    CHECK(setenv("WAYLAND_DISPLAY",failed_socket,1)==0);
    desc.title_utf8="failed";desc.allocator.user=&failed_heap;
    CHECK(nx_window_create(&desc,&second)==NX_OK);

    atomic_store(&failed.disconnect_requested,1);
    wl_surface_commit(nx_window_wayland_surface(second));
    CHECK(wl_display_flush(nx_window_wayland_display(second))>=0);
    CHECK(nx_window_poll(second,1000)==NX_ERROR_PLATFORM);
    atomic_store(&healthy.send_offer,1);
    wl_surface_commit(nx_window_wayland_surface(first));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(first))>=0);
    char text[16];size_t length=0;
    CHECK(nx_window_clipboard_get_text(first,text,sizeof(text),&length)==NX_OK);
    CHECK(length==4 && !memcmp(text,"Once",4));
    CHECK(nx_window_poll(first,0)==NX_OK);
    CHECK(nx_window_poll(second,0)==NX_ERROR_PLATFORM);

    /* The reading window's own connection loss still aborts its transfer. */
    atomic_store(&healthy.send_offer,1);
    wl_surface_commit(nx_window_wayland_surface(first));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(first))>=0);
    atomic_store(&healthy.disconnect_on_receive,1);
    CHECK(nx_window_clipboard_get_text(first,text,sizeof(text),&length)==NX_ERROR_PLATFORM);
    CHECK(nx_window_poll(first,0)==NX_ERROR_PLATFORM);
    int fd=atomic_exchange(&healthy.stalled_fd,0);CHECK(fd>0 && close(fd-1)==0);

    nx_window_destroy(second);nx_window_destroy(first);
    CHECK(!healthy_heap.live && !failed_heap.live);
    wl_display_terminate(failed.display);wl_display_terminate(healthy.display);
    CHECK(pthread_join(failed_thread,NULL)==0 && pthread_join(healthy_thread,NULL)==0);
    wl_display_destroy(failed.display);wl_display_destroy(healthy.display);
    CHECK(unlink(failed_path)==0 || access(failed_path,F_OK)!=0);
    CHECK(unlink(healthy_path)==0 || access(healthy_path,F_OK)!=0);
    CHECK(rmdir(runtime)==0);
    puts("A disconnected window does not abort another window's clipboard read");
    return 0;
}
