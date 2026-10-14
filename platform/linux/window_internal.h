#ifndef NEXAUI_LINUX_WINDOW_INTERNAL_H
#define NEXAUI_LINUX_WINDOW_INTERNAL_H

#include "nexaui/nexaui.h"

/* Private owner-thread render lease. At most one renderer may use a native
   window. The window must outlive the lease. Wayland background commits are
   suppressed while leased; release restores the platform background. */
nx_result nx_window_renderer_acquire(nx_window *window, const void *owner);
nx_result nx_window_renderer_release(nx_window *window, const void *owner);

#endif
