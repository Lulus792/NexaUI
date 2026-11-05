#ifndef NEXAUI_WAYLAND_PIXELS_H
#define NEXAUI_WAYLAND_PIXELS_H
#include <stddef.h>
#include <stdint.h>

/* Convert straight byte-ordered RGBA8 to native-endian premultiplied ARGB. */
static void nx_wayland_premultiply_argb(void *pixels, size_t size) {
    uint8_t *rgba=pixels;
    uint32_t *argb=pixels;
    for (size_t i=0; i<size/4u; ++i) {
        unsigned r=rgba[i*4u], g=rgba[i*4u+1u], b=rgba[i*4u+2u], a=rgba[i*4u+3u];
        argb[i]=(a<<24u)|(((r*a+127u)/255u)<<16u)|
                (((g*a+127u)/255u)<<8u)|((b*a+127u)/255u);
    }
}
#endif
