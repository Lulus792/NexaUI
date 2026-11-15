#define _GNU_SOURCE
#include "view.h"
#include "login/connection.h"
#include "fixture_font.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "connection view line %d: %s (errno=%d)\n", __LINE__, #condition, errno); \
    goto done; \
} } while (0)

struct admission { unsigned calls; int error; };
static int check_admission(void *user) {
    struct admission *admission = user;
    ++admission->calls;
    return admission->error;
}

static nx_semantic_view node(nx_context *ctx, const char *name) {
    size_t count = 0, length = strlen(name);
    if (nx_semantics_count(ctx, &count) != NX_OK) return (nx_semantic_view){0};
    for (uint32_t i = 0; i < count; ++i) {
        nx_semantic_view item;
        if (nx_semantics_get(ctx, i, &item) == NX_OK && item.name.length == length &&
            !memcmp(item.name.data, name, length)) return item;
    }
    return (nx_semantic_view){0};
}

static int frame(aetheri_login_view *view, unsigned queued) {
    unsigned requests = 0, changes = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (aetheri_login_view_frame(view, 320, 240, &changes) != NX_OK) return 0;
        if (changes & AETHERI_LOGIN_REQUEST_QUEUED) ++requests;
        if (!(changes & AETHERI_LOGIN_REPAINT)) break;
    }
    if (requests != queued || (changes & AETHERI_LOGIN_REPAINT)) return 0;
    const nx_command *commands = NULL;
    size_t count = 0;
    static uint8_t pixels[320u * 240u * 4u];
    return nx_frame_commands(aetheri_login_view_context(view), &commands, &count) == NX_OK && count &&
        nx_headless_render(commands, count, pixels, 320, 240, 320u * 4u,
                           (nx_color){0, 0, 0, 1}) == NX_OK;
}

static int focus(aetheri_login_view *view) {
    nx_event event = {.type = NX_EVENT_FOCUS, .focused = 1};
    return nx_context_queue_event(aetheri_login_view_context(view), &event) == NX_OK && frame(view, 0);
}

static int type(nx_context *ctx, const char *text) {
    for (; *text; ++text) {
        nx_event event = {.type = NX_EVENT_TEXT, .scalar = (unsigned char)*text};
        if (nx_context_queue_event(ctx, &event) != NX_OK) return 0;
    }
    return 1;
}

static int press(nx_context *ctx, const char *name) {
    nx_semantic_view item = node(ctx, name);
    return item.id && nx_semantics_request_action(ctx, item.id, NX_ACTION_PRESS) == NX_OK;
}

static int phase(struct ae_login_connection *connection, enum ae_login_phase expected) {
    struct ae_login_status status;
    return !ae_login_connection_status(connection, &status) && status.phase == expected;
}

static int no_bytes(int peer) {
    char byte;
    return recv(peer, &byte, 1, MSG_DONTWAIT) == -1 && (errno == EAGAIN || errno == EWOULDBLOCK);
}

static int request(struct ae_login_connection *connection, int peer, const char *expected) {
    int socket_fd = -1, peer_pidfd = -1;
    short events = 0;
    if (!phase(connection, AE_LOGIN_SENDING) ||
        ae_login_connection_watch(connection, &socket_fd, &events, &peer_pidfd) ||
        socket_fd < 0 || peer_pidfd < 0 || !(events & POLLOUT) ||
        ae_login_connection_pump(connection) || !phase(connection, AE_LOGIN_WAITING)) return 0;
    uint8_t bytes[AE_LOGIN_WIRE_MAX + 4u];
    ssize_t size = recv(peer, bytes, sizeof(bytes), MSG_DONTWAIT);
    if (size < 4) return 0;
    uint32_t length = 0;
    memcpy(&length, bytes, 4);
    return length == (size_t)size - 4u && length == strlen(expected) &&
        !memcmp(bytes + 4, expected, length) && no_bytes(peer);
}

static int reply(struct ae_login_connection *connection, int peer, const char *body) {
    uint8_t bytes[AE_LOGIN_TEXT_MAX + 256u];
    size_t size = strlen(body);
    if (size + 4u > sizeof(bytes) || size > UINT32_MAX) return 0;
    uint32_t length = (uint32_t)size;
    memcpy(bytes, &length, 4);
    memcpy(bytes + 4, body, size);
    /* Fragment actual socket input, rather than feeding a second UI model. */
    for (size_t i = 0; i < size + 4u; ++i) {
        if (send(peer, bytes + i, 1, MSG_NOSIGNAL) != 1 ||
            ae_login_connection_pump(connection) != (i + 1u == size + 4u ? 1 : 0)) return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    int result = 1;
    int pair[2] = {-1, -1}, pin = -1;
    struct ae_login_connection *connection = NULL;
    aetheri_login_view *view = NULL;
    nx_font *font = NULL;
    nx_font_collection *fonts = NULL;
    struct admission admission = {0};
    uint8_t font_bytes[512];
    CHECK(argc == 2);
    int flow = !strcmp(argv[1], "flow"), loss = !strcmp(argv[1], "loss");
    int admitted = !strcmp(argv[1], "admission"), lifetime = !strcmp(argv[1], "lifetime");
    CHECK(flow || loss || admitted || lifetime);
    font_fixture(font_bytes);
    CHECK(nx_font_open(font_bytes, sizeof(font_bytes), NULL, &font) == NX_OK &&
          nx_font_collection_create(NULL, &fonts) == NX_OK && nx_font_collection_add(fonts, font) == NX_OK);
    if (lifetime) {
        CHECK(aetheri_login_view_create_connection(NULL, fonts, NULL, &view) == NX_OK && frame(view, 0));
        CHECK(node(aetheri_login_view_context(view), "Login service unavailable.").id);
        CHECK(!node(aetheri_login_view_context(view), "Continue").id);
        aetheri_login_view_destroy(view);
        view = NULL;
    }
    CHECK(!socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, pair));
    long descriptor = syscall(SYS_pidfd_open, getpid(), 0);
    CHECK(descriptor >= 0 && descriptor <= INT_MAX);
    pin = (int)descriptor;
    struct ae_login_connection_source source = {.peer_pidfd = pin, .uid = getuid(), .gid = getgid(),
        .check = check_admission, .userdata = &admission};
    CHECK(!ae_login_connection_create(&connection, &pair[0], &source) && pair[0] == -1);
    CHECK(close(pin) == 0);
    pin = -1;
    unsigned checks = admission.calls;
    CHECK(aetheri_login_view_create_connection(connection, fonts, NULL, &view) == NX_OK && focus(view));
    nx_context *ctx = aetheri_login_view_context(view);
    CHECK(admission.calls == checks && phase(connection, AE_LOGIN_IDLE) && no_bytes(pair[1]));
    CHECK(type(ctx, "fixture") && frame(view, 0) && press(ctx, "Continue") && frame(view, 1));
    CHECK(admission.calls > checks && request(connection, pair[1], "{\"type\":\"create_session\",\"username\":\"fixture\"}"));
    CHECK(frame(view, 0) && node(ctx, "Waiting for the login service.").id && no_bytes(pair[1]));
    CHECK(reply(connection, pair[1], "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"Synthetic question\"}") && frame(view, 0));
    nx_semantic_view secret = node(ctx, "Secret response");
    CHECK(secret.id && (secret.state & NX_SEM_PROTECTED) && !secret.value.length);
    nx_event letter = {.type = NX_EVENT_TEXT, .scalar = 0x00e1};
    CHECK(nx_context_queue_event(ctx, &letter) == NX_OK && frame(view, 0));
    CHECK(!node(ctx, "Secret response").value.length && no_bytes(pair[1]));
    if (flow) {
        CHECK(press(ctx, "Answer") && frame(view, 1));
        CHECK(request(connection, pair[1], "{\"type\":\"post_auth_message_response\",\"response\":\"\xc3\xa1\"}"));
        CHECK(reply(connection, pair[1], "{\"type\":\"auth_message\",\"auth_message_type\":\"visible\",\"auth_message\":\"Synthetic visible question\"}") && frame(view, 0));
        uint32_t index = 0;
        CHECK(nx_semantics_find(ctx, secret.id, &index) != NX_OK && node(ctx, "Response").id);
        CHECK(press(ctx, "Answer") && frame(view, 1));
        CHECK(request(connection, pair[1], "{\"type\":\"post_auth_message_response\",\"response\":\"\"}"));
        CHECK(reply(connection, pair[1], "{\"type\":\"auth_message\",\"auth_message_type\":\"info\",\"auth_message\":\"Synthetic information\"}") && frame(view, 0));
        CHECK(!node(ctx, "Response").id && press(ctx, "Acknowledge") && frame(view, 1));
        CHECK(request(connection, pair[1], "{\"type\":\"post_auth_message_response\",\"response\":null}"));
        CHECK(reply(connection, pair[1], "{\"type\":\"success\"}") && frame(view, 0));
        CHECK(phase(connection, AE_LOGIN_AUTHENTICATED) && no_bytes(pair[1]));
        CHECK(press(ctx, "Start session") && frame(view, 1));
        CHECK(request(connection, pair[1], "{\"type\":\"start_session\",\"cmd\":[\"/usr/libexec/aetheri/aetheri-session\"],\"env\":[\"XDG_SESSION_TYPE=wayland\",\"XDG_CURRENT_DESKTOP=Aetheri\",\"XDG_SESSION_DESKTOP=Aetheri\"]}"));
        CHECK(reply(connection, pair[1], "{\"type\":\"success\"}") && frame(view, 0));
        CHECK(node(ctx, "The login service accepted the session request.").id && !node(ctx, "Start session").id);
        CHECK(close(pair[1]) == 0);
        pair[1] = -1;
        CHECK(ae_login_connection_pump(connection) == -ECONNRESET && phase(connection, AE_LOGIN_START_ACKNOWLEDGED));
        CHECK(frame(view, 0) && node(ctx, "The login service accepted the session request.").id);
    } else if (loss || admitted) {
        CHECK(press(ctx, "Answer"));
        if (loss) {
            CHECK(close(pair[1]) == 0);
            pair[1] = -1;
            CHECK(ae_login_connection_pump(connection) == -ECONNRESET);
        } else admission.error = -EACCES;
        CHECK(frame(view, 0) && phase(connection, AE_LOGIN_UNKNOWN));
        CHECK(!node(ctx, "Secret response").id && !node(ctx, "Answer").id);
        CHECK(node(ctx, "Login state unknown. Close this view to leave the conversation.").id);
        if (admitted) {
            CHECK(no_bytes(pair[1]) && ae_login_connection_pump(connection) == -EACCES);
            admission.error = 0;
            CHECK(ae_login_connection_begin(connection, "fixture", 7) == -EACCES && frame(view, 0));
            CHECK(no_bytes(pair[1]));
        }
    } else {
        int socket_fd = -1, peer_pidfd = -1;
        short interests = 0;
        CHECK(!ae_login_connection_watch(connection, &socket_fd, &interests, &peer_pidfd));
        aetheri_login_view_destroy(view);
        view = NULL;
        CHECK(fcntl(socket_fd, F_GETFD) >= 0 && fcntl(peer_pidfd, F_GETFD) >= 0 &&
              phase(connection, AE_LOGIN_PROMPT) && no_bytes(pair[1]));
        CHECK(aetheri_login_view_create_connection(connection, fonts, NULL, &view) == NX_OK && focus(view));
        ctx = aetheri_login_view_context(view);
        CHECK(node(ctx, "Secret response").id && no_bytes(pair[1]));
        CHECK(press(ctx, "Cancel") && frame(view, 1) && request(connection, pair[1], "{\"type\":\"cancel_session\"}"));
        CHECK(reply(connection, pair[1], "{\"type\":\"success\"}") && frame(view, 0));
        CHECK(phase(connection, AE_LOGIN_IDLE) && node(ctx, "Account name").id && no_bytes(pair[1]));
    }
    result = 0;
done:
    aetheri_login_view_destroy(view);
    if (ae_login_connection_close(&connection)) result = 1;
    for (unsigned i = 0; i < 2; ++i) if (pair[i] >= 0 && close(pair[i])) result = 1;
    if (pin >= 0 && close(pin)) result = 1;
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    if (!result) printf("Original connection view: %s passed; host owns admission, transport and lifetime\n", argv[1]);
    return result;
}
