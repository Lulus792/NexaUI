#define _GNU_SOURCE
#include "connection_poll.h"
#include "login/connection.h"
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

struct aetheri_login_poll {
    struct ae_login_connection *connection;
    int descriptor, wake, watched[2], error;
    uint32_t interests[2];
};

nx_result aetheri_login_poll_time(uint64_t *out) {
    if (!out) return NX_ERROR_ARGUMENT;
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value)) return NX_ERROR_PLATFORM;
    if (value.tv_sec < 0 || (uint64_t)value.tv_sec > (UINT64_MAX - 999999999u) / 1000000000u)
        return NX_ERROR_LIMIT;
    *out = (uint64_t)value.tv_sec * 1000000000u + (uint64_t)value.tv_nsec;
    return NX_OK;
}

nx_result aetheri_login_poll_open(struct ae_login_connection *connection,
    int wake_fd, aetheri_login_poll **out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out = NULL;
    if (!connection || wake_fd < -1) return NX_ERROR_ARGUMENT;
    struct ae_login_status status;
    if (ae_login_connection_status(connection, &status)) return NX_ERROR_STATE;
    aetheri_login_poll *poller = calloc(1, sizeof(*poller));
    if (!poller) return NX_ERROR_MEMORY;
    poller->connection = connection;
    poller->wake = wake_fd;
    poller->watched[0] = poller->watched[1] = -1;
    poller->descriptor = epoll_create1(EPOLL_CLOEXEC);
    if (poller->descriptor < 0) { free(poller); return NX_ERROR_PLATFORM; }
    if (wake_fd >= 0) {
        struct epoll_event event = {.events = EPOLLIN, .data.u32 = 3};
        if (epoll_ctl(poller->descriptor, EPOLL_CTL_ADD, wake_fd, &event)) {
            int error = errno;
            aetheri_login_poll_close(&poller);
            return error == EPERM ? NX_ERROR_UNSUPPORTED : NX_ERROR_ARGUMENT;
        }
    }
    *out = poller;
    return NX_OK;
}

void aetheri_login_poll_close(aetheri_login_poll **out) {
    if (!out || !*out) return;
    aetheri_login_poll *poller = *out;
    close(poller->descriptor);
    free(poller);
    *out = NULL;
}

static void fence(aetheri_login_poll *poller, int error, aetheri_login_poll_outcome *out) {
    poller->error = error;
    for (unsigned i = 0; i < 2; ++i) {
        if (poller->watched[i] >= 0)
            (void)epoll_ctl(poller->descriptor, EPOLL_CTL_DEL, poller->watched[i], NULL);
        poller->watched[i] = -1;
        poller->interests[i] = 0;
    }
    out->connection_error = error;
    out->changed = 1;
}

static nx_result track(aetheri_login_poll *poller, unsigned index, int fd, uint32_t interests) {
    if (fd == poller->wake || fd == poller->descriptor) return NX_ERROR_ARGUMENT;
    if (poller->watched[index] == fd && poller->interests[index] == interests) return NX_OK;
    int operation = EPOLL_CTL_MOD;
    if (poller->watched[index] != fd) {
        if (poller->watched[index] >= 0)
            (void)epoll_ctl(poller->descriptor, EPOLL_CTL_DEL, poller->watched[index], NULL);
        poller->watched[index] = -1;
        operation = EPOLL_CTL_ADD;
    }
    struct epoll_event event = {.events = interests, .data.u32 = index + 1u};
    if (epoll_ctl(poller->descriptor, operation, fd, &event)) return NX_ERROR_PLATFORM;
    poller->watched[index] = fd;
    poller->interests[index] = interests;
    return NX_OK;
}

nx_result aetheri_login_poll_step(aetheri_login_poll *poller, nx_window *window,
    uint32_t timeout_ms, uint64_t deadline, aetheri_login_poll_outcome *out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out = (aetheri_login_poll_outcome){0};
    if (!poller || !window) return NX_ERROR_ARGUMENT;
    struct ae_login_status before;
    if (ae_login_connection_status(poller->connection, &before)) return NX_ERROR_STATE;
    out->connection_error = poller->error;
    if (!poller->error) {
        int socket_fd = -1, peer_fd = -1;
        short interests = 0;
        int result = ae_login_connection_watch(poller->connection, &socket_fd, &interests, &peer_fd);
        if (result) { fence(poller, result, out); return NX_OK; }
        uint32_t flags = EPOLLIN | EPOLLRDHUP;
        if (interests & POLLOUT) flags |= EPOLLOUT;
        nx_result tracked = track(poller, 0, socket_fd, flags);
        if (tracked == NX_OK) tracked = track(poller, 1, peer_fd, EPOLLIN);
        if (tracked != NX_OK) return tracked;
        if (deadline) {
            uint64_t now = 0;
            nx_result clock = aetheri_login_poll_time(&now);
            if (clock != NX_OK) return clock;
            if (now >= deadline) {
                fence(poller, ae_login_connection_interrupt(poller->connection), out);
                return NX_OK;
            }
            uint64_t remaining = deadline - now;
            uint64_t milliseconds = remaining / 1000000u + (remaining % 1000000u != 0);
            if (milliseconds < timeout_ms) timeout_ms = (uint32_t)milliseconds;
        }
    }
    uint32_t ready = 0;
    nx_result native = nx_window_poll_with_fd_events(window, timeout_ms,
        poller->descriptor, NX_WINDOW_FD_READABLE, &ready);
    if (native != NX_OK) return native;
    if (!poller->error && deadline) {
        uint64_t now = 0;
        nx_result clock = aetheri_login_poll_time(&now);
        if (clock != NX_OK) return clock;
        if (now >= deadline) {
            fence(poller, ae_login_connection_interrupt(poller->connection), out);
            return NX_OK;
        }
    }
    if (!ready) return NX_OK;
    struct epoll_event events[3];
    int count = epoll_wait(poller->descriptor, events, 3, 0);
    if (count < 0) return errno == EINTR ? NX_OK : NX_ERROR_PLATFORM;
    int service_ready = 0;
    for (int i = 0; i < count; ++i) {
        if (events[i].data.u32 <= 2) service_ready = 1;
        else {
            if (events[i].events & EPOLLIN) out->application_events |= NX_WINDOW_FD_READABLE;
            if (events[i].events & EPOLLHUP) out->application_events |= NX_WINDOW_FD_HANGUP;
            if (events[i].events & EPOLLERR) out->application_events |= NX_WINDOW_FD_ERROR;
        }
    }
    if (service_ready && !poller->error) {
        int result = ae_login_connection_pump(poller->connection);
        if (result < 0) fence(poller, result, out);
        else {
            struct ae_login_status after;
            if (ae_login_connection_status(poller->connection, &after)) return NX_ERROR_STATE;
            out->changed = result == 1 || before.phase != after.phase ||
                before.prompt != after.prompt || before.operation != after.operation;
        }
    }
    return NX_OK;
}
