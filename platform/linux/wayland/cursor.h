#ifndef NEXAUI_WAYLAND_CURSOR_H
#define NEXAUI_WAYLAND_CURSOR_H

#include "nexaui/nexaui.h"
#include <stdint.h>

#define NX_WAYLAND_CURSOR_SIZE 32

void nx_wayland_cursor_render(nx_cursor_shape shape,
    uint32_t pixels[NX_WAYLAND_CURSOR_SIZE * NX_WAYLAND_CURSOR_SIZE],
    int *hotspot_x, int *hotspot_y);

#endif
