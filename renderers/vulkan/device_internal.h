#ifndef NEXAUI_VULKAN_DEVICE_INTERNAL_H
#define NEXAUI_VULKAN_DEVICE_INTERNAL_H

#include "device.h"
#include <vulkan/vulkan.h>

/* Immutable dispatch is copied into each object. This also allows failure
   injection without mutable process-wide Vulkan state. */
typedef struct nx_vk_api {
    PFN_vkCreateInstance create_instance;
    PFN_vkDestroyInstance destroy_instance;
    PFN_vkEnumeratePhysicalDevices enumerate_devices;
    PFN_vkGetPhysicalDeviceProperties get_properties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties get_queues;
    PFN_vkEnumerateDeviceExtensionProperties enumerate_extensions;
    PFN_vkCreateDevice create_device;
    PFN_vkDestroyDevice destroy_device;
    PFN_vkGetDeviceQueue get_queue;
    PFN_vkDeviceWaitIdle wait_idle;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR surface_support;
    PFN_vkDestroySurfaceKHR destroy_surface;
} nx_vk_api;

typedef struct nx_vk_handles {
    VkInstance instance;
    VkPhysicalDevice physical;
    VkDevice logical;
    VkQueue graphics_queue;
    uint32_t graphics_queue_family;
    VkSurfaceKHR surface;
    VkQueue present_queue;
    uint32_t present_queue_family;
} nx_vk_handles;

typedef struct nx_vk_surface_source {
    void *user;
    VkResult (*create)(void *user, VkInstance instance, VkSurfaceKHR *out_surface);
} nx_vk_surface_source;

/* Creates and owns the surface after instance creation, before queue selection.
   desc supplies the matching platform instance extension. The chosen graphics
   and present families may differ; both queues are created in that case. */
nx_result nx_vk_device_create_for_surface(const nx_vk_device_desc *desc,
    const nx_vk_surface_source *source, nx_vk_device **out_device);

/* Borrowed until destruction, owner thread only. Any externally submitted
   work and resources must be retired before destroying this device. */
nx_result nx_vk_device_get_handles(nx_vk_device *device,
                                   nx_vk_handles *out_handles);

#ifdef NEXAUI_VULKAN_TEST_HOOKS
nx_result nx_vk_test_device_create(const nx_vk_device_desc *desc,
                                   const nx_vk_api *api,
                                   nx_vk_device **out_device);
nx_result nx_vk_test_surface_device_create(const nx_vk_device_desc *desc,
    const nx_vk_api *api, const nx_vk_surface_source *source,
    nx_vk_device **out_device);
#endif

#endif
