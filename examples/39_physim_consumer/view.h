#ifndef EXAMPLE_PHYSIM_VIEW_H
#define EXAMPLE_PHYSIM_VIEW_H
#include "model.h"
typedef struct physim_view physim_view;
/* App-owned reference presentation, not Physim's approved application design.
   Borrow model, original plot view and fonts until destroy. The model must be
   an immutable successful physim_model_run result; only apply may replace it.
   One owner thread; no callbacks may mutate/destroy these borrowed objects. */
nx_result physim_view_create(physim_snapshot *model, ps_plot_view *plot,
    const nx_font_collection *fonts, physim_view **out);
/* Owns begin/end_frame. Returns action IDs 12 Apply, 13 Zoom, 14 Pan, 15 Fit,
   16 Close. Caller dispatches these only after rendering/complete. All borrowed
   commands/pixels must finish before complete, next frame or destruction.
   Bounded reference layout: logical width>=640 and height>=400. */
nx_result physim_view_frame(physim_view *view, nx_context *context,
    float width, float height, double scale, unsigned *action);
nx_result physim_view_complete(physim_view *view);
/* Confirms both drafts against original bounds; runs original model into a
   separate result before publishing it. Invalid drafts preserve the model and
   return OK/valid=0. Generation increases once on success. A live lease blocks
   apply/dispatch/destroy. Zoom/pan/fit call original ps_plot_view functions. */
nx_result physim_view_dispatch(physim_view *view, unsigned action, int *valid);
nx_result physim_view_destroy(physim_view **view);
#endif
