#include "wayland_v3_fixture.h"
#include "nexaui/software.h"
#include <time.h>

typedef struct software_server {
    test_server base;
    atomic_int capture, captured, release_requested;
    atomic_uint pixel, format;
    struct wl_resource *held[3];
} software_server;

typedef struct software_heap {
    window_heap base;
    int fail_next, destroy_next;
    nx_window *window;
    nx_software_view **view;
    nx_result reentrant_close;
} software_heap;
static void *software_allocate(void *user, size_t size) {
    software_heap *heap=user;
    if (heap->fail_next) { heap->fail_next=0; return NULL; }
    if (heap->destroy_next) {
        heap->destroy_next=0;
        nx_window_destroy(heap->window);
        heap->reentrant_close=nx_software_view_close(heap->view);
    }
    return window_allocate(&heap->base,size);
}
static void software_free(void *user, void *pointer, size_t size) {
    software_heap *heap=user;
    window_free(&heap->base,pointer,size);
}

static void capture_buffer(test_server *base, struct wl_resource *buffer) {
    software_server *server=(software_server *)base;
    if (!atomic_load(&server->capture)) return;
    int n=atomic_load(&server->captured);
    CHECK(n<3);
    struct wl_shm_buffer *shm=wl_shm_buffer_get(buffer);
    CHECK(shm);
    wl_shm_buffer_begin_access(shm);
    uint32_t pixel;
    memcpy(&pixel,wl_shm_buffer_get_data(shm),sizeof(pixel));
    wl_shm_buffer_end_access(shm);
    atomic_store(&server->pixel,pixel);
    atomic_store(&server->format,wl_shm_buffer_get_format(shm));
    server->held[n]=buffer;
    atomic_store(&server->captured,n+1);
}
static void release_buffers(test_server *base, struct wl_client *client) {
    (void)client;
    software_server *server=(software_server *)base;
    if (!atomic_exchange(&server->release_requested,0)) return;
    for (int i=0; i<3; ++i) if (server->held[i]) {
        wl_buffer_send_release(server->held[i]);
        server->held[i]=NULL;
    }
}
static void *wrong_thread(void *arg) {
    nx_software_view *view=arg, *copy=view;
    nx_software_view_outcome outcome;
    CHECK(nx_software_view_render(view,NULL,0,(nx_color){0},&outcome)==NX_ERROR_STATE);
    CHECK(nx_software_view_close(&copy)==NX_ERROR_STATE && copy==view);
    return NULL;
}
static void roundtrip(nx_window *window) {
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
}
int main(void) {
    char runtime[]="/tmp/nexaui-software-XXXXXX";
    CHECK(mkdtemp(runtime) && setenv("XDG_RUNTIME_DIR",runtime,1)==0 &&
          setenv("NEXAUI_PLATFORM","wayland",1)==0);
    unsetenv("WAYLAND_SOCKET");
    software_server server={0};
    server.base.width=32; server.base.height=24;
    server.base.buffer_attached=capture_buffer;
    server.base.surface_committed=release_buffers;
    server.base.display=wl_display_create(); CHECK(server.base.display);
    CHECK(wl_global_create(server.base.display,&wl_compositor_interface,4,&server.base,bind_compositor));
    CHECK(wl_display_init_shm(server.base.display)==0);
    CHECK(wl_global_create(server.base.display,&wl_seat_interface,5,&server.base,bind_seat));
    CHECK(wl_global_create(server.base.display,&wl_data_device_manager_interface,3,&server.base,bind_data_manager));
    CHECK(wl_global_create(server.base.display,&xdg_wm_base_interface,1,&server.base,bind_wm));
    CHECK(wl_global_create(server.base.display,&zwp_text_input_manager_v3_interface,1,&server.base,bind_manager));
    const char *socket=wl_display_add_socket_auto(server.base.display); CHECK(socket);
    char path[256]; CHECK(snprintf(path,sizeof(path),"%s/%s",runtime,socket)>0);
    CHECK(setenv("WAYLAND_DISPLAY",socket,1)==0);
    pthread_t server_thread, thread;
    CHECK(pthread_create(&server_thread,NULL,run_server,&server.base)==0);
    software_heap heap={0}; nx_window_desc desc={"software",32,24,0,{&heap,software_allocate,software_free}};
    nx_window *window=NULL; CHECK(nx_window_create(&desc,&window)==NX_OK); roundtrip(window);
    nx_software_view *view=NULL, *other=NULL;
    CHECK(nx_software_view_open(window,&view)==NX_OK && view);
    heap.window=window; heap.view=&view;
    CHECK(nx_software_view_open(window,&other)==NX_ERROR_STATE && !other);
    CHECK(pthread_create(&thread,NULL,wrong_thread,view)==0);
    CHECK(pthread_join(thread,NULL)==0);
    atomic_store(&server.capture,1);
    nx_software_view_outcome outcome;
    heap.fail_next=1;
    CHECK(nx_software_view_render(view,NULL,0,(nx_color){0},&outcome)==NX_ERROR_MEMORY);
    nx_command bad={.type=NX_COMMAND_POP_CLIP};
    CHECK(nx_software_view_render(view,&bad,1,(nx_color){0},&outcome)==NX_ERROR_COMMAND);
    for (int i=0; i<3; ++i)
        CHECK(nx_software_view_render(view,NULL,0,(nx_color){1,.5f,.25f,.5f},&outcome)==NX_OK && outcome==NX_SOFTWARE_VIEW_READY);
    CHECK(nx_software_view_render(view,NULL,0,(nx_color){0},&outcome)==NX_OK && outcome==NX_SOFTWARE_VIEW_BUSY);
    roundtrip(window);
    CHECK(atomic_load(&server.captured)==3);
    CHECK(atomic_load(&server.format)==WL_SHM_FORMAT_ARGB8888);
    CHECK(atomic_load(&server.pixel)==0x80804020u);
    atomic_store(&server.capture,0);
    atomic_store(&server.release_requested,1);
    struct wl_display *display=nx_window_wayland_display(window);
    struct wl_event_queue *queue=wl_display_create_queue(display); CHECK(queue);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip_queue(display,queue)>=0);
    wl_event_queue_destroy(queue);
    struct timespec before, after;
    CHECK(clock_gettime(CLOCK_MONOTONIC,&before)==0);
    CHECK(nx_window_poll(window,1000)==NX_OK);
    CHECK(clock_gettime(CLOCK_MONOTONIC,&after)==0);
    CHECK((after.tv_sec-before.tv_sec)*1000000000LL + after.tv_nsec-before.tv_nsec < 500000000LL);
    CHECK(nx_software_view_render(view,NULL,0,(nx_color){0,0,0,1},&outcome)==NX_OK && outcome==NX_SOFTWARE_VIEW_READY);
    CHECK(nx_software_view_close(&view)==NX_OK && !view);
    CHECK(nx_software_view_open(window,&view)==NX_OK && view);
    CHECK(nx_software_view_render(view,NULL,0,(nx_color){0,0,1,1},&outcome)==NX_OK && outcome==NX_SOFTWARE_VIEW_READY);
    heap.destroy_next=1;
    CHECK(nx_software_view_render(view,NULL,0,(nx_color){0},&outcome)==NX_ERROR_STATE);
    CHECK(heap.reentrant_close==NX_ERROR_STATE && view);
    CHECK(nx_software_view_render(view,NULL,0,(nx_color){0},&outcome)==NX_ERROR_STATE);
    CHECK(nx_software_view_close(&view)==NX_OK && !view);
    CHECK(heap.base.live==0);
    wl_display_terminate(server.base.display);
    CHECK(pthread_join(server_thread,NULL)==0);
    wl_display_destroy_clients(server.base.display);
    wl_display_destroy(server.base.display);
    CHECK(unlink(path)==0 || access(path,F_OK)!=0);
    CHECK(rmdir(runtime)==0);
    return 0;
}
