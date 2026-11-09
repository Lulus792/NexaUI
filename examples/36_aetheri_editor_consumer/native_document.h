#ifndef NEXAUI_EDITOR_NATIVE_DOCUMENT_H
#define NEXAUI_EDITOR_NATIVE_DOCUMENT_H
#include "accessible_model.h"

/* Owner-only projection correlation, not a document/history model. A global
   monotone serial also invalidates policy-only modal roundtrips and tab changes. */
struct editor_native_document {
    struct ae_direct_token token;
    uint64_t document,content_revision,serial;
    size_t anchor,cursor;
    uint32_t operations;
    bool changed;
    const char *notice;
};
#endif
