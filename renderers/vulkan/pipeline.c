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
    return NX_ERROR_PLATFORM;
}

nx_result nx_vk_painter_preflight(const nx_command *commands, size_t count,
                                 double scale) {
    if (!isfinite(scale) || scale <= 0) return NX_ERROR_ARGUMENT;
    if (count > UINT32_MAX / 6u || count > SIZE_MAX / (6u * sizeof(nx_vk_vertex)))
        return NX_ERROR_LIMIT;
    nx_result result = nx_render_validate_commands(commands, count);
    if (result != NX_OK) return result;
    for (size_t i = 0; i < count; ++i) {
        const nx_command *command = &commands[i];
        if ((command->type != NX_COMMAND_FILL_RECT &&
             command->type != NX_COMMAND_FILL_TRIANGLE) || command->color.a != 1.0f)
            return NX_ERROR_UNSUPPORTED;
        /* Bound NDC conversion even at a one-pixel target. No float overflow
           or optional shaderFloat64 requirement is hidden in the pipeline. */
        double coords[6];
        if (command->type == NX_COMMAND_FILL_RECT) {
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
        for (uint32_t i = 0; i < NX_VK_PAINTER_IMAGES; ++i) {
            if (painter->frames[i]) vkDestroyFramebuffer(painter->device, painter->frames[i], NULL);
            if (painter->views[i]) vkDestroyImageView(painter->device, painter->views[i], NULL);
        }
        if (painter->pipeline) vkDestroyPipeline(painter->device, painter->pipeline, NULL);
        if (painter->layout) vkDestroyPipelineLayout(painter->device, painter->layout, NULL);
        if (painter->render_pass) vkDestroyRenderPass(painter->device, painter->render_pass, NULL);
    }
    uint64_t allocations = painter->allocations;
    *painter = (nx_vk_painter){0};
    painter->allocations = allocations;
}

static nx_result nx_vk_painter_create(nx_vk_painter *painter,
    const nx_vk_handles *native, VkFormat format, VkExtent2D extent,
    const VkImage *images, uint32_t count) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(native->physical, &properties);
    if (!count || count > NX_VK_PAINTER_IMAGES || !extent.width || !extent.height ||
        extent.width > INT32_MAX || extent.height > INT32_MAX ||
        extent.width > properties.limits.maxViewportDimensions[0] ||
        extent.height > properties.limits.maxViewportDimensions[1] ||
        extent.width > properties.limits.maxFramebufferWidth ||
        extent.height > properties.limits.maxFramebufferHeight ||
        (double)extent.width > properties.limits.viewportBoundsRange[1] ||
        (double)extent.height > properties.limits.viewportBoundsRange[1]) return NX_ERROR_LIMIT;
    painter->device = native->logical;
    painter->physical = native->physical;
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
    VkResult status = vkCreateRenderPass(native->logical, &pass, NULL, &painter->render_pass);
    if (status != VK_SUCCESS) { painter->render_pass = VK_NULL_HANDLE; goto fail; }
    VkPipelineLayoutCreateInfo layout = {0};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
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
    for (uint32_t i = 0; i < count; ++i) {
        VkImageViewCreateInfo view = {0};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image = images[i];
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = format;
        view.subresourceRange = (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        status = vkCreateImageView(native->logical, &view, NULL, &painter->views[i]);
        if (status != VK_SUCCESS) { painter->views[i] = VK_NULL_HANDLE; goto fail; }
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
        frame.pAttachments = &painter->views[i];
        frame.width = extent.width;
        frame.height = extent.height;
        status = vkCreateFramebuffer(native->logical, &frame, NULL, &painter->frames[i]);
        if (status != VK_SUCCESS) { painter->frames[i] = VK_NULL_HANDLE; goto fail; }
    }
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
    vertex->rgba[3] = 1.0f;
    memset(vertex->triangle, 0, sizeof(vertex->triangle));
    vertex->is_triangle = 0;
}

nx_result nx_vk_painter_prepare(nx_vk_painter *painter,
    const nx_vk_handles *native, VkFormat format, VkExtent2D extent,
    const VkImage *images, uint32_t image_count,
    const nx_command *commands, size_t count, double scale) {
    painter->vertex_count = 0;
    if (!count) return NX_OK;
    if (!painter->pipeline) {
        nx_result result = nx_vk_painter_create(painter, native, format, extent, images, image_count);
        if (result != NX_OK) return result;
    }
    nx_result result = nx_vk_painter_reserve(painter, (VkDeviceSize)count * 6u * sizeof(nx_vk_vertex));
    if (result != NX_OK) return result;
    nx_vk_vertex *vertices = painter->mapped;
    for (size_t i = 0; i < count; ++i) {
        const nx_command *command = &commands[i];
        if (command->type == NX_COMMAND_FILL_RECT) {
            double x0 = nx_vk_pixel_edge(command->rect.x, scale, extent.width);
            double y0 = nx_vk_pixel_edge(command->rect.y, scale, extent.height);
            double x1 = nx_vk_pixel_edge((double)command->rect.x + command->rect.width, scale, extent.width);
            double y1 = nx_vk_pixel_edge((double)command->rect.y + command->rect.height, scale, extent.height);
            if (x0 >= x1 || y0 >= y1) continue;
            const double positions[6][2] = {{x0,y0},{x1,y0},{x0,y1},{x1,y0},{x1,y1},{x0,y1}};
            for (unsigned j = 0; j < 6u; ++j)
                nx_vk_write_vertex(&vertices[painter->vertex_count++], positions[j][0], positions[j][1],
                                   extent, command->color);
        } else {
            nx_point points[3];
            for (unsigned j = 0; j < 3u; ++j)
                points[j] = (nx_point){(float)(command->points[j].x * scale),
                                       (float)(command->points[j].y * scale)};
            double ax = points[0].x, ay = points[0].y;
            double bx = points[1].x, by = points[1].y;
            double cx = points[2].x, cy = points[2].y;
            double orientation = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
            if (orientation == 0) continue;
            if (orientation < 0) { nx_point swap = points[1]; points[1] = points[2]; points[2] = swap; }
            double x0 = nx_vk_pixel_edge(fmin(ax, fmin(bx, cx)), 1, extent.width);
            double y0 = nx_vk_pixel_edge(fmin(ay, fmin(by, cy)), 1, extent.height);
            double x1 = nx_vk_pixel_edge(fmax(ax, fmax(bx, cx)), 1, extent.width);
            double y1 = nx_vk_pixel_edge(fmax(ay, fmax(by, cy)), 1, extent.height);
            if (x0 >= x1 || y0 >= y1) continue;
            const double positions[6][2] = {{x0,y0},{x1,y0},{x0,y1},{x1,y0},{x1,y1},{x0,y1}};
            for (unsigned j = 0; j < 6u; ++j) {
                nx_vk_vertex *vertex = &vertices[painter->vertex_count++];
                nx_vk_write_vertex(vertex, positions[j][0], positions[j][1], extent, command->color);
                memcpy(vertex->triangle, points, sizeof(points));
                vertex->is_triangle = 1;
            }
        }
    }
    return NX_OK;
}

void nx_vk_painter_record(nx_vk_painter *painter, VkCommandBuffer commands,
                          uint32_t index, VkExtent2D extent) {
    VkRenderPassBeginInfo begin = {0};
    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass = painter->render_pass;
    begin.framebuffer = painter->frames[index];
    begin.renderArea.extent = extent;
    vkCmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, painter->pipeline);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commands, 0, 1, &painter->vertices, &offset);
    VkViewport viewport = {0, 0, (float)extent.width, (float)extent.height, 0, 1};
    VkRect2D scissor = {{0, 0}, extent};
    vkCmdSetViewport(commands, 0, 1, &viewport);
    vkCmdSetScissor(commands, 0, 1, &scissor);
    vkCmdDraw(commands, painter->vertex_count, 1, 0, 0);
    vkCmdEndRenderPass(commands);
}
