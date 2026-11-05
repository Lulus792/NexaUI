#include "wayland_v3_fixture.h"
#include <time.h>

typedef struct closing_heap {
    window_heap heap;
    nx_window *window;
    int armed, closed;
} closing_heap;
static void *closing_allocate(void *user,size_t size) {
    closing_heap *state=user;
    if(state->armed) {
        state->armed=0;state->closed=1;nx_window_destroy(state->window);
    }
    return window_allocate(&state->heap,size);
}
static void closing_free(void *user,void *pointer,size_t size) {
    closing_heap *state=user;window_free(&state->heap,pointer,size);
}

static const char *start_server(test_server *server, pthread_t *thread, const char *socket) {
    server->clipboard_auto_focus=1;
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

int main(int argc,char **argv) {
    CHECK(argc==1 || (argc==2 && (!strcmp(argv[1],"--self-destroy") || !strcmp(argv[1],"--self-destroy-stall") || !strcmp(argv[1],"--other-destroy") || !strcmp(argv[1],"--reader-destroy") || !strcmp(argv[1],"--next-destroy"))));
    int self_stall=argc==2 && !strcmp(argv[1],"--self-destroy-stall");
    int self_destroy=argc==2 && (!strcmp(argv[1],"--self-destroy") || self_stall);
    int reader_destroy=argc==2 && !strcmp(argv[1],"--reader-destroy");
    int next_destroy=argc==2 && !strcmp(argv[1],"--next-destroy");
    int other_destroy=argc==2 && (!strcmp(argv[1],"--other-destroy") || reader_destroy);
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
    closing_heap closing={0};
    nx_window_desc desc={"healthy",240,120,NX_WINDOW_HIDDEN,{&healthy_heap,window_allocate,window_free}};
    if(self_destroy)desc.allocator=(nx_allocator){&closing,closing_allocate,closing_free};
    nx_window *first=NULL,*second=NULL;
    CHECK(nx_window_create(&desc,&first)==NX_OK);
    if(self_destroy) {
        closing.window=first;
        atomic_store(&healthy.send_offer,1);
        wl_surface_commit(nx_window_wayland_surface(first));
        CHECK(wl_display_roundtrip(nx_window_wayland_display(first))>=0);
        atomic_store(&healthy.offer_stall,self_stall);
        closing.armed=1;
        char text[16];size_t length=99;
        struct timespec begin,end;CHECK(clock_gettime(CLOCK_MONOTONIC,&begin)==0);
        CHECK(nx_window_clipboard_get_text(first,text,sizeof(text),&length)==NX_ERROR_STATE);
        CHECK(clock_gettime(CLOCK_MONOTONIC,&end)==0);
        CHECK((double)(end.tv_sec-begin.tv_sec)+(double)(end.tv_nsec-begin.tv_nsec)/1e9<1.5);
        CHECK(closing.closed && !closing.heap.live && length==0);
        if(self_stall) {int fd=atomic_exchange(&healthy.stalled_fd,0);CHECK(fd>0 && close(fd-1)==0);}
        wl_display_terminate(healthy.display);CHECK(pthread_join(healthy_thread,NULL)==0);
        wl_display_destroy(healthy.display);
        CHECK(unlink(healthy_path)==0 || access(healthy_path,F_OK)!=0);CHECK(rmdir(runtime)==0);
        puts("Window destruction during clipboard allocation is deferred safely");return 0;
    }
    const char *failed_socket=start_server(&failed,&failed_thread,"failed");
    CHECK(snprintf(failed_path,sizeof(failed_path),"%s/%s",runtime,failed_socket)>0);
    CHECK(setenv("WAYLAND_DISPLAY",failed_socket,1)==0);
    desc.title_utf8="failed";desc.allocator.user=&failed_heap;
    if(other_destroy)desc.allocator=(nx_allocator){&closing,closing_allocate,closing_free};
    CHECK(nx_window_create(&desc,&second)==NX_OK);

    if(next_destroy) {
        test_server third_server={0};pthread_t third_thread;
        const char *third_socket=start_server(&third_server,&third_thread,"third");
        char third_path[256];CHECK(snprintf(third_path,sizeof(third_path),"%s/%s",runtime,third_socket)>0);
        CHECK(setenv("WAYLAND_DISPLAY",third_socket,1)==0);
        desc.title_utf8="third";desc.allocator=(nx_allocator){&closing,closing_allocate,closing_free};
        nx_window *third=NULL;CHECK(nx_window_create(&desc,&third)==NX_OK);
        closing.window=second;
        atomic_store(&healthy.send_offer,1);wl_surface_commit(nx_window_wayland_surface(first));
        CHECK(wl_display_roundtrip(nx_window_wayland_display(first))>=0);
        atomic_store(&third_server.send_offer,1);wl_surface_commit(nx_window_wayland_surface(third));
        CHECK(wl_display_flush(nx_window_wayland_display(third))>=0);
        for(unsigned i=0;i<2000 && !atomic_load(&third_server.offer_sent);++i) {
            struct timespec delay={0,1000000};nanosleep(&delay,NULL);
        }
        CHECK(atomic_load(&third_server.offer_sent));closing.armed=1;
        char text[16];size_t length=0;
        CHECK(nx_window_clipboard_get_text(first,text,sizeof(text),&length)==NX_OK);
        CHECK(length==4 && !memcmp(text,"Once",4) && closing.closed && !failed_heap.live);
        CHECK(nx_window_poll(third,0)==NX_OK && nx_window_poll(first,0)==NX_OK);
        nx_window_destroy(third);nx_window_destroy(first);CHECK(!healthy_heap.live && !closing.heap.live);
        wl_display_terminate(third_server.display);wl_display_terminate(failed.display);wl_display_terminate(healthy.display);
        CHECK(pthread_join(third_thread,NULL)==0 && pthread_join(failed_thread,NULL)==0 && pthread_join(healthy_thread,NULL)==0);
        wl_display_destroy(third_server.display);wl_display_destroy(failed.display);wl_display_destroy(healthy.display);
        CHECK(unlink(third_path)==0 || access(third_path,F_OK)!=0);
        CHECK(unlink(failed_path)==0 || access(failed_path,F_OK)!=0);
        CHECK(unlink(healthy_path)==0 || access(healthy_path,F_OK)!=0);CHECK(rmdir(runtime)==0);
        puts("A later window can close while another window is pumped");return 0;
    }

    if(other_destroy) {
        closing.window=reader_destroy?first:second;
        atomic_store(&healthy.send_offer,1);
        wl_surface_commit(nx_window_wayland_surface(first));
        CHECK(wl_display_roundtrip(nx_window_wayland_display(first))>=0);
        atomic_store(&failed.send_offer,1);
        wl_surface_commit(nx_window_wayland_surface(second));
        CHECK(wl_display_flush(nx_window_wayland_display(second))>=0);
        for(unsigned i=0;i<2000 && !atomic_load(&failed.offer_sent);++i) {
            struct timespec delay={0,1000000};nanosleep(&delay,NULL);
        }
        CHECK(atomic_load(&failed.offer_sent));closing.armed=1;
        char text[16];size_t length=0;
        CHECK(nx_window_clipboard_get_text(first,text,sizeof(text),&length)==(reader_destroy?NX_ERROR_STATE:NX_OK));
        CHECK(closing.closed);
        if(reader_destroy) {
            CHECK(length==0 && !healthy_heap.live && nx_window_poll(second,0)==NX_OK);
            nx_window_destroy(second);CHECK(!closing.heap.live);
        } else {
            CHECK(length==4 && !memcmp(text,"Once",4) && !closing.heap.live);
            CHECK(nx_window_poll(first,0)==NX_OK);nx_window_destroy(first);CHECK(!healthy_heap.live);
        }
        wl_display_terminate(failed.display);wl_display_terminate(healthy.display);
        CHECK(pthread_join(failed_thread,NULL)==0 && pthread_join(healthy_thread,NULL)==0);
        wl_display_destroy(failed.display);wl_display_destroy(healthy.display);
        CHECK(unlink(failed_path)==0 || access(failed_path,F_OK)!=0);
        CHECK(unlink(healthy_path)==0 || access(healthy_path,F_OK)!=0);CHECK(rmdir(runtime)==0);
        puts("Another window can close during the clipboard event pump");return 0;
    }

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
