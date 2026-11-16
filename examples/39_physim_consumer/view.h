#ifndef EXAMPLE_PHYSIM_VIEW_H
#define EXAMPLE_PHYSIM_VIEW_H
#include "model.h"
typedef struct physim_view physim_view;
enum { PHYSIM_INSPECT_NEXT = 22, PHYSIM_INSPECT_PICK = 24 };
/* App-owned reference presentation, not Physim's approved application design.
   Borrow model, original plot view and fonts until destroy. The model must be
   an immutable successful physim_model_run result; only apply may replace it.
   One owner thread; no callbacks may mutate/destroy these borrowed objects. */
nx_result physim_view_create(physim_snapshot *model, ps_plot_view *plot,
    const nx_font_collection *fonts, physim_view **out);
/* Optional native numeric IME binding. Window must outlive this view and its
   caller context. Call on their owner thread outside any live render lease.
   On a setter failure retire the view before releasing either window. */
nx_result physim_view_set_ime_window(physim_view *view, nx_window *window);
/* Owns begin/end_frame. Returns action IDs 12 Apply, 13 Zoom, 14 Pan, 15 Fit,
   16 Close, 17 Replace view, 22 Inspect next, 24 Viewport pick. Caller handles
   Close/Replace and dispatches other actions only after rendering/complete.
   Dispatch a viewport pick before the next frame. All borrowed
   commands/pixels must finish before complete, next frame or destruction.
   Bounded reference layout: logical width>=640 and height>=400. */
nx_result physim_view_frame(physim_view *view, nx_context *context,
    float width, float height, double scale, unsigned *action);
nx_result physim_view_complete(physim_view *view);
/* Confirms both drafts against original bounds; runs original model into a
   separate result before publishing it. Invalid drafts preserve the model and
   return OK/valid=0. Generation increases once on success. A live lease blocks
   apply/dispatch/destroy. Zoom/pan/fit call original ps_plot_view functions.
   Inspection follows the published scene order and resets on valid Apply. */
nx_result physim_view_dispatch(physim_view *view, unsigned action, int *valid);
nx_result physim_view_destroy(physim_view **view);

typedef struct physim_export_image {
    uint8_t *pixels;
    uint32_t width, height;
    size_t stride;
} physim_export_image;

/* Render an immutable published snapshot and a copy of the original plot range
   in a separate context. Use the same owner thread as the live view, if any.
   Logical size is 640..2048 by 400..1200, scale is finite in [0.5, 2].
   The packed RGBA8 dimensions round each scaled extent up. Zero-initialize
   out, which is assigned only on success and holds at most 40 MiB. A live
   output must be released before reuse. The caller owns file encoding. */
nx_result physim_export_render(const physim_snapshot *model, const ps_plot_view *plot,
    const nx_font_collection *fonts, uint32_t width, uint32_t height, double scale,
    physim_export_image *out);
void physim_export_release(physim_export_image *image);
#endif
