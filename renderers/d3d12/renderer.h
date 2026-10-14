#ifndef NEXAUI_D3D12_RENDERER_H
#define NEXAUI_D3D12_RENDERER_H

#include "nexaui/nexaui.h"
#include "renderer_backend.h"

typedef struct nx_d3d12_renderer nx_d3d12_renderer;

typedef struct nx_d3d12_renderer_info {
    uint32_t pixel_width, pixel_height;
    uint64_t submitted_frames;
    uint64_t last_draw_calls;
    uint64_t last_bitmap_uploads;
    uint64_t last_composite_copy_texels;
    uint64_t texture_creations;
    uint64_t vertex_ring_allocations;
    uint64_t vertex_ring_wraps;
    uint64_t vertex_ring_capacity;
    uint64_t vertex_ring_head;
    uint64_t last_timed_frame;
    uint64_t gpu_timestamp_frequency;
    double last_gpu_upload_ms;
    double last_gpu_render_ms;
    double last_gpu_frame_ms;
    uint32_t pooled_textures;
    int using_warp;
} nx_d3d12_renderer_info;

/* Private Windows D3D12 renderer. The caller owns window and must keep it
   alive until renderer destruction. All calls run on the window's thread.
   Hardware is preferred, with DXGI WARP as a software fallback. */
nx_result nx_d3d12_renderer_create(nx_window *window,
                                    nx_d3d12_renderer **out_renderer);
/* Read the current client pixel size, resize when it changes, clear the
   backbuffer and present. Zero-size windows return NX_OK with presented=0;
   a removed device returns NX_ERROR_STATE until recovery succeeds. */
nx_result nx_d3d12_clear_present(nx_d3d12_renderer *renderer,
                                  nx_color clear_color,
                                  int *out_presented);
nx_result nx_d3d12_renderer_get_info(nx_d3d12_renderer *renderer,
                                     nx_d3d12_renderer_info *out_info);
/* Rebuild all device and swap-chain objects after device removal. May be
   retried if the replacement adapter is temporarily unavailable. */
nx_result nx_d3d12_renderer_recover(nx_d3d12_renderer *renderer);
void nx_d3d12_renderer_destroy(nx_d3d12_renderer *renderer);

/* Open the private F11 driver. It accepts the current core command stream in
   paint order, including shapes, RGBA8/A8 bitmaps and nested clips. Bitmap
   bytes are staged before submit returns. */
nx_result nx_d3d12_driver_open(nx_window *window,
                                nx_renderer_driver *out_driver);

#ifdef NEXAUI_D3D12_TEST_HOOKS
/* Forces a real D3D12 device-removal event where ID3D12Device5 is supported. */
nx_result nx_d3d12_test_remove_device(nx_d3d12_renderer *renderer);
/* Fail once after the replacement swap chain exists to test cleanup/retry. */
nx_result nx_d3d12_test_fail_next_rebuild(nx_d3d12_renderer *renderer);
/* Render and copy the current back buffer to caller-owned RGBA8 pixels. */
nx_result nx_d3d12_test_render_readback(nx_d3d12_renderer *renderer,
    const nx_command *commands, size_t count, double scale,
    nx_color clear, uint8_t *pixels, size_t stride_bytes);
#endif

#endif
