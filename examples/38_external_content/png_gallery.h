#ifndef CONTENT_PNG_GALLERY_H
#define CONTENT_PNG_GALLERY_H

#include "surface.h"

typedef struct png_gallery png_gallery;

/* Read the host directory's ordered gallery.txt and pin each regular file.
   The directory is selected by the host. Names are UTF-8 leaf names and
   must be unique within the collection. */
nx_result png_gallery_open(const char *directory,png_gallery **out);
void png_gallery_close(png_gallery *gallery);
unsigned png_gallery_count(const png_gallery *gallery);
const char *png_gallery_name(const png_gallery *gallery,unsigned image);

/* One-based image index. Transfer the fitted frame and optionally the
   original packed raster to the caller on success. */
nx_result png_gallery_frame(const png_gallery *gallery,unsigned image,content_request request,
    uint64_t sequence,void **out,size_t *bytes,nx_bitmap_view *original);

#endif
