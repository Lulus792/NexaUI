#ifndef NEXAUI_METAL_RENDERER_H
#define NEXAUI_METAL_RENDERER_H

#include "nexaui/nexaui.h"
#include "renderer_backend.h"

typedef struct nx_metal_renderer nx_metal_renderer;
#define NX_METAL_FRAME_SLOTS 3u
#define NX_METAL_TIMING_HISTORY 64u
/* CPU-side submission measurements, published only after commit. Copy byte
   counts are one-way payloads, not estimates of physical GPU memory traffic. */
typedef struct nx_metal_frame_profile {
    uint32_t pixel_width, pixel_height;
    double draw_scale;
    uint64_t submission, input_commands, draw_records, draw_calls, render_passes;
    uint64_t bound_passes, fixed_state_bindings;
    uint64_t history_copies, history_copy_bytes, bitmap_upload_bytes;
    uint64_t draw_argument_bytes, present_copy_bytes;
    uint64_t record_upload_bytes, record_buffer_bindings;
    /* prepare: submit-local validation/pipeline/atlas preparation; encode:
       draw/upload/history encoding; acquire: layer flush + nextDrawable;
       commit: present-copy encoding and asynchronous commit, not GPU time. */
    double prepare_ms, encode_ms, acquire_ms, commit_ms;
    int cpu_timing_valid;
    int per_draw_bindings; /* Test baseline; production always uses per-pass state. */
    int batched;
    uint32_t frame_slot, frame_limit, pending_frames;
} nx_metal_frame_profile;

typedef enum nx_metal_timing_state {
    NX_MTL_TIMING_PENDING = 1, /* Committed, not yet collected at retirement. */
    NX_MTL_TIMING_READY,
    NX_MTL_TIMING_UNAVAILABLE, /* Successful work, absent/invalid GPU timestamps. */
    NX_MTL_TIMING_FAILED
} nx_metal_timing_state;
typedef struct nx_metal_gpu_timing {
    uint64_t submission;
    uint32_t frame_slot, pixel_width, pixel_height;
    nx_metal_timing_state state;
    /* Command-buffer GPU elapsed time, not CPU time or display latency.
       Absolute seconds use Metal's clock; never subtract the CPU profile clock.
       Invalid pairs remain zero, distinguished by state/kernel_timing_valid. */
    double gpu_start_seconds, gpu_end_seconds, gpu_ms;
    double kernel_start_seconds, kernel_end_seconds, kernel_ms;
    int kernel_timing_valid; /* CPU driver scheduling, separate from GPU work. */
} nx_metal_gpu_timing;

typedef struct nx_metal_slot_info {
    uint64_t submission, record_capacity_bytes, bitmap_capacity_bytes;
    uint32_t atlas_width, atlas_height;
    int pending, owns_readback;
} nx_metal_slot_info;
typedef struct nx_metal_renderer_info {
    char device_name[256];
    uint32_t pixel_width, pixel_height;
    uint64_t submitted_frames, completed_frames;
    int pending, pending_readback, closing, broken;
    uint32_t atlas_width, atlas_height, unique_bitmaps;
    uint64_t bitmap_capacity_bytes, atlas_allocations, bitmap_upload_allocations;
    uint64_t record_capacity_bytes, record_upload_allocations;
    /* Capacity fields above sum all slots; atlas dimensions describe the last
       committed slot. Pending counts retained submissions, which may already
       be GPU-terminal but have not yet been inspected/retired on this thread. */
    uint32_t frame_limit, pending_frames, peak_pending_frames, next_frame_slot;
    nx_metal_slot_info slots[NX_METAL_FRAME_SLOTS];
    nx_metal_frame_profile last_frame;
    nx_metal_gpu_timing last_completed_timing;
    /* Cumulative CPU polling time across successful/timeout retirements,
       preserved through recovery. No-pending calls add no wait sample. */
    uint64_t retirement_wait_ns, retirement_wait_calls, retirement_clock_errors;
} nx_metal_renderer_info;

/* Private AppKit/Metal backend. Every call requires the main thread. The
   renderer leases its window until successful destruction; native objects
   never appear in the public installed C API. Initial drawing supports shapes,
   nested clips, straight alpha and owned RGBA8/A8 uploads. */
nx_result nx_metal_renderer_create(nx_window *window, const nx_allocator *allocator,
    nx_metal_renderer **out_renderer);
nx_result nx_metal_render(nx_metal_renderer *renderer, const nx_command *commands,
    size_t count, double scale, nx_color clear, int *out_presented);
nx_result nx_metal_renderer_get_info(nx_metal_renderer *renderer,
    nx_metal_renderer_info *out_info);
/* Cached nonblocking lookup for the latest 64 committed submissions. PENDING
   means not collected; it does not inspect GPU status or wait. Missing/expired
   IDs return STATE with zero output. Recovery clears the timing history. */
nx_result nx_metal_renderer_get_timing(nx_metal_renderer *renderer,
    uint64_t submission, nx_metal_gpu_timing *out_timing);
/* Collect tail submissions within the same bounded retirement budget used by
   close. Failure retains resources. This does not wait for visible display. */
nx_result nx_metal_renderer_wait_idle(nx_metal_renderer *renderer);
nx_result nx_metal_renderer_recover(nx_metal_renderer *renderer);
/* Bounded wait failure retains the renderer and window lease. Retry close;
   do not destroy the window or free the renderer's allocator state meanwhile. */
nx_result nx_metal_renderer_destroy(nx_metal_renderer *renderer);
nx_result nx_metal_driver_open(nx_window *window, nx_renderer_driver *out_driver);

#ifdef NEXAUI_METAL_TEST_HOOKS
nx_result nx_metal_test_readback(nx_metal_renderer *renderer, nx_color clear,
    uint8_t *pixels, size_t stride);
nx_result nx_metal_test_render_readback(nx_metal_renderer *renderer,
    const nx_command *commands, size_t count, double scale, nx_color clear,
    uint8_t *pixels, size_t stride);
typedef enum nx_metal_test_fault {
    NX_MTL_TEST_DRAWABLE_UNAVAILABLE = 1,
    NX_MTL_TEST_CANVAS_MEMORY,
    NX_MTL_TEST_HISTORY_MEMORY,
    NX_MTL_TEST_COMMAND_MEMORY,
    NX_MTL_TEST_PIPELINE_MEMORY,
    NX_MTL_TEST_ATLAS_MEMORY,
    NX_MTL_TEST_UPLOAD_MEMORY,
    NX_MTL_TEST_GEOMETRY_MEMORY,
    NX_MTL_TEST_WAIT_TIMEOUT
} nx_metal_test_fault;
nx_result nx_metal_test_fail_next(nx_metal_renderer *renderer, nx_metal_test_fault fault);
nx_result nx_metal_test_invalidate(nx_metal_renderer *renderer);
/* Test-only A/B baseline for redundant invariant state bindings. Existing
   pending commands are immutable; the mode affects subsequent encoding. */
nx_result nx_metal_test_set_per_draw_bindings(nx_metal_renderer *renderer, int enabled);
nx_result nx_metal_test_set_batching(nx_metal_renderer *renderer, int enabled);
/* Drains before changing the in-flight limit; production always uses three.
   Test capture storage is owned, keyed by submission, and expires on slot
   reuse, resize, recovery or destruction. No caller output pointer is kept. */
nx_result nx_metal_test_set_frame_limit(nx_metal_renderer *renderer, uint32_t limit);
/* Test-only GPU gate. Enabling requires an empty ring; disabling signals the
   shared event before releasing it. It never installs an asynchronous callback. */
nx_result nx_metal_test_hold_gpu(nx_metal_renderer *renderer, int enabled);
nx_result nx_metal_test_queue_capture(nx_metal_renderer *renderer,
    const nx_command *commands, size_t count, double scale, nx_color clear,
    uint64_t *out_submission);
nx_result nx_metal_test_read_capture(nx_metal_renderer *renderer,
    uint64_t submission, uint8_t *pixels, size_t stride);
#endif

#endif
