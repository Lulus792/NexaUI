#include "timing_history.h"
#include <math.h>

void nx_mtl_timing_begin(nx_mtl_timing_history *h, const nx_metal_frame_profile *frame) {
    nx_metal_gpu_timing value = {0};
    value.submission = frame->submission; value.frame_slot = frame->frame_slot;
    value.pixel_width = frame->pixel_width; value.pixel_height = frame->pixel_height;
    value.state = NX_MTL_TIMING_PENDING;
    h->entries[value.submission % NX_METAL_TIMING_HISTORY] = value;
}
static int valid_pair(double start, double end, double *milliseconds) {
    if (!isfinite(start) || !isfinite(end) || start <= 0 || end < start) return 0;
    double elapsed = (end-start)*1000.;
    if (!isfinite(elapsed)) return 0;
    *milliseconds = elapsed; return 1;
}
nx_result nx_mtl_timing_finish(nx_mtl_timing_history *h, uint64_t submission,
    int failed, double gs, double ge, double ks, double ke) {
    if (!h || !submission) return NX_ERROR_ARGUMENT;
    nx_metal_gpu_timing *value = &h->entries[submission % NX_METAL_TIMING_HISTORY];
    if (value->submission != submission || value->state != NX_MTL_TIMING_PENDING) return NX_ERROR_STATE;
    value->state = failed ? NX_MTL_TIMING_FAILED : NX_MTL_TIMING_UNAVAILABLE;
    if (!failed) {
        if (valid_pair(gs,ge,&value->gpu_ms)) {
            value->gpu_start_seconds = gs; value->gpu_end_seconds = ge; value->state = NX_MTL_TIMING_READY;
        }
        if (valid_pair(ks,ke,&value->kernel_ms)) {
            value->kernel_start_seconds = ks; value->kernel_end_seconds = ke; value->kernel_timing_valid = 1;
        }
    }
    if (submission > h->last_completed.submission) h->last_completed = *value;
    return NX_OK;
}
nx_result nx_mtl_timing_find(const nx_mtl_timing_history *h, uint64_t submission, nx_metal_gpu_timing *out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out = (nx_metal_gpu_timing){0};
    if (!h || !submission) return NX_ERROR_ARGUMENT;
    const nx_metal_gpu_timing *value = &h->entries[submission % NX_METAL_TIMING_HISTORY];
    if (value->submission != submission) return NX_ERROR_STATE;
    *out = *value; return NX_OK;
}
