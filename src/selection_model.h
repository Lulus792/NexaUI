#ifndef NEXAUI_PRIVATE_SELECTION_MODEL_H
#define NEXAUI_PRIVATE_SELECTION_MODEL_H
#include "nexaui/nexaui.h"

/* Scalar-only owner binding for future context/native snapshots. No caller may
   infer model lifetime from an address or retain an editor pointer in COM. */
typedef struct nx_selection_target {
    uint64_t identity, revision;
} nx_selection_target;
nx_selection_target nx_selection_model_target(const nx_selection_model *model);
uint64_t nx_selection_model_membership(const nx_selection_model *model, uint64_t id);
/* Frame-finalization staging: no allocation/call-outs between prepare/commit.
   Each model is bound once. Caller validates all groups before any commit. */
nx_result nx_selection_model_prepare(nx_selection_model *model,
    const nx_selection_change *changes, size_t count, int required,
    int *changed, size_t *selected_count);
void nx_selection_model_commit(nx_selection_model *model, int changed);
nx_result nx_selection_model_item(const nx_selection_model *model, uint64_t id,
    int staged, int *selected, int *disabled);

typedef struct nx_selection_guard {
    /* retain/release/current only touch native synchronized scalars. They must
       not allocate, invoke application code, or mutate a context/model. */
    void *user;
    uint64_t token;
    void (*retain)(void *user);
    void (*release)(void *user);
    int (*current)(void *user, uint64_t token, uint64_t context_identity,
        uint64_t id, uint64_t owner_id, nx_selection_target target);
} nx_selection_guard;
nx_selection_target nx_context_selection_target(const nx_context *context, uint64_t id);
uint64_t nx_context_selection_membership(const nx_context *context, uint64_t id);
nx_result nx_semantics_request_selection_guarded(nx_context *context, uint64_t id,
    uint32_t action, const nx_selection_guard *guard);
#endif
