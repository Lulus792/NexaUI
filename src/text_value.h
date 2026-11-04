#ifndef NEXAUI_PRIVATE_TEXT_VALUE_H
#define NEXAUI_PRIVATE_TEXT_VALUE_H
#include "nexaui/nexaui.h"

/* Owner-thread model identities, never model addresses. Native snapshots may
   copy these scalars, but may not access the editor. A revision invalidates an
   admitted request when the application replaces text or changes edit policy. */
typedef struct nx_text_value_target {
    uint64_t identity, revision;
    int multiline;
} nx_text_value_target;

/* Owner-thread, completed-frame snapshot. Offsets are UTF-8 bytes in the
   semantic value for that same frame, not native character offsets. Real
   non-protected text fields only; never contains a field/model pointer. */
typedef struct nx_text_selection_snapshot {
    size_t anchor, cursor;
    int present;
} nx_text_selection_snapshot;
nx_text_selection_snapshot nx_context_text_selection_snapshot(const nx_context *context,
                                                               uint64_t id);

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
nx_result nx_text_buffer_replace_range_private(nx_text_buffer *buffer,
    size_t start, size_t end, const char *bytes, size_t length);
#endif
