#define _POSIX_C_SOURCE 200809L
#include "renderer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC,&t)) return -1;
    return (double)t.tv_sec + (double)t.tv_nsec / 1.e9;
}
int main(int argc, char **argv) {
    int smoke = argc == 2 && !strcmp(argv[1],"--smoke");
    if (argc > 1 && !smoke) { fputs("usage: nexaui_native_clear [--smoke]\n",stderr); return 2; }
    nx_window *window = NULL; nx_renderer_driver renderer = {0};
    nx_window_desc desc = {"NexaUI Metal clear/present",640,400,0,{0}};
    nx_result result = nx_window_create(&desc,&window);
    if (result != NX_OK) { fprintf(stderr,"AppKit window: %d\n",result); return 1; }
    result = nx_metal_driver_open(window,&renderer);
    if (result != NX_OK) {
        nx_window_destroy(window);
        if (smoke && result == NX_ERROR_UNSUPPORTED && !getenv("NEXAUI_REQUIRE_METAL")) {
            puts("Metal device unavailable; native GPU smoke skipped."); return 77;
        }
        fprintf(stderr,"Metal create: %d\n",result); return 1;
    }
    double start = now(), duration = smoke ? .25 : 3.0;
    int okay = start >= 0; unsigned frames = 0;
    while (okay) {
        double current = now(); nx_window_info info;
        if (current < 0 || nx_window_poll(window,0) != NX_OK || nx_window_get_info(window,&info) != NX_OK) { okay = 0; break; }
        if (info.should_close || current - start >= duration) break;
        nx_render_target target = {(uint32_t)ceil((double)info.width*info.scale),
            (uint32_t)ceil((double)info.height*info.scale),info.scale};
        nx_render_outcome outcome;
        float phase = (float)((current-start)/duration);
        result = nx_renderer_run(&renderer,target,NULL,0,(nx_color){.04f+phase*.12f,.15f,.3f+phase*.3f,1},&outcome);
        if (result != NX_OK) { okay = 0; break; }
        if (outcome == NX_RENDER_READY) ++frames;
        else if (outcome == NX_RENDER_DEVICE_LOST) okay = nx_renderer_recover(&renderer) == NX_OK;
        else okay = nx_window_poll(window,4) == NX_OK;
    }
    result = nx_renderer_close(&renderer);
    for (unsigned i = 0; result == NX_ERROR_LIMIT && i < 3; ++i) result = nx_renderer_close(&renderer);
    if (result != NX_OK) { fprintf(stderr,"Retirement incomplete: %d\n",result); return 1; }
    nx_window_destroy(window);
    if (smoke && !frames) okay = 0;
    printf("Submitted %u Metal clear/present frames\n",frames);
    return okay ? 0 : 1;
}
