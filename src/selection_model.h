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
/* Whole native selection replacement, independent of the 64 action-batch
   limit. Allocates nothing and only stages scratch. A disabled member may be
   preserved but may not be added/removed by a native replacement. */
nx_result nx_selection_model_prepare_set(nx_selection_model *model,
    const uint64_t *ids, size_t count, int required, int *changed,
    size_t *selected_count);
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
/* Private complete-set admission. Borrow ids only until return; the native
   caller owns a stable copy across allocator call-outs and retains its request
   separately. current receives owner_id for both id and owner_id. No model
   pointer crosses admission. Pending payloads share an 8 MiB context budget. */
#define NX_SELECTION_SET_MAX_QUEUED_BYTES (8u * 1024u * 1024u)
nx_result nx_semantics_request_selection_set_guarded(nx_context *context,
    uint64_t owner_id, const uint64_t *ids, size_t count,
    const nx_selection_guard *guard);
#endif
