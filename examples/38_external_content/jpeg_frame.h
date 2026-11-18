#ifndef EXAMPLE_CONTENT_JPEG_FRAME_H
#define EXAMPLE_CONTENT_JPEG_FRAME_H

#include "surface.h"

/* Decode ordinary 8-bit lossy JPEG into owned, oriented RGBA8. The raster may
   be scaled to the shared gallery budget. Free out->pixels on success. The
   original dimensions describe the oriented image. Errors leave outputs intact. */
nx_result content_jpeg_bitmap(const void *encoded,size_t size,nx_bitmap_view *out,
    content_jpeg_size *original);

#endif
