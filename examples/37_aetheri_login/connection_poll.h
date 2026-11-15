#ifndef AETHERI_LOGIN_CONNECTION_POLL_H
#define AETHERI_LOGIN_CONNECTION_POLL_H
#include "view.h"
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
typedef struct aetheri_login_poll aetheri_login_poll;
typedef struct aetheri_login_poll_outcome {
    int changed;
    int connection_error; /* Original negative errno; zero before a fence. */
    uint32_t application_events; /* NX_WINDOW_FD_* for the optional wake fd. */
} aetheri_login_poll_outcome;

/* Linux reference host glue using public NexaUI polling and the original
   connection watch/pump. Borrow connection and optional readable wake fd (-1
   means none); both outlive this object. Close releases only its epoll fd.
   Use one active poller per connection on its window/connection owner thread.
   Drive transport through this poller while active. Admission stays with the original
   connection and its host. This glue creates no thread or new connection. */
nx_result aetheri_login_poll_open(struct ae_login_connection *connection,
    int wake_fd, aetheri_login_poll **out);
void aetheri_login_poll_close(aetheri_login_poll **poller);
nx_result aetheri_login_poll_time(uint64_t *nanoseconds);
/* deadline is absolute CLOCK_MONOTONIC nanoseconds, or zero. The host selects
   it for a request and must not extend it after partial progress. Expiry calls
   the original interrupt, preserving uncertainty rather than cancelling work.
   Step makes at most one original pump call. Original failures retire service
   watchers and leave native UI polling usable; caller drains its wake fd. */
nx_result aetheri_login_poll_step(aetheri_login_poll *poller, nx_window *window,
    uint32_t timeout_ms, uint64_t deadline, aetheri_login_poll_outcome *out);
#endif
#endif
