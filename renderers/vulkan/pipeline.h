#ifndef NEXAUI_VULKAN_PIPELINE_H
#define NEXAUI_VULKAN_PIPELINE_H

#include "device_internal.h"

typedef struct nx_vk_draw {
    VkRect2D scissor;
    uint32_t first_vertex, vertex_count;
    int composite;
} nx_vk_draw;

/* Set allocator before first use; preflight before prepare. All storage
   belongs to one renderer. The caller retires its submission
   before prepare/destroy and its presentations before destroying the images. */
typedef struct nx_vk_painter {
    nx_allocator allocator;
    VkDevice device;
    VkPhysicalDevice physical;
    VkRenderPass render_pass;
    VkPipelineLayout layout;
    VkPipeline pipeline;
    VkImage canvas, history;
    VkDeviceMemory canvas_memory, history_memory;
    VkImageView canvas_view, history_view;
    VkFramebuffer frame;
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor;
    VkSampler sampler;
    nx_vk_draw *draws;
    size_t draw_capacity, draw_count;
    int history_initialized;
    VkBuffer vertices;
    VkDeviceMemory memory;
    void *mapped;
    VkDeviceSize capacity;
    uint64_t allocations;
    uint32_t vertex_count;
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    int fail_framebuffer, fail_vertex_memory, fail_draw_memory;
#endif
} nx_vk_painter;

/* Validates the entire stream before image acquisition. Supports core shapes,
   straight alpha and balanced nested rectangular clips; bitmaps remain open. */
nx_result nx_vk_painter_preflight(const nx_command *commands, size_t count,
                                 double scale);
nx_result nx_vk_painter_prepare(nx_vk_painter *painter,
    const nx_vk_handles *native, VkFormat format, VkExtent2D extent,
    const nx_command *commands, size_t count, double scale);
void nx_vk_painter_record(nx_vk_painter *painter, VkCommandBuffer commands,
                          VkExtent2D extent);
void nx_vk_painter_destroy(nx_vk_painter *painter);

#endif
