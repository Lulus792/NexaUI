#ifndef EXAMPLE_AETHERI_RECOVERY_VIEW_MODEL_H
#define EXAMPLE_AETHERI_RECOVERY_VIEW_MODEL_H
#include <nexaui/nexaui.h>
#include "catalogue/recovery.h"

/* Optional consumer presentation ownership, not a public SDK or authority API.
   The trusted original owner supplies its checked context and original copied
   read result. No catalogue handle, path, SQL, admission or replay enters here.
   All methods/refcounts are confined to one application owner thread. */
typedef struct rv_owner rv_owner;
typedef struct rv_snapshot rv_snapshot;
typedef struct rv_key { enum ae_recovery_record_kind kind; uint8_t id[16]; } rv_key;
typedef enum rv_observation { RV_UNOBSERVED, RV_READING, RV_OBSERVED, RV_UNAVAILABLE } rv_observation;

/* *out must be NULL; failures preserve it. Context is the original owner's
   observation, not a capability or a claim that its external actors are ready. */
nx_result rv_owner_create(const struct ae_catalogue_identity *,const struct ae_catalogue_info *,rv_owner **out);
void rv_owner_destroy(rv_owner **);
/* Bind an already accepted application/S02 query ticket. Strictly increasing;
   superseding does not cancel old work or release its channel reservation. */
nx_result rv_owner_expect(rv_owner *,uint64_t ticket);
/* Current ticket only. An original negative errno completes as UNAVAILABLE,
   retaining the previous immutable observation/selection. Success moves the
   entire original result once; *result becomes NULL. Every rejection preserves
   caller ownership, current observation and pending ticket. Caller disposes
   rejected/retired results using the original free API. There is no retry here.
   Same source/boot/runtime and nondecreasing revisions are required. Equal
   catalogue revisions must describe identical records. A changed source/context
   needs a new owner, never silent rebinding of this one. */
nx_result rv_owner_complete(rv_owner *,uint64_t ticket,int original_error,struct ae_recovery_inventory **result);
rv_observation rv_owner_state(const rv_owner *);
int rv_owner_error(const rv_owner *);
uint64_t rv_owner_epoch(const rv_owner *);
uint64_t rv_owner_pending(const rv_owner *);
/* Selection is read-only inspection of a full kind+opaque-ID tuple. It conveys
   no operation/holder state or permission. Surviving keys persist across a
   successful refresh; positions never select replacement objects. */
nx_result rv_owner_select(rv_owner *,rv_key);
int rv_owner_selection(const rv_owner *,rv_key *out);
size_t rv_snapshot_find(const rv_snapshot *,rv_key); /* SIZE_MAX when absent. */
nx_result rv_owner_snapshot(rv_owner *,rv_snapshot **out); /* owned reference */
/* Borrowed original data, including nested records; never modify or free any
   field. It remains valid until this owned snapshot reference is released. */
const struct ae_recovery_inventory *rv_snapshot_data(const rv_snapshot *);
void rv_snapshot_release(rv_snapshot **);
#endif
