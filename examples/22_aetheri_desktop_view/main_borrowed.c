#define _POSIX_C_SOURCE 200809L
#include <nexaui/nexaui.h>
#include <nexaui/wayland_host.h>
#include "view.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
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

/* The producer owns no Wayland/NexaUI objects. The owner thread consumes the
   bounded reply and is solely responsible for any future view changes. */
static void *worker(void *data) {
    int fd=*(int *)data; struct timespec delay={0,150000000};
    while (nanosleep(&delay,&delay)<0 && errno==EINTR) {}
    unsigned char reply=42;
    ssize_t written; do { written=write(fd,&reply,1); } while (written<0 && errno==EINTR);
    return written==1?NULL:data;
}

static unsigned char *load_font(const char *path,size_t *length) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;long n=0;unsigned char *p=NULL;
    if(!fseek(f,0,SEEK_END) && (n=ftell(f))>0 && n<64*1024*1024 && !fseek(f,0,SEEK_SET) &&
       (p=malloc((size_t)n)) && fread(p,1,(size_t)n,f)==(size_t)n)*length=(size_t)n;
    else {free(p);p=NULL;}
    if(fclose(f)){free(p);p=NULL;}return p;
}

int main(int argc,char **argv) {
    if(argc!=2){fputs("usage: aetheri_desktop_borrowed FONT.ttf\n",stderr);return 2;}
    int okay=0; host h={0}; nx_context *ui=NULL; nx_wayland_shm_host *renderer=NULL;
    size_t font_length=0;unsigned char *font=load_font(argv[1],&font_length);
    aetheri_desktop_view *view=NULL;struct ae_desktop_model model;ae_desktop_model_init(&model);
    ae_desktop_model_search(&model,0,true);aetheri_desktop_intent intent;
    struct wl_display *display=wl_display_connect(NULL);
    struct wl_event_queue *queue=NULL; struct wl_registry *registry=NULL;
    struct wl_surface *surface=NULL; struct xdg_surface *xdg=NULL;
    struct xdg_toplevel *toplevel=NULL;
    int fds[2]={-1,-1}, worker_running=0; pthread_t worker_thread;
    if (!font || !display || !(queue=wl_display_create_queue(display)) ||
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
    if (h.closed || !aetheri_desktop_view_create(&model,font,font_length,&view) ||
        !(ui=aetheri_desktop_view_context(view)) ||
        nx_wayland_shm_host_create(display,h.shm,surface,queue,&renderer)!=NX_OK ||
        !aetheri_desktop_view_frame(view,640,480,1,&intent) ||
        intent.kind!=AETHERI_DESKTOP_NONE) goto cleanup;
    const nx_command *commands=NULL; size_t count=0;
    nx_wayland_shm_outcome outcome;
    if (nx_frame_commands(ui,&commands,&count)!=NX_OK ||
        nx_wayland_shm_host_present(renderer,(nx_pixel_size){640,480},1,commands,count,
                                    (nx_color){0,0,0,0},&outcome)!=NX_OK ||
        outcome!=NX_WAYLAND_SHM_READY || wl_display_roundtrip_queue(display,queue)<0)
        goto cleanup;
    if (pipe(fds)!=0 || pthread_create(&worker_thread,NULL,worker,&fds[1])!=0) goto cleanup;
    worker_running=1;
    uint32_t events=0;
    for (unsigned i=0;i<10 && !events;++i)
        if (nx_wayland_shm_host_poll(renderer,1000,fds[0],&events)!=NX_OK) goto cleanup;
    unsigned char reply=0;
    if (!(events&NX_WINDOW_FD_READABLE) || (events&NX_WINDOW_FD_ERROR) ||
        read(fds[0],&reply,1)!=1 || reply!=42) goto cleanup;
    /* The host owns the model and applies an asynchronous update itself. */
    if (!ae_desktop_model_text(&model,"Terminal") ||
        !aetheri_desktop_view_frame(view,640,480,2,&intent) ||
        intent.kind!=AETHERI_DESKTOP_NONE ||
        nx_frame_commands(ui,&commands,&count)!=NX_OK ||
        nx_wayland_shm_host_present(renderer,(nx_pixel_size){640,480},1,commands,count,
                                    (nx_color){0,0,0,0},&outcome)!=NX_OK ||
        outcome!=NX_WAYLAND_SHM_READY || wl_display_roundtrip_queue(display,queue)<0)
        goto cleanup;
    puts("Host configured its surface, presented two model revisions and received a worker reply.");
    okay=1;
cleanup:
    if (worker_running) {
        void *worker_result=NULL;
        if (pthread_join(worker_thread,&worker_result)!=0 || worker_result) okay=0;
    }
    if (fds[0]>=0) close(fds[0]);
    if (fds[1]>=0) close(fds[1]);
    if (renderer && nx_wayland_shm_host_destroy(&renderer)!=NX_OK) okay=0;
    aetheri_desktop_view_destroy(view);free(font);
    if (toplevel) xdg_toplevel_destroy(toplevel);
    if (xdg) xdg_surface_destroy(xdg);
    if (surface) wl_surface_destroy(surface);
    if (h.wm) xdg_wm_base_destroy(h.wm);
    if (h.shm) wl_shm_destroy(h.shm);
    if (h.compositor) wl_compositor_destroy(h.compositor);
    if (registry) wl_registry_destroy(registry);
    if (queue) wl_event_queue_destroy(queue);
    if (display) wl_display_disconnect(display);
    if (!okay) fputs("Borrowed-surface desktop example failed.\n",stderr);
    return okay?0:1;
}
