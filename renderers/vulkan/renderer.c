#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#define _POSIX_C_SOURCE 200809L
#include "device_internal.h"
#include "renderer.h"
#include "pipeline.h"
#include "internal.h"
#include "window_internal.h"
#include "x11/window_internal.h"
#include "wayland/window_internal.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef NEXAUI_VULKAN_TEST_HOOKS
#include <stdio.h>
#endif

#define NX_VK_MAX_IMAGES 64u
#define NX_VK_MAX_FORMATS 256u
#define NX_VK_WAIT_NS UINT64_C(1000000000)

typedef struct nx_vk_frame_slot {
    VkCommandBuffer commands;
    VkSemaphore acquired;
    VkFence fence;
    int pending;
    uint64_t submission;
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    VkBuffer capture_buffer;
    VkDeviceMemory capture_memory;
    VkExtent2D capture_extent;
    VkFormat capture_format;
    uint64_t capture_submission;
#endif
} nx_vk_frame_slot;

struct nx_vulkan_renderer {
    nx_allocator allocator;
    pthread_t owner;
    nx_window *window;
    nx_vk_device *device;
    nx_vk_handles native;
    VkSwapchainKHR swapchain;
    VkImage images[NX_VK_MAX_IMAGES];
    VkSemaphore finished[NX_VK_MAX_IMAGES];
    VkFence present_fences[NX_VK_MAX_IMAGES];
    unsigned char present_pending[NX_VK_MAX_IMAGES];
    uint32_t image_count;
    VkFormat format;
    VkExtent2D extent;
    VkCommandPool pool;
    nx_vk_frame_slot slots[NX_VK_FRAME_SLOTS];
    uint32_t slot_index, frame_slots;
    VkFence acquire_fence;
    VkBuffer readback_buffer;
    VkDeviceMemory readback_memory;
    uint32_t acquired_index;
    int acquire_pending, has_acquired;
    int device_lost, closing, legacy_presented;
    nx_vulkan_retirement_policy retirement;
    nx_vk_present_fence_support present_support;
    nx_vk_painter painter;
    uint32_t last_submitted_vertices;
    uint64_t completed_present_fences;
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    unsigned char test_faults[NX_VK_TEST_BITMAP_UPLOAD_MEMORY + 1];
    int disable_batching;
    VkSemaphore gate;
    uint64_t gate_value;
    int gate_signaled;
#endif
    uint64_t presented_frames;
    uint64_t submitted_frames, retired_frames;
    nx_vulkan_frame_profile last_frame;
    uint64_t swapchain_generation;
    int leased, dirty, broken, readback, drawing;
};

static uint64_t nx_vk_profile_now(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC,&t)) return 0;
    return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
}
static double nx_vk_profile_ms(uint64_t start, uint64_t end, int *valid) {
    if (!start || !end || end < start) { *valid = 0; return 0; }
    return (double)(end-start)/1.e6;
}

static void *nx_vk_renderer_malloc(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_vk_renderer_free(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static nx_result nx_vk_status(VkResult status) {
    if (status == VK_SUCCESS) return NX_OK;
    if (status == VK_ERROR_OUT_OF_HOST_MEMORY ||
        status == VK_ERROR_OUT_OF_DEVICE_MEMORY) return NX_ERROR_MEMORY;
    if (status == VK_ERROR_DEVICE_LOST) return NX_ERROR_STATE;
    if (status == VK_ERROR_EXTENSION_NOT_PRESENT ||
        status == VK_ERROR_FEATURE_NOT_PRESENT ||
        status == VK_ERROR_FORMAT_NOT_SUPPORTED) return NX_ERROR_UNSUPPORTED;
    if (status == VK_TIMEOUT || status == VK_NOT_READY) return NX_ERROR_LIMIT;
    return NX_ERROR_PLATFORM;
}

static int nx_vk_renderer_owner(nx_vulkan_renderer *renderer) {
    return renderer && pthread_equal(renderer->owner, pthread_self());
}

static void nx_vk_slot_retired(nx_vulkan_renderer *renderer, unsigned index) {
    nx_vk_frame_slot *slot = &renderer->slots[index];
    if (slot->pending && slot->submission) ++renderer->retired_frames;
    slot->pending = 0;
}

/* One budget for the selected subset; never release storage on timeout. */
static nx_result nx_vk_wait_slots(nx_vulkan_renderer *renderer, int all, unsigned selected) {
    VkFence fences[NX_VK_FRAME_SLOTS];
    unsigned count = 0;
    for (unsigned i = 0; i < NX_VK_FRAME_SLOTS; ++i)
        if ((all || i == selected) && renderer->slots[i].pending)
            fences[count++] = renderer->slots[i].fence;
    if (!count) return NX_OK;
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    if (renderer->test_faults[NX_VK_TEST_SUBMISSION_WAIT_TIMEOUT]) {
        --renderer->test_faults[NX_VK_TEST_SUBMISSION_WAIT_TIMEOUT];
        return NX_ERROR_LIMIT;
    }
#endif
    VkResult status = vkWaitForFences(renderer->native.logical, count,
        fences, VK_TRUE, NX_VK_WAIT_NS);
    if (status == VK_ERROR_DEVICE_LOST) {
        renderer->device_lost = renderer->broken = 1;
        for (unsigned i = 0; i < NX_VK_FRAME_SLOTS; ++i) renderer->slots[i].pending = 0;
        return NX_ERROR_STATE;
    }
    if (status == VK_SUCCESS)
        for (unsigned i = 0; i < NX_VK_FRAME_SLOTS; ++i)
            if (all || i == selected) nx_vk_slot_retired(renderer, i);
    return nx_vk_status(status);
}

#ifdef NEXAUI_VULKAN_TEST_HOOKS
static nx_result nx_vk_wait_submission(nx_vulkan_renderer *renderer) {
    return nx_vk_wait_slots(renderer, 1, 0);
}
#endif

static void nx_vk_free_readback(nx_vulkan_renderer *renderer) {
    if (renderer->readback_buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(renderer->native.logical, renderer->readback_buffer, NULL);
    if (renderer->readback_memory != VK_NULL_HANDLE)
        vkFreeMemory(renderer->native.logical, renderer->readback_memory, NULL);
    renderer->readback_buffer = VK_NULL_HANDLE;
    renderer->readback_memory = VK_NULL_HANDLE;
}

/* One aggregate timeout, not one timeout per swapchain image. No handles or
   pending flags are discarded on timeout/error. Device loss is terminal. */
static nx_result nx_vk_retire(nx_vulkan_renderer *renderer) {
    if (!renderer->device) return NX_OK;
    VkFence fences[NX_VK_MAX_IMAGES + NX_VK_FRAME_SLOTS + 1u];
    uint32_t count = 0;
    if (renderer->acquire_pending) fences[count++] = renderer->acquire_fence;
    for (unsigned i = 0; i < NX_VK_FRAME_SLOTS; ++i)
        if (renderer->slots[i].pending) fences[count++] = renderer->slots[i].fence;
    for (uint32_t i = 0; i < renderer->image_count; ++i)
        if (renderer->present_pending[i]) fences[count++] = renderer->present_fences[i];
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    if (count && renderer->test_faults[NX_VK_TEST_RETIRE_TIMEOUT]) {
        renderer->test_faults[NX_VK_TEST_RETIRE_TIMEOUT] = 0;
        return NX_ERROR_LIMIT;
    }
#endif
    if (count) {
        VkResult status = vkWaitForFences(renderer->native.logical, count,
            fences, VK_TRUE, NX_VK_WAIT_NS);
        if (status == VK_ERROR_DEVICE_LOST) {
            renderer->device_lost = renderer->broken = 1;
            renderer->acquire_pending = 0;
            for (unsigned i = 0; i < NX_VK_FRAME_SLOTS; ++i) renderer->slots[i].pending = 0;
            memset(renderer->present_pending, 0, sizeof(renderer->present_pending));
            return NX_OK;
        }
        if (status != VK_SUCCESS) return nx_vk_status(status);
    }
    if (renderer->legacy_presented) {
        nx_result result = nx_vk_device_wait_idle(renderer->device);
        if (result == NX_ERROR_STATE) renderer->device_lost = renderer->broken = 1;
        else if (result != NX_OK) return result;
    }
    renderer->acquire_pending = 0;
    for (unsigned i = 0; i < NX_VK_FRAME_SLOTS; ++i) nx_vk_slot_retired(renderer, i);
    for (uint32_t i = 0; i < renderer->image_count; ++i)
        if (renderer->present_pending[i]) {
            ++renderer->completed_present_fences;
            renderer->present_pending[i] = 0;
        }
    return NX_OK;
}

static VkResult nx_vk_create_window_surface(void *user, VkInstance instance,
                                            VkSurfaceKHR *surface) {
    nx_window *window = user;
    struct wl_display *wayland = nx_window_wayland_display(window);
    if (wayland) {
        PFN_vkCreateWaylandSurfaceKHR create = (PFN_vkCreateWaylandSurfaceKHR)
            vkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR");
        if (!create) return VK_ERROR_EXTENSION_NOT_PRESENT;
        VkWaylandSurfaceCreateInfoKHR info = {0};
        info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        info.display = wayland;
        info.surface = nx_window_wayland_surface(window);
        return create(instance, &info, NULL, surface);
    }
    PFN_vkCreateXlibSurfaceKHR create = (PFN_vkCreateXlibSurfaceKHR)
        vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");
    if (!create) return VK_ERROR_EXTENSION_NOT_PRESENT;
    VkXlibSurfaceCreateInfoKHR info = {0};
    info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    info.dpy = nx_window_x11_display(window);
    info.window = nx_window_x11_id(window);
    if (!info.dpy || !info.window) return VK_ERROR_INITIALIZATION_FAILED;
    return create(instance, &info, NULL, surface);
}

static void nx_vk_drop_swapchain(nx_vulkan_renderer *renderer) {
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    for (unsigned i = 0; i < NX_VK_FRAME_SLOTS; ++i) {
        nx_vk_frame_slot *slot = &renderer->slots[i];
        if (slot->capture_buffer) vkDestroyBuffer(renderer->native.logical, slot->capture_buffer, NULL);
        if (slot->capture_memory) vkFreeMemory(renderer->native.logical, slot->capture_memory, NULL);
        slot->capture_buffer = VK_NULL_HANDLE; slot->capture_memory = VK_NULL_HANDLE;
        slot->capture_submission = 0;
    }
#endif
    nx_vk_painter_destroy(&renderer->painter);
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    int trace = getenv("NEXAUI_VULKAN_TRACE_RETIRE") != NULL;
    if (trace) fprintf(stderr,"Vulkan drop: policy=%d support=%d generation=%llu acquired=%d acquisition_pending=%d submission_pending=%d legacy=%d\n",
        (int)renderer->retirement,(int)renderer->present_support,
        (unsigned long long)renderer->swapchain_generation,renderer->has_acquired,
        renderer->acquire_pending,renderer->slots[renderer->slot_index].pending,renderer->legacy_presented);
#endif
    for (uint32_t i = 0; i < NX_VK_MAX_IMAGES; ++i) {
#ifdef NEXAUI_VULKAN_TEST_HOOKS
        if (trace && renderer->finished[i] != VK_NULL_HANDLE) fprintf(stderr,
            "  image=%u finished=0x%llx present_pending=%u present_fence_status=%d\n",i,
            (unsigned long long)(uint64_t)renderer->finished[i],(unsigned)renderer->present_pending[i],
            renderer->present_fences[i] != VK_NULL_HANDLE ?
                (int)vkGetFenceStatus(renderer->native.logical,renderer->present_fences[i]) : -999);
#endif
        if (renderer->finished[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(renderer->native.logical, renderer->finished[i], NULL);
        renderer->finished[i] = VK_NULL_HANDLE;
        if (renderer->present_fences[i] != VK_NULL_HANDLE)
            vkDestroyFence(renderer->native.logical, renderer->present_fences[i], NULL);
        renderer->present_fences[i] = VK_NULL_HANDLE;
        renderer->present_pending[i] = 0;
        renderer->images[i] = VK_NULL_HANDLE;
    }
    if (renderer->swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(renderer->native.logical, renderer->swapchain, NULL);
    renderer->swapchain = VK_NULL_HANDLE;
    renderer->image_count = 0;
    renderer->extent = (VkExtent2D){0, 0};
    renderer->has_acquired = renderer->legacy_presented = 0;
}

static nx_result nx_vk_drop_native(nx_vulkan_renderer *renderer) {
    nx_result result = nx_vk_retire(renderer);
    if (result != NX_OK) return result;
    if (renderer->device) {
        nx_vk_free_readback(renderer);
        nx_vk_drop_swapchain(renderer);
        for (unsigned i = 0; i < NX_VK_FRAME_SLOTS; ++i) {
            if (renderer->slots[i].fence != VK_NULL_HANDLE)
                vkDestroyFence(renderer->native.logical, renderer->slots[i].fence, NULL);
            if (renderer->slots[i].acquired != VK_NULL_HANDLE)
                vkDestroySemaphore(renderer->native.logical, renderer->slots[i].acquired, NULL);
        }
        if (renderer->acquire_fence != VK_NULL_HANDLE)
            vkDestroyFence(renderer->native.logical, renderer->acquire_fence, NULL);
        if (renderer->pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(renderer->native.logical, renderer->pool, NULL);
#ifdef NEXAUI_VULKAN_TEST_HOOKS
        if (renderer->gate) vkDestroySemaphore(renderer->native.logical, renderer->gate, NULL);
        renderer->gate = VK_NULL_HANDLE; renderer->gate_value = 0; renderer->gate_signaled = 0;
#endif
        nx_vk_device_destroy_retired(renderer->device);
    }
    renderer->device = NULL;
    renderer->native = (nx_vk_handles){0};
    memset(renderer->slots, 0, sizeof(renderer->slots));
    renderer->slot_index = 0;
    renderer->pool = VK_NULL_HANDLE;
    renderer->acquire_fence = VK_NULL_HANDLE;
    renderer->acquire_pending = 0;
    renderer->device_lost = 0;
    return NX_OK;
}

static nx_result nx_vk_open_native(nx_vulkan_renderer *renderer) {
    const char *extension = nx_window_wayland_display(renderer->window)
        ? VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME : VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
    nx_vk_device_desc desc = {0};
    desc.allocator = renderer->allocator;
    desc.instance_extensions = &extension;
    desc.instance_extension_count = 1;
    desc.require_swapchain = 1;
    desc.present_fences = renderer->retirement == NX_VULKAN_RETIRE_LEGACY
        ? NX_VK_PRESENT_FENCES_DISABLED : renderer->retirement == NX_VULKAN_RETIRE_COMPATIBLE
        ? NX_VK_PRESENT_FENCES_OPTIONAL : NX_VK_PRESENT_FENCES_REQUIRED;
    nx_vk_surface_source source = {renderer->window, nx_vk_create_window_surface};
    nx_result result = nx_vk_device_create_for_surface(&desc, &source,
                                                       &renderer->device);
    if (result != NX_OK) return result;
    result = nx_vk_device_get_handles(renderer->device, &renderer->native);
    if (result != NX_OK) return result;
    nx_vk_device_info device_info;
    result = nx_vk_device_get_info(renderer->device, &device_info);
    if (result != NX_OK) return result;
    renderer->present_support = device_info.present_fences;
    VkCommandPoolCreateInfo pool = {0};
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = renderer->native.graphics_queue_family;
    VkResult status = vkCreateCommandPool(renderer->native.logical, &pool,
                                          NULL, &renderer->pool);
    if (status != VK_SUCCESS) { renderer->pool = VK_NULL_HANDLE; return nx_vk_status(status); }
    VkCommandBufferAllocateInfo allocate = {0};
    allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate.commandPool = renderer->pool;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1;
    for (unsigned i = 0; i < NX_VK_FRAME_SLOTS; ++i) {
        nx_vk_frame_slot *slot = &renderer->slots[i];
        status = vkAllocateCommandBuffers(renderer->native.logical, &allocate, &slot->commands);
        if (status != VK_SUCCESS) return nx_vk_status(status);
        VkSemaphoreCreateInfo semaphore = {0};
        semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        status = vkCreateSemaphore(renderer->native.logical, &semaphore, NULL, &slot->acquired);
        if (status != VK_SUCCESS) { slot->acquired = VK_NULL_HANDLE; return nx_vk_status(status); }
        VkFenceCreateInfo fence = {0};
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        status = vkCreateFence(renderer->native.logical, &fence, NULL, &slot->fence);
        if (status != VK_SUCCESS) { slot->fence = VK_NULL_HANDLE; return nx_vk_status(status); }
    }
    VkFenceCreateInfo fence = {0};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    status = vkCreateFence(renderer->native.logical, &fence, NULL, &renderer->acquire_fence);
    if (status != VK_SUCCESS) renderer->acquire_fence = VK_NULL_HANDLE;
    renderer->dirty = 1;
    return nx_vk_status(status);
}

static nx_result nx_vk_surface_format(nx_vulkan_renderer *renderer,
                                       VkSurfaceFormatKHR *out_format) {
    for (unsigned attempt = 0; attempt < 4u; ++attempt) {
        uint32_t count = 0;
        VkResult status = vkGetPhysicalDeviceSurfaceFormatsKHR(
            renderer->native.physical, renderer->native.surface, &count, NULL);
        if (status != VK_SUCCESS) return nx_vk_status(status);
        if (!count) return NX_ERROR_UNSUPPORTED;
        if (count > NX_VK_MAX_FORMATS) return NX_ERROR_LIMIT;
        VkSurfaceFormatKHR formats[NX_VK_MAX_FORMATS];
        status = vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->native.physical,
            renderer->native.surface, &count, formats);
        if (status == VK_INCOMPLETE) continue;
        if (status != VK_SUCCESS) return nx_vk_status(status);
        if (count > NX_VK_MAX_FORMATS) return NX_ERROR_PLATFORM;
        for (uint32_t i = 0; i < count; ++i)
            if (formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
                (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM ||
                 formats[i].format == VK_FORMAT_R8G8B8A8_UNORM ||
                 (count == 1u && formats[i].format == VK_FORMAT_UNDEFINED))) {
                *out_format = formats[i];
                if (out_format->format == VK_FORMAT_UNDEFINED)
                    out_format->format = VK_FORMAT_B8G8R8A8_UNORM;
                return NX_OK;
            }
        return NX_ERROR_UNSUPPORTED;
    }
    return NX_ERROR_LIMIT;
}

static nx_result nx_vk_resize(nx_vulkan_renderer *renderer, uint32_t width,
                               uint32_t height, nx_render_outcome *outcome) {
    *outcome = NX_RENDER_SUSPENDED;
    if (renderer->broken) { *outcome = NX_RENDER_DEVICE_LOST; return NX_OK; }
    if (renderer->closing) return NX_ERROR_STATE;
    if (!width || !height) return NX_OK;
    /* A timed-out image fence can leave an acquired image waiting for retry.
       Finish that frame before resizing or discarding its acquisition signal. */
    if (renderer->has_acquired) { *outcome = NX_RENDER_READY; return NX_OK; }
    VkSurfaceCapabilitiesKHR caps;
    VkResult status = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        renderer->native.physical, renderer->native.surface, &caps);
    if (status == VK_ERROR_DEVICE_LOST || status == VK_ERROR_SURFACE_LOST_KHR) {
        renderer->broken = 1;
        *outcome = NX_RENDER_DEVICE_LOST;
        return NX_OK;
    }
    if (status != VK_SUCCESS) return nx_vk_status(status);
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = width < caps.minImageExtent.width ? caps.minImageExtent.width
            : width > caps.maxImageExtent.width ? caps.maxImageExtent.width : width;
        extent.height = height < caps.minImageExtent.height ? caps.minImageExtent.height
            : height > caps.maxImageExtent.height ? caps.maxImageExtent.height : height;
    }
    if (!extent.width || !extent.height) return NX_OK;
    if (!renderer->dirty && renderer->swapchain != VK_NULL_HANDLE &&
        renderer->extent.width == extent.width && renderer->extent.height == extent.height) {
        *outcome = NX_RENDER_READY;
        return NX_OK;
    }
    if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        return NX_ERROR_UNSUPPORTED;
    if (!caps.minImageCount || caps.minImageCount > NX_VK_MAX_IMAGES)
        return NX_ERROR_LIMIT;
    VkSurfaceFormatKHR format;
    nx_result result = nx_vk_surface_format(renderer, &format);
    if (result == NX_ERROR_STATE) {
        renderer->broken = 1;
        *outcome = NX_RENDER_DEVICE_LOST;
        return NX_OK;
    }
    if (result != NX_OK) return result;
    result = nx_vk_retire(renderer);
    if (result != NX_OK) {
        if (result == NX_ERROR_LIMIT) return NX_OK;
        return result;
    }
    if (renderer->device_lost) { *outcome = NX_RENDER_DEVICE_LOST; return NX_OK; }
    nx_vk_drop_swapchain(renderer);
    VkSwapchainCreateInfoKHR create = {0};
    create.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create.surface = renderer->native.surface;
    create.minImageCount = caps.minImageCount < NX_VK_MAX_IMAGES
        ? caps.minImageCount + 1u : caps.minImageCount;
    if (caps.maxImageCount && create.minImageCount > caps.maxImageCount)
        create.minImageCount = caps.maxImageCount;
    create.imageFormat = format.format;
    create.imageColorSpace = format.colorSpace;
    create.imageExtent = extent;
    create.imageArrayLayers = 1;
    create.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(renderer->native.physical, format.format, &format_properties);
    VkFormatFeatureFlags drawing_features = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    renderer->drawing = (format_properties.optimalTilingFeatures & drawing_features) == drawing_features;
    renderer->readback = (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
    if (renderer->readback) create.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    uint32_t families[] = {renderer->native.graphics_queue_family,
                           renderer->native.present_queue_family};
    create.imageSharingMode = families[0] == families[1] ? VK_SHARING_MODE_EXCLUSIVE
                                                       : VK_SHARING_MODE_CONCURRENT;
    if (families[0] != families[1]) {
        create.queueFamilyIndexCount = 2;
        create.pQueueFamilyIndices = families;
    }
    create.preTransform = caps.currentTransform;
    const VkCompositeAlphaFlagBitsKHR modes[] = {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
    for (unsigned i = 0; i < 4u; ++i)
        if (caps.supportedCompositeAlpha & modes[i]) { create.compositeAlpha = modes[i]; break; }
    if (!create.compositeAlpha) return NX_ERROR_UNSUPPORTED;
    create.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    create.clipped = VK_TRUE;
    status = vkCreateSwapchainKHR(renderer->native.logical, &create, NULL,
                                 &renderer->swapchain);
    if (status != VK_SUCCESS) {
        renderer->swapchain = VK_NULL_HANDLE;
        renderer->dirty = 1;
        if (status == VK_ERROR_OUT_OF_DATE_KHR) return NX_OK;
        if (status == VK_ERROR_SURFACE_LOST_KHR || status == VK_ERROR_DEVICE_LOST) {
            renderer->broken = 1;
            *outcome = NX_RENDER_DEVICE_LOST;
            return NX_OK;
        }
        return nx_vk_status(status);
    }
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    if (renderer->test_faults[NX_VK_TEST_REBUILD_MEMORY]) {
        renderer->test_faults[NX_VK_TEST_REBUILD_MEMORY] = 0;
        nx_vk_drop_swapchain(renderer);
        return NX_ERROR_MEMORY;
    }
#endif
    uint32_t count = NX_VK_MAX_IMAGES;
    status = vkGetSwapchainImagesKHR(renderer->native.logical, renderer->swapchain,
                                     &count, renderer->images);
    if (status != VK_SUCCESS || !count || count > NX_VK_MAX_IMAGES) {
        nx_vk_drop_swapchain(renderer);
        return status == VK_SUCCESS || status == VK_INCOMPLETE ? NX_ERROR_LIMIT
                                                               : nx_vk_status(status);
    }
    renderer->image_count = count;
    VkSemaphoreCreateInfo semaphore = {0};
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (uint32_t i = 0; i < count; ++i) {
        status = vkCreateSemaphore(renderer->native.logical, &semaphore, NULL,
                                   &renderer->finished[i]);
        if (status != VK_SUCCESS) {
            renderer->finished[i] = VK_NULL_HANDLE;
            nx_vk_drop_swapchain(renderer);
            return nx_vk_status(status);
        }
        if (renderer->present_support != NX_VK_PRESENT_FENCES_NONE) {
            VkFenceCreateInfo fence = {0};
            fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            status = vkCreateFence(renderer->native.logical, &fence, NULL,
                                    &renderer->present_fences[i]);
            if (status != VK_SUCCESS) {
                renderer->present_fences[i] = VK_NULL_HANDLE;
                nx_vk_drop_swapchain(renderer);
                return nx_vk_status(status);
            }
        }
    }
    renderer->extent = extent;
    renderer->format = format.format;
    renderer->dirty = 0;
    ++renderer->swapchain_generation;
    *outcome = NX_RENDER_READY;
    return NX_OK;
}

static nx_result nx_vk_window_target(nx_vulkan_renderer *renderer,
                                      uint32_t *width, uint32_t *height) {
    nx_window_info info;
    nx_result result = nx_window_get_info(renderer->window, &info);
    if (result != NX_OK) return result;
    double w = ceil((double)info.width * info.scale);
    double h = ceil((double)info.height * info.scale);
    if (!isfinite(w) || !isfinite(h) || w < 0 || h < 0 ||
        w > UINT32_MAX || h > UINT32_MAX) return NX_ERROR_LIMIT;
    *width = (uint32_t)w;
    *height = (uint32_t)h;
    return NX_OK;
}

nx_result nx_vulkan_renderer_create_config(nx_window *window,
    const nx_vulkan_renderer_desc *desc, nx_vulkan_renderer **out_renderer) {
    if (!out_renderer) return NX_ERROR_ARGUMENT;
    *out_renderer = NULL;
    nx_window_info info;
    if (!window) return NX_ERROR_ARGUMENT;
    nx_result result = nx_window_get_info(window, &info);
    if (result != NX_OK) return result;
    nx_vulkan_renderer_desc options = desc ? *desc : (nx_vulkan_renderer_desc){0};
    if (options.retirement < NX_VULKAN_RETIRE_STRICT || options.retirement > NX_VULKAN_RETIRE_LEGACY)
        return NX_ERROR_ARGUMENT;
    nx_allocator a = options.allocator;
    if ((a.allocate == NULL) != (a.deallocate == NULL)) return NX_ERROR_ARGUMENT;
    if (!a.allocate)
        a = (nx_allocator){NULL, nx_vk_renderer_malloc, nx_vk_renderer_free};
    nx_vulkan_renderer *renderer = a.allocate(a.user, sizeof(*renderer));
    if (!renderer) return NX_ERROR_MEMORY;
    *renderer = (nx_vulkan_renderer){0};
    renderer->allocator = a;
    renderer->painter.allocator = a;
    renderer->painter.upload = &renderer->painter.uploads[0];
    renderer->frame_slots = NX_VK_FRAME_SLOTS;
    renderer->owner = pthread_self();
    renderer->window = window;
    renderer->retirement = options.retirement;
    result = nx_window_renderer_acquire(window, renderer);
    if (result == NX_OK) {
        renderer->leased = 1;
        result = nx_vk_open_native(renderer);
    }
    if (result != NX_OK) { nx_vulkan_renderer_destroy(renderer); return result; }
    *out_renderer = renderer;
    return NX_OK;
}

nx_result nx_vulkan_renderer_create(nx_window *window,
    const nx_allocator *allocator, nx_vulkan_renderer **out_renderer) {
    nx_vulkan_renderer_desc desc = {0};
    if (allocator) desc.allocator = *allocator;
    return nx_vulkan_renderer_create_config(window, &desc, out_renderer);
}

static nx_result nx_vk_present(nx_vulkan_renderer *renderer,
    const nx_command *draw_commands, size_t draw_count, double scale, nx_color clear,
    VkBuffer readback, int offscreen, int *presented) {
    *presented = 0;
    if (renderer->broken || renderer->closing) return NX_ERROR_STATE;
    if (!renderer->image_count) return NX_OK;
    if (renderer->submitted_frames == UINT64_MAX) return NX_ERROR_LIMIT;
    nx_vulkan_frame_profile profile = {0};
    profile.pixel_width = renderer->extent.width; profile.pixel_height = renderer->extent.height;
    profile.draw_scale = scale; profile.input_commands = draw_count; profile.cpu_timing_valid = 1;
    profile.frame_slot = renderer->slot_index; profile.frames_in_flight = renderer->frame_slots;
    profile.batched = 1;
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    profile.batched = !renderer->disable_batching;
    renderer->painter.disable_batching = renderer->disable_batching;
#endif
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    if (!offscreen && renderer->slots[renderer->slot_index].capture_buffer) return NX_ERROR_LIMIT;
#endif
    uint64_t phase = nx_vk_profile_now();
    VkDevice device = renderer->native.logical;
    nx_result submission_result = nx_vk_wait_slots(renderer,
        readback == VK_NULL_HANDLE && renderer->readback_buffer != VK_NULL_HANDLE, renderer->slot_index);
    renderer->painter.upload = &renderer->painter.uploads[renderer->slot_index];
    if (submission_result == NX_ERROR_LIMIT) return NX_OK;
    if (submission_result != NX_OK) return submission_result;
    profile.retirement_ms = nx_vk_profile_ms(phase,nx_vk_profile_now(),&profile.cpu_timing_valid);
    phase = nx_vk_profile_now();
    if (draw_count && !renderer->drawing) return NX_ERROR_UNSUPPORTED;
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    if (draw_count) {
        renderer->painter.fail_framebuffer = renderer->test_faults[NX_VK_TEST_PIPELINE_MEMORY];
        renderer->painter.fail_vertex_memory = renderer->test_faults[NX_VK_TEST_VERTEX_MEMORY];
        renderer->painter.fail_draw_memory = renderer->test_faults[NX_VK_TEST_DRAW_MEMORY];
        renderer->painter.fail_bitmap_records = renderer->test_faults[NX_VK_TEST_BITMAP_RECORD_MEMORY];
        renderer->painter.fail_atlas_memory = renderer->test_faults[NX_VK_TEST_ATLAS_MEMORY];
        renderer->painter.fail_bitmap_upload = renderer->test_faults[NX_VK_TEST_BITMAP_UPLOAD_MEMORY];
        renderer->test_faults[NX_VK_TEST_PIPELINE_MEMORY] = 0;
        renderer->test_faults[NX_VK_TEST_VERTEX_MEMORY] = 0;
        renderer->test_faults[NX_VK_TEST_DRAW_MEMORY] = 0;
        renderer->test_faults[NX_VK_TEST_BITMAP_RECORD_MEMORY] = 0;
        renderer->test_faults[NX_VK_TEST_ATLAS_MEMORY] = 0;
        renderer->test_faults[NX_VK_TEST_BITMAP_UPLOAD_MEMORY] = 0;
    }
#endif
    nx_result prepared = nx_vk_painter_prepare(&renderer->painter, &renderer->native,
        renderer->format, renderer->extent, draw_commands, draw_count, scale);
    if (prepared != NX_OK) {
        if (prepared == NX_ERROR_STATE) renderer->broken = renderer->device_lost = 1;
        return prepared;
    }
    profile.prepare_ms = nx_vk_profile_ms(phase,nx_vk_profile_now(),&profile.cpu_timing_valid);
    phase = nx_vk_profile_now();
    if (readback == VK_NULL_HANDLE && renderer->readback_buffer != VK_NULL_HANDLE)
        nx_vk_free_readback(renderer);
    VkResult status;
    uint32_t index = 0;
    if (!offscreen) {
        if (!renderer->has_acquired) {
            status = vkResetFences(device, 1, &renderer->acquire_fence);
            if (status != VK_SUCCESS) goto fatal;
            status = vkAcquireNextImageKHR(device, renderer->swapchain, NX_VK_WAIT_NS,
                renderer->slots[renderer->slot_index].acquired, renderer->acquire_fence, &renderer->acquired_index);
            if (status == VK_ERROR_DEVICE_LOST) renderer->acquire_pending = 1;
            if (status == VK_TIMEOUT || status == VK_NOT_READY) return NX_OK;
            if (status == VK_ERROR_OUT_OF_DATE_KHR) { renderer->dirty = 1; return NX_OK; }
            if (status != VK_SUCCESS && status != VK_SUBOPTIMAL_KHR) goto fatal;
            renderer->has_acquired = renderer->acquire_pending = 1;
            if (status == VK_SUBOPTIMAL_KHR) renderer->dirty = 1;
        }
        index = renderer->acquired_index;
        if (index >= renderer->image_count) { status = VK_ERROR_UNKNOWN; goto fatal; }
        if (renderer->acquire_pending) {
            status = vkWaitForFences(device, 1, &renderer->acquire_fence, VK_TRUE, NX_VK_WAIT_NS);
            if (status == VK_TIMEOUT) return NX_OK;
            if (status != VK_SUCCESS) goto fatal;
            renderer->acquire_pending = 0;
        }
        if (renderer->present_pending[index]) {
#ifdef NEXAUI_VULKAN_TEST_HOOKS
            if (renderer->test_faults[NX_VK_TEST_PRESENT_WAIT_TIMEOUT]) {
                renderer->test_faults[NX_VK_TEST_PRESENT_WAIT_TIMEOUT] = 0;
                return NX_OK;
            }
#endif
            status = vkWaitForFences(device, 1, &renderer->present_fences[index], VK_TRUE, NX_VK_WAIT_NS);
            if (status == VK_TIMEOUT) return NX_OK;
            if (status != VK_SUCCESS) goto fatal;
            renderer->present_pending[index] = 0;
            ++renderer->completed_present_fences;
        }
    }
    profile.acquire_ms = nx_vk_profile_ms(phase,nx_vk_profile_now(),&profile.cpu_timing_valid);
    phase = nx_vk_profile_now();
    status = vkResetCommandBuffer(renderer->slots[renderer->slot_index].commands, 0);
    if (status != VK_SUCCESS) goto fatal;
    VkCommandBufferBeginInfo begin = {0};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    status = vkBeginCommandBuffer(renderer->slots[renderer->slot_index].commands, &begin);
    if (status != VK_SUCCESS) goto fatal;
    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    int buffered = renderer->painter.canvas != VK_NULL_HANDLE;
    VkImage target = buffered ? renderer->painter.canvas : renderer->images[index];
    barrier.image = target;
    barrier.subresourceRange = (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.srcAccessMask = buffered ? VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT : 0;
    vkCmdPipelineBarrier(renderer->slots[renderer->slot_index].commands,
        buffered ? VK_PIPELINE_STAGE_ALL_COMMANDS_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    VkClearColorValue color = {{floorf(clear.r * 255.0f + 0.5f) / 255.0f,
        floorf(clear.g * 255.0f + 0.5f) / 255.0f,
        floorf(clear.b * 255.0f + 0.5f) / 255.0f,
        floorf(clear.a * 255.0f + 0.5f) / 255.0f}};
    vkCmdClearColorImage(renderer->slots[renderer->slot_index].commands, target,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &barrier.subresourceRange);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    if (renderer->painter.vertex_count) {
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(renderer->slots[renderer->slot_index].commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        nx_vk_painter_record(&renderer->painter, renderer->slots[renderer->slot_index].commands, renderer->extent);
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    if (readback != VK_NULL_HANDLE || buffered) {
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(renderer->slots[renderer->slot_index].commands, source_stage,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        if (readback != VK_NULL_HANDLE) {
            VkBufferImageCopy copy = {0};
            copy.imageSubresource = (VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = (VkExtent3D){renderer->extent.width, renderer->extent.height, 1};
            vkCmdCopyImageToBuffer(renderer->slots[renderer->slot_index].commands, target,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &copy);
            VkBufferMemoryBarrier host = {0};
            host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            host.buffer = readback;
            host.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(renderer->slots[renderer->slot_index].commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &host, 0, NULL);
        }
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    if (buffered && !offscreen) {
        VkImageMemoryBarrier destination = barrier;
        destination.image = renderer->images[index];
        destination.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        destination.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        destination.srcAccessMask = 0;
        destination.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(renderer->slots[renderer->slot_index].commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &destination);
        VkImageCopy copy = {0};
        copy.srcSubresource = copy.dstSubresource = (VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.extent = (VkExtent3D){renderer->extent.width, renderer->extent.height, 1};
        vkCmdCopyImage(renderer->slots[renderer->slot_index].commands, target, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            renderer->images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        barrier = destination;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    if (!offscreen) {
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(renderer->slots[renderer->slot_index].commands, source_stage,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    }
    status = vkEndCommandBuffer(renderer->slots[renderer->slot_index].commands);
    if (status != VK_SUCCESS) goto fatal;
    profile.encode_ms = nx_vk_profile_ms(phase,nx_vk_profile_now(),&profile.cpu_timing_valid);
    phase = nx_vk_profile_now();
    status = vkResetFences(device, 1, &renderer->slots[renderer->slot_index].fence);
    if (status != VK_SUCCESS) goto fatal;
    VkSemaphore waits[2] = {renderer->slots[renderer->slot_index].acquired, VK_NULL_HANDLE};
    VkPipelineStageFlags wait_stages[2] = {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};
    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = offscreen ? 0u : 1u;
    submit.pWaitSemaphores = waits;
    submit.pWaitDstStageMask = wait_stages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &renderer->slots[renderer->slot_index].commands;
    submit.signalSemaphoreCount = offscreen ? 0u : 1u;
    submit.pSignalSemaphores = &renderer->finished[index];
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    uint64_t wait_values[2] = {0, 0}, signal_value = 0;
    VkTimelineSemaphoreSubmitInfoKHR timeline = {0};
    if (renderer->gate) {
        unsigned gate_index = submit.waitSemaphoreCount++;
        waits[gate_index] = renderer->gate;
        wait_stages[gate_index] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        wait_values[gate_index] = renderer->gate_value;
        timeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR;
        timeline.waitSemaphoreValueCount = submit.waitSemaphoreCount;
        timeline.pWaitSemaphoreValues = wait_values;
        timeline.signalSemaphoreValueCount = submit.signalSemaphoreCount;
        timeline.pSignalSemaphoreValues = &signal_value;
        submit.pNext = &timeline;
    }
#endif
    renderer->slots[renderer->slot_index].submission = 0;
    status = vkQueueSubmit(renderer->native.graphics_queue, 1, &submit, renderer->slots[renderer->slot_index].fence);
    if (status == VK_ERROR_DEVICE_LOST) renderer->slots[renderer->slot_index].pending = 1;
    if (status != VK_SUCCESS) goto fatal;
    renderer->slots[renderer->slot_index].pending = 1;
    profile.submission = ++renderer->submitted_frames;
    renderer->slots[renderer->slot_index].submission = profile.submission;
    profile.draw_records = renderer->painter.vertex_count/6u;
    profile.draw_calls = renderer->painter.recorded_draws;
    profile.render_passes = renderer->painter.recorded_passes;
    profile.history_copies = renderer->painter.recorded_history_copies;
    profile.history_copy_bytes = renderer->painter.recorded_history_bytes;
    profile.vertex_upload_bytes = renderer->painter.vertex_upload_bytes;
    profile.bitmap_upload_bytes = (uint64_t)renderer->painter.bitmap_extent.width*renderer->painter.bitmap_extent.height*4u;
    profile.present_copy_bytes = buffered && !offscreen ? (uint64_t)renderer->extent.width*renderer->extent.height*4u : 0;
    profile.submit_ms = nx_vk_profile_ms(phase,nx_vk_profile_now(),&profile.cpu_timing_valid);
    renderer->last_frame = profile;
    phase = nx_vk_profile_now();
    renderer->last_submitted_vertices = renderer->painter.vertex_count;
    renderer->slot_index = (renderer->slot_index + 1u) % renderer->frame_slots;
    if (offscreen) { *presented = 1; return NX_OK; }
    VkPresentInfoKHR present = {0};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderer->finished[index];
    present.swapchainCount = 1;
    present.pSwapchains = &renderer->swapchain;
    present.pImageIndices = &index;
    VkSwapchainPresentFenceInfoEXT present_fence = {0};
    if (renderer->present_support != NX_VK_PRESENT_FENCES_NONE) {
        status = vkResetFences(device, 1, &renderer->present_fences[index]);
        if (status != VK_SUCCESS) goto fatal;
        present_fence.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT;
        present_fence.swapchainCount = 1;
        present_fence.pFences = &renderer->present_fences[index];
        present.pNext = &present_fence;
    }
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    if (renderer->test_faults[NX_VK_TEST_PRESENT_OOM]) {
        renderer->test_faults[NX_VK_TEST_PRESENT_OOM] = 0;
        status = VK_ERROR_OUT_OF_HOST_MEMORY;
        goto fatal; /* No present call: the fence must not be awaited. */
    }
#endif
    status = vkQueuePresentKHR(renderer->native.present_queue, &present);
    renderer->last_frame.present_ms = nx_vk_profile_ms(phase,nx_vk_profile_now(),&renderer->last_frame.cpu_timing_valid);
    if (status == VK_ERROR_DEVICE_LOST) {
        if (renderer->present_support != NX_VK_PRESENT_FENCES_NONE)
            renderer->present_pending[index] = 1;
        else renderer->legacy_presented = 1;
    }
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    if ((status == VK_SUCCESS || status == VK_SUBOPTIMAL_KHR) &&
        renderer->test_faults[NX_VK_TEST_PRESENT_OUT_OF_DATE]) {
        renderer->test_faults[NX_VK_TEST_PRESENT_OUT_OF_DATE] = 0;
        status = VK_ERROR_OUT_OF_DATE_KHR; /* Real operation was enqueued. */
    }
#endif
    if (status != VK_ERROR_OUT_OF_HOST_MEMORY && status != VK_ERROR_OUT_OF_DEVICE_MEMORY &&
        status != VK_ERROR_DEVICE_LOST) {
        renderer->has_acquired = 0;
        if (renderer->present_support != NX_VK_PRESENT_FENCES_NONE)
            renderer->present_pending[index] = 1;
        else renderer->legacy_presented = 1;
    }
    if (status == VK_ERROR_OUT_OF_DATE_KHR || status == VK_SUBOPTIMAL_KHR)
        renderer->dirty = 1;
    else if (status != VK_SUCCESS) goto fatal;
    if (status != VK_ERROR_OUT_OF_DATE_KHR) {
        *presented = 1;
        ++renderer->presented_frames;
    }
    return NX_OK;
fatal:
    renderer->broken = 1;
    if (status == VK_ERROR_DEVICE_LOST) renderer->device_lost = 1;
    return nx_vk_status(status);
}

nx_result nx_vulkan_clear_present(nx_vulkan_renderer *renderer,
                                  nx_color clear, int *out_presented) {
    return nx_vulkan_render(renderer, NULL, 0, 1.0, clear, out_presented);
}

nx_result nx_vulkan_render(nx_vulkan_renderer *renderer,
    const nx_command *commands, size_t count, double scale,
    nx_color clear, int *out_presented) {
    if (!out_presented) return NX_ERROR_ARGUMENT;
    *out_presented = 0;
    if (!renderer || !nx_valid_color(clear)) return NX_ERROR_ARGUMENT;
    if (!nx_vk_renderer_owner(renderer)) return NX_ERROR_STATE;
    nx_result validation = nx_vk_painter_preflight(commands, count, scale);
    if (validation != NX_OK) return validation;
    uint32_t width, height;
    nx_result result = nx_vk_window_target(renderer, &width, &height);
    nx_render_outcome outcome;
    if (result == NX_OK) result = nx_vk_resize(renderer, width, height, &outcome);
    if (result != NX_OK) return result;
    if (outcome == NX_RENDER_DEVICE_LOST) return NX_ERROR_STATE;
    return outcome == NX_RENDER_SUSPENDED ? NX_OK
        : nx_vk_present(renderer, commands, count, scale, clear, VK_NULL_HANDLE, 0, out_presented);
}

nx_result nx_vulkan_renderer_get_info(nx_vulkan_renderer *renderer,
                                     nx_vulkan_renderer_info *out_info) {
    if (!out_info) return NX_ERROR_ARGUMENT;
    *out_info = (nx_vulkan_renderer_info){0};
    if (!renderer) return NX_ERROR_ARGUMENT;
    if (!nx_vk_renderer_owner(renderer)) return NX_ERROR_STATE;
    nx_result result = nx_vk_device_get_info(renderer->device, &out_info->device);
    if (result != NX_OK) return result;
    out_info->pixel_width = renderer->extent.width;
    out_info->pixel_height = renderer->extent.height;
    out_info->image_count = renderer->image_count;
    out_info->presented_frames = renderer->presented_frames;
    out_info->submitted_frames = renderer->submitted_frames; out_info->last_frame = renderer->last_frame;
    out_info->swapchain_generation = renderer->swapchain_generation;
    out_info->readback_supported = renderer->readback;
    out_info->completed_present_fences = renderer->completed_present_fences;
    out_info->has_acquired_image = renderer->has_acquired;
    out_info->closing = renderer->closing;
    out_info->pending_readback = renderer->readback_buffer != VK_NULL_HANDLE;
    out_info->drawing_supported = renderer->drawing;
    out_info->retired_frames = renderer->retired_frames;
    out_info->frame_slots = renderer->frame_slots;
    for (unsigned i = 0; i < NX_VK_FRAME_SLOTS; ++i) {
        out_info->pending_submissions += renderer->slots[i].pending != 0;
#ifdef NEXAUI_VULKAN_TEST_HOOKS
        out_info->pending_captures += renderer->slots[i].capture_buffer != VK_NULL_HANDLE;
#endif
        out_info->slot_vertex_capacity_bytes[i] = renderer->painter.uploads[i].capacity;
        out_info->slot_bitmap_capacity_bytes[i] = renderer->painter.uploads[i].bitmap_capacity;
        out_info->vertex_capacity_bytes += renderer->painter.uploads[i].capacity;
        out_info->bitmap_upload_capacity_bytes += renderer->painter.uploads[i].bitmap_capacity;
    }
    out_info->vertex_buffer_allocations = renderer->painter.allocations;
    out_info->last_submitted_vertices = renderer->last_submitted_vertices;
    out_info->atlas_width = renderer->painter.upload->atlas_extent.width;
    out_info->atlas_height = renderer->painter.upload->atlas_extent.height;
    out_info->unique_bitmaps = (uint32_t)renderer->painter.bitmap_count;
    out_info->bitmap_upload_allocations = renderer->painter.bitmap_allocations;
    out_info->atlas_allocations = renderer->painter.atlas_allocations;
    for (uint32_t i = 0; i < renderer->image_count; ++i)
        if (renderer->present_pending[i]) ++out_info->pending_present_fences;
    return NX_OK;
}

nx_result nx_vulkan_renderer_recover(nx_vulkan_renderer *renderer) {
    if (!renderer) return NX_ERROR_ARGUMENT;
    if (!nx_vk_renderer_owner(renderer)) return NX_ERROR_STATE;
    if (renderer->closing) return NX_ERROR_STATE;
    renderer->broken = 1;
    nx_result result = nx_vk_drop_native(renderer);
    if (result != NX_OK) return result;
    result = nx_vk_open_native(renderer);
    if (result == NX_OK) renderer->broken = 0;
    return result;
}

nx_result nx_vulkan_renderer_destroy(nx_vulkan_renderer *renderer) {
    if (!renderer) return NX_OK;
    if (!nx_vk_renderer_owner(renderer)) return NX_ERROR_STATE;
    renderer->closing = 1;
    nx_result result = nx_vk_drop_native(renderer);
    if (result != NX_OK) return result;
    if (renderer->leased)
        (void)nx_window_renderer_release(renderer->window, renderer);
    nx_allocator allocator = renderer->allocator;
    allocator.deallocate(allocator.user, renderer, sizeof(*renderer));
    return NX_OK;
}

static nx_result nx_vk_driver_resize(void *user, nx_render_target target,
                                      nx_render_outcome *outcome) {
    nx_vulkan_renderer *renderer = user;
    if (!nx_vk_renderer_owner(renderer)) return NX_ERROR_STATE;
    return nx_vk_resize(renderer, target.pixel_width, target.pixel_height, outcome);
}

static nx_result nx_vk_driver_submit(void *user, nx_render_target target,
    const nx_command *commands, size_t count, nx_color clear,
    nx_render_outcome *outcome) {
    nx_result validation = nx_vk_painter_preflight(commands, count, target.scale);
    if (validation != NX_OK) return validation;
    nx_vulkan_renderer *renderer = user;
    if (!nx_vk_renderer_owner(renderer)) return NX_ERROR_STATE;
    /* Surface suboptimality can change without an application target change,
       so the backend must also honor its own dirty flag on cached F11 targets. */
    nx_result result = nx_vk_resize(renderer, target.pixel_width,
                                     target.pixel_height, outcome);
    if (result != NX_OK || *outcome != NX_RENDER_READY) return result;
    int presented = 0;
    result = nx_vk_present(renderer, commands, count, target.scale, clear, VK_NULL_HANDLE, 0, &presented);
    if (renderer->broken) {
        *outcome = NX_RENDER_DEVICE_LOST;
        return result == NX_ERROR_MEMORY ? result : NX_OK;
    }
    *outcome = presented ? NX_RENDER_READY : NX_RENDER_SUSPENDED;
    return result;
}

static nx_result nx_vk_driver_recover(void *user) {
    return nx_vulkan_renderer_recover(user);
}

static nx_result nx_vk_driver_destroy(void *user) { return nx_vulkan_renderer_destroy(user); }

nx_result nx_vulkan_driver_open(nx_window *window, nx_renderer_driver *driver) {
    if (!driver || driver->open) return NX_ERROR_ARGUMENT;
    nx_vulkan_renderer *renderer = NULL;
    nx_result result = nx_vulkan_renderer_create(window, NULL, &renderer);
    if (result != NX_OK) return result;
    nx_render_backend backend = {renderer, nx_vk_driver_resize, nx_vk_driver_submit,
                                 nx_vk_driver_recover, nx_vk_driver_destroy};
    result = nx_renderer_open(driver, &backend);
    if (result != NX_OK) nx_vulkan_renderer_destroy(renderer);
    return result;
}

#ifdef NEXAUI_VULKAN_TEST_HOOKS
nx_result nx_vulkan_test_fail_next(nx_vulkan_renderer *renderer,
                                   nx_vulkan_test_fault fault) {
    if (!renderer || fault < NX_VK_TEST_RETIRE_TIMEOUT || fault > NX_VK_TEST_BITMAP_UPLOAD_MEMORY)
        return NX_ERROR_ARGUMENT;
    if (!nx_vk_renderer_owner(renderer)) return NX_ERROR_STATE;
    /* Readback checks submission once for copying and once during cleanup.
       Hold both waits to exercise ownership across the entire failed call. */
    renderer->test_faults[fault] = fault == NX_VK_TEST_SUBMISSION_WAIT_TIMEOUT ? 2 : 1;
    return NX_OK;
}

nx_result nx_vulkan_test_set_batching(nx_vulkan_renderer *renderer, int enabled) {
    if (!renderer || (enabled != 0 && enabled != 1)) return NX_ERROR_ARGUMENT;
    if (!nx_vk_renderer_owner(renderer) || renderer->closing || renderer->broken) return NX_ERROR_STATE;
    renderer->disable_batching = !enabled; return NX_OK;
}
nx_result nx_vulkan_test_invalidate(nx_vulkan_renderer *renderer, int broken) {
    if (!renderer) return NX_ERROR_ARGUMENT;
    if (!nx_vk_renderer_owner(renderer)) return NX_ERROR_STATE;
    if (renderer->closing) return NX_ERROR_STATE;
    renderer->dirty = 1;
    if (broken) renderer->broken = 1;
    return NX_OK;
}

nx_result nx_vulkan_test_set_frame_slots(nx_vulkan_renderer *renderer, unsigned slots) {
    if (!renderer || (slots != 1u && slots != NX_VK_FRAME_SLOTS)) return NX_ERROR_ARGUMENT;
    if (!nx_vk_renderer_owner(renderer) || renderer->closing || renderer->broken || renderer->has_acquired)
        return NX_ERROR_STATE;
    nx_result result = nx_vk_wait_submission(renderer);
    if (result != NX_OK) return result;
    renderer->frame_slots = slots;
    renderer->slot_index = 0;
    return NX_OK;
}

nx_result nx_vulkan_test_readback(nx_vulkan_renderer *renderer, nx_color clear,
                                 uint8_t *pixels, size_t stride) {
    return nx_vulkan_test_render_readback(renderer, NULL, 0, 1.0, clear, pixels, stride);
}

static nx_result nx_vk_allocate_capture(nx_vulkan_renderer *renderer, uint32_t width, uint32_t height,
                                        VkBuffer *out_buffer, VkDeviceMemory *out_memory) {
    if (!width || !height || (uint64_t)width * 4u > SIZE_MAX ||
        height > SIZE_MAX / ((size_t)width * 4u)) return NX_ERROR_LIMIT;
    VkDevice device = renderer->native.logical;
    VkBuffer buffer = VK_NULL_HANDLE; VkDeviceMemory memory = VK_NULL_HANDLE;
    nx_result result;
    VkBufferCreateInfo create = {0};
    create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create.size = (VkDeviceSize)width * height * 4u;
    create.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult status = vkCreateBuffer(device, &create, NULL, &buffer);
    if (status != VK_SUCCESS) return nx_vk_status(status);
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, buffer, &requirements);
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(renderer->native.physical, &properties);
    uint32_t type = UINT32_MAX;
    VkMemoryPropertyFlags needed = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
        if ((requirements.memoryTypeBits & (1u << i)) &&
            (properties.memoryTypes[i].propertyFlags & needed) == needed) { type = i; break; }
    if (type == UINT32_MAX) { result = NX_ERROR_UNSUPPORTED; goto fail; }
    VkMemoryAllocateInfo allocate = {0};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = type;
    status = vkAllocateMemory(device, &allocate, NULL, &memory);
    if (status != VK_SUCCESS) { memory = VK_NULL_HANDLE; result = nx_vk_status(status); goto fail; }
    status = vkBindBufferMemory(device, buffer, memory, 0);
    if (status != VK_SUCCESS) { result = nx_vk_status(status); goto fail; }
    *out_buffer = buffer; *out_memory = memory;
    return NX_OK;
fail:
    if (buffer) vkDestroyBuffer(device, buffer, NULL);
    if (memory) vkFreeMemory(device, memory, NULL);
    return result;
}

static nx_result nx_vk_copy_capture(nx_vulkan_renderer *renderer, VkDeviceMemory memory,
    VkExtent2D extent, VkFormat format, uint8_t *pixels, size_t stride) {
    void *mapped = NULL;
    VkResult status = vkMapMemory(renderer->native.logical, memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    if (status != VK_SUCCESS) return nx_vk_status(status);
    const uint8_t *source = mapped;
    int bgra = format == VK_FORMAT_B8G8R8A8_UNORM;
    for (uint32_t y = 0; y < extent.height; ++y)
        for (uint32_t x = 0; x < extent.width; ++x) {
            size_t from = ((size_t)y * extent.width + x) * 4u;
            size_t to = (size_t)y * stride + (size_t)x * 4u;
            pixels[to] = source[from + (bgra ? 2u : 0u)];
            pixels[to + 1u] = source[from + 1u];
            pixels[to + 2u] = source[from + (bgra ? 0u : 2u)];
            pixels[to + 3u] = source[from + 3u];
        }
    vkUnmapMemory(renderer->native.logical, memory);
    return NX_OK;
}

nx_result nx_vulkan_test_gate_begin(nx_vulkan_renderer *renderer) {
    if (!renderer) return NX_ERROR_ARGUMENT;
    if (!nx_vk_renderer_owner(renderer) || renderer->broken || renderer->closing || renderer->has_acquired)
        return NX_ERROR_STATE;
    for (unsigned i = 0; i < NX_VK_FRAME_SLOTS; ++i)
        if (renderer->slots[i].pending || renderer->slots[i].capture_buffer) return NX_ERROR_STATE;
    if (!renderer->native.test_timeline) return NX_ERROR_UNSUPPORTED;
    if (renderer->gate_value == UINT64_MAX) return NX_ERROR_LIMIT;
    if (!renderer->gate) {
        VkSemaphoreTypeCreateInfoKHR type = {0};
        type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR;
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR;
        VkSemaphoreCreateInfo create = {0}; create.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        create.pNext = &type;
        VkResult status = vkCreateSemaphore(renderer->native.logical, &create, NULL, &renderer->gate);
        if (status != VK_SUCCESS) { renderer->gate = VK_NULL_HANDLE; return nx_vk_status(status); }
    }
    ++renderer->gate_value; renderer->gate_signaled = 0;
    return NX_OK;
}

nx_result nx_vulkan_test_gate_signal(nx_vulkan_renderer *renderer) {
    if (!renderer) return NX_ERROR_ARGUMENT;
    /* Closing/recovery timeouts must still allow the host to release the gate. */
    if (!nx_vk_renderer_owner(renderer) || !renderer->gate) return NX_ERROR_STATE;
    if (renderer->gate_signaled) return NX_OK;
    PFN_vkSignalSemaphoreKHR signal = (PFN_vkSignalSemaphoreKHR)
        vkGetDeviceProcAddr(renderer->native.logical, "vkSignalSemaphoreKHR");
    if (!signal) return NX_ERROR_UNSUPPORTED;
    VkSemaphoreSignalInfoKHR info = {0}; info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO_KHR;
    info.semaphore = renderer->gate; info.value = renderer->gate_value;
    VkResult status = signal(renderer->native.logical, &info);
    if (status == VK_SUCCESS) renderer->gate_signaled = 1;
    return nx_vk_status(status);
}

nx_result nx_vulkan_test_queue_capture(nx_vulkan_renderer *renderer,
    const nx_command *commands, size_t count, double scale, nx_color clear, uint64_t *out_submission) {
    if (!out_submission) return NX_ERROR_ARGUMENT;
    *out_submission = 0;
    if (!renderer || !count || !nx_valid_color(clear)) return NX_ERROR_ARGUMENT;
    if (!nx_vk_renderer_owner(renderer) || renderer->closing || renderer->broken || renderer->has_acquired)
        return NX_ERROR_STATE;
    nx_result result = nx_vk_painter_preflight(commands, count, scale);
    if (result != NX_OK) return result;
    uint32_t width, height;
    result = nx_vk_window_target(renderer, &width, &height);
    nx_render_outcome outcome;
    if (result == NX_OK) result = nx_vk_resize(renderer, width, height, &outcome);
    if (result != NX_OK) return result;
    if (outcome != NX_RENDER_READY) return NX_ERROR_STATE;
    result = nx_vk_wait_slots(renderer, 0, renderer->slot_index);
    if (result != NX_OK) return result;
    nx_vk_frame_slot *slot = &renderer->slots[renderer->slot_index];
    if (slot->capture_buffer) return NX_ERROR_LIMIT;
    result = nx_vk_allocate_capture(renderer, renderer->extent.width, renderer->extent.height,
        &slot->capture_buffer, &slot->capture_memory);
    if (result != NX_OK) return result;
    slot->capture_extent = renderer->extent; slot->capture_format = renderer->format;
    int queued = 0;
    result = nx_vk_present(renderer, commands, count, scale, clear, slot->capture_buffer, 1, &queued);
    if (queued) {
        *out_submission = slot->capture_submission = renderer->submitted_frames;
        return result;
    }
    if (!slot->pending) {
        vkDestroyBuffer(renderer->native.logical, slot->capture_buffer, NULL);
        vkFreeMemory(renderer->native.logical, slot->capture_memory, NULL);
        slot->capture_buffer = VK_NULL_HANDLE; slot->capture_memory = VK_NULL_HANDLE;
    }
    return result == NX_OK ? NX_ERROR_LIMIT : result;
}

nx_result nx_vulkan_test_collect_capture(nx_vulkan_renderer *renderer,
    uint64_t submission, uint8_t *pixels, size_t stride) {
    if (!renderer || !submission || !pixels) return NX_ERROR_ARGUMENT;
    if (!nx_vk_renderer_owner(renderer) || renderer->closing) return NX_ERROR_STATE;
    for (unsigned i = 0; i < NX_VK_FRAME_SLOTS; ++i) {
        nx_vk_frame_slot *slot = &renderer->slots[i];
        if (slot->capture_submission != submission) continue;
        if ((uint64_t)slot->capture_extent.width * 4u > SIZE_MAX ||
            stride < (size_t)slot->capture_extent.width * 4u || slot->capture_extent.height > SIZE_MAX / stride)
            return NX_ERROR_LIMIT;
        nx_result result = nx_vk_wait_slots(renderer, 0, i);
        if (result != NX_OK) return result;
        result = nx_vk_copy_capture(renderer, slot->capture_memory, slot->capture_extent,
            slot->capture_format, pixels, stride);
        if (result != NX_OK) return result;
        vkDestroyBuffer(renderer->native.logical, slot->capture_buffer, NULL);
        vkFreeMemory(renderer->native.logical, slot->capture_memory, NULL);
        slot->capture_buffer = VK_NULL_HANDLE; slot->capture_memory = VK_NULL_HANDLE;
        slot->capture_submission = 0;
        return NX_OK;
    }
    return NX_ERROR_STATE;
}

nx_result nx_vulkan_test_render_readback(nx_vulkan_renderer *renderer,
    const nx_command *commands, size_t count, double scale, nx_color clear,
    uint8_t *pixels, size_t stride) {
    if (!renderer || !pixels || !nx_valid_color(clear)) return NX_ERROR_ARGUMENT;
    if (!nx_vk_renderer_owner(renderer)) return NX_ERROR_STATE;
    if (renderer->closing) return NX_ERROR_STATE;
    nx_result validation = nx_vk_painter_preflight(commands, count, scale);
    if (validation != NX_OK) return validation;
    if (renderer->readback_buffer != VK_NULL_HANDLE) {
        nx_result pending = nx_vk_wait_submission(renderer);
        if (pending != NX_OK) return pending;
        nx_vk_free_readback(renderer);
    }
    uint32_t width, height;
    nx_result result = nx_vk_window_target(renderer, &width, &height);
    nx_render_outcome outcome;
    if (result == NX_OK) result = nx_vk_resize(renderer, width, height, &outcome);
    if (result != NX_OK) return result;
    if (outcome != NX_RENDER_READY) return NX_ERROR_STATE;
    if (!renderer->readback) return NX_ERROR_UNSUPPORTED;
    width = renderer->extent.width;
    height = renderer->extent.height;
    if ((uint64_t)width * 4u > SIZE_MAX || stride < (size_t)width * 4u ||
        height > SIZE_MAX / stride || (size_t)height > SIZE_MAX / ((size_t)width * 4u))
        return NX_ERROR_LIMIT;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    result = nx_vk_allocate_capture(renderer, width, height, &buffer, &memory);
    if (result != NX_OK) return result;
    renderer->readback_buffer = buffer;
    renderer->readback_memory = memory;
    int presented;
    result = nx_vk_present(renderer, commands, count, scale, clear, buffer, 0, &presented);
    if (result != NX_OK || !presented) { if (result == NX_OK) result = NX_ERROR_STATE; goto cleanup; }
    result = nx_vk_wait_submission(renderer);
    if (result != NX_OK) goto cleanup;
    result = nx_vk_copy_capture(renderer, memory, renderer->extent, renderer->format, pixels, stride);
cleanup:
    {
        nx_result pending = nx_vk_wait_submission(renderer);
        if (pending == NX_OK || renderer->device_lost) nx_vk_free_readback(renderer);
        else return pending; /* Keep the in-flight allocation on the renderer. */
    }
    return result;
}
#endif
