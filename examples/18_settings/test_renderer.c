#include "renderer.h"
#include "nexaui/software.h"
#include "nexaui/vulkan.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
struct nx_vulkan_view { int unused; };
struct nx_software_view { int unused; };
static struct nx_vulkan_view gpu;
static struct nx_software_view cpu;
static nx_result gpu_open, gpu_render, gpu_close, cpu_open, cpu_render, cpu_close;
static int gpu_opens, gpu_closes, cpu_opens, cpu_closes, cpu_renders;
static nx_vulkan_view_outcome gpu_outcome;
static nx_software_view_outcome cpu_outcome;

nx_result nx_vulkan_view_open(nx_window *window, nx_vulkan_view **out) {
    CHECK(window && out); ++gpu_opens; *out=gpu_open==NX_OK?&gpu:NULL; return gpu_open;
}
nx_result nx_vulkan_view_render(nx_vulkan_view *view, nx_pixel_size pixels, double scale,
    const nx_command *commands, size_t count, nx_color clear, nx_vulkan_view_outcome *out) {
    (void)pixels;(void)scale;(void)commands;(void)count;(void)clear;
    CHECK(view==&gpu && out); *out=gpu_outcome; return gpu_render;
}
nx_result nx_vulkan_view_recover(nx_vulkan_view *view) { CHECK(view==&gpu); return NX_OK; }
nx_result nx_vulkan_view_close(nx_vulkan_view **view) {
    CHECK(view && *view==&gpu); ++gpu_closes; if(gpu_close==NX_OK)*view=NULL; return gpu_close;
}
nx_result nx_software_view_open(nx_window *window, nx_software_view **out) {
    CHECK(window && out); ++cpu_opens; *out=cpu_open==NX_OK?&cpu:NULL; return cpu_open;
}
nx_result nx_software_view_render(nx_software_view *view, const nx_command *commands,
    size_t count, nx_color clear, nx_software_view_outcome *out) {
    (void)commands;(void)count;(void)clear; CHECK(view==&cpu && out); ++cpu_renders;
    *out=cpu_outcome; return cpu_render;
}
nx_result nx_software_view_close(nx_software_view **view) {
    CHECK(view && *view==&cpu); ++cpu_closes; if(cpu_close==NX_OK)*view=NULL; return cpu_close;
}
static void reset(void) {
    gpu_open=gpu_render=gpu_close=cpu_open=cpu_render=cpu_close=NX_OK;
    gpu_opens=gpu_closes=cpu_opens=cpu_closes=cpu_renders=0;
    gpu_outcome=NX_VULKAN_VIEW_READY; cpu_outcome=NX_SOFTWARE_VIEW_READY;
}
static nx_result render(settings_renderer *renderer, settings_renderer_outcome *out) {
    return settings_renderer_render(renderer,(nx_pixel_size){20,20},1,NULL,0,(nx_color){0},out);
}
int main(void) {
    nx_window *window=(nx_window *)&gpu;
    settings_renderer *renderer=NULL; settings_renderer_outcome outcome;
    reset();
    CHECK(settings_renderer_open(window,&renderer)==NX_OK && gpu_opens==1 && cpu_opens==0);
    CHECK(render(renderer,&outcome)==NX_OK && outcome==SETTINGS_RENDER_READY);
    CHECK(settings_renderer_close(&renderer)==NX_OK && !renderer && gpu_closes==1);

    reset(); gpu_open=NX_ERROR_UNSUPPORTED;
    CHECK(settings_renderer_open(window,&renderer)==NX_OK && cpu_opens==1);
    cpu_outcome=NX_SOFTWARE_VIEW_BUSY;
    CHECK(render(renderer,&outcome)==NX_OK && outcome==SETTINGS_RENDER_BUSY);
    CHECK(settings_renderer_close(&renderer)==NX_OK && !renderer && cpu_closes==1);

    reset(); gpu_open=NX_ERROR_MEMORY;
    CHECK(settings_renderer_open(window,&renderer)==NX_ERROR_MEMORY && !renderer && cpu_opens==0);
    reset(); gpu_open=NX_ERROR_UNSUPPORTED; cpu_open=NX_ERROR_PLATFORM;
    CHECK(settings_renderer_open(window,&renderer)==NX_ERROR_PLATFORM && !renderer && cpu_opens==1);

    reset(); CHECK(settings_renderer_open(window,&renderer)==NX_OK);
    gpu_render=NX_ERROR_UNSUPPORTED; gpu_close=NX_ERROR_LIMIT;
    CHECK(render(renderer,&outcome)==NX_ERROR_LIMIT && cpu_opens==0 && gpu_closes==1);
    gpu_close=NX_OK;
    CHECK(settings_renderer_close(&renderer)==NX_OK && !renderer && gpu_closes==2);

    reset(); CHECK(settings_renderer_open(window,&renderer)==NX_OK);
    gpu_render=NX_ERROR_UNSUPPORTED;
    CHECK(render(renderer,&outcome)==NX_OK && outcome==SETTINGS_RENDER_READY &&
          gpu_closes==1 && cpu_opens==1 && cpu_renders==1);
    CHECK(settings_renderer_close(&renderer)==NX_OK && !renderer && cpu_closes==1);

    reset(); CHECK(settings_renderer_open(window,&renderer)==NX_OK);
    gpu_render=NX_ERROR_UNSUPPORTED; cpu_open=NX_ERROR_PLATFORM;
    CHECK(render(renderer,&outcome)==NX_ERROR_PLATFORM && gpu_closes==1 && cpu_opens==1);
    CHECK(settings_renderer_close(&renderer)==NX_OK && !renderer);

    reset(); CHECK(settings_renderer_open(window,&renderer)==NX_OK);
    gpu_render=NX_ERROR_MEMORY;
    CHECK(render(renderer,&outcome)==NX_ERROR_MEMORY && cpu_opens==0 && gpu_closes==0);
    CHECK(settings_renderer_close(&renderer)==NX_OK && !renderer);
    reset(); CHECK(settings_renderer_open(window,&renderer)==NX_OK);
    gpu_outcome=NX_VULKAN_VIEW_DEVICE_LOST;
    CHECK(render(renderer,&outcome)==NX_OK && outcome==SETTINGS_RENDER_DEVICE_LOST && cpu_opens==0);
    CHECK(settings_renderer_recover(renderer)==NX_OK);
    CHECK(settings_renderer_close(&renderer)==NX_OK && !renderer);
    puts("Renderer selection, unsupported fallback and failure/retirement paths passed.");
    return 0;
}
