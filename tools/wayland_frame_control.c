#define _GNU_SOURCE
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

typedef struct state {
    struct wl_display *display;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *base;
    struct wl_callback *frame;
    unsigned configured, frames, released;
} state;
static double now(void) {
    struct timespec ts; assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
    return ts.tv_sec + ts.tv_nsec/1e9;
}
static void ping(void *data, struct xdg_wm_base *base, uint32_t serial) {
    (void)data; xdg_wm_base_pong(base, serial);
}
static const struct xdg_wm_base_listener base_listener = {ping};
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    state *s = data;
    if (!strcmp(interface, "wl_compositor")) s->compositor = wl_registry_bind(registry,name,&wl_compositor_interface,version < 4 ? version : 4);
    else if (!strcmp(interface, "wl_shm")) s->shm = wl_registry_bind(registry,name,&wl_shm_interface,1);
    else if (!strcmp(interface, "xdg_wm_base")) {
        s->base=wl_registry_bind(registry,name,&xdg_wm_base_interface,1);
        assert(xdg_wm_base_add_listener(s->base,&base_listener,s) == 0);
    }
}
static void removed(void *data, struct wl_registry *registry, uint32_t name) { (void)data;(void)registry;(void)name; }
static const struct wl_registry_listener registry_listener = {global,removed};
static void configure(void *data, struct xdg_surface *surface, uint32_t serial) {
    state *s=data; xdg_surface_ack_configure(surface,serial); ++s->configured;
}
static const struct xdg_surface_listener surface_listener = {configure};
static void top_configure(void *data,struct xdg_toplevel *top,int32_t w,int32_t h,struct wl_array *states) {
    (void)data;(void)top;(void)w;(void)h;(void)states;
}
static void top_close(void *data,struct xdg_toplevel *top) { (void)data;(void)top; }
static const struct xdg_toplevel_listener top_listener = {.configure=top_configure,.close=top_close};
static void frame_done(void *data,struct wl_callback *callback,uint32_t time) {
    (void)time;state *s=data;++s->frames;wl_callback_destroy(callback);s->frame=NULL;
}
static const struct wl_callback_listener frame_listener = {frame_done};
static void buffer_release(void *data,struct wl_buffer *buffer) { (void)buffer;++((state *)data)->released; }
static const struct wl_buffer_listener buffer_listener = {buffer_release};
static int until(state *s, unsigned *value, double deadline) {
    while (!*value && now()<deadline) {
        while (wl_display_prepare_read(s->display)!=0) if(wl_display_dispatch_pending(s->display)<0)return -1;
        if (wl_display_flush(s->display)<0) { wl_display_cancel_read(s->display);return -1; }
        struct pollfd fd={wl_display_get_fd(s->display),POLLIN,0};
        int result=poll(&fd,1,50);
        if(result>0 && (fd.revents&POLLIN)) {
            if(wl_display_read_events(s->display)<0)return -1;
        } else {wl_display_cancel_read(s->display);if(result<0&&errno!=EINTR)return -1;}
        if(wl_display_dispatch_pending(s->display)<0)return -1;
    }
    return *value ? 0 : 2;
}
int main(void) {
    state s={0};s.display=wl_display_connect(NULL);assert(s.display);
    struct wl_registry *registry=wl_display_get_registry(s.display);assert(registry);
    assert(wl_registry_add_listener(registry,&registry_listener,&s)==0);
    assert(wl_display_roundtrip(s.display)>=0 && s.compositor && s.shm && s.base);
    struct wl_surface *surface=wl_compositor_create_surface(s.compositor);assert(surface);
    struct xdg_surface *xdg=xdg_wm_base_get_xdg_surface(s.base,surface);assert(xdg);
    struct xdg_toplevel *top=xdg_surface_get_toplevel(xdg);assert(top);
    assert(xdg_surface_add_listener(xdg,&surface_listener,&s)==0);
    assert(xdg_toplevel_add_listener(top,&top_listener,&s)==0);
    xdg_toplevel_set_title(top,"NexaUI independent SHM frame control");
    xdg_toplevel_set_app_id(top,"nexaui-frame-control");
    xdg_surface_set_window_geometry(xdg,0,0,128,96);wl_surface_commit(surface);
    int result=until(&s,&s.configured,now()+5);assert(result==0);
    size_t bytes=128*96*4;int fd=memfd_create("nexaui-frame-control",MFD_CLOEXEC);assert(fd>=0);
    assert(ftruncate(fd,(off_t)bytes)==0);
    uint32_t *pixels=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);assert(pixels!=MAP_FAILED);
    for(unsigned i=0;i<128*96;++i)pixels[i]=0xff1353c9;
    struct wl_shm_pool *pool=wl_shm_create_pool(s.shm,fd,(int32_t)bytes);assert(pool);
    struct wl_buffer *buffer=wl_shm_pool_create_buffer(pool,0,128,96,512,WL_SHM_FORMAT_XRGB8888);assert(buffer);
    assert(wl_buffer_add_listener(buffer,&buffer_listener,&s)==0);wl_shm_pool_destroy(pool);close(fd);
    wl_surface_attach(surface,buffer,0,0);wl_surface_damage(surface,0,0,128,96);
    s.frame=wl_surface_frame(surface);assert(s.frame);
    assert(wl_callback_add_listener(s.frame,&frame_listener,&s)==0);wl_surface_commit(surface);
    double start=now();result=until(&s,&s.frames,start+5);
    printf("SHM_FRAME configured=%u done=%u released=%u seconds=%.6f result=%d\n",s.configured,s.frames,s.released,now()-start,result);fflush(stdout);
    if(s.frame)wl_callback_destroy(s.frame);
    wl_buffer_destroy(buffer);xdg_toplevel_destroy(top);xdg_surface_destroy(xdg);wl_surface_destroy(surface);
    xdg_wm_base_destroy(s.base);wl_shm_destroy(s.shm);wl_compositor_destroy(s.compositor);wl_registry_destroy(registry);
    wl_display_flush(s.display);wl_display_disconnect(s.display);assert(munmap(pixels,bytes)==0);
    return result==0 ? 0 : 2;
}
