#include "repeat.h"

#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

int main(void) {
    nx_wayland_repeat repeat = {0};
    nx_wayland_repeat_info(&repeat, 25, 400, 0);
    nx_wayland_repeat_press(&repeat, 30, 1, 1000000000ull);
    CHECK(repeat.active && repeat.key == 30);
    CHECK(nx_wayland_repeat_timeout(&repeat, 1000000000ull, 1000) == 400);
    CHECK(nx_wayland_repeat_timeout(&repeat, 1399999999ull, 1000) == 1);
    CHECK(nx_wayland_repeat_take_due(&repeat, 1399999999ull, 8) == 0);
    CHECK(nx_wayland_repeat_take_due(&repeat, 1400000000ull, 8) == 1);
    CHECK(nx_wayland_repeat_timeout(&repeat, 1400000000ull, 1000) == 40);
    CHECK(nx_wayland_repeat_take_due(&repeat, 1600000000ull, 4) == 4);
    CHECK(nx_wayland_repeat_timeout(&repeat, 1600000000ull, 1000) == 40);
    nx_wayland_repeat_press(&repeat, 42, 0, 1600000000ull);
    CHECK(repeat.active && repeat.key == 30);
    nx_wayland_repeat_release(&repeat, 42);
    CHECK(repeat.active);
    nx_wayland_repeat_release(&repeat, 30);
    CHECK(!repeat.active);
    nx_wayland_repeat_press(&repeat, 31, 1, 1700000000ull);
    nx_wayland_repeat_info(&repeat, 0, 400, 1700000000ull);
    CHECK(!repeat.active && nx_wayland_repeat_timeout(&repeat, 0, 250) == 250);
    nx_wayland_repeat_info(&repeat, 10000, 0, 0);
    nx_wayland_repeat_press(&repeat, 30, 1, 0);
    CHECK(nx_wayland_repeat_take_due(&repeat, 0, 8) == 1);
    CHECK(nx_wayland_repeat_timeout(&repeat, 0, 1000) == 1);
    nx_wayland_repeat_cancel(&repeat);
    CHECK(!repeat.active);
    puts("Wayland repeat timing and cancellation verified");
    return 0;
}
