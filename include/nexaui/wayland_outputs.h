#ifndef NEXAUI_WAYLAND_OUTPUTS_H
#define NEXAUI_WAYLAND_OUTPUTS_H
#include "nexaui.h"

#if defined(__linux__)
/* One compositor-advertised wl_output, not a persistent device identity.
   Dimensions are current-mode hardware units, refresh is mHz (zero means
   unavailable), scale is an integer wl_output scale, not fractional surface
   scale. name may be empty if absent, invalid or too long. */
typedef struct nx_wayland_output_info {
    uint32_t id;
    int32_t width, height, refresh_millihz, scale;
    int mode_known;
    char name[128];
} nx_wayland_output_info;

/* Owner-thread snapshot of the outputs advertised on this window's connection.
   A first call subscribes to coalesced NX_EVENT_WAKE notifications. Each
   output's fields become visible together on wl_output.done. STATE means an
   announced output is not ready (the caller should poll and retry); PLATFORM
   means the connection failed. X11 returns UNSUPPORTED. On success count and
   revision identify a complete snapshot; records are copied, not borrowed.
   With items=NULL and capacity=0, query the count without copying. With an
   insufficient capacity returns LIMIT, count and revision are set and items
   is untouched. Other errors clear count/revision and leave items untouched.
   IDs are registry names valid only for this window connection, and are not
   device, authorization, persistence or physical-presentation evidence. */
NX_API nx_result nx_window_get_wayland_outputs(nx_window *window,
    nx_wayland_output_info *items, size_t capacity, size_t *count, uint64_t *revision);
#endif
#endif
