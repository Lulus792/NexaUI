#include "scale.h"
#include <limits.h>

nx_result nx_wayland_scale_extent(int logical_width, int logical_height,
                                   uint32_t scale_120,
                                   int *pixel_width, int *pixel_height) {
    if (!pixel_width || !pixel_height) return NX_ERROR_ARGUMENT;
    *pixel_width = 0;
    *pixel_height = 0;
    if (logical_width <= 0 || logical_height <= 0 || !scale_120)
        return NX_ERROR_ARGUMENT;
    uint64_t width = ((uint64_t)logical_width * scale_120 + 60u) / 120u;
    uint64_t height = ((uint64_t)logical_height * scale_120 + 60u) / 120u;
    if (!width || !height || width > INT_MAX || height > INT_MAX ||
        width > (uint64_t)INT_MAX / 4u / height)
        return NX_ERROR_LIMIT;
    *pixel_width = (int)width;
    *pixel_height = (int)height;
    return NX_OK;
}
