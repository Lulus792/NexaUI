#include "renderer.h"
#include "nexaui/software.h"
#ifdef NEXAUI_SETTINGS_HAVE_VULKAN
#include "nexaui/vulkan.h"
#endif
#include <stdio.h>
#include <stdlib.h>

struct settings_renderer {
    nx_window *window;
    nx_software_view *software;
#ifdef NEXAUI_SETTINGS_HAVE_VULKAN
    nx_vulkan_view *vulkan;
#endif
};

static nx_result open_software(settings_renderer *renderer, int fallback) {
    nx_result result=nx_software_view_open(renderer->window,&renderer->software);
    if (result==NX_OK)
        fprintf(stderr,"Settings renderer: software%s\n",fallback?" (Vulkan unsupported)":"");
    return result;
}

nx_result settings_renderer_open(nx_window *window, settings_renderer **out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out=NULL;
    if (!window) return NX_ERROR_ARGUMENT;
    settings_renderer *renderer=calloc(1,sizeof(*renderer));
    if (!renderer) return NX_ERROR_MEMORY;
    renderer->window=window;
    nx_result result;
#ifdef NEXAUI_SETTINGS_HAVE_VULKAN
    result=nx_vulkan_view_open(window,&renderer->vulkan);
    if (result==NX_ERROR_UNSUPPORTED) result=open_software(renderer,1);
    else if (result==NX_OK) fputs("Settings renderer: Vulkan\n",stderr);
#else
    result=open_software(renderer,0);
#endif
    if (result!=NX_OK) {free(renderer);return result;}
    *out=renderer;
    return NX_OK;
}

nx_result settings_renderer_render(settings_renderer *renderer, nx_pixel_size pixels,
    double scale, const nx_command *commands, size_t count, nx_color clear,
    settings_renderer_outcome *outcome) {
    if (outcome) *outcome=SETTINGS_RENDER_SUSPENDED;
    if (!renderer || !outcome) return NX_ERROR_ARGUMENT;
#ifdef NEXAUI_SETTINGS_HAVE_VULKAN
    if (renderer->vulkan) {
        nx_vulkan_view_outcome gpu=NX_VULKAN_VIEW_SUSPENDED;
        nx_result result=nx_vulkan_view_render(renderer->vulkan,pixels,scale,commands,count,clear,&gpu);
        if (result==NX_ERROR_UNSUPPORTED) {
            result=nx_vulkan_view_close(&renderer->vulkan);
            if (result!=NX_OK) return result;
            result=open_software(renderer,1);
            if (result!=NX_OK) return result;
        } else {
            if (result==NX_OK)
                *outcome=gpu==NX_VULKAN_VIEW_READY?SETTINGS_RENDER_READY:
                    gpu==NX_VULKAN_VIEW_DEVICE_LOST?SETTINGS_RENDER_DEVICE_LOST:SETTINGS_RENDER_SUSPENDED;
            return result;
        }
    }
#else
    (void)pixels; (void)scale;
#endif
    if (!renderer->software) return NX_ERROR_STATE;
    nx_software_view_outcome cpu=NX_SOFTWARE_VIEW_SUSPENDED;
    nx_result result=nx_software_view_render(renderer->software,commands,count,clear,&cpu);
    if (result==NX_OK)
        *outcome=cpu==NX_SOFTWARE_VIEW_READY?SETTINGS_RENDER_READY:
            cpu==NX_SOFTWARE_VIEW_BUSY?SETTINGS_RENDER_BUSY:SETTINGS_RENDER_SUSPENDED;
    return result;
}

nx_result settings_renderer_recover(settings_renderer *renderer) {
    if (!renderer) return NX_ERROR_ARGUMENT;
#ifdef NEXAUI_SETTINGS_HAVE_VULKAN
    if (renderer->vulkan) return nx_vulkan_view_recover(renderer->vulkan);
#endif
    return NX_ERROR_STATE;
}

nx_result settings_renderer_close(settings_renderer **address) {
    if (!address) return NX_ERROR_ARGUMENT;
    settings_renderer *renderer=*address;
    if (!renderer) return NX_OK;
    nx_result result=NX_OK;
#ifdef NEXAUI_SETTINGS_HAVE_VULKAN
    if (renderer->vulkan) result=nx_vulkan_view_close(&renderer->vulkan);
#endif
    if (result==NX_OK && renderer->software) result=nx_software_view_close(&renderer->software);
    if (result==NX_OK || (!renderer->software
#ifdef NEXAUI_SETTINGS_HAVE_VULKAN
        && !renderer->vulkan
#endif
        )) {free(renderer);*address=NULL;}
    return result;
}
