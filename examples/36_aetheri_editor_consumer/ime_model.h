#ifndef NEXAUI_EDITOR_IME_MODEL_H
#define NEXAUI_EDITOR_IME_MODEL_H
#include "editor/direct.h"

/* App-owned integration seam, not an SDK document model or a grant.
   The live original controller must still match every field of its captured
   token. The caller has already admitted one complete native transaction and
   computes its UTF-8 byte range against that original context. Payload must not
   alias document storage. UTF-8 scalar endpoints may lie inside old graphemes;
   the original document preflight computes the final grapheme-aligned caret.
   Partial scalars/CRLF, unsupported controls, limits and allocation failure
   preserve document, selection, history and input correlation. Success records
   one replacement/undo group and consumes the old controller token. Empty
   payload can delete a nonempty range; a successful no-op is an input gesture.
   Focus/window/tab/native generation checks remain the owning host's duty. */
enum ae_document_result editor_direct_ime_replace(struct ae_direct *input,
    const struct ae_direct_token *token,size_t begin,size_t end,
    const char *text,size_t size);
#endif
