#include "png_frame.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) { fprintf(stderr, "line %d: %s\n", __LINE__, #value); goto done; } } while (0)

static uint8_t *encode(uint32_t width, uint32_t height, const void *pixels, size_t *bytes) {
    png_image image = {.version=PNG_IMAGE_VERSION, .width=width, .height=height, .format=PNG_FORMAT_RGBA};
    png_alloc_size_t size = 0;
    if (!png_image_write_to_memory(&image, NULL, &size, 0, pixels, 0, NULL)) return NULL;
    uint8_t *data = malloc(size);
    if (!data) return NULL;
    if (!png_image_write_to_memory(&image, data, &size, 0, pixels, 0, NULL)) {
        free(data);
        return NULL;
    }
    *bytes = size;
    return data;
}

int main(void) {
    const uint8_t pixels[] = {255,0,0,255, 0,128,200,153, 0,255,0,64, 93,7,121,255};
    const uint8_t expected[] = {
        0,0,0,0, 255,0,0,255, 0,128,200,153, 0,0,0,0,
        0,0,0,0, 0,255,0,64, 93,7,121,255, 0,0,0,0
    };
    size_t encoded_size = 0, oversized_size = 0, bytes = 0;
    uint8_t *encoded = encode(2, 2, pixels, &encoded_size);
    uint8_t *oversized = NULL, *row = calloc(4096u, 4u);
    void *frame = NULL;
    int ok = 0;
    CHECK(encoded && row);
    content_request request = {17, 2, 4, 2};
    CHECK(content_png_frame(encoded, encoded_size, request, 8, &frame, &bytes) == NX_OK);
    CHECK(bytes == sizeof(content_frame_header) + sizeof(expected));
    content_frame_header header;
    memcpy(&header, frame, sizeof(header));
    CHECK(header.request.generation == 17 && header.request.resize == 2 && header.request.width == 4 &&
        header.request.height == 2 && header.sequence == 8 && header.stride == 16 &&
        header.format == NX_BITMAP_RGBA8 && !header.damage_base);
    CHECK(!memcmp((const uint8_t *)frame + sizeof(header), expected, sizeof(expected)));
    free(frame);
    frame = NULL;

    void *sentinel = &header;
    size_t untouched = 71;
    CHECK(content_png_frame(encoded, 8, request, 9, &sentinel, &untouched) == NX_ERROR_ARGUMENT);
    CHECK(content_png_frame(encoded, CONTENT_PNG_ENCODED_LIMIT + 1u, request, 9,
        &sentinel, &untouched) == NX_ERROR_LIMIT);
    CHECK(content_png_frame(encoded, encoded_size, (content_request){17,2,2048,2048}, 9,
        &sentinel, &untouched) == NX_ERROR_LIMIT);
    oversized = encode(4096, 1, row, &oversized_size);
    CHECK(oversized && content_png_frame(oversized, oversized_size, request, 9,
        &sentinel, &untouched) == NX_ERROR_LIMIT);
    CHECK(sentinel == &header && untouched == 71);
    CHECK(content_png_frame(encoded, encoded_size, (content_request){17,3,1,1}, 1, &frame, &bytes) == NX_OK);
    CHECK(bytes == sizeof(header) + 4 && !memcmp((const uint8_t *)frame + sizeof(header), pixels, 4));
    puts("PNG preview verified: straight RGBA, fit, bounds and retry");
    ok = 1;
done:
    free(row);
    free(encoded);
    free(oversized);
    free(frame);
    return ok ? 0 : 1;
}
