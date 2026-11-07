#ifndef EXAMPLE_SOFTWARE_WORKER_H
#define EXAMPLE_SOFTWARE_WORKER_H
#include "software_components.h"
/* Application-private trusted Root adapter following example33's owner-worker
   pattern. Source configuration and jobs are copied; no GUI object or callback
   enters the worker. Thread separation is not an authority/IPC boundary. */
typedef struct sww_worker sww_worker;
typedef struct sww_key { uint64_t view,request; } sww_key;
typedef struct sww_reply { sww_key key; int original_error; sw_result *result; nx_result components_status;sw_components *components; } sww_reply;
/* Copy the explicit protected source path/Root identity. Only the worker opens,
   queries and closes the original catalogue. Wait only for open/get-info here;
   no selected-app scan runs during this handshake. Context changes on success. */
nx_result sww_start(const char *,const struct ae_catalogue_identity *,sww_worker **,
                    struct ae_catalogue_info *,int *original_error);
/* Optional immutable manifest-source configuration, copied before thread start.
   No manifest file is opened until an eligible selected original row is read.
   Source changes require another worker. Component failure never fabricates an
   empty inventory or invalidates the separately valid app observation. */
nx_result sww_start_with_manifests(const char *,const struct ae_catalogue_identity *,
    const sw_manifest_source *,size_t,sww_worker **,struct ae_catalogue_info *,int *original_error);
/* Owner-thread calls. Two S02 slots reserve completions until receive takes
   them. Copy the complete scope and selected IDs before waking the worker.
   Scope/target validation errors complete as original producer errors; bound,
   pointer and key errors are rejected before channel admission. */
nx_result sww_submit(sww_worker *,sww_key,const sw_scope *,const sw_id *,size_t,uint64_t *ticket);
int sww_wake_fd(const sww_worker *);
/* Drain pipe before completions; repeat until ticket=0, then poll. Every taken
   message needs disposal, including rejected/retired results. Correlate both
   key and ticket before moving a response into an owner. */
nx_result sww_receive(sww_worker *,nx_sim_message *);
sww_reply *sww_reply_get(const nx_sim_message *);
nx_result sww_reply_take(sww_reply *,sw_result **);
/* Stop rejects new work and completes queued jobs as cancelled. A dequeued
   original batch can finish without GUI access. Retire GUI first; join has no
   filesystem deadline. Destroy drains owned replies before channel/pipe free. */
nx_result sww_stop(sww_worker *);
nx_result sww_join(sww_worker *);
nx_result sww_destroy(sww_worker **);
#endif
