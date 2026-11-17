#include "png_frame.h"

#include <png.h>
#include <stdlib.h>
#include <string.h>

nx_result content_png_bitmap(const void *encoded, size_t encoded_size, nx_bitmap_view *out) {
    if (!encoded || !encoded_size || !out) return NX_ERROR_ARGUMENT;
    if (encoded_size>CONTENT_PNG_ENCODED_LIMIT) return NX_ERROR_LIMIT;

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
    *out=(nx_bitmap_view){decoded,width,height,(size_t)width*4u,NX_BITMAP_RGBA8};
    decoded=NULL;
    result=NX_OK;
done:
    free(decoded);
    png_image_free(&image);
    return result;
}

static nx_result check_frame(content_request request,uint64_t sequence,void **out,size_t *out_size) {
    if (!out || !out_size || !request.generation || !request.resize ||
        !request.width || !request.height || !sequence) return NX_ERROR_ARGUMENT;
    if (request.width > NX_VIEWPORT_MAX_DIMENSION || request.height > NX_VIEWPORT_MAX_DIMENSION)
        return NX_ERROR_LIMIT;
    return (size_t)request.width*4u>CONTENT_PNG_PIXEL_LIMIT/request.height?NX_ERROR_LIMIT:NX_OK;
}

nx_result content_png_fit_bitmap(nx_bitmap_view bitmap,content_request request,
    uint64_t sequence,void **out,size_t *out_size,content_png_fit *fit) {
    nx_result checked=check_frame(request,sequence,out,out_size);
    if (checked!=NX_OK) return checked;
    uint32_t width=bitmap.width,height=bitmap.height;
    if (!bitmap.pixels || bitmap.format!=NX_BITMAP_RGBA8 || !width || !height ||
        width>CONTENT_PNG_SOURCE_DIMENSION || height>CONTENT_PNG_SOURCE_DIMENSION ||
        bitmap.stride_bytes!=(size_t)width*4u || bitmap.stride_bytes>CONTENT_PNG_PIXEL_LIMIT/height)
        return NX_ERROR_ARGUMENT;
    size_t stride=(size_t)request.width*4u;

    size_t bytes = sizeof(content_frame_header) + stride * request.height;
    uint8_t *data = calloc(1, bytes);
    if (!data) return NX_ERROR_MEMORY;

    uint32_t fit_width = request.width, fit_height = request.height;
    /* Dimension bounds keep products below UINT32_MAX. */
    if (fit_width * height > fit_height * width) fit_width = fit_height * width / height;
    else fit_height = fit_width * height / width;
    if (!fit_width) fit_width = 1;
    if (!fit_height) fit_height = 1;
    uint32_t x_offset = (request.width - fit_width) / 2u;
    uint32_t y_offset = (request.height - fit_height) / 2u;
    content_frame_header header = {.request=request, .sequence=sequence,
        .format=NX_BITMAP_RGBA8,.stride=(uint32_t)stride,
        .image={x_offset,y_offset,fit_width,fit_height,width,height}};
    memcpy(data,&header,sizeof(header));
    uint8_t *dest = data + sizeof(header) + y_offset * stride + x_offset * 4u;
    for (uint32_t y = 0; y < fit_height; ++y) {
        const uint8_t *source = bitmap.pixels + (size_t)(y * height / fit_height) * bitmap.stride_bytes;
        uint8_t *row = dest + (size_t)y * stride;
        for (uint32_t x = 0; x < fit_width; ++x)
            memcpy(row + (size_t)x * 4u, source + (size_t)(x * width / fit_width) * 4u, 4u);
    }
    *out = data;
    *out_size = bytes;
    if (fit) *fit = (content_png_fit){width, height, x_offset, y_offset, fit_width, fit_height};
    return NX_OK;
}

nx_result content_png_frame_fit(const void *encoded, size_t encoded_size, content_request request,
    uint64_t sequence, void **out, size_t *out_size, content_png_fit *fit) {
    if (!encoded || !encoded_size) return NX_ERROR_ARGUMENT;
    nx_result result=check_frame(request,sequence,out,out_size);
    if (result!=NX_OK) return result;
    nx_bitmap_view bitmap;
    result=content_png_bitmap(encoded,encoded_size,&bitmap);
    if (result==NX_OK) {
        result=content_png_fit_bitmap(bitmap,request,sequence,out,out_size,fit);
        free((void *)bitmap.pixels);
    }
    return result;
}

nx_result content_png_frame(const void *encoded, size_t encoded_size, content_request request,
    uint64_t sequence, void **out, size_t *out_size) {
    return content_png_frame_fit(encoded, encoded_size, request, sequence, out, out_size, NULL);
}
