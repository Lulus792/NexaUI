#include "nexaui/vulkan.h"
#include "renderer.h"
#include <pthread.h>
#include <stdlib.h>

struct nx_vulkan_view {
    nx_renderer_driver driver;
    pthread_t owner;
};

nx_result nx_vulkan_view_open(nx_window *window, nx_vulkan_view **out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out=NULL;
    if (!window) return NX_ERROR_ARGUMENT;
    nx_vulkan_view *view=calloc(1,sizeof(*view));
    if (!view) return NX_ERROR_MEMORY;
    view->owner=pthread_self();
    nx_result result=nx_vulkan_driver_open(window,&view->driver);
    if (result!=NX_OK) {free(view);return result;}
    *out=view; return NX_OK;
}

nx_result nx_vulkan_view_render(nx_vulkan_view *view, nx_pixel_size pixels,
    double scale, const nx_command *commands, size_t count, nx_color clear,
    nx_vulkan_view_outcome *outcome) {
    if (outcome) *outcome=NX_VULKAN_VIEW_SUSPENDED;
    if (!view || !outcome) return NX_ERROR_ARGUMENT;
    if (!pthread_equal(view->owner,pthread_self())) return NX_ERROR_STATE;
    nx_render_outcome internal=NX_RENDER_SUSPENDED;
    nx_result result=nx_renderer_run(&view->driver,
        (nx_render_target){pixels.width,pixels.height,scale},commands,count,clear,&internal);
    if (result==NX_OK) {
        if (internal==NX_RENDER_READY) *outcome=NX_VULKAN_VIEW_READY;
        else if (internal==NX_RENDER_DEVICE_LOST) *outcome=NX_VULKAN_VIEW_DEVICE_LOST;
        else *outcome=NX_VULKAN_VIEW_SUSPENDED;
    }
    return result;
}

nx_result nx_vulkan_view_recover(nx_vulkan_view *view) {
    if (!view) return NX_ERROR_ARGUMENT;
    if (!pthread_equal(view->owner,pthread_self())) return NX_ERROR_STATE;
    return nx_renderer_recover(&view->driver);
}

nx_result nx_vulkan_view_close(nx_vulkan_view **address) {
    if (!address) return NX_ERROR_ARGUMENT;
    nx_vulkan_view *view=*address;
    if (!view) return NX_OK;
    if (!pthread_equal(view->owner,pthread_self())) return NX_ERROR_STATE;
    nx_result result=nx_renderer_close(&view->driver);
    if (result==NX_OK) {free(view);*address=NULL;}
    return result;
}
