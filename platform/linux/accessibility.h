#ifndef NEXAUI_LINUX_ACCESSIBILITY_H
#define NEXAUI_LINUX_ACCESSIBILITY_H
#include "nexaui/nexaui.h"

/* Owner-thread-only transport. No context pointer survives publish. Native IPC
   is optional; missing bus/SDK/runtime must not disable the window itself. */
typedef struct nx_linux_ax nx_linux_ax;
typedef struct nx_linux_ax_geometry {
    double x, y, width, height;
    int screen_position, visible, focused;
} nx_linux_ax_geometry;
nx_result nx_linux_ax_publish(nx_linux_ax **bridge, nx_window *window,
                             nx_allocator allocator, const nx_context *context);
void nx_linux_ax_destroy(nx_linux_ax *bridge);
int nx_linux_ax_matches(nx_linux_ax *bridge, const nx_context *context);
nx_result nx_linux_ax_route(nx_linux_ax *bridge, nx_context *context, size_t *count);
int nx_linux_ax_pump(nx_linux_ax *bridge);
int nx_linux_ax_fd(nx_linux_ax *bridge, int *writable);
void nx_linux_window_ax_geometry(nx_window *window, nx_linux_ax_geometry *out);
int nx_linux_window_ax_focus(nx_window *window);
uint64_t nx_linux_window_ax_revision(nx_window *window);
int nx_linux_window_ax_current(nx_window *window,uint64_t revision);
#ifdef NEXAUI_X11_TEST_HOOKS
nx_result nx_linux_ax_test_disconnect(nx_linux_ax *bridge);
nx_result nx_window_linux_ax_test_disconnect(nx_window *window);
#endif
#endif
