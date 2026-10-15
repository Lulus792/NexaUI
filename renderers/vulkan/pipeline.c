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
    /* Matches shape.vert/frag: 0 rect, 1 triangle, 2 inside stroke,
       3 rounded fill, 4 round-capped line, 5 bitmap. Triangles carry three points;
       strokes/rounds carry origin, size, inset/radius; lines carry endpoints
       and half-thickness. All values are physical pixel coordinates. */
    nx_point geometry[3];
    uint32_t shape_kind;
    nx_point bitmap_size;
} nx_vk_vertex;

#define NX_VK_ATLAS_BYTES (64u * 1024u * 1024u)
#define NX_VK_BITMAP_LIMIT 4096u

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
            command->type != NX_COMMAND_STROKE_RECT && command->type != NX_COMMAND_FILL_ROUND_RECT &&
            command->type != NX_COMMAND_LINE && command->type != NX_COMMAND_PUSH_CLIP &&
            command->type != NX_COMMAND_BITMAP)
            return NX_ERROR_UNSUPPORTED;
        if (command->type == NX_COMMAND_BITMAP &&
            (command->bitmap.width > 16384u || command->bitmap.height > 16384u ||
             (uint64_t)command->bitmap.width * command->bitmap.height > NX_VK_ATLAS_BYTES / 4u))
            return NX_ERROR_LIMIT;
        /* Bound NDC conversion even at a one-pixel target. No float overflow
           or optional shaderFloat64 requirement is hidden in the pipeline. */
        double coords[8] = {0};
        if (command->type != NX_COMMAND_FILL_TRIANGLE && command->type != NX_COMMAND_LINE) {
            coords[0] = command->rect.x;
            coords[1] = command->rect.y;
            coords[2] = (double)command->rect.x + command->rect.width;
            coords[3] = (double)command->rect.y + command->rect.height;
            coords[4] = command->rect.width;
            coords[5] = command->rect.height;
        } else {
            unsigned points = command->type == NX_COMMAND_LINE ? 2u : 3u;
            for (unsigned j = 0; j < points; ++j) {
                coords[j * 2u] = command->points[j].x;
                coords[j * 2u + 1u] = command->points[j].y;
            }
        }
        if (command->type == NX_COMMAND_LINE || command->type == NX_COMMAND_STROKE_RECT)
            coords[6] = command->thickness;
        if (command->type == NX_COMMAND_FILL_ROUND_RECT) coords[7] = command->radius;
        for (unsigned j = 0; j < 8u; ++j) {
            /* Fields irrelevant to the command may contain arbitrary values. */
            if (j == 6u && command->type != NX_COMMAND_LINE && command->type != NX_COMMAND_STROKE_RECT) continue;
            if (j == 7u && command->type != NX_COMMAND_FILL_ROUND_RECT) continue;
            double scaled = coords[j] * scale * 2.0;
            if (!isfinite(scaled) || fabs(scaled) > (double)FLT_MAX / 2.0)
                return NX_ERROR_LIMIT;
            /* Edge products need finite float32 headroom even when the
               triangle is mostly outside the visible target. */
            if (command->type == NX_COMMAND_FILL_TRIANGLE &&
                fabs(scaled) > sqrt((double)FLT_MAX) / 4.0) return NX_ERROR_LIMIT;
            if (command->type == NX_COMMAND_LINE &&
                fabs(scaled) > sqrt(sqrt((double)FLT_MAX)) / 16.0) return NX_ERROR_LIMIT;
            if (command->type == NX_COMMAND_FILL_ROUND_RECT &&
                fabs(scaled) > sqrt((double)FLT_MAX) / 8.0) return NX_ERROR_LIMIT;
            if (command->type == NX_COMMAND_BITMAP && fabs(scaled) > sqrt((double)FLT_MAX) / 32768.0)
                return NX_ERROR_LIMIT;
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
        if (painter->bitmap_mapped) vkUnmapMemory(painter->device, painter->bitmap_memory);
        if (painter->bitmap_upload) vkDestroyBuffer(painter->device, painter->bitmap_upload, NULL);
        if (painter->bitmap_memory) vkFreeMemory(painter->device, painter->bitmap_memory, NULL);
        if (painter->frame) vkDestroyFramebuffer(painter->device, painter->frame, NULL);
        if (painter->pipeline) vkDestroyPipeline(painter->device, painter->pipeline, NULL);
        if (painter->layout) vkDestroyPipelineLayout(painter->device, painter->layout, NULL);
        if (painter->render_pass) vkDestroyRenderPass(painter->device, painter->render_pass, NULL);
        if (painter->descriptor_pool) vkDestroyDescriptorPool(painter->device, painter->descriptor_pool, NULL);
        if (painter->descriptor_layout) vkDestroyDescriptorSetLayout(painter->device, painter->descriptor_layout, NULL);
        if (painter->sampler) vkDestroySampler(painter->device, painter->sampler, NULL);
        if (painter->canvas_view) vkDestroyImageView(painter->device, painter->canvas_view, NULL);
        if (painter->history_view) vkDestroyImageView(painter->device, painter->history_view, NULL);
        if (painter->atlas_view) vkDestroyImageView(painter->device, painter->atlas_view, NULL);
        if (painter->canvas) vkDestroyImage(painter->device, painter->canvas, NULL);
        if (painter->history) vkDestroyImage(painter->device, painter->history, NULL);
        if (painter->atlas) vkDestroyImage(painter->device, painter->atlas, NULL);
        if (painter->canvas_memory) vkFreeMemory(painter->device, painter->canvas_memory, NULL);
        if (painter->history_memory) vkFreeMemory(painter->device, painter->history_memory, NULL);
        if (painter->atlas_memory) vkFreeMemory(painter->device, painter->atlas_memory, NULL);
    }
    if (painter->draws) painter->allocator.deallocate(painter->allocator.user,
        painter->draws, painter->draw_capacity * sizeof(*painter->draws));
    if (painter->bitmaps) painter->allocator.deallocate(painter->allocator.user,
        painter->bitmaps, painter->bitmap_record_capacity * sizeof(*painter->bitmaps));
    nx_allocator allocator = painter->allocator;
    uint64_t allocations = painter->allocations;
    uint64_t atlas_allocations = painter->atlas_allocations, bitmap_allocations = painter->bitmap_allocations;
    *painter = (nx_vk_painter){0};
    painter->allocations = allocations;
    painter->atlas_allocations = atlas_allocations;
    painter->bitmap_allocations = bitmap_allocations;
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

static void nx_vk_atlas_descriptor(nx_vk_painter *painter) {
    VkDescriptorImageInfo image = {painter->sampler, painter->atlas_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write = {0}; write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = painter->descriptor; write.dstBinding = 1; write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo = &image;
    vkUpdateDescriptorSets(painter->device, 1, &write, 0, NULL);
}

static nx_result nx_vk_atlas_reserve(nx_vk_painter *painter, VkExtent2D extent) {
    if (extent.width <= painter->atlas_extent.width && extent.height <= painter->atlas_extent.height) return NX_OK;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkResult status;
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    if (painter->fail_atlas_memory) { painter->fail_atlas_memory = 0; return NX_ERROR_MEMORY; }
#endif
    status = nx_vk_painter_image(painter, VK_FORMAT_R8G8B8A8_UNORM, extent,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &image, &memory, &view);
    if (status != VK_SUCCESS) {
        if (view) vkDestroyImageView(painter->device, view, NULL);
        if (image) vkDestroyImage(painter->device, image, NULL);
        if (memory) vkFreeMemory(painter->device, memory, NULL);
        return nx_vk_pipeline_status(status);
    }
    if (painter->atlas_view) vkDestroyImageView(painter->device, painter->atlas_view, NULL);
    if (painter->atlas) vkDestroyImage(painter->device, painter->atlas, NULL);
    if (painter->atlas_memory) vkFreeMemory(painter->device, painter->atlas_memory, NULL);
    painter->atlas = image; painter->atlas_memory = memory; painter->atlas_view = view;
    painter->atlas_extent = extent; painter->atlas_initialized = 0;
    ++painter->atlas_allocations;
    nx_vk_atlas_descriptor(painter);
    return NX_OK;
}

static nx_result nx_vk_bitmap_upload_reserve(nx_vk_painter *painter, VkDeviceSize bytes) {
    if (bytes <= painter->bitmap_capacity) return NX_OK;
    VkDeviceSize capacity = painter->bitmap_capacity ? painter->bitmap_capacity : 4096u;
    while (capacity < bytes) capacity *= 2u; /* bytes is bounded by NX_VK_ATLAS_BYTES. */
#ifdef NEXAUI_VULKAN_TEST_HOOKS
    if (painter->fail_bitmap_upload) { painter->fail_bitmap_upload = 0; return NX_ERROR_MEMORY; }
#endif
    VkBuffer buffer = VK_NULL_HANDLE; VkDeviceMemory memory = VK_NULL_HANDLE;
    VkBufferCreateInfo create = {0}; create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create.size = capacity; create.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult status = vkCreateBuffer(painter->device, &create, NULL, &buffer);
    if (status != VK_SUCCESS) return nx_vk_pipeline_status(status);
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(painter->device, buffer, &requirements);
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(painter->physical, &properties);
    uint32_t type = UINT32_MAX;
    VkMemoryPropertyFlags required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
        if ((requirements.memoryTypeBits & (1u << i)) &&
            (properties.memoryTypes[i].propertyFlags & required) == required) { type = i; break; }
    if (type == UINT32_MAX) { vkDestroyBuffer(painter->device, buffer, NULL); return NX_ERROR_UNSUPPORTED; }
    VkMemoryAllocateInfo allocate = {0}; allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize = requirements.size; allocate.memoryTypeIndex = type;
    status = vkAllocateMemory(painter->device, &allocate, NULL, &memory);
    if (status != VK_SUCCESS) { memory = VK_NULL_HANDLE; goto fail; }
    status = vkBindBufferMemory(painter->device, buffer, memory, 0);
    if (status != VK_SUCCESS) goto fail;
    void *mapped = NULL;
    status = vkMapMemory(painter->device, memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    if (status != VK_SUCCESS) goto fail;
    if (painter->bitmap_mapped) vkUnmapMemory(painter->device, painter->bitmap_memory);
    if (painter->bitmap_upload) vkDestroyBuffer(painter->device, painter->bitmap_upload, NULL);
    if (painter->bitmap_memory) vkFreeMemory(painter->device, painter->bitmap_memory, NULL);
    painter->bitmap_upload = buffer; painter->bitmap_memory = memory;
    painter->bitmap_mapped = mapped; painter->bitmap_capacity = capacity;
    ++painter->bitmap_allocations;
    return NX_OK;
fail:
    vkDestroyBuffer(painter->device, buffer, NULL);
    if (memory) vkFreeMemory(painter->device, memory, NULL);
    return nx_vk_pipeline_status(status);
}

static int nx_vk_same_bitmap(nx_bitmap_view a, nx_bitmap_view b) {
    return a.pixels == b.pixels && a.width == b.width && a.height == b.height &&
           a.stride_bytes == b.stride_bytes && a.format == b.format;
}

static uint32_t nx_vk_grow_axis(uint32_t need, uint32_t previous, uint32_t limit) {
    uint32_t size = previous ? previous : 1;
    while (size < need) size = size > limit / 2u ? limit : size * 2u;
    return size;
}

static nx_result nx_vk_bitmaps_prepare(nx_vk_painter *painter, const nx_command *commands, size_t count) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(painter->physical, &properties);
    uint32_t limit = properties.limits.maxImageDimension2D;
    if (limit > 16384u) limit = 16384u;
    uint32_t width = painter->atlas_extent.width > 1 ? painter->atlas_extent.width : (limit < 1024u ? limit : 1024u);
    for (size_t i = 0; i < count; ++i) {
        if (commands[i].type != NX_COMMAND_BITMAP || commands[i].rect.width == 0 ||
            commands[i].rect.height == 0 || floorf(commands[i].color.a * 255.f + .5f) == 0) continue;
        nx_bitmap_view view = commands[i].bitmap;
        if (view.width > limit || view.height > limit) return NX_ERROR_LIMIT;
        size_t j = 0;
        while (j < painter->bitmap_count && !nx_vk_same_bitmap(painter->bitmaps[j].view, view)) ++j;
        if (j != painter->bitmap_count) continue;
        if (j == NX_VK_BITMAP_LIMIT) return NX_ERROR_LIMIT;
        if (j == painter->bitmap_record_capacity) {
            size_t capacity = painter->bitmap_record_capacity ? painter->bitmap_record_capacity * 2u : 32u;
#ifdef NEXAUI_VULKAN_TEST_HOOKS
            if (painter->fail_bitmap_records) { painter->fail_bitmap_records = 0; return NX_ERROR_MEMORY; }
#endif
            nx_vk_bitmap *bitmaps = painter->allocator.allocate(painter->allocator.user, capacity * sizeof(*bitmaps));
            if (!bitmaps) return NX_ERROR_MEMORY;
            if (j) memcpy(bitmaps, painter->bitmaps, j * sizeof(*bitmaps));
            if (painter->bitmaps) painter->allocator.deallocate(painter->allocator.user,
                painter->bitmaps, painter->bitmap_record_capacity * sizeof(*bitmaps));
            painter->bitmaps = bitmaps; painter->bitmap_record_capacity = capacity;
        }
        painter->bitmaps[painter->bitmap_count++] = (nx_vk_bitmap){view, 0, 0};
        if (view.width > width) width = nx_vk_grow_axis(view.width, width, limit);
    }
    if (!painter->bitmap_count) return NX_OK;
    uint32_t height = 0;
    /* Widen and retry the complete shelf pack if the height limit is hit. */
    for (;;) {
        uint32_t x = 0, y = 0, row = 0;
        for (size_t i = 0; i < painter->bitmap_count; ++i) {
            nx_vk_bitmap *entry = &painter->bitmaps[i];
            if (entry->view.width > width - x) { x = 0; y += row; row = 0; }
            entry->x = x; entry->y = y; x += entry->view.width;
            if (entry->view.height > row) row = entry->view.height;
        }
        height = y + row; /* At most 4096 * 16384; uint32_t cannot wrap. */
        if (height <= limit) break;
        if (width == limit) return NX_ERROR_LIMIT;
        width = nx_vk_grow_axis(width + 1u, width, limit);
    }
    VkExtent2D extent = {width, nx_vk_grow_axis(height, painter->atlas_extent.height, limit)};
    if ((uint64_t)extent.width * extent.height > NX_VK_ATLAS_BYTES / 4u) return NX_ERROR_LIMIT;
    nx_result result = nx_vk_atlas_reserve(painter, extent);
    if (result != NX_OK) return result;
    result = nx_vk_bitmap_upload_reserve(painter, (VkDeviceSize)width * height * 4u);
    if (result != NX_OK) return result;
    painter->bitmap_extent = (VkExtent2D){width, height};
    size_t stride = (size_t)width * 4u;
    memset(painter->bitmap_mapped, 0, stride * height);
    for (size_t i = 0; i < painter->bitmap_count; ++i) {
        nx_vk_bitmap *entry = &painter->bitmaps[i];
        for (uint32_t y = 0; y < entry->view.height; ++y) {
            uint8_t *dst = (uint8_t *)painter->bitmap_mapped + (size_t)(entry->y + y) * stride + entry->x * 4u;
            const uint8_t *src = entry->view.pixels + (size_t)y * entry->view.stride_bytes;
            if (entry->view.format == NX_BITMAP_RGBA8) memcpy(dst, src, (size_t)entry->view.width * 4u);
            else for (uint32_t x = 0; x < entry->view.width; ++x) {
                dst[x * 4u] = dst[x * 4u + 1u] = dst[x * 4u + 2u] = 255;
                dst[x * 4u + 3u] = src[x];
            }
        }
    }
    return NX_OK;
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
    VkFormatProperties atlas_format;
    vkGetPhysicalDeviceFormatProperties(native->physical, VK_FORMAT_R8G8B8A8_UNORM, &atlas_format);
    if (!(atlas_format.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
        status = VK_ERROR_FORMAT_NOT_SUPPORTED; goto fail;
    }
    /* A valid descriptor is required even for shaders taking a shape branch. */
    status = nx_vk_painter_image(painter, VK_FORMAT_R8G8B8A8_UNORM, (VkExtent2D){1, 1},
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        &painter->atlas, &painter->atlas_memory, &painter->atlas_view);
    if (status != VK_SUCCESS) goto fail;
    painter->atlas_extent = (VkExtent2D){1, 1}; ++painter->atlas_allocations;
    VkSamplerCreateInfo sampler = {0}; sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter = sampler.minFilter = VK_FILTER_NEAREST;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    status = vkCreateSampler(native->logical, &sampler, NULL, &painter->sampler);
    if (status != VK_SUCCESS) { painter->sampler = VK_NULL_HANDLE; goto fail; }
    VkDescriptorSetLayoutBinding binding_info[2] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL}};
    VkDescriptorSetLayoutCreateInfo descriptor_layout = {0};
    descriptor_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_layout.bindingCount = 2; descriptor_layout.pBindings = binding_info;
    status = vkCreateDescriptorSetLayout(native->logical, &descriptor_layout, NULL, &painter->descriptor_layout);
    if (status != VK_SUCCESS) { painter->descriptor_layout = VK_NULL_HANDLE; goto fail; }
    VkDescriptorPoolSize pool_size = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2};
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
    nx_vk_atlas_descriptor(painter);
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
    VkVertexInputAttributeDescription attributes[7] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(nx_vk_vertex, x)},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(nx_vk_vertex, rgba)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(nx_vk_vertex, geometry[0])},
        {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(nx_vk_vertex, geometry[1])},
        {4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(nx_vk_vertex, geometry[2])},
        {5, 0, VK_FORMAT_R32_UINT, offsetof(nx_vk_vertex, shape_kind)},
        {6, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(nx_vk_vertex, bitmap_size)}
    };
    VkPipelineVertexInputStateCreateInfo vertex = {0};
    vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex.vertexBindingDescriptionCount = 1;
    vertex.pVertexBindingDescriptions = &binding;
    vertex.vertexAttributeDescriptionCount = 7;
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
    memset(vertex->geometry, 0, sizeof(vertex->geometry));
    vertex->shape_kind = 0;
    vertex->bitmap_size = (nx_point){0};
}

nx_result nx_vk_painter_prepare(nx_vk_painter *painter,
    const nx_vk_handles *native, VkFormat format, VkExtent2D extent,
    const nx_command *commands, size_t count, double scale) {
    painter->vertex_count = 0;
    painter->recorded_draws = painter->recorded_passes = painter->recorded_history_copies = painter->recorded_history_bytes = 0;
    painter->vertex_upload_bytes = 0;
    painter->draw_count = 0;
    painter->bitmap_count = 0;
    painter->bitmap_extent = (VkExtent2D){0};
    if (!count) return NX_OK;
    if (!painter->pipeline) {
        nx_result result = nx_vk_painter_create(painter, native, format, extent);
        if (result != NX_OK) return result;
    }
    nx_result result = nx_vk_bitmaps_prepare(painter, commands, count);
    if (result != NX_OK) return result;
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
    result = nx_vk_painter_reserve(painter, (VkDeviceSize)count * 6u * sizeof(nx_vk_vertex));
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
        nx_point bitmap_size = {0};
        uint32_t kind = 0;
        if (command->type == NX_COMMAND_LINE) {
            kind = 4;
            for (unsigned j = 0; j < 2; ++j)
                points[j] = (nx_point){(float)(command->points[j].x * scale), (float)(command->points[j].y * scale)};
            points[2].x = (float)(command->thickness * scale) * .5f;
            x0 = nx_vk_pixel_edge(fmin(points[0].x, points[1].x) - points[2].x, 1, extent.width);
            y0 = nx_vk_pixel_edge(fmin(points[0].y, points[1].y) - points[2].x, 1, extent.height);
            x1 = nx_vk_pixel_edge(fmax(points[0].x, points[1].x) + points[2].x, 1, extent.width);
            y1 = nx_vk_pixel_edge(fmax(points[0].y, points[1].y) + points[2].x, 1, extent.height);
        } else if (command->type == NX_COMMAND_STROKE_RECT || command->type == NX_COMMAND_FILL_ROUND_RECT ||
                   command->type == NX_COMMAND_BITMAP) {
            kind = command->type == NX_COMMAND_STROKE_RECT ? 2u : command->type == NX_COMMAND_FILL_ROUND_RECT ? 3u : 5u;
            points[0] = (nx_point){(float)(command->rect.x * scale), (float)(command->rect.y * scale)};
            points[1] = (nx_point){(float)(command->rect.width * scale), (float)(command->rect.height * scale)};
            if (kind == 5) {
                if (!points[1].x || !points[1].y || floorf(command->color.a * 255.f + .5f) == 0) continue;
                size_t j = 0;
                while (j < painter->bitmap_count && !nx_vk_same_bitmap(command->bitmap, painter->bitmaps[j].view)) ++j;
                if (j == painter->bitmap_count) return NX_ERROR_STATE;
                points[2] = (nx_point){(float)painter->bitmaps[j].x, (float)painter->bitmaps[j].y};
                bitmap_size = (nx_point){(float)command->bitmap.width, (float)command->bitmap.height};
            } else points[2].x = kind == 2 ? (float)(command->thickness * scale) :
                fminf((float)(command->radius * scale), fminf(points[1].x, points[1].y) * .5f);
            x0 = nx_vk_pixel_edge(points[0].x, 1, extent.width);
            y0 = nx_vk_pixel_edge(points[0].y, 1, extent.height);
            x1 = nx_vk_pixel_edge((double)points[0].x + points[1].x, 1, extent.width);
            y1 = nx_vk_pixel_edge((double)points[0].y + points[1].y, 1, extent.height);
        } else if (command->type != NX_COMMAND_FILL_TRIANGLE) {
            x0 = nx_vk_pixel_edge(command->rect.x, scale, extent.width);
            y0 = nx_vk_pixel_edge(command->rect.y, scale, extent.height);
            x1 = nx_vk_pixel_edge((double)command->rect.x + command->rect.width, scale, extent.width);
            y1 = nx_vk_pixel_edge((double)command->rect.y + command->rect.height, scale, extent.height);
        } else {
            kind = 1;
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
            kind == 5 || floorf(command->color.a * 255.0f + .5f) < 255};
        const double positions[6][2] = {{x0,y0},{x1,y0},{x0,y1},{x1,y0},{x1,y1},{x0,y1}};
        for (unsigned j = 0; j < 6u; ++j) {
            nx_vk_vertex *vertex = &vertices[painter->vertex_count++];
            nx_vk_write_vertex(vertex, positions[j][0], positions[j][1], extent, command->color);
            if (kind) {
                memcpy(vertex->geometry, points, sizeof(points));
                vertex->shape_kind = kind;
                vertex->bitmap_size = bitmap_size;
            }
        }
    }
    painter->vertex_upload_bytes = (uint64_t)painter->vertex_count * sizeof(nx_vk_vertex);
    return NX_OK;
}

static void nx_vk_painter_begin(nx_vk_painter *painter, VkCommandBuffer commands, VkExtent2D extent) {
    ++painter->recorded_passes;
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
    VkImageMemoryBarrier atlas = {0}; atlas.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    atlas.srcQueueFamilyIndex = atlas.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    atlas.image = painter->atlas;
    atlas.subresourceRange = (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    atlas.oldLayout = painter->atlas_initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    atlas.srcAccessMask = painter->atlas_initialized ? VK_ACCESS_SHADER_READ_BIT : 0;
    VkPipelineStageFlags atlas_stage = painter->atlas_initialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (painter->bitmap_extent.width) {
        atlas.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        atlas.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(commands, atlas_stage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &atlas);
        VkBufferImageCopy copy = {0};
        copy.imageSubresource = (VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = (VkExtent3D){painter->bitmap_extent.width, painter->bitmap_extent.height, 1};
        vkCmdCopyBufferToImage(commands, painter->bitmap_upload, painter->atlas,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        atlas.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        atlas.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        atlas_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    if (painter->bitmap_extent.width || !painter->atlas_initialized) {
        atlas.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        atlas.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commands, atlas_stage, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &atlas);
        painter->atlas_initialized = 1;
    }
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
            ++painter->recorded_history_copies;
            painter->recorded_history_bytes += (uint64_t)draw->scissor.extent.width * draw->scissor.extent.height * 4u;
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
        ++painter->recorded_draws;
    }
    if (open) vkCmdEndRenderPass(commands);
}
