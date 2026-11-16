#include "png_frame.h"

#include <png.h>
#include <stdlib.h>
#include <string.h>

nx_result content_png_frame(const void *encoded, size_t encoded_size, content_request request,
    uint64_t sequence, void **out, size_t *out_size) {
    if (!encoded || !encoded_size || !out || !out_size || !request.generation || !request.resize ||
        !request.width || !request.height || !sequence) return NX_ERROR_ARGUMENT;
    if (encoded_size > CONTENT_PNG_ENCODED_LIMIT || request.width > NX_VIEWPORT_MAX_DIMENSION ||
        request.height > NX_VIEWPORT_MAX_DIMENSION) return NX_ERROR_LIMIT;
    size_t stride = (size_t)request.width * 4u;
    if (stride > CONTENT_PNG_PIXEL_LIMIT / request.height) return NX_ERROR_LIMIT;

    png_image image = {.version = PNG_IMAGE_VERSION};
    nx_result result = NX_ERROR_ARGUMENT;
    uint8_t *decoded = NULL;
    if (!png_image_begin_read_from_memory(&image, encoded, encoded_size)) goto done;
    uint32_t width = image.width, height = image.height;
    if (!width || !height || width > CONTENT_PNG_SOURCE_DIMENSION || height > CONTENT_PNG_SOURCE_DIMENSION ||
        (size_t)width * 4u > CONTENT_PNG_PIXEL_LIMIT / height) {
        result = NX_ERROR_LIMIT;
        goto done;
    }
    decoded = malloc((size_t)width * height * 4u);
    if (!decoded) { result = NX_ERROR_MEMORY; goto done; }
    image.format = PNG_FORMAT_RGBA;
    if (!png_image_finish_read(&image, NULL, decoded, 0, NULL)) goto done;

    size_t bytes = sizeof(content_frame_header) + stride * request.height;
    uint8_t *data = calloc(1, bytes);
    if (!data) { result = NX_ERROR_MEMORY; goto done; }
    content_frame_header header = {.request=request, .sequence=sequence,
        .format=NX_BITMAP_RGBA8, .stride=(uint32_t)stride};
    memcpy(data, &header, sizeof(header));

    uint32_t fit_width = request.width, fit_height = request.height;
    /* Dimension bounds keep products below UINT32_MAX. */
    if (fit_width * height > fit_height * width) fit_width = fit_height * width / height;
    else fit_height = fit_width * height / width;
    if (!fit_width) fit_width = 1;
    if (!fit_height) fit_height = 1;
    uint8_t *dest = data + sizeof(header) + ((request.height - fit_height) / 2u) * stride +
        ((request.width - fit_width) / 2u) * 4u;
    for (uint32_t y = 0; y < fit_height; ++y) {
        const uint8_t *source = decoded + (size_t)(y * height / fit_height) * width * 4u;
        uint8_t *row = dest + (size_t)y * stride;
        for (uint32_t x = 0; x < fit_width; ++x)
            memcpy(row + (size_t)x * 4u, source + (size_t)(x * width / fit_width) * 4u, 4u);
    }
    *out = data;
    *out_size = bytes;
    result = NX_OK;
done:
    free(decoded);
    png_image_free(&image);
    return result;
}
