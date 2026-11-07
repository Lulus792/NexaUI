#ifndef EXAMPLE_AETHERI_PICKER_WORKER_H
#define EXAMPLE_AETHERI_PICKER_WORKER_H
#include <nexaui/nexaui.h>
#include "files/model.h"

/* Consumer adapter, not an SDK API or replacement Files model. All original
   model calls/borrowed views belong exclusively to one worker. No UI, context,
   font, window, authority or arbitrary callbacks enter that worker. */
typedef struct fp_worker fp_worker;
typedef struct fp_key { uint64_t view, request; } fp_key;
typedef enum fp_operation { FP_READ, FP_REFRESH, FP_ENTER, FP_UP, FP_ANCESTOR,
                           FP_FILTER, FP_HIDDEN, FP_PICK } fp_operation;
typedef struct fp_command {
    fp_key key;
    uint64_t expected_state; /* 0 allowed only for a read of current state. */
    fp_operation operation;
    size_t ordinal; /* Index in the complete original snapshot, not display row. */
    unsigned depth;
    int show_hidden;
    size_t query_bytes; /* Preserve original byte validation, including interior NULs. */
    char query[AE_FILES_QUERY + 1];
} fp_command;
typedef struct fp_row { size_t ordinal; struct ae_files_entry entry; } fp_row;
typedef struct fp_reply {
    fp_key key;
    uint64_t state, revision;
    enum ae_files_result result;
    unsigned depth;
    size_t total, count;
    int show_hidden;
    char query[AE_FILES_QUERY + 1];
    char ancestors[AE_FILES_DEPTH + 1][AE_FILES_LABEL + 1];
    /* Owned ordinary-user directory/O_PATH witness from original open_selected.
       No StorageRef, grant, authority admission, installation or content read. */
    int directory, witness;
    char leaf[AE_FILES_NAME + 1];
    fp_row rows[];
} fp_reply;

/* Duplicates root, creates a bounded two-slot S02 channel and owned wake pipe,
   then starts the model owner. No directory scan runs on the caller thread.
   Caller may close its root after success. All methods below use one UI/owner
   thread; the worker is internal. Output is NULL on failure. */
nx_result fp_worker_start(int root, fp_worker **out);
nx_result fp_worker_submit(fp_worker *, const fp_command *, uint64_t *ticket);
/* Borrowed nonblocking FD for nx_window_poll_with_fd/poll. Do not read it
   directly: receive drains notifications BEFORE taking a completion, preventing
   a lost wakeup between an empty-queue observation and the next native poll.
   Drain receive until message.ticket==0; dispose EVERY received message once. */
int fp_worker_wake_fd(const fp_worker *);
nx_result fp_worker_receive(fp_worker *, nx_sim_message *message);
/* Validate native in-process reply bounds; borrowed until message disposal.
   A model error still has a complete retained observation, distinguished by
   reply.result. A failed initial read has revision/state=0, never empty success. */
fp_reply *fp_reply_get(const nx_sim_message *);
/* Transfer an admitted selection exactly once, on the consumer thread. Outputs
   start empty; message disposal otherwise closes unclaimed witnesses. */
nx_result fp_reply_take(fp_reply *, int *directory, int *witness, char leaf[AE_FILES_NAME + 1]);
/* Stop rejects new submissions. Accepted queued work receives cancellation;
   already running original I/O may finish. Call Stop before Join; otherwise
   Join returns NX_ERROR_STATE. Join has NO filesystem timeout.
   GUI/context may retire first: the worker accesses neither. Destroy stops,
   joins and disposes queued replies/FDs before releasing channel/pipe/root. */
nx_result fp_worker_stop(fp_worker *);
nx_result fp_worker_join(fp_worker *);
nx_result fp_worker_destroy(fp_worker **);
#endif
