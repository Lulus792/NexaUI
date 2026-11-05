#ifndef NEXAUI_WAYLAND_SCALE_H
#define NEXAUI_WAYLAND_SCALE_H

#include "nexaui/nexaui.h"
#include <stdint.h>

/* Wayland toplevel scale is expressed in 120ths. The buffer dimensions are
   rounded to nearest pixel, with positive half-pixel ties rounded up. */
nx_result nx_wayland_scale_extent(int logical_width, int logical_height,
                                   uint32_t scale_120,
                                   int *pixel_width, int *pixel_height);

#endif
