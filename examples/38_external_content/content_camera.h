#ifndef EXAMPLE_CONTENT_CAMERA_H
#define EXAMPLE_CONTENT_CAMERA_H

#include <nexaui/nexaui.h>

enum { CONTENT_ZOOM_OUT=48, CONTENT_ZOOM_IN, CONTENT_ZOOM_FIT };
#define CONTENT_ZOOM_MAX 400u

typedef struct content_camera {
    unsigned percent;
    float center_x, center_y;
    int dragging;
    nx_point pointer;
} content_camera;

typedef struct content_camera_view {
    nx_rect viewport, image, fitted;
} content_camera_view;

void content_camera_fit(content_camera *camera);
/* fitted is the area occupied by the image in the unzoomed padded canvas,
   in unit coordinates. The image remains centered on a non-scrollable axis. */
content_camera_view content_camera_place(const content_camera *camera, nx_rect viewport, nx_rect fitted);
/* Map a normalized image rectangle to its visible logical window extent. */
int content_camera_visible(content_camera_view view, nx_rect normalized, nx_rect *out);
/* Apply commands and current-frame events for the next presentation. */
nx_result content_camera_update(content_camera *camera, const nx_context *context, content_camera_view view,
    uint64_t viewport_id, unsigned action, int *redraw);

#endif
