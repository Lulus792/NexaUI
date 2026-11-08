#ifndef NEXAUI_EDITOR_IME_HOST_H
#define NEXAUI_EDITOR_IME_HOST_H
#include "ime_batch.h"
struct editor_ime_host {
    struct editor_ime_batch batch;
    struct ae_direct_token published;
    uint64_t serial,semantic;
    bool available,owns,failed,caret_valid,rect_sent;
    nx_rect caret,sent;
    char preedit[4001];size_t length;
    const char *notice; /* Borrowed translated string literal, never model text. */
    int32_t anchor,cursor;
};
struct app;
struct painted_row;
static bool editor_ime_initialize(struct app *);
static void editor_ime_cancel(struct app *);
static bool editor_ime_frame(struct app *);
static bool editor_ime_event(struct app *,const nx_event *,bool *);
static bool editor_ime_idle(struct app *);
static bool editor_ime_paint(struct app *,const struct painted_row *,nx_point,const nx_text_caret *);
#endif
