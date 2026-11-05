#ifndef NEXAUI_AETHERI_TERMINAL_VIEW_H
#define NEXAUI_AETHERI_TERMINAL_VIEW_H
#include <nexaui/cell_grid.h>
#include "model.h"
typedef struct aetheri_terminal_view aetheri_terminal_view;
/* The application supplies real combining-mark placement in logical pixels;
   this view does not shape text or infer a mark's position. The callback must
   not mutate the model, destroy the view or reenter it. */
typedef nx_result (*aetheri_terminal_mark_position)(void *user,uint32_t base,uint32_t mark,
    unsigned index,float *x,float *y);
typedef struct aetheri_terminal_view_desc {
    struct ae_terminal *model; /* borrowed; same owner thread */
    const nx_font *regular,*bold; /* borrowed; bold required if used */
    uint64_t text_identity; /* nonzero, changes for a replaced terminal */
    float cell_width,cell_height,baseline;
    double pixels_per_em;
    aetheri_terminal_mark_position mark_position;
    void *mark_user;
} aetheri_terminal_view_desc;
/* Optional app-owned overlay, called inside the frame after the terminal grid.
   It must not mutate the model, destroy/reenter the view or retain the context. */
typedef nx_result (*aetheri_terminal_overlay)(void *user,nx_context *context,
    const aetheri_terminal_view_desc *desc);
nx_result aetheri_terminal_view_create(const aetheri_terminal_view_desc *desc,aetheri_terminal_view **out);
void aetheri_terminal_view_destroy(aetheri_terminal_view *view);
/* revision increases on every owner model mutation, including selection clear
   and resize. Snapshot content is visible cells only; no history is copied. */
nx_result aetheri_terminal_view_frame(aetheri_terminal_view *view,uint64_t revision,float raster_scale);
nx_result aetheri_terminal_view_frame_with_overlay(aetheri_terminal_view *view,uint64_t revision,
    float raster_scale,aetheri_terminal_overlay overlay,void *user);
nx_context *aetheri_terminal_view_context(aetheri_terminal_view *view);
/* Borrowed until next frame/destroy; helpers reject a changed owner revision. */
nx_result aetheri_terminal_view_cells(const aetheri_terminal_view *view,uint64_t revision,
    const nx_cell_view **cells,size_t *rows,size_t *columns);
nx_result aetheri_terminal_view_text(const aetheri_terminal_view *view,uint64_t revision,
    nx_utf8_span *text,size_t *anchor,size_t *cursor,int *has_caret);
nx_result aetheri_terminal_view_hit(const aetheri_terminal_view *view,uint64_t revision,
    float x,float y,size_t *row,size_t *column);
/* Route one host-authorized NexaUI event to the original selection model.
   The caller must route modal/focus policy first, and rebuild with a strictly
   newer revision whenever changed is set before routing another event. Copy
   queued events before rebuilding: nx_frame_events storage expires at begin_frame. Capture
   is canceled on focus loss, pointer cancel, or a stale model. This is not a
   PTY, clipboard, IME or native-window event loop. */
nx_result aetheri_terminal_view_selection_event(aetheri_terminal_view *view,uint64_t revision,
    const nx_event *event,int *handled,int *changed);
#endif
