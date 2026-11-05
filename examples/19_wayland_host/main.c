#include <nexaui/nexaui.h>
#include <nexaui/wayland_host.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

typedef struct host {
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *wm;
    int configured, closed;
} host;

static void wm_ping(void *data,struct xdg_wm_base *wm,uint32_t serial) {
    (void)data; xdg_wm_base_pong(wm,serial);
}
static const struct xdg_wm_base_listener wm_listener={.ping=wm_ping};
static void global(void *data,struct wl_registry *registry,uint32_t name,
                   const char *interface,uint32_t version) {
    host *h=data; (void)version;
    if (!strcmp(interface,"wl_compositor"))
        h->compositor=wl_registry_bind(registry,name,&wl_compositor_interface,1);
    else if (!strcmp(interface,"wl_shm"))
        h->shm=wl_registry_bind(registry,name,&wl_shm_interface,1);
    else if (!strcmp(interface,"xdg_wm_base")) {
        h->wm=wl_registry_bind(registry,name,&xdg_wm_base_interface,1);
        if (h->wm) xdg_wm_base_add_listener(h->wm,&wm_listener,h);
    }
}
static void global_remove(void *data,struct wl_registry *registry,uint32_t name) {
    (void)data; (void)registry; (void)name;
}
static const struct wl_registry_listener registry_listener={global,global_remove};
static void surface_configure(void *data,struct xdg_surface *surface,uint32_t serial) {
    host *h=data; xdg_surface_ack_configure(surface,serial); h->configured=1;
}
static const struct xdg_surface_listener surface_listener={.configure=surface_configure};
static void toplevel_configure(void *data,struct xdg_toplevel *toplevel,
                               int32_t width,int32_t height,struct wl_array *states) {
    (void)data;(void)toplevel;(void)width;(void)height;(void)states;
}
static void toplevel_close(void *data,struct xdg_toplevel *toplevel) {
    (void)toplevel; ((host *)data)->closed=1;
}
static const struct xdg_toplevel_listener toplevel_listener={
    .configure=toplevel_configure,.close=toplevel_close
};

int main(void) {
    int okay=0; host h={0}; nx_context *ui=NULL; nx_wayland_shm_host *renderer=NULL;
    struct wl_display *display=wl_display_connect(NULL);
    struct wl_event_queue *queue=NULL; struct wl_registry *registry=NULL;
    struct wl_surface *surface=NULL; struct xdg_surface *xdg=NULL;
    struct xdg_toplevel *toplevel=NULL;
    if (!display || !(queue=wl_display_create_queue(display)) ||
        !(registry=wl_display_get_registry(display))) goto cleanup;
    wl_proxy_set_queue((struct wl_proxy *)registry,queue);
    if (wl_registry_add_listener(registry,&registry_listener,&h)!=0 ||
        wl_display_roundtrip_queue(display,queue)<0 || !h.compositor || !h.shm || !h.wm)
        goto cleanup;
    if (!(surface=wl_compositor_create_surface(h.compositor)) ||
        !(xdg=xdg_wm_base_get_xdg_surface(h.wm,surface)) ||
        xdg_surface_add_listener(xdg,&surface_listener,&h)!=0 ||
        !(toplevel=xdg_surface_get_toplevel(xdg)) ||
        xdg_toplevel_add_listener(toplevel,&toplevel_listener,&h)!=0) goto cleanup;
    xdg_toplevel_set_title(toplevel,"NexaUI host-owned surface example");
    wl_surface_commit(surface); /* A role receives initial configure before a buffer. */
    while (!h.configured && !h.closed)
        if (wl_display_dispatch_queue(display,queue)<0) goto cleanup;
    if (h.closed || nx_context_create(NULL,&ui)!=NX_OK ||
        nx_wayland_shm_host_create(display,h.shm,surface,queue,&renderer)!=NX_OK ||
        nx_begin_frame(ui,320,80,0)!=NX_OK ||
        nx_fill_round_rect(ui,(nx_rect){8,8,304,64},12,(nx_color){.2f,.5f,.9f,.8f})!=NX_OK ||
        nx_end_frame(ui)!=NX_OK) goto cleanup;
    const nx_command *commands=NULL; size_t count=0;
    nx_wayland_shm_outcome outcome;
    if (nx_frame_commands(ui,&commands,&count)!=NX_OK ||
        nx_wayland_shm_host_present(renderer,(nx_pixel_size){320,80},1,commands,count,
                                    (nx_color){0,0,0,0},&outcome)!=NX_OK ||
        outcome!=NX_WAYLAND_SHM_READY || wl_display_roundtrip_queue(display,queue)<0)
        goto cleanup;
    puts("Host configured and committed one NexaUI frame on its own surface.");
    okay=1;
cleanup:
    if (renderer && nx_wayland_shm_host_destroy(&renderer)!=NX_OK) okay=0;
    nx_context_destroy(ui);
    if (toplevel) xdg_toplevel_destroy(toplevel);
    if (xdg) xdg_surface_destroy(xdg);
    if (surface) wl_surface_destroy(surface);
    if (h.wm) xdg_wm_base_destroy(h.wm);
    if (h.shm) wl_shm_destroy(h.shm);
    if (h.compositor) wl_compositor_destroy(h.compositor);
    if (registry) wl_registry_destroy(registry);
    if (queue) wl_event_queue_destroy(queue);
    if (display) wl_display_disconnect(display);
    if (!okay) fputs("Host-owned surface example failed.\n",stderr);
    return okay?0:1;
}
