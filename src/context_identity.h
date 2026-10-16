#ifndef NEXAUI_CONTEXT_IDENTITY_H
#define NEXAUI_CONTEXT_IDENTITY_H
#include "nexaui/nexaui.h"
/* Private native transport binding identity. Owner-thread only for context reads; the
   native adapter supplies a saturated process-wide atomic sequence. No borrowed
   context pointer survives publication or is dereferenced by a native query. */
#if defined(_WIN32) || defined(__APPLE__)
uint64_t nx_native_next_identity(void);
uint64_t nx_context_identity(const nx_context *context);
uint64_t nx_context_frame_serial(const nx_context *context);
#endif
#endif
