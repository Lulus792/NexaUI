#ifndef NEXAUI_METAL_TIMING_HISTORY_H
#define NEXAUI_METAL_TIMING_HISTORY_H
#include "renderer.h"

typedef struct nx_mtl_timing_history {
    nx_metal_gpu_timing entries[NX_METAL_TIMING_HISTORY];
    nx_metal_gpu_timing last_completed;
} nx_mtl_timing_history;

/* Private, fixed-storage journal. begin receives a validated committed frame;
   serials are monotonic and there are at most three pending submissions. */
void nx_mtl_timing_begin(nx_mtl_timing_history *history, const nx_metal_frame_profile *frame);
nx_result nx_mtl_timing_finish(nx_mtl_timing_history *history, uint64_t submission,
    int failed, double gpu_start, double gpu_end, double kernel_start, double kernel_end);
nx_result nx_mtl_timing_find(const nx_mtl_timing_history *history, uint64_t submission,
    nx_metal_gpu_timing *out_timing);
#endif
