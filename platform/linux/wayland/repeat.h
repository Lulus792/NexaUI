#ifndef NEXAUI_WAYLAND_REPEAT_H
#define NEXAUI_WAYLAND_REPEAT_H

#include <stdint.h>

typedef struct nx_wayland_repeat {
    uint32_t key;
    int rate, delay_ms, active;
    uint64_t next_ns;
} nx_wayland_repeat;

void nx_wayland_repeat_info(nx_wayland_repeat *repeat, int rate,
                            int delay_ms, uint64_t now_ns);
void nx_wayland_repeat_press(nx_wayland_repeat *repeat, uint32_t key,
                             int repeatable, uint64_t now_ns);
void nx_wayland_repeat_release(nx_wayland_repeat *repeat, uint32_t key);
void nx_wayland_repeat_cancel(nx_wayland_repeat *repeat);
int nx_wayland_repeat_timeout(const nx_wayland_repeat *repeat,
                              uint64_t now_ns, int requested_ms);
unsigned nx_wayland_repeat_take_due(nx_wayland_repeat *repeat,
                                     uint64_t now_ns, unsigned limit);

#endif
