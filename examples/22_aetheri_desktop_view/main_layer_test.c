/* Adapted from Aetheri tests/test_desktop_client.c (MIT); host owns all Wayland objects. */
#include "view.h"
#include <nexaui/wayland_host.h>
#include "aetheri-development-desktop-v1-client.h"
#include "wlr-layer-shell-unstable-v1-client.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <linux/input-event-codes.h>
#include <math.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
struct app;
struct view {
    struct app *app; unsigned index;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer;
    uint32_t width, height;
    bool expanded;
    struct ae_desktop_model model; uint64_t revision;
    aetheri_desktop_view *view; nx_wayland_shm_host *presenter;
    bool dirty, configured;
};
struct app {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;
    struct wl_output *outputs[3];
    struct aetheri_development_desktop_v1 *control;
    struct zwlr_layer_shell_v1 *layers;
    struct view views[3];
    unsigned char *font; size_t font_size;
    unsigned output_count;
    uint32_t control_name, layer_name, first_id;
    bool ordinary, retired;
    struct xkb_context *xkb;
    struct xkb_keymap *keymap;
    struct xkb_state *key_state;
    struct view *focused;
    struct view *pointer_view;
    float pointer_x, pointer_y;
    double axis_x,axis_y,steps_x,steps_y;
};
static unsigned char *load_font(const char *path, size_t *length) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;long n=0;unsigned char *p=NULL;
    if(!fseek(f,0,SEEK_END) && (n=ftell(f))>0 && n<64*1024*1024 && !fseek(f,0,SEEK_SET) &&
       (p=malloc((size_t)n)) && fread(p,1,(size_t)n,f)==(size_t)n)*length=(size_t)n;
    else {free(p);p=NULL;}
    if(fclose(f)){free(p);p=NULL;}return p;
}
static void advance(struct view *v) {
    aetheri_desktop_intent intent;
    CHECK(aetheri_desktop_view_frame(v->view,(float)v->width,(float)v->height,v->revision,&intent));
    if(intent.kind==AETHERI_DESKTOP_QUERY) {
        ae_desktop_model_search(&v->model,v->index,true);
        CHECK(ae_desktop_model_text(&v->model,intent.query)); ++v->revision;
        printf("QUERY %u %s\n",v->index,v->model.query);
    } else if(intent.kind==AETHERI_DESKTOP_SELECT) {
        v->model.selected=intent.window;v->model.selected_app=intent.app;++v->revision;
        printf("SELECT %u %d %u\n",v->index,intent.app,intent.window);
    } else if(intent.kind==AETHERI_DESKTOP_LAUNCH || intent.kind==AETHERI_DESKTOP_ACTIVATE || intent.kind==AETHERI_DESKTOP_SEARCH) {
        printf("INTENT %u %d %d %u\n",v->index,intent.kind,intent.app,intent.window);
    } else CHECK(intent.kind==AETHERI_DESKTOP_NONE);
    if(intent.kind==AETHERI_DESKTOP_QUERY || intent.kind==AETHERI_DESKTOP_SELECT) {
        CHECK(aetheri_desktop_view_frame(v->view,(float)v->width,(float)v->height,v->revision,&intent));
        CHECK(intent.kind==AETHERI_DESKTOP_NONE);
    }
    v->dirty=true;
}
static void paint(struct view *v) {
    struct app *a=v->app; CHECK(v->width>=320 && v->height>=84);
    if(!v->view) CHECK(aetheri_desktop_view_create(&v->model,a->font,a->font_size,&v->view));
    if(!v->presenter) CHECK(nx_wayland_shm_host_create(a->display,a->shm,v->surface,NULL,&v->presenter)==NX_OK);
    const nx_command *commands=NULL; size_t count=0;
    nx_wayland_shm_outcome outcome;
    advance(v);
    CHECK(nx_frame_commands(aetheri_desktop_view_context(v->view),&commands,&count)==NX_OK);
    CHECK(nx_wayland_shm_host_present(v->presenter,(nx_pixel_size){v->width,v->height},1,commands,count,
                                      (nx_color){0,0,0,0},&outcome)==NX_OK);
    if(outcome==NX_WAYLAND_SHM_BUSY){v->dirty=true;return;}
    CHECK(outcome==NX_WAYLAND_SHM_READY);
    v->dirty=false;
    printf("PAINT %u %u %u\n",v->index,v->width,v->height);
}
static void configured(void *data, struct zwlr_layer_surface_v1 *layer, uint32_t serial, uint32_t width, uint32_t height) {
    struct view *v = data; bool had_view=v->view!=NULL; v->width = width; v->height = height;v->configured=true;
    zwlr_layer_surface_v1_ack_configure(layer, serial); paint(v);
    if(!had_view && v->app->focused==v && v->expanded)
        CHECK(nx_focus_set(aetheri_desktop_view_context(v->view),20)==NX_OK);
}
static void closed(void *data, struct zwlr_layer_surface_v1 *layer) { (void)data; (void)layer; CHECK(false); }
static const struct zwlr_layer_surface_v1_listener layer_listener = {.configure = configured, .closed = closed};
static void pointer_event(struct app *a,nx_event_type type,uint32_t button);
static void view_state(struct view *v, bool expanded) {
    if(v->app->pointer_view==v)pointer_event(v->app,NX_EVENT_POINTER_CANCEL,0);
    v->expanded = expanded;v->configured=false; ae_desktop_model_search(&v->model,v->index,expanded); ++v->revision;
    uint32_t anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
    if (expanded) anchors |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
    zwlr_layer_surface_v1_set_size(v->layer, 0, expanded ? 0 : 84);
    zwlr_layer_surface_v1_set_anchor(v->layer, anchors);
    zwlr_layer_surface_v1_set_exclusive_zone(v->layer, 84);
    zwlr_layer_surface_v1_set_exclusive_edge(v->layer, ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
    zwlr_layer_surface_v1_set_keyboard_interactivity(v->layer, expanded ?
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE : ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    wl_surface_commit(v->surface);
}
static void desktop_output(void *data, struct aetheri_development_desktop_v1 *control, uint32_t index, const char *name) {
    (void)data; (void)control; printf("OUTPUT %u %s\n", index, name);
}
static void desktop_window(void *data, struct aetheri_development_desktop_v1 *control, uint32_t id,
        const char *title, const char *app_id, uint32_t active) {
    (void)control; struct app *a = data; if (!a->first_id) a->first_id = id;
    CHECK(active<=1);
    for(unsigned i=0;i<3;++i) {
        struct view *v=&a->views[i];
        CHECK(ae_desktop_model_window(&v->model,id,title,app_id,active!=0));
        CHECK(v->revision<UINT64_MAX);++v->revision;
        if(v->view)v->dirty=true;
    }
    printf("WINDOW %u %u %s | %s\n", id, active, title, app_id);
}
static void desktop_closed(void *data, struct aetheri_development_desktop_v1 *control, uint32_t id) {
    struct app *a=data;(void)control;
    for(unsigned i=0;i<3;++i) {
        struct view *v=&a->views[i];ae_desktop_model_closed(&v->model,id);
        CHECK(v->revision<UINT64_MAX);++v->revision;
        if(v->view)v->dirty=true;
    }
    printf("CLOSED_WINDOW %u\n", id);
}
static void desktop_toggle(void *data, struct aetheri_development_desktop_v1 *control, uint32_t output) {
    (void)data; (void)control; printf("TOGGLE %u\n", output);
}
static void desktop_activated(void *data, struct aetheri_development_desktop_v1 *control, uint32_t id, uint32_t found) {
    (void)data; (void)control; printf("ACTIVATED %u %u\n", id, found);
}
static void desktop_retired(void *data, struct aetheri_development_desktop_v1 *control) {
    (void)control; struct app *a = data; a->retired = true; puts("RETIRED");
}
static void desktop_done(void *data, struct aetheri_development_desktop_v1 *control) { (void)data; (void)control; puts("DONE"); }
static const struct aetheri_development_desktop_v1_listener desktop_listener = {
    .output = desktop_output, .window = desktop_window, .closed = desktop_closed, .toggle = desktop_toggle,
    .activated = desktop_activated, .retired = desktop_retired, .done = desktop_done};
static void keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int32_t fd, uint32_t size) {
    struct app *a=data;(void)keyboard;
    CHECK(format==WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && size && size<=8u*1024u*1024u);
    void *text=mmap(NULL,size,PROT_READ,MAP_PRIVATE,fd,0);CHECK(close(fd)==0 && text!=MAP_FAILED);
    CHECK(((const char *)text)[size-1]==0);
    struct xkb_keymap *map=xkb_keymap_new_from_string(a->xkb,text,XKB_KEYMAP_FORMAT_TEXT_V1,XKB_KEYMAP_COMPILE_NO_FLAGS);
    CHECK(munmap(text,size)==0 && map);struct xkb_state *state=xkb_state_new(map);CHECK(state);
    xkb_state_unref(a->key_state);xkb_keymap_unref(a->keymap);a->keymap=map;a->key_state=state;
}
static void enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
    (void)keyboard; (void)serial; (void)keys; struct app *a = data;
    for (unsigned i = 0; i < 3; ++i) if (a->views[i].surface == surface) {
        a->focused=&a->views[i];
        if(a->focused->expanded && a->focused->view) CHECK(nx_focus_set(aetheri_desktop_view_context(a->focused->view),20)==NX_OK);
        printf("ENTER %u\n", i);
    }
}
static void leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface) {
    struct app *a=data;(void)keyboard;(void)serial;
    if(a->focused && a->focused->surface==surface)a->focused=NULL;
    puts("LEAVE");
}
static nx_key logical_key(xkb_keysym_t sym) {
    if(sym>=XKB_KEY_a && sym<=XKB_KEY_z)return (nx_key)(NX_KEY_A+sym-XKB_KEY_a);
    if(sym>=XKB_KEY_A && sym<=XKB_KEY_Z)return (nx_key)(NX_KEY_A+sym-XKB_KEY_A);
    switch(sym) {
    case XKB_KEY_Tab: case XKB_KEY_ISO_Left_Tab:return NX_KEY_TAB;
    case XKB_KEY_Return: case XKB_KEY_KP_Enter:return NX_KEY_ENTER;
    case XKB_KEY_BackSpace:return NX_KEY_BACKSPACE;
    case XKB_KEY_Left:return NX_KEY_LEFT;case XKB_KEY_Right:return NX_KEY_RIGHT;
    case XKB_KEY_Up:return NX_KEY_UP;case XKB_KEY_Down:return NX_KEY_DOWN;
    case XKB_KEY_Home:return NX_KEY_HOME;case XKB_KEY_End:return NX_KEY_END;
    case XKB_KEY_Delete:return NX_KEY_DELETE;case XKB_KEY_Escape:return NX_KEY_ESCAPE;
    default:return NX_KEY_UNKNOWN;
    }
}
static void key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t code, uint32_t state) {
    struct app *a=data;(void)keyboard;(void)serial;(void)time;
    struct view *v=a->focused;
    if(!v || !v->configured || !v->expanded || !v->view || !a->key_state || code>UINT32_MAX-8u)return;
    uint32_t mods=0;
    if(xkb_state_mod_name_is_active(a->key_state,XKB_MOD_NAME_SHIFT,XKB_STATE_MODS_EFFECTIVE)>0)mods|=NX_MOD_SHIFT;
    if(xkb_state_mod_name_is_active(a->key_state,XKB_MOD_NAME_CTRL,XKB_STATE_MODS_EFFECTIVE)>0)mods|=NX_MOD_CONTROL;
    if(xkb_state_mod_name_is_active(a->key_state,XKB_MOD_NAME_ALT,XKB_STATE_MODS_EFFECTIVE)>0)mods|=NX_MOD_ALT;
    if(xkb_state_mod_name_is_active(a->key_state,XKB_MOD_NAME_LOGO,XKB_STATE_MODS_EFFECTIVE)>0)mods|=NX_MOD_SUPER;
    xkb_keycode_t xcode=code+8u;nx_key logical=logical_key(xkb_state_key_get_one_sym(a->key_state,xcode));
    if(logical!=NX_KEY_UNKNOWN) {
        nx_event event={0};event.type=state==WL_KEYBOARD_KEY_STATE_PRESSED?NX_EVENT_KEY_DOWN:NX_EVENT_KEY_UP;
        event.key=logical;event.modifiers=mods;CHECK(aetheri_desktop_view_queue_event(v->view,&event)==NX_OK);advance(v);
    }
    if(state==WL_KEYBOARD_KEY_STATE_PRESSED && !(mods&(NX_MOD_CONTROL|NX_MOD_ALT|NX_MOD_SUPER))) {
        uint32_t scalar=xkb_state_key_get_utf32(a->key_state,xcode);
        if(scalar>=32 && scalar!=127 && scalar<=0x10ffff && (scalar<0xd800 || scalar>0xdfff)) {
            nx_event event={0};event.type=NX_EVENT_TEXT;event.scalar=scalar;
            CHECK(aetheri_desktop_view_queue_event(v->view,&event)==NX_OK);advance(v);
        }
    }
}
static void modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    struct app *a=data;(void)keyboard;(void)serial;
    if(a->key_state)xkb_state_update_mask(a->key_state,depressed,latched,locked,0,0,group);
}
static void repeat(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay) {
    (void)data; (void)keyboard; (void)rate; (void)delay;
}
static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keymap, .enter = enter, .leave = leave, .key = key, .modifiers = modifiers, .repeat_info = repeat};
static void pointer_event(struct app *a,nx_event_type type,uint32_t button) {
    struct view *v=a->pointer_view;if(!v || !v->configured || !v->view)return;
    nx_event event={0};event.type=type;event.x=a->pointer_x;event.y=a->pointer_y;event.button=button;
    CHECK(aetheri_desktop_view_queue_event(v->view,&event)==NX_OK);advance(v);
}
static void pointer_enter(void *data,struct wl_pointer *pointer,uint32_t serial,
                          struct wl_surface *surface,wl_fixed_t x,wl_fixed_t y) {
    struct app *a=data;(void)pointer;(void)serial;a->pointer_view=NULL;
    for(unsigned i=0;i<3;++i)if(a->views[i].surface==surface)a->pointer_view=&a->views[i];
    a->pointer_x=(float)wl_fixed_to_double(x);a->pointer_y=(float)wl_fixed_to_double(y);
    pointer_event(a,NX_EVENT_POINTER_MOVE,0);
}
static void pointer_leave(void *data,struct wl_pointer *pointer,uint32_t serial,struct wl_surface *surface) {
    struct app *a=data;(void)pointer;(void)serial;
    if(a->pointer_view && a->pointer_view->surface==surface) {
        pointer_event(a,NX_EVENT_POINTER_CANCEL,0);a->pointer_view=NULL;
        a->axis_x=a->axis_y=a->steps_x=a->steps_y=0;
    }
}
static void pointer_motion(void *data,struct wl_pointer *pointer,uint32_t time,wl_fixed_t x,wl_fixed_t y) {
    struct app *a=data;(void)pointer;(void)time;
    a->pointer_x=(float)wl_fixed_to_double(x);a->pointer_y=(float)wl_fixed_to_double(y);
    pointer_event(a,NX_EVENT_POINTER_MOVE,0);
}
static void pointer_button(void *data,struct wl_pointer *pointer,uint32_t serial,uint32_t time,
                           uint32_t button,uint32_t state) {
    struct app *a=data;(void)pointer;(void)serial;(void)time;
    if(button==BTN_LEFT) {
        printf("POINTER BUTTON %u\n",state);
        pointer_event(a,state==WL_POINTER_BUTTON_STATE_PRESSED?
                         NX_EVENT_POINTER_DOWN:NX_EVENT_POINTER_UP,NX_BUTTON_LEFT);
    }
}
static void pointer_axis(void *data,struct wl_pointer *pointer,uint32_t time,uint32_t axis,wl_fixed_t value) {
    struct app *a=data;(void)pointer;(void)time;
    if(axis==WL_POINTER_AXIS_VERTICAL_SCROLL)a->axis_y-=wl_fixed_to_double(value);
    else if(axis==WL_POINTER_AXIS_HORIZONTAL_SCROLL)a->axis_x-=wl_fixed_to_double(value);
}
static void pointer_frame(void *data,struct wl_pointer *pointer) {
    struct app *a=data;(void)pointer;
    double dx=a->steps_x?a->steps_x:a->axis_x/10.0;
    double dy=a->steps_y?a->steps_y:a->axis_y/10.0;
    struct view *v=a->pointer_view;
    if(v && v->configured && v->view && isfinite(dx) && isfinite(dy) &&
       fabs(dx)<=1000000 && fabs(dy)<=1000000 && (dx || dy)) {
        nx_event event={0};event.type=NX_EVENT_WHEEL;
        event.x=a->pointer_x;event.y=a->pointer_y;event.delta_x=(float)dx;event.delta_y=(float)dy;
        CHECK(aetheri_desktop_view_queue_event(v->view,&event)==NX_OK);advance(v);
    }
    a->axis_x=a->axis_y=a->steps_x=a->steps_y=0;
}
static void pointer_axis_source(void *data,struct wl_pointer *pointer,uint32_t source) {
    (void)data;(void)pointer;(void)source;
}
static void pointer_axis_stop(void *data,struct wl_pointer *pointer,uint32_t time,uint32_t axis) {
    (void)data;(void)pointer;(void)time;(void)axis;
}
static void pointer_axis_discrete(void *data,struct wl_pointer *pointer,uint32_t axis,int32_t discrete) {
    struct app *a=data;(void)pointer;
    if(axis==WL_POINTER_AXIS_VERTICAL_SCROLL)a->steps_y-=(double)discrete;
    else if(axis==WL_POINTER_AXIS_HORIZONTAL_SCROLL)a->steps_x-=(double)discrete;
}
static const struct wl_pointer_listener pointer_listener={
    .enter=pointer_enter,.leave=pointer_leave,.motion=pointer_motion,.button=pointer_button,.axis=pointer_axis,
    .frame=pointer_frame,.axis_source=pointer_axis_source,.axis_stop=pointer_axis_stop,.axis_discrete=pointer_axis_discrete};
static void capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    struct app *a = data;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !a->keyboard) {
        a->keyboard = wl_seat_get_keyboard(seat); CHECK(a->keyboard);
        CHECK(wl_keyboard_add_listener(a->keyboard, &keyboard_listener, a) == 0);
    }
    if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && a->keyboard) {
        wl_keyboard_release(a->keyboard);a->keyboard=NULL;a->focused=NULL;puts("KEYBOARD REMOVED");
    }
    if((caps & WL_SEAT_CAPABILITY_POINTER) && !a->pointer) {
        a->pointer=wl_seat_get_pointer(seat);CHECK(a->pointer);
        CHECK(wl_pointer_add_listener(a->pointer,&pointer_listener,a)==0);
    }
    if(!(caps & WL_SEAT_CAPABILITY_POINTER) && a->pointer) {
        pointer_event(a,NX_EVENT_POINTER_CANCEL,0);a->pointer_view=NULL;
        wl_pointer_release(a->pointer);a->pointer=NULL;
        a->axis_x=a->axis_y=a->steps_x=a->steps_y=0;puts("POINTER REMOVED");
    }
}
static void seat_name(void *data, struct wl_seat *seat, const char *name) { (void)data; (void)seat; (void)name; }
static const struct wl_seat_listener seat_listener = {.capabilities = capabilities, .name = seat_name};
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    (void)version; struct app *a = data;
    if (!strcmp(interface, "aetheri_development_desktop_v1")) {
        a->control_name = name;
        if (!a->ordinary) {
            a->control = wl_registry_bind(registry, name, &aetheri_development_desktop_v1_interface, 1); CHECK(a->control);
            CHECK(aetheri_development_desktop_v1_add_listener(a->control, &desktop_listener, a) == 0);
        }
    } else if (!strcmp(interface, "zwlr_layer_shell_v1")) {
        a->layer_name = name;
        if (!a->ordinary) { CHECK(version >= 5); a->layers = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 5); CHECK(a->layers); }
    } else if (!strcmp(interface, "wl_compositor")) a->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    else if (!strcmp(interface, "wl_shm")) a->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    else if (!strcmp(interface, "wl_output")) {
        CHECK(a->output_count < 3); a->outputs[a->output_count++] = wl_registry_bind(registry, name, &wl_output_interface, 1);
    } else if (!strcmp(interface, "wl_seat")) {
        a->seat = wl_registry_bind(registry, name, &wl_seat_interface, 5); CHECK(a->seat);
        CHECK(wl_seat_add_listener(a->seat, &seat_listener, a) == 0);
    }
}
static void global_remove(void *data, struct wl_registry *registry, uint32_t name) { (void)data; (void)registry; (void)name; CHECK(false); }
static const struct wl_registry_listener registry_listener = {.global = global, .global_remove = global_remove};
int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    CHECK(argc == 1 || argc == 2 || argc == 4);
    struct app a = {.ordinary = argc > 1 && (!strcmp(argv[1], "--ordinary") || !strcmp(argv[1], "--forge"))};
    for(unsigned i=0;i<3;++i) {
        a.views[i].app=&a;a.views[i].index=i;ae_desktop_model_init(&a.views[i].model);
    }
    a.xkb=xkb_context_new(XKB_CONTEXT_NO_FLAGS);CHECK(a.xkb);
    const char *font=getenv("NEXAUI_DESKTOP_FONT"); CHECK(font && (a.font=load_font(font,&a.font_size)));
    a.display = wl_display_connect(NULL); CHECK(a.display);
    a.registry = wl_display_get_registry(a.display); CHECK(a.registry);
    CHECK(wl_registry_add_listener(a.registry, &registry_listener, &a) == 0);
    CHECK(wl_display_roundtrip(a.display) >= 0 && a.compositor && a.shm && a.output_count == 3);
    if (a.ordinary) {
        CHECK(!a.control_name && !a.layer_name);
        if (argc == 4) {
            const struct wl_interface *type = !strcmp(argv[3], "layer") ? &zwlr_layer_shell_v1_interface : &aetheri_development_desktop_v1_interface;
            struct wl_proxy *forged = wl_registry_bind(a.registry, (uint32_t)strtoul(argv[2], NULL, 10), type, 1); CHECK(forged);
            CHECK(wl_display_roundtrip(a.display) < 0 && wl_display_get_error(a.display) == EPROTO);
            wl_proxy_destroy(forged); puts("FORGED_BIND_DENIED");
        } else puts("PRIVATE_GLOBALS_HIDDEN");
    } else {
        CHECK(a.control && a.layers);
        printf("GLOBALS %u %u\n", a.control_name, a.layer_name);
        CHECK(wl_display_roundtrip(a.display) >= 0);
        if (argc == 2 && !strcmp(argv[1], "--duplicate")) {
            struct wl_proxy *duplicate = wl_registry_bind(a.registry, a.control_name, &aetheri_development_desktop_v1_interface, 1);
            CHECK(duplicate); CHECK(wl_display_roundtrip(a.display) >= 0);
        }
        for (unsigned i = 0; i < 3; ++i) {
            struct view *v = &a.views[i];
            v->surface = wl_compositor_create_surface(a.compositor); CHECK(v->surface);
            v->layer = zwlr_layer_shell_v1_get_layer_surface(a.layers, v->surface, a.outputs[i],
                ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "aetheri.desktop"); CHECK(v->layer);
            CHECK(zwlr_layer_surface_v1_add_listener(v->layer, &layer_listener, v) == 0);
            view_state(v, false);
            if (argc == 2 && !strcmp(argv[1], "--invalid")) {
                zwlr_layer_surface_v1_set_exclusive_zone(v->layer, 100); wl_surface_commit(v->surface); break;
            }
        }
        bool stop = false;
        while (!stop) {
            CHECK(wl_display_dispatch_pending(a.display) >= 0);
            for(unsigned i=0;i<3;++i)if(a.views[i].dirty && a.views[i].configured)paint(&a.views[i]);
            CHECK(wl_display_flush(a.display) >= 0 || errno == EAGAIN);
            struct pollfd fds[2] = {{wl_display_get_fd(a.display), POLLIN, 0}, {STDIN_FILENO, POLLIN, 0}};
            int ready; do { ready = poll(fds, 2, 10000); } while (ready < 0 && errno == EINTR);
            CHECK(ready > 0);
            if (fds[0].revents) CHECK(wl_display_dispatch(a.display) >= 0);
            if (fds[1].revents) {
                char c; CHECK(read(STDIN_FILENO, &c, 1) == 1);
                if (c == 'e') view_state(&a.views[0], true);
                else if (c == 'n') view_state(&a.views[0], false);
                else if (c == 'a') aetheri_development_desktop_v1_activate(a.control, a.first_id);
                else if (c == 's') aetheri_development_desktop_v1_activate(a.control, UINT32_MAX);
                else if (c == 'p') paint(&a.views[0]);
                else if (c == 'z') {
                    struct view *v=&a.views[0];ae_desktop_model_search(&v->model,0,true);
                    CHECK(v->revision<UINT64_MAX);++v->revision;paint(v);
                }
                else if (c == 'w') {
                    for(unsigned i=0;i<3;++i)
                        if(a.views[i].dirty && a.views[i].configured)paint(&a.views[i]);
                    for(unsigned i=0;i<3;++i)for(size_t j=0;j<a.views[i].model.count;++j) {
                        const struct ae_desktop_window *w=&a.views[i].model.windows[j];
                        printf("MODEL %u %u %s\n",i,w->id,w->title);
                    }
                    struct view *v=&a.views[0];nx_context *context=aetheri_desktop_view_context(v->view);
                    for(size_t j=0;j<v->model.count;++j) {
                        uint32_t index;nx_semantic_view row;
                        uint64_t id=UINT64_C(0x100000000)+v->model.windows[j].id;
                        if(nx_semantics_find(context,id,&index)==NX_OK && nx_semantics_get(context,index,&row)==NX_OK)
                            printf("WINDOW_ROW %u %.1f %.*s\n",v->model.windows[j].id,row.bounds.y,(int)row.name.length,row.name.data);
                    }
                }
                else if (c == 'f') {
                    struct view *v=&a.views[0];ae_desktop_model_search(&v->model,0,true);
                    for(uint32_t id=1;id<=30;++id) {
                        char title[32];CHECK(snprintf(title,sizeof(title),"Window %02u",id)>0);
                        CHECK(ae_desktop_model_window(&v->model,id,title,"org.example.Window",false));
                    }
                    ++v->revision;paint(v);
                }
                else if (c == 'i') {
                    uint32_t index;nx_semantic_view row;
                    nx_context *context=aetheri_desktop_view_context(a.views[0].view);
                    CHECK(nx_semantics_find(context,UINT64_C(0x100000000)+20,&index)==NX_OK);
                    CHECK(nx_semantics_get(context,index,&row)==NX_OK);printf("ROW 20 %.1f\n",row.bounds.y);
                }
                else if (c == 'u') {
                    wl_surface_attach(a.views[0].surface, NULL, 0, 0); wl_surface_commit(a.views[0].surface);
                }
                else if (c == 'r') view_state(&a.views[0], false);
                else if (c == 'q') {
                    aetheri_development_desktop_v1_retire(a.control);
                    CHECK(wl_display_roundtrip(a.display) >= 0 && a.retired); stop = true;
                } else CHECK(false);
                CHECK(wl_display_roundtrip(a.display) >= 0); printf("SYNC %c\n", c);
            }
        }
    }
    for (unsigned i = 0; i < 3; ++i) {
        if (a.views[i].presenter) CHECK(nx_wayland_shm_host_destroy(&a.views[i].presenter)==NX_OK);
        aetheri_desktop_view_destroy(a.views[i].view);
        if (a.views[i].layer) zwlr_layer_surface_v1_destroy(a.views[i].layer);
        if (a.views[i].surface) wl_surface_destroy(a.views[i].surface);
        if (a.outputs[i]) wl_output_destroy(a.outputs[i]);
    }
    free(a.font);
    xkb_state_unref(a.key_state);xkb_keymap_unref(a.keymap);xkb_context_unref(a.xkb);
    if (a.control) aetheri_development_desktop_v1_destroy(a.control);
    if (a.layers) zwlr_layer_shell_v1_destroy(a.layers);
    if (a.keyboard) wl_keyboard_release(a.keyboard);
    if (a.pointer) wl_pointer_release(a.pointer);
    if (a.seat) wl_seat_release(a.seat);
    wl_shm_destroy(a.shm); wl_compositor_destroy(a.compositor); wl_registry_destroy(a.registry);
    wl_display_disconnect(a.display); puts("CLOSED"); return 0;
}
