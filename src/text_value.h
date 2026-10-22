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
#endif
