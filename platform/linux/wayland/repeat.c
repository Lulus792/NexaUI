#include "repeat.h"

#include <limits.h>

static uint64_t nx_wayland_repeat_add(uint64_t lhs, uint64_t rhs) {
    return UINT64_MAX - lhs < rhs ? UINT64_MAX : lhs + rhs;
}

static uint64_t nx_wayland_repeat_period(const nx_wayland_repeat *repeat) {
    int rate = repeat->rate > 1000 ? 1000 : repeat->rate;
    return 1000000000ull / (uint64_t)rate;
}

void nx_wayland_repeat_info(nx_wayland_repeat *repeat, int rate,
                            int delay_ms, uint64_t now_ns) {
    if (!repeat) return;
    repeat->rate = rate > 0 ? rate : 0;
    repeat->delay_ms = delay_ms > 0 ? delay_ms : 0;
    if (repeat->rate == 0) repeat->active = 0;
    else if (repeat->active)
        repeat->next_ns = nx_wayland_repeat_add(now_ns,
            (uint64_t)repeat->delay_ms * 1000000ull);
}

void nx_wayland_repeat_press(nx_wayland_repeat *repeat, uint32_t key,
                             int repeatable, uint64_t now_ns) {
    if (!repeat || !repeatable || repeat->rate <= 0) return;
    repeat->key = key;
    repeat->active = 1;
    repeat->next_ns = nx_wayland_repeat_add(now_ns,
        (uint64_t)repeat->delay_ms * 1000000ull);
}

void nx_wayland_repeat_release(nx_wayland_repeat *repeat, uint32_t key) {
    if (repeat && repeat->active && repeat->key == key) repeat->active = 0;
}

void nx_wayland_repeat_cancel(nx_wayland_repeat *repeat) {
    if (repeat) repeat->active = 0;
}

int nx_wayland_repeat_timeout(const nx_wayland_repeat *repeat,
                              uint64_t now_ns, int requested_ms) {
    if (!repeat || !repeat->active || repeat->rate <= 0)
        return requested_ms;
    if (now_ns >= repeat->next_ns) return 0;
    uint64_t remaining_ns = repeat->next_ns - now_ns;
    uint64_t remaining_ms = remaining_ns / 1000000ull +
                            (remaining_ns % 1000000ull != 0);
    int timeout = remaining_ms > INT_MAX ? INT_MAX : (int)remaining_ms;
    return timeout < requested_ms ? timeout : requested_ms;
}

unsigned nx_wayland_repeat_take_due(nx_wayland_repeat *repeat,
                                     uint64_t now_ns, unsigned limit) {
    if (!repeat || !repeat->active || repeat->rate <= 0 || limit == 0)
        return 0;
    uint64_t period = nx_wayland_repeat_period(repeat);
    unsigned count = 0;
    while (now_ns >= repeat->next_ns && count < limit) {
        ++count;
        repeat->next_ns = nx_wayland_repeat_add(repeat->next_ns, period);
        if (repeat->next_ns == UINT64_MAX) {
            repeat->active = 0;
            break;
        }
    }
    if (count == limit && now_ns >= repeat->next_ns)
        repeat->next_ns = nx_wayland_repeat_add(now_ns, period);
    return count;
}
