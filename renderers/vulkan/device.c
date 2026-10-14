#include "device_internal.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define NX_VK_ENUM_LIMIT 4096u
#define NX_VK_ENUM_ATTEMPTS 4u

struct nx_vk_device {
    nx_allocator allocator;
    nx_vk_api api;
    nx_vk_handles handles;
    nx_vk_device_info info;
    pthread_t owner;
};

typedef struct nx_vk_candidate {
    VkPhysicalDevice physical;
    VkPhysicalDeviceProperties properties;
    uint32_t queue_family;
    unsigned rank;
    int has_swapchain;
} nx_vk_candidate;

static void *nx_vk_malloc(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_vk_free(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static nx_result nx_vk_result(VkResult result) {
    if (result == VK_SUCCESS) return NX_OK;
    if (result == VK_ERROR_OUT_OF_HOST_MEMORY ||
        result == VK_ERROR_OUT_OF_DEVICE_MEMORY) return NX_ERROR_MEMORY;
    if (result == VK_ERROR_DEVICE_LOST) return NX_ERROR_STATE;
    if (result == VK_ERROR_EXTENSION_NOT_PRESENT ||
        result == VK_ERROR_LAYER_NOT_PRESENT ||
        result == VK_ERROR_FEATURE_NOT_PRESENT ||
        result == VK_ERROR_INCOMPATIBLE_DRIVER) return NX_ERROR_UNSUPPORTED;
    return NX_ERROR_PLATFORM;
}

static int nx_vk_valid_name(const char *name) {
    if (!name || !name[0]) return 0;
    for (size_t i = 1; i < VK_MAX_EXTENSION_NAME_SIZE; ++i)
        if (!name[i]) return 1;
    return 0;
}

static int nx_vk_valid_api(const nx_vk_api *api) {
    return api && api->create_instance && api->destroy_instance &&
        api->enumerate_devices && api->get_properties && api->get_queues &&
        api->enumerate_extensions && api->create_device &&
        api->destroy_device && api->get_queue && api->wait_idle;
}

static void nx_vk_deallocate(nx_vk_device *device, void *data, size_t bytes) {
    if (data) device->allocator.deallocate(device->allocator.user, data, bytes);
}

static nx_result nx_vk_enumerate(nx_vk_device *device,
                                  VkPhysicalDevice **out_devices,
                                  uint32_t *out_count) {
    *out_devices = NULL;
    *out_count = 0;
    for (unsigned attempt = 0; attempt < NX_VK_ENUM_ATTEMPTS; ++attempt) {
        uint32_t count = 0;
        VkResult status = device->api.enumerate_devices(
            device->handles.instance, &count, NULL);
        if (status != VK_SUCCESS) return nx_vk_result(status);
        if (!count) return NX_ERROR_UNSUPPORTED;
        if (count > NX_VK_ENUM_LIMIT) return NX_ERROR_LIMIT;
        size_t bytes = (size_t)count * sizeof(VkPhysicalDevice);
        VkPhysicalDevice *devices = device->allocator.allocate(
            device->allocator.user, bytes);
        if (!devices) return NX_ERROR_MEMORY;
        uint32_t written = count;
        status = device->api.enumerate_devices(device->handles.instance,
                                                &written, devices);
        if (status == VK_SUCCESS && written <= count) {
            /* Keep the allocation size for exact-size allocator release even
               when an adapter vanished between the two enumeration calls. */
            if (written < count)
                memset(devices + written, 0,
                       (size_t)(count - written) * sizeof(*devices));
            *out_devices = devices;
            *out_count = count;
            return NX_OK;
        }
        nx_vk_deallocate(device, devices, bytes);
        if (status != VK_INCOMPLETE) return status == VK_SUCCESS
            ? NX_ERROR_PLATFORM : nx_vk_result(status);
    }
    return NX_ERROR_LIMIT;
}

static nx_result nx_vk_swapchain_support(nx_vk_device *device,
                                          VkPhysicalDevice physical,
                                          int *out_supported) {
    *out_supported = 0;
    for (unsigned attempt = 0; attempt < NX_VK_ENUM_ATTEMPTS; ++attempt) {
        uint32_t count = 0;
        VkResult status = device->api.enumerate_extensions(physical, NULL,
                                                            &count, NULL);
        if (status != VK_SUCCESS) return nx_vk_result(status);
        if (!count) return NX_OK;
        if (count > NX_VK_ENUM_LIMIT) return NX_ERROR_LIMIT;
        size_t bytes = (size_t)count * sizeof(VkExtensionProperties);
        VkExtensionProperties *extensions = device->allocator.allocate(
            device->allocator.user, bytes);
        if (!extensions) return NX_ERROR_MEMORY;
        uint32_t written = count;
        status = device->api.enumerate_extensions(physical, NULL, &written,
                                                   extensions);
        if (status == VK_SUCCESS && written <= count) {
            for (uint32_t i = 0; i < written; ++i) {
                extensions[i].extensionName[VK_MAX_EXTENSION_NAME_SIZE - 1u] = 0;
                if (!strcmp(extensions[i].extensionName,
                            VK_KHR_SWAPCHAIN_EXTENSION_NAME))
                    *out_supported = 1;
            }
        }
        nx_vk_deallocate(device, extensions, bytes);
        if (status == VK_SUCCESS) return written <= count
            ? NX_OK : NX_ERROR_PLATFORM;
        if (status != VK_INCOMPLETE) return nx_vk_result(status);
    }
    return NX_ERROR_LIMIT;
}

static unsigned nx_vk_rank(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 5u;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 4u;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 3u;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return 1u;
    default: return 2u;
    }
}

static nx_vk_device_kind nx_vk_kind(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return NX_VK_DEVICE_DISCRETE;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return NX_VK_DEVICE_INTEGRATED;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return NX_VK_DEVICE_VIRTUAL;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return NX_VK_DEVICE_CPU;
    default: return NX_VK_DEVICE_OTHER;
    }
}

static nx_result nx_vk_inspect(nx_vk_device *device, VkPhysicalDevice physical,
                                int require_swapchain,
                                nx_vk_candidate *candidate) {
    *candidate = (nx_vk_candidate){0};
    if (physical == VK_NULL_HANDLE) return NX_OK;
    uint32_t count = 0;
    device->api.get_queues(physical, &count, NULL);
    if (!count) return NX_OK;
    if (count > NX_VK_ENUM_LIMIT) return NX_ERROR_LIMIT;
    size_t bytes = (size_t)count * sizeof(VkQueueFamilyProperties);
    VkQueueFamilyProperties *queues = device->allocator.allocate(
        device->allocator.user, bytes);
    if (!queues) return NX_ERROR_MEMORY;
    uint32_t written = count;
    device->api.get_queues(physical, &written, queues);
    uint32_t family = UINT32_MAX;
    if (written <= count)
        for (uint32_t i = 0; i < written; ++i)
            if (queues[i].queueCount &&
                (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                family = i;
                break;
            }
    nx_vk_deallocate(device, queues, bytes);
    if (written > count) return NX_ERROR_PLATFORM;
    if (family == UINT32_MAX) return NX_OK;
    int swapchain = 0;
    nx_result result = nx_vk_swapchain_support(device, physical, &swapchain);
    if (result != NX_OK) return result;
    if (require_swapchain && !swapchain) return NX_OK;
    device->api.get_properties(physical, &candidate->properties);
    if (candidate->properties.apiVersion < VK_API_VERSION_1_0) return NX_OK;
    candidate->physical = physical;
    candidate->queue_family = family;
    candidate->has_swapchain = swapchain;
    candidate->rank = nx_vk_rank(candidate->properties.deviceType);
    return NX_OK;
}

static nx_result nx_vk_select(nx_vk_device *device, int require_swapchain) {
    VkPhysicalDevice *devices = NULL;
    uint32_t count = 0;
    nx_result result = nx_vk_enumerate(device, &devices, &count);
    if (result != NX_OK) return result;
    size_t candidate_bytes = (size_t)count * sizeof(nx_vk_candidate);
    nx_vk_candidate *candidates = device->allocator.allocate(
        device->allocator.user, candidate_bytes);
    if (!candidates) {
        nx_vk_deallocate(device, devices, (size_t)count * sizeof(*devices));
        return NX_ERROR_MEMORY;
    }
    for (uint32_t i = 0; i < count; ++i) {
        result = nx_vk_inspect(device, devices[i], require_swapchain,
                                &candidates[i]);
        if (result != NX_OK) goto cleanup;
    }
    result = NX_ERROR_UNSUPPORTED;
    for (uint32_t attempt = 0; attempt < count; ++attempt) {
        nx_vk_candidate *best = NULL;
        for (uint32_t i = 0; i < count; ++i)
            if (candidates[i].rank &&
                (!best || candidates[i].rank > best->rank))
                best = &candidates[i];
        if (!best) break;
        best->rank = 0;
        float priority = 1.0f;
        VkDeviceQueueCreateInfo queue = {0};
        queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue.queueFamilyIndex = best->queue_family;
        queue.queueCount = 1;
        queue.pQueuePriorities = &priority;
        const char *extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        VkDeviceCreateInfo create = {0};
        create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create.queueCreateInfoCount = 1;
        create.pQueueCreateInfos = &queue;
        create.enabledExtensionCount = best->has_swapchain ? 1u : 0u;
        create.ppEnabledExtensionNames = best->has_swapchain ? &extension : NULL;
        VkDevice logical = VK_NULL_HANDLE;
        VkResult status = device->api.create_device(best->physical, &create,
                                                     NULL, &logical);
        if (status != VK_SUCCESS) {
            result = nx_vk_result(status);
            if (result == NX_ERROR_MEMORY) goto cleanup;
            continue;
        }
        VkQueue graphics = VK_NULL_HANDLE;
        device->api.get_queue(logical, best->queue_family, 0, &graphics);
        if (graphics == VK_NULL_HANDLE) {
            device->api.destroy_device(logical, NULL);
            result = NX_ERROR_PLATFORM;
            continue;
        }
        device->handles.physical = best->physical;
        device->handles.logical = logical;
        device->handles.graphics_queue = graphics;
        device->handles.graphics_queue_family = best->queue_family;
        memcpy(device->info.name, best->properties.deviceName,
               sizeof(device->info.name));
        device->info.name[sizeof(device->info.name) - 1u] = 0;
        device->info.vendor_id = best->properties.vendorID;
        device->info.device_id = best->properties.deviceID;
        device->info.api_version = best->properties.apiVersion;
        device->info.driver_version = best->properties.driverVersion;
        device->info.kind = nx_vk_kind(best->properties.deviceType);
        device->info.graphics_queue_family = best->queue_family;
        device->info.swapchain_enabled = best->has_swapchain;
        result = NX_OK;
        break;
    }
cleanup:
    nx_vk_deallocate(device, candidates, candidate_bytes);
    nx_vk_deallocate(device, devices, (size_t)count * sizeof(*devices));
    return result;
}

static nx_result nx_vk_create(const nx_vk_device_desc *desc,
                               const nx_vk_api *api, nx_vk_device **out_device) {
    if (!out_device) return NX_ERROR_ARGUMENT;
    *out_device = NULL;
    nx_vk_device_desc options = desc ? *desc : (nx_vk_device_desc){0};
    if (!nx_vk_valid_api(api) ||
        ((options.allocator.allocate == NULL) !=
         (options.allocator.deallocate == NULL)) ||
        options.instance_extension_count > 64u ||
        (options.instance_extension_count && !options.instance_extensions))
        return NX_ERROR_ARGUMENT;
    for (uint32_t i = 0; i < options.instance_extension_count; ++i) {
        if (!nx_vk_valid_name(options.instance_extensions[i]))
            return NX_ERROR_ARGUMENT;
        for (uint32_t j = 0; j < i; ++j)
            if (!strcmp(options.instance_extensions[i],
                        options.instance_extensions[j])) return NX_ERROR_ARGUMENT;
    }
    nx_allocator allocator = options.allocator;
    if (!allocator.allocate)
        allocator = (nx_allocator){NULL, nx_vk_malloc, nx_vk_free};
    nx_vk_device *device = allocator.allocate(allocator.user, sizeof(*device));
    if (!device) return NX_ERROR_MEMORY;
    *device = (nx_vk_device){0};
    device->allocator = allocator;
    device->api = *api;
    device->owner = pthread_self();
    VkApplicationInfo application = {0};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "NexaUI";
    application.pEngineName = "NexaUI";
    application.apiVersion = VK_API_VERSION_1_0;
    /* VK_KHR_swapchain has an instance-level dependency on VK_KHR_surface.
       Enabling it needs no display connection. Platform-specific surface
       extensions can be supplied by the future window renderer. */
    const char *extensions[65] = {VK_KHR_SURFACE_EXTENSION_NAME};
    uint32_t extension_count = 1;
    for (uint32_t i = 0; i < options.instance_extension_count; ++i)
        if (strcmp(options.instance_extensions[i], VK_KHR_SURFACE_EXTENSION_NAME))
            extensions[extension_count++] = options.instance_extensions[i];
    VkInstanceCreateInfo create = {0};
    create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create.pApplicationInfo = &application;
    create.enabledExtensionCount = extension_count;
    create.ppEnabledExtensionNames = extensions;
    VkResult status = device->api.create_instance(&create, NULL,
                                                  &device->handles.instance);
    nx_result result = nx_vk_result(status);
    if (result == NX_OK)
        result = nx_vk_select(device, options.require_swapchain != 0);
    if (result != NX_OK) {
        /* Failed Vulkan creation leaves the output handle undefined. */
        if (status != VK_SUCCESS) device->handles.instance = VK_NULL_HANDLE;
        nx_vk_device_destroy(device);
        return result;
    }
    *out_device = device;
    return NX_OK;
}

nx_result nx_vk_device_create(const nx_vk_device_desc *desc,
                              nx_vk_device **out_device) {
    const nx_vk_api api = {
        vkCreateInstance, vkDestroyInstance, vkEnumeratePhysicalDevices,
        vkGetPhysicalDeviceProperties, vkGetPhysicalDeviceQueueFamilyProperties,
        vkEnumerateDeviceExtensionProperties, vkCreateDevice, vkDestroyDevice,
        vkGetDeviceQueue, vkDeviceWaitIdle
    };
    return nx_vk_create(desc, &api, out_device);
}

nx_result nx_vk_device_get_info(nx_vk_device *device,
                                nx_vk_device_info *out_info) {
    if (!out_info) return NX_ERROR_ARGUMENT;
    *out_info = (nx_vk_device_info){0};
    if (!device) return NX_ERROR_ARGUMENT;
    if (!pthread_equal(device->owner, pthread_self())) return NX_ERROR_STATE;
    *out_info = device->info;
    return NX_OK;
}

nx_result nx_vk_device_get_handles(nx_vk_device *device,
                                   nx_vk_handles *out_handles) {
    if (!out_handles) return NX_ERROR_ARGUMENT;
    *out_handles = (nx_vk_handles){0};
    if (!device) return NX_ERROR_ARGUMENT;
    if (!pthread_equal(device->owner, pthread_self())) return NX_ERROR_STATE;
    *out_handles = device->handles;
    return NX_OK;
}

nx_result nx_vk_device_wait_idle(nx_vk_device *device) {
    if (!device) return NX_ERROR_ARGUMENT;
    if (!pthread_equal(device->owner, pthread_self())) return NX_ERROR_STATE;
    return nx_vk_result(device->api.wait_idle(device->handles.logical));
}

void nx_vk_device_destroy(nx_vk_device *device) {
    if (!device || !pthread_equal(device->owner, pthread_self())) return;
    if (device->handles.logical != VK_NULL_HANDLE) {
        (void)device->api.wait_idle(device->handles.logical);
        device->api.destroy_device(device->handles.logical, NULL);
    }
    if (device->handles.instance != VK_NULL_HANDLE)
        device->api.destroy_instance(device->handles.instance, NULL);
    nx_allocator allocator = device->allocator;
    allocator.deallocate(allocator.user, device, sizeof(*device));
}

#ifdef NEXAUI_VULKAN_TEST_HOOKS
nx_result nx_vk_test_device_create(const nx_vk_device_desc *desc,
                                   const nx_vk_api *api,
                                   nx_vk_device **out_device) {
    return nx_vk_create(desc, api, out_device);
}
#endif
