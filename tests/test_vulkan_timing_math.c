#include "timing_history.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

int main(void) {
    nx_vk_timing_history history={0}; nx_vulkan_gpu_timing value;
    nx_vulkan_frame_profile frame={.submission=1,.frame_slot=2,.pixel_width=800,.pixel_height=600};
    CHECK(nx_vk_timing_find(NULL,1,&value)==NX_ERROR_ARGUMENT && !value.submission);
    CHECK(nx_vk_timing_find(&history,0,&value)==NX_ERROR_ARGUMENT);
    CHECK(nx_vk_timing_find(&history,1,NULL)==NX_ERROR_ARGUMENT);
    CHECK(nx_vk_timing_find(&history,1,&value)==NX_ERROR_STATE);
    CHECK(nx_vk_timing_finish(NULL,1,0,1,0,1,1)==NX_ERROR_ARGUMENT);
    nx_vk_timing_begin(&history,&frame,64,2,1);
    CHECK(nx_vk_timing_find(&history,1,&value)==NX_OK && value.state==NX_VK_TIMING_PENDING);
    CHECK(value.frame_slot==2 && value.pixel_width==800 && value.pixel_height==600 && value.includes_readback);
    CHECK(nx_vk_timing_finish(&history,1,0,1,0,500000,1000001)==NX_OK);
    CHECK(history.last_completed.state==NX_VK_TIMING_READY && history.last_completed.gpu_ms==1);
    CHECK(nx_vk_timing_finish(&history,1,0,1,0,1,1)==NX_ERROR_STATE);
    /* Zero ticks and equal timestamps are valid when availability says so. */
    frame.submission=2; nx_vk_timing_begin(&history,&frame,64,1,0);
    CHECK(nx_vk_timing_finish(&history,2,0,1,0,0,0)==NX_OK);
    CHECK(history.last_completed.state==NX_VK_TIMING_READY && !history.last_completed.gpu_ms);
    frame.submission=3; nx_vk_timing_begin(&history,&frame,36,1,0);
    CHECK(nx_vk_timing_finish(&history,3,0,1,(UINT64_C(1)<<36)-5,6,11)==NX_OK);
    CHECK(history.last_completed.state==NX_VK_TIMING_READY && history.last_completed.gpu_ms==11./1.e6);
    frame.submission=4; nx_vk_timing_begin(&history,&frame,64,1,0);
    CHECK(nx_vk_timing_finish(&history,4,0,1,UINT64_MAX-4,6,11)==NX_OK);
    CHECK(history.last_completed.state==NX_VK_TIMING_READY && history.last_completed.gpu_ms==11./1.e6);
    struct invalid { unsigned bits; double period; uint64_t start,end,observed; int available; } cases[]={
        {0,1,0,1,10,1},{35,1,0,1,10,1},{65,1,0,1,10,1},
        {64,NAN,0,1,10,1},{64,INFINITY,0,1,10,1},{64,0,0,1,10,1},
        {64,-1,0,1,10,1},{64,DBL_MAX,0,1,10,1},{64,1,0,1,10,0},
        {36,1,0,0,UINT64_C(1)<<36,1}, /* Ambiguous full wrap, even for equal ticks. */
        {36,1,UINT64_C(1)<<36,1,10,1}, /* Bits beyond the advertised counter. */
        {64,1,10,9,10,1}, /* Reversed pair is not a plausible wrap in this host bound. */
        {64,2,0,1000,10,1}
    };
    for (size_t i=0;i<sizeof(cases)/sizeof(*cases);++i) {
        frame.submission=10+i; struct invalid v=cases[i];
        nx_vk_timing_begin(&history,&frame,v.bits,v.period,0);
        CHECK(nx_vk_timing_finish(&history,frame.submission,0,v.available,v.start,v.end,v.observed)==NX_OK);
        CHECK(history.last_completed.state==NX_VK_TIMING_UNAVAILABLE && !history.last_completed.gpu_ms &&
              !history.last_completed.start_tick && !history.last_completed.end_tick);
    }
    frame.submission=30; nx_vk_timing_begin(&history,&frame,64,1,0);
    CHECK(nx_vk_timing_finish(&history,30,1,1,1,2,10)==NX_OK && history.last_completed.state==NX_VK_TIMING_FAILED);
    frame.submission=31; nx_vk_timing_begin(&history,&frame,64,1,0);
    frame.submission=32; nx_vk_timing_begin(&history,&frame,64,1,0);
    CHECK(nx_vk_timing_finish(&history,32,0,1,1,2,10)==NX_OK);
    CHECK(nx_vk_timing_finish(&history,31,0,1,1,2,10)==NX_OK && history.last_completed.submission==32);
    history=(nx_vk_timing_history){0};
    for (uint64_t base=1;base<=130;base+=3) {
        for (unsigned slot=0;slot<3;++slot) {
            frame.submission=base+slot; frame.frame_slot=slot;
            nx_vk_timing_begin(&history,&frame,64,1,0);
        }
        for (unsigned slot=0;slot<3;++slot) {
            CHECK(nx_vk_timing_finish(&history,base+slot,0,1,0,100,1000)==NX_OK);
            CHECK(nx_vk_timing_find(&history,base+slot,&value)==NX_OK && value.frame_slot==slot && value.state==NX_VK_TIMING_READY);
        }
    }
    CHECK(nx_vk_timing_find(&history,1,&value)==NX_ERROR_STATE && !value.submission);
    for (uint64_t id=69;id<=132;++id) CHECK(nx_vk_timing_find(&history,id,&value)==NX_OK && value.submission==id);
    nx_vulkan_gpu_timing last=history.last_completed;
    frame.submission=196; nx_vk_timing_begin(&history,&frame,64,1,0);
    CHECK(nx_vk_timing_finish(&history,132,0,1,0,100,1000)==NX_ERROR_STATE);
    CHECK(nx_vk_timing_find(&history,196,&value)==NX_OK && value.state==NX_VK_TIMING_PENDING);
    CHECK(!memcmp(&last,&history.last_completed,sizeof(last)));
    frame.submission=UINT64_MAX; nx_vk_timing_begin(&history,&frame,64,1,0);
    CHECK(nx_vk_timing_finish(&history,UINT64_MAX,0,1,0,100,1000)==NX_OK);
    CHECK(history.last_completed.submission==UINT64_MAX && history.last_completed.state==NX_VK_TIMING_READY);
    puts("Vulkan timing math: available zero, 36/64-bit wrap, ambiguity, finite conversion, failure and 64-ID retention passed.");
    return 0;
}
