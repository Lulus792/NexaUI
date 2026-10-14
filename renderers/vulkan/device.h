#ifndef NEXAUI_VULKAN_DEVICE_H
#define NEXAUI_VULKAN_DEVICE_H

#include "nexaui/nexaui.h"

typedef struct nx_vk_device nx_vk_device;

typedef struct nx_vk_device_desc {
    nx_allocator allocator; /* Zero selects malloc/free for NexaUI storage. */
    const char *const *instance_extensions; /* Borrowed during create only. */
    uint32_t instance_extension_count; /* At most 64 unique names. */
    int require_swapchain;
} nx_vk_device_desc;

typedef enum nx_vk_device_kind {
    NX_VK_DEVICE_OTHER = 0,
    NX_VK_DEVICE_DISCRETE,
    NX_VK_DEVICE_INTEGRATED,
    NX_VK_DEVICE_VIRTUAL,
    NX_VK_DEVICE_CPU
} nx_vk_device_kind;

typedef struct nx_vk_device_info {
    char name[256];
    uint32_t vendor_id, device_id, api_version, driver_version;
    uint32_t graphics_queue_family;
    nx_vk_device_kind kind;
    int swapchain_enabled;
} nx_vk_device_info;

/* Private Linux Vulkan bootstrap, not an installed consumer API. No window,
   surface or swap chain is created yet. Enables VK_KHR_surface without opening
   a display connection. Requests Vulkan 1.0 with no optional
   device features. Prefers discrete, integrated, virtual, other, then CPU
   adapters with a nonempty graphics queue; enumeration order breaks ties.
   An adapter that cannot create a logical device is skipped, but allocation
   failures are reported immediately. The swapchain extension is enabled
   when supported, and is mandatory only when require_swapchain is nonzero.

   All calls, including destruction, belong to the creating thread. Separate
   objects are independent. Create clears *out_device on failure and releases
   all partial resources. The allocator owns only NexaUI bookkeeping; Vulkan
   driver storage uses the driver's default allocation callbacks. */
nx_result nx_vk_device_create(const nx_vk_device_desc *desc,
                              nx_vk_device **out_device);
nx_result nx_vk_device_get_info(nx_vk_device *device,
                                nx_vk_device_info *out_info);
nx_result nx_vk_device_wait_idle(nx_vk_device *device);
/* Waits for outstanding work, then destroys device and instance. NULL is a
   no-op. A call from another thread is ignored; the owner must destroy it. */
void nx_vk_device_destroy(nx_vk_device *device);

#endif
