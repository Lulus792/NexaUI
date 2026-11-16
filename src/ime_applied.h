#ifndef NEXAUI_IME_APPLIED_H
#define NEXAUI_IME_APPLIED_H

#include "nexaui/nexaui.h"

/* Private, owner-thread feedback from built-in editors. Only a successfully
   accepted native edit may witness a before/after revision. No allocation,
   callbacks, borrowed text or new public ABI. Other platforms need no feedback. */
#if defined(NEXAUI_BUILD_LINUX)
#if defined(__GNUC__) || defined(__clang__)
__attribute__((visibility("hidden")))
#endif
void nx_linux_window_ime_applied(nx_window *window, uint64_t target,
    uint64_t source_revision, uint64_t before_revision, uint64_t after_revision);
#endif

#endif
