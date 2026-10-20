/* Standalone API/lifetime diagnostic: links Vulkan and dl, never NexaUI.
   --device performs ordinary creation/destruction; --draw additionally renders
   and checks a complete16x16 RGBA buffer. All objects are explicitly destroyed.
   Module snapshots observe unload behavior without retaining loader handles. */
#define _GNU_SOURCE
#include <vulkan/vulkan.h>
#include <elf.h>
#include <link.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "vulkan_lifetime_shaders.h"

static unsigned validation_errors;
static VKAPI_ATTR VkBool32 VKAPI_CALL message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) {
    (void)type; (void)user;
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++validation_errors;
    fprintf(stderr, "Vulkan control validation: %s\n", data->pMessage);
    return VK_FALSE;
}

static int module(struct dl_phdr_info *info, size_t size, void *user) {
    (void)size;
    if (!info->dlpi_name || !strstr(info->dlpi_name, ".so") || (!strstr(info->dlpi_name, "vulkan") &&
        !strstr(info->dlpi_name, "VkLayer"))) return 0;
    uintptr_t extent = 0;
    const unsigned char *id = NULL; size_t id_size = 0;
    for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr) *p = &info->dlpi_phdr[i];
        if (p->p_type == PT_LOAD && p->p_vaddr + p->p_memsz > extent)
            extent = p->p_vaddr + p->p_memsz;
        if (p->p_type != PT_NOTE) continue;
        const unsigned char *bytes = (const unsigned char *)(info->dlpi_addr + p->p_vaddr);
        size_t length = (size_t)p->p_memsz, offset = 0;
        while (offset <= length && length - offset >= sizeof(ElfW(Nhdr))) {
            ElfW(Nhdr) note; memcpy(&note, bytes + offset, sizeof(note));
            offset += sizeof(note);
            size_t names = ((size_t)note.n_namesz + 3u) & ~(size_t)3u;
            size_t desc = ((size_t)note.n_descsz + 3u) & ~(size_t)3u;
            if (names > length - offset || desc > length - offset - names) break;
            if (note.n_type == NT_GNU_BUILD_ID && note.n_namesz == 4 &&
                memcmp(bytes + offset, "GNU", 4) == 0) {
                id = bytes + offset + names; id_size = note.n_descsz;
            }
            offset += names + desc;
        }
    }
    printf("MODULE\t%s\t%s\t0x%" PRIxPTR "\t0x%" PRIxPTR "\t",
        (const char *)user, info->dlpi_name, (uintptr_t)info->dlpi_addr, extent);
    for (size_t i = 0; i < id_size; ++i) printf("%02x", id[i]);
    putchar('\n');
    return 0;
}
static void snapshot(const char *phase) {
    printf("SNAPSHOT\t%s\n", phase);
    dl_iterate_phdr(module, (void *)phase);
    fflush(stdout);
}
static uint32_t memory_type(VkPhysicalDevice physical, uint32_t bits,
    VkMemoryPropertyFlags wanted) {
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(physical, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (props.memoryTypes[i].propertyFlags & wanted) == wanted)
            return i;
    return UINT32_MAX;
}
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "control:%d: %s\n", __LINE__, #x); goto cleanup; } } while (0)
#define CALL(x) do { VkResult status_ = (x); if (status_ != VK_SUCCESS) { \
    fprintf(stderr, "control:%d: %s returned%d\n", __LINE__, #x, (int)status_); goto cleanup; } } while (0)

int main(int argc, char **argv) {
    if (argc != 2 || (strcmp(argv[1], "--device") && strcmp(argv[1], "--draw"))) {
        fputs("usage: nexaui_vulkan_lifetime_control --device|--draw\n", stderr); return 2;
    }
    int draw = strcmp(argv[1], "--draw") == 0, okay = 0;
    VkInstance instance = VK_NULL_HANDLE; VkDevice device = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug = NULL;
    VkImage image = VK_NULL_HANDLE; VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory image_memory = VK_NULL_HANDLE, buffer_memory = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE; VkRenderPass render_pass = VK_NULL_HANDLE;
    VkBuffer vertex_buffer = VK_NULL_HANDLE; VkDeviceMemory vertex_memory = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE; VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE; VkShaderModule vertex = VK_NULL_HANDLE, fragment = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE; VkFence fence = VK_NULL_HANDLE;
    void *mapped = NULL;
    VkApplicationInfo app = {0}; app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "NexaUI independent Vulkan lifetime control"; app.apiVersion = VK_API_VERSION_1_0;
    VkDebugUtilsMessengerCreateInfoEXT debug = {0}; debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    debug.pfnUserCallback = message;
    const char *extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    VkInstanceCreateInfo create = {0}; create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create.pApplicationInfo = &app; create.pNext = &debug;
    create.enabledExtensionCount = 1; create.ppEnabledExtensionNames = &extension;
    CALL(vkCreateInstance(&create, NULL, &instance));
    PFN_vkCreateDebugUtilsMessengerEXT create_debug =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    destroy_debug = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    CHECK(create_debug && destroy_debug); CALL(create_debug(instance, &debug, NULL, &messenger));
    uint32_t count = 0; CALL(vkEnumeratePhysicalDevices(instance, &count, NULL));
    CHECK(count > 0 && count <= 32);
    VkPhysicalDevice devices[32]; CALL(vkEnumeratePhysicalDevices(instance, &count, devices));
    VkPhysicalDevice physical = devices[0]; VkPhysicalDeviceProperties properties;
    for (uint32_t i = 0; i < count; ++i) {
        vkGetPhysicalDeviceProperties(devices[i], &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) { physical = devices[i]; break; }
    }
    vkGetPhysicalDeviceProperties(physical, &properties);
    printf("DEVICE\t%s\t%u\t%u\n", properties.deviceName, properties.vendorID, properties.driverVersion);
    count = 0; vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, NULL);
    CHECK(count > 0 && count <= 64);
    VkQueueFamilyProperties families[64]; vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families);
    uint32_t family = 0;
    while (family < count && (!families[family].queueCount || !(families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT))) ++family;
    CHECK(family < count);
    float priority = 1; VkDeviceQueueCreateInfo queue_create = {0}; queue_create.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create.queueFamilyIndex = family; queue_create.queueCount = 1; queue_create.pQueuePriorities = &priority;
    VkDeviceCreateInfo device_create = {0}; device_create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create.queueCreateInfoCount = 1; device_create.pQueueCreateInfos = &queue_create;
    CALL(vkCreateDevice(physical, &device_create, NULL, &device));
    snapshot("device-created");
    if (draw) {
        VkQueue queue; vkGetDeviceQueue(device, family, 0, &queue);
        VkImageCreateInfo image_create = {0}; image_create.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_create.imageType = VK_IMAGE_TYPE_2D; image_create.format = VK_FORMAT_R8G8B8A8_UNORM;
        image_create.extent = (VkExtent3D){16,16,1}; image_create.mipLevels = image_create.arrayLayers = 1;
        image_create.samples = VK_SAMPLE_COUNT_1_BIT; image_create.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_create.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        CALL(vkCreateImage(device, &image_create, NULL, &image));
        VkMemoryRequirements requirements; vkGetImageMemoryRequirements(device, image, &requirements);
        VkMemoryAllocateInfo allocation = {0}; allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocation.allocationSize = requirements.size; allocation.memoryTypeIndex = memory_type(physical, requirements.memoryTypeBits, 0);
        CHECK(allocation.memoryTypeIndex != UINT32_MAX); CALL(vkAllocateMemory(device, &allocation, NULL, &image_memory));
        CALL(vkBindImageMemory(device, image, image_memory, 0));
        VkImageViewCreateInfo view_create = {0}; view_create.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_create.image = image; view_create.viewType = VK_IMAGE_VIEW_TYPE_2D; view_create.format = image_create.format;
        view_create.subresourceRange = (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        CALL(vkCreateImageView(device, &view_create, NULL, &view));
        VkAttachmentDescription attachment = {0}; attachment.format = image_create.format; attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        VkAttachmentReference reference = {0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass = {0}; subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &reference;
        VkSubpassDependency dependency = {0}; dependency.srcSubpass = 0; dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; dependency.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; dependency.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        VkRenderPassCreateInfo pass_create = {0}; pass_create.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        pass_create.attachmentCount = 1; pass_create.pAttachments = &attachment; pass_create.subpassCount = 1; pass_create.pSubpasses = &subpass;
        pass_create.dependencyCount = 1; pass_create.pDependencies = &dependency; CALL(vkCreateRenderPass(device, &pass_create, NULL, &render_pass));
        VkFramebufferCreateInfo fb = {0}; fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb.renderPass = render_pass; fb.attachmentCount = 1; fb.pAttachments = &view; fb.width = fb.height = 16; fb.layers = 1;
        CALL(vkCreateFramebuffer(device, &fb, NULL, &framebuffer));
        VkShaderModuleCreateInfo shader = {0}; shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader.codeSize = sizeof(lifetime_vert); shader.pCode = lifetime_vert; CALL(vkCreateShaderModule(device, &shader, NULL, &vertex));
        shader.codeSize = sizeof(lifetime_frag); shader.pCode = lifetime_frag; CALL(vkCreateShaderModule(device, &shader, NULL, &fragment));
        VkPipelineShaderStageCreateInfo stages[2] = {{0},{0}};
        for (unsigned i = 0; i < 2; ++i) { stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[i].pName = "main"; }
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vertex;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fragment;
        VkVertexInputBindingDescription binding = {0,6*sizeof(float),VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attributes[2] = {{0,0,VK_FORMAT_R32G32_SFLOAT,0},{1,0,VK_FORMAT_R32G32B32A32_SFLOAT,2*sizeof(float)}};
        VkPipelineVertexInputStateCreateInfo input = {0}; input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        input.vertexBindingDescriptionCount = 1; input.pVertexBindingDescriptions = &binding;
        input.vertexAttributeDescriptionCount = 2; input.pVertexAttributeDescriptions = attributes;
        VkPipelineInputAssemblyStateCreateInfo assembly = {0}; assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO; assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport = {0,0,16,16,0,1}; VkRect2D scissor = {{0,0},{16,16}};
        VkPipelineViewportStateCreateInfo viewport_state = {0}; viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = viewport_state.scissorCount = 1; viewport_state.pViewports = &viewport; viewport_state.pScissors = &scissor;
        VkPipelineRasterizationStateCreateInfo raster = {0}; raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL; raster.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo samples = {0}; samples.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO; samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState color = {0}; color.colorWriteMask = 15;
        VkPipelineColorBlendStateCreateInfo blend = {0}; blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO; blend.attachmentCount = 1; blend.pAttachments = &color;
        VkPipelineLayoutCreateInfo layout_create = {0}; layout_create.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        CALL(vkCreatePipelineLayout(device, &layout_create, NULL, &layout));
        VkGraphicsPipelineCreateInfo pipeline_create = {0}; pipeline_create.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_create.stageCount = 2; pipeline_create.pStages = stages; pipeline_create.pVertexInputState = &input;
        pipeline_create.pInputAssemblyState = &assembly; pipeline_create.pViewportState = &viewport_state;
        pipeline_create.pRasterizationState = &raster; pipeline_create.pMultisampleState = &samples; pipeline_create.pColorBlendState = &blend;
        pipeline_create.layout = layout; pipeline_create.renderPass = render_pass;
        CALL(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create, NULL, &pipeline));
        VkBufferCreateInfo buffer_create = {0}; buffer_create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; buffer_create.size = 16*16*4; buffer_create.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        CALL(vkCreateBuffer(device, &buffer_create, NULL, &buffer)); vkGetBufferMemoryRequirements(device, buffer, &requirements);
        allocation.allocationSize = requirements.size; allocation.memoryTypeIndex = memory_type(physical, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        CHECK(allocation.memoryTypeIndex != UINT32_MAX); CALL(vkAllocateMemory(device, &allocation, NULL, &buffer_memory)); CALL(vkBindBufferMemory(device, buffer, buffer_memory, 0));
        const float vertices[18] = {-1,-1,1,0,0,1, 3,-1,1,0,0,1, -1,3,1,0,0,1};
        buffer_create.size = sizeof(vertices); buffer_create.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        CALL(vkCreateBuffer(device, &buffer_create, NULL, &vertex_buffer)); vkGetBufferMemoryRequirements(device, vertex_buffer, &requirements);
        allocation.allocationSize = requirements.size; allocation.memoryTypeIndex = memory_type(physical, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        CHECK(allocation.memoryTypeIndex != UINT32_MAX); CALL(vkAllocateMemory(device, &allocation, NULL, &vertex_memory)); CALL(vkBindBufferMemory(device, vertex_buffer, vertex_memory, 0));
        void *vertex_map = NULL; CALL(vkMapMemory(device, vertex_memory, 0, VK_WHOLE_SIZE, 0, &vertex_map));
        memcpy(vertex_map, vertices, sizeof(vertices)); vkUnmapMemory(device, vertex_memory);
        VkCommandPoolCreateInfo pool_create = {0}; pool_create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; pool_create.queueFamilyIndex = family;
        CALL(vkCreateCommandPool(device, &pool_create, NULL, &pool));
        VkCommandBufferAllocateInfo command_create = {0}; command_create.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_create.commandPool = pool; command_create.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; command_create.commandBufferCount = 1;
        VkCommandBuffer command; CALL(vkAllocateCommandBuffers(device, &command_create, &command));
        VkCommandBufferBeginInfo begin = {0}; begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        CALL(vkBeginCommandBuffer(command, &begin));
        VkBufferMemoryBarrier vertex_barrier = {0}; vertex_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        vertex_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT; vertex_barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        vertex_barrier.srcQueueFamilyIndex = vertex_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; vertex_barrier.buffer = vertex_buffer; vertex_barrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0,NULL,1,&vertex_barrier,0,NULL);
        VkClearValue clear = {0}; VkRenderPassBeginInfo pass_begin = {0}; pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        pass_begin.renderPass = render_pass; pass_begin.framebuffer = framebuffer; pass_begin.renderArea = scissor; pass_begin.clearValueCount = 1; pass_begin.pClearValues = &clear;
        vkCmdBeginRenderPass(command, &pass_begin, VK_SUBPASS_CONTENTS_INLINE); vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize vertex_offset = 0; vkCmdBindVertexBuffers(command, 0, 1, &vertex_buffer, &vertex_offset);
        vkCmdDraw(command, 3, 1, 0, 0); vkCmdEndRenderPass(command);
        VkBufferImageCopy copy = {0}; copy.imageSubresource = (VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent = image_create.extent;
        vkCmdCopyImageToBuffer(command, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copy);
        VkBufferMemoryBarrier barrier = {0}; barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; barrier.buffer = buffer; barrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,NULL,1,&barrier,0,NULL);
        CALL(vkEndCommandBuffer(command));
        VkFenceCreateInfo fence_create = {0}; fence_create.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; CALL(vkCreateFence(device, &fence_create, NULL, &fence));
        VkSubmitInfo submit = {0}; submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
        CALL(vkQueueSubmit(queue, 1, &submit, fence)); CALL(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_C(30000000000)));
        CALL(vkMapMemory(device, buffer_memory, 0, VK_WHOLE_SIZE, 0, &mapped));
        const uint8_t *pixels = mapped;
        for (unsigned i = 0; i < 256; ++i) CHECK(pixels[4*i] == 255 && pixels[4*i+1] == 0 && pixels[4*i+2] == 0 && pixels[4*i+3] == 255);
        puts("PIXELS\t256\t1024\tall-red-rgba-equal");
        snapshot("draw-completed");
    }
    okay = validation_errors == 0;
cleanup:
    if (device) {
        if (vkDeviceWaitIdle(device) != VK_SUCCESS) okay = 0;
        if (mapped) vkUnmapMemory(device, buffer_memory);
        if (fence) vkDestroyFence(device, fence, NULL);
        if (pool) vkDestroyCommandPool(device, pool, NULL);
        if (buffer) vkDestroyBuffer(device, buffer, NULL);
        if (buffer_memory) vkFreeMemory(device, buffer_memory, NULL);
        if (vertex_buffer) vkDestroyBuffer(device, vertex_buffer, NULL);
        if (vertex_memory) vkFreeMemory(device, vertex_memory, NULL);
        if (pipeline) vkDestroyPipeline(device, pipeline, NULL);
        if (layout) vkDestroyPipelineLayout(device, layout, NULL);
        if (fragment) vkDestroyShaderModule(device, fragment, NULL);
        if (vertex) vkDestroyShaderModule(device, vertex, NULL);
        if (framebuffer) vkDestroyFramebuffer(device, framebuffer, NULL);
        if (render_pass) vkDestroyRenderPass(device, render_pass, NULL);
        if (view) vkDestroyImageView(device, view, NULL);
        if (image) vkDestroyImage(device, image, NULL);
        if (image_memory) vkFreeMemory(device, image_memory, NULL);
        vkDestroyDevice(device, NULL);
        snapshot("device-destroyed");
    }
    if (messenger) destroy_debug(instance, messenger, NULL);
    if (instance) vkDestroyInstance(instance, NULL);
    snapshot("instance-destroyed");
    if (validation_errors) okay = 0;
    if (okay) printf("Vulkan lifetime control complete: mode=%s validation-errors=0 all-owned-objects-destroyed\n", draw ? "draw" : "device");
    return okay ? 0 : 1;
}
