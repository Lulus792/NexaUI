#include "timing_history.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

int main(void) {
    nx_mtl_timing_history history = {0}; nx_metal_gpu_timing value;
    nx_metal_frame_profile frame = {.submission=1,.frame_slot=2,.pixel_width=800,.pixel_height=600};
    CHECK(nx_mtl_timing_find(NULL,1,&value) == NX_ERROR_ARGUMENT && !value.submission);
    CHECK(nx_mtl_timing_find(&history,0,&value) == NX_ERROR_ARGUMENT);
    CHECK(nx_mtl_timing_find(&history,1,NULL) == NX_ERROR_ARGUMENT);
    CHECK(nx_mtl_timing_find(&history,1,&value) == NX_ERROR_STATE);
    CHECK(nx_mtl_timing_finish(NULL,1,0,1,2,3,4) == NX_ERROR_ARGUMENT);
    CHECK(nx_mtl_timing_finish(&history,0,0,1,2,3,4) == NX_ERROR_ARGUMENT);
    nx_mtl_timing_begin(&history,&frame);
    CHECK(nx_mtl_timing_find(&history,1,&value) == NX_OK && value.state == NX_MTL_TIMING_PENDING);
    CHECK(value.pixel_width == 800 && value.pixel_height == 600 && value.frame_slot == 2);
    CHECK(!value.gpu_ms && !value.kernel_timing_valid && !history.last_completed.submission);
    CHECK(nx_mtl_timing_finish(&history,2,0,1,2,3,4) == NX_ERROR_STATE);
    CHECK(nx_mtl_timing_finish(&history,1,0,1,1.125,2.25,2.5) == NX_OK);
    CHECK(nx_mtl_timing_find(&history,1,&value) == NX_OK && value.state == NX_MTL_TIMING_READY);
    CHECK(value.gpu_ms == 125 && value.kernel_ms == 250 && value.kernel_timing_valid);
    CHECK(value.gpu_start_seconds == 1 && value.gpu_end_seconds == 1.125);
    CHECK(nx_mtl_timing_finish(&history,1,1,0,0,0,0) == NX_ERROR_STATE);
    CHECK(history.last_completed.state == NX_MTL_TIMING_READY && history.last_completed.submission == 1);

    const double bad[][2] = {{0,0},{0,1},{-1,2},{2,1},{NAN,3},{3,NAN},{INFINITY,INFINITY},{1,INFINITY},{1,DBL_MAX}};
    for (unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
        frame.submission=i+2; nx_mtl_timing_begin(&history,&frame);
        CHECK(nx_mtl_timing_finish(&history,frame.submission,0,bad[i][0],bad[i][1],4,4) == NX_OK);
        CHECK(nx_mtl_timing_find(&history,frame.submission,&value) == NX_OK && value.state == NX_MTL_TIMING_UNAVAILABLE);
        CHECK(!value.gpu_start_seconds && !value.gpu_end_seconds && !value.gpu_ms);
        CHECK(value.kernel_timing_valid && !value.kernel_ms); /* Valid equal timestamps. */
    }
    frame.submission=20; nx_mtl_timing_begin(&history,&frame);
    CHECK(nx_mtl_timing_finish(&history,20,1,1,2,3,4) == NX_OK);
    CHECK(history.last_completed.state == NX_MTL_TIMING_FAILED && !history.last_completed.gpu_ms && !history.last_completed.kernel_timing_valid);
    frame.submission=21; nx_mtl_timing_begin(&history,&frame);
    CHECK(nx_mtl_timing_finish(&history,21,0,4,4,NAN,7) == NX_OK);
    CHECK(history.last_completed.state == NX_MTL_TIMING_READY && !history.last_completed.gpu_ms && !history.last_completed.kernel_timing_valid);

    /* Delayed/out-of-order completion never relabels the latest result. */
    frame.submission=22; nx_mtl_timing_begin(&history,&frame);
    frame.submission=23; nx_mtl_timing_begin(&history,&frame);
    CHECK(nx_mtl_timing_finish(&history,23,0,7,8,9,10) == NX_OK);
    CHECK(nx_mtl_timing_finish(&history,22,0,4,5,6,7) == NX_OK);
    CHECK(history.last_completed.submission == 23);
    /* Two full journal wraps, with three independently pending slot identities. */
    history=(nx_mtl_timing_history){0};
    for (uint64_t base=1;base<=130;base+=3) {
        for (uint32_t slot=0;slot<3;++slot) {
            frame.submission=base+slot; frame.frame_slot=slot; nx_mtl_timing_begin(&history,&frame);
        }
        for (uint32_t slot=0;slot<3;++slot) {
            CHECK(nx_mtl_timing_finish(&history,base+slot,0,1,2,0,0) == NX_OK);
            CHECK(nx_mtl_timing_find(&history,base+slot,&value) == NX_OK && value.frame_slot == slot && value.state == NX_MTL_TIMING_READY);
        }
    }
    CHECK(nx_mtl_timing_find(&history,1,&value) == NX_ERROR_STATE && !value.submission);
    for (uint64_t id=69;id<=132;++id) CHECK(nx_mtl_timing_find(&history,id,&value) == NX_OK && value.submission == id);
    nx_metal_gpu_timing last=history.last_completed;
    frame.submission=196; nx_mtl_timing_begin(&history,&frame); /* Same journal index as 132. */
    CHECK(nx_mtl_timing_finish(&history,132,0,1,2,3,4) == NX_ERROR_STATE);
    CHECK(nx_mtl_timing_find(&history,196,&value) == NX_OK && value.state == NX_MTL_TIMING_PENDING);
    CHECK(!memcmp(&last,&history.last_completed,sizeof(last)));
    frame.submission=UINT64_MAX; nx_mtl_timing_begin(&history,&frame);
    CHECK(nx_mtl_timing_finish(&history,UINT64_MAX,0,1,1.125,0,0) == NX_OK);
    CHECK(history.last_completed.submission == UINT64_MAX && history.last_completed.gpu_ms == 125);
    puts("Metal timing journal: IDs, pending/error/unavailable states, finite pairs, zero duration and bounded retention passed.");
    return 0;
}
