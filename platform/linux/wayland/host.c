#define _POSIX_C_SOURCE 200809L
#include "nexaui/wayland_host.h"
#include "pixels.h"
#include "../../../src/internal.h"
#include "../../../src/renderer_backend.h"

#include <fcntl.h>
#include <limits.h>
#include <math.h>
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
    if (!pthread_equal(host->owner,pthread_self())) return NX_ERROR_STATE;
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
    char path[]="/tmp/nexaui-host-XXXXXX";
    int fd=mkstemp(path);
    if (fd<0) { free(buffer); return NX_ERROR_PLATFORM; }
    if (unlink(path)!=0 || ftruncate(fd,(off_t)bytes)!=0) {
        close(fd); free(buffer); return NX_ERROR_PLATFORM;
    }
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

nx_result nx_wayland_shm_host_destroy(nx_wayland_shm_host **address) {
    if (!address) return NX_ERROR_ARGUMENT;
    nx_wayland_shm_host *host=*address;
    if (!host) return NX_OK;
    if (!pthread_equal(host->owner,pthread_self())) return NX_ERROR_STATE;
    while (host->buffers) nx_host_buffer_destroy(host->buffers);
    free(host); *address=NULL;
    return NX_OK;
}
