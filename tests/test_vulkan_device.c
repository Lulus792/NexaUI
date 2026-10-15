#include "device_internal.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; \
} } while (0)
#define VERIFY(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: mock contract: %s\n", __FILE__, __LINE__, #condition); \
    abort(); \
} } while (0)
#define HANDLE(type, value) ((type)(uintptr_t)(value))

typedef struct adapter {
    VkPhysicalDeviceType type;
    int graphics, swapchain, null_queue;
    VkResult create_result;
} adapter;

typedef struct mock_state {
    adapter adapters[4];
    uint32_t count;
    unsigned instances, devices, creates, destroyed_devices, list_calls;
    unsigned create_order[8];
    int incomplete, extensions_incomplete, shrink, huge_queues, huge_devices;
    VkResult instance_result, enumerate_result, extension_result, wait_result;
    VkResult surface_result, surface_support_result;
    unsigned surfaces;
    int present_mode;
    unsigned instance_maintenance, enabled_maintenance, device_maintenance[4];
    int maintenance_feature[4], missing_dependencies, missing_proc, instance_incomplete;
    VkResult instance_enumeration_result;
} mock_state;

static mock_state mock;

static unsigned adapter_index(VkPhysicalDevice physical) {
    unsigned index = (unsigned)(uintptr_t)physical - 1u;
    VERIFY(index < mock.count);
    return index;
}

static VKAPI_ATTR VkResult VKAPI_CALL create_instance(
    const VkInstanceCreateInfo *create, const VkAllocationCallbacks *allocator,
    VkInstance *instance) {
    VERIFY(!allocator && create->sType == VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
    VERIFY(create->pApplicationInfo->apiVersion == VK_API_VERSION_1_0);
    VERIFY(create->enabledExtensionCount >= 1 &&
           !strcmp(create->ppEnabledExtensionNames[0],
                   VK_KHR_SURFACE_EXTENSION_NAME));
    mock.enabled_maintenance = 0;
    for (uint32_t i = 0; i < create->enabledExtensionCount; ++i) {
        if (!strcmp(create->ppEnabledExtensionNames[i], "VK_EXT_surface_maintenance1"))
            mock.enabled_maintenance |= 2u;
        if (!strcmp(create->ppEnabledExtensionNames[i], "VK_KHR_surface_maintenance1"))
            mock.enabled_maintenance |= 4u;
        for (uint32_t j = 0; j < i; ++j)
            VERIFY(strcmp(create->ppEnabledExtensionNames[i], create->ppEnabledExtensionNames[j]));
    }
    *instance = HANDLE(VkInstance, 100u);
    if (mock.instance_result == VK_SUCCESS) ++mock.instances;
    return mock.instance_result;
}

static VKAPI_ATTR void VKAPI_CALL destroy_instance(
    VkInstance instance, const VkAllocationCallbacks *allocator) {
    VERIFY(instance == HANDLE(VkInstance, 100u) && !allocator);
    VERIFY(mock.instances && !mock.surfaces);
    --mock.instances;
}

static VKAPI_ATTR VkResult VKAPI_CALL enumerate_devices(
    VkInstance instance, uint32_t *count, VkPhysicalDevice *devices) {
    VERIFY(instance != VK_NULL_HANDLE);
    if (mock.enumerate_result != VK_SUCCESS) return mock.enumerate_result;
    if (!devices) {
        *count = mock.huge_devices ? 4097u :
            mock.count - (mock.incomplete == 2 && !mock.list_calls ? 1u : 0u);
        return VK_SUCCESS;
    }
    ++mock.list_calls;
    uint32_t written = mock.count - (mock.shrink && mock.count ? 1u : 0u);
    if (written > *count) written = *count;
    for (uint32_t i = 0; i < written; ++i)
        devices[i] = HANDLE(VkPhysicalDevice, i + 1u);
    *count = written;
    if (mock.incomplete == 1 || (mock.incomplete == 2 && mock.list_calls == 1u))
        return VK_INCOMPLETE;
    return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL get_properties(
    VkPhysicalDevice physical, VkPhysicalDeviceProperties *properties) {
    unsigned index = adapter_index(physical);
    memset(properties, 0, sizeof(*properties));
    properties->apiVersion = VK_API_VERSION_1_0;
    properties->driverVersion = 42;
    properties->vendorID = 123;
    properties->deviceID = index + 1u;
    properties->limits.timestampPeriod = 1.25f;
    properties->deviceType = mock.adapters[index].type;
    (void)snprintf(properties->deviceName, sizeof(properties->deviceName),
                   "Adapter %u", index + 1u);
}

static VKAPI_ATTR void VKAPI_CALL get_queues(
    VkPhysicalDevice physical, uint32_t *count,
    VkQueueFamilyProperties *queues) {
    unsigned index = adapter_index(physical);
    if (!queues) {
        *count = mock.huge_queues ? 4097u : 3u;
        return;
    }
    VERIFY(*count >= 3);
    memset(queues, 0, 3u * sizeof(*queues));
    queues[0].queueCount = 1;
    queues[0].queueFlags = VK_QUEUE_COMPUTE_BIT;
    queues[0].timestampValidBits = 36;
    /* An empty graphics family must not be selected. */
    queues[1].queueFlags = VK_QUEUE_GRAPHICS_BIT;
    queues[2].queueCount = 1;
    queues[2].timestampValidBits = 48;
    queues[2].queueFlags = mock.adapters[index].graphics
        ? VK_QUEUE_GRAPHICS_BIT : VK_QUEUE_TRANSFER_BIT;
    *count = 3;
}

static VKAPI_ATTR VkResult VKAPI_CALL enumerate_extensions(
    VkPhysicalDevice physical, const char *layer, uint32_t *count,
    VkExtensionProperties *extensions) {
    unsigned index = adapter_index(physical);
    VERIFY(!layer);
    if (mock.extension_result != VK_SUCCESS) return mock.extension_result;
    const char *names[3];
    uint32_t total = 0;
    if (mock.adapters[index].swapchain) names[total++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    if (mock.device_maintenance[index] & 2u) names[total++] = "VK_EXT_swapchain_maintenance1";
    if (mock.device_maintenance[index] & 4u) names[total++] = "VK_KHR_swapchain_maintenance1";
    if (!extensions) {
        *count = total;
        return VK_SUCCESS;
    }
    VERIFY(*count >= total);
    memset(extensions, 0, total * sizeof(*extensions));
    for (uint32_t i = 0; i < total; ++i)
        strcpy(extensions[i].extensionName, names[i]);
    *count = total;
    return mock.extensions_incomplete ? VK_INCOMPLETE : VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL create_device(
    VkPhysicalDevice physical, const VkDeviceCreateInfo *create,
    const VkAllocationCallbacks *allocator, VkDevice *device) {
    unsigned index = adapter_index(physical);
    VERIFY(!allocator && !create->pEnabledFeatures);
    VERIFY(create->queueCreateInfoCount ==
           (mock.surfaces && mock.present_mode == 1 ? 2u : 1u));
    VERIFY(create->pQueueCreateInfos[0].queueFamilyIndex == 2);
    VERIFY(create->pQueueCreateInfos[0].queueCount == 1);
    VERIFY(create->pQueueCreateInfos[0].pQueuePriorities[0] == 1.0f);
    if (create->queueCreateInfoCount == 2)
        VERIFY(create->pQueueCreateInfos[1].queueFamilyIndex == 0);
    VERIFY(create->enabledExtensionCount ==
           (mock.adapters[index].swapchain ? 1u : 0u) + (create->pNext ? 1u : 0u));
    if (create->pNext) {
        const VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT *features = create->pNext;
        VERIFY(features->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT);
        VERIFY(features->swapchainMaintenance1 && mock.maintenance_feature[index]);
        unsigned available = mock.device_maintenance[index] & mock.enabled_maintenance;
        VERIFY(available);
        VERIFY(!strcmp(create->ppEnabledExtensionNames[1], (available & 4u)
            ? "VK_KHR_swapchain_maintenance1" : "VK_EXT_swapchain_maintenance1"));
    }
    if (create->enabledExtensionCount)
        VERIFY(!strcmp(create->ppEnabledExtensionNames[0],
                       VK_KHR_SWAPCHAIN_EXTENSION_NAME));
    VERIFY(mock.creates < 8u);
    mock.create_order[mock.creates++] = index;
    *device = HANDLE(VkDevice, 200u + index);
    if (mock.adapters[index].create_result == VK_SUCCESS) ++mock.devices;
    return mock.adapters[index].create_result;
}

static VKAPI_ATTR void VKAPI_CALL destroy_device(
    VkDevice device, const VkAllocationCallbacks *allocator) {
    VERIFY(device != VK_NULL_HANDLE && !allocator && mock.devices);
    --mock.devices;
    ++mock.destroyed_devices;
}

static VKAPI_ATTR void VKAPI_CALL get_queue(
    VkDevice device, uint32_t family, uint32_t index, VkQueue *queue) {
    unsigned adapter_id = (unsigned)(uintptr_t)device - 200u;
    VERIFY((family == 2u || (mock.surfaces && mock.present_mode == 1 && !family)) &&
           !index && adapter_id < mock.count);
    *queue = mock.adapters[adapter_id].null_queue ? VK_NULL_HANDLE
        : HANDLE(VkQueue, 300u + adapter_id);
}

static VKAPI_ATTR VkResult VKAPI_CALL wait_idle(VkDevice device) {
    VERIFY(device != VK_NULL_HANDLE);
    return mock.wait_result;
}

static VkResult create_surface(void *user, VkInstance instance,
                                VkSurfaceKHR *surface) {
    VERIFY(user == &mock && instance != VK_NULL_HANDLE && mock.instances);
    *surface = HANDLE(VkSurfaceKHR, 400u);
    if (mock.surface_result == VK_SUCCESS) ++mock.surfaces;
    return mock.surface_result;
}

static VKAPI_ATTR void VKAPI_CALL destroy_surface(VkInstance instance,
    VkSurfaceKHR surface, const VkAllocationCallbacks *allocator) {
    VERIFY(instance != VK_NULL_HANDLE && surface == HANDLE(VkSurfaceKHR, 400u));
    VERIFY(!allocator && mock.surfaces && !mock.devices);
    --mock.surfaces;
}

static VKAPI_ATTR VkResult VKAPI_CALL surface_support(VkPhysicalDevice physical,
    uint32_t family, VkSurfaceKHR surface, VkBool32 *supported) {
    unsigned index = adapter_index(physical);
    VERIFY(surface == HANDLE(VkSurfaceKHR, 400u) && mock.surfaces);
    *supported = VK_FALSE;
    if (mock.present_mode == 0) *supported = family == 2 ? VK_TRUE : VK_FALSE;
    else if (mock.present_mode == 1) *supported = !family ? VK_TRUE : VK_FALSE;
    else if (mock.present_mode == 3)
        *supported = !index && family == 2 ? VK_TRUE : VK_FALSE;
    return mock.surface_support_result;
}

static VKAPI_ATTR VkResult VKAPI_CALL instance_extensions(const char *layer,
    uint32_t *count, VkExtensionProperties *extensions) {
    VERIFY(!layer);
    if (mock.instance_enumeration_result != VK_SUCCESS) return mock.instance_enumeration_result;
    const char *names[4];
    uint32_t total = 0;
    if (!mock.missing_dependencies)
        names[total++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
    names[total++] = VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;
    if (mock.instance_maintenance & 2u) names[total++] = "VK_EXT_surface_maintenance1";
    if (mock.instance_maintenance & 4u) names[total++] = "VK_KHR_surface_maintenance1";
    if (!extensions) { *count = total; return VK_SUCCESS; }
    VERIFY(*count >= total);
    memset(extensions, 0, total * sizeof(*extensions));
    for (uint32_t i = 0; i < total; ++i) strcpy(extensions[i].extensionName, names[i]);
    *count = total;
    return mock.instance_incomplete ? VK_INCOMPLETE : VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL query_features(VkPhysicalDevice physical,
                                                 VkPhysicalDeviceFeatures2 *features) {
    unsigned index = adapter_index(physical);
    VERIFY(features->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR);
    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT *maintenance = features->pNext;
    VERIFY(maintenance && maintenance->sType ==
           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT);
    maintenance->swapchainMaintenance1 = mock.maintenance_feature[index] ? VK_TRUE : VK_FALSE;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL instance_proc(VkInstance instance,
                                                             const char *name) {
    VERIFY(instance != VK_NULL_HANDLE && !strcmp(name, "vkGetPhysicalDeviceFeatures2KHR"));
    return mock.missing_proc ? NULL : (PFN_vkVoidFunction)query_features;
}

static const nx_vk_api api = {
    create_instance, destroy_instance, enumerate_devices, get_properties,
    get_queues, enumerate_extensions, create_device, destroy_device,
    get_queue, wait_idle, surface_support, destroy_surface, instance_extensions, instance_proc
};

typedef struct allocation { void *pointer; size_t size; } allocation;
typedef struct tracker {
    allocation live[32];
    size_t calls, fail_at, count;
} tracker;

static void *track_allocate(void *user, size_t size) {
    tracker *track = user;
    ++track->calls;
    if (track->calls == track->fail_at) return NULL;
    VERIFY(track->count < 32u);
    void *pointer = malloc(size);
    VERIFY(pointer);
    track->live[track->count++] = (allocation){pointer, size};
    return pointer;
}

static void track_deallocate(void *user, void *pointer, size_t size) {
    tracker *track = user;
    size_t i = 0;
    while (i < track->count && track->live[i].pointer != pointer) ++i;
    VERIFY(i < track->count && track->live[i].size == size);
    track->live[i] = track->live[--track->count];
    free(pointer);
}

static void reset_mock(void) {
    memset(&mock, 0, sizeof(mock));
    mock.count = 3;
    mock.adapters[0] = (adapter){VK_PHYSICAL_DEVICE_TYPE_CPU, 1, 1, 0, VK_SUCCESS};
    mock.adapters[1] = (adapter){VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
                                 1, 1, 0, VK_SUCCESS};
    mock.adapters[2] = (adapter){VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
                                 1, 1, 0, VK_SUCCESS};
}

static int selection(void) {
    reset_mock();
    nx_vk_device *device = NULL;
    nx_vk_device_info info;
    nx_vk_handles handles;
    CHECK(nx_vk_test_device_create(NULL, &api, &device) == NX_OK);
    CHECK(nx_vk_device_get_info(device, &info) == NX_OK);
    CHECK(info.kind == NX_VK_DEVICE_DISCRETE && info.device_id == 3);
    CHECK(info.graphics_queue_family == 2 && info.swapchain_enabled);
    CHECK(!strcmp(info.name, "Adapter 3") && info.vendor_id == 123);
    CHECK(nx_vk_device_get_handles(device, &handles) == NX_OK);
    CHECK(handles.physical == HANDLE(VkPhysicalDevice, 3));
    CHECK(handles.graphics_queue != VK_NULL_HANDLE);
    CHECK(handles.timestamp_valid_bits == 48 && handles.timestamp_period_ns == 1.25);
    CHECK(nx_vk_device_wait_idle(device) == NX_OK);
    nx_vk_device_destroy(device);
    CHECK(!mock.instances && !mock.devices);

    reset_mock();
    mock.adapters[2].create_result = VK_ERROR_INITIALIZATION_FAILED;
    mock.adapters[1].graphics = 0;
    CHECK(nx_vk_test_device_create(NULL, &api, &device) == NX_OK);
    CHECK(mock.creates == 2 && mock.create_order[0] == 2 &&
          mock.create_order[1] == 0);
    CHECK(nx_vk_device_get_info(device, &info) == NX_OK);
    CHECK(info.kind == NX_VK_DEVICE_CPU);
    nx_vk_device_destroy(device);
    CHECK(!mock.instances && !mock.devices);

    reset_mock();
    mock.adapters[2].null_queue = 1;
    CHECK(nx_vk_test_device_create(NULL, &api, &device) == NX_OK);
    CHECK(mock.destroyed_devices == 1);
    CHECK(nx_vk_device_get_info(device, &info) == NX_OK);
    CHECK(info.kind == NX_VK_DEVICE_INTEGRATED);
    nx_vk_device_destroy(device);
    CHECK(!mock.instances && !mock.devices);
    reset_mock();
    mock.adapters[0].type = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    mock.adapters[2].type = VK_PHYSICAL_DEVICE_TYPE_CPU;
    CHECK(nx_vk_test_device_create(NULL, &api, &device) == NX_OK);
    CHECK(nx_vk_device_get_info(device, &info) == NX_OK);
    CHECK(info.device_id == 1); /* Enumeration order breaks equal-rank ties. */
    nx_vk_device_destroy(device);
    return 0;
}

static int enumeration_and_capabilities(void) {
    nx_vk_device *device = NULL;
    nx_vk_device_info info;
    reset_mock();
    mock.incomplete = 2;
    CHECK(nx_vk_test_device_create(NULL, &api, &device) == NX_OK);
    CHECK(mock.list_calls == 2);
    CHECK(nx_vk_device_get_info(device, &info) == NX_OK && info.device_id == 3);
    nx_vk_device_destroy(device);
    reset_mock();
    mock.incomplete = 1;
    CHECK(nx_vk_test_device_create(NULL, &api, &device) == NX_ERROR_LIMIT);
    CHECK(!device && mock.list_calls == 4 && !mock.instances);
    reset_mock();
    mock.extensions_incomplete = 1;
    CHECK(nx_vk_test_device_create(NULL, &api, &device) == NX_ERROR_LIMIT);
    CHECK(!device && !mock.instances);
    reset_mock();
    mock.shrink = 1;
    CHECK(nx_vk_test_device_create(NULL, &api, &device) == NX_OK);
    CHECK(nx_vk_device_get_info(device, &info) == NX_OK);
    CHECK(info.kind == NX_VK_DEVICE_INTEGRATED);
    nx_vk_device_destroy(device);

    reset_mock();
    mock.count = 0;
    CHECK(nx_vk_test_device_create(NULL, &api, &device) == NX_ERROR_UNSUPPORTED);
    CHECK(!device && !mock.instances);
    reset_mock();
    mock.huge_devices = 1;
    CHECK(nx_vk_test_device_create(NULL, &api, &device) == NX_ERROR_LIMIT);
    CHECK(!device && !mock.instances);
    reset_mock();
    for (unsigned i = 0; i < mock.count; ++i) mock.adapters[i].graphics = 0;
    CHECK(nx_vk_test_device_create(NULL, &api, &device) == NX_ERROR_UNSUPPORTED);
    CHECK(!device && !mock.instances && !mock.creates);
    reset_mock();
    mock.huge_queues = 1;
    CHECK(nx_vk_test_device_create(NULL, &api, &device) == NX_ERROR_LIMIT);
    CHECK(!device && !mock.instances);
    reset_mock();
    for (unsigned i = 0; i < mock.count; ++i) mock.adapters[i].swapchain = 0;
    nx_vk_device_desc desc = {0};
    desc.require_swapchain = 1;
    CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_ERROR_UNSUPPORTED);
    CHECK(!device && !mock.instances && !mock.creates);
    CHECK(nx_vk_test_device_create(NULL, &api, &device) == NX_OK);
    CHECK(nx_vk_device_get_info(device, &info) == NX_OK);
    CHECK(!info.swapchain_enabled);
    nx_vk_device_destroy(device);
    CHECK(!mock.instances && !mock.devices);
    return 0;
}

static int failures_and_allocations(void) {
    nx_vk_device *device = NULL;
    tracker track = {0};
    nx_vk_device_desc desc = {0};
    desc.allocator = (nx_allocator){&track, track_allocate, track_deallocate};
    reset_mock();
    CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_OK);
    size_t calls = track.calls;
    nx_vk_device_destroy(device);
    CHECK(!track.count && calls > 3);
    for (size_t fail = 1; fail <= calls; ++fail) {
        reset_mock();
        track = (tracker){0};
        track.fail_at = fail;
        CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_ERROR_MEMORY);
        CHECK(!device && !track.count && !mock.instances && !mock.devices);
    }
    /* A shrinking enumeration must still free the original byte count. */
    reset_mock();
    track = (tracker){0};
    mock.shrink = 1;
    CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_OK);
    nx_vk_device_destroy(device);
    CHECK(!track.count);

    const VkResult failures[] = {VK_ERROR_OUT_OF_HOST_MEMORY,
        VK_ERROR_OUT_OF_DEVICE_MEMORY, VK_ERROR_INITIALIZATION_FAILED,
        VK_ERROR_INCOMPATIBLE_DRIVER, VK_ERROR_DEVICE_LOST};
    const nx_result expected[] = {NX_ERROR_MEMORY, NX_ERROR_MEMORY,
        NX_ERROR_PLATFORM, NX_ERROR_UNSUPPORTED, NX_ERROR_STATE};
    for (unsigned i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        reset_mock();
        mock.instance_result = failures[i];
        CHECK(nx_vk_test_device_create(NULL, &api, &device) == expected[i]);
        CHECK(!device && !mock.instances && !mock.devices);
        reset_mock();
        mock.enumerate_result = failures[i];
        CHECK(nx_vk_test_device_create(NULL, &api, &device) == expected[i]);
        CHECK(!device && !mock.instances && !mock.devices);
        reset_mock();
        mock.extension_result = failures[i];
        CHECK(nx_vk_test_device_create(NULL, &api, &device) == expected[i]);
        CHECK(!device && !mock.instances && !mock.devices);
        reset_mock();
        for (unsigned j = 0; j < mock.count; ++j)
            mock.adapters[j].create_result = failures[i];
        CHECK(nx_vk_test_device_create(NULL, &api, &device) == expected[i]);
        CHECK(!device && !mock.instances && !mock.devices);
        if (expected[i] == NX_ERROR_MEMORY) CHECK(mock.creates == 1);
    }
    return 0;
}

typedef struct thread_check { nx_vk_device *device; int passed; } thread_check;

static void *other_thread(void *user) {
    thread_check *check = user;
    nx_vk_device_info info;
    nx_vk_handles handles;
    check->passed =
        nx_vk_device_get_info(check->device, &info) == NX_ERROR_STATE &&
        !info.name[0] &&
        nx_vk_device_get_handles(check->device, &handles) == NX_ERROR_STATE &&
        handles.logical == VK_NULL_HANDLE &&
        nx_vk_device_wait_idle(check->device) == NX_ERROR_STATE;
    nx_vk_device_destroy(check->device);
    return NULL;
}

static int arguments_and_ownership(void) {
    reset_mock();
    nx_vk_device *device = HANDLE(nx_vk_device *, 1u);
    CHECK(nx_vk_test_device_create(NULL, NULL, &device) == NX_ERROR_ARGUMENT);
    CHECK(!device);
    CHECK(nx_vk_test_device_create(NULL, &api, NULL) == NX_ERROR_ARGUMENT);
    nx_vk_device_desc desc = {0};
    desc.allocator.allocate = track_allocate;
    CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_ERROR_ARGUMENT);
    desc = (nx_vk_device_desc){0};
    desc.instance_extension_count = 1;
    CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_ERROR_ARGUMENT);
    const char *names[] = {"VK_KHR_surface", "VK_KHR_surface"};
    desc.instance_extensions = names;
    desc.instance_extension_count = 2;
    CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_ERROR_ARGUMENT);
    names[1] = "";
    CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_ERROR_ARGUMENT);
    CHECK(nx_vk_test_device_create(NULL, &api, &device) == NX_OK);
    thread_check check = {device, 0};
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, other_thread, &check) == 0);
    CHECK(pthread_join(thread, NULL) == 0 && check.passed);
    CHECK(mock.devices == 1 && mock.instances == 1);
    nx_vk_device *second = NULL;
    CHECK(nx_vk_test_device_create(NULL, &api, &second) == NX_OK);
    CHECK(mock.devices == 2);
    mock.wait_result = VK_ERROR_DEVICE_LOST;
    CHECK(nx_vk_device_wait_idle(device) == NX_ERROR_STATE);
    nx_vk_device_destroy(device);
    CHECK(mock.devices == 1 && mock.instances == 1);
    mock.wait_result = VK_SUCCESS;
    CHECK(nx_vk_device_wait_idle(second) == NX_OK);
    nx_vk_device_destroy(second);
    CHECK(!mock.devices && !mock.instances);
    nx_vk_device_destroy(NULL);
    return 0;
}

static int presentation_selection(void) {
    nx_vk_surface_source source = {&mock, create_surface};
    nx_vk_device *device = NULL;
    nx_vk_device_info info;
    nx_vk_handles handles;
    for (int mode = 0; mode < 4; ++mode) {
        reset_mock();
        mock.present_mode = mode;
        nx_result result = nx_vk_test_surface_device_create(NULL, &api, &source, &device);
        if (mode == 2) {
            CHECK(result == NX_ERROR_UNSUPPORTED && !device);
            CHECK(!mock.instances && !mock.surfaces && !mock.devices);
            continue;
        }
        CHECK(result == NX_OK);
        CHECK(nx_vk_device_get_info(device, &info) == NX_OK);
        CHECK(info.has_surface && info.present_queue_family == (mode == 1 ? 0u : 2u));
        CHECK(info.device_id == (mode == 3 ? 1u : 3u));
        CHECK(nx_vk_device_get_handles(device, &handles) == NX_OK);
        CHECK(handles.surface != VK_NULL_HANDLE && handles.present_queue != VK_NULL_HANDLE);
        nx_vk_device_destroy(device);
        CHECK(!mock.instances && !mock.surfaces && !mock.devices);
    }
    reset_mock();
    mock.surface_result = VK_ERROR_OUT_OF_HOST_MEMORY;
    CHECK(nx_vk_test_surface_device_create(NULL, &api, &source, &device) == NX_ERROR_MEMORY);
    CHECK(!device && !mock.instances && !mock.surfaces);
    reset_mock();
    mock.surface_support_result = VK_ERROR_SURFACE_LOST_KHR;
    CHECK(nx_vk_test_surface_device_create(NULL, &api, &source, &device) == NX_ERROR_PLATFORM);
    CHECK(!device && !mock.instances && !mock.surfaces);
    reset_mock();
    tracker track = {0};
    nx_vk_device_desc desc = {0};
    desc.allocator = (nx_allocator){&track, track_allocate, track_deallocate};
    CHECK(nx_vk_test_surface_device_create(&desc, &api, &source, &device) == NX_OK);
    size_t calls = track.calls;
    nx_vk_device_destroy(device);
    for (size_t fail = 1; fail <= calls; ++fail) {
        reset_mock();
        track = (tracker){0};
        track.fail_at = fail;
        CHECK(nx_vk_test_surface_device_create(&desc, &api, &source, &device) == NX_ERROR_MEMORY);
        CHECK(!device && !track.count && !mock.instances && !mock.surfaces && !mock.devices);
    }
    return 0;
}

static void maintenance_adapter(unsigned mask) {
    mock.instance_maintenance = mask;
    for (unsigned i = 0; i < mock.count; ++i) {
        mock.device_maintenance[i] = mask;
        mock.maintenance_feature[i] = 1;
    }
}

static int maintenance_selection(void) {
    nx_vk_device *device = NULL;
    nx_vk_device_desc desc = {0};
    desc.present_fences = NX_VK_PRESENT_FENCES_REQUIRED;
    nx_vk_device_info info;
    const unsigned variants[] = {2u, 4u, 6u};
    for (unsigned i = 0; i < 3u; ++i) {
        reset_mock();
        maintenance_adapter(variants[i]);
        CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_OK);
        CHECK(nx_vk_device_get_info(device, &info) == NX_OK);
        CHECK(info.present_fences == (variants[i] & 4u ? NX_VK_PRESENT_FENCES_KHR
                                                       : NX_VK_PRESENT_FENCES_EXT));
        nx_vk_device_destroy_retired(device);
        CHECK(!mock.instances && !mock.devices);
    }
    reset_mock();
    maintenance_adapter(6u);
    mock.maintenance_feature[2] = 0;
    CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_OK);
    CHECK(nx_vk_device_get_info(device, &info) == NX_OK && info.device_id == 2u);
    nx_vk_device_destroy(device);
    for (unsigned mode = 0; mode < 5u; ++mode) {
        reset_mock();
        maintenance_adapter(6u);
        if (!mode) mock.instance_maintenance = 0;
        if (mode == 1u) mock.missing_dependencies = 1;
        if (mode == 2u) mock.missing_proc = 1;
        if (mode == 3u) for (unsigned i = 0; i < mock.count; ++i) mock.device_maintenance[i] = 0;
        if (mode == 4u) for (unsigned i = 0; i < mock.count; ++i) mock.maintenance_feature[i] = 0;
        desc.present_fences = NX_VK_PRESENT_FENCES_REQUIRED;
        CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_ERROR_UNSUPPORTED);
        CHECK(!device && !mock.instances && !mock.devices);
        desc.present_fences = NX_VK_PRESENT_FENCES_OPTIONAL;
        CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_OK);
        CHECK(nx_vk_device_get_info(device, &info) == NX_OK &&
              info.present_fences == NX_VK_PRESENT_FENCES_NONE);
        nx_vk_device_destroy(device);
    }
    reset_mock();
    maintenance_adapter(6u);
    desc.present_fences = NX_VK_PRESENT_FENCES_REQUIRED;
    mock.instance_incomplete = 1;
    CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_ERROR_LIMIT);
    CHECK(!device && !mock.instances);
    reset_mock();
    maintenance_adapter(6u);
    mock.instance_enumeration_result = VK_ERROR_OUT_OF_HOST_MEMORY;
    CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_ERROR_MEMORY);
    CHECK(!device && !mock.instances);
    tracker track = {0};
    desc.allocator = (nx_allocator){&track, track_allocate, track_deallocate};
    reset_mock();
    maintenance_adapter(6u);
    CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_OK);
    size_t calls = track.calls;
    nx_vk_device_destroy(device);
    CHECK(!track.count);
    for (size_t fail = 1; fail <= calls; ++fail) {
        reset_mock();
        maintenance_adapter(6u);
        track = (tracker){0};
        track.fail_at = fail;
        CHECK(nx_vk_test_device_create(&desc, &api, &device) == NX_ERROR_MEMORY);
        CHECK(!device && !track.count && !mock.instances && !mock.devices);
    }
    return 0;
}

int main(void) {
    CHECK(selection() == 0);
    CHECK(enumeration_and_capabilities() == 0);
    CHECK(failures_and_allocations() == 0);
    CHECK(arguments_and_ownership() == 0);
    CHECK(presentation_selection() == 0);
    CHECK(maintenance_selection() == 0);
    return 0;
}
