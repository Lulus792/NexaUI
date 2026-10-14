#ifndef NEXAUI_VULKAN_PIPELINE_H
#define NEXAUI_VULKAN_PIPELINE_H

#include "device_internal.h"

#define NX_VK_PAINTER_IMAGES 64u

/* All storage belongs to one renderer. The caller retires its submission
   before prepare/destroy and its presentations before destroying the images. */
typedef struct nx_vk_painter {
    VkDevice device;
    VkPhysicalDevice physical;
    VkRenderPass render_pass;
    VkPipelineLayout layout;
    VkPipeline pipeline;
    VkImageView views[NX_VK_PAINTER_IMAGES];
    VkFramebuffer frames[NX_VK_PAINTER_IMAGES];
    VkBuffer vertices;
    VkDeviceMemory memory;
    void *mapped;
    VkDeviceSize capacity;
    uint64_t allocations;
    uint32_t vertex_count;
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    int fail_framebuffer, fail_vertex_memory;
#endif
} nx_vk_painter;

/* Validates the entire stream before image acquisition. Initially supports
   opaque filled rectangles/triangles; later stages add clips and blending. */
nx_result nx_vk_painter_preflight(const nx_command *commands, size_t count,
                                 double scale);
nx_result nx_vk_painter_prepare(nx_vk_painter *painter,
    const nx_vk_handles *native, VkFormat format, VkExtent2D extent,
    const VkImage *images, uint32_t image_count,
    const nx_command *commands, size_t count, double scale);
void nx_vk_painter_record(nx_vk_painter *painter, VkCommandBuffer commands,
                          uint32_t image_index, VkExtent2D extent);
void nx_vk_painter_destroy(nx_vk_painter *painter);

#endif
