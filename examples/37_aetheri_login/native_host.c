#define _GNU_SOURCE
#include "view.h"
#include <nexaui/software.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fixture_conversation.h"
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
#include "connection_poll.h"
#include <fcntl.h>
#endif

static uint8_t *font_bytes(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    uint8_t *data = NULL;
    if (!fseek(file, 0, SEEK_END)) {
        long length = ftell(file);
        if (length > 0 && length <= 16 * 1024 * 1024 && !fseek(file, 0, SEEK_SET)) {
            data = malloc((size_t)length);
            if (data && fread(data, 1, (size_t)length, file) != (size_t)length) {
                free(data);
                data = NULL;
            }
            if (data) *size = (size_t)length;
        }
    }
    fclose(file);
    return data;
}

int main(void) {
    const char *isolated = getenv("NEXAUI_TEST_PRIVATE_WAYLAND");
    const char *display = getenv("WAYLAND_DISPLAY");
    const char *path = getenv("NEXAUI_LOGIN_TEST_FONT");
    if (!isolated || strcmp(isolated, "1") || !display || strcmp(display, "nexaui-test") ||
        !getenv("NEXAUI_TEST_ATSPI_ADDRESS") || !path) return 2;
#ifndef AETHERI_LOGIN_CONNECTION_VIEW
    if (getenv("NEXAUI_LOGIN_TEST_CONNECTION") || getenv("NEXAUI_LOGIN_TEST_CONNECTION_LOOP") ||
        getenv("NEXAUI_LOGIN_TEST_CONNECTION_LOSS")) return 2;
#endif
    fixture_conversation *model = NULL;
    nx_window *window = NULL;
    nx_software_view *software = NULL;
    nx_font *font = NULL;
    nx_font_collection *fonts = NULL;
    aetheri_login_view *view = NULL;
    size_t bytes = 0;
    uint8_t *data = font_bytes(path, &bytes);
    unsigned requests = 0, frames = 0;
    int ime = getenv("NEXAUI_LOGIN_TEST_SIMPLE_IME") != NULL;
    int cancel = getenv("NEXAUI_LOGIN_TEST_IME_CANCEL") != NULL;
    const char *loss = getenv("NEXAUI_LOGIN_TEST_CONNECTION_LOSS");
    int loop = getenv("NEXAUI_LOGIN_TEST_CONNECTION_LOOP") != NULL;
    if (loss && (!loop || !ime || cancel ||
        (strcmp(loss, "eof") && strcmp(loss, "admission") && strcmp(loss, "timeout")))) {
        free(data);
        return 2;
    }
    int unknown_presented = 0, service_error = 0;
    unsigned expected_requests = loss ? (!strcmp(loss, "timeout") ? 1u : 0u) : 2u;
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
    aetheri_login_poll *poller = NULL;
    int control = -1, queued = 0;
    uint64_t deadline = 0;
#endif
    int secret_preedit = 0, secret_cleared = 0, visible_preedit = 0, visible_cleared = 0;
    const char *secret_request = !ime ?
        "{\"type\":\"post_auth_message_response\",\"response\":\"test\"}" : cancel ?
        "{\"type\":\"post_auth_message_response\",\"response\":\"a\"}" :
        "{\"type\":\"post_auth_message_response\",\"response\":\"\xc3\xa1\"}";
    const char *visible_request = ime ?
        "{\"type\":\"post_auth_message_response\",\"response\":\"\xc3\xa1\"}" :
        "{\"type\":\"post_auth_message_response\",\"response\":\"visible\"}";
    int result = 1;
    const nx_window_desc desc = {.title_utf8 = "NexaUI login fixture", .width = 420, .height = 260};
    if (!data || nx_font_open(data, bytes, NULL, &font) != NX_OK ||
        nx_font_collection_create(NULL, &fonts) != NX_OK || nx_font_collection_add(fonts, font) != NX_OK ||
        fixture_open(&model) || fixture_begin(model, "fixture", 7) ||
        !consume_request(model, "{\"type\":\"create_session\",\"username\":\"fixture\"}") ||
        !receive_reply(model, "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"Fixture secret prompt\"}") ||
        nx_window_create(&desc, &window) != NX_OK || nx_software_view_open(window, &software) != NX_OK ||
        fixture_view(model, fonts, window, &view) != NX_OK) goto done;

#ifdef AETHERI_LOGIN_CONNECTION_VIEW
    if (loop) {
        const char *value = getenv("NEXAUI_LOGIN_TEST_CONTROL_FD");
        if (value) {
            char *end = NULL;
            errno = 0;
            long number = strtol(value, &end, 10);
            if (errno || !*value || !end || *end || number < 0 || number > INT_MAX) goto done;
            control = (int)number;
            int flags = fcntl(control, F_GETFD);
            if (flags < 0 || fcntl(control, F_SETFD, flags | FD_CLOEXEC)) goto done;
        }
        if (!model->connection || aetheri_login_poll_open(model->connection, control, &poller) != NX_OK) goto done;
    }
#endif

    nx_context *ctx = aetheri_login_view_context(view);
    int first = 1, pending_render = 0;
    for (;;) {
        nx_window_info info;
        size_t routed = 0;
        int connection_changed = 0;
        nx_result polled;
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
        if (poller) {
            aetheri_login_poll_outcome outcome;
            polled = aetheri_login_poll_step(poller, window, first ? 0 : 200, deadline, &outcome);
            connection_changed = outcome.changed;
            service_error = outcome.connection_error;
            if (outcome.application_events) {
                char command = 0;
                if (read(control, &command, 1) != 1) goto done;
                if (command == 'E' && model->peer >= 0) {
                    if (close(model->peer)) goto done;
                    model->peer = -1;
                } else if (command == 'A') model->admission_error = -EACCES;
                else goto done;
            }
            struct ae_login_status current;
            if (fixture_status(model, &current)) goto done;
            if (deadline && current.phase != AE_LOGIN_SENDING && current.phase != AE_LOGIN_WAITING) deadline = 0;
            if (queued && current.phase == AE_LOGIN_WAITING) {
                if (requests >= 2 || !consume_request(model, requests ? visible_request : secret_request)) goto done;
                ++requests;
                queued = 0;
                if (!loss || strcmp(loss, "timeout")) {
                    if (!fixture_send_reply(model, requests == 1 ?
                        "{\"type\":\"auth_message\",\"auth_message_type\":\"visible\",\"auth_message\":\"Fixture visible prompt\"}" :
                        "{\"type\":\"success\"}")) goto done;
                }
            }
        } else
#endif
        polled = nx_window_poll(window, first ? 0 : 200);
        if (polled != NX_OK || nx_window_get_info(window, &info) != NX_OK ||
            nx_window_route_events(window, ctx, &routed) != NX_OK) break;
        if (info.should_close) {
            result = requests == expected_requests && (!loss || unknown_presented) ? 0 : 1;
            break;
        }
        if (first || routed || connection_changed) {
            first = 0;
            unsigned changes;
            do {
                if (++frames > 100 || aetheri_login_view_frame(view, info.width, info.height, &changes) != NX_OK)
                    goto done;
                if (nx_key_pressed(ctx, NX_KEY_ESCAPE)) {
                    result = requests == expected_requests && (!loss || unknown_presented) ? 0 : 1;
                    goto done;
                }
                if (changes & AETHERI_LOGIN_REQUEST_QUEUED) {
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
                    if (poller) {
                        if (queued || aetheri_login_poll_time(&deadline) != NX_OK) goto done;
                        uint64_t budget = loss && !strcmp(loss, "timeout") ? 150000000u : 5000000000u;
                        if (deadline > UINT64_MAX - budget) goto done;
                        deadline += budget;
                        queued = 1;
                    } else
#endif
                    if (requests == 0 && consume_request(model, secret_request) &&
                        receive_reply(model, "{\"type\":\"auth_message\",\"auth_message_type\":\"visible\",\"auth_message\":\"Fixture visible prompt\"}")) {
                        ++requests;
                    } else if (requests == 1 && consume_request(model, visible_request) &&
                        receive_reply(model, "{\"type\":\"success\"}")) {
                        ++requests;
                    } else goto done;
                }
            } while (changes & AETHERI_LOGIN_REPAINT);
            if (nx_window_publish_semantics(window, ctx) != NX_OK) break;
            if (ime) {
                size_t preedit_bytes = 0;
                int32_t begin = 0, end = 0;
                struct ae_login_status status;
                if (nx_window_get_ime_preedit(window, NULL, 0, &preedit_bytes, &begin, &end) != NX_OK ||
                    fixture_status(model, &status)) break;
                if (status.phase == AE_LOGIN_PROMPT && status.prompt == AE_LOGIN_SECRET && requests == 0) {
                    if (preedit_bytes && !secret_preedit) {
                        secret_preedit = 1;
                        puts("LOGIN_FIXTURE_PREEDIT secret requests=0");
                    } else if (!preedit_bytes && secret_preedit && !secret_cleared) {
                        secret_cleared = 1;
                        puts("LOGIN_FIXTURE_PREEDIT_CLEARED secret requests=0");
                    }
                }
                if (status.phase == AE_LOGIN_PROMPT && status.prompt == AE_LOGIN_VISIBLE && requests == 1) {
                    if (preedit_bytes && !visible_preedit) {
                        visible_preedit = 1;
                        puts("LOGIN_FIXTURE_PREEDIT visible requests=1");
                    } else if (!preedit_bytes && visible_preedit && !visible_cleared) {
                        visible_cleared = 1;
                        puts("LOGIN_FIXTURE_PREEDIT_CLEARED visible requests=1");
                    }
                }
                if (loss && status.phase == AE_LOGIN_UNKNOWN && !unknown_presented) {
                    if (preedit_bytes || !service_error) goto done;
                    unknown_presented = 1;
                    printf("LOGIN_FIXTURE_CONNECTION_UNKNOWN error=%d preedit=0 requests=%u\n", service_error, requests);
                }
                fflush(stdout);
            }
            pending_render = 1;
        }
        if (pending_render) {
            const nx_command *commands = NULL;
            size_t count = 0;
            nx_software_view_outcome outcome;
            if (nx_frame_commands(ctx, &commands, &count) != NX_OK ||
                nx_software_view_render(software, commands, count, (nx_color){1, 1, 1, 1}, &outcome) != NX_OK)
                break;
            pending_render = outcome != NX_SOFTWARE_VIEW_READY;
        }
    }
done:
    if (ime && !loss && (!secret_preedit || !secret_cleared || !visible_preedit || !visible_cleared))
        result = 1;
    if (loss && (!secret_preedit || !unknown_presented)) result = 1;
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
    if (loss && model) {
        int wanted = !strcmp(loss, "eof") ? -ECONNRESET : !strcmp(loss, "admission") ? -EACCES : -ECANCELED;
        if (service_error != wanted || !fixture_peer_empty(model)) result = 1;
    }
#endif
    if (window && nx_window_publish_semantics(window, NULL) != NX_OK) result = 1;
    if (software && nx_software_view_close(&software) != NX_OK) result = 1;
    aetheri_login_view_destroy(view);
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
    aetheri_login_poll_close(&poller);
    if (control >= 0 && close(control)) result = 1;
#endif
    if (window) nx_window_destroy(window);
    if (fixture_close(&model)) result = 1;
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    free(data);
    printf("LOGIN_FIXTURE_EXIT result=%d requests=%u frames=%u\n", result, requests, frames);
    return result;
}
