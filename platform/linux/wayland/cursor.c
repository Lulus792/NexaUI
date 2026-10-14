#include "cursor.h"

#include <stdlib.h>
#include <string.h>

#define S NX_WAYLAND_CURSOR_SIZE

static void nx_cursor_point(unsigned char mask[S * S], int x, int y) {
    if (x >= 0 && x < S && y >= 0 && y < S) mask[y * S + x] = 1;
}

static void nx_cursor_line(unsigned char mask[S * S], int x0, int y0,
                           int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        nx_cursor_point(mask, x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int twice = error * 2;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}

static void nx_cursor_rect(unsigned char mask[S * S], int x0, int y0,
                           int x1, int y1) {
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            nx_cursor_point(mask, x, y);
}

static void nx_cursor_ring(unsigned char mask[S * S]) {
    for (int y = 3; y < 29; ++y)
        for (int x = 3; x < 29; ++x) {
            int dx = x - 16, dy = y - 16;
            int squared = dx * dx + dy * dy;
            if (squared >= 108 && squared <= 151)
                nx_cursor_point(mask, x, y);
        }
}

static void nx_cursor_arrow_heads(unsigned char mask[S * S],
                                   int x0, int y0, int x1, int y1) {
    nx_cursor_line(mask, x0, y0, x1, y1);
    int dx = x1 > x0 ? 1 : x1 < x0 ? -1 : 0;
    int dy = y1 > y0 ? 1 : y1 < y0 ? -1 : 0;
    if (dx) {
        nx_cursor_line(mask, x0, y0, x0 + dx * 5, y0 - 4);
        nx_cursor_line(mask, x0, y0, x0 + dx * 5, y0 + 4);
        nx_cursor_line(mask, x1, y1, x1 - dx * 5, y1 - 4);
        nx_cursor_line(mask, x1, y1, x1 - dx * 5, y1 + 4);
    }
    if (dy) {
        nx_cursor_line(mask, x0, y0, x0 - 4, y0 + dy * 5);
        nx_cursor_line(mask, x0, y0, x0 + 4, y0 + dy * 5);
        nx_cursor_line(mask, x1, y1, x1 - 4, y1 - dy * 5);
        nx_cursor_line(mask, x1, y1, x1 + 4, y1 - dy * 5);
    }
}

void nx_wayland_cursor_render(nx_cursor_shape shape, uint32_t pixels[S * S],
                              int *hotspot_x, int *hotspot_y) {
    if (!pixels || !hotspot_x || !hotspot_y) return;
    unsigned char mask[S * S];
    memset(mask, 0, sizeof(mask));
    *hotspot_x = *hotspot_y = 16;
    switch (shape) {
    case NX_CURSOR_DEFAULT:
        *hotspot_x = 3; *hotspot_y = 2;
        for (int y = 2; y <= 21; ++y)
            for (int x = 3; x <= 3 + (y - 2) * 2 / 3; ++x)
                nx_cursor_point(mask, x, y);
        nx_cursor_line(mask, 11, 16, 17, 28);
        nx_cursor_line(mask, 12, 16, 18, 28);
        break;
    case NX_CURSOR_TEXT:
        nx_cursor_rect(mask, 15, 4, 16, 27);
        nx_cursor_rect(mask, 10, 3, 21, 5);
        nx_cursor_rect(mask, 10, 26, 21, 28);
        break;
    case NX_CURSOR_LINK:
        *hotspot_x = 12; *hotspot_y = 2;
        nx_cursor_rect(mask, 11, 2, 14, 20);
        nx_cursor_rect(mask, 15, 13, 18, 21);
        nx_cursor_rect(mask, 19, 15, 22, 22);
        nx_cursor_rect(mask, 7, 17, 10, 24);
        nx_cursor_rect(mask, 9, 21, 22, 28);
        break;
    case NX_CURSOR_CROSSHAIR:
        nx_cursor_line(mask, 16, 3, 16, 12);
        nx_cursor_line(mask, 16, 20, 16, 29);
        nx_cursor_line(mask, 3, 16, 12, 16);
        nx_cursor_line(mask, 20, 16, 29, 16);
        nx_cursor_ring(mask);
        break;
    case NX_CURSOR_RESIZE_HORIZONTAL:
        nx_cursor_arrow_heads(mask, 3, 16, 29, 16);
        break;
    case NX_CURSOR_RESIZE_VERTICAL:
        nx_cursor_arrow_heads(mask, 16, 3, 16, 29);
        break;
    case NX_CURSOR_RESIZE_DIAGONAL_NW_SE:
        nx_cursor_arrow_heads(mask, 4, 4, 28, 28);
        break;
    case NX_CURSOR_RESIZE_DIAGONAL_NE_SW:
        nx_cursor_arrow_heads(mask, 28, 4, 4, 28);
        break;
    case NX_CURSOR_MOVE:
        nx_cursor_line(mask, 16, 3, 16, 29);
        nx_cursor_line(mask, 3, 16, 29, 16);
        nx_cursor_line(mask, 16, 3, 12, 8);
        nx_cursor_line(mask, 16, 3, 20, 8);
        nx_cursor_line(mask, 16, 29, 12, 24);
        nx_cursor_line(mask, 16, 29, 20, 24);
        nx_cursor_line(mask, 3, 16, 8, 12);
        nx_cursor_line(mask, 3, 16, 8, 20);
        nx_cursor_line(mask, 29, 16, 24, 12);
        nx_cursor_line(mask, 29, 16, 24, 20);
        break;
    case NX_CURSOR_NOT_ALLOWED:
        nx_cursor_ring(mask);
        nx_cursor_line(mask, 7, 25, 25, 7);
        nx_cursor_line(mask, 8, 26, 26, 8);
        break;
    case NX_CURSOR_BUSY:
        nx_cursor_ring(mask);
        nx_cursor_line(mask, 16, 16, 16, 8);
        nx_cursor_line(mask, 16, 16, 22, 19);
        break;
    default: break;
    }
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            int i = y * S + x;
            if (mask[i]) pixels[i] = 0xff151515u;
            else {
                int outline = 0;
                for (int dy = -1; dy <= 1 && !outline; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        int xx = x + dx, yy = y + dy;
                        if (xx >= 0 && xx < S && yy >= 0 && yy < S &&
                            mask[yy * S + xx]) outline = 1;
                    }
                pixels[i] = outline ? 0xffffffffu : 0u;
            }
        }
}
