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
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

/* Private synthetic peer only. This self-process socket/pin and test callback
   do not admit Aetheri's service, greeter, policy or a production trusted role. */
typedef struct fixture_conversation {
    struct ae_login *model;
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
    struct ae_login_connection *connection;
    int peer;
    unsigned checks;
    int admission_error;
#endif
} fixture_conversation;

#ifdef AETHERI_LOGIN_CONNECTION_VIEW
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
    fixture->peer = -1;
    if (getenv("NEXAUI_LOGIN_TEST_CONNECTION")) {
        int sockets[2] = {-1, -1};
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, sockets)) return -1;
        long descriptor = syscall(SYS_pidfd_open, getpid(), 0);
        if (descriptor < 0 || descriptor > INT_MAX) {
            close(sockets[0]); close(sockets[1]);
            return -1;
        }
        int pin = (int)descriptor;
        struct ae_login_connection_source source = {.peer_pidfd = pin,
            .uid = getuid(), .gid = getgid(), .check = fixture_admitted, .userdata = fixture};
        int result = ae_login_connection_create(&fixture->connection, &sockets[0], &source);
        close(pin);
        if (result) { close(sockets[0]); close(sockets[1]); return result; }
        fixture->peer = sockets[1];
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
        if (ae_login_connection_pump(fixture->connection)) return 0;
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
