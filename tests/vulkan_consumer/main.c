#include <nexaui/nexaui.h>
#include <nexaui/vulkan.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) {fprintf(stderr,"consumer:%d: %s\n",__LINE__,#x); goto cleanup;} } while (0)
#ifdef NEXAUI_CONSUMER_PLUGIN
nx_result consumer_plugin_close_empty(void);
#endif
typedef struct wrong_args {nx_vulkan_view *view; nx_result result;} wrong_args;
static void *wrong_close(void *p) {
    wrong_args *a=p; a->result=nx_vulkan_view_close(&a->view); return NULL;
}
int main(void) {
    const char *backend=getenv("NEXAUI_PLATFORM");
    if (backend && !strcmp(backend,"wayland") && !getenv("WAYLAND_DISPLAY")) return 77;
    if (backend && !strcmp(backend,"x11") && !getenv("DISPLAY")) return 77;
    if (!backend && !getenv("WAYLAND_DISPLAY") && !getenv("DISPLAY")) return 77;
    nx_window *window=NULL; nx_context *ctx=NULL; nx_vulkan_view *view=NULL;
    int okay=0, opened=0; nx_vulkan_view_outcome outcome=NX_VULKAN_VIEW_READY;
#ifdef NEXAUI_CONSUMER_PLUGIN
    CHECK(consumer_plugin_close_empty()==NX_OK);
#endif
    nx_vulkan_view *invalid=(nx_vulkan_view *)(uintptr_t)1;
    CHECK(nx_vulkan_view_open(NULL,&invalid)==NX_ERROR_ARGUMENT && !invalid);
    CHECK(nx_vulkan_view_open(NULL,NULL)==NX_ERROR_ARGUMENT);
    CHECK(nx_vulkan_view_close(NULL)==NX_ERROR_ARGUMENT && nx_vulkan_view_close(&view)==NX_OK);
    nx_window_desc desc={"Public Vulkan consumer",320,200,0,{0}};
    CHECK(nx_window_create(&desc,&window)==NX_OK);
    CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    CHECK(nx_vulkan_view_open(window,&view)==NX_OK && view); opened=1;
    nx_vulkan_view *duplicate=NULL;
    CHECK(nx_vulkan_view_open(window,&duplicate)==NX_ERROR_STATE && !duplicate);
    pthread_t thread; wrong_args wrong={view,NX_OK};
    CHECK(pthread_create(&thread,NULL,wrong_close,&wrong)==0 && pthread_join(thread,NULL)==0);
    CHECK(wrong.result==NX_ERROR_STATE && wrong.view==view);
    CHECK(nx_vulkan_view_recover(view)==NX_ERROR_STATE);
    nx_pixel_size pixels={0}; nx_window_info info={0};
    CHECK(nx_window_poll(window,50)==NX_OK && nx_window_get_info(window,&info)==NX_OK &&
          nx_window_get_pixel_size(window,&pixels)==NX_OK);
    CHECK(nx_vulkan_view_render(view,pixels,info.scale,NULL,1,(nx_color){0,0,0,1},&outcome)==NX_ERROR_ARGUMENT &&
          outcome==NX_VULKAN_VIEW_SUSPENDED);
    CHECK(nx_vulkan_view_render(view,(nx_pixel_size){0,0},info.scale,NULL,0,(nx_color){0,0,0,1},&outcome)==NX_OK &&
          outcome==NX_VULKAN_VIEW_SUSPENDED);
    CHECK(nx_begin_frame(ctx,info.width,info.height,0)==NX_OK);
    CHECK(nx_fill_rect(ctx,(nx_rect){16,16,48,32},(nx_color){1,0,0,1})==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK);
    const nx_command *commands=NULL; size_t count=0;
    CHECK(nx_frame_commands(ctx,&commands,&count)==NX_OK && count>0);
    for(unsigned i=0;i<100;++i) {
        CHECK(nx_window_poll(window,20)==NX_OK && nx_window_get_info(window,&info)==NX_OK &&
              nx_window_get_pixel_size(window,&pixels)==NX_OK);
        CHECK(nx_vulkan_view_render(view,pixels,info.scale,commands,count,(nx_color){0,0,0,1},&outcome)==NX_OK);
        if(outcome==NX_VULKAN_VIEW_READY) break;
        CHECK(outcome==NX_VULKAN_VIEW_SUSPENDED);
    }
    CHECK(outcome==NX_VULKAN_VIEW_READY);
    printf("Public Vulkan consumer submitted a frame: logical %.0fx%.0f, pixels %ux%u, scale %.2f\n",
           info.width,info.height,pixels.width,pixels.height,info.scale);
    okay=1;
cleanup:
    if(opened) {
        nx_result r=NX_ERROR_LIMIT;
        for(unsigned i=0;i<8 && r==NX_ERROR_LIMIT;++i) {r=nx_vulkan_view_close(&view);if(r==NX_ERROR_LIMIT)(void)nx_window_poll(window,20);}
        if(r!=NX_OK || view) okay=0;
    }
    nx_context_destroy(ctx); nx_window_destroy(window); return okay?0:1;
}
