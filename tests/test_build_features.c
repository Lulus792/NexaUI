#include <nexaui/nexaui.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    uint64_t expected = 0, actual = UINT64_MAX;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "win32")) expected |= NX_BUILD_WINDOW_WIN32;
        else if (!strcmp(argv[i], "appkit")) expected |= NX_BUILD_WINDOW_APPKIT;
        else if (!strcmp(argv[i], "wayland")) expected |= NX_BUILD_WINDOW_WAYLAND;
        else if (!strcmp(argv[i], "x11")) expected |= NX_BUILD_WINDOW_X11;
        else if (!strcmp(argv[i], "atspi")) expected |= NX_BUILD_LINUX_ATSPI_TRANSPORT;
        else if (!strcmp(argv[i], "fractional")) expected |= NX_BUILD_WAYLAND_FRACTIONAL_SCALE;
        else if (strcmp(argv[i], "none")) return 2;
    }
    if (nx_get_build_features(NULL) != NX_ERROR_ARGUMENT ||
        nx_get_build_features(&actual) != NX_OK || actual != expected) {
        fprintf(stderr, "build features: expected %llu, got %llu\n",
                (unsigned long long)expected, (unsigned long long)actual);
        return 1;
    }
    return 0;
}
