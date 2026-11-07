#ifndef EXAMPLE_AETHERI_SOFTWARE_MODEL_H
#define EXAMPLE_AETHERI_SOFTWARE_MODEL_H
#include <nexaui/nexaui.h>
#include "catalogue/projection.h"

enum { SW_MAX_TARGETS=100000 };
typedef struct sw_id { uint8_t bytes[16]; } sw_id;
typedef struct sw_scope {
    struct ae_catalogue_identity identity;
    struct ae_catalogue_info context;
    uint8_t account[16];
    uint32_t os_uid;
} sw_scope;
typedef struct sw_row {
    sw_id install;
    /* Zero: exact original observation. -ENOENT: no eligible active app was
       observed for this recipient; never infer removed/uninstalled/denied. */
    int error;
    struct ae_install_snapshot value; /* all zero when error is nonzero */
} sw_row;
typedef struct sw_result { sw_scope scope; size_t count; sw_row *rows; } sw_result;

/* Optional trusted owner-side producer, never a client path/SQL interface.
   The caller retains and serializes the original Root handle on its worker.
   Input is an explicit selected target set, NOT catalogue enumeration. Empty
   success means no selected targets, never no installed software. No method
   here starts a runtime, writes/caches metadata, admits or executes an action.
   Per-target -ENOENT stays a row; any other error rejects the whole batch.
   Before/after original contexts and every successful row must agree; this
   guard does not create a transaction or authority across separate queries.
   *out must be NULL; errors preserve it. Original negative errno is returned. */
int sw_read(struct ae_catalogue *,const sw_scope *,const sw_id *,size_t,sw_result **out);
void sw_result_free(sw_result **);

/* Owner-thread presentation state. Source/recipient/target changes require a
   new owner; no catalogue, SQL, launch/permission capability enters this model.
   Targets are copied and canonically sorted by their complete opaque IDs. */
typedef struct sw_owner sw_owner;
typedef struct sw_snapshot sw_snapshot;
typedef enum sw_state { SW_UNOBSERVED,SW_READING,SW_OBSERVED,SW_UNAVAILABLE } sw_state;
nx_result sw_owner_create(const sw_scope *,const sw_id *,size_t,sw_owner **out);
void sw_owner_destroy(sw_owner **);
nx_result sw_owner_expect(sw_owner *,uint64_t ticket);
/* Only current ticket; success moves the entire validated result once. All
   rejected inputs/OOM preserve caller ownership, pending ticket and old view.
   A negative original error completes as unavailable with the old view retained.
   Equal catalogue revisions cannot silently change the observed rows. */
nx_result sw_owner_complete(sw_owner *,uint64_t ticket,int error,sw_result **);
sw_state sw_owner_state(const sw_owner *);
int sw_owner_error(const sw_owner *);
uint64_t sw_owner_epoch(const sw_owner *);
uint64_t sw_owner_pending(const sw_owner *);
nx_result sw_owner_select(sw_owner *,sw_id);
int sw_owner_selection(const sw_owner *,sw_id *out);
nx_result sw_owner_snapshot(sw_owner *,sw_snapshot **out);
const sw_result *sw_snapshot_data(const sw_snapshot *);
size_t sw_snapshot_find(const sw_snapshot *,sw_id); /* SIZE_MAX when absent */
void sw_snapshot_release(sw_snapshot **);
#endif
