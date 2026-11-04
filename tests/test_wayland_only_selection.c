#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(e) do { if (!(e)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #e); return 1; } } while (0)

int main(void) {
    nx_window_desc desc = {0};
    desc.title_utf8 = "Wayland-only backend selection";
    desc.width = 120; desc.height = 80; desc.flags = NX_WINDOW_HIDDEN;
    nx_window *window = (nx_window *)1;
    CHECK(setenv("NEXAUI_PLATFORM", "x11", 1) == 0);
    CHECK(nx_window_create(&desc, &window) == NX_ERROR_UNSUPPORTED && !window);
    CHECK(setenv("NEXAUI_PLATFORM", "invalid", 1) == 0);
    CHECK(nx_window_create(&desc, &window) == NX_ERROR_ARGUMENT && !window);
    CHECK(unsetenv("NEXAUI_PLATFORM") == 0);
    CHECK(setenv("WAYLAND_DISPLAY", "nexaui-no-such-display", 1) == 0);
    CHECK(unsetenv("WAYLAND_SOCKET") == 0);
    CHECK(nx_window_create(&desc, &window) == NX_ERROR_PLATFORM && !window);
    return 0;
}
