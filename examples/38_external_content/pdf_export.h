#ifndef CONTENT_PDF_EXPORT_H
#define CONTENT_PDF_EXPORT_H

#include <nexaui/nexaui.h>

typedef struct pdf_export_view {
    nx_rect viewport, page;
    uint32_t width, height; /* Requested logical canvas. */
    double scale;
} pdf_export_view;

/* The caller owns the immutable pinned file and serializes its cursor.
   Render the captured page view into a white canvas preserving its aspect.
   Physical dimensions are ceil(logical * scale). On success free the
   returned packed RGBA8 pixels. Failures leave out untouched. */
nx_result content_pdf_export_render(const char *renderer, int crop, int file, unsigned page,
    pdf_export_view view, nx_bitmap_view *out);

/* Publish a complete PNG at the absolute host-selected path. Existing files
   are preserved and return NX_ERROR_STATE. The caller retains the raster. */
nx_result content_pdf_export_png(const char *path, nx_bitmap_view image);

#endif
