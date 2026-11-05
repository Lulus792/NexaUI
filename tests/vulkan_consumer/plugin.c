#include <nexaui/vulkan.h>

/* Link the renderer archive into a shared consumer, without private headers. */
nx_result consumer_plugin_close_empty(void) {
    nx_vulkan_view *view = 0;
    return nx_vulkan_view_close(&view);
}
