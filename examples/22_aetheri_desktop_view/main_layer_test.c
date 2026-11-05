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
};
static unsigned char *load_font(const char *path, size_t *length) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;long n=0;unsigned char *p=NULL;
    if(!fseek(f,0,SEEK_END) && (n=ftell(f))>0 && n<64*1024*1024 && !fseek(f,0,SEEK_SET) &&
       (p=malloc((size_t)n)) && fread(p,1,(size_t)n,f)==(size_t)n)*length=(size_t)n;
    else {free(p);p=NULL;}
    if(fclose(f)){free(p);p=NULL;}return p;
}
static void paint(struct view *v) {
    struct app *a=v->app; CHECK(v->width>=320 && v->height>=84);
    if(!v->view) CHECK(aetheri_desktop_view_create(&v->model,a->font,a->font_size,&v->view));
    if(!v->presenter) CHECK(nx_wayland_shm_host_create(a->display,a->shm,v->surface,NULL,&v->presenter)==NX_OK);
    aetheri_desktop_intent intent; const nx_command *commands=NULL; size_t count=0;
    nx_wayland_shm_outcome outcome;
    CHECK(aetheri_desktop_view_frame(v->view,(float)v->width,(float)v->height,v->revision,&intent));
    CHECK(intent.kind==AETHERI_DESKTOP_NONE);
    CHECK(nx_frame_commands(aetheri_desktop_view_context(v->view),&commands,&count)==NX_OK);
    CHECK(nx_wayland_shm_host_present(v->presenter,(nx_pixel_size){v->width,v->height},1,commands,count,
                                      (nx_color){0,0,0,0},&outcome)==NX_OK);
    CHECK(outcome==NX_WAYLAND_SHM_READY);
    printf("PAINT %u %u %u\n",v->index,v->width,v->height);
}
static void configured(void *data, struct zwlr_layer_surface_v1 *layer, uint32_t serial, uint32_t width, uint32_t height) {
    struct view *v = data; v->width = width; v->height = height;
    zwlr_layer_surface_v1_ack_configure(layer, serial); paint(v);
}
static void closed(void *data, struct zwlr_layer_surface_v1 *layer) { (void)data; (void)layer; CHECK(false); }
static const struct zwlr_layer_surface_v1_listener layer_listener = {.configure = configured, .closed = closed};
static void view_state(struct view *v, bool expanded) {
    v->expanded = expanded; ae_desktop_model_search(&v->model,v->index,expanded); ++v->revision;
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
    printf("WINDOW %u %u %s | %s\n", id, active, title, app_id);
}
static void desktop_closed(void *data, struct aetheri_development_desktop_v1 *control, uint32_t id) {
    (void)data; (void)control; printf("CLOSED_WINDOW %u\n", id);
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
    (void)data; (void)keyboard; (void)format; (void)size; CHECK(close(fd) == 0);
}
static void enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
    (void)keyboard; (void)serial; (void)keys; struct app *a = data;
    for (unsigned i = 0; i < 3; ++i) if (a->views[i].surface == surface) printf("ENTER %u\n", i);
}
static void leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface) {
    (void)data; (void)keyboard; (void)serial; (void)surface; puts("LEAVE");
}
static void key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t code, uint32_t state) {
    (void)data; (void)keyboard; (void)serial; (void)time; printf("KEY %u %u\n", code, state);
}
static void modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    (void)data; (void)keyboard; (void)serial; (void)depressed; (void)latched; (void)locked; (void)group;
}
static void repeat(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay) {
    (void)data; (void)keyboard; (void)rate; (void)delay;
}
static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keymap, .enter = enter, .leave = leave, .key = key, .modifiers = modifiers, .repeat_info = repeat};
static void capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    struct app *a = data;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !a->keyboard) {
        a->keyboard = wl_seat_get_keyboard(seat); CHECK(a->keyboard);
        CHECK(wl_keyboard_add_listener(a->keyboard, &keyboard_listener, a) == 0);
    }
    if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && a->keyboard) { wl_keyboard_release(a->keyboard); a->keyboard = NULL; }
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
            struct view *v = &a.views[i]; v->app = &a; v->index = i;
            ae_desktop_model_init(&v->model);
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
    if (a.control) aetheri_development_desktop_v1_destroy(a.control);
    if (a.layers) zwlr_layer_shell_v1_destroy(a.layers);
    if (a.keyboard) wl_keyboard_release(a.keyboard);
    if (a.seat) wl_seat_release(a.seat);
    wl_shm_destroy(a.shm); wl_compositor_destroy(a.compositor); wl_registry_destroy(a.registry);
    wl_display_disconnect(a.display); puts("CLOSED"); return 0;
}
