#include "device_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Every native wait is finite. CTest also bounds the whole process. */
static int submit_empty_frame(nx_vk_device *device) {
    nx_vk_handles handles;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    int passed = 0;
    if (nx_vk_device_get_handles(device, &handles) != NX_OK) return 0;
    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = handles.graphics_queue_family;
    if (vkCreateCommandPool(handles.logical, &pool_info, NULL, &pool) != VK_SUCCESS)
        goto cleanup;
    VkCommandBufferAllocateInfo allocate = {0};
    allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate.commandPool = pool;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(handles.logical, &allocate, &commands) != VK_SUCCESS)
        goto cleanup;
    VkCommandBufferBeginInfo begin = {0};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(commands, &begin) != VK_SUCCESS ||
        vkEndCommandBuffer(commands) != VK_SUCCESS) goto cleanup;
    VkFenceCreateInfo fence_info = {0};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(handles.logical, &fence_info, NULL, &fence) != VK_SUCCESS)
        goto cleanup;
    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commands;
    if (vkQueueSubmit(handles.graphics_queue, 1, &submit, fence) != VK_SUCCESS)
        goto cleanup;
    if (vkWaitForFences(handles.logical, 1, &fence, VK_TRUE,
                        UINT64_C(5000000000)) != VK_SUCCESS) {
        /* Do not destroy in-flight resources after a timeout. CTest terminates
           a stuck driver; successful/lost completion can be cleaned normally. */
        fprintf(stderr, "Vulkan queue did not complete within five seconds\n");
        exit(1);
    }
    passed = 1;
cleanup:
    if (fence != VK_NULL_HANDLE) vkDestroyFence(handles.logical, fence, NULL);
    if (pool != VK_NULL_HANDLE) vkDestroyCommandPool(handles.logical, pool, NULL);
    return passed;
}

int main(void) {
    nx_vk_device *device = NULL;
    nx_vk_device_desc desc = {0};
    desc.require_swapchain = 1;
    nx_result result = nx_vk_device_create(&desc, &device);
    if (result == NX_ERROR_UNSUPPORTED && !getenv("NEXAUI_REQUIRE_VULKAN")) {
        fprintf(stderr, "No suitable Vulkan driver; native device test skipped\n");
        return 77;
    }
    if (result != NX_OK) {
        fprintf(stderr, "Vulkan device creation failed: %d\n", (int)result);
        return 1;
    }
    nx_vk_device_info info;
    int passed = nx_vk_device_get_info(device, &info) == NX_OK &&
        info.name[0] && info.swapchain_enabled && submit_empty_frame(device);
    if (passed) printf("Vulkan adapter: %s; vendor=%u device=%u family=%u type=%d\n",
                       info.name, info.vendor_id, info.device_id,
                       info.graphics_queue_family, (int)info.kind);
    nx_vk_device *second = NULL;
    if (passed) {
        passed = nx_vk_device_create(NULL, &second) == NX_OK;
        if (passed) passed = submit_empty_frame(second);
    }
    nx_vk_device_destroy(device);
    if (second) {
        if (passed) passed = submit_empty_frame(second);
        nx_vk_device_destroy(second);
    }
    if (!passed) return 1;
    for (unsigned i = 0; i < 8u; ++i) {
        if (nx_vk_device_create(NULL, &device) != NX_OK) return 1;
        passed = submit_empty_frame(device);
        nx_vk_device_destroy(device);
        if (!passed) return 1;
    }
    return 0;
}
