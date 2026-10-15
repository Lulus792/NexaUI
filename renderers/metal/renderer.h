#ifndef NEXAUI_METAL_RENDERER_H
#define NEXAUI_METAL_RENDERER_H

#include "nexaui/nexaui.h"
#include "renderer_backend.h"

typedef struct nx_metal_renderer nx_metal_renderer;
typedef struct nx_metal_renderer_info {
    char device_name[256];
    uint32_t pixel_width, pixel_height;
    uint64_t submitted_frames, completed_frames;
    int pending, pending_readback, closing, broken;
} nx_metal_renderer_info;

/* Private AppKit/Metal backend. Every call requires the main thread. The
   renderer leases its window until successful destruction; native objects
   never appear in the public installed C API. Initial drawing supports opaque
   shapes and nested clips. Alpha/image commands fail before drawable acquisition. */
nx_result nx_metal_renderer_create(nx_window *window, const nx_allocator *allocator,
    nx_metal_renderer **out_renderer);
nx_result nx_metal_render(nx_metal_renderer *renderer, const nx_command *commands,
    size_t count, double scale, nx_color clear, int *out_presented);
nx_result nx_metal_renderer_get_info(nx_metal_renderer *renderer,
    nx_metal_renderer_info *out_info);
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
    NX_MTL_TEST_COMMAND_MEMORY,
    NX_MTL_TEST_PIPELINE_MEMORY,
    NX_MTL_TEST_WAIT_TIMEOUT
} nx_metal_test_fault;
nx_result nx_metal_test_fail_next(nx_metal_renderer *renderer, nx_metal_test_fault fault);
nx_result nx_metal_test_invalidate(nx_metal_renderer *renderer);
#endif

#endif
