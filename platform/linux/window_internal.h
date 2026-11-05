#ifndef NEXAUI_LINUX_WINDOW_INTERNAL_H
#define NEXAUI_LINUX_WINDOW_INTERNAL_H

#include "nexaui/nexaui.h"

/* Owner-thread references used while pumping other native windows. */
int nx_linux_window_hold(nx_window *window);
int nx_linux_window_retired(nx_window *window);
void nx_linux_window_unhold(nx_window *window);

/* Private owner-thread render lease. At most one renderer may use a native
   window. The window must outlive the lease. Wayland background commits are
   suppressed while leased; release restores the platform background. */
nx_result nx_window_renderer_acquire(nx_window *window, const void *owner);
nx_result nx_window_renderer_release(nx_window *window, const void *owner);

#endif
