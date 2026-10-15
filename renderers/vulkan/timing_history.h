#ifndef NEXAUI_VULKAN_TIMING_HISTORY_H
#define NEXAUI_VULKAN_TIMING_HISTORY_H
#include "renderer.h"

typedef struct nx_vk_timing_history {
    nx_vulkan_gpu_timing entries[NX_VULKAN_TIMING_HISTORY];
    nx_vulkan_gpu_timing last_completed;
} nx_vk_timing_history;

/* Commit-validated IDs, at most three pending slots. No allocation or GPU API. */
void nx_vk_timing_begin(nx_vk_timing_history *history, const nx_vulkan_frame_profile *frame,
    uint32_t valid_bits, double period_ns, int readback);
nx_result nx_vk_timing_finish(nx_vk_timing_history *history, uint64_t submission,
    int failed, int available, uint64_t start, uint64_t end, uint64_t observed_ns);
nx_result nx_vk_timing_find(const nx_vk_timing_history *history, uint64_t submission,
    nx_vulkan_gpu_timing *out_timing);
#endif
