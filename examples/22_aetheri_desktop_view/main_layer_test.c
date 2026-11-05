#define _POSIX_C_SOURCE 200809L
/* Adapted from Aetheri tests/test_desktop_client.c (MIT); host owns all Wayland objects. */
#include "view.h"
#include <nexaui/wayland_host.h>
#include <nexaui/linux_accessibility_host.h>
#include "aetheri-development-desktop-v1-client.h"
#include "wlr-layer-shell-unstable-v1-client.h"
#include "text-input-v3-client.h"
#include "ime_transaction.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <signal.h>

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
    nx_linux_accessibility_host *accessibility;
    bool dirty, configured, remove_pending;
};
struct app {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;
    struct wl_callback *pointer_release_barrier;
    struct wl_pointer *pointer_release_device;
    struct view *pointer_release_view;
    nx_event pointer_release_event;
    uint64_t pointer_release_revision;
    struct zwp_text_input_manager_v3 *ime_manager;
    struct zwp_text_input_v3 *ime_input;
    struct wl_surface *ime_surface;
    aetheri_ime_transaction ime_transaction;
    uint32_t ime_serial;
    uint64_t ime_revision;
    nx_rect ime_caret;
    char ime_surrounding[AE_DESKTOP_QUERY];
    size_t ime_length,ime_cursor,ime_anchor;
    bool ime_requested,ime_enabled,ime_defer;
    struct wl_output *outputs[3];
    uint32_t output_names[3];
    struct aetheri_development_desktop_v1 *control;
    struct zwlr_layer_shell_v1 *layers;
    struct view views[3];
    unsigned char *font; size_t font_size;
    unsigned output_count;
    uint32_t control_name, layer_name, first_id;
    bool ordinary, retired, owner_fixture, ready_reported, close_search, ax_requested;
    int toggle_output;
    struct xkb_context *xkb;
    struct xkb_keymap *keymap;
    struct xkb_state *key_state;
    struct view *focused;
    struct view *pointer_view;
    float pointer_x, pointer_y;
    double axis_x,axis_y,steps_x,steps_y;
    uint32_t activation_id;
    uint64_t activation_revision;
    unsigned activation_phase, activation_output;
    bool activation_enabled, activation_hold, activation_invalidated, hold_after_collapse;
    int launcher;
};
static volatile sig_atomic_t stop_requested,signal_writer=-1;
static void ime_stop(struct app *a);
static void ime_sync(struct app *a);
static void pointer_cancel_pending(struct app *a);
static void stop_signal(int sig) {
    (void)sig;int saved=errno;stop_requested=1;
    if(signal_writer>=0){char byte=1;(void)write((int)signal_writer,&byte,1);}
    errno=saved;
}
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
        if(intent.kind==AETHERI_DESKTOP_LAUNCH && v->app->launcher>=0) {
            struct app *a=v->app;enum ae_desktop_app target=intent.app;
            CHECK(target==AE_DESKTOP_SETTINGS || target==AE_DESKTOP_TERMINAL);
            for(unsigned i=0;i<3;++i)CHECK(!a->views[i].model.pending[target] && a->views[i].revision<UINT64_MAX);
            for(unsigned i=0;i<3;++i) {
                CHECK(ae_desktop_model_request(&a->views[i].model,target));++a->views[i].revision;
                if(a->views[i].view)a->views[i].dirty=true;
            }
            char packet[3]={'L','2',target==AE_DESKTOP_SETTINGS?'S':'T'};
            ssize_t sent;do {sent=send(a->launcher,packet,sizeof(packet),MSG_NOSIGNAL|MSG_DONTWAIT);} while(sent<0 && errno==EINTR);
            CHECK(sent==(ssize_t)sizeof(packet));printf("LAUNCH_REQUEST %c\n",packet[2]);
            aetheri_desktop_intent reconciled;
            CHECK(aetheri_desktop_view_frame(v->view,(float)v->width,(float)v->height,v->revision,&reconciled));
            CHECK(reconciled.kind==AETHERI_DESKTOP_NONE);
        }
        if(intent.kind==AETHERI_DESKTOP_ACTIVATE && v->app->activation_enabled) {
            CHECK(!v->app->activation_phase && intent.window);
            v->app->activation_id=intent.window;v->app->activation_revision=v->revision;
            v->app->activation_output=v->index;
            v->app->activation_phase=1;v->app->activation_invalidated=false;
        }
    } else CHECK(intent.kind==AETHERI_DESKTOP_NONE);
    if(intent.kind==AETHERI_DESKTOP_QUERY || intent.kind==AETHERI_DESKTOP_SELECT) {
        CHECK(aetheri_desktop_view_frame(v->view,(float)v->width,(float)v->height,v->revision,&intent));
        CHECK(intent.kind==AETHERI_DESKTOP_NONE);
    }
    v->dirty=true;
}
static void ime_clear_visible(struct app *a) {
    struct view *v=a->focused;uint64_t revision=0;
    if(a->ime_enabled && v && v->view && v->surface==a->ime_surface &&
       aetheri_desktop_view_host_ime_revision(v->view,&revision)==NX_OK &&
       aetheri_desktop_view_host_ime_preedit(v->view,revision,NULL,0,0,0)==NX_OK)
        v->dirty=true;
}
static void ime_stop(struct app *a) {
    ime_clear_visible(a);
    if(a->ime_input && a->ime_enabled) {
        if(a->ime_serial==UINT32_MAX) {
            zwp_text_input_v3_destroy(a->ime_input);a->ime_input=NULL;
        } else {
            zwp_text_input_v3_disable(a->ime_input);
            zwp_text_input_v3_commit(a->ime_input);++a->ime_serial;
        }
    }
    aetheri_ime_retire(&a->ime_transaction);
    a->ime_enabled=false;a->ime_revision=0;a->ime_defer=false;
}
static void ime_sync(struct app *a) {
    if(!a->ime_input)return;
    struct view *v=a->focused;uint64_t revision=0;nx_rect caret={0};
    if(!v || !v->view || !v->configured || !v->expanded || a->ime_surface!=v->surface ||
       aetheri_desktop_view_host_ime_revision(v->view,&revision)!=NX_OK) {
        if(a->ime_enabled)ime_stop(a);return;
    }
    if(aetheri_desktop_view_host_ime_caret(v->view,&caret)!=NX_OK) {
        if(!a->ime_enabled)return;
        caret=a->ime_caret; /* Hidden composition cursor: retain last visible placement. */
    }
    char surrounding[AE_DESKTOP_QUERY];size_t length=0,cursor=0,anchor=0;
    CHECK(aetheri_desktop_view_host_ime_surrounding(v->view,revision,surrounding,sizeof(surrounding),
          &length,&cursor,&anchor)==NX_OK);
    if(a->ime_defer)return;
    if(a->ime_enabled && revision==a->ime_revision && !memcmp(&caret,&a->ime_caret,sizeof(caret)))return;
    double x=floor((double)caret.x),y=floor((double)caret.y);
    double width=ceil((double)caret.width),height=ceil((double)caret.height);
    if(!isfinite(x) || !isfinite(y) || !isfinite(width) || !isfinite(height) ||
       x<INT32_MIN || x>INT32_MAX || y<INT32_MIN || y>INT32_MAX ||
       width<1 || width>INT32_MAX || height<1 || height>INT32_MAX) {
        ime_stop(a);return;
    }
    if(a->ime_serial==UINT32_MAX) {ime_stop(a);return;}
    if(!a->ime_enabled) {
        zwp_text_input_v3_enable(a->ime_input);
        zwp_text_input_v3_set_content_type(a->ime_input,ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE,
                                          ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL);
    }
    zwp_text_input_v3_set_surrounding_text(a->ime_input,surrounding,(int32_t)cursor,(int32_t)anchor);
    zwp_text_input_v3_set_cursor_rectangle(a->ime_input,(int32_t)x,(int32_t)y,(int32_t)width,(int32_t)height);
    zwp_text_input_v3_commit(a->ime_input);++a->ime_serial;
    CHECK(aetheri_ime_publish(&a->ime_transaction,a->ime_serial,revision)==NX_OK);
    a->ime_enabled=true;a->ime_revision=revision;a->ime_caret=caret;
    memcpy(a->ime_surrounding,surrounding,length+1);a->ime_length=length;
    a->ime_cursor=cursor;a->ime_anchor=anchor;
}
static void ime_enter(void *data,struct zwp_text_input_v3 *input,struct wl_surface *surface) {
    struct app *a=data;(void)input;ime_stop(a);a->ime_surface=surface;
}
static void ime_leave(void *data,struct zwp_text_input_v3 *input,struct wl_surface *surface) {
    struct app *a=data;(void)input;
    if(a->ime_surface==surface) {
        ime_clear_visible(a);
        aetheri_ime_retire(&a->ime_transaction);
        a->ime_enabled=false;a->ime_revision=0;a->ime_defer=false;a->ime_surface=NULL;
    }
}
static void ime_preedit(void *data,struct zwp_text_input_v3 *input,const char *text,int32_t begin,int32_t end) {
    struct app *a=data;(void)input;size_t n=text?strnlen(text,AETHERI_IME_TEXT_MAX+1u):0;
    if(!text)begin=end=0;
    (void)aetheri_ime_preedit(&a->ime_transaction,text,n,begin,end);
}
static void ime_commit(void *data,struct zwp_text_input_v3 *input,const char *text) {
    struct app *a=data;(void)input;size_t n=text?strnlen(text,AETHERI_IME_TEXT_MAX+1u):0;
    (void)aetheri_ime_commit(&a->ime_transaction,text,n);
}
static void ime_delete(void *data,struct zwp_text_input_v3 *input,uint32_t before,uint32_t after) {
    struct app *a=data;(void)input;aetheri_ime_delete(&a->ime_transaction,before,after);
}
static void ime_done(void *data,struct zwp_text_input_v3 *input,uint32_t serial) {
    struct app *a=data;(void)input;struct view *v=a->focused;uint64_t current=0;
    if(a->ime_enabled && serial==a->ime_serial)a->ime_defer=false;
    if(!a->ime_enabled || !v || !v->view || !v->expanded || v->surface!=a->ime_surface ||
       aetheri_desktop_view_host_ime_revision(v->view,&current)!=NX_OK)current=0;
    aetheri_ime_result result;
    if(aetheri_ime_done(&a->ime_transaction,serial,current,&result)!=NX_OK)return;
    a->ime_defer=!result.can_commit_state;
    size_t lo=a->ime_cursor<a->ime_anchor?a->ime_cursor:a->ime_anchor;
    size_t hi=a->ime_cursor>a->ime_anchor?a->ime_cursor:a->ime_anchor;
    if(result.before>lo || result.after>a->ime_length-hi)return;
    size_t start=lo-result.before,end=hi+result.after;
    if((start<a->ime_length && ((unsigned char)a->ime_surrounding[start]&0xc0u)==0x80u) ||
       (end<a->ime_length && ((unsigned char)a->ime_surrounding[end]&0xc0u)==0x80u))return;
    size_t incoming=0;
    for(size_t i=0;i<result.scalar_count;++i) {
        uint32_t scalar=result.scalars[i];
        incoming+=scalar<0x80u?1u:scalar<0x800u?2u:scalar<0x10000u?3u:4u;
    }
    size_t removed=(result.before || result.after || result.scalar_count)?end-start:0;
    if(a->ime_length-removed+incoming>=AE_DESKTOP_QUERY)return;
    CHECK(aetheri_desktop_view_host_ime_preedit(v->view,current,NULL,0,0,0)==NX_OK);
    nx_event event={0};event.ime_target=20;event.ime_revision=current;
    if(result.before || result.after) {
        event.type=NX_EVENT_IME_DELETE_SURROUNDING;event.ime_before=result.before;event.ime_after=result.after;
        CHECK(aetheri_desktop_view_queue_event(v->view,&event)==NX_OK);
    }
    for(size_t i=0;i<result.scalar_count;++i) {
        event.type=NX_EVENT_TEXT;event.scalar=result.scalars[i];
        CHECK(aetheri_desktop_view_queue_event(v->view,&event)==NX_OK);
    }
    advance(v);
    if(aetheri_desktop_view_host_ime_revision(v->view,&current)!=NX_OK)advance(v);
    if(aetheri_desktop_view_host_ime_revision(v->view,&current)==NX_OK) {
        CHECK(aetheri_desktop_view_host_ime_preedit(v->view,current,result.preedit,result.preedit_length,
                                                     result.begin,result.end)==NX_OK);
        advance(v);
    }
}
static const struct zwp_text_input_v3_listener ime_listener={
    .enter=ime_enter,.leave=ime_leave,.preedit_string=ime_preedit,
    .commit_string=ime_commit,.delete_surrounding_text=ime_delete,.done=ime_done};
static void launcher_reply(struct app *a) {
    char bytes[8];union {struct cmsghdr align;char bytes[CMSG_SPACE(sizeof(int)*4)];} ancillary;
    struct iovec io={.iov_base=bytes,.iov_len=sizeof(bytes)};
    struct msghdr message={.msg_iov=&io,.msg_iovlen=1,.msg_control=ancillary.bytes,.msg_controllen=sizeof(ancillary.bytes)};
    ssize_t n;do {n=recvmsg(a->launcher,&message,MSG_CMSG_CLOEXEC|MSG_DONTWAIT);} while(n<0 && errno==EINTR);
    bool extra=false;
    if(n>=0)for(struct cmsghdr *h=CMSG_FIRSTHDR(&message);h;h=CMSG_NXTHDR(&message,h)) {
        extra=true;
        if(h->cmsg_level==SOL_SOCKET && h->cmsg_type==SCM_RIGHTS && h->cmsg_len>=CMSG_LEN(0)) {
            size_t count=(h->cmsg_len-CMSG_LEN(0))/sizeof(int);
            for(size_t i=0;i<count;++i) {int fd;memcpy(&fd,(unsigned char *)CMSG_DATA(h)+i*sizeof(fd),sizeof(fd));close(fd);}
        }
    }
    CHECK(n==3 && !extra && !(message.msg_flags&(MSG_TRUNC|MSG_CTRUNC)) && bytes[1]=='2' &&
          (bytes[2]=='S' || bytes[2]=='T') && (bytes[0]=='A' || bytes[0]=='E' || bytes[0]=='X'));
    enum ae_desktop_app target=bytes[2]=='S'?AE_DESKTOP_SETTINGS:AE_DESKTOP_TERMINAL;
    for(unsigned i=0;i<3;++i) {
        struct view *v=&a->views[i];CHECK(v->revision<UINT64_MAX);
        CHECK(ae_desktop_model_reply(&v->model,target,bytes[0]));++v->revision;
        if(v->view)v->dirty=true;
    }
    printf("LAUNCH_REPLY %c %c\n",bytes[0],bytes[2]);
    if(a->owner_fixture && bytes[0]=='A') {
        struct view *v=NULL;
        for(unsigned i=0;i<3;++i)if(a->views[i].expanded){v=&a->views[i];break;}
        if(v && !a->activation_phase) {
            const char *app_id=ae_desktop_app_id(target);
            for(size_t i=0;i<v->model.count;++i) {
                const struct ae_desktop_window *window=&v->model.windows[i];
                if(strcmp(window->app_id,app_id))continue;
                a->activation_id=window->id;a->activation_revision=v->revision;
                a->activation_output=v->index;a->activation_phase=1;
                a->activation_invalidated=false;
                printf("REUSE_WINDOW %c %u\n",bytes[2],window->id);
                break;
            }
        }
        if(!a->activation_phase)a->close_search=true;
    }
}
static void accessibility_publish(struct view *v) {
    struct app *a=v->app;
    if(v->accessibility) {
        nx_linux_accessibility_geometry geometry={0,0,v->width,v->height,0,1,a->focused==v && a->keyboard!=NULL};
        nx_result result=nx_linux_accessibility_host_publish(v->accessibility,
            aetheri_desktop_view_context(v->view),&geometry);
        CHECK(result==NX_OK || result==NX_ERROR_UNSUPPORTED);
    }
}
static uint32_t accessibility_prepare(struct app *a,struct pollfd *fds,uint32_t wait_ms) {
    for(unsigned i=0;i<3;++i) {
        struct view *v=&a->views[i];fds[i].fd=-1;fds[i].events=fds[i].revents=0;
        if(!v->accessibility || !v->configured)continue;
        int fd=-1,writable=0;uint32_t timeout=wait_ms;
        nx_result result=nx_linux_accessibility_host_prepare(v->accessibility,&fd,&writable,&timeout);
        CHECK(result==NX_OK || result==NX_ERROR_UNSUPPORTED);
        if(result==NX_OK) {
            fds[i].fd=fd;fds[i].events=(short)(POLLIN|(writable?POLLOUT:0));
            if(timeout<wait_ms)wait_ms=timeout;
            size_t routed=0;int focus_requested=0;
            CHECK(nx_linux_accessibility_host_route(v->accessibility,aetheri_desktop_view_context(v->view),
                                                     &routed,&focus_requested)==NX_OK);
            if(routed) {advance(v);wait_ms=0;}
        }
    }
    return wait_ms;
}
static void paint(struct view *v) {
    struct app *a=v->app; CHECK(v->width>=320 && v->height>=84);
    if(!v->view) {
        CHECK(aetheri_desktop_view_create(&v->model,a->font,a->font_size,&v->view));
        if(a->ime_requested)CHECK(aetheri_desktop_view_set_host_ime(v->view,1)==NX_OK);
        if(a->ax_requested)CHECK(nx_linux_accessibility_host_create(&v->accessibility)==NX_OK);
        if(a->focused==v) CHECK(aetheri_desktop_view_surface_focus(v->view,1)==NX_OK);
    }
    if(!v->presenter) CHECK(nx_wayland_shm_host_create(a->display,a->shm,v->surface,NULL,&v->presenter)==NX_OK);
    const nx_command *commands=NULL; size_t count=0;
    nx_wayland_shm_outcome outcome;
    advance(v);
    if(a->ime_requested && a->focused==v && v->expanded &&
       nx_focus_id(aetheri_desktop_view_context(v->view))==20) {
        uint64_t revision=0;
        if(aetheri_desktop_view_host_ime_revision(v->view,&revision)!=NX_OK)advance(v);
    }
    CHECK(nx_frame_commands(aetheri_desktop_view_context(v->view),&commands,&count)==NX_OK);
    CHECK(nx_wayland_shm_host_present(v->presenter,(nx_pixel_size){v->width,v->height},1,commands,count,
                                      (nx_color){0,0,0,0},&outcome)==NX_OK);
    if(outcome==NX_WAYLAND_SHM_BUSY){v->dirty=true;return;}
    CHECK(outcome==NX_WAYLAND_SHM_READY);
    accessibility_publish(v);
    v->dirty=false;
    printf("PAINT %u %u %u\n",v->index,v->width,v->height);
}
static void configured(void *data, struct zwlr_layer_surface_v1 *layer, uint32_t serial, uint32_t width, uint32_t height) {
    struct view *v = data; bool had_view=v->view!=NULL; v->width = width; v->height = height;v->configured=true;
    zwlr_layer_surface_v1_ack_configure(layer, serial); paint(v);
    if(!had_view && v->app->focused==v && v->expanded)
        CHECK(nx_focus_set(aetheri_desktop_view_context(v->view),20)==NX_OK);
    if(v->app->owner_fixture && !v->app->ready_reported &&
       v->app->views[0].configured && v->app->views[1].configured && v->app->views[2].configured) {
        v->app->ready_reported=true;puts("READY nexaui-layer");puts("LAYOUT 1");
    }
}
static void closed(void *data, struct zwlr_layer_surface_v1 *layer) {
    struct view *v=data;(void)layer;v->remove_pending=true;
}
static const struct zwlr_layer_surface_v1_listener layer_listener = {.configure = configured, .closed = closed};
static void pointer_event(struct app *a,nx_event_type type,uint32_t button);
static void view_state(struct view *v, bool expanded) {
    if(v->app->pointer_release_view==v)pointer_cancel_pending(v->app);
    if(!expanded && v->app->focused==v)ime_stop(v->app);
    if(v->app->pointer_view==v)pointer_event(v->app,NX_EVENT_POINTER_CANCEL,0);
    CHECK(v->revision<UINT64_MAX);
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
static bool has_window(const struct view *v,uint32_t id) {
    for(size_t i=0;i<v->model.count;++i)if(v->model.windows[i].id==id)return true;
    return false;
}
/* Test owner: defer protocol changes until outside Wayland callbacks, and
   revalidate after the compositor has configured the collapsed surface. */
static void activation_step(struct app *a) {
    if(!a->activation_phase || a->activation_hold)return;
    CHECK(a->activation_output<3);struct view *v=&a->views[a->activation_output];
    if((a->activation_phase==1 && v->revision!=a->activation_revision) ||
       a->activation_invalidated || !has_window(v,a->activation_id)) {
        printf("ACTIVATION_CANCELLED %u\n",a->activation_id);
        a->activation_phase=0;a->activation_enabled=false;
        if(a->owner_fixture)a->close_search=true;
        return;
    }
    if(a->activation_phase==1) {
        if(!v->expanded || !v->configured)return;
        view_state(v,false);
        a->activation_revision=v->revision;a->activation_phase=2;
        if(a->hold_after_collapse) {
            a->activation_hold=true;printf("ACTIVATION_HELD %u\n",a->activation_id);
        }
    } else if(v->configured && !v->expanded) {
        printf("ACTIVATION_REQUEST %u\n",a->activation_id);
        aetheri_development_desktop_v1_activate(a->control,a->activation_id);
        a->activation_phase=0;a->activation_enabled=false;
    }
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
    if(a->activation_phase && id==a->activation_id)a->activation_invalidated=true;
    for(unsigned i=0;i<3;++i) {
        struct view *v=&a->views[i];ae_desktop_model_closed(&v->model,id);
        CHECK(v->revision<UINT64_MAX);++v->revision;
        if(v->view)v->dirty=true;
    }
    printf("CLOSED_WINDOW %u\n", id);
}
static void desktop_toggle(void *data, struct aetheri_development_desktop_v1 *control, uint32_t output) {
    struct app *a=data;(void)control;printf("TOGGLE %u\n", output);
    if(a->owner_fixture){CHECK(output<3);a->toggle_output=(int)output;}
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
        if(a->focused->view) {
            CHECK(aetheri_desktop_view_surface_focus(a->focused->view,1)==NX_OK);
            if(a->focused->expanded) CHECK(nx_focus_set(aetheri_desktop_view_context(a->focused->view),20)==NX_OK);
            if(a->focused->configured)advance(a->focused);
        }
        printf("ENTER %u\n", i);
    }
}
static void leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface) {
    struct app *a=data;(void)keyboard;(void)serial;
    if(a->focused && a->focused->surface==surface) {
        struct view *v=a->focused;
        ime_stop(a);
        if(v->view) {
            CHECK(aetheri_desktop_view_surface_focus(v->view,0)==NX_OK);
            if(v->configured)advance(v);
        }
        a->focused=NULL;
    }
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
static void pointer_cancel_pending(struct app *a) {
    if(!a->pointer_release_barrier)return;
    struct view *v=a->pointer_release_view;
    wl_callback_destroy(a->pointer_release_barrier);a->pointer_release_barrier=NULL;
    a->pointer_release_view=NULL;a->pointer_release_device=NULL;
    if(v && v->view && v->configured) {
        nx_event event={0};event.type=NX_EVENT_POINTER_CANCEL;
        CHECK(aetheri_desktop_view_queue_event(v->view,&event)==NX_OK);advance(v);
    }
}
/* Called after dispatch returns, never from a Wayland listener. A removed
   output cannot keep input capture, an accessibility tree or a pending intent. */
static void retire_output(struct app *a,unsigned index) {
    CHECK(index<3);struct view *v=&a->views[index];
    if(!v->remove_pending)return;
    v->remove_pending=false;
    if(a->pointer_release_view==v)pointer_cancel_pending(a);
    if(a->pointer_view==v){
        pointer_event(a,NX_EVENT_POINTER_CANCEL,0);a->pointer_view=NULL;
        a->axis_x=a->axis_y=a->steps_x=a->steps_y=0;
    }
    if(a->focused==v) {
        ime_stop(a);
        if(v->view)CHECK(aetheri_desktop_view_surface_focus(v->view,0)==NX_OK);
        a->focused=NULL;
    }
    if(a->ime_surface==v->surface)a->ime_surface=NULL;
    if(a->activation_phase && a->activation_output==index) {
        a->activation_phase=0;a->activation_enabled=false;
    }
    if(a->toggle_output==(int)index)a->toggle_output=-1;
    if(v->accessibility)CHECK(nx_linux_accessibility_host_destroy(&v->accessibility)==NX_OK);
    if(v->presenter)CHECK(nx_wayland_shm_host_destroy(&v->presenter)==NX_OK);
    aetheri_desktop_view_destroy(v->view);v->view=NULL;
    if(v->layer){zwlr_layer_surface_v1_destroy(v->layer);v->layer=NULL;}
    if(v->surface){wl_surface_destroy(v->surface);v->surface=NULL;}
    if(a->outputs[index]){wl_output_destroy(a->outputs[index]);a->outputs[index]=NULL;}
    a->output_names[index]=0;v->configured=false;v->expanded=false;v->dirty=false;
    printf("OUTPUT_REMOVED %u\n",index);
}
static void pointer_release_ready(void *data,struct wl_callback *callback,uint32_t serial) {
    struct app *a=data;(void)serial;CHECK(callback==a->pointer_release_barrier);
    struct view *v=a->pointer_release_view;struct wl_pointer *device=a->pointer_release_device;
    nx_event event=a->pointer_release_event;uint64_t revision=a->pointer_release_revision;
    wl_callback_destroy(callback);a->pointer_release_barrier=NULL;
    a->pointer_release_view=NULL;a->pointer_release_device=NULL;
    if(v && v->view && v->configured && a->pointer==device && a->pointer_view==v && v->revision==revision) {
        CHECK(aetheri_desktop_view_queue_event(v->view,&event)==NX_OK);advance(v);
    } else if(v && v->view && v->configured) {
        event=(nx_event){0};event.type=NX_EVENT_POINTER_CANCEL;
        CHECK(aetheri_desktop_view_queue_event(v->view,&event)==NX_OK);advance(v);
    }
}
static const struct wl_callback_listener pointer_release_listener={.done=pointer_release_ready};
static void pointer_enter(void *data,struct wl_pointer *pointer,uint32_t serial,
                          struct wl_surface *surface,wl_fixed_t x,wl_fixed_t y) {
    struct app *a=data;(void)pointer;(void)serial;pointer_cancel_pending(a);a->pointer_view=NULL;
    for(unsigned i=0;i<3;++i)if(a->views[i].surface==surface)a->pointer_view=&a->views[i];
    a->pointer_x=(float)wl_fixed_to_double(x);a->pointer_y=(float)wl_fixed_to_double(y);
    pointer_event(a,NX_EVENT_POINTER_MOVE,0);
}
static void pointer_leave(void *data,struct wl_pointer *pointer,uint32_t serial,struct wl_surface *surface) {
    struct app *a=data;(void)pointer;(void)serial;
    if(a->pointer_view && a->pointer_view->surface==surface) {
        pointer_cancel_pending(a);
        pointer_event(a,NX_EVENT_POINTER_CANCEL,0);a->pointer_view=NULL;
        a->axis_x=a->axis_y=a->steps_x=a->steps_y=0;
    }
}
static void pointer_motion(void *data,struct wl_pointer *pointer,uint32_t time,wl_fixed_t x,wl_fixed_t y) {
    struct app *a=data;(void)pointer;(void)time;
    pointer_cancel_pending(a);
    a->pointer_x=(float)wl_fixed_to_double(x);a->pointer_y=(float)wl_fixed_to_double(y);
    pointer_event(a,NX_EVENT_POINTER_MOVE,0);
}
static void pointer_button(void *data,struct wl_pointer *pointer,uint32_t serial,uint32_t time,
                           uint32_t button,uint32_t state) {
    struct app *a=data;(void)pointer;(void)serial;(void)time;
    if(button==BTN_LEFT) {
        printf("POINTER BUTTON %u\n",state);
        pointer_cancel_pending(a);
        if(state==WL_POINTER_BUTTON_STATE_PRESSED)pointer_event(a,NX_EVENT_POINTER_DOWN,NX_BUTTON_LEFT);
        else if(state==WL_POINTER_BUTTON_STATE_RELEASED && a->pointer_view && a->pointer_view->view &&
                a->pointer_view->configured && a->display && a->pointer==pointer) {
            a->pointer_release_view=a->pointer_view;a->pointer_release_device=pointer;
            a->pointer_release_revision=a->pointer_view->revision;
            a->pointer_release_event=(nx_event){0};a->pointer_release_event.type=NX_EVENT_POINTER_UP;
            a->pointer_release_event.x=a->pointer_x;a->pointer_release_event.y=a->pointer_y;
            a->pointer_release_event.button=NX_BUTTON_LEFT;
            a->pointer_release_barrier=wl_display_sync(a->display);CHECK(a->pointer_release_barrier);
            CHECK(!wl_callback_add_listener(a->pointer_release_barrier,&pointer_release_listener,a));
        } else if(state==WL_POINTER_BUTTON_STATE_RELEASED)pointer_event(a,NX_EVENT_POINTER_CANCEL,0);
    }
}
static void pointer_axis(void *data,struct wl_pointer *pointer,uint32_t time,uint32_t axis,wl_fixed_t value) {
    struct app *a=data;(void)pointer;(void)time;
    pointer_cancel_pending(a);
    if(axis==WL_POINTER_AXIS_VERTICAL_SCROLL)a->axis_y-=wl_fixed_to_double(value);
    else if(axis==WL_POINTER_AXIS_HORIZONTAL_SCROLL)a->axis_x-=wl_fixed_to_double(value);
}
static void pointer_frame(void *data,struct wl_pointer *pointer) {
    struct app *a=data;(void)pointer;
    double dx=a->steps_x?a->steps_x:a->axis_x/10.0;
    double dy=a->steps_y?a->steps_y:a->axis_y/10.0;
    if(dx || dy)pointer_cancel_pending(a);
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
        ime_stop(a);
        if(a->focused && a->focused->view) {
            CHECK(aetheri_desktop_view_surface_focus(a->focused->view,0)==NX_OK);
            if(a->focused->configured)advance(a->focused);
        }
        wl_keyboard_release(a->keyboard);a->keyboard=NULL;a->focused=NULL;puts("KEYBOARD REMOVED");
    }
    if((caps & WL_SEAT_CAPABILITY_POINTER) && !a->pointer) {
        a->pointer=wl_seat_get_pointer(seat);CHECK(a->pointer);
        CHECK(wl_pointer_add_listener(a->pointer,&pointer_listener,a)==0);
    }
    if(!(caps & WL_SEAT_CAPABILITY_POINTER) && a->pointer) {
        pointer_cancel_pending(a);
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
    else if (!strcmp(interface,"zwp_text_input_manager_v3") && a->ime_requested)
        a->ime_manager=wl_registry_bind(registry,name,&zwp_text_input_manager_v3_interface,1);
    else if (!strcmp(interface, "wl_shm")) a->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    else if (!strcmp(interface, "wl_output")) {
        CHECK(a->output_count < 3);a->output_names[a->output_count]=name;
        a->outputs[a->output_count++] = wl_registry_bind(registry, name, &wl_output_interface, 1);
    } else if (!strcmp(interface, "wl_seat")) {
        a->seat = wl_registry_bind(registry, name, &wl_seat_interface, 5); CHECK(a->seat);
        CHECK(wl_seat_add_listener(a->seat, &seat_listener, a) == 0);
    }
}
static void global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    struct app *a=data;(void)registry;
    for(unsigned i=0;i<3;++i)if(a->output_names[i]==name && name)a->views[i].remove_pending=true;
}
static const struct wl_registry_listener registry_listener = {.global = global, .global_remove = global_remove};
int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    bool owner_fixture=argc>1 && !strcmp(argv[1],"--font");
    CHECK(owner_fixture || argc == 1 || argc == 2 || argc == 4);
    struct app a = {.launcher=-1,.toggle_output=-1,.owner_fixture=owner_fixture,
                    .ime_requested=getenv("NEXAUI_TEST_IME")!=NULL,
                    .ax_requested=getenv("NEXAUI_TEST_ATSPI")!=NULL,
                    .ordinary = argc > 1 && (!strcmp(argv[1], "--ordinary") || !strcmp(argv[1], "--forge"))};
    const char *font=getenv("NEXAUI_DESKTOP_FONT");
    const char *launcher=getenv("NEXAUI_TEST_LAUNCH_FD");
    if(owner_fixture) {
        font=NULL;launcher=NULL;const char *cache=NULL;bool diagnostics=false;
        for(int i=1;i<argc;++i) {
            if(!strcmp(argv[i],"--font") && !font && i+1<argc)font=argv[++i];
            else if(!strcmp(argv[i],"--cache") && !cache && i+1<argc)cache=argv[++i];
            else if(!strcmp(argv[i],"--launcher-fd") && !launcher && i+1<argc)launcher=argv[++i];
            else if(!strcmp(argv[i],"--diagnostics") && !diagnostics)diagnostics=true;
            else CHECK(false);
        }
        CHECK(font && cache && cache[0]=='/' && launcher && diagnostics);
    }
    if(launcher) {
        char *end=NULL;errno=0;long fd=strtol(launcher,&end,10);
        CHECK(*launcher && end && !*end && !errno && fd>=3 && fd<=INT32_MAX);
        int type=0;socklen_t size=sizeof(type);struct sockaddr_storage peer;socklen_t peer_size=sizeof(peer);
        CHECK(!getsockopt((int)fd,SOL_SOCKET,SO_TYPE,&type,&size) && type==SOCK_SEQPACKET &&
              !getpeername((int)fd,(struct sockaddr *)&peer,&peer_size) && peer.ss_family==AF_UNIX);
        CHECK(fcntl((int)fd,F_SETFD,FD_CLOEXEC)==0);a.launcher=(int)fd;
    }
    a.activation_enabled=getenv("NEXAUI_TEST_ACTIVATE")!=NULL;
    a.activation_hold=getenv("NEXAUI_TEST_HOLD_ACTIVATION")!=NULL;
    a.hold_after_collapse=getenv("NEXAUI_TEST_HOLD_AFTER_COLLAPSE")!=NULL;
    for(unsigned i=0;i<3;++i) {
        a.views[i].app=&a;a.views[i].index=i;ae_desktop_model_init(&a.views[i].model);
    }
    a.xkb=xkb_context_new(XKB_CONTEXT_NO_FLAGS);CHECK(a.xkb);
    CHECK(font && (a.font=load_font(font,&a.font_size)));
    int signal_fds[2]={-1,-1};
    if(owner_fixture) {
        CHECK(pipe(signal_fds)==0);
        for(unsigned i=0;i<2;++i) {
            int flags=fcntl(signal_fds[i],F_GETFL),desc=fcntl(signal_fds[i],F_GETFD);
            CHECK(flags>=0 && desc>=0 && fcntl(signal_fds[i],F_SETFL,flags|O_NONBLOCK)==0 &&
                  fcntl(signal_fds[i],F_SETFD,desc|FD_CLOEXEC)==0);
        }
        struct sigaction action={0};action.sa_handler=stop_signal;sigemptyset(&action.sa_mask);
        signal_writer=signal_fds[1];CHECK(sigaction(SIGINT,&action,NULL)==0 && sigaction(SIGTERM,&action,NULL)==0);
    }
    a.display = wl_display_connect(NULL); CHECK(a.display);
    a.registry = wl_display_get_registry(a.display); CHECK(a.registry);
    CHECK(wl_registry_add_listener(a.registry, &registry_listener, &a) == 0);
    CHECK(wl_display_roundtrip(a.display) >= 0 && a.compositor && a.shm && a.output_count == 3);
    if(a.ime_requested && a.ime_manager && a.seat) {
        a.ime_input=zwp_text_input_manager_v3_get_text_input(a.ime_manager,a.seat);CHECK(a.ime_input);
        CHECK(zwp_text_input_v3_add_listener(a.ime_input,&ime_listener,&a)==0);
    }
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
            for(unsigned i=0;i<3;++i)retire_output(&a,i);
            if(stop_requested) {
                aetheri_development_desktop_v1_retire(a.control);
                CHECK(wl_display_roundtrip(a.display)>=0 && a.retired);stop=true;break;
            }
            if(a.toggle_output>=0) {
                unsigned output=(unsigned)a.toggle_output;a.toggle_output=-1;
                bool open=!a.views[output].expanded;
                for(unsigned i=0;i<3;++i)if(a.views[i].expanded && (!open || i!=output))view_state(&a.views[i],false);
                if(open && a.views[output].layer)view_state(&a.views[output],true);
            }
            if(a.close_search) {
                a.close_search=false;
                for(unsigned i=0;i<3;++i)if(a.views[i].expanded)view_state(&a.views[i],false);
            }
            activation_step(&a);
            for(unsigned i=0;i<3;++i)if(a.views[i].dirty && a.views[i].configured)paint(&a.views[i]);
            ime_sync(&a);
            CHECK(wl_display_flush(a.display) >= 0 || errno == EAGAIN);
            struct pollfd fds[7] = {{wl_display_get_fd(a.display), POLLIN, 0}, {owner_fixture?-1:STDIN_FILENO, POLLIN, 0}, {a.launcher,POLLIN,0}, {signal_fds[0],POLLIN,0}};
            uint32_t wait_ms=accessibility_prepare(&a,&fds[4],owner_fixture?UINT32_MAX:10000u);
            int wait=wait_ms>(uint32_t)INT32_MAX?-1:(int)wait_ms;
            int ready; do { ready = poll(fds, 7, wait); } while (ready < 0 && errno == EINTR && !stop_requested);
            if(stop_requested)continue;
            CHECK(ready >= 0);
            if (fds[0].revents) CHECK(wl_display_dispatch(a.display) >= 0);
            for(unsigned i=0;i<3;++i)retire_output(&a,i);
            if(fds[2].revents) {CHECK(!(fds[2].revents&(POLLERR|POLLHUP|POLLNVAL)));launcher_reply(&a);}
            if (fds[1].revents) {
                char c; CHECK(read(STDIN_FILENO, &c, 1) == 1);
                if(strchr("enpzwfiur",c) && !a.views[0].layer) {
                    printf("IGNORED_REMOVED_OUTPUT %c\n",c);continue;
                }
                if (c == 'e') view_state(&a.views[0], true);
                else if (c == 'n') view_state(&a.views[0], false);
                else if (c == 'a') aetheri_development_desktop_v1_activate(a.control, a.first_id);
                else if (c == 's') aetheri_development_desktop_v1_activate(a.control, UINT32_MAX);
                else if (c == 'g') a.activation_hold=false;
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
    pointer_cancel_pending(&a);
    ime_stop(&a);
    if(a.ime_input)zwp_text_input_v3_destroy(a.ime_input);
    if(a.ime_manager)zwp_text_input_manager_v3_destroy(a.ime_manager);
    for (unsigned i = 0; i < 3; ++i) {
        if(a.views[i].accessibility)CHECK(nx_linux_accessibility_host_destroy(&a.views[i].accessibility)==NX_OK);
        if (a.views[i].presenter) CHECK(nx_wayland_shm_host_destroy(&a.views[i].presenter)==NX_OK);
        aetheri_desktop_view_destroy(a.views[i].view);
        if (a.views[i].layer) zwlr_layer_surface_v1_destroy(a.views[i].layer);
        if (a.views[i].surface) wl_surface_destroy(a.views[i].surface);
        if (a.outputs[i]) wl_output_destroy(a.outputs[i]);
    }
    free(a.font);
    signal_writer=-1;if(signal_fds[0]>=0)close(signal_fds[0]);if(signal_fds[1]>=0)close(signal_fds[1]);
    if(a.launcher>=0)close(a.launcher);
    xkb_state_unref(a.key_state);xkb_keymap_unref(a.keymap);xkb_context_unref(a.xkb);
    if (a.control) aetheri_development_desktop_v1_destroy(a.control);
    if (a.layers) zwlr_layer_shell_v1_destroy(a.layers);
    if (a.keyboard) wl_keyboard_release(a.keyboard);
    if (a.pointer) wl_pointer_release(a.pointer);
    if (a.seat) wl_seat_release(a.seat);
    wl_shm_destroy(a.shm); wl_compositor_destroy(a.compositor); wl_registry_destroy(a.registry);
    wl_display_disconnect(a.display); puts("CLOSED"); return 0;
}
