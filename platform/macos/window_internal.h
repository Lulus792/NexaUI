#ifndef NEXAUI_MACOS_WINDOW_INTERNAL_H
#define NEXAUI_MACOS_WINDOW_INTERNAL_H

#include "nexaui/nexaui.h"

/* Main-thread-only native access and renderer lease. A leased window cannot
   be destroyed; retire and release the renderer before destroying its owner. */
nx_result nx_window_renderer_acquire(nx_window *window, const void *owner);
nx_result nx_window_renderer_release(nx_window *window, const void *owner);
#ifdef __OBJC__
@class NSWindow, NSView;
NSWindow *nx_window_native_nswindow(nx_window *window);
NSView *nx_window_native_nsview(nx_window *window);
#endif

#endif
