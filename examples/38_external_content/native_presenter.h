#ifndef NEXAUI_CONTENT_NATIVE_PRESENTER_H
#define NEXAUI_CONTENT_NATIVE_PRESENTER_H

#include <nexaui/software.h>
#ifdef CONTENT_NATIVE_VULKAN
#include <nexaui/vulkan.h>
#endif
#include <stdio.h>

typedef struct native_presenter {
    nx_software_view *software;
#ifdef CONTENT_NATIVE_VULKAN
    nx_vulkan_view *vulkan;
#endif
} native_presenter;

static nx_result native_presenter_open(native_presenter *presenter, nx_window *window, int use_vulkan) {
    nx_result result;
    if (use_vulkan) {
#ifdef CONTENT_NATIVE_VULKAN
        result=nx_vulkan_view_open(window,&presenter->vulkan);
#else
        result=NX_ERROR_UNSUPPORTED;
#endif
    } else result=nx_software_view_open(window,&presenter->software);
    if (result==NX_OK) {
        printf("CONTENT_RENDERER selected=%s\n",use_vulkan?"vulkan":"software");
        fflush(stdout);
    } else fprintf(stderr,"Content renderer %s could not open: %d\n",use_vulkan?"vulkan":"software",(int)result);
    return result;
}

static nx_result native_presenter_render(native_presenter *presenter, nx_window *window, float scale,
    const nx_command *commands, size_t count, int *ready) {
    const nx_color clear={1,1,1,1};
    *ready=0;
#ifdef CONTENT_NATIVE_VULKAN
    if (presenter->vulkan) {
        nx_pixel_size pixels;
        nx_result result=nx_window_get_pixel_size(window,&pixels);
        if (result!=NX_OK) return result;
        nx_vulkan_view_outcome outcome;
        result=nx_vulkan_view_render(presenter->vulkan,pixels,scale,commands,count,clear,&outcome);
        if (result!=NX_OK) return result;
        if (outcome==NX_VULKAN_VIEW_DEVICE_LOST) return nx_vulkan_view_recover(presenter->vulkan);
        *ready=outcome==NX_VULKAN_VIEW_READY;
        return NX_OK;
    }
#else
    (void)window;
    (void)scale;
#endif
    nx_software_view_outcome outcome;
    nx_result result=nx_software_view_render(presenter->software,commands,count,clear,&outcome);
    if (result==NX_OK) *ready=outcome==NX_SOFTWARE_VIEW_READY;
    return result;
}

static nx_result native_presenter_close(native_presenter *presenter, nx_window *window) {
#ifdef CONTENT_NATIVE_VULKAN
    if (presenter->vulkan) {
        nx_result result=nx_vulkan_view_close(&presenter->vulkan);
        for (unsigned attempt=0; result==NX_ERROR_LIMIT && attempt<4; ++attempt) {
            if (nx_window_poll(window,16)!=NX_OK) break;
            result=nx_vulkan_view_close(&presenter->vulkan);
        }
        if (result==NX_OK) puts("CONTENT_RENDERER closed=vulkan");
        else fprintf(stderr,"Vulkan retirement incomplete: %d\n",(int)result);
        return result;
    }
#else
    (void)window;
#endif
    return nx_software_view_close(&presenter->software);
}

#endif
