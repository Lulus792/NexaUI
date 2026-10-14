#include "nexaui/nexaui.h"

#include <math.h>

static int nx_color_component(float value) {
    return isfinite(value) && value >= 0.0f && value <= 1.0f;
}

static int nx_color_valid(nx_color color) {
    return nx_color_component(color.r) && nx_color_component(color.g) &&
           nx_color_component(color.b) && nx_color_component(color.a);
}

nx_result nx_color_to_hsv(nx_color color, nx_hsv *out_hsv) {
    if (!out_hsv || !nx_color_valid(color)) return NX_ERROR_ARGUMENT;
    double r = color.r, g = color.g, b = color.b;
    double maximum = fmax(r, fmax(g, b));
    double minimum = fmin(r, fmin(g, b));
    double delta = maximum - minimum;
    double hue = 0.0;
    if (delta > 0.0) {
        if (maximum == r) hue = fmod((g - b) / delta, 6.0);
        else if (maximum == g) hue = (b - r) / delta + 2.0;
        else hue = (r - g) / delta + 4.0;
        hue /= 6.0;
        if (hue < 0.0) hue += 1.0;
    }
    *out_hsv = (nx_hsv){(float)hue,
                        (float)(maximum > 0.0 ? delta / maximum : 0.0),
                        (float)maximum, color.a};
    return NX_OK;
}

nx_result nx_color_from_hsv(nx_hsv hsv, nx_color *out_color) {
    if (!out_color || !nx_color_component(hsv.h) ||
        !nx_color_component(hsv.s) || !nx_color_component(hsv.v) ||
        !nx_color_component(hsv.a)) return NX_ERROR_ARGUMENT;
    double position = hsv.h == 1.0f ? 0.0 : (double)hsv.h * 6.0;
    double chroma = (double)hsv.v * hsv.s;
    double secondary = chroma * (1.0 - fabs(fmod(position, 2.0) - 1.0));
    double offset = (double)hsv.v - chroma;
    double r = 0.0, g = 0.0, b = 0.0;
    if (position < 1.0) { r = chroma; g = secondary; }
    else if (position < 2.0) { r = secondary; g = chroma; }
    else if (position < 3.0) { g = chroma; b = secondary; }
    else if (position < 4.0) { g = secondary; b = chroma; }
    else if (position < 5.0) { r = secondary; b = chroma; }
    else { r = chroma; b = secondary; }
    *out_color = (nx_color){(float)(r + offset), (float)(g + offset),
                            (float)(b + offset), hsv.a};
    return NX_OK;
}

static int nx_hex_digit(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

nx_result nx_color_parse_hex(nx_utf8_span text, nx_color *out_color) {
    if (!out_color || !text.data ||
        (text.length != 4 && text.length != 5 &&
         text.length != 7 && text.length != 9) ||
        text.data[0] != '#') return NX_ERROR_ARGUMENT;
    unsigned bytes[4] = {0, 0, 0, 255};
    int short_form = text.length <= 5;
    size_t components = text.length == 5 || text.length == 9 ? 4u : 3u;
    for (size_t i = 0; i < components; ++i) {
        int high = nx_hex_digit((unsigned char)text.data[1u + i *
                                       (short_form ? 1u : 2u)]);
        if (high < 0) return NX_ERROR_ARGUMENT;
        if (short_form) bytes[i] = (unsigned)(high * 17);
        else {
            int low = nx_hex_digit((unsigned char)text.data[2u + i * 2u]);
            if (low < 0) return NX_ERROR_ARGUMENT;
            bytes[i] = (unsigned)(high * 16 + low);
        }
    }
    *out_color = (nx_color){bytes[0] / 255.0f, bytes[1] / 255.0f,
                            bytes[2] / 255.0f, bytes[3] / 255.0f};
    return NX_OK;
}

nx_result nx_color_format_hex(nx_color color, int include_alpha,
                              char *bytes, size_t capacity,
                              size_t *out_length) {
    if (!out_length) return NX_ERROR_ARGUMENT;
    *out_length = 0;
    if (!nx_color_valid(color)) return NX_ERROR_ARGUMENT;
    size_t length = include_alpha ? 9u : 7u;
    *out_length = length;
    if (!bytes) return NX_OK;
    if (capacity <= length) return NX_ERROR_LIMIT;
    const char digits[] = "0123456789ABCDEF";
    const float components[4] = {color.r, color.g, color.b, color.a};
    bytes[0] = '#';
    for (size_t i = 0; i < (include_alpha ? 4u : 3u); ++i) {
        unsigned value = (unsigned)floor((double)components[i] * 255.0 + 0.5);
        bytes[1u + i * 2u] = digits[value >> 4u];
        bytes[2u + i * 2u] = digits[value & 15u];
    }
    bytes[length] = 0;
    return NX_OK;
}
