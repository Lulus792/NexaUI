#ifndef NEXAUI_AETHERI_DESKTOP_VIEW_H
#define NEXAUI_AETHERI_DESKTOP_VIEW_H
#include <nexaui/nexaui.h>
#include "model.h"
typedef struct aetheri_desktop_view aetheri_desktop_view;
typedef enum aetheri_desktop_intent_kind {
    AETHERI_DESKTOP_NONE, AETHERI_DESKTOP_SEARCH,
    AETHERI_DESKTOP_LAUNCH, AETHERI_DESKTOP_ACTIVATE, AETHERI_DESKTOP_QUERY,
    AETHERI_DESKTOP_SELECT
} aetheri_desktop_intent_kind;
typedef struct aetheri_desktop_intent {
    aetheri_desktop_intent_kind kind;
    enum ae_desktop_app app;
    uint32_t window;
    char query[AE_DESKTOP_QUERY];
} aetheri_desktop_intent;
/* Model and font bytes are borrowed until destroy. No process or role is owned. */
int aetheri_desktop_view_create(struct ae_desktop_model *model,const void *font,size_t length,aetheri_desktop_view **out);
int aetheri_desktop_view_create_with_context(struct ae_desktop_model *model,const void *font,size_t length,
    const nx_context_desc *context_desc,aetheri_desktop_view **out);
nx_context *aetheri_desktop_view_context(aetheri_desktop_view *view);
/* Optional ordinary NexaUI window for IME and clipboard. The caller keeps it
   alive until this is cleared and the view completes another frame. */
nx_result aetheri_desktop_view_set_window(aetheri_desktop_view *view,nx_window *window);
/* External IME and ordinary-window IME are mutually exclusive. */
nx_result aetheri_desktop_view_set_host_ime(aetheri_desktop_view *view,int enabled);
nx_result aetheri_desktop_view_host_ime_revision(aetheri_desktop_view *view,uint64_t *out_revision);
nx_result aetheri_desktop_view_host_ime_preedit(aetheri_desktop_view *view,uint64_t revision,
    const char *text,size_t length,int32_t begin,int32_t end);
nx_result aetheri_desktop_view_host_ime_caret(aetheri_desktop_view *view,nx_rect *out_rect);
nx_result aetheri_desktop_view_host_ime_surrounding(aetheri_desktop_view *view,uint64_t revision,
    char *bytes,size_t capacity,size_t *length,size_t *cursor,size_t *anchor);
/* Queue real surface keyboard focus changes before processing input. */
nx_result aetheri_desktop_view_surface_focus(aetheri_desktop_view *view,int focused);
/* Queue host-translated events here. Tab/Shift-Tab in the search field map to
   next/previous result; native-consumed and modified keys pass through.
   Ctrl-Tab/Ctrl-Shift-Tab traverse focus between search and dock controls. */
nx_result aetheri_desktop_view_queue_event(aetheri_desktop_view *view,const nx_event *event);
/* revision must increase whenever the caller mutates the model, even if an ID
   or value is later reused. Events from an older revision are discarded. */
int aetheri_desktop_view_frame(aetheri_desktop_view *view,float width,float height,uint64_t revision,aetheri_desktop_intent *intent);
void aetheri_desktop_view_destroy(aetheri_desktop_view *view);
#endif
