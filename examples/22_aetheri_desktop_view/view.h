#ifndef NEXAUI_AETHERI_DESKTOP_VIEW_H
#define NEXAUI_AETHERI_DESKTOP_VIEW_H
#include <nexaui/nexaui.h>
#include "model.h"
typedef struct aetheri_desktop_view aetheri_desktop_view;
typedef enum aetheri_desktop_intent_kind {
    AETHERI_DESKTOP_NONE, AETHERI_DESKTOP_SEARCH,
    AETHERI_DESKTOP_LAUNCH, AETHERI_DESKTOP_ACTIVATE, AETHERI_DESKTOP_QUERY
} aetheri_desktop_intent_kind;
typedef struct aetheri_desktop_intent {
    aetheri_desktop_intent_kind kind;
    enum ae_desktop_app app;
    uint32_t window;
    char query[AE_DESKTOP_QUERY];
} aetheri_desktop_intent;
/* Model and font bytes are borrowed until destroy. No process or role is owned. */
int aetheri_desktop_view_create(struct ae_desktop_model *model,const void *font,size_t length,aetheri_desktop_view **out);
nx_context *aetheri_desktop_view_context(aetheri_desktop_view *view);
/* revision must increase whenever the caller mutates the model, even if an ID
   or value is later reused. Events from an older revision are discarded. */
int aetheri_desktop_view_frame(aetheri_desktop_view *view,float width,float height,uint64_t revision,aetheri_desktop_intent *intent);
void aetheri_desktop_view_destroy(aetheri_desktop_view *view);
#endif
