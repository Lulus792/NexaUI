#include "internal.h"
#include "renderer_backend.h"

#include <stdint.h>
#include <string.h>

typedef struct nx_bounds {
    double left, top, right, bottom;
} nx_bounds;

static nx_bounds nx_rect_bounds(nx_rect r) {
    return (nx_bounds){r.x, r.y, (double)r.x + r.width, (double)r.y + r.height};
}

static nx_bounds nx_intersect(nx_bounds a, nx_bounds b) {
    nx_bounds out;
    out.left = a.left > b.left ? a.left : b.left;
    out.top = a.top > b.top ? a.top : b.top;
    out.right = a.right < b.right ? a.right : b.right;
    out.bottom = a.bottom < b.bottom ? a.bottom : b.bottom;
    return out;
}

static uint8_t nx_byte(float value) {
    return (uint8_t)(value * 255.0f + 0.5f);
}

static void nx_blend(uint8_t *dst, const uint8_t *src) {
    uint32_t sa = src[3], da = dst[3];
    if (sa == 0u) return;
    if (sa == 255u || da == 0u) {
        memcpy(dst, src, 4);
        return;
    }
    uint32_t inv = 255u - sa;
    uint32_t a_num = sa * 255u + da * inv;
    for (unsigned c = 0; c < 3; ++c) {
        uint32_t numerator = (uint32_t)src[c] * sa * 255u +
                             (uint32_t)dst[c] * da * inv;
        dst[c] = (uint8_t)((numerator + a_num / 2u) / a_num);
    }
    dst[3] = (uint8_t)((a_num + 127u) / 255u);
}

static uint32_t nx_first_pixel(double edge, uint32_t limit) {
    double pixel = ceil(edge - 0.5);
    if (pixel <= 0.0) return 0;
    if (pixel >= (double)limit) return limit;
    return (uint32_t)pixel;
}

static void nx_paint_rect(uint8_t *pixels, uint32_t width, uint32_t height,
                          size_t stride, nx_bounds bounds, nx_color color) {
    bounds = nx_intersect(bounds, (nx_bounds){0, 0, width, height});
    if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) return;
    uint32_t x0 = nx_first_pixel(bounds.left, width);
    uint32_t y0 = nx_first_pixel(bounds.top, height);
    uint32_t x1 = nx_first_pixel(bounds.right, width);
    uint32_t y1 = nx_first_pixel(bounds.bottom, height);
    uint8_t src[4] = {nx_byte(color.r), nx_byte(color.g),
                      nx_byte(color.b), nx_byte(color.a)};
    for (uint32_t y = y0; y < y1; ++y) {
        uint8_t *row = pixels + (size_t)y * stride;
        for (uint32_t x = x0; x < x1; ++x)
            nx_blend(row + (size_t)x * 4u, src);
    }
}

static nx_bounds nx_shape_bounds(const nx_command *cmd) {
    if (cmd->type == NX_COMMAND_LINE) {
        double r = (double)cmd->thickness / 2.0;
        double ax = cmd->points[0].x, ay = cmd->points[0].y;
        double bx = cmd->points[1].x, by = cmd->points[1].y;
        return (nx_bounds){fmin(ax, bx) - r, fmin(ay, by) - r,
                           fmax(ax, bx) + r, fmax(ay, by) + r};
    }
    if (cmd->type == NX_COMMAND_FILL_TRIANGLE) {
        const nx_point *p = cmd->points;
        return (nx_bounds){fmin(p[0].x, fmin(p[1].x, p[2].x)),
                           fmin(p[0].y, fmin(p[1].y, p[2].y)),
                           fmax(p[0].x, fmax(p[1].x, p[2].x)),
                           fmax(p[0].y, fmax(p[1].y, p[2].y))};
    }
    return nx_rect_bounds(cmd->rect);
}

static int nx_shape_contains(const nx_command *cmd, double x, double y) {
    if (cmd->type == NX_COMMAND_STROKE_RECT) {
        nx_bounds outer = nx_rect_bounds(cmd->rect);
        double t = cmd->thickness;
        return !(x >= outer.left + t && x < outer.right - t &&
                 y >= outer.top + t && y < outer.bottom - t);
    }
    if (cmd->type == NX_COMMAND_FILL_ROUND_RECT) {
        nx_bounds b = nx_rect_bounds(cmd->rect);
        double r = fmin(cmd->radius, fmin(cmd->rect.width, cmd->rect.height) / 2.0);
        if (r == 0.0) return 1;
        double cx = fmax(b.left + r, fmin(x, b.right - r));
        double cy = fmax(b.top + r, fmin(y, b.bottom - r));
        double dx = x - cx, dy = y - cy;
        return dx * dx + dy * dy <= r * r;
    }
    if (cmd->type == NX_COMMAND_LINE) {
        double ax = cmd->points[0].x, ay = cmd->points[0].y;
        double bx = cmd->points[1].x, by = cmd->points[1].y;
        double dx = bx - ax, dy = by - ay;
        double length2 = dx * dx + dy * dy;
        double along = length2 ? ((x - ax) * dx + (y - ay) * dy) / length2 : 0.0;
        along = fmax(0.0, fmin(1.0, along));
        double ex = x - (ax + along * dx), ey = y - (ay + along * dy);
        double r = (double)cmd->thickness / 2.0;
        return ex * ex + ey * ey <= r * r;
    }
    const nx_point *p = cmd->points;
    double ax = p[0].x, ay = p[0].y;
    double bx = p[1].x, by = p[1].y;
    double cx = p[2].x, cy = p[2].y;
    double orientation = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    if (orientation == 0.0) return 0;
    if (orientation < 0.0) {
        double tmp = bx; bx = cx; cx = tmp;
        tmp = by; by = cy; cy = tmp;
    }
    double e0 = (bx - ax) * (y - ay) - (by - ay) * (x - ax);
    double e1 = (cx - bx) * (y - by) - (cy - by) * (x - bx);
    double e2 = (ax - cx) * (y - cy) - (ay - cy) * (x - cx);
    return (e0 > 0.0 || (e0 == 0.0 && (by < ay || (by == ay && bx > ax)))) &&
           (e1 > 0.0 || (e1 == 0.0 && (cy < by || (cy == by && cx > bx)))) &&
           (e2 > 0.0 || (e2 == 0.0 && (ay < cy || (ay == cy && ax > cx))));
}

static void nx_paint_shape(uint8_t *pixels, uint32_t width, uint32_t height,
                           size_t stride, nx_bounds clip, const nx_command *cmd) {
    nx_bounds bounds = nx_intersect(clip, nx_shape_bounds(cmd));
    bounds = nx_intersect(bounds, (nx_bounds){0, 0, width, height});
    if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) return;
    uint32_t x0 = nx_first_pixel(bounds.left, width);
    uint32_t y0 = nx_first_pixel(bounds.top, height);
    uint32_t x1 = nx_first_pixel(bounds.right, width);
    uint32_t y1 = nx_first_pixel(bounds.bottom, height);
    uint8_t src[4] = {nx_byte(cmd->color.r), nx_byte(cmd->color.g),
                      nx_byte(cmd->color.b), nx_byte(cmd->color.a)};
    for (uint32_t y = y0; y < y1; ++y) {
        uint8_t *row = pixels + (size_t)y * stride;
        for (uint32_t x = x0; x < x1; ++x) {
            if (nx_shape_contains(cmd, (double)x + 0.5, (double)y + 0.5))
                nx_blend(row + (size_t)x * 4u, src);
        }
    }
}

static uint8_t nx_multiply_byte(uint8_t a, uint8_t b) {
    return (uint8_t)(((uint32_t)a * b + 127u) / 255u);
}

static void nx_paint_bitmap(uint8_t *pixels, uint32_t width, uint32_t height,
                             size_t stride, nx_bounds clip, const nx_command *cmd) {
    if (cmd->rect.width == 0.0f || cmd->rect.height == 0.0f) return;
    nx_bounds bounds = nx_intersect(clip, nx_rect_bounds(cmd->rect));
    bounds = nx_intersect(bounds, (nx_bounds){0, 0, width, height});
    if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) return;
    uint32_t x0 = nx_first_pixel(bounds.left, width);
    uint32_t y0 = nx_first_pixel(bounds.top, height);
    uint32_t x1 = nx_first_pixel(bounds.right, width);
    uint32_t y1 = nx_first_pixel(bounds.bottom, height);
    uint8_t tint[4] = {nx_byte(cmd->color.r), nx_byte(cmd->color.g),
                       nx_byte(cmd->color.b), nx_byte(cmd->color.a)};
    const nx_bitmap_view *bitmap = &cmd->bitmap;
    size_t channels = bitmap->format == NX_BITMAP_RGBA8 ? 4u : 1u;
    for (uint32_t y = y0; y < y1; ++y) {
        uint8_t *row = pixels + (size_t)y * stride;
        double v = ((double)y + 0.5 - cmd->rect.y) / cmd->rect.height;
        uint32_t sy = (uint32_t)fmin((double)bitmap->height - 1.0,
                                     floor(v * bitmap->height));
        const uint8_t *source_row = bitmap->pixels + (size_t)sy * bitmap->stride_bytes;
        for (uint32_t x = x0; x < x1; ++x) {
            double u = ((double)x + 0.5 - cmd->rect.x) / cmd->rect.width;
            uint32_t sx = (uint32_t)fmin((double)bitmap->width - 1.0,
                                         floor(u * bitmap->width));
            const uint8_t *sample = source_row + (size_t)sx * channels;
            uint8_t src[4];
            if (channels == 4u) {
                for (unsigned c = 0; c < 4; ++c)
                    src[c] = nx_multiply_byte(sample[c], tint[c]);
            } else {
                src[0] = tint[0];
                src[1] = tint[1];
                src[2] = tint[2];
                src[3] = nx_multiply_byte(sample[0], tint[3]);
            }
            nx_blend(row + (size_t)x * 4u, src);
        }
    }
}

nx_result nx_headless_render(const nx_command *commands, size_t count,
                             uint8_t *pixels, uint32_t width, uint32_t height,
                             size_t stride_bytes, nx_color clear_color) {
    if (!pixels || !width || !height || !nx_valid_color(clear_color) ||
        (!commands && count) || width > SIZE_MAX / 4u ||
        stride_bytes < (size_t)width * 4u ||
        ((size_t)height - 1u) > (SIZE_MAX - (size_t)width * 4u) / stride_bytes)
        return NX_ERROR_ARGUMENT;

    nx_result validation = nx_render_validate_commands(commands, count);
    if (validation != NX_OK) return validation;

    uint8_t clear[4] = {nx_byte(clear_color.r), nx_byte(clear_color.g),
                        nx_byte(clear_color.b), nx_byte(clear_color.a)};
    for (uint32_t y = 0; y < height; ++y) {
        uint8_t *row = pixels + (size_t)y * stride_bytes;
        for (uint32_t x = 0; x < width; ++x)
            memcpy(row + (size_t)x * 4u, clear, 4);
    }

    nx_bounds clips[NX_MAX_CLIP_DEPTH + 1u];
    clips[0] = (nx_bounds){0, 0, width, height};
    unsigned depth = 0;
    for (size_t i = 0; i < count; ++i) {
        const nx_command *cmd = &commands[i];
        if (cmd->type == NX_COMMAND_PUSH_CLIP) {
            clips[depth + 1u] = nx_intersect(clips[depth], nx_rect_bounds(cmd->rect));
            ++depth;
        } else if (cmd->type == NX_COMMAND_POP_CLIP) {
            --depth;
        } else if (cmd->type == NX_COMMAND_FILL_RECT) {
            nx_paint_rect(pixels, width, height, stride_bytes,
                          nx_intersect(clips[depth], nx_rect_bounds(cmd->rect)),
                          cmd->color);
        } else if (cmd->type == NX_COMMAND_BITMAP) {
            nx_paint_bitmap(pixels, width, height, stride_bytes,
                            clips[depth], cmd);
        } else {
            nx_paint_shape(pixels, width, height, stride_bytes,
                           clips[depth], cmd);
        }
    }
    return NX_OK;
}
