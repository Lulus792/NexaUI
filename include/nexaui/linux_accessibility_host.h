#ifndef NEXAUI_LINUX_ACCESSIBILITY_HOST_H
#define NEXAUI_LINUX_ACCESSIBILITY_HOST_H
#include "nexaui/nexaui.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_linux_accessibility_host nx_linux_accessibility_host;
typedef struct nx_linux_accessibility_geometry {
    double x,y,width,height;
    int screen_position,visible,focused;
} nx_linux_accessibility_geometry;
/* Linux only. All calls use the creating thread and must not reenter another
   operation on this object. The host owns its native
   surface and event loop; no Wayland connection, role or window is created. */
NX_API nx_result nx_linux_accessibility_host_create(nx_linux_accessibility_host **out);
/* Copy a completed context snapshot and native geometry. The caller supplies
   truthful surface visibility and native keyboard focus, not logical focus.
   Screen coordinates are unavailable when screen_position is zero.
   UNSUPPORTED means the optional AT-SPI transport is not currently available. */
NX_API nx_result nx_linux_accessibility_host_publish(nx_linux_accessibility_host *host,
    const nx_context *context,const nx_linux_accessibility_geometry *geometry);
/* Pump the bus nonblocking, then expose a borrowed descriptor and a bounded
   wait time. fd may be -1 and can change after any call. writable requests
   POLLOUT as well as POLLIN. Pass the desired wait in milliseconds by pointer;
   the result may be shorter for reconnect or request deadlines. The caller
   must call again when the fd is ready or that timeout expires. */
NX_API nx_result nx_linux_accessibility_host_prepare(nx_linux_accessibility_host *host,
    int *fd,int *writable,uint32_t *timeout_ms);
/* Route admitted actions to the same context after a completed publication,
   before its next frame. focus_requested is a request to the host, not proof
   that native focus changed. The caller must actually observe native focus. */
NX_API nx_result nx_linux_accessibility_host_route(nx_linux_accessibility_host *host,
    nx_context *context,size_t *routed,int *focus_requested);
/* Retire native objects and pending requests; it is safe to publish anew. */
NX_API nx_result nx_linux_accessibility_host_clear(nx_linux_accessibility_host *host);
NX_API nx_result nx_linux_accessibility_host_destroy(nx_linux_accessibility_host **host);
#ifdef __cplusplus
}
#endif
#endif
