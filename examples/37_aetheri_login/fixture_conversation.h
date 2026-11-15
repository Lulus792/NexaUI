#ifndef AETHERI_LOGIN_FIXTURE_CONVERSATION_H
#define AETHERI_LOGIN_FIXTURE_CONVERSATION_H

#include "view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
#include "login/connection.h"
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

/* Private synthetic peer only. The self/owned-child pins and test callback
   do not admit Aetheri's service, greeter, policy or a production trusted role. */
typedef struct fixture_conversation {
    struct ae_login *model;
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
    struct ae_login_connection *connection;
    int peer;
    unsigned checks;
    int admission_error;
    pid_t child;
    int child_control, client_watch, child_watch;
#endif
} fixture_conversation;

#ifdef AETHERI_LOGIN_CONNECTION_VIEW
/* The child creates both stream endpoints, so their kernel peer credentials
   name that child. It transfers the client plus a remote keeper to the parent,
   closes its client alias, then confirms exclusive client ownership. The parent
   alone drives synthetic protocol bytes. Keeping the remote alias after child
   exit deliberately prevents EOF and isolates the original pidfd check. */
static void fixture_child(int control, pid_t owner) {
    if (prctl(PR_SET_PDEATHSIG, SIGKILL) || getppid() != owner) _exit(120);
    int pair[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, pair)) _exit(121);
    char tag = 'T';
    struct iovec vector = {.iov_base = &tag, .iov_len = 1};
    union { struct cmsghdr align; unsigned char bytes[CMSG_SPACE(2 * sizeof(int))]; } ancillary;
    memset(&ancillary, 0, sizeof(ancillary));
    struct msghdr message = {.msg_iov = &vector, .msg_iovlen = 1,
        .msg_control = ancillary.bytes, .msg_controllen = sizeof(ancillary.bytes)};
    struct cmsghdr *rights = CMSG_FIRSTHDR(&message);
    rights->cmsg_level = SOL_SOCKET; rights->cmsg_type = SCM_RIGHTS;
    rights->cmsg_len = CMSG_LEN(sizeof(pair));
    memcpy(CMSG_DATA(rights), pair, sizeof(pair));
    if (sendmsg(control, &message, MSG_NOSIGNAL) != 1 || close(pair[0]) ||
        send(control, "R", 1, MSG_NOSIGNAL) != 1) _exit(122);
    ssize_t count;
    do { count = recv(control, &tag, 1, 0); } while (count < 0 && errno == EINTR);
    int result = count == 0 || (count == 1 && tag == 'D') ? 0 : 123;
    close(pair[1]); close(control);
    _exit(result);
}

static int fixture_readable(int fd) {
    struct pollfd ready = {.fd = fd, .events = POLLIN};
    return poll(&ready, 1, 2000) == 1 && (ready.revents & POLLIN);
}

static int fixture_process_pair(fixture_conversation *fixture, int *client) {
    int controls[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, controls)) return -1;
    pid_t owner = getpid();
    fixture->child = fork();
    if (!fixture->child) {
        close(controls[0]);
        fixture_child(controls[1], owner);
    }
    close(controls[1]);
    fixture->child_control = controls[0];
    if (fixture->child < 0 || !fixture_readable(controls[0])) return -1;
    char tag = 0;
    struct iovec vector = {.iov_base = &tag, .iov_len = 1};
    union { struct cmsghdr align; unsigned char bytes[CMSG_SPACE(2 * sizeof(int))]; } ancillary;
    memset(&ancillary, 0, sizeof(ancillary));
    struct msghdr message = {.msg_iov = &vector, .msg_iovlen = 1,
        .msg_control = ancillary.bytes, .msg_controllen = sizeof(ancillary.bytes)};
    ssize_t size = recvmsg(controls[0], &message, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
    struct cmsghdr *rights = CMSG_FIRSTHDR(&message);
    int received[2] = {-1, -1};
    size_t count = 0;
    if (rights && rights->cmsg_level == SOL_SOCKET && rights->cmsg_type == SCM_RIGHTS &&
        rights->cmsg_len >= CMSG_LEN(0) && rights->cmsg_len <= message.msg_controllen) {
        count = (rights->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        if (count > 2) count = 2;
        memcpy(received, CMSG_DATA(rights), count * sizeof(int));
    }
    if (count != 2) {
        for (size_t i = 0; i < count; ++i) close(received[i]);
        return -1;
    }
    *client = received[0]; fixture->peer = received[1];
    if (size != 1 || tag != 'T' || (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) || !fixture_readable(controls[0]) ||
        recv(controls[0], &tag, 1, MSG_DONTWAIT) != 1 || tag != 'R') return -1;
    struct ucred credentials;
    socklen_t length = sizeof(credentials);
    if (getsockopt(*client, SOL_SOCKET, SO_PEERCRED, &credentials, &length) ||
        length != sizeof(credentials) || credentials.pid != fixture->child) return -1;
    return 0;
}

static int fixture_admitted(void *user) {
    fixture_conversation *fixture = user;
    ++fixture->checks;
    return fixture->admission_error;
}

static int fixture_send_reply(fixture_conversation *fixture, const char *json) {
    uint8_t wire[512];
    size_t size = strlen(json);
    if (size > sizeof(wire) - 4u) return 0;
    uint32_t length = (uint32_t)size;
    memcpy(wire, &length, 4);
    memcpy(wire + 4, json, size);
    return send(fixture->peer, wire, size + 4u, MSG_DONTWAIT | MSG_NOSIGNAL) == (ssize_t)(size + 4u);
}

static int fixture_peer_empty(fixture_conversation *fixture) {
    if (fixture->peer < 0) return 1;
    char byte;
    return recv(fixture->peer, &byte, 1, MSG_DONTWAIT) == -1 &&
        (errno == EAGAIN || errno == EWOULDBLOCK);
}

static int fixture_child_witness(fixture_conversation *fixture) {
    if (fixture->child <= 0 || fixture->client_watch < 0 || fixture->child_watch < 0) return 0;
    struct pollfd watched[2] = {{.fd = fixture->client_watch, .events = POLLIN},
                               {.fd = fixture->child_watch, .events = POLLIN}};
    /* The original client descriptor is borrowed for poll only. EOF would
       make it readable; the owned remote keeper deliberately prevents it. */
    if (poll(watched, 2, 0) != 1 || watched[0].revents ||
        !(watched[1].revents & POLLIN) || !fixture_peer_empty(fixture)) return 0;
    int status = 0;
    if (waitpid(fixture->child, &status, WNOHANG) != fixture->child) return 0;
    fixture->child = -1;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
#endif

static int fixture_close(fixture_conversation **out) {
    if (!*out) return 0;
    fixture_conversation *fixture = *out;
    int result = 0;
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
    if (fixture->connection) {
        printf("LOGIN_FIXTURE_CONNECTION checks=%u\n", fixture->checks);
        result = ae_login_connection_close(&fixture->connection);
    }
    if (fixture->peer >= 0 && close(fixture->peer)) result = -1;
    if (fixture->child_control >= 0 && close(fixture->child_control)) result = -1;
    if (fixture->child > 0) {
        int status = 0;
        if (!fixture_readable(fixture->child_watch)) {
            (void)kill(fixture->child, SIGKILL);
            result = -1;
        }
        pid_t ended;
        do { ended = waitpid(fixture->child, &status, 0); } while (ended < 0 && errno == EINTR);
        if (ended != fixture->child || !WIFEXITED(status) || WEXITSTATUS(status)) result = -1;
    }
    if (fixture->child_watch >= 0 && close(fixture->child_watch)) result = -1;
#endif
    ae_login_close(&fixture->model);
    free(fixture);
    *out = NULL;
    return result;
}

static int fixture_open(fixture_conversation **out) {
    fixture_conversation *fixture = calloc(1, sizeof(*fixture));
    if (!fixture) return -1;
    *out = fixture;
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
    fixture->peer = fixture->child_control = fixture->client_watch = fixture->child_watch = -1;
    fixture->child = -1;
    if (getenv("NEXAUI_LOGIN_TEST_CONNECTION")) {
        int sockets[2] = {-1, -1};
        const char *loss = getenv("NEXAUI_LOGIN_TEST_CONNECTION_LOSS");
        int process = loss && !strcmp(loss, "peer-exit");
        if (process) {
            if (fixture_process_pair(fixture, &sockets[0])) {
                if (sockets[0] >= 0) close(sockets[0]);
                return -1;
            }
        } else if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, sockets)) return -1;
        long descriptor = syscall(SYS_pidfd_open, process ? fixture->child : getpid(), 0);
        if (descriptor < 0 || descriptor > INT_MAX) {
            close(sockets[0]); if (sockets[1] >= 0) close(sockets[1]);
            return -1;
        }
        int pin = (int)descriptor;
        struct ae_login_connection_source source = {.peer_pidfd = pin,
            .uid = getuid(), .gid = getgid(), .check = fixture_admitted, .userdata = fixture};
        int result = ae_login_connection_create(&fixture->connection, &sockets[0], &source);
        if (process) fixture->child_watch = pin;
        else close(pin);
        if (result) { close(sockets[0]); if (sockets[1] >= 0) close(sockets[1]); return result; }
        if (process) {
            short events = 0;
            int peer_pin = -1;
            if (ae_login_connection_watch(fixture->connection, &fixture->client_watch, &events, &peer_pin)) return -1;
            puts("LOGIN_FIXTURE_PROCESS separate_peer=1 retained_remote_alias=1");
        } else fixture->peer = sockets[1];
        return 0;
    }
#endif
    return ae_login_create(&fixture->model);
}

static int fixture_begin(fixture_conversation *fixture, const char *name, size_t length) {
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
    if (fixture->connection) return ae_login_connection_begin(fixture->connection, name, length);
#endif
    return ae_login_begin(fixture->model, name, length);
}

static int fixture_status(fixture_conversation *fixture, struct ae_login_status *status) {
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
    if (fixture->connection) return ae_login_connection_status(fixture->connection, status);
#endif
    return ae_login_status(fixture->model, status);
}

static nx_result fixture_view(fixture_conversation *fixture, const nx_font_collection *fonts,
                              nx_window *window, aetheri_login_view **out) {
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
    if (fixture->connection) return aetheri_login_view_create_connection(fixture->connection, fonts, window, out);
#endif
    return aetheri_login_view_create(fixture->model, fonts, window, out);
}

static int consume_request(fixture_conversation *fixture, const char *expected) {
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
    if (fixture->connection) {
        uint8_t wire[AE_LOGIN_WIRE_MAX + 4u];
        uint32_t length = 0;
        struct ae_login_status status;
        if (ae_login_connection_status(fixture->connection, &status) ||
            (status.phase != AE_LOGIN_SENDING && status.phase != AE_LOGIN_WAITING)) return 0;
        if (status.phase == AE_LOGIN_SENDING && ae_login_connection_pump(fixture->connection)) return 0;
        ssize_t count = recv(fixture->peer, wire, sizeof(wire), MSG_DONTWAIT);
        int matches = 0;
        if (count >= 4) {
            memcpy(&length, wire, 4);
            matches = (size_t)count - 4u == length && length == strlen(expected) &&
                !memcmp(wire + 4, expected, length);
        }
        volatile uint8_t *retired = wire;
        for (size_t i = 0; i < sizeof(wire); ++i) retired[i] = 0;
        return matches;
    }
#endif
    const uint8_t *wire = NULL;
    size_t size = 0;
    uint32_t length;
    if (ae_login_output(fixture->model, &wire, &size) || size < 4) return 0;
    memcpy(&length, wire, 4);
    return size - 4 == length && length == strlen(expected) &&
        !memcmp(wire + 4, expected, length) && !ae_login_sent(fixture->model, size);
}

static int receive_reply(fixture_conversation *fixture, const char *json) {
    size_t size = strlen(json);
    if (size > UINT32_MAX) return 0;
    uint32_t length = (uint32_t)size;
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
    if (fixture->connection) {
        for (size_t i = 0; i < size + 4u; ++i) {
            const uint8_t *byte = i < 4u ? (const uint8_t *)&length + i : (const uint8_t *)json + i - 4u;
            if (send(fixture->peer, byte, 1, MSG_NOSIGNAL) != 1 ||
                ae_login_connection_pump(fixture->connection) != (i + 1u == size + 4u ? 1 : 0)) return 0;
        }
        return 1;
    }
#endif
    return !ae_login_receive(fixture->model, (const uint8_t *)&length, 4) &&
        ae_login_receive(fixture->model, (const uint8_t *)json, length) == 1;
}
#endif
