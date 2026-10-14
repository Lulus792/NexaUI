#include "pipeline.h"
#include "renderer_backend.h"
#include "shaders.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

typedef struct nx_vk_vertex {
    float x, y;
    float rgba[4];
    nx_point triangle[3];
    uint32_t is_triangle;
} nx_vk_vertex;

static nx_result nx_vk_pipeline_status(VkResult status) {
    if (status == VK_SUCCESS) return NX_OK;
    if (status == VK_ERROR_OUT_OF_HOST_MEMORY || status == VK_ERROR_OUT_OF_DEVICE_MEMORY)
        return NX_ERROR_MEMORY;
    if (status == VK_ERROR_DEVICE_LOST) return NX_ERROR_STATE;
    if (status == VK_ERROR_FORMAT_NOT_SUPPORTED) return NX_ERROR_UNSUPPORTED;
    return NX_ERROR_PLATFORM;
}

nx_result nx_vk_painter_preflight(const nx_command *commands, size_t count,
                                 double scale) {
    if (!isfinite(scale) || scale <= 0) return NX_ERROR_ARGUMENT;
    if (count > UINT32_MAX / 6u || count > SIZE_MAX / (6u * sizeof(nx_vk_vertex)) ||
        count > SIZE_MAX / sizeof(nx_vk_draw))
        return NX_ERROR_LIMIT;
    nx_result result = nx_render_validate_commands(commands, count);
    if (result != NX_OK) return result;
    for (size_t i = 0; i < count; ++i) {
        const nx_command *command = &commands[i];
        if (command->type == NX_COMMAND_POP_CLIP) continue;
        if (command->type != NX_COMMAND_FILL_RECT && command->type != NX_COMMAND_FILL_TRIANGLE &&
            command->type != NX_COMMAND_PUSH_CLIP)
            return NX_ERROR_UNSUPPORTED;
        /* Bound NDC conversion even at a one-pixel target. No float overflow
           or optional shaderFloat64 requirement is hidden in the pipeline. */
        double coords[6];
        if (command->type != NX_COMMAND_FILL_TRIANGLE) {
            coords[0] = command->rect.x;
            coords[1] = command->rect.y;
            coords[2] = (double)command->rect.x + command->rect.width;
            coords[3] = (double)command->rect.y + command->rect.height;
            coords[4] = coords[5] = 0;
        } else {
            for (unsigned j = 0; j < 3u; ++j) {
                coords[j * 2u] = command->points[j].x;
                coords[j * 2u + 1u] = command->points[j].y;
            }
        }
        for (unsigned j = 0; j < 6u; ++j) {
            double scaled = coords[j] * scale * 2.0;
            if (!isfinite(scaled) || fabs(scaled) > (double)FLT_MAX / 2.0)
                return NX_ERROR_LIMIT;
            /* Edge products need finite float32 headroom even when the
               triangle is mostly outside the visible target. */
            if (command->type == NX_COMMAND_FILL_TRIANGLE &&
                fabs(scaled) > sqrt((double)FLT_MAX) / 4.0) return NX_ERROR_LIMIT;
        }
    }
    return NX_OK;
}

static void nx_vk_vertex_buffer_destroy(nx_vk_painter *painter) {
    if (painter->mapped) vkUnmapMemory(painter->device, painter->memory);
    if (painter->vertices) vkDestroyBuffer(painter->device, painter->vertices, NULL);
    if (painter->memory) vkFreeMemory(painter->device, painter->memory, NULL);
    painter->mapped = NULL;
    painter->vertices = VK_NULL_HANDLE;
    painter->memory = VK_NULL_HANDLE;
    painter->capacity = 0;
}

void nx_vk_painter_destroy(nx_vk_painter *painter) {
    if (painter->device) {
        nx_vk_vertex_buffer_destroy(painter);
        if (painter->frame) vkDestroyFramebuffer(painter->device, painter->frame, NULL);
        if (painter->pipeline) vkDestroyPipeline(painter->device, painter->pipeline, NULL);
        if (painter->layout) vkDestroyPipelineLayout(painter->device, painter->layout, NULL);
        if (painter->render_pass) vkDestroyRenderPass(painter->device, painter->render_pass, NULL);
        if (painter->descriptor_pool) vkDestroyDescriptorPool(painter->device, painter->descriptor_pool, NULL);
        if (painter->descriptor_layout) vkDestroyDescriptorSetLayout(painter->device, painter->descriptor_layout, NULL);
        if (painter->sampler) vkDestroySampler(painter->device, painter->sampler, NULL);
        if (painter->canvas_view) vkDestroyImageView(painter->device, painter->canvas_view, NULL);
        if (painter->history_view) vkDestroyImageView(painter->device, painter->history_view, NULL);
        if (painter->canvas) vkDestroyImage(painter->device, painter->canvas, NULL);
        if (painter->history) vkDestroyImage(painter->device, painter->history, NULL);
        if (painter->canvas_memory) vkFreeMemory(painter->device, painter->canvas_memory, NULL);
        if (painter->history_memory) vkFreeMemory(painter->device, painter->history_memory, NULL);
    }
    if (painter->draws) painter->allocator.deallocate(painter->allocator.user,
        painter->draws, painter->draw_capacity * sizeof(*painter->draws));
    nx_allocator allocator = painter->allocator;
    uint64_t allocations = painter->allocations;
    *painter = (nx_vk_painter){0};
    painter->allocations = allocations;
    painter->allocator = allocator;
}

static VkResult nx_vk_painter_image(nx_vk_painter *painter, VkFormat format,
    VkExtent2D extent, VkImageUsageFlags usage, VkImage *image,
    VkDeviceMemory *memory, VkImageView *view) {
    VkImageCreateInfo info = {0};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D; info.format = format;
    info.extent = (VkExtent3D){extent.width, extent.height, 1};
    info.mipLevels = info.arrayLayers = 1; info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL; info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult status = vkCreateImage(painter->device, &info, NULL, image);
    if (status != VK_SUCCESS) { *image = VK_NULL_HANDLE; return status; }
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(painter->device, *image, &requirements);
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(painter->physical, &properties);
    uint32_t type = UINT32_MAX;
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if (!(requirements.memoryTypeBits & (1u << i))) continue;
        if (type == UINT32_MAX) type = i;
        if (properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) { type = i; break; }
    }
    if (type == UINT32_MAX) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    VkMemoryAllocateInfo allocate = {0};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize = requirements.size; allocate.memoryTypeIndex = type;
    status = vkAllocateMemory(painter->device, &allocate, NULL, memory);
    if (status != VK_SUCCESS) { *memory = VK_NULL_HANDLE; return status; }
    status = vkBindImageMemory(painter->device, *image, *memory, 0);
    if (status != VK_SUCCESS) return status;
    VkImageViewCreateInfo create_view = {0};
    create_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_view.image = *image; create_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_view.format = format;
    create_view.subresourceRange = (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    status = vkCreateImageView(painter->device, &create_view, NULL, view);
    if (status != VK_SUCCESS) *view = VK_NULL_HANDLE;
    return status;
}

static nx_result nx_vk_painter_create(nx_vk_painter *painter,
    const nx_vk_handles *native, VkFormat format, VkExtent2D extent) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(native->physical, &properties);
    if (!extent.width || !extent.height ||
        extent.width > INT32_MAX || extent.height > INT32_MAX ||
        extent.width > properties.limits.maxImageDimension2D ||
        extent.height > properties.limits.maxImageDimension2D ||
        extent.width > properties.limits.maxViewportDimensions[0] ||
        extent.height > properties.limits.maxViewportDimensions[1] ||
        extent.width > properties.limits.maxFramebufferWidth ||
        extent.height > properties.limits.maxFramebufferHeight ||
        (double)extent.width > properties.limits.viewportBoundsRange[1] ||
        (double)extent.height > properties.limits.viewportBoundsRange[1]) return NX_ERROR_LIMIT;
    painter->device = native->logical;
    painter->physical = native->physical;
    VkResult status = nx_vk_painter_image(painter, format, extent,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        &painter->canvas, &painter->canvas_memory, &painter->canvas_view);
    if (status != VK_SUCCESS) goto fail;
    status = nx_vk_painter_image(painter, format, extent,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        &painter->history, &painter->history_memory, &painter->history_view);
    if (status != VK_SUCCESS) goto fail;
    VkSamplerCreateInfo sampler = {0}; sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter = sampler.minFilter = VK_FILTER_NEAREST;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    status = vkCreateSampler(native->logical, &sampler, NULL, &painter->sampler);
    if (status != VK_SUCCESS) { painter->sampler = VK_NULL_HANDLE; goto fail; }
    VkDescriptorSetLayoutBinding binding_info = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL};
    VkDescriptorSetLayoutCreateInfo descriptor_layout = {0};
    descriptor_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_layout.bindingCount = 1; descriptor_layout.pBindings = &binding_info;
    status = vkCreateDescriptorSetLayout(native->logical, &descriptor_layout, NULL, &painter->descriptor_layout);
    if (status != VK_SUCCESS) { painter->descriptor_layout = VK_NULL_HANDLE; goto fail; }
    VkDescriptorPoolSize pool_size = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo pool = {0}; pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.maxSets = pool.poolSizeCount = 1; pool.pPoolSizes = &pool_size;
    status = vkCreateDescriptorPool(native->logical, &pool, NULL, &painter->descriptor_pool);
    if (status != VK_SUCCESS) { painter->descriptor_pool = VK_NULL_HANDLE; goto fail; }
    VkDescriptorSetAllocateInfo set = {0}; set.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set.descriptorPool = painter->descriptor_pool; set.descriptorSetCount = 1;
    set.pSetLayouts = &painter->descriptor_layout;
    status = vkAllocateDescriptorSets(native->logical, &set, &painter->descriptor);
    if (status != VK_SUCCESS) goto fail;
    VkDescriptorImageInfo sampled = {painter->sampler, painter->history_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write = {0}; write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = painter->descriptor; write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo = &sampled;
    vkUpdateDescriptorSets(native->logical, 1, &write, 0, NULL);
    VkAttachmentDescription attachment = {0};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference reference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &reference;
    VkRenderPassCreateInfo pass = {0};
    pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass.attachmentCount = pass.subpassCount = 1;
    pass.pAttachments = &attachment;
    pass.pSubpasses = &subpass;
    status = vkCreateRenderPass(native->logical, &pass, NULL, &painter->render_pass);
    if (status != VK_SUCCESS) { painter->render_pass = VK_NULL_HANDLE; goto fail; }
    VkPipelineLayoutCreateInfo layout = {0};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.setLayoutCount = 1; layout.pSetLayouts = &painter->descriptor_layout;
    status = vkCreatePipelineLayout(native->logical, &layout, NULL, &painter->layout);
    if (status != VK_SUCCESS) { painter->layout = VK_NULL_HANDLE; goto fail; }
    VkShaderModule modules[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkShaderModuleCreateInfo shader = {0};
    shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.codeSize = sizeof(nx_vk_shape_vert);
    shader.pCode = nx_vk_shape_vert;
    status = vkCreateShaderModule(native->logical, &shader, NULL, &modules[0]);
    if (status != VK_SUCCESS) goto fail;
    shader.codeSize = sizeof(nx_vk_shape_frag);
    shader.pCode = nx_vk_shape_frag;
    status = vkCreateShaderModule(native->logical, &shader, NULL, &modules[1]);
    if (status != VK_SUCCESS) {
        vkDestroyShaderModule(native->logical, modules[0], NULL);
        goto fail;
    }
    VkPipelineShaderStageCreateInfo stages[2] = {{0}, {0}};
    for (unsigned i = 0; i < 2u; ++i) {
        stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[i].stage = i ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT;
        stages[i].module = modules[i];
        stages[i].pName = "main";
    }
    VkVertexInputBindingDescription binding = {0, sizeof(nx_vk_vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attributes[6] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(nx_vk_vertex, x)},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(nx_vk_vertex, rgba)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(nx_vk_vertex, triangle[0])},
        {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(nx_vk_vertex, triangle[1])},
        {4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(nx_vk_vertex, triangle[2])},
        {5, 0, VK_FORMAT_R32_UINT, offsetof(nx_vk_vertex, is_triangle)}
    };
    VkPipelineVertexInputStateCreateInfo vertex = {0};
    vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex.vertexBindingDescriptionCount = 1;
    vertex.pVertexBindingDescriptions = &binding;
    vertex.vertexAttributeDescriptionCount = 6;
    vertex.pVertexAttributeDescriptions = attributes;
    VkPipelineInputAssemblyStateCreateInfo assembly = {0};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport = {0};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster = {0};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo samples = {0};
    samples.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend_attachment = {0};
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend = {0};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blend_attachment;
    const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic = {0};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamic_states;
    VkGraphicsPipelineCreateInfo pipeline = {0};
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount = 2;
    pipeline.pStages = stages;
    pipeline.pVertexInputState = &vertex;
    pipeline.pInputAssemblyState = &assembly;
    pipeline.pViewportState = &viewport;
    pipeline.pRasterizationState = &raster;
    pipeline.pMultisampleState = &samples;
    pipeline.pColorBlendState = &blend;
    pipeline.pDynamicState = &dynamic;
    pipeline.layout = painter->layout;
    pipeline.renderPass = painter->render_pass;
    status = vkCreateGraphicsPipelines(native->logical, VK_NULL_HANDLE, 1, &pipeline, NULL,
                                       &painter->pipeline);
    vkDestroyShaderModule(native->logical, modules[1], NULL);
    vkDestroyShaderModule(native->logical, modules[0], NULL);
    if (status != VK_SUCCESS) goto fail; /* Partial pipeline outputs are destroyable. */
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    if (painter->fail_framebuffer) {
        painter->fail_framebuffer = 0;
        status = VK_ERROR_OUT_OF_HOST_MEMORY;
        goto fail;
    }
#endif
    VkFramebufferCreateInfo frame = {0};
    frame.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frame.renderPass = painter->render_pass;
    frame.attachmentCount = frame.layers = 1;
    frame.pAttachments = &painter->canvas_view;
    frame.width = extent.width;
    frame.height = extent.height;
    status = vkCreateFramebuffer(native->logical, &frame, NULL, &painter->frame);
    if (status != VK_SUCCESS) { painter->frame = VK_NULL_HANDLE; goto fail; }
    return NX_OK;
fail:
    nx_vk_painter_destroy(painter);
    return nx_vk_pipeline_status(status);
}

static nx_result nx_vk_painter_reserve(nx_vk_painter *painter, VkDeviceSize bytes) {
    if (bytes <= painter->capacity) return NX_OK;
    VkDeviceSize capacity = painter->capacity ? painter->capacity : 4096u;
    while (capacity < bytes) {
        if (capacity > UINT64_MAX / 2u) { capacity = bytes; break; }
        capacity *= 2u;
    }
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkBufferCreateInfo create = {0};
    create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create.size = capacity;
    create.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult status = vkCreateBuffer(painter->device, &create, NULL, &buffer);
    if (status != VK_SUCCESS) return nx_vk_pipeline_status(status);
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(painter->device, buffer, &requirements);
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(painter->physical, &properties);
    uint32_t type = UINT32_MAX;
    VkMemoryPropertyFlags required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
        if ((requirements.memoryTypeBits & (1u << i)) &&
            (properties.memoryTypes[i].propertyFlags & required) == required) { type = i; break; }
    if (type == UINT32_MAX) {
        vkDestroyBuffer(painter->device, buffer, NULL);
        return NX_ERROR_UNSUPPORTED;
    }
    VkMemoryAllocateInfo allocate = {0};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = type;
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    if (painter->fail_vertex_memory) {
        painter->fail_vertex_memory = 0;
        status = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        goto fail;
    }
#endif
    status = vkAllocateMemory(painter->device, &allocate, NULL, &memory);
    if (status != VK_SUCCESS) { memory = VK_NULL_HANDLE; goto fail; }
    status = vkBindBufferMemory(painter->device, buffer, memory, 0);
    if (status != VK_SUCCESS) goto fail;
    void *mapped = NULL;
    status = vkMapMemory(painter->device, memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    if (status != VK_SUCCESS) goto fail;
    nx_vk_vertex_buffer_destroy(painter);
    painter->vertices = buffer;
    painter->memory = memory;
    painter->mapped = mapped;
    painter->capacity = capacity;
    ++painter->allocations;
    return NX_OK;
fail:
    vkDestroyBuffer(painter->device, buffer, NULL);
    if (memory) vkFreeMemory(painter->device, memory, NULL);
    return nx_vk_pipeline_status(status);
}

static double nx_vk_pixel_edge(double coordinate, double scale, uint32_t limit) {
    double pixel = ceil(coordinate * scale - 0.5);
    return pixel < 0 ? 0 : pixel > limit ? limit : pixel;
}

static void nx_vk_write_vertex(nx_vk_vertex *vertex, double x, double y,
                                VkExtent2D extent, nx_color color) {
    vertex->x = (float)(x * 2.0 / extent.width - 1.0);
    vertex->y = (float)(y * 2.0 / extent.height - 1.0);
    vertex->rgba[0] = floorf(color.r * 255.0f + 0.5f) / 255.0f;
    vertex->rgba[1] = floorf(color.g * 255.0f + 0.5f) / 255.0f;
    vertex->rgba[2] = floorf(color.b * 255.0f + 0.5f) / 255.0f;
    vertex->rgba[3] = floorf(color.a * 255.0f + 0.5f) / 255.0f;
    memset(vertex->triangle, 0, sizeof(vertex->triangle));
    vertex->is_triangle = 0;
}

nx_result nx_vk_painter_prepare(nx_vk_painter *painter,
    const nx_vk_handles *native, VkFormat format, VkExtent2D extent,
    const nx_command *commands, size_t count, double scale) {
    painter->vertex_count = 0;
    painter->draw_count = 0;
    if (!count) return NX_OK;
    if (!painter->pipeline) {
        nx_result result = nx_vk_painter_create(painter, native, format, extent);
        if (result != NX_OK) return result;
    }
    if (count > painter->draw_capacity) {
        size_t capacity = painter->draw_capacity ? painter->draw_capacity : 64u;
        while (capacity < count) {
            if (capacity > SIZE_MAX / (2u * sizeof(nx_vk_draw))) { capacity = count; break; }
            capacity *= 2u;
        }
#ifdef NEXAUI_VULKAN_TEST_HOOKS
        if (painter->fail_draw_memory) { painter->fail_draw_memory = 0; return NX_ERROR_MEMORY; }
#endif
        nx_vk_draw *draws = painter->allocator.allocate(painter->allocator.user, capacity * sizeof(*draws));
        if (!draws) return NX_ERROR_MEMORY;
        if (painter->draws) painter->allocator.deallocate(painter->allocator.user,
            painter->draws, painter->draw_capacity * sizeof(*draws));
        painter->draws = draws; painter->draw_capacity = capacity;
    }
    nx_result result = nx_vk_painter_reserve(painter, (VkDeviceSize)count * 6u * sizeof(nx_vk_vertex));
    if (result != NX_OK) return result;
    nx_vk_vertex *vertices = painter->mapped;
    VkRect2D clips[NX_MAX_CLIP_DEPTH + 1u];
    clips[0] = (VkRect2D){{0, 0}, extent};
    unsigned depth = 0;
    for (size_t i = 0; i < count; ++i) {
        const nx_command *command = &commands[i];
        if (command->type == NX_COMMAND_POP_CLIP) { --depth; continue; }
        double x0, y0, x1, y1;
        nx_point points[3] = {{0}};
        if (command->type != NX_COMMAND_FILL_TRIANGLE) {
            x0 = nx_vk_pixel_edge(command->rect.x, scale, extent.width);
            y0 = nx_vk_pixel_edge(command->rect.y, scale, extent.height);
            x1 = nx_vk_pixel_edge((double)command->rect.x + command->rect.width, scale, extent.width);
            y1 = nx_vk_pixel_edge((double)command->rect.y + command->rect.height, scale, extent.height);
        } else {
            for (unsigned j = 0; j < 3u; ++j)
                points[j] = (nx_point){(float)(command->points[j].x * scale),
                                       (float)(command->points[j].y * scale)};
            double ax = points[0].x, ay = points[0].y;
            double bx = points[1].x, by = points[1].y;
            double cx = points[2].x, cy = points[2].y;
            double orientation = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
            if (orientation == 0) continue;
            if (orientation < 0) { nx_point swap = points[1]; points[1] = points[2]; points[2] = swap; }
            x0 = nx_vk_pixel_edge(fmin(ax, fmin(bx, cx)), 1, extent.width);
            y0 = nx_vk_pixel_edge(fmin(ay, fmin(by, cy)), 1, extent.height);
            x1 = nx_vk_pixel_edge(fmax(ax, fmax(bx, cx)), 1, extent.width);
            y1 = nx_vk_pixel_edge(fmax(ay, fmax(by, cy)), 1, extent.height);
        }
        VkRect2D clip = clips[depth];
        x0 = fmax(x0, clip.offset.x); y0 = fmax(y0, clip.offset.y);
        x1 = fmin(x1, (double)clip.offset.x + clip.extent.width);
        y1 = fmin(y1, (double)clip.offset.y + clip.extent.height);
        VkRect2D scissor = {{(int32_t)x0, (int32_t)y0},
            {(uint32_t)fmax(0, x1 - x0), (uint32_t)fmax(0, y1 - y0)}};
        if (command->type == NX_COMMAND_PUSH_CLIP) { clips[++depth] = scissor; continue; }
        if (x0 >= x1 || y0 >= y1 || floorf(command->color.a * 255.0f + .5f) == 0) continue;
        nx_vk_draw *draw = &painter->draws[painter->draw_count++];
        *draw = (nx_vk_draw){scissor, painter->vertex_count, 6,
            floorf(command->color.a * 255.0f + .5f) < 255};
        const double positions[6][2] = {{x0,y0},{x1,y0},{x0,y1},{x1,y0},{x1,y1},{x0,y1}};
        for (unsigned j = 0; j < 6u; ++j) {
            nx_vk_vertex *vertex = &vertices[painter->vertex_count++];
            nx_vk_write_vertex(vertex, positions[j][0], positions[j][1], extent, command->color);
            if (command->type == NX_COMMAND_FILL_TRIANGLE) {
                memcpy(vertex->triangle, points, sizeof(points));
                vertex->is_triangle = 1;
            }
        }
    }
    return NX_OK;
}

static void nx_vk_painter_begin(nx_vk_painter *painter, VkCommandBuffer commands, VkExtent2D extent) {
    VkRenderPassBeginInfo begin = {0};
    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass = painter->render_pass;
    begin.framebuffer = painter->frame;
    begin.renderArea.extent = extent;
    vkCmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, painter->pipeline);
    vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, painter->layout,
        0, 1, &painter->descriptor, 0, NULL);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commands, 0, 1, &painter->vertices, &offset);
    VkViewport viewport = {0, 0, (float)extent.width, (float)extent.height, 0, 1};
    vkCmdSetViewport(commands, 0, 1, &viewport);
}

void nx_vk_painter_record(nx_vk_painter *painter, VkCommandBuffer commands, VkExtent2D extent) {
    VkImageMemoryBarrier history = {0};
    history.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    history.srcQueueFamilyIndex = history.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    history.image = painter->history;
    history.subresourceRange = (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (!painter->history_initialized) {
        history.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        history.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        history.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, NULL, 0, NULL, 1, &history);
        painter->history_initialized = 1;
    }
    int open = 0;
    for (size_t i = 0; i < painter->draw_count; ++i) {
        nx_vk_draw *draw = &painter->draws[i];
        if (draw->composite) {
            if (open) { vkCmdEndRenderPass(commands); open = 0; }
            VkImageMemoryBarrier barriers[2] = {history, history};
            barriers[0].image = painter->canvas;
            barriers[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barriers[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 2, barriers);
            VkImageCopy copy = {0};
            copy.srcSubresource = copy.dstSubresource = (VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.srcOffset = copy.dstOffset = (VkOffset3D){draw->scissor.offset.x, draw->scissor.offset.y, 0};
            copy.extent = (VkExtent3D){draw->scissor.extent.width, draw->scissor.extent.height, 1};
            vkCmdCopyImage(commands, painter->canvas, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                painter->history, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barriers[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, NULL, 0, NULL, 2, barriers);
        }
        if (!open) { nx_vk_painter_begin(painter, commands, extent); open = 1; }
        vkCmdSetScissor(commands, 0, 1, &draw->scissor);
        vkCmdDraw(commands, draw->vertex_count, 1, draw->first_vertex, 0);
    }
    if (open) vkCmdEndRenderPass(commands);
}
