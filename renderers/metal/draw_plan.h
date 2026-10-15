#ifndef NEXAUI_METAL_DRAW_PLAN_H
#define NEXAUI_METAL_DRAW_PLAN_H

#include "renderer_backend.h"

/* Plain C preparation, independently testable without an Apple SDK. This
   initial path accepts opaque shapes and clips; image/alpha drawing follows. */
typedef struct nx_mtl_shape {
    nx_point a, b, c;
    uint32_t kind;
    uint32_t color[4];
} nx_mtl_shape;
typedef struct nx_mtl_box { uint32_t x, y, width, height; } nx_mtl_box;
typedef struct nx_mtl_draw {
    nx_point vertices[6]; /* Metal NDC: logical top maps to +1. */
    nx_mtl_shape shape;
    nx_mtl_box scissor;
} nx_mtl_draw;
typedef struct nx_mtl_plan {
    const nx_command *commands;
    size_t count, index;
    double scale;
    uint32_t width, height;
    unsigned depth;
    nx_mtl_box clips[NX_MAX_CLIP_DEPTH + 1u];
} nx_mtl_plan;

nx_result nx_mtl_plan_begin(nx_mtl_plan *plan, const nx_command *commands,
    size_t count, double scale, uint32_t width, uint32_t height);
/* Only call after successful begin. Borrowed commands must remain unchanged
   until iteration finishes; no bitmap pointers are read by this initial path. */
int nx_mtl_plan_next(nx_mtl_plan *plan, nx_mtl_draw *out_draw);

#endif
