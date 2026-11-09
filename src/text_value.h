#ifndef NEXAUI_PRIVATE_TEXT_VALUE_H
#define NEXAUI_PRIVATE_TEXT_VALUE_H
#include "nexaui/nexaui.h"

#if defined(_WIN32)
/* Internal editor identity tracking; does not expose native surrounding text. */
void nx_windows_window_set_ime_target(nx_window *window, uint64_t target, uint64_t revision);
#endif

#if defined(__linux__)
/* Private single-transfer clipboard read. The caller frees *out_bytes. */
nx_result nx_linux_window_clipboard_fetch_text(nx_window *window, char **out_bytes,
                                                size_t *out_length);
nx_result nx_linux_window_clipboard_async_start(nx_window *window, size_t limit,
    uint32_t timeout_ms, int sensitive, uint64_t *token);
nx_result nx_linux_window_clipboard_async_take(nx_window *window, uint64_t token,
    char **bytes, size_t *length, int *ready);
nx_result nx_linux_window_clipboard_async_cancel(nx_window *window, uint64_t token);
int nx_linux_window_clipboard_async_current(nx_window *window, uint64_t token);
nx_result nx_linux_window_clipboard_async_cancel_active(nx_window *window);
void nx_linux_clipboard_cancel_token(uint64_t token);
int nx_linux_window_clipboard_async_supported(nx_window *window);
#endif

/* Owner-thread model identities, never model addresses. Native snapshots may
   copy these scalars, but may not access the editor. A revision invalidates an
   admitted request when the application replaces text or changes edit policy. */
typedef struct nx_text_value_target {
    uint64_t identity, revision;
    int multiline;
    uint32_t document_operations; /* Zero for built-ins; domain + app policy. */
} nx_text_value_target;
#define NX_TEXT_DOCUMENT_BOUND (UINT32_C(1) << 31)

/* Saturating counter for changes to text or selection. */
uint64_t nx_text_buffer_change_serial(const nx_text_buffer *buffer);

/* Owner-thread, completed-frame snapshot. Offsets are UTF-8 bytes in the
   semantic value for that same frame, not native character offsets. Real
   non-protected text fields or explicit application-owned read-only snapshots;
   never contains a field/model pointer. */
typedef struct nx_text_selection_snapshot {
    size_t anchor, cursor;
    int present;
} nx_text_selection_snapshot;
nx_text_selection_snapshot nx_context_text_selection_snapshot(const nx_context *context,
                                                               uint64_t id);
int nx_context_text_snapshot_only(const nx_context *context, uint64_t id);
/* Node-local disability without overlay-derived suppression; native bridges
   use this to inherit real disabled state across an active nested popup. */
int nx_context_semantic_intrinsically_disabled(const nx_context *context,uint64_t id);

/* Optional native lifetime lease. retain/release cannot call out; current runs
   only when the owner submits the actual widget, outside native snapshot locks.
   A detached/rebound/disabled window can revoke already forwarded requests
   without retaining or dereferencing a context pointer. */
typedef struct nx_text_value_guard {
    void *user;
    uint64_t token;
    void (*retain)(void *user);
    void (*release)(void *user);
    int (*current)(void *user, uint64_t token, uint64_t context_identity, uint64_t id,
                   nx_text_value_target target);
#if defined(__linux__)
    /* Optional owner-thread lease transition for a pending Wayland paste.
       Must only update native request state and must not call application code. */
    int (*defer_paste)(void *user, uint32_t *remaining_ms);
#endif
} nx_text_value_guard;

nx_text_value_target nx_context_text_value_target(const nx_context *context,
                                                  uint64_t id);
nx_result nx_semantics_request_text_value_guarded(nx_context *context,
    uint64_t id, nx_utf8_span value, const nx_text_value_guard *guard);

/* Internal completion for native asynchronous bridges. The guard owns user
   until completion has returned. complete must only record the result: it
   must not call application code, native APIs, or reenter NexaUI. It is called
   exactly once for an admitted request (including cancellation), never for a
   rejected admission. applied means the model accepted the replacement, even
   when the new value equals the old one; it does not imply frame publication. */
nx_result nx_semantics_request_text_value_with_completion(nx_context *context,
    uint64_t id, nx_utf8_span value, const nx_text_value_guard *guard,
    void (*complete)(void *user, int applied));

/* Byte offsets refer to the admitted completed-frame value and must be UTF-8
   scalar boundaries. The core revalidates the target and edits its real
   buffer atomically, retaining its previous selection for undo. */
nx_result nx_semantics_request_text_range_with_completion(nx_context *context,
    uint64_t id, size_t start, size_t end, nx_utf8_span value,
    const nx_text_value_guard *guard, void (*complete)(void *user, int applied));
nx_result nx_semantics_request_text_selection_with_completion(nx_context *context,
    uint64_t id, size_t anchor, size_t cursor,
    const nx_text_value_guard *guard, void (*complete)(void *user, int applied));
/* Copy/cut uses the submitted field's clipboard callback, never a retained
   window or a callback copied into the native snapshot. */
nx_result nx_semantics_request_text_clipboard_with_completion(nx_context *context,
    uint64_t id, size_t start, size_t end, int cut,
    const nx_text_value_guard *guard, void (*complete)(void *user, int applied));
nx_result nx_semantics_request_text_paste_with_completion(nx_context *context,
    uint64_t id, size_t position,
    const nx_text_value_guard *guard, void (*complete)(void *user, int applied));
nx_result nx_text_buffer_replace_range_private(nx_text_buffer *buffer,
    size_t start, size_t end, const char *bytes, size_t length);
/* One complete marked IME transaction, one existing undo record. Invalid or
   rejected input preserves text/selection/history and returns ARGUMENT. Scratch
   is bounded to 1MiB, freed on every path and wiped for sensitive buffers. */
#if !defined(_WIN32) && (defined(__GNUC__) || defined(__clang__))
__attribute__((visibility("hidden")))
#endif
nx_result nx_text_buffer_ime_transaction_private(nx_text_buffer *buffer,
    const nx_event *events, size_t count, int *applied);
#endif
