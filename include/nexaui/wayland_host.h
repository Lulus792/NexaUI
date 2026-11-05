#ifndef NEXAUI_WAYLAND_HOST_H
#define NEXAUI_WAYLAND_HOST_H

#include "nexaui/nexaui.h"

#ifdef __cplusplus
extern "C" {
#endif

struct wl_shm;
struct wl_surface;
struct wl_event_queue;
struct wl_display;
typedef struct nx_wayland_shm_host nx_wayland_shm_host;
typedef enum nx_wayland_shm_outcome {
    NX_WAYLAND_SHM_READY = 1,
    NX_WAYLAND_SHM_SUSPENDED = 2,
    NX_WAYLAND_SHM_BUSY = 3
} nx_wayland_shm_outcome;

/* Linux Wayland host seam in NexaUI::NexaUI. All calls and queue dispatch are
   on the creating thread. display, shm, surface and optional queue are borrowed,
   must share one live host-owned connection and must outlive this object.
   NULL queue means the connection's default queue. The host owns the role,
   configure/ack, input, viewport, buffer scale, event dispatch and flushing;
   the host must ensure this is the surface's sole buffer presenter.
   This API opens no connection, creates no role and grants no authority.
   *out_host is NULL on failure. */
NX_API nx_result nx_wayland_shm_host_create(struct wl_display *display, struct wl_shm *shm,
    struct wl_surface *surface, struct wl_event_queue *queue,
    nx_wayland_shm_host **out_host);
/* The host must have completed the role's initial configure/ack and prepared
   compatible surface scale/viewport state before this call. Physical pixel
   dimensions and the logical-to-pixel scale describe the same frame. The call
   copies the borrowed commands/bitmap bytes, attaches and commits the buffer,
   including any pending host surface state, but does not flush or dispatch.
   READY means queued for submission, not received or displayed. Zero-by-zero
   dimensions suspend; a single zero is invalid. BUSY means three buffers await
   release: dispatch the chosen queue, then retry. Each buffer is <=128 MiB.
   Validation/allocation errors enqueue no attach or commit. A known display
   failure returns PLATFORM; failure noticed after commit has unknown outcome. */
NX_API nx_result nx_wayland_shm_host_present(nx_wayland_shm_host *host,
    nx_pixel_size pixels, double scale, const nx_command *commands, size_t count,
    nx_color clear, nx_wayland_shm_outcome *out_outcome);
/* Dispatch this host's selected Wayland queue and wait for its display or one
   borrowed application-owned readable fd. Pass -1 for no application fd.
   The fd must remain open throughout the call and differ from the display fd;
   NX_WINDOW_FD_* flags report readable, hangup and error without consuming it.
   Timeout is milliseconds; values above INT_MAX clamp to INT_MAX. Returns
   promptly after dispatching already queued events. The host must arrange a
   single reader/dispatcher for the connection during this call. Other queues
   are not dispatched. Callbacks may update host state, but cannot reenter this
   object's present, poll or destroy operations. *out_events is cleared on error. */
NX_API nx_result nx_wayland_shm_host_poll(nx_wayland_shm_host *host,
    uint32_t timeout_ms, int app_fd, uint32_t *out_events);
/* Same owner thread. NULL object is a no-op. Releases owned proxies/mappings,
   including outstanding buffers, and clears *host; it does not touch the
   borrowed surface, queue, shm or connection. Host must stop using aliases. */
NX_API nx_result nx_wayland_shm_host_destroy(nx_wayland_shm_host **host);

#ifdef __cplusplus
}
#endif
#endif
