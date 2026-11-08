#ifndef NEXAUI_EDITOR_IME_BATCH_H
#define NEXAUI_EDITOR_IME_BATCH_H
#include "ime_model.h"
#include <nexaui/nexaui.h>

/* App-owned bounded transaction collector; the original document remains the
   only text authority. No window, focus, file, clipboard or native dispatch.
   The host must admit the current window/document/focus/target/native revision
   for each event, opt in to complete SDK boundaries before IME enable, and
   invalidate on context changes, native errors and teardown. Do not infer END
   from queue exhaustion. A rejected batch withdraws this binding until rebind.
   Own successful batches update the original token, allowing already queued
   batches of the same advertised native revision. Publish a fresh surrounding
   context after that queue drains; external input must invalidate immediately. */
enum editor_ime_batch_outcome { EDITOR_IME_NOT_NATIVE,EDITOR_IME_BUFFERED,
    EDITOR_IME_APPLIED,EDITOR_IME_REJECTED };
struct editor_ime_batch {
    uint64_t target,revision;
    struct ae_direct_token expected;
    uint32_t flags;
    nx_event deletion;
    size_t bytes;
    bool collecting,has_delete,has_text,text_ended;
    enum ae_document_result result;
    char commit[AE_DOCUMENT_LINE_BYTES];
};
bool editor_ime_batch_bind(struct editor_ime_batch *,struct ae_direct *,uint64_t target,uint64_t revision);
void editor_ime_batch_invalidate(struct editor_ime_batch *);
enum editor_ime_batch_outcome editor_ime_batch_event(struct editor_ime_batch *,
    struct ae_direct *,const nx_event *,bool admitted);
#endif
