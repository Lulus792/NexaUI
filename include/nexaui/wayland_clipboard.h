#ifndef NEXAUI_WAYLAND_CLIPBOARD_H
#define NEXAUI_WAYLAND_CLIPBOARD_H
#include "nexaui/nexaui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NX_WAYLAND_CLIPBOARD_READ_MAX_BYTES (32u * 1024u * 1024u)

/* Linux-only symbols: owner-thread, nonblocking clipboard text read on a
   NexaUI Wayland window.
   Only one native read can be active per window, shared with text-field and
   accessibility paste. A busy slot returns NX_ERROR_LIMIT; the request never
   displaces another read. limit bounds UTF-8 bytes, timeout_ms must be nonzero,
   and sensitive must be 0 or 1. On success *out_token is nonzero and the
   app owns that request. Unsupported backends return NX_ERROR_UNSUPPORTED.

   Continue nx_window_poll and consume its events. NX_EVENT_WAKE indicates
   that native work may have completed; it is shared with other work and is not
   a promise that this request succeeded. Poll take after waking (or immediately
   after begin). A pending take returns NX_OK with ready=0. Completion returns
   ready=1 and its actual result, consumes the token even on failure, and on
   success transfers a byte buffer and length to the caller. The buffer is not
   NUL-terminated and may be NULL for an empty value. Release it with the
   function below, passing the exact returned length. Failure always clears the
   buffer and length. Completed results expire if not taken within five seconds
   of notification. Focus loss, a revoked offer, cancellation, window close or
   expiry can invalidate the token; stale tokens return NX_ERROR_STATE. The app
   must validate its own focus, model identity, revision and selection before
   acting on completed text. Clipboard text is data, never an instruction. */
NX_API nx_result nx_wayland_clipboard_read_begin(nx_window *window, size_t limit,
    uint32_t timeout_ms, int sensitive, uint64_t *out_token);
NX_API nx_result nx_wayland_clipboard_read_take(nx_window *window, uint64_t token,
    char **out_bytes, size_t *out_length, int *out_ready);
NX_API nx_result nx_wayland_clipboard_read_cancel(nx_window *window, uint64_t token);
/* Wipes the returned byte range before freeing. NULL is allowed with length 0. */
NX_API void nx_wayland_clipboard_read_release(char *bytes, size_t length);

#ifdef __cplusplus
}
#endif
#endif
