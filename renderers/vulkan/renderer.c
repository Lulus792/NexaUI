#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#include "device_internal.h"
#include "renderer.h"
#include "internal.h"
#include "window_internal.h"
#include "x11/window_internal.h"
#include "wayland/window_internal.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define NX_VK_MAX_IMAGES 64u
#define NX_VK_MAX_FORMATS 256u
#define NX_VK_WAIT_NS UINT64_C(1000000000)

struct nx_vulkan_renderer {
    nx_allocator allocator;
    pthread_t owner;
    nx_window *window;
    nx_vk_device *device;
    nx_vk_handles native;
    VkSwapchainKHR swapchain;
    VkImage images[NX_VK_MAX_IMAGES];
    VkSemaphore finished[NX_VK_MAX_IMAGES];
    uint32_t image_count;
    VkFormat format;
    VkExtent2D extent;
    VkCommandPool pool;
    VkCommandBuffer commands;
    VkSemaphore acquired;
    VkFence fence;
    uint64_t presented_frames;
    uint64_t swapchain_generation;
    int leased, dirty, broken, readback;
};

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
    for (uint32_t i = 0; i < NX_VK_MAX_IMAGES; ++i) {
        if (renderer->finished[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(renderer->native.logical, renderer->finished[i], NULL);
        renderer->finished[i] = VK_NULL_HANDLE;
        renderer->images[i] = VK_NULL_HANDLE;
    }
    if (renderer->swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(renderer->native.logical, renderer->swapchain, NULL);
    renderer->swapchain = VK_NULL_HANDLE;
    renderer->image_count = 0;
    renderer->extent = (VkExtent2D){0, 0};
}

static void nx_vk_drop_native(nx_vulkan_renderer *renderer) {
    if (renderer->device) {
        (void)nx_vk_device_wait_idle(renderer->device);
        nx_vk_drop_swapchain(renderer);
        if (renderer->fence != VK_NULL_HANDLE)
            vkDestroyFence(renderer->native.logical, renderer->fence, NULL);
        if (renderer->acquired != VK_NULL_HANDLE)
            vkDestroySemaphore(renderer->native.logical, renderer->acquired, NULL);
        if (renderer->pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(renderer->native.logical, renderer->pool, NULL);
        nx_vk_device_destroy(renderer->device);
    }
    renderer->device = NULL;
    renderer->native = (nx_vk_handles){0};
    renderer->fence = VK_NULL_HANDLE;
    renderer->acquired = VK_NULL_HANDLE;
    renderer->pool = VK_NULL_HANDLE;
    renderer->commands = VK_NULL_HANDLE;
}

static nx_result nx_vk_open_native(nx_vulkan_renderer *renderer) {
    const char *extension = nx_window_wayland_display(renderer->window)
        ? VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME : VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
    nx_vk_device_desc desc = {0};
    desc.allocator = renderer->allocator;
    desc.instance_extensions = &extension;
    desc.instance_extension_count = 1;
    desc.require_swapchain = 1;
    nx_vk_surface_source source = {renderer->window, nx_vk_create_window_surface};
    nx_result result = nx_vk_device_create_for_surface(&desc, &source,
                                                       &renderer->device);
    if (result != NX_OK) return result;
    result = nx_vk_device_get_handles(renderer->device, &renderer->native);
    if (result != NX_OK) return result;
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
    status = vkAllocateCommandBuffers(renderer->native.logical, &allocate,
                                      &renderer->commands);
    if (status != VK_SUCCESS) return nx_vk_status(status);
    VkSemaphoreCreateInfo semaphore = {0};
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    status = vkCreateSemaphore(renderer->native.logical, &semaphore,
                               NULL, &renderer->acquired);
    if (status != VK_SUCCESS) { renderer->acquired = VK_NULL_HANDLE; return nx_vk_status(status); }
    VkFenceCreateInfo fence = {0};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    status = vkCreateFence(renderer->native.logical, &fence, NULL, &renderer->fence);
    if (status != VK_SUCCESS) renderer->fence = VK_NULL_HANDLE;
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
    if (!width || !height) return NX_OK;
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
    result = nx_vk_device_wait_idle(renderer->device);
    if (result != NX_OK) {
        renderer->broken = 1;
        if (result != NX_ERROR_STATE) return result;
        *outcome = NX_RENDER_DEVICE_LOST;
        return NX_OK;
    }
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

nx_result nx_vulkan_renderer_create(nx_window *window,
    const nx_allocator *allocator, nx_vulkan_renderer **out_renderer) {
    if (!out_renderer) return NX_ERROR_ARGUMENT;
    *out_renderer = NULL;
    nx_window_info info;
    if (!window) return NX_ERROR_ARGUMENT;
    nx_result result = nx_window_get_info(window, &info);
    if (result != NX_OK) return result;
    nx_allocator a = allocator ? *allocator : (nx_allocator){0};
    if ((a.allocate == NULL) != (a.deallocate == NULL)) return NX_ERROR_ARGUMENT;
    if (!a.allocate)
        a = (nx_allocator){NULL, nx_vk_renderer_malloc, nx_vk_renderer_free};
    nx_vulkan_renderer *renderer = a.allocate(a.user, sizeof(*renderer));
    if (!renderer) return NX_ERROR_MEMORY;
    *renderer = (nx_vulkan_renderer){0};
    renderer->allocator = a;
    renderer->owner = pthread_self();
    renderer->window = window;
    result = nx_window_renderer_acquire(window, renderer);
    if (result == NX_OK) {
        renderer->leased = 1;
        result = nx_vk_open_native(renderer);
    }
    if (result != NX_OK) { nx_vulkan_renderer_destroy(renderer); return result; }
    *out_renderer = renderer;
    return NX_OK;
}

static nx_result nx_vk_present(nx_vulkan_renderer *renderer, nx_color clear,
    VkBuffer readback, int *presented) {
    *presented = 0;
    if (renderer->broken) return NX_ERROR_STATE;
    if (!renderer->image_count) return NX_OK;
    VkDevice device = renderer->native.logical;
    VkResult status = vkWaitForFences(device, 1, &renderer->fence, VK_TRUE, NX_VK_WAIT_NS);
    if (status == VK_TIMEOUT) return NX_OK;
    if (status != VK_SUCCESS) goto fatal;
    uint32_t index = 0;
    status = vkAcquireNextImageKHR(device, renderer->swapchain, NX_VK_WAIT_NS,
                                   renderer->acquired, VK_NULL_HANDLE, &index);
    if (status == VK_TIMEOUT || status == VK_NOT_READY) return NX_OK;
    if (status == VK_ERROR_OUT_OF_DATE_KHR) { renderer->dirty = 1; return NX_OK; }
    if (status != VK_SUCCESS && status != VK_SUBOPTIMAL_KHR) goto fatal;
    if (status == VK_SUBOPTIMAL_KHR) renderer->dirty = 1;
    if (index >= renderer->image_count) { status = VK_ERROR_UNKNOWN; goto fatal; }
    status = vkResetCommandBuffer(renderer->commands, 0);
    if (status != VK_SUCCESS) goto fatal;
    VkCommandBufferBeginInfo begin = {0};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    status = vkBeginCommandBuffer(renderer->commands, &begin);
    if (status != VK_SUCCESS) goto fatal;
    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = renderer->images[index];
    barrier.subresourceRange = (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(renderer->commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    VkClearColorValue color = {{clear.r, clear.g, clear.b, clear.a}};
    vkCmdClearColorImage(renderer->commands, renderer->images[index],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &barrier.subresourceRange);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    if (readback != VK_NULL_HANDLE) {
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(renderer->commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        VkBufferImageCopy copy = {0};
        copy.imageSubresource = (VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = (VkExtent3D){renderer->extent.width, renderer->extent.height, 1};
        vkCmdCopyImageToBuffer(renderer->commands, renderer->images[index],
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &copy);
        VkBufferMemoryBarrier host = {0};
        host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        host.buffer = readback;
        host.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(renderer->commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &host, 0, NULL);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.dstAccessMask = 0;
    vkCmdPipelineBarrier(renderer->commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    status = vkEndCommandBuffer(renderer->commands);
    if (status != VK_SUCCESS) goto fatal;
    status = vkResetFences(device, 1, &renderer->fence);
    if (status != VK_SUCCESS) goto fatal;
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &renderer->acquired;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &renderer->commands;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderer->finished[index];
    status = vkQueueSubmit(renderer->native.graphics_queue, 1, &submit, renderer->fence);
    if (status != VK_SUCCESS) goto fatal;
    VkPresentInfoKHR present = {0};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderer->finished[index];
    present.swapchainCount = 1;
    present.pSwapchains = &renderer->swapchain;
    present.pImageIndices = &index;
    status = vkQueuePresentKHR(renderer->native.present_queue, &present);
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
    return nx_vk_status(status);
}

nx_result nx_vulkan_clear_present(nx_vulkan_renderer *renderer,
                                  nx_color clear, int *out_presented) {
    if (!out_presented) return NX_ERROR_ARGUMENT;
    *out_presented = 0;
    if (!renderer || !nx_valid_color(clear)) return NX_ERROR_ARGUMENT;
    if (!nx_vk_renderer_owner(renderer)) return NX_ERROR_STATE;
    uint32_t width, height;
    nx_result result = nx_vk_window_target(renderer, &width, &height);
    nx_render_outcome outcome;
    if (result == NX_OK) result = nx_vk_resize(renderer, width, height, &outcome);
    if (result != NX_OK) return result;
    if (outcome == NX_RENDER_DEVICE_LOST) return NX_ERROR_STATE;
    return outcome == NX_RENDER_SUSPENDED ? NX_OK
        : nx_vk_present(renderer, clear, VK_NULL_HANDLE, out_presented);
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
    out_info->swapchain_generation = renderer->swapchain_generation;
    out_info->readback_supported = renderer->readback;
    return NX_OK;
}

nx_result nx_vulkan_renderer_recover(nx_vulkan_renderer *renderer) {
    if (!renderer) return NX_ERROR_ARGUMENT;
    if (!nx_vk_renderer_owner(renderer)) return NX_ERROR_STATE;
    nx_vk_drop_native(renderer);
    renderer->broken = 1;
    nx_result result = nx_vk_open_native(renderer);
    if (result == NX_OK) renderer->broken = 0;
    return result;
}

void nx_vulkan_renderer_destroy(nx_vulkan_renderer *renderer) {
    if (!nx_vk_renderer_owner(renderer)) return;
    nx_vk_drop_native(renderer);
    if (renderer->leased)
        (void)nx_window_renderer_release(renderer->window, renderer);
    nx_allocator allocator = renderer->allocator;
    allocator.deallocate(allocator.user, renderer, sizeof(*renderer));
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
    (void)commands;
    if (count) return NX_ERROR_UNSUPPORTED;
    nx_vulkan_renderer *renderer = user;
    if (!nx_vk_renderer_owner(renderer)) return NX_ERROR_STATE;
    /* Surface suboptimality can change without an application target change,
       so the backend must also honor its own dirty flag on cached F11 targets. */
    nx_result result = nx_vk_resize(renderer, target.pixel_width,
                                     target.pixel_height, outcome);
    if (result != NX_OK || *outcome != NX_RENDER_READY) return result;
    int presented = 0;
    result = nx_vk_present(renderer, clear, VK_NULL_HANDLE, &presented);
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

static void nx_vk_driver_destroy(void *user) { nx_vulkan_renderer_destroy(user); }

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
nx_result nx_vulkan_test_invalidate(nx_vulkan_renderer *renderer, int broken) {
    if (!renderer) return NX_ERROR_ARGUMENT;
    if (!nx_vk_renderer_owner(renderer)) return NX_ERROR_STATE;
    renderer->dirty = 1;
    if (broken) renderer->broken = 1;
    return NX_OK;
}

nx_result nx_vulkan_test_readback(nx_vulkan_renderer *renderer, nx_color clear,
                                 uint8_t *pixels, size_t stride) {
    if (!renderer || !pixels || !nx_valid_color(clear)) return NX_ERROR_ARGUMENT;
    if (!nx_vk_renderer_owner(renderer)) return NX_ERROR_STATE;
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
    VkDevice device = renderer->native.logical;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
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
    if (type == UINT32_MAX) { result = NX_ERROR_UNSUPPORTED; goto cleanup; }
    VkMemoryAllocateInfo allocate = {0};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = type;
    status = vkAllocateMemory(device, &allocate, NULL, &memory);
    if (status != VK_SUCCESS) { memory = VK_NULL_HANDLE; result = nx_vk_status(status); goto cleanup; }
    status = vkBindBufferMemory(device, buffer, memory, 0);
    if (status != VK_SUCCESS) { result = nx_vk_status(status); goto cleanup; }
    int presented;
    result = nx_vk_present(renderer, clear, buffer, &presented);
    if (result != NX_OK || !presented) { if (result == NX_OK) result = NX_ERROR_STATE; goto cleanup; }
    status = vkWaitForFences(device, 1, &renderer->fence, VK_TRUE, NX_VK_WAIT_NS);
    if (status != VK_SUCCESS) { result = nx_vk_status(status); goto cleanup; }
    void *mapped = NULL;
    status = vkMapMemory(device, memory, 0, requirements.size, 0, &mapped);
    if (status != VK_SUCCESS) { result = nx_vk_status(status); goto cleanup; }
    const uint8_t *source = mapped;
    int bgra = renderer->format == VK_FORMAT_B8G8R8A8_UNORM;
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < width; ++x) {
            size_t from = ((size_t)y * width + x) * 4u;
            size_t to = (size_t)y * stride + (size_t)x * 4u;
            pixels[to] = source[from + (bgra ? 2u : 0u)];
            pixels[to + 1u] = source[from + 1u];
            pixels[to + 2u] = source[from + (bgra ? 0u : 2u)];
            pixels[to + 3u] = source[from + 3u];
        }
    vkUnmapMemory(device, memory);
    result = NX_OK;
cleanup:
    /* Includes failed/timed-out submissions; never free a buffer still read
       or written by the queue. CTest bounds a nonresponsive driver's wait. */
    (void)vkDeviceWaitIdle(device);
    if (buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, buffer, NULL);
    if (memory != VK_NULL_HANDLE) vkFreeMemory(device, memory, NULL);
    return result;
}
#endif
