#ifndef NEXAUI_PRIVATE_NUMERIC_VALUE_H
#define NEXAUI_PRIVATE_NUMERIC_VALUE_H
#include "nexaui/nexaui.h"

/* Native owner-thread lease, never an application/model pointer. Callbacks may
   only inspect/record bridge state; they must not call out or reenter NexaUI.
   An admitted guard is retained until one completion and release, including
   omission, rejection on the next submission, and context destruction. */
typedef struct nx_numeric_value_guard {
    void *user;
    uint64_t token;
    void (*retain)(void *user);
    void (*release)(void *user);
    int (*current)(void *user, uint64_t token, uint64_t context_identity,
                   uint64_t id, uint64_t source_frame);
} nx_numeric_value_guard;

nx_result nx_semantics_request_numeric_value_guarded(nx_context *context,
    uint64_t id, const nx_numeric_value_snapshot *snapshot, double value,
    const nx_numeric_value_guard *guard, void (*complete)(void *user, int applied));
#endif
