#ifndef NEXAUI_EDITOR_ACCESSIBLE_MODEL_H
#define NEXAUI_EDITOR_ACCESSIBLE_MODEL_H
#include "ime_model.h"

/* Original document/controller, one owner thread; no SDK text buffer, native
   object, focus/window grant or clipboard side effect. Scalar offsets count
   Unicode scalar values, including both CR and LF. end=-1 means document end
   for read/replace only. Read may address a scalar inside a grapheme or CRLF;
   edits retain original scalar/CRLF preflight and final grapheme caret. Selection
   endpoints must both be original grapheme boundaries, in either direction.
   All six original token fields must match. Invalid/stale/OOM requests preserve
   content, selection, history and correlation; successful mutations consume the
   old token once. Selection preflights both endpoints before changing anything.
   Input/output buffers must not alias document storage or each other. */
struct editor_accessible_state {
    struct ae_direct_token token;
    size_t bytes,characters,anchor,cursor;
};
bool editor_accessible_snapshot(const struct ae_direct *,struct editor_accessible_state *);
/* Query with out=NULL,capacity=0. On LIMIT, *length reports the required byte
   count and out remains untouched. No terminator is written. Reads allocate
   nothing and do not consume input tokens or end typing/history coalescing. */
enum ae_document_result editor_accessible_read(const struct ae_direct *,
    const struct ae_direct_token *,int64_t start,int64_t end,
    char *out,size_t capacity,size_t *length);
enum ae_document_result editor_accessible_replace(struct ae_direct *,
    const struct ae_direct_token *,int64_t start,int64_t end,
    const char *text,size_t length);
bool editor_accessible_select(struct ae_direct *,const struct ae_direct_token *,
    int64_t anchor,int64_t cursor);
#endif
