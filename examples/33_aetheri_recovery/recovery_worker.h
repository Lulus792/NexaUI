#ifndef EXAMPLE_RECOVERY_WORKER_H
#define EXAMPLE_RECOVERY_WORKER_H
#include "view_model.h"
/* Private in-process trusted-owner adapter, not an authority service or IPC
   boundary. Only the worker thread opens/uses/closes the original catalogue.
   No GUI/window/font/view-model object or application callback enters it. */
typedef struct rw_worker rw_worker;
typedef struct rw_key {uint64_t view,request;} rw_key;
typedef struct rw_reply {
    rw_key key;
    int original_error; /* Original negative errno, zero only with inventory. */
    struct ae_recovery_inventory *inventory; /* Once-owned original allocation. */
} rw_reply;
/* Explicit trusted private-owner configuration, never a path selected by UI.
   Copy path/identity; synchronously wait only for original open/get-info on the
   worker. No inventory scan runs here. *out must be NULL. Context changes only
   on success; original_error is zero for non-backend failures. */
nx_result rw_start(const char *,const struct ae_catalogue_identity *,rw_worker **out,
                   struct ae_catalogue_info *initial_context,int *original_error);
/* Owner-thread calls. A bounded two-slot S02 channel reserves each completion
   until receive takes it. Copied keys bind immutable request identity; caller
   correlates both key and ticket before adopting any observation. */
nx_result rw_submit(rw_worker *,rw_key,uint64_t *ticket);
int rw_wake_fd(const rw_worker *);
/* Drain wake pipe BEFORE taking completions. Call repeatedly until ticket=0,
   then poll; dispose every taken message, including rejected/retired replies. */
nx_result rw_receive(rw_worker *,nx_sim_message *);
rw_reply *rw_reply_get(const nx_sim_message *);
/* Takes an inventory once, leaving ordinary message disposal harmless. Output
   must start NULL. The model's complete API may instead move reply.inventory. */
nx_result rw_reply_take(rw_reply *,struct ae_recovery_inventory **out);
/* Stop rejects new submits. Accepted queued reads complete with ECANCELED;
   an already dequeued original read finishes without GUI access. Join requires
   Stop and has NO filesystem timeout. Retire GUI first if required. Destroy
   joins/disposes all queued replies before releasing channel and pipe. */
nx_result rw_stop(rw_worker *);
nx_result rw_join(rw_worker *);
nx_result rw_destroy(rw_worker **);
#endif
