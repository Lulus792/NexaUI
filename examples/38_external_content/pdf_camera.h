#ifndef EXAMPLE_PDF_CAMERA_H
#define EXAMPLE_PDF_CAMERA_H

#include <nexaui/nexaui.h>

enum { PDF_ZOOM_OUT=48, PDF_ZOOM_IN, PDF_ZOOM_FIT };
#define PDF_ZOOM_MAX 400u

typedef struct pdf_camera {
    unsigned percent;
    float center_x, center_y;
    int dragging;
    nx_point pointer;
} pdf_camera;

typedef struct pdf_camera_view {
    nx_rect viewport, image;
} pdf_camera_view;

void pdf_camera_fit(pdf_camera *camera);
pdf_camera_view pdf_camera_place(const pdf_camera *camera, nx_rect viewport);
/* Map a normalized image rectangle to its visible logical window extent. */
int pdf_camera_visible(pdf_camera_view view, nx_rect normalized, nx_rect *out);
/* Apply commands and current-frame events for the next presentation. */
nx_result pdf_camera_update(pdf_camera *camera, const nx_context *context, nx_rect viewport,
    uint64_t viewport_id, unsigned action, int *redraw);

#endif
