#define _GNU_SOURCE
#include "nexaui/wayland_host.h"
#include "pixels.h"
#include "shm_file.h"
#include "../../../src/internal.h"
#include "../../../src/renderer_backend.h"

#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

typedef struct nx_wayland_host_buffer {
    struct nx_wayland_host_buffer *next;
    struct nx_wayland_shm_host *host;
    struct wl_buffer *buffer;
    void *pixels;
    size_t size;
} nx_wayland_host_buffer;

struct nx_wayland_shm_host {
    struct wl_display *display;
    struct wl_shm *shm;
    struct wl_surface *surface;
    struct wl_event_queue *queue;
    pthread_t owner;
    nx_wayland_host_buffer *buffers;
    unsigned count;
    int polling;
};

static void nx_host_buffer_destroy(nx_wayland_host_buffer *buffer) {
    nx_wayland_shm_host *host=buffer->host;
    nx_wayland_host_buffer **slot=&host->buffers;
    while (*slot && *slot!=buffer) slot=&(*slot)->next;
    if (*slot) { *slot=buffer->next; --host->count; }
    if (buffer->buffer) wl_buffer_destroy(buffer->buffer);
    if (buffer->pixels) munmap(buffer->pixels,buffer->size);
    free(buffer);
}

static void nx_host_buffer_release(void *data, struct wl_buffer *native) {
    (void)native;
    nx_host_buffer_destroy(data);
}
static const struct wl_buffer_listener nx_host_buffer_listener={nx_host_buffer_release};

nx_result nx_wayland_shm_host_create(struct wl_display *display, struct wl_shm *shm, struct wl_surface *surface,
    struct wl_event_queue *queue, nx_wayland_shm_host **out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out=NULL;
    if (!display || !shm || !surface) return NX_ERROR_ARGUMENT;
    if (wl_display_get_error(display)) return NX_ERROR_PLATFORM;
    nx_wayland_shm_host *host=calloc(1,sizeof(*host));
    if (!host) return NX_ERROR_MEMORY;
    host->display=display; host->shm=shm; host->surface=surface; host->queue=queue;
    host->owner=pthread_self(); *out=host;
    return NX_OK;
}

nx_result nx_wayland_shm_host_present(nx_wayland_shm_host *host,
    nx_pixel_size size, double scale, const nx_command *commands, size_t count,
    nx_color clear, nx_wayland_shm_outcome *outcome) {
    if (outcome) *outcome=NX_WAYLAND_SHM_SUSPENDED;
    if (!host || !outcome || !isfinite(scale) || scale<.125 || scale>8 ||
        !nx_valid_color(clear) || (!commands && count) ||
        (!!size.width != !!size.height)) return NX_ERROR_ARGUMENT;
    if (!pthread_equal(host->owner,pthread_self()) || host->polling) return NX_ERROR_STATE;
    if (wl_display_get_error(host->display)) return NX_ERROR_PLATFORM;
    if (count>SIZE_MAX/sizeof(*commands)) return NX_ERROR_LIMIT;
    nx_result result=nx_render_validate_commands(commands,count);
    if (result!=NX_OK) return result;
    if (!size.width) return NX_OK;
    if (size.width>(uint32_t)(INT_MAX/4) ||
        size.height>(uint32_t)(INT_MAX/(int)(size.width*4u))) return NX_ERROR_LIMIT;
    size_t bytes=(size_t)size.width*4u*size.height;
    if (bytes>128u*1024u*1024u) return NX_ERROR_LIMIT;
    if (host->count>=3u) { *outcome=NX_WAYLAND_SHM_BUSY; return NX_OK; }
    nx_wayland_host_buffer *buffer=calloc(1,sizeof(*buffer));
    if (!buffer) return NX_ERROR_MEMORY;
    buffer->host=host; buffer->size=bytes;
    int fd=nx_wayland_shm_file(bytes);
    if (fd<0) { free(buffer); return NX_ERROR_PLATFORM; }
    buffer->pixels=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if (buffer->pixels==MAP_FAILED) {
        close(fd); free(buffer); return NX_ERROR_PLATFORM;
    }
    result=nx_headless_render_scaled(commands,count,buffer->pixels,size.width,size.height,
                                      (size_t)size.width*4u,clear,scale);
    if (result!=NX_OK) { close(fd); nx_host_buffer_destroy(buffer); return result; }
    nx_wayland_premultiply_argb(buffer->pixels,bytes);
    struct wl_shm_pool *pool=wl_shm_create_pool(host->shm,fd,(int32_t)bytes);
    close(fd);
    if (!pool) { nx_host_buffer_destroy(buffer); return NX_ERROR_MEMORY; }
    wl_proxy_set_queue((struct wl_proxy *)pool,host->queue);
    buffer->buffer=wl_shm_pool_create_buffer(pool,0,(int32_t)size.width,
        (int32_t)size.height,(int32_t)(size.width*4u),WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    if (!buffer->buffer || wl_buffer_add_listener(buffer->buffer,&nx_host_buffer_listener,buffer)!=0) {
        nx_host_buffer_destroy(buffer); return NX_ERROR_MEMORY;
    }
    buffer->next=host->buffers; host->buffers=buffer; ++host->count;
    wl_surface_attach(host->surface,buffer->buffer,0,0);
    wl_surface_damage(host->surface,0,0,INT_MAX,INT_MAX);
    wl_surface_commit(host->surface);
    if (wl_display_get_error(host->display)) return NX_ERROR_PLATFORM;
    *outcome=NX_WAYLAND_SHM_READY;
    return NX_OK;
}

static int nx_host_dispatch_pending(nx_wayland_shm_host *host) {
    return host->queue ? wl_display_dispatch_queue_pending(host->display,host->queue) :
                         wl_display_dispatch_pending(host->display);
}

nx_result nx_wayland_shm_host_poll(nx_wayland_shm_host *host,
    uint32_t timeout_ms, int app_fd, uint32_t *out_events) {
    if (out_events) *out_events=0;
    if (!host || !out_events || app_fd < -1) return NX_ERROR_ARGUMENT;
    if (!pthread_equal(host->owner,pthread_self()) || host->polling) return NX_ERROR_STATE;
    int display_fd=wl_display_get_fd(host->display);
    if (app_fd>=0 && app_fd==display_fd) return NX_ERROR_ARGUMENT;
    if (wl_display_get_error(host->display)) return NX_ERROR_PLATFORM;
    host->polling=1;
    nx_result result=NX_OK;
    uint32_t events=0;
    int dispatched=nx_host_dispatch_pending(host);
    if (dispatched<0) { result=NX_ERROR_PLATFORM; goto done; }
    if (dispatched) timeout_ms=0;
    while ((host->queue ? wl_display_prepare_read_queue(host->display,host->queue) :
                          wl_display_prepare_read(host->display))!=0) {
        dispatched=nx_host_dispatch_pending(host);
        if (dispatched<0) { result=NX_ERROR_PLATFORM; goto done; }
        if (wl_display_get_error(host->display)) { result=NX_ERROR_PLATFORM; goto done; }
        if (dispatched) timeout_ms=0;
    }
    int writable=0;
    if (wl_display_flush(host->display)<0) {
        if (errno!=EAGAIN) { wl_display_cancel_read(host->display); result=NX_ERROR_PLATFORM; goto done; }
        writable=1;
    }
    struct pollfd fds[2]={{display_fd,(short)(POLLIN|(writable?POLLOUT:0)),0},
                          {app_fd,POLLIN,0}};
    int timeout=timeout_ms>(uint32_t)INT_MAX?INT_MAX:(int)timeout_ms;
    int ready=poll(fds,app_fd>=0?2u:1u,timeout);
    int poll_error=errno;
    if (ready>0 && app_fd>=0) {
        if (fds[1].revents&POLLIN) events|=NX_WINDOW_FD_READABLE;
        if (fds[1].revents&POLLHUP) events|=NX_WINDOW_FD_HANGUP;
        if (fds[1].revents&(POLLERR|POLLNVAL)) events|=NX_WINDOW_FD_ERROR;
    }
    if (ready>0 && (fds[0].revents&POLLIN)) {
        if (wl_display_read_events(host->display)<0) { result=NX_ERROR_PLATFORM; goto done; }
    } else {
        wl_display_cancel_read(host->display);
        if ((ready<0 && poll_error!=EINTR) ||
            (ready>0 && (fds[0].revents&(POLLERR|POLLHUP|POLLNVAL)))) {
            result=NX_ERROR_PLATFORM; goto done;
        }
    }
    if (ready>0 && (fds[0].revents&POLLOUT) &&
        wl_display_flush(host->display)<0 && errno!=EAGAIN) {
        result=NX_ERROR_PLATFORM; goto done;
    }
    if (nx_host_dispatch_pending(host)<0 || wl_display_get_error(host->display))
        result=NX_ERROR_PLATFORM;
done:
    host->polling=0;
    if (result==NX_OK) *out_events=events;
    return result;
}

nx_result nx_wayland_shm_host_destroy(nx_wayland_shm_host **address) {
    if (!address) return NX_ERROR_ARGUMENT;
    nx_wayland_shm_host *host=*address;
    if (!host) return NX_OK;
    if (!pthread_equal(host->owner,pthread_self()) || host->polling) return NX_ERROR_STATE;
    while (host->buffers) nx_host_buffer_destroy(host->buffers);
    free(host); *address=NULL;
    return NX_OK;
}
