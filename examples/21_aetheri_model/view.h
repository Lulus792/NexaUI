#ifndef NEXAUI_AETHERI_MODEL_VIEW_H
#define NEXAUI_AETHERI_MODEL_VIEW_H
#include <nexaui/nexaui.h>
#include "model.h"
typedef struct aetheri_view aetheri_view;
/* The caller owns model and font bytes, which outlive the view. */
int aetheri_view_create(struct ae_settings *model, const void *font, size_t length, aetheri_view **out);
nx_context *aetheri_view_context(aetheri_view *view);
int aetheri_view_frame(aetheri_view *view, float width, float height, int *changed);
int aetheri_view_close_requested(const aetheri_view *view);
void aetheri_view_destroy(aetheri_view *view);
#endif
