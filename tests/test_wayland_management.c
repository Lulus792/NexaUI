#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include "wayland/window_internal.h"
#include "xdg-shell-server-protocol.h"
#include "xdg-decoration-unstable-v1-server-protocol.h"
#include <wayland-server.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e); abort(); } } while (0)
typedef struct server {
    struct wl_display *display;
    struct wl_resource *surface,*xdg,*top,*seat,*pointer,*decoration;
    int configured,reject;
    uint32_t serial,state;
    atomic_int command,moves,resizes,minimizes,decorations,attaches,acks;
} server;
static void destroy(struct wl_client *c,struct wl_resource *r) { (void)c; wl_resource_destroy(r); }
static void configure(server *s) {
    struct wl_array a; wl_array_init(&a);
    uint32_t *v=wl_array_add(&a,sizeof(*v)); CHECK(v); *v=XDG_TOPLEVEL_STATE_ACTIVATED;
    if (s->state&1u) { v=wl_array_add(&a,sizeof(*v)); CHECK(v); *v=XDG_TOPLEVEL_STATE_MAXIMIZED; }
    if (s->state&2u) { v=wl_array_add(&a,sizeof(*v)); CHECK(v); *v=XDG_TOPLEVEL_STATE_FULLSCREEN; }
    xdg_toplevel_send_configure(s->top,s->state ? 480 : 0,s->state ? 240 : 0,&a); wl_array_release(&a);
    xdg_surface_send_configure(s->xdg,++s->serial);
}
static void caps(server *s,int enabled) {
    struct wl_array a; wl_array_init(&a);
    if (enabled) {
        uint32_t *v=wl_array_add(&a,2*sizeof(*v)); CHECK(v);
        v[0]=XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE; v[1]=XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE;
    }
    xdg_toplevel_send_wm_capabilities(s->top,&a); wl_array_release(&a);
}
static void commit(struct wl_client *c,struct wl_resource *r) {
    (void)c; server *s=wl_resource_get_user_data(r); if (r!=s->surface || !s->top) return;
    if (!s->configured) {
        s->configured=1; caps(s,1);
        if (s->decoration) zxdg_toplevel_decoration_v1_send_configure(s->decoration,s->reject ? 2 : 1);
        configure(s); return;
    }
    switch (atomic_exchange(&s->command,0)) {
    case 1: /* Zero is a valid native press serial; it must not be used as a sentinel. */
        CHECK(s->pointer); wl_pointer_send_enter(s->pointer,100,s->surface,wl_fixed_from_int(20),wl_fixed_from_int(20));
        wl_pointer_send_button(s->pointer,0,1,0x110,WL_POINTER_BUTTON_STATE_PRESSED); break;
    case 2: wl_pointer_send_button(s->pointer,1,2,0x110,WL_POINTER_BUTTON_STATE_RELEASED); break;
    case 3: caps(s,0); break;
    case 4: zxdg_toplevel_decoration_v1_send_configure(s->decoration,2); configure(s); break;
    case 5: wl_pointer_send_leave(s->pointer,2,s->surface); break;
    case 6: wl_seat_send_capabilities(s->seat,0); break;
    case 7: xdg_toplevel_send_close(s->top); break;
    case 8: s->state=2; configure(s); break;
    case 10: s->state=3; configure(s); break;
    case 9: {
        struct wl_array a; wl_array_init(&a);
        xdg_toplevel_send_configure(s->top,300,150,&a); wl_array_release(&a);
        xdg_surface_send_configure(s->xdg,++s->serial); break;
    }
    }
}
static void attach(struct wl_client *c,struct wl_resource *r,struct wl_resource *buffer,int32_t x,int32_t y) {
    (void)c;(void)x;(void)y; server *s=wl_resource_get_user_data(r);
    if (buffer) { atomic_fetch_add(&s->attaches,1); wl_buffer_send_release(buffer); }
}
static void damage(struct wl_client *c,struct wl_resource *r,int32_t x,int32_t y,int32_t w,int32_t h) { (void)c;(void)r;(void)x;(void)y;(void)w;(void)h; }
static void scale(struct wl_client *c,struct wl_resource *r,int32_t n) { (void)c;(void)r; CHECK(n>0); }
static const struct wl_surface_interface surface_impl={.destroy=destroy,.attach=attach,.damage=damage,.commit=commit,.set_buffer_scale=scale};
static void surface(struct wl_client *c,struct wl_resource *r,uint32_t id) {
    server *s=wl_resource_get_user_data(r); struct wl_resource *v=wl_resource_create(c,&wl_surface_interface,4,id); CHECK(v);
    if (!s->surface) s->surface=v; wl_resource_set_implementation(v,&surface_impl,s,NULL);
}
static const struct wl_compositor_interface compositor_impl={.create_surface=surface};
static void bind_compositor(struct wl_client *c,void *s,uint32_t version,uint32_t id) {
    struct wl_resource *r=wl_resource_create(c,&wl_compositor_interface,version,id); CHECK(r); wl_resource_set_implementation(r,&compositor_impl,s,NULL);
}
static void cursor(struct wl_client *c,struct wl_resource *r,uint32_t serial,struct wl_resource *surface,int32_t x,int32_t y) { (void)c;(void)r;(void)serial;(void)surface;(void)x;(void)y; }
static const struct wl_pointer_interface pointer_impl={.set_cursor=cursor,.release=destroy};
static void pointer(struct wl_client *c,struct wl_resource *r,uint32_t id) {
    server *s=wl_resource_get_user_data(r); s->pointer=wl_resource_create(c,&wl_pointer_interface,5,id); CHECK(s->pointer);
    wl_resource_set_implementation(s->pointer,&pointer_impl,s,NULL);
}
static const struct wl_seat_interface seat_impl={.get_pointer=pointer,.release=destroy};
static void bind_seat(struct wl_client *c,void *data,uint32_t version,uint32_t id) {
    server *s=data; s->seat=wl_resource_create(c,&wl_seat_interface,version,id); CHECK(s->seat);
    wl_resource_set_implementation(s->seat,&seat_impl,s,NULL); wl_seat_send_capabilities(s->seat,WL_SEAT_CAPABILITY_POINTER);
}
static void string(struct wl_client *c,struct wl_resource *r,const char *text) { (void)c;(void)r;(void)text; }
static void maximize(struct wl_client *c,struct wl_resource *r) { (void)c; server *s=wl_resource_get_user_data(r); s->state|=1; configure(s); }
static void restore(struct wl_client *c,struct wl_resource *r) { (void)c; server *s=wl_resource_get_user_data(r); if(!(s->state&2u)) s->state&=~1u; configure(s); }
static void unfullscreen(struct wl_client *c,struct wl_resource *r) { (void)c; server *s=wl_resource_get_user_data(r); s->state&=~2u; configure(s); }
static void minimize(struct wl_client *c,struct wl_resource *r) { (void)c; atomic_fetch_add(&((server *)wl_resource_get_user_data(r))->minimizes,1); }
static void move(struct wl_client *c,struct wl_resource *r,struct wl_resource *seat,uint32_t serial) {
    (void)c; server *s=wl_resource_get_user_data(r); CHECK(seat==s->seat && serial==0); atomic_fetch_add(&s->moves,1);
}
static void resize(struct wl_client *c,struct wl_resource *r,struct wl_resource *seat,uint32_t serial,uint32_t edge) {
    (void)c; server *s=wl_resource_get_user_data(r); static const uint32_t expected[]={5,1,9,8,10,2,6,4};
    int i=atomic_fetch_add(&s->resizes,1); CHECK(seat==s->seat && serial==0 && i<8 && edge==expected[i]);
}
static const struct xdg_toplevel_interface top_impl={.destroy=destroy,.set_title=string,.set_app_id=string,
    .set_maximized=maximize,.unset_maximized=restore,.unset_fullscreen=unfullscreen,.set_minimized=minimize,.move=move,.resize=resize};
static void top(struct wl_client *c,struct wl_resource *r,uint32_t id) {
    server *s=wl_resource_get_user_data(r); s->top=wl_resource_create(c,&xdg_toplevel_interface,5,id); CHECK(s->top);
    wl_resource_set_implementation(s->top,&top_impl,s,NULL);
}
static void geometry(struct wl_client *c,struct wl_resource *r,int32_t x,int32_t y,int32_t w,int32_t h) { (void)c;(void)r; CHECK(x==0 && y==0 && ((w==240 && h==120) || (w==480 && h==240) || (w==300 && h==150))); }
static void ack(struct wl_client *c,struct wl_resource *r,uint32_t serial) { (void)c; server *s=wl_resource_get_user_data(r); CHECK(serial>0 && serial<=s->serial); atomic_fetch_add(&s->acks,1); }
static const struct xdg_surface_interface xdg_impl={.destroy=destroy,.get_toplevel=top,.set_window_geometry=geometry,.ack_configure=ack};
static void xdg(struct wl_client *c,struct wl_resource *r,uint32_t id,struct wl_resource *surface) {
    server *s=wl_resource_get_user_data(r); CHECK(surface==s->surface); s->xdg=wl_resource_create(c,&xdg_surface_interface,5,id); CHECK(s->xdg);
    wl_resource_set_implementation(s->xdg,&xdg_impl,s,NULL);
}
static const struct xdg_wm_base_interface wm_impl={.destroy=destroy,.get_xdg_surface=xdg};
static void bind_wm(struct wl_client *c,void *s,uint32_t version,uint32_t id) {
    struct wl_resource *r=wl_resource_create(c,&xdg_wm_base_interface,version,id); CHECK(r); wl_resource_set_implementation(r,&wm_impl,s,NULL);
}
static void mode(struct wl_client *c,struct wl_resource *r,uint32_t value) { (void)c;(void)r; CHECK(value==1); }
static const struct zxdg_toplevel_decoration_v1_interface decoration_impl={.destroy=destroy,.set_mode=mode};
static void decoration(struct wl_client *c,struct wl_resource *r,uint32_t id,struct wl_resource *toplevel) {
    server *s=wl_resource_get_user_data(r); CHECK(toplevel==s->top); s->decoration=wl_resource_create(c,&zxdg_toplevel_decoration_v1_interface,1,id); CHECK(s->decoration);
    wl_resource_set_implementation(s->decoration,&decoration_impl,s,NULL); atomic_fetch_add(&s->decorations,1);
}
static const struct zxdg_decoration_manager_v1_interface manager_impl={.destroy=destroy,.get_toplevel_decoration=decoration};
static void bind_decoration(struct wl_client *c,void *s,uint32_t version,uint32_t id) {
    struct wl_resource *r=wl_resource_create(c,&zxdg_decoration_manager_v1_interface,version,id); CHECK(r); wl_resource_set_implementation(r,&manager_impl,s,NULL);
}
static void *run(void *data) { wl_display_run(((server *)data)->display); return NULL; }
typedef struct heap { size_t live; } heap;
static void *allocate(void *data,size_t n) { void *p=malloc(n); if(p) ++((heap *)data)->live; return p; }
static void deallocate(void *data,void *p,size_t n) { (void)n; CHECK(((heap *)data)->live); --((heap *)data)->live; free(p); }
static void roundtrip(nx_window *w) { CHECK(wl_display_roundtrip(nx_window_wayland_display(w))>=0); }
static void send(server *s,nx_window *w,int command) {
    atomic_store(&s->command,command); wl_surface_commit(nx_window_wayland_surface(w)); roundtrip(w);
}
static void test(int client_mode,int reject) {
    server s={0}; s.reject=reject; heap h={0};
    char runtime[]="/tmp/nexaui-window-management-XXXXXX"; CHECK(mkdtemp(runtime));
    CHECK(!setenv("XDG_RUNTIME_DIR",runtime,1) && !setenv("NEXAUI_PLATFORM","wayland",1));
    s.display=wl_display_create(); CHECK(s.display && wl_display_init_shm(s.display)==0);
    CHECK(wl_global_create(s.display,&wl_compositor_interface,4,&s,bind_compositor));
    CHECK(wl_global_create(s.display,&wl_seat_interface,5,&s,bind_seat));
    CHECK(wl_global_create(s.display,&xdg_wm_base_interface,5,&s,bind_wm));
    CHECK(wl_global_create(s.display,&zxdg_decoration_manager_v1_interface,1,&s,bind_decoration));
    const char *socket=wl_display_add_socket_auto(s.display); CHECK(socket && !setenv("WAYLAND_DISPLAY",socket,1));
    char path[256]; CHECK(snprintf(path,sizeof(path),"%s/%s",runtime,socket)>0);
    pthread_t thread; CHECK(!pthread_create(&thread,NULL,run,&s));
    nx_window_desc d={"Wayland management",240,120,client_mode ? NX_WINDOW_CLIENT_CHROME|NX_WINDOW_STATE_EVENTS : 0,{&h,allocate,deallocate}};
    nx_window *w=NULL; nx_result result=nx_window_create(&d,&w);
    if(reject) CHECK(result==NX_ERROR_UNSUPPORTED && !w && !h.live && !atomic_load(&s.attaches));
    else {
        CHECK(result==NX_OK && w); roundtrip(w);
        CHECK(atomic_load(&s.decorations)==client_mode);
        nx_window_status status; CHECK(nx_window_get_status(w,&status)==NX_OK && status.known==14u && status.state==NX_WINDOW_STATE_ACTIVE && status.client_chrome==client_mode);
        CHECK(!(status.capabilities&NX_WINDOW_CAP_RESTORE_MINIMIZED));
        CHECK(nx_window_begin_move(w)==NX_ERROR_STATE && nx_window_begin_resize(w,(nx_window_resize_edge)9)==NX_ERROR_ARGUMENT);
        CHECK(nx_window_maximize(w)==NX_OK && nx_window_get_status(w,&status)==NX_OK && !(status.state&NX_WINDOW_STATE_MAXIMIZED));
        roundtrip(w); CHECK(nx_window_get_status(w,&status)==NX_OK && (status.state&NX_WINDOW_STATE_MAXIMIZED));
        nx_window_info dimensions; CHECK(nx_window_get_info(w,&dimensions)==NX_OK && dimensions.width==480 && dimensions.height==240);
        CHECK(nx_window_restore(w)==NX_OK); roundtrip(w); CHECK(nx_window_get_status(w,&status)==NX_OK && !(status.state&NX_WINDOW_STATE_MAXIMIZED));
        CHECK(nx_window_get_info(w,&dimensions)==NX_OK && dimensions.width==240 && dimensions.height==120);
        send(&s,w,10); CHECK(nx_window_get_status(w,&status)==NX_OK &&
            (status.state&(NX_WINDOW_STATE_FULLSCREEN|NX_WINDOW_STATE_MAXIMIZED))==(NX_WINDOW_STATE_FULLSCREEN|NX_WINDOW_STATE_MAXIMIZED));
        CHECK(nx_window_restore(w)==NX_OK); roundtrip(w); roundtrip(w); roundtrip(w);
        CHECK(nx_window_get_status(w,&status)==NX_OK && !(status.state&(NX_WINDOW_STATE_FULLSCREEN|NX_WINDOW_STATE_MAXIMIZED)));
        CHECK(nx_window_get_info(w,&dimensions)==NX_OK && dimensions.width==240 && dimensions.height==120);
        send(&s,w,8); CHECK(nx_window_get_status(w,&status)==NX_OK && (status.state&NX_WINDOW_STATE_FULLSCREEN));
        CHECK(nx_window_restore(w)==NX_OK); roundtrip(w);
        CHECK(nx_window_get_status(w,&status)==NX_OK && !(status.state&NX_WINDOW_STATE_FULLSCREEN));
        CHECK(nx_window_get_info(w,&dimensions)==NX_OK && dimensions.width==240 && dimensions.height==120);
        send(&s,w,9); CHECK(nx_window_get_info(w,&dimensions)==NX_OK && dimensions.width==300 && dimensions.height==150);
        CHECK(nx_window_maximize(w)==NX_OK); roundtrip(w); CHECK(nx_window_restore(w)==NX_OK); roundtrip(w);
        CHECK(nx_window_get_info(w,&dimensions)==NX_OK && dimensions.width==300 && dimensions.height==150);
        CHECK(nx_window_minimize(w)==NX_OK); roundtrip(w); CHECK(atomic_load(&s.minimizes)==1 && nx_window_get_status(w,&status)==NX_OK && !(status.known&NX_WINDOW_STATE_MINIMIZED));
        for(int i=0;i<9;++i) {
            send(&s,w,1); CHECK(i==8 ? nx_window_begin_move(w)==NX_OK : nx_window_begin_resize(w,(nx_window_resize_edge)(i+1))==NX_OK);
            CHECK(nx_window_begin_move(w)==NX_ERROR_STATE); roundtrip(w);
        }
        CHECK(atomic_load(&s.moves)==1 && atomic_load(&s.resizes)==8);
        send(&s,w,1); send(&s,w,2); CHECK(nx_window_begin_move(w)==NX_ERROR_STATE);
        send(&s,w,1); send(&s,w,5); CHECK(nx_window_begin_move(w)==NX_ERROR_STATE);
        send(&s,w,3); CHECK(nx_window_maximize(w)==NX_ERROR_UNSUPPORTED && nx_window_minimize(w)==NX_ERROR_UNSUPPORTED);
        nx_event e; int available; unsigned states=0,cancels=0;
        do { CHECK(nx_window_next_event(w,&e,&available)==NX_OK); if(available && e.type==NX_EVENT_WINDOW_STATE) ++states; if(available && e.type==NX_EVENT_POINTER_CANCEL) ++cancels; } while(available);
        CHECK((client_mode ? states>=3 : states==0) && cancels==9);
        send(&s,w,1); send(&s,w,6); CHECK(nx_window_begin_move(w)==NX_ERROR_STATE);
        if(client_mode) { send(&s,w,4); CHECK(nx_window_get_status(w,&status)==NX_ERROR_UNSUPPORTED && !status.known && !status.capabilities); }
        else { send(&s,w,7); CHECK(nx_window_get_status(w,&status)==NX_ERROR_STATE && !status.state && !status.known && !status.capabilities && !status.client_chrome); }
        nx_window_destroy(w); CHECK(!h.live);
    }
    wl_display_terminate(s.display); CHECK(!pthread_join(thread,NULL)); wl_display_destroy_clients(s.display); wl_display_destroy(s.display);
    CHECK(!unlink(path) || access(path,F_OK)!=0); CHECK(!rmdir(runtime));
}
int main(void) {
    test(0,0); test(1,0); test(1,1);
    puts("Wayland: real protocol configure/capabilities, decoration acceptance/refusal/cleanup, seat/zero-serial move and eight resize directions, release/leave/removal guards passed; no physical acceptance.");
    return 0;
}
