#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct probe { unsigned expected, unexpected; } probe;
static VKAPI_ATTR VkBool32 VKAPI_CALL message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) {
    (void)severity; (void)type;
    probe *p = user;
    if (data->pMessageIdName && strstr(data->pMessageIdName,"VkFenceCreateInfo-flags-parameter")) ++p->expected;
    else { ++p->unexpected; fprintf(stderr,"Unexpected validation message: %s\n",data->pMessage); }
    return VK_TRUE; /* Abort the invalid call before driver dispatch. */
}
int main(void) {
    int required = getenv("NEXAUI_REQUIRE_VALIDATION") != NULL;
    uint32_t count = 0;
    if (vkEnumerateInstanceLayerProperties(&count,NULL) != VK_SUCCESS || count > 128) return 1;
    VkLayerProperties layers[128];
    if (vkEnumerateInstanceLayerProperties(&count,layers) != VK_SUCCESS) return 1;
    const VkLayerProperties *selected = NULL;
    for (uint32_t i = 0; i < count; ++i)
        if (!strcmp(layers[i].layerName,"VK_LAYER_KHRONOS_validation")) selected = &layers[i];
    if (!selected) { fputs("Khronos validation unavailable.\n",stderr); return required ? 1 : 77; }
    char version[32];
    snprintf(version,sizeof(version),"%u.%u.%u",VK_VERSION_MAJOR(selected->specVersion),
        VK_VERSION_MINOR(selected->specVersion),VK_VERSION_PATCH(selected->specVersion));
    const char *expected = getenv("NEXAUI_CI_VALIDATION_VERSION");
    if (expected && strcmp(version,expected)) {
        fprintf(stderr,"Validation version %s, expected %s\n",version,expected); return 1;
    }
    int result = 1;
    VkInstance instance = VK_NULL_HANDLE; VkDevice device = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE; VkFence fence = VK_NULL_HANDLE;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger = NULL;
    probe p = {0};
    VkDebugUtilsMessengerCreateInfoEXT debug = {0};
    debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    debug.pfnUserCallback = message; debug.pUserData = &p;
    const char *layer = "VK_LAYER_KHRONOS_validation", *extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    VkApplicationInfo app = {0}; app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo create = {0}; create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create.pApplicationInfo = &app; create.pNext = &debug;
    create.enabledLayerCount = create.enabledExtensionCount = 1;
    create.ppEnabledLayerNames = &layer; create.ppEnabledExtensionNames = &extension;
    if (vkCreateInstance(&create,NULL,&instance) != VK_SUCCESS) goto cleanup;
    PFN_vkCreateDebugUtilsMessengerEXT create_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT");
    destroy_messenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT");
    if (!create_messenger || !destroy_messenger || create_messenger(instance,&debug,NULL,&messenger) != VK_SUCCESS) goto cleanup;
    VkPhysicalDevice physical[32]; count = 32;
    VkResult status = vkEnumeratePhysicalDevices(instance,&count,physical);
    if ((status != VK_SUCCESS && status != VK_INCOMPLETE) || !count) { result = required ? 1 : 77; goto cleanup; }
    uint32_t families = 0; vkGetPhysicalDeviceQueueFamilyProperties(physical[0],&families,NULL);
    if (!families || families > 64) goto cleanup;
    VkQueueFamilyProperties queues[64]; vkGetPhysicalDeviceQueueFamilyProperties(physical[0],&families,queues);
    uint32_t family = 0;
    while (family < families && !queues[family].queueCount) ++family;
    if (family == families) goto cleanup;
    float priority = 1;
    VkDeviceQueueCreateInfo queue = {0}; queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueFamilyIndex = family; queue.queueCount = 1; queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo logical = {0}; logical.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    logical.queueCreateInfoCount = 1; logical.pQueueCreateInfos = &queue;
    if (vkCreateDevice(physical[0],&logical,NULL,&device) != VK_SUCCESS || p.unexpected) goto cleanup;
    /* No queue work is submitted. Reserved flags form a controlled validation
       counterexample; the error callback requests that dispatch be aborted. */
    VkFenceCreateInfo invalid = {0}; invalid.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    invalid.flags = (VkFenceCreateFlags)0x80000000u;
    status = vkCreateFence(device,&invalid,NULL,&fence);
    if (status == VK_ERROR_VALIDATION_FAILED_EXT && fence == VK_NULL_HANDLE && p.expected && !p.unexpected) {
        printf("Khronos validation %s rejected reserved fence flags; counterexample passed.\n",version);
        result = 0;
    } else fprintf(stderr,"Validation counterexample was not rejected: status=%d expected=%u unexpected=%u\n",
        (int)status,p.expected,p.unexpected);
cleanup:
    if (fence != VK_NULL_HANDLE) vkDestroyFence(device,fence,NULL);
    if (device != VK_NULL_HANDLE) vkDestroyDevice(device,NULL);
    if (messenger != VK_NULL_HANDLE) destroy_messenger(instance,messenger,NULL);
    if (instance != VK_NULL_HANDLE) vkDestroyInstance(instance,NULL);
    if (p.unexpected) result = 1;
    return result;
}
