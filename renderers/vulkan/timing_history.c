#include "timing_history.h"
#include <math.h>

void nx_vk_timing_begin(nx_vk_timing_history *h, const nx_vulkan_frame_profile *frame,
    uint32_t bits, double period, int readback) {
    nx_vulkan_gpu_timing value = {0};
    value.submission = frame->submission; value.frame_slot = frame->frame_slot;
    value.pixel_width = frame->pixel_width; value.pixel_height = frame->pixel_height;
    value.timestamp_valid_bits = bits; value.timestamp_period_ns = period;
    value.includes_readback = readback; value.state = NX_VK_TIMING_PENDING;
    h->entries[value.submission % NX_VULKAN_TIMING_HISTORY] = value;
}

static int decode(uint32_t bits, double period, uint64_t start, uint64_t end,
                  uint64_t observed_ns, double *milliseconds) {
    if (bits < 36u || bits > 64u || !isfinite(period) || period <= 0) return 0;
    double wrap_ns = ldexp(period, (int)bits);
    /* Host elapsed, sampled before Submit and after its fence, is a conservative
       upper bound, not a cross-clock subtraction. If a full device-counter wrap
       could fit, the modular delta cannot prove the number of wraps. */
    if (!isfinite(wrap_ns) || (double)observed_ns >= wrap_ns) return 0;
    uint64_t mask = bits == 64u ? UINT64_MAX : (UINT64_C(1) << bits)-1u;
    if ((start & ~mask) || (end & ~mask)) return 0;
    uint64_t delta = (end-start) & mask;
    double ns = (double)delta*period;
    /* Permit one tick of quantization at the two clock observations. */
    if (!isfinite(ns) || ns > (double)observed_ns+period) return 0;
    *milliseconds = ns/1.e6;
    return isfinite(*milliseconds);
}

nx_result nx_vk_timing_finish(nx_vk_timing_history *h, uint64_t submission,
    int failed, int available, uint64_t start, uint64_t end, uint64_t observed_ns) {
    if (!h || !submission) return NX_ERROR_ARGUMENT;
    nx_vulkan_gpu_timing *value = &h->entries[submission % NX_VULKAN_TIMING_HISTORY];
    if (value->submission != submission || value->state != NX_VK_TIMING_PENDING) return NX_ERROR_STATE;
    value->state = failed ? NX_VK_TIMING_FAILED : NX_VK_TIMING_UNAVAILABLE;
    value->observation_ns = observed_ns;
    if (!failed && available && decode(value->timestamp_valid_bits, value->timestamp_period_ns,
                                      start, end, observed_ns, &value->gpu_ms)) {
        value->start_tick = start; value->end_tick = end;
        value->state = NX_VK_TIMING_READY;
    }
    if (submission > h->last_completed.submission) h->last_completed = *value;
    return NX_OK;
}

nx_result nx_vk_timing_find(const nx_vk_timing_history *h, uint64_t submission, nx_vulkan_gpu_timing *out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out = (nx_vulkan_gpu_timing){0};
    if (!h || !submission) return NX_ERROR_ARGUMENT;
    const nx_vulkan_gpu_timing *value = &h->entries[submission % NX_VULKAN_TIMING_HISTORY];
    if (value->submission != submission) return NX_ERROR_STATE;
    *out = *value; return NX_OK;
}
