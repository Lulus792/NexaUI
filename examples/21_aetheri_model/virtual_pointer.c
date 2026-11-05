#define _POSIX_C_SOURCE 200809L
/* Test-only wlroots virtual pointer. Never connect to an ordinary session. */
#include <wayland-client.h>
#include "virtual-pointer-client.h"
#include <linux/input-event-codes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct globals { struct wl_seat *seat; struct zwlr_virtual_pointer_manager_v1 *manager; };
static void global(void *data,struct wl_registry *registry,uint32_t name,const char *interface,uint32_t version) {
    struct globals *g=data; (void)version;
    if(!strcmp(interface,"wl_seat") && !g->seat)
        g->seat=wl_registry_bind(registry,name,&wl_seat_interface,1);
    if(!strcmp(interface,"zwlr_virtual_pointer_manager_v1") && !g->manager)
        g->manager=wl_registry_bind(registry,name,&zwlr_virtual_pointer_manager_v1_interface,1);
}
static void remove_global(void *data,struct wl_registry *registry,uint32_t name) {
    (void)data;(void)registry;(void)name;
}
static void pause_input(void) {struct timespec delay={0,150000000}; (void)nanosleep(&delay,NULL);}
int main(int argc,char **argv) {
    const char *guard=getenv("NEXAUI_TEST_PRIVATE_SWAY"),*socket=getenv("SWAYSOCK");
    if(argc!=5 || !guard || strcmp(guard,"1") || !socket || !*socket) return 2;
    unsigned long values[4];
    for(int i=0;i<4;++i) {char *end=NULL; values[i]=strtoul(argv[i+1],&end,10);
        if(!*argv[i+1] || !end || *end || values[i]>UINT32_MAX) return 2;}
    if(!values[2] || !values[3] || values[0]>=values[2] || values[1]>=values[3]) return 2;
    int ok=0; struct globals g={0}; struct wl_display *display=wl_display_connect(NULL);
    struct wl_registry *registry=NULL; struct zwlr_virtual_pointer_v1 *pointer=NULL;
    if(!display) goto done;
    registry=wl_display_get_registry(display); if(!registry) goto done;
    static const struct wl_registry_listener listener={global,remove_global};
    if(wl_registry_add_listener(registry,&listener,&g)<0 || wl_display_roundtrip(display)<0 ||
       !g.seat || !g.manager) goto done;
    pointer=zwlr_virtual_pointer_manager_v1_create_virtual_pointer(g.manager,g.seat);
    if(!pointer || wl_display_roundtrip(display)<0) goto done;
    pause_input();
    zwlr_virtual_pointer_v1_motion_absolute(pointer,1,(uint32_t)values[0],(uint32_t)values[1],
                                           (uint32_t)values[2],(uint32_t)values[3]);
    zwlr_virtual_pointer_v1_frame(pointer); if(wl_display_roundtrip(display)<0) goto done;
    pause_input();
    zwlr_virtual_pointer_v1_button(pointer,2,BTN_LEFT,WL_POINTER_BUTTON_STATE_PRESSED);
    zwlr_virtual_pointer_v1_frame(pointer); if(wl_display_roundtrip(display)<0) goto done;
    pause_input();
    zwlr_virtual_pointer_v1_button(pointer,3,BTN_LEFT,WL_POINTER_BUTTON_STATE_RELEASED);
    zwlr_virtual_pointer_v1_frame(pointer); if(wl_display_roundtrip(display)<0) goto done;
    pause_input(); ok=1;
done:
    if(pointer) zwlr_virtual_pointer_v1_destroy(pointer);
    if(g.manager) zwlr_virtual_pointer_manager_v1_destroy(g.manager);
    if(g.seat) wl_seat_destroy(g.seat);
    if(registry) wl_registry_destroy(registry);
    if(display) wl_display_disconnect(display);
    if(!ok) fputs("Private virtual pointer failed\n",stderr);
    return ok?0:1;
}
