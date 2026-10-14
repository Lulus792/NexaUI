#ifndef NEXAUI_VULKAN_RENDERER_H
#define NEXAUI_VULKAN_RENDERER_H

#include "device.h"
#include "renderer_backend.h"

typedef struct nx_vulkan_renderer nx_vulkan_renderer;

typedef struct nx_vulkan_renderer_info {
    nx_vk_device_info device;
    uint32_t pixel_width, pixel_height, image_count;
    uint64_t presented_frames;
    uint64_t swapchain_generation;
    int readback_supported;
} nx_vulkan_renderer_info;

/* Private Linux clear/present path. The window must outlive the renderer;
   every call runs on its owner thread. Only one renderer may lease a window.
   The optional allocator covers NexaUI storage, not Vulkan driver storage.
   Creation does not present a frame. Hidden windows are not suitable for
   Wayland presentation. Swapchain format is RGBA8/BGRA8 UNORM, FIFO present.
   Graphics/present families may differ; concurrent sharing handles that case. */
nx_result nx_vulkan_renderer_create(nx_window *window,
    const nx_allocator *allocator, nx_vulkan_renderer **out_renderer);
/* Rechecks window size, acquires with a finite timeout and clears/presents.
   Temporary acquisition/resize unavailability returns NX_OK, presented=0.
   A fatal submission error latches the renderer until explicit recovery. */
nx_result nx_vulkan_clear_present(nx_vulkan_renderer *renderer,
                                  nx_color clear, int *out_presented);
nx_result nx_vulkan_renderer_get_info(nx_vulkan_renderer *renderer,
                                     nx_vulkan_renderer_info *out_info);
nx_result nx_vulkan_renderer_recover(nx_vulkan_renderer *renderer);
void nx_vulkan_renderer_destroy(nx_vulkan_renderer *renderer);

/* F11 adapter: empty command streams only; drawing commands return unsupported
   before acquiring an image. Zero-size targets suspend; recovery is explicit. */
nx_result nx_vulkan_driver_open(nx_window *window,
                                nx_renderer_driver *out_driver);

#ifdef NEXAUI_VULKAN_TEST_HOOKS
/* One clear frame copied from the actual swapchain image before presentation.
   Output is tightly row-addressed RGBA8; stride may include padding. */
nx_result nx_vulkan_test_readback(nx_vulkan_renderer *renderer, nx_color clear,
                                 uint8_t *pixels, size_t stride_bytes);
/* Deterministic state injection, not a claim of physical device removal. */
nx_result nx_vulkan_test_invalidate(nx_vulkan_renderer *renderer, int broken);
#endif

#endif
