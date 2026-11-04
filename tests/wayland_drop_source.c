#define _GNU_SOURCE
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Independent Wayland drag source: this executable deliberately does not link NexaUI. */
typedef struct source {
    struct wl_display *display; struct wl_compositor *compositor; struct wl_shm *shm;
    struct wl_seat *seat; struct wl_pointer *pointer; struct wl_data_device_manager *manager;
    struct wl_data_device *device; struct wl_data_source *data;
    struct wl_data_offer *offer;
    struct xdg_wm_base *wm; struct xdg_surface *xdg; struct xdg_toplevel *top;
    struct wl_surface *surface; struct wl_buffer *buffer; int done, failed, started;
} source;
static void ping(void *p, struct xdg_wm_base *wm, uint32_t serial) {(void)p;xdg_wm_base_pong(wm,serial);}
static const struct xdg_wm_base_listener wm_listener={.ping=ping};
static void configured(void *p, struct xdg_surface *xdg, uint32_t serial) {
    source *s=p; xdg_surface_ack_configure(xdg,serial);
    wl_surface_attach(s->surface,s->buffer,0,0); wl_surface_damage(s->surface,0,0,220,150);
    wl_surface_commit(s->surface); puts("SOURCE_READY");fflush(stdout);
}
static const struct xdg_surface_listener xdg_listener={.configure=configured};
static void top_configure(void *p,struct xdg_toplevel *t,int32_t w,int32_t h,struct wl_array *a){(void)p;(void)t;(void)w;(void)h;(void)a;}
static void top_close(void *p,struct xdg_toplevel *t){(void)t;((source *)p)->failed=1;}
static const struct xdg_toplevel_listener top_listener={.configure=top_configure,.close=top_close};
static void target(void *p,struct wl_data_source *d,const char *mime){(void)p;(void)d;(void)mime;}
static void send_data(void *p,struct wl_data_source *d,const char *mime,int32_t fd){
    (void)d;source *s=p;const char payload[]="file:///tmp/nexaui%20drop.txt\r\n";
    if(strcmp(mime,"text/uri-list") || write(fd,payload,sizeof(payload)-1)!=(ssize_t)(sizeof(payload)-1))s->failed=1;
    close(fd);
}
static void cancelled(void *p,struct wl_data_source *d){(void)d;source *s=p;s->failed=1;s->done=1;}
static void drop_performed(void *p,struct wl_data_source *d){(void)p;(void)d;}
static void finished(void *p,struct wl_data_source *d){(void)d;source *s=p;puts("SOURCE_FINISHED");fflush(stdout);s->done=1;}
static void action(void *p,struct wl_data_source *d,uint32_t a){(void)p;(void)d;printf("SOURCE_ACTION %u\n",a);fflush(stdout);}
static const struct wl_data_source_listener data_listener={.target=target,.send=send_data,.cancelled=cancelled,.dnd_drop_performed=drop_performed,.dnd_finished=finished,.action=action};
static void offered_mime(void *p,struct wl_data_offer *o,const char *mime){(void)p;(void)o;(void)mime;}
static void offered_actions(void *p,struct wl_data_offer *o,uint32_t a){(void)p;(void)o;(void)a;}
static const struct wl_data_offer_listener offer_listener={.offer=offered_mime,.source_actions=offered_actions,.action=offered_actions};
static void device_offer(void *p,struct wl_data_device *d,struct wl_data_offer *o){
    (void)d;source *s=p;if(s->offer)wl_data_offer_destroy(s->offer);s->offer=o;wl_data_offer_add_listener(o,&offer_listener,s);
}
static void device_enter(void *p,struct wl_data_device *d,uint32_t serial,struct wl_surface *surface,wl_fixed_t x,wl_fixed_t y,struct wl_data_offer *o){
    (void)p;(void)d;(void)surface;(void)x;(void)y;
    if(o)wl_data_offer_accept(o,serial,NULL);
}
static void device_leave(void *p,struct wl_data_device *d){(void)d;source *s=p;if(s->offer){wl_data_offer_destroy(s->offer);s->offer=NULL;}}
static void device_motion(void *p,struct wl_data_device *d,uint32_t time,wl_fixed_t x,wl_fixed_t y){(void)p;(void)d;(void)time;(void)x;(void)y;}
static void device_drop(void *p,struct wl_data_device *d){(void)p;(void)d;}
static void device_selection(void *p,struct wl_data_device *d,struct wl_data_offer *o){
    (void)d;source *s=p;if(o){wl_data_offer_destroy(o);if(s->offer==o)s->offer=NULL;}
}
static const struct wl_data_device_listener device_listener={.data_offer=device_offer,.enter=device_enter,.leave=device_leave,.motion=device_motion,.drop=device_drop,.selection=device_selection};
static void enter(void *p,struct wl_pointer *ptr,uint32_t serial,struct wl_surface *surface,wl_fixed_t x,wl_fixed_t y){
    (void)p;(void)ptr;(void)serial;(void)surface;printf("SOURCE_POINTER %.0f %.0f\n",wl_fixed_to_double(x),wl_fixed_to_double(y));fflush(stdout);
}
static void leave(void *p,struct wl_pointer *ptr,uint32_t serial,struct wl_surface *surface){(void)p;(void)ptr;(void)serial;(void)surface;}
static void motion(void *p,struct wl_pointer *ptr,uint32_t time,wl_fixed_t x,wl_fixed_t y){
    (void)p;(void)ptr;(void)time;printf("SOURCE_POINTER %.0f %.0f\n",wl_fixed_to_double(x),wl_fixed_to_double(y));fflush(stdout);
}
static void button(void *p,struct wl_pointer *ptr,uint32_t serial,uint32_t time,uint32_t btn,uint32_t state){
    (void)ptr;(void)time;source *s=p;
    if(btn==BTN_LEFT && state==WL_POINTER_BUTTON_STATE_PRESSED && !s->started) {
        s->started=1;wl_data_device_start_drag(s->device,s->data,s->surface,NULL,serial);
    }
}
static void axis(void *p,struct wl_pointer *ptr,uint32_t time,uint32_t a,wl_fixed_t v){(void)p;(void)ptr;(void)time;(void)a;(void)v;}
static const struct wl_pointer_listener pointer_listener={.enter=enter,.leave=leave,.motion=motion,.button=button,.axis=axis};
static void capabilities(void *p,struct wl_seat *seat,uint32_t caps){
    source *s=p;if((caps & WL_SEAT_CAPABILITY_POINTER) && !s->pointer){s->pointer=wl_seat_get_pointer(seat);wl_pointer_add_listener(s->pointer,&pointer_listener,s);}
}
static void seat_name(void *p,struct wl_seat *seat,const char *name){(void)p;(void)seat;(void)name;}
static const struct wl_seat_listener seat_listener={.capabilities=capabilities,.name=seat_name};
static void global(void *p,struct wl_registry *r,uint32_t name,const char *iface,uint32_t version){
    source *s=p;
    if(!strcmp(iface,wl_compositor_interface.name))s->compositor=wl_registry_bind(r,name,&wl_compositor_interface,1);
    else if(!strcmp(iface,wl_shm_interface.name))s->shm=wl_registry_bind(r,name,&wl_shm_interface,1);
    else if(!strcmp(iface,wl_seat_interface.name)){s->seat=wl_registry_bind(r,name,&wl_seat_interface,1);wl_seat_add_listener(s->seat,&seat_listener,s);}
    else if(!strcmp(iface,wl_data_device_manager_interface.name) && version>=3)s->manager=wl_registry_bind(r,name,&wl_data_device_manager_interface,3);
    else if(!strcmp(iface,xdg_wm_base_interface.name)){s->wm=wl_registry_bind(r,name,&xdg_wm_base_interface,1);xdg_wm_base_add_listener(s->wm,&wm_listener,s);}
}
static void removed(void *p,struct wl_registry *r,uint32_t name){(void)p;(void)r;(void)name;}
static const struct wl_registry_listener registry_listener={.global=global,.global_remove=removed};
int main(void){
    source s={0};s.display=wl_display_connect(NULL);if(!s.display)return 1;
    struct wl_registry *registry=wl_display_get_registry(s.display);wl_registry_add_listener(registry,&registry_listener,&s);
    if(wl_display_roundtrip(s.display)<0 || !s.compositor || !s.shm || !s.seat || !s.manager || !s.wm)return 2;
    if(wl_display_roundtrip(s.display)<0 || !s.pointer)return 3;
    s.device=wl_data_device_manager_get_data_device(s.manager,s.seat);wl_data_device_add_listener(s.device,&device_listener,&s);
    s.data=wl_data_device_manager_create_data_source(s.manager);
    wl_data_source_add_listener(s.data,&data_listener,&s);wl_data_source_offer(s.data,"text/uri-list");
    wl_data_source_set_actions(s.data,WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
    int fd=memfd_create("nexaui-dnd-source",MFD_CLOEXEC);size_t size=220u*150u*4u;
    if(fd<0 || ftruncate(fd,(off_t)size)!=0)return 4;
    void *pixels=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);if(pixels==MAP_FAILED)return 4;
    for(size_t i=0;i<size/4;i++)((uint32_t *)pixels)[i]=0xff3475b5u;
    struct wl_shm_pool *pool=wl_shm_create_pool(s.shm,fd,(int32_t)size);
    s.buffer=wl_shm_pool_create_buffer(pool,0,220,150,220*4,WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);close(fd);
    s.surface=wl_compositor_create_surface(s.compositor);s.xdg=xdg_wm_base_get_xdg_surface(s.wm,s.surface);
    xdg_surface_add_listener(s.xdg,&xdg_listener,&s);s.top=xdg_surface_get_toplevel(s.xdg);
    xdg_toplevel_add_listener(s.top,&top_listener,&s);xdg_toplevel_set_title(s.top,"NexaUI independent drag source");wl_surface_commit(s.surface);
    while(!s.done && !s.failed && wl_display_dispatch(s.display)>=0) {
        if(s.started==1) {
            if(wl_display_roundtrip(s.display)<0){s.failed=1;break;}
            s.started=2;puts("SOURCE_STARTED");fflush(stdout);
        }
    }
    (void)wl_display_dispatch_pending(s.display);if(s.offer)wl_data_offer_destroy(s.offer);
    wl_data_source_destroy(s.data);wl_data_device_release(s.device);wl_pointer_destroy(s.pointer);wl_seat_destroy(s.seat);
    xdg_toplevel_destroy(s.top);xdg_surface_destroy(s.xdg);wl_surface_destroy(s.surface);wl_buffer_destroy(s.buffer);
    xdg_wm_base_destroy(s.wm);wl_shm_destroy(s.shm);wl_compositor_destroy(s.compositor);wl_data_device_manager_destroy(s.manager);wl_registry_destroy(registry);
    wl_display_disconnect(s.display);munmap(pixels,size);return s.done && !s.failed?0:5;
}
