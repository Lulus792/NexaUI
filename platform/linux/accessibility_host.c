#include "nexaui/linux_accessibility_host.h"
#include "accessibility.h"
#include <math.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>

struct nx_linux_accessibility_host {
    pthread_t thread;
    nx_linux_ax *bridge;
    nx_linux_ax_geometry geometry;
    uint64_t revision;
    int publishing,focus_requested;
};
static void *host_allocate(void *user,size_t size) {(void)user;return malloc(size);}
static void host_deallocate(void *user,void *p,size_t size) {(void)user;(void)size;free(p);}
static int on_thread(const nx_linux_accessibility_host *host) {
    return host && pthread_equal(pthread_self(),host->thread);
}
static void host_geometry(void *object,nx_linux_ax_geometry *out) {
    *out=((nx_linux_accessibility_host *)object)->geometry;
}
static int host_focus(void *object) {
    ((nx_linux_accessibility_host *)object)->focus_requested=1;return 1;
}
static uint64_t host_revision(void *object) {return ((nx_linux_accessibility_host *)object)->revision;}
static int host_current(void *object,uint64_t revision) {
    nx_linux_accessibility_host *host=object;
    return host->publishing==1 && host->revision==revision;
}
nx_result nx_linux_accessibility_host_create(nx_linux_accessibility_host **out) {
    if(!out)return NX_ERROR_ARGUMENT;
    *out=NULL;
    nx_linux_accessibility_host *host=calloc(1,sizeof(*host));if(!host)return NX_ERROR_MEMORY;
    host->thread=pthread_self();*out=host;return NX_OK;
}
nx_result nx_linux_accessibility_host_publish(nx_linux_accessibility_host *host,
    const nx_context *context,const nx_linux_accessibility_geometry *g) {
    if(!host || !context || !g)return NX_ERROR_ARGUMENT;
    if(!on_thread(host) || host->publishing)return NX_ERROR_STATE;
    if(!isfinite(g->x) || !isfinite(g->y) || !isfinite(g->width) || !isfinite(g->height) ||
       g->width<0 || g->height<0 || g->width>INT32_MAX || g->height>INT32_MAX ||
       (g->screen_position && (g->x<INT32_MIN || g->x>INT32_MAX || g->y<INT32_MIN || g->y>INT32_MAX)) ||
       (g->screen_position!=0 && g->screen_position!=1) ||
       (g->visible!=0 && g->visible!=1) || (g->focused!=0 && g->focused!=1))return NX_ERROR_ARGUMENT;
    if(host->revision==UINT64_MAX)return NX_ERROR_LIMIT;
    nx_linux_ax_geometry previous=host->geometry;
    host->geometry=(nx_linux_ax_geometry){g->screen_position?g->x:0,g->screen_position?g->y:0,
        g->width,g->height,g->screen_position,g->visible,g->focused};
    nx_linux_ax_owner owner={host,host_geometry,host_focus,host_revision,host_current};
    nx_allocator allocator={NULL,host_allocate,host_deallocate};
    ++host->revision;host->publishing=1;
    nx_result result=nx_linux_ax_publish_owner(&host->bridge,owner,allocator,context);
    host->publishing=0;
    if(result!=NX_OK && result!=NX_ERROR_UNSUPPORTED)host->geometry=previous;
    return result;
}
nx_result nx_linux_accessibility_host_prepare(nx_linux_accessibility_host *host,
    int *fd,int *writable,uint32_t *timeout_ms) {
    if(!fd || !writable || !timeout_ms)return NX_ERROR_ARGUMENT;
    *fd=-1;*writable=0;
    if(!host)return NX_ERROR_ARGUMENT;
    if(!on_thread(host) || host->publishing)return NX_ERROR_STATE;
    if(!host->bridge)return NX_ERROR_UNSUPPORTED;
    host->publishing=2;
    if(nx_linux_ax_pump(host->bridge))*timeout_ms=0;
    *timeout_ms=nx_linux_ax_timeout(host->bridge,*timeout_ms);
    *fd=nx_linux_ax_fd(host->bridge,writable);host->publishing=0;return NX_OK;
}
nx_result nx_linux_accessibility_host_route(nx_linux_accessibility_host *host,
    nx_context *context,size_t *routed,int *focus_requested) {
    if(!routed || !focus_requested)return NX_ERROR_ARGUMENT;
    *routed=0;*focus_requested=0;
    if(!host || !context)return NX_ERROR_ARGUMENT;
    if(!on_thread(host) || host->publishing)return NX_ERROR_STATE;
    if(!host->bridge)return NX_ERROR_UNSUPPORTED;
    host->publishing=2;
    nx_result result=nx_linux_ax_route(host->bridge,context,routed);
    *focus_requested=host->focus_requested;host->focus_requested=0;host->publishing=0;return result;
}
nx_result nx_linux_accessibility_host_clear(nx_linux_accessibility_host *host) {
    if(!host)return NX_ERROR_ARGUMENT;
    if(!on_thread(host) || host->publishing)return NX_ERROR_STATE;
    nx_linux_ax *bridge=host->bridge;host->bridge=NULL;host->focus_requested=0;
    nx_linux_ax_destroy(bridge);return NX_OK;
}
nx_result nx_linux_accessibility_host_destroy(nx_linux_accessibility_host **address) {
    if(!address)return NX_ERROR_ARGUMENT;
    nx_linux_accessibility_host *host=*address;if(!host)return NX_OK;
    nx_result result=nx_linux_accessibility_host_clear(host);if(result!=NX_OK)return result;
    *address=NULL;free(host);return NX_OK;
}
