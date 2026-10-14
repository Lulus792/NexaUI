#ifndef NEXAUI_VULKAN_RENDERER_H
#define NEXAUI_VULKAN_RENDERER_H

#include "device.h"
#include "renderer_backend.h"

typedef struct nx_vulkan_renderer nx_vulkan_renderer;

typedef enum nx_vulkan_retirement_policy {
    NX_VULKAN_RETIRE_STRICT = 0, /* Requires KHR or EXT present fences. */
    NX_VULKAN_RETIRE_COMPATIBLE = 1, /* Prefer fences, allow legacy fallback. */
    NX_VULKAN_RETIRE_LEGACY = 2 /* Explicit unextended device-idle policy. */
} nx_vulkan_retirement_policy;

typedef struct nx_vulkan_renderer_desc {
    nx_allocator allocator;
    nx_vulkan_retirement_policy retirement;
} nx_vulkan_renderer_desc;

typedef struct nx_vulkan_renderer_info {
    nx_vk_device_info device;
    uint32_t pixel_width, pixel_height, image_count;
    uint64_t presented_frames;
    uint64_t swapchain_generation;
    int readback_supported;
    uint32_t pending_present_fences;
    uint64_t completed_present_fences;
    int has_acquired_image, closing, pending_readback;
    int drawing_supported;
    uint64_t vertex_capacity_bytes, vertex_buffer_allocations;
    uint32_t last_submitted_vertices;
    uint32_t atlas_width, atlas_height, unique_bitmaps;
    uint64_t bitmap_upload_capacity_bytes, bitmap_upload_allocations, atlas_allocations;
} nx_vulkan_renderer_info;

/* Private Linux clear/present and initial drawing path. The window must outlive the renderer;
   every call runs on its owner thread. Only one renderer may lease a window.
   The optional allocator covers NexaUI storage, not Vulkan driver storage.
   Creation does not present a frame. Hidden windows are not suitable for
   Wayland presentation. Swapchain format is RGBA8/BGRA8 UNORM, FIFO present.
   Graphics/present families may differ; concurrent sharing handles that case. */
nx_result nx_vulkan_renderer_create(nx_window *window,
    const nx_allocator *allocator, nx_vulkan_renderer **out_renderer);
/* Default create uses strict retirement. Compatibility/legacy modes preserve
   the unextended path explicitly; their idle wait has no formal present-
   completion guarantee. Device diagnostics expose the enabled fence variant. */
nx_result nx_vulkan_renderer_create_config(nx_window *window,
    const nx_vulkan_renderer_desc *desc, nx_vulkan_renderer **out_renderer);
/* Rechecks window size, acquires with a finite timeout and clears/presents.
   Temporary acquisition/resize unavailability returns NX_OK, presented=0.
   A fatal submission error latches the renderer until explicit recovery. */
nx_result nx_vulkan_clear_present(nx_vulkan_renderer *renderer,
                                  nx_color clear, int *out_presented);
/* Core shapes and RGBA8/A8 bitmaps with straight alpha and nested clips. Entire
   streams are validated before acquisition; unsupported command types fail.
   Logical coordinates are multiplied by the supplied backing scale. */
nx_result nx_vulkan_render(nx_vulkan_renderer *renderer,
    const nx_command *commands, size_t count, double scale,
    nx_color clear, int *out_presented);
nx_result nx_vulkan_renderer_get_info(nx_vulkan_renderer *renderer,
                                     nx_vulkan_renderer_info *out_info);
nx_result nx_vulkan_renderer_recover(nx_vulkan_renderer *renderer);
/* Failure retains the renderer/window lease. Retry after pending work completes.
   While close is pending only diagnostics and another destroy are permitted. */
nx_result nx_vulkan_renderer_destroy(nx_vulkan_renderer *renderer);

/* F11 adapter for clear and initial drawing commands. Zero-size targets
   suspend; recovery is explicit. Unsupported streams do not acquire images. */
nx_result nx_vulkan_driver_open(nx_window *window,
                                nx_renderer_driver *out_driver);

#ifdef NEXAUI_VULKAN_TEST_HOOKS
/* Read the GPU render target before presentation: the owned canvas for drawn
   frames, otherwise the swapchain image. The finished canvas is copied to the
   swapchain without conversion. RGBA8 output; stride may include padding. */
nx_result nx_vulkan_test_readback(nx_vulkan_renderer *renderer, nx_color clear,
                                 uint8_t *pixels, size_t stride_bytes);
nx_result nx_vulkan_test_render_readback(nx_vulkan_renderer *renderer,
    const nx_command *commands, size_t count, double scale, nx_color clear,
    uint8_t *pixels, size_t stride_bytes);
/* Deterministic state injection, not a claim of physical device removal. */
nx_result nx_vulkan_test_invalidate(nx_vulkan_renderer *renderer, int broken);
typedef enum nx_vulkan_test_fault {
    NX_VK_TEST_RETIRE_TIMEOUT = 1,
    NX_VK_TEST_PRESENT_WAIT_TIMEOUT,
    NX_VK_TEST_PRESENT_OOM,
    NX_VK_TEST_PRESENT_OUT_OF_DATE,
    NX_VK_TEST_REBUILD_MEMORY,
    NX_VK_TEST_SUBMISSION_WAIT_TIMEOUT,
    NX_VK_TEST_PIPELINE_MEMORY,
    NX_VK_TEST_VERTEX_MEMORY,
    NX_VK_TEST_DRAW_MEMORY,
    NX_VK_TEST_BITMAP_RECORD_MEMORY,
    NX_VK_TEST_ATLAS_MEMORY,
    NX_VK_TEST_BITMAP_UPLOAD_MEMORY
} nx_vulkan_test_fault;
nx_result nx_vulkan_test_fail_next(nx_vulkan_renderer *renderer,
                                   nx_vulkan_test_fault fault);
#endif

#endif
