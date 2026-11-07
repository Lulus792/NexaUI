#ifndef EXAMPLE_SOFTWARE_HISTORY_H
#define EXAMPLE_SOFTWARE_HISTORY_H
#include "software_model.h"
#include "catalogue/execution.h"
#include "catalogue/unit.h"
/* Trusted owner supplies explicit original profiles, never numeric PID lookup
   or catalogue enumeration. This is historical enrollment metadata, not a live
   process/resource/image observation, permission, replay or drain authority. */
enum { SWH_MAX_PROFILES=64 };
typedef struct swh_record {
    sw_row observed; /* Exact selected-app observation anchoring this inspection. */
    struct ae_execution_profile profile; /* Own original generation/runtime. */
    uint8_t digest[32];
    int starter_error,app_error; /* 0 only for an available original record. */
    struct ae_execution_process_snapshot starter,app;
    int unit_error,custody_error; /* Independent historical metadata availability. */
    struct ae_unit_allocation_snapshot unit;
    struct ae_unit_custody_snapshot custody;
} swh_record;
typedef struct swh_batch swh_batch;
/* Worker-only on the serialized original Root handle. Copies profiles, scope
   and selected rows before any original read. Foreign epoch/account/boot or
   duplicate launch/profile inputs reject the whole request before reading.
   Unselected/ineligible targets cause no process/unit/custody queries. Original
   unit/custody getters bind the complete historical profile under the current
   same-epoch Root owner; these records grant no current custody or effect right.
   ENOENT and unsupported
   schemas remain per-role unavailable metadata; unexpected errors/context drift
   reject the entire batch, with *out unchanged. Before/after context equality
   is no cross-query transaction or source authentication. No mutating API,
   process handle, filesystem source, launcher or service call is used.
   *out must be NULL. Original negative errno is returned on failure. */
int swh_read(struct ae_catalogue *,const sw_result *,const struct ae_execution_profile *,size_t,swh_batch **out);
/* UI-thread borrowed range, sorted by full profile digest within installation.
   Check each original role/unit/custody error before its zero-on-error snapshot.
   No selected profile (including a NULL batch) yields count=0, not no processes.
   Current source/recipient/row/revision must match exactly. Historical profile
   runtime/generation/artifact may differ from that current selected app; retain
   them explicitly and never relabel stored PID/state as current/running.
   *out and *count must initially be NULL/0. Batch outlives the borrowed range. */
nx_result swh_get(const swh_batch *,const sw_snapshot *,sw_id,const swh_record **out,size_t *count);
void swh_free(swh_batch **);
#endif
