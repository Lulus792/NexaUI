#ifndef NEXAUI_VULKAN_H
#define NEXAUI_VULKAN_H

#include "nexaui/nexaui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Optional Linux Vulkan adapter, provided by the NexaUI::Vulkan target. */
typedef struct nx_vulkan_view nx_vulkan_view;
typedef enum nx_vulkan_view_outcome {
    NX_VULKAN_VIEW_READY = 1,
    NX_VULKAN_VIEW_SUSPENDED = 2,
    NX_VULKAN_VIEW_DEVICE_LOST = 3
} nx_vulkan_view_outcome;

/* A successful open leases the window, which must outlive the view. The view
   and window are used and closed on their creating thread. Strict presentation
   retirement is required; unsupported devices return NX_ERROR_UNSUPPORTED.
   Wrapper storage uses the default allocator. *out_view is NULL on failure.
   No frame is submitted by open. */
nx_result nx_vulkan_view_open(nx_window *window, nx_vulkan_view **out_view);
/* Full-stream validation precedes backend work. Commands and bitmap bytes are
   borrowed only during this call. Pixel dimensions are physical; coordinates
   remain logical and scale maps them to pixels. Zero dimensions suspend.
   READY means the submission was accepted, not physically displayed. */
nx_result nx_vulkan_view_render(nx_vulkan_view *view, nx_pixel_size pixels,
    double scale, const nx_command *commands, size_t count, nx_color clear,
    nx_vulkan_view_outcome *out_outcome);
/* Recover only after DEVICE_LOST; the next render rebuilds target resources. */
nx_result nx_vulkan_view_recover(nx_vulkan_view *view);
/* A failed close retains *view and its window lease for retry; no further render
   or recovery is permitted once close starts. Successful close sets *view to
   NULL and invalidates aliases; closing a NULL view is a no-op. Wrong-thread
   calls leave it unchanged. */
nx_result nx_vulkan_view_close(nx_vulkan_view **view);

#ifdef __cplusplus
}
#endif
#endif
