#include "nexaui/nexaui.h"
#include "nexaui/software.h"
#include "nexaui/wayland_clipboard.h"
#include "nexaui/wayland_outputs.h"
#ifndef NEXAUI_LINUX_WAYLAND_ONLY
#include "x11/backend.h"
#include "x11/window_internal.h"
#endif
#include "wayland/backend.h"
#include "wayland/window_internal.h"
#include "window_internal.h"
#include "accessibility.h"
#include "../../src/secure_zero.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* Discard the unselected expression at preprocessing time: Wayland-only
   builds must not require Xlib declarations or link the X11 adapter. */
#ifdef NEXAUI_LINUX_WAYLAND_ONLY
#define NX_LINUX_DISPATCH(wayland, x11) (wayland)
#else
#define NX_LINUX_DISPATCH(wayland, x11) \
    (window->backend == NX_LINUX_WAYLAND ? (wayland) : (x11))
#endif

typedef enum nx_linux_backend {
    NX_LINUX_X11,
    NX_LINUX_WAYLAND
} nx_linux_backend;

struct nx_window {
    nx_allocator allocator;
    pthread_t owner_thread;
    nx_linux_backend backend;
    const void *renderer_owner;
    nx_linux_ax *accessibility;
    size_t operations;
    uint64_t ax_revision;
    uint64_t public_clipboard_token;
    int retired, ax_publishing, ax_cancelled, polling_group;
    union {
#ifndef NEXAUI_LINUX_WAYLAND_ONLY
        nx_x11_window *x11;
#endif
        nx_wayland_window *wayland;
    } native;
};

static void *nx_linux_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_linux_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static int nx_linux_on_owner_thread(const nx_window *window) {
    return pthread_equal(pthread_self(), window->owner_thread);
}

/* A native selection admission can call the application's allocator. Retire
   immediately, but keep the wrapper and native backend alive until the outer
   owner-thread operation has stopped using them. */
static void nx_linux_window_release(nx_window *window) {
    if (--window->operations) return;
#ifndef NEXAUI_LINUX_WAYLAND_ONLY
    if (window->backend == NX_LINUX_WAYLAND)
        nx_wayland_window_destroy(window->native.wayland);
    else nx_x11_window_destroy(window->native.x11);
#else
    nx_wayland_window_destroy(window->native.wayland);
#endif
    nx_allocator allocator=window->allocator;
    allocator.deallocate(allocator.user,window,sizeof(*window));
}

int nx_linux_window_hold(nx_window *window) {
    if (!window || !nx_linux_on_owner_thread(window) || window->retired ||
        window->operations == SIZE_MAX) return 0;
    ++window->operations;
    return 1;
}
int nx_linux_window_retired(nx_window *window) {
    return !window || window->retired;
}
void nx_linux_window_unhold(nx_window *window) {
    nx_linux_window_release(window);
}

nx_result nx_window_create(const nx_window_desc *desc, nx_window **out_window) {
    if (!out_window) return NX_ERROR_ARGUMENT;
    *out_window = NULL;
    if (!desc || (desc->allocator.allocate == NULL) !=
                 (desc->allocator.deallocate == NULL)) return NX_ERROR_ARGUMENT;
    nx_allocator allocator = desc->allocator;
    if (!allocator.allocate)
        allocator = (nx_allocator){NULL, nx_linux_allocate, nx_linux_deallocate};
    const char *forced = getenv("NEXAUI_PLATFORM");
    if (forced && strcmp(forced, "wayland") && strcmp(forced, "x11"))
        return NX_ERROR_ARGUMENT;
#ifdef NEXAUI_LINUX_WAYLAND_ONLY
    if (forced && !strcmp(forced, "x11")) return NX_ERROR_UNSUPPORTED;
    nx_wayland_window *wayland = NULL;
    nx_result result = nx_wayland_window_create(desc, &wayland);
    if (result != NX_OK) return result;
#else
    int try_wayland = forced ? !strcmp(forced, "wayland") :
                      getenv("WAYLAND_DISPLAY") != NULL;
    nx_result result = NX_ERROR_PLATFORM;
    nx_wayland_window *wayland = NULL;
    nx_x11_window *x11 = NULL;
    nx_linux_backend backend = NX_LINUX_X11;
    if (try_wayland) {
        result = nx_wayland_window_create(desc, &wayland);
        if (result == NX_OK) backend = NX_LINUX_WAYLAND;
        else if (forced || result != NX_ERROR_PLATFORM) return result;
    }
    if (!wayland) {
        result = nx_x11_window_create(desc, &x11);
        if (result != NX_OK) return result;
    }
#endif
    nx_window *window = (nx_window *)allocator.allocate(allocator.user,
                                                        sizeof(*window));
    if (!window) {
#ifdef NEXAUI_LINUX_WAYLAND_ONLY
        nx_wayland_window_destroy(wayland);
#else
        if (wayland) nx_wayland_window_destroy(wayland);
        else nx_x11_window_destroy(x11);
#endif
        return NX_ERROR_MEMORY;
    }
    *window = (nx_window){0};
    window->allocator = allocator;
    window->owner_thread = pthread_self();
#ifdef NEXAUI_LINUX_WAYLAND_ONLY
    window->backend = NX_LINUX_WAYLAND;
#else
    window->backend = backend;
#endif
    window->operations=1;
#ifdef NEXAUI_LINUX_WAYLAND_ONLY
    window->native.wayland = wayland;
#else
    if (wayland) window->native.wayland = wayland;
    else window->native.x11 = x11;
#endif
    if (wayland) nx_wayland_window_set_owner(wayland, window);
    *out_window = window;
    return NX_OK;
}

static nx_result nx_linux_poll(nx_window *window, uint32_t timeout_ms,
                               int app_fd, uint32_t interests, uint32_t *app_events) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window) || window->retired || window->polling_group) return NX_ERROR_STATE;
    ++window->operations;
    if (nx_linux_ax_pump(window->accessibility)) timeout_ms=0;
    if (window->retired) { nx_linux_window_release(window); return NX_ERROR_STATE; }
    timeout_ms=nx_linux_ax_timeout(window->accessibility,timeout_ms);
    int writable=0, fd=nx_linux_ax_fd(window->accessibility,&writable);
    nx_result result=NX_LINUX_DISPATCH(nx_wayland_window_poll_with_fd(window->native.wayland,timeout_ms,fd,writable,app_fd,interests,app_events),
        nx_x11_window_poll_with_fd(window->native.x11,timeout_ms,fd,writable,app_fd,interests,app_events));
    if (!window->retired) nx_linux_ax_pump(window->accessibility);
    if (window->retired) result=NX_ERROR_STATE;
    nx_linux_window_release(window); return result;
}

nx_result nx_window_poll(nx_window *window, uint32_t timeout_ms) {
    return nx_linux_poll(window,timeout_ms,-1,0,NULL);
}

nx_result nx_window_poll_with_fd(nx_window *window, uint32_t timeout_ms,
                                 int fd, uint32_t *out_events) {
    if (out_events) *out_events=0;
    if (!window || fd<0 || !out_events) return NX_ERROR_ARGUMENT;
    return nx_linux_poll(window,timeout_ms,fd,NX_WINDOW_FD_READABLE,out_events);
}

static int nx_linux_fd_interests_valid(int fd,uint32_t interests) {
    const uint32_t allowed=NX_WINDOW_FD_READABLE|NX_WINDOW_FD_WRITABLE;
    return fd>=-1 && !(interests&~allowed) && (fd<0 ? interests==0 : interests!=0);
}
nx_result nx_window_poll_with_fd_events(nx_window *window,uint32_t timeout_ms,
    int fd,uint32_t interests,uint32_t *out_events) {
    if(out_events)*out_events=0;
    if(!window || fd<0 || !out_events || !nx_linux_fd_interests_valid(fd,interests))return NX_ERROR_ARGUMENT;
    return nx_linux_poll(window,timeout_ms,fd,interests,out_events);
}

nx_result nx_window_set_wayland_app_id(nx_window *window, const char *id) {
    if (!window || !id || !*id) return NX_ERROR_ARGUMENT;
    size_t i=0;
    for (; i<255 && id[i]; ++i) {
        unsigned char c=(unsigned char)id[i];
        if (!((c>='a' && c<='z') || (c>='A' && c<='Z') ||
              (c>='0' && c<='9') || c=='.' || c=='_' || c=='-')) return NX_ERROR_ARGUMENT;
    }
    if (i==255 && id[i]) return NX_ERROR_LIMIT;
    if (!nx_linux_on_owner_thread(window) || window->retired) return NX_ERROR_STATE;
    return NX_LINUX_DISPATCH(nx_wayland_window_set_app_id(window->native.wayland,id),
                             NX_ERROR_UNSUPPORTED);
}

nx_result nx_window_get_wayland_outputs(nx_window *window, nx_wayland_output_info *items,
                                         size_t capacity, size_t *count, uint64_t *revision) {
    if (count) *count=0;
    if (revision) *revision=0;
    if (!window || !count || !revision || (!items && capacity)) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window) || window->retired) return NX_ERROR_STATE;
    return NX_LINUX_DISPATCH(nx_wayland_window_get_outputs(window->native.wayland,items,capacity,count,revision),
                             NX_ERROR_UNSUPPORTED);
}

nx_result nx_window_poll_many_with_fd_events(nx_window *const *windows,size_t count,
                              uint32_t timeout_ms,int fd,uint32_t interests,uint32_t *out_events) {
    if(out_events)*out_events=0;
    if(!windows || !out_events || !count || count>NX_WINDOW_POLL_MAX_WINDOWS ||
       !nx_linux_fd_interests_valid(fd,interests))
        return NX_ERROR_ARGUMENT;
    nx_wayland_window *native[NX_WINDOW_POLL_MAX_WINDOWS]={0};
    nx_window *members[NX_WINDOW_POLL_MAX_WINDOWS];
    int extras[NX_WINDOW_POLL_MAX_WINDOWS]={0}, writable[NX_WINDOW_POLL_MAX_WINDOWS]={0};
    for(size_t i=0;i<count;++i) {
        members[i]=windows[i];
        if(!members[i])return NX_ERROR_ARGUMENT;
        for(size_t j=0;j<i;++j)if(members[i]==members[j])return NX_ERROR_ARGUMENT;
        if(!nx_linux_on_owner_thread(members[i]) || members[i]->retired || members[i]->polling_group)
            return NX_ERROR_STATE;
        if(members[i]->backend!=NX_LINUX_WAYLAND)return NX_ERROR_UNSUPPORTED;
        if(members[i]->operations==SIZE_MAX)return NX_ERROR_LIMIT;
    }
    for(size_t i=0;i<count;++i){++members[i]->operations;members[i]->polling_group=1;native[i]=members[i]->native.wayland;}
    nx_result result=NX_OK;
    for(size_t i=0;i<count;++i) {
        if(nx_linux_ax_pump(members[i]->accessibility))timeout_ms=0;
        for(size_t j=0;j<count;++j)if(members[j]->retired){result=NX_ERROR_STATE;goto done;}
    }
    for(size_t i=0;i<count;++i) {
        timeout_ms=nx_linux_ax_timeout(members[i]->accessibility,timeout_ms);
        extras[i]=nx_linux_ax_fd(members[i]->accessibility,&writable[i]);
    }
    result=nx_wayland_windows_poll(native,count,timeout_ms,extras,writable,fd,interests,out_events);
    for(size_t i=0;i<count;++i) {
        if(!members[i]->retired)nx_linux_ax_pump(members[i]->accessibility);
    }
    for(size_t i=0;i<count;++i)if(members[i]->retired && result==NX_OK)result=NX_ERROR_STATE;
done:
    for(size_t i=0;i<count;++i)members[i]->polling_group=0;
    for(size_t i=0;i<count;++i)nx_linux_window_release(members[i]);
    return result;
}
nx_result nx_window_poll_many(nx_window *const *windows,size_t count,
                              uint32_t timeout_ms,int fd,uint32_t *out_events) {
    return nx_window_poll_many_with_fd_events(windows,count,timeout_ms,fd,
        fd>=0?NX_WINDOW_FD_READABLE:0,out_events);
}

nx_result nx_window_get_info(nx_window *window, nx_window_info *out_info) {
    if (!window) return NX_ERROR_ARGUMENT;
    return NX_LINUX_DISPATCH(nx_wayland_window_get_info(window->native.wayland, out_info),
        nx_x11_window_get_info(window->native.x11, out_info));
}

nx_result nx_window_take_close_request(nx_window *window,nx_window_close_request *out) {
    if(!out)return NX_ERROR_ARGUMENT;
    *out=(nx_window_close_request){0};
    if(!window)return NX_ERROR_ARGUMENT;
    if(!nx_linux_on_owner_thread(window)||window->retired)return NX_ERROR_STATE;
    nx_result r=NX_LINUX_DISPATCH(nx_wayland_window_take_close(window->native.wayland,&out->id),
        nx_x11_window_take_close(window->native.x11,&out->id));
    if(r==NX_OK&&out->id)out->window=window;
    return r;
}
nx_result nx_window_resolve_close(nx_window *window,const nx_window_close_request *request,nx_window_close_action action) {
    if(!window||!request||(action!=NX_WINDOW_CLOSE_CANCEL&&action!=NX_WINDOW_CLOSE_ACCEPT))return NX_ERROR_ARGUMENT;
    if(!nx_linux_on_owner_thread(window)||window->retired||request->window!=window)return NX_ERROR_STATE;
    return NX_LINUX_DISPATCH(nx_wayland_window_resolve_close(window->native.wayland,request->id,action),
        nx_x11_window_resolve_close(window->native.x11,request->id,action));
}

nx_result nx_window_get_pixel_size(nx_window *window, nx_pixel_size *out_size) {
    if (!out_size) return NX_ERROR_ARGUMENT;
    *out_size=(nx_pixel_size){0};
    if (!window) return NX_ERROR_ARGUMENT;
    return NX_LINUX_DISPATCH(nx_wayland_window_get_pixel_size(window->native.wayland,out_size),
        nx_x11_window_get_pixel_size(window->native.x11,out_size));
}

nx_result nx_window_renderer_acquire(nx_window *window, const void *owner) {
    if (!window || !owner) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window) || window->renderer_owner)
        return NX_ERROR_STATE;
    if (window->backend == NX_LINUX_WAYLAND) {
        nx_result result = nx_wayland_window_set_external_renderer(
            window->native.wayland, 1);
        if (result != NX_OK) return result;
    }
    window->renderer_owner = owner;
    return NX_OK;
}

nx_result nx_window_renderer_release(nx_window *window, const void *owner) {
    if (!window || !owner) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window) || window->renderer_owner != owner)
        return NX_ERROR_STATE;
    nx_result result=NX_LINUX_DISPATCH(nx_wayland_window_set_external_renderer(window->native.wayland, 0),
        NX_OK);
    window->renderer_owner = NULL;
    return result;
}

struct nx_software_view {
    nx_window *window;
    pthread_t owner;
    int busy;
};

nx_result nx_software_view_open(nx_window *window, nx_software_view **out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out=NULL;
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window) || window->retired) return NX_ERROR_STATE;
    if (window->backend != NX_LINUX_WAYLAND) return NX_ERROR_UNSUPPORTED;
    nx_software_view *view=calloc(1,sizeof(*view));
    if (!view) return NX_ERROR_MEMORY;
    view->owner=pthread_self(); view->window=window;
    if (!nx_linux_window_hold(window)) { free(view); return NX_ERROR_STATE; }
    nx_result result=nx_window_renderer_acquire(window,view);
    if (result!=NX_OK) { nx_linux_window_unhold(window); free(view); return result; }
    *out=view;
    return NX_OK;
}

nx_result nx_software_view_render(nx_software_view *view,
    const nx_command *commands, size_t count, nx_color clear,
    nx_software_view_outcome *outcome) {
    if (outcome) *outcome=NX_SOFTWARE_VIEW_SUSPENDED;
    if (!view || !outcome) return NX_ERROR_ARGUMENT;
    if (!pthread_equal(view->owner,pthread_self()) || view->busy || view->window->retired)
        return NX_ERROR_STATE;
    view->busy=1;
    int internal=2;
    nx_result result=nx_wayland_window_software_render(view->window->native.wayland,
                                                        commands,count,clear,&internal);
    view->busy=0;
    if (result==NX_OK) *outcome=(nx_software_view_outcome)internal;
    return result;
}

nx_result nx_software_view_close(nx_software_view **address) {
    if (!address) return NX_ERROR_ARGUMENT;
    nx_software_view *view=*address;
    if (!view) return NX_OK;
    if (!pthread_equal(view->owner,pthread_self()) || view->busy) return NX_ERROR_STATE;
    view->busy=1;
    nx_window *window=view->window;
    nx_result result=NX_OK;
    if (window->retired) window->renderer_owner=NULL;
    else result=nx_window_renderer_release(window,view);
    *address=NULL;
    free(view);
    nx_linux_window_unhold(window);
    return result;
}

nx_result nx_window_set_cursor(nx_window *window, nx_cursor_shape shape) {
    if (!window) return NX_ERROR_ARGUMENT;
    return NX_LINUX_DISPATCH(nx_wayland_window_set_cursor(window->native.wayland, shape),
        nx_x11_window_set_cursor(window->native.x11, shape));
}

nx_result nx_window_get_reduced_motion(nx_window *window, int *out_enabled) {
    if (!window) return NX_ERROR_ARGUMENT;
    return NX_LINUX_DISPATCH(nx_wayland_window_get_reduced_motion(window->native.wayland,
                                              out_enabled),
        nx_x11_window_get_reduced_motion(window->native.x11, out_enabled));
}

nx_result nx_window_get_status(nx_window *window, nx_window_status *out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out = (nx_window_status){0};
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    return NX_LINUX_DISPATCH(nx_wayland_window_get_status(window->native.wayland, out),
        nx_x11_window_get_status(window->native.x11, out));
}

nx_result nx_window_minimize(nx_window *window) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    return NX_LINUX_DISPATCH(nx_wayland_window_minimize(window->native.wayland),
        nx_x11_window_minimize(window->native.x11));
}
nx_result nx_window_maximize(nx_window *window) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    return NX_LINUX_DISPATCH(nx_wayland_window_maximize(window->native.wayland),
        nx_x11_window_maximize(window->native.x11));
}
nx_result nx_window_fullscreen(nx_window *window) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    return NX_LINUX_DISPATCH(nx_wayland_window_fullscreen(window->native.wayland),
        nx_x11_window_fullscreen(window->native.x11));
}
nx_result nx_window_restore(nx_window *window) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    return NX_LINUX_DISPATCH(nx_wayland_window_restore(window->native.wayland),
        nx_x11_window_restore(window->native.x11));
}
nx_result nx_window_begin_move(nx_window *window) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    return NX_LINUX_DISPATCH(nx_wayland_window_begin_move(window->native.wayland),
        nx_x11_window_begin_move(window->native.x11));
}
nx_result nx_window_begin_resize(nx_window *window, nx_window_resize_edge edge) {
    if (!window || edge < NX_WINDOW_RESIZE_TOP_LEFT || edge > NX_WINDOW_RESIZE_LEFT) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    return NX_LINUX_DISPATCH(nx_wayland_window_begin_resize(window->native.wayland, edge),
        nx_x11_window_begin_resize(window->native.x11, edge));
}

nx_result nx_window_set_ime_rect(nx_window *window, nx_rect caret) {
    if (!window) return NX_ERROR_ARGUMENT;
    return NX_LINUX_DISPATCH(nx_wayland_window_set_ime_rect(window->native.wayland, caret),
        nx_x11_window_set_ime_rect(window->native.x11, caret));
}

nx_result nx_window_set_ime_enabled(nx_window *window, int enabled) {
    if (!window || (enabled != 0 && enabled != 1)) return NX_ERROR_ARGUMENT;
    return NX_LINUX_DISPATCH(nx_wayland_window_set_ime_enabled(window->native.wayland, enabled),
        nx_x11_window_set_ime_enabled(window->native.x11, enabled));
}

nx_result nx_window_set_ime_transaction_events(nx_window *window, int enabled) {
    if (!window || (enabled != 0 && enabled != 1)) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    return NX_LINUX_DISPATCH(nx_wayland_window_set_ime_transaction_events(
        window->native.wayland, enabled), NX_ERROR_UNSUPPORTED);
}

nx_result nx_window_set_ime_purpose(nx_window *window,
                                    nx_ime_purpose purpose) {
    if (!window || (purpose != NX_IME_PURPOSE_NORMAL &&
                    purpose != NX_IME_PURPOSE_PASSWORD))
        return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    if (window->backend == NX_LINUX_WAYLAND)
        return nx_wayland_window_set_ime_purpose(window->native.wayland,
                                                 purpose);
    return purpose == NX_IME_PURPOSE_NORMAL ? NX_OK : NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_get_ime_preedit(nx_window *window,
    char *bytes, size_t capacity, size_t *out_length,
    int32_t *out_cursor_begin, int32_t *out_cursor_end) {
    if (!out_length || !out_cursor_begin || !out_cursor_end ||
        (!bytes && capacity)) return NX_ERROR_ARGUMENT;
    *out_length = 0;
    *out_cursor_begin = *out_cursor_end = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    return NX_LINUX_DISPATCH(nx_wayland_window_get_ime_preedit(window->native.wayland,
            bytes, capacity, out_length, out_cursor_begin,
            out_cursor_end),
        nx_x11_window_get_ime_preedit(window->native.x11,
            bytes, capacity, out_length, out_cursor_begin, out_cursor_end));
}

nx_result nx_window_get_ime_preedit_for(nx_window *window,
    uint64_t target, uint64_t revision, char *bytes, size_t capacity,
    size_t *out_length, int32_t *out_cursor_begin, int32_t *out_cursor_end) {
    if (!out_length || !out_cursor_begin || !out_cursor_end || (!bytes && capacity))
        return NX_ERROR_ARGUMENT;
    *out_length=0;*out_cursor_begin=*out_cursor_end=0;
    if (!window || !target || !revision) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    return NX_LINUX_DISPATCH(nx_wayland_window_get_ime_preedit_for(window->native.wayland,
        target,revision,bytes,capacity,out_length,out_cursor_begin,out_cursor_end),NX_ERROR_UNSUPPORTED);
}

nx_result nx_window_set_ime_surrounding_text(nx_window *window,
    uint64_t target, uint64_t revision, const char *bytes, size_t length,
    size_t cursor, size_t anchor) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    return NX_LINUX_DISPATCH(nx_wayland_window_set_ime_surrounding_text(window->native.wayland,
            target, revision, bytes, length, cursor, anchor),
        NX_ERROR_UNSUPPORTED);
}

nx_result nx_window_next_event(nx_window *window, nx_event *out_event,
                                int *available) {
    if (!window) {
        if (out_event) *out_event = (nx_event){0};
        if (available) *available = 0;
        return NX_ERROR_ARGUMENT;
    }
    return NX_LINUX_DISPATCH(nx_wayland_window_next_event(window->native.wayland,
                                      out_event, available),
        nx_x11_window_next_event(window->native.x11, out_event, available));
}

nx_result nx_window_route_events(nx_window *window, nx_context *context,
                                  size_t *out_count) {
    if (out_count) *out_count=0;
    if (!window || !context || !out_count) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window) || window->retired ||
        !nx_linux_ax_matches(window->accessibility,context)) return NX_ERROR_STATE;
    ++window->operations;
    nx_result result=NX_LINUX_DISPATCH(nx_wayland_window_route_events(window->native.wayland,context,out_count),
        nx_x11_window_route_events(window->native.x11,context,out_count));
    size_t native_count=0;
    if (result==NX_OK && !window->retired)
        result=nx_linux_ax_route(window->accessibility,context,&native_count);
    *out_count+=native_count;
    nx_linux_window_release(window);
    return result;
}

nx_result nx_window_next_file_drop(nx_window *window, char *path_utf8,
                                    size_t capacity, size_t *out_length,
                                    nx_file_drop_info *out_info,
                                    int *available) {
    if (!window) {
        if (out_length) *out_length = 0;
        if (out_info) *out_info = (nx_file_drop_info){0};
        if (available) *available = 0;
        return NX_ERROR_ARGUMENT;
    }
    return NX_LINUX_DISPATCH(nx_wayland_window_next_file_drop(window->native.wayland, path_utf8,
                    capacity, out_length, out_info, available),
        nx_x11_window_next_file_drop(window->native.x11, path_utf8,
                    capacity, out_length, out_info, available));
}

nx_result nx_window_clipboard_set_text(nx_window *window,
                                        const char *bytes, size_t length) {
    if (!window) return NX_ERROR_ARGUMENT;
    return NX_LINUX_DISPATCH(nx_wayland_window_clipboard_set_text(window->native.wayland,
                                              bytes, length),
        nx_x11_window_clipboard_set_text(window->native.x11, bytes, length));
}

nx_result nx_window_clipboard_get_text(nx_window *window,
                                        char *bytes, size_t capacity,
                                        size_t *out_length) {
    if (!window) {
        if (out_length) *out_length = 0;
        return NX_ERROR_ARGUMENT;
    }
    if (!nx_linux_on_owner_thread(window) || window->retired) {
        if (out_length) *out_length = 0;
        return NX_ERROR_STATE;
    }
    ++window->operations;
    nx_result result = NX_LINUX_DISPATCH(nx_wayland_window_clipboard_get_text(window->native.wayland,
                    bytes, capacity, out_length),
        nx_x11_window_clipboard_get_text(window->native.x11,
                    bytes, capacity, out_length));
    if (window->retired) {
        if (out_length) *out_length = 0;
        result = NX_ERROR_STATE;
    }
    nx_linux_window_release(window);
    return result;
}

nx_result nx_linux_window_clipboard_fetch_text(nx_window *window, char **out_bytes,
                                                size_t *out_length) {
    if (!out_bytes || !out_length) return NX_ERROR_ARGUMENT;
    *out_bytes = NULL; *out_length = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window) || window->retired) return NX_ERROR_STATE;
    ++window->operations;
    nx_result result;
#ifndef NEXAUI_LINUX_WAYLAND_ONLY
    if (window->backend == NX_LINUX_X11) {
        size_t length = 0;
        result = nx_x11_window_clipboard_get_text(window->native.x11, NULL, 0, &length);
        if (result == NX_OK && length != SIZE_MAX && !window->retired) {
            char *bytes = malloc(length + 1u);
            if (!bytes) result = NX_ERROR_MEMORY;
            else {
                size_t actual = 0;
                result = nx_x11_window_clipboard_get_text(window->native.x11, bytes, length + 1u, &actual);
                if (result == NX_OK && actual <= length) { *out_bytes = bytes; *out_length = actual; }
                else free(bytes);
            }
        } else if (result == NX_OK) result = NX_ERROR_STATE;
    } else
#endif
        result = nx_wayland_window_clipboard_fetch_text(window->native.wayland, out_bytes, out_length);
    if (window->retired) {
        free(*out_bytes); *out_bytes = NULL; *out_length = 0; result = NX_ERROR_STATE;
    }
    nx_linux_window_release(window);
    return result;
}

nx_result nx_linux_window_clipboard_async_start(nx_window *window, size_t limit,
    uint32_t timeout_ms, int sensitive, uint64_t *token) {
    if (token) *token = 0;
    if (!window || !token) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window) || window->retired) return NX_ERROR_STATE;
#ifndef NEXAUI_LINUX_WAYLAND_ONLY
    if (window->backend != NX_LINUX_WAYLAND) return NX_ERROR_UNSUPPORTED;
#endif
    ++window->operations;
    nx_result result = nx_wayland_window_clipboard_async_start(window->native.wayland, limit, timeout_ms, sensitive, token);
    if (window->retired) { *token = 0; result = NX_ERROR_STATE; }
    nx_linux_window_release(window); return result;
}

nx_result nx_linux_window_clipboard_async_take(nx_window *window, uint64_t token,
    char **bytes, size_t *length, int *ready) {
    if (!window || !bytes || !length || !ready) return NX_ERROR_ARGUMENT;
    *bytes = NULL; *length = 0; *ready = 0;
    if (!nx_linux_on_owner_thread(window) || window->retired) return NX_ERROR_STATE;
#ifndef NEXAUI_LINUX_WAYLAND_ONLY
    if (window->backend != NX_LINUX_WAYLAND) return NX_ERROR_UNSUPPORTED;
#endif
    ++window->operations;
    nx_result result = nx_wayland_window_clipboard_async_take(window->native.wayland, token, bytes, length, ready);
    if (window->retired) {
        if (*bytes) { nx_private_secure_zero(*bytes, *length); free(*bytes); }
        *bytes = NULL; *length = 0; *ready = 0; result = NX_ERROR_STATE;
    }
    nx_linux_window_release(window); return result;
}

nx_result nx_linux_window_clipboard_async_cancel(nx_window *window, uint64_t token) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window) || window->retired) return NX_ERROR_STATE;
#ifndef NEXAUI_LINUX_WAYLAND_ONLY
    if (window->backend != NX_LINUX_WAYLAND) return NX_ERROR_UNSUPPORTED;
#endif
    ++window->operations;
    nx_result result = nx_wayland_window_clipboard_async_cancel(window->native.wayland, token);
    if (window->retired) result = NX_ERROR_STATE;
    nx_linux_window_release(window); return result;
}

int nx_linux_window_clipboard_async_current(nx_window *window, uint64_t token) {
    if (!window || !nx_linux_on_owner_thread(window) || window->retired) return 0;
#ifndef NEXAUI_LINUX_WAYLAND_ONLY
    if (window->backend != NX_LINUX_WAYLAND) return 0;
#endif
    return nx_wayland_window_clipboard_async_current(window->native.wayland, token);
}

nx_result nx_linux_window_clipboard_async_cancel_active(nx_window *window) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window) || window->retired) return NX_ERROR_STATE;
#ifndef NEXAUI_LINUX_WAYLAND_ONLY
    if (window->backend != NX_LINUX_WAYLAND) return NX_ERROR_UNSUPPORTED;
#endif
    if (window->public_clipboard_token &&
        nx_wayland_window_clipboard_async_current(window->native.wayland, window->public_clipboard_token))
        return NX_ERROR_STATE;
    ++window->operations;
    nx_result result = nx_wayland_window_clipboard_async_cancel_active(window->native.wayland);
    if (window->retired) result = NX_ERROR_STATE;
    nx_linux_window_release(window); return result;
}

void nx_linux_clipboard_cancel_token(uint64_t token) { nx_wayland_clipboard_cancel_token(token); }

int nx_linux_window_clipboard_async_supported(nx_window *window) {
    if (!window || !nx_linux_on_owner_thread(window) || window->retired) return 0;
#ifndef NEXAUI_LINUX_WAYLAND_ONLY
    if (window->backend != NX_LINUX_WAYLAND) return 0;
#endif
    return 1;
}

nx_result nx_wayland_clipboard_read_begin(nx_window *window, size_t limit,
    uint32_t timeout_ms, int sensitive, uint64_t *out_token) {
    if (out_token) *out_token = 0;
    if (!window || !out_token || (sensitive != 0 && sensitive != 1)) return NX_ERROR_ARGUMENT;
    if (!nx_linux_window_hold(window)) return NX_ERROR_STATE;
    nx_result result = nx_linux_window_clipboard_async_start(window, limit, timeout_ms, sensitive, out_token);
    if (result == NX_OK) window->public_clipboard_token = *out_token;
    nx_linux_window_unhold(window);
    return result;
}

nx_result nx_wayland_clipboard_read_take(nx_window *window, uint64_t token,
    char **bytes, size_t *length, int *ready) {
    if (bytes) *bytes = NULL;
    if (length) *length = 0;
    if (ready) *ready = 0;
    if (!window || !bytes || !length || !ready || !token) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window) || window->retired || window->public_clipboard_token != token)
        return NX_ERROR_STATE;
    if (!nx_linux_window_hold(window)) return NX_ERROR_STATE;
    nx_result result = nx_linux_window_clipboard_async_take(window, token, bytes, length, ready);
    if (*ready || result == NX_ERROR_STATE) window->public_clipboard_token = 0;
    nx_linux_window_unhold(window);
    return result;
}

nx_result nx_wayland_clipboard_read_cancel(nx_window *window, uint64_t token) {
    if (!window || !token) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window) || window->retired || window->public_clipboard_token != token)
        return NX_ERROR_STATE;
    if (!nx_linux_window_hold(window)) return NX_ERROR_STATE;
    nx_result result = nx_linux_window_clipboard_async_cancel(window, token);
    if (result == NX_OK || result == NX_ERROR_STATE) window->public_clipboard_token = 0;
    nx_linux_window_unhold(window);
    return result;
}

void nx_wayland_clipboard_read_release(char *bytes, size_t length) {
    if (bytes) { nx_private_secure_zero(bytes, length); free(bytes); }
}

void nx_window_destroy(nx_window *window) {
    if (!window || !nx_linux_on_owner_thread(window) || window->retired) return;
    window->retired=1;
    nx_linux_ax *bridge=window->accessibility;window->accessibility=NULL;
    nx_linux_ax_destroy(bridge);
    nx_linux_window_release(window);
}

#ifndef NEXAUI_LINUX_WAYLAND_ONLY
Display *nx_window_x11_display(nx_window *window) {
    return window && window->backend == NX_LINUX_X11 ?
        nx_x11_window_x11_display(window->native.x11) : NULL;
}

Window nx_window_x11_id(nx_window *window) {
    return window && window->backend == NX_LINUX_X11 ?
        nx_x11_window_x11_id(window->native.x11) : None;
}

#ifdef NEXAUI_X11_TEST_HOOKS
nx_result nx_window_x11_test_ime_edit(nx_window *window,
    const XIMText *text, int first, int removed, int caret) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (window->backend != NX_LINUX_X11) return NX_ERROR_UNSUPPORTED;
    return nx_x11_window_test_ime_edit(window->native.x11, text,
                                        first, removed, caret);
}

nx_result nx_window_x11_test_ime_clear(nx_window *window) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (window->backend != NX_LINUX_X11) return NX_ERROR_UNSUPPORTED;
    return nx_x11_window_test_ime_clear(window->native.x11);
}
#endif
#endif /* !NEXAUI_LINUX_WAYLAND_ONLY */

struct wl_display *nx_window_wayland_display(nx_window *window) {
    return window ? NX_LINUX_DISPATCH(nx_wayland_window_display(window->native.wayland),
        NULL) : NULL;
}

struct wl_surface *nx_window_wayland_surface(nx_window *window) {
    return window ? NX_LINUX_DISPATCH(nx_wayland_window_surface(window->native.wayland),
        NULL) : NULL;
}

unsigned nx_window_wayland_ready(nx_window *window) {
    return window ? NX_LINUX_DISPATCH(nx_wayland_window_ready(window->native.wayland),
        0u) : 0u;
}

#ifdef NEXAUI_WAYLAND_TEST_HOOKS
nx_result nx_window_wayland_test_uri_drop(nx_window *window,
                                          const char *text, size_t length,
                                          float x, float y) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_uri_drop(window->native.wayland, text,
                                           length, x, y);
}

nx_result nx_window_wayland_test_clipboard_roundtrip(
    nx_window *window, const char *input, size_t length,
    char *output, size_t capacity, size_t *out_length) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_clipboard_roundtrip(
        window->native.wayland, input, length, output, capacity, out_length);
}

nx_result nx_window_wayland_test_pointer_enter(nx_window *window,
                                                int x, int y) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_pointer_enter(window->native.wayland,
                                                 x, y);
}

nx_result nx_window_wayland_test_focus(nx_window *window, int focused) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_focus(window->native.wayland, focused);
}

nx_result nx_window_wayland_test_key(nx_window *window, char letter, int down) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_key(window->native.wayland, letter, down);
}

nx_result nx_window_wayland_test_repeat_info(nx_window *window,
                                              int rate, int delay_ms) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_repeat_info(window->native.wayland,
                                               rate, delay_ms);
}

nx_result nx_window_wayland_test_output(nx_window *window,
                                        int scale, int entered) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_output(window->native.wayland,
                                         scale, entered);
}

nx_result nx_window_wayland_test_extents(nx_window *window,
                                         int *background_width,
                                         int *background_height,
                                         int *cursor_width,
                                         int *cursor_height) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_extents(window->native.wayland,
        background_width, background_height, cursor_width, cursor_height);
}

nx_result nx_window_wayland_test_ime_commit(nx_window *window,
                                            const char *text, int stale) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_ime_commit(window->native.wayland,
                                             text, stale);
}

nx_result nx_window_wayland_test_ime_preedit(nx_window *window,
    const char *text, int32_t cursor, int stale) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_ime_preedit(window->native.wayland,
        text, cursor, stale);
}

nx_result nx_window_wayland_test_ime_state(nx_window *window,
                                           int *requested, int *activated) {
    if (!window || window->backend != NX_LINUX_WAYLAND)
        return NX_ERROR_ARGUMENT;
    return nx_wayland_window_test_ime_state(window->native.wayland,
                                            requested, activated);
}
#endif

nx_result nx_window_publish_semantics(nx_window *window, const nx_context *context) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window) || window->retired) return NX_ERROR_STATE;
    if (!context) {
        ++window->operations;window->ax_cancelled=1;
        nx_linux_ax *bridge=window->accessibility;window->accessibility=NULL;
        nx_linux_ax_destroy(bridge);nx_linux_window_release(window);return NX_OK;
    }
    if (window->ax_publishing) {window->ax_cancelled=1;return NX_ERROR_STATE;}
    if (window->ax_revision==UINT64_MAX) return NX_ERROR_LIMIT;
    ++window->operations;++window->ax_revision;
    window->ax_publishing=1;window->ax_cancelled=0;
    nx_result result=nx_linux_ax_publish(&window->accessibility,window,window->allocator,context);
    window->ax_publishing=0;
    nx_linux_window_release(window);return result;
}
uint64_t nx_linux_window_ax_revision(nx_window *window) {return window->ax_revision;}
int nx_linux_window_ax_current(nx_window *window,uint64_t revision) {
    return !window->retired && !window->ax_cancelled && window->ax_revision==revision;
}
void nx_linux_window_ax_geometry(nx_window *window,nx_linux_ax_geometry *out) {
#ifdef NEXAUI_LINUX_WAYLAND_ONLY
    nx_wayland_window_ax_geometry(window->native.wayland,out);
#else
    if (window->backend==NX_LINUX_WAYLAND)
        nx_wayland_window_ax_geometry(window->native.wayland,out);
    else nx_x11_window_ax_geometry(window->native.x11,out);
#endif
}
int nx_linux_window_ax_focus(nx_window *window) {
    return NX_LINUX_DISPATCH(nx_wayland_window_ax_focus(window->native.wayland),
        nx_x11_window_ax_focus(window->native.x11));
}
#ifdef NEXAUI_X11_TEST_HOOKS
nx_result nx_window_linux_ax_test_disconnect(nx_window *window) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_linux_on_owner_thread(window)) return NX_ERROR_STATE;
    return nx_linux_ax_test_disconnect(window->accessibility);
}
#endif
