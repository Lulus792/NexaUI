#define _POSIX_C_SOURCE 200809L
#include "view.h"
#include <nexaui/software.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int consume_request(struct ae_login *model, const char *expected) {
    const uint8_t *wire = NULL;
    size_t size = 0;
    uint32_t length;
    if (ae_login_output(model, &wire, &size) || size < 4) return 0;
    memcpy(&length, wire, 4);
    return size - 4 == length && length == strlen(expected) &&
        !memcmp(wire + 4, expected, length) && !ae_login_sent(model, size);
}

static int receive_reply(struct ae_login *model, const char *json) {
    uint32_t length = (uint32_t)strlen(json);
    return !ae_login_receive(model, (const uint8_t *)&length, 4) &&
        ae_login_receive(model, (const uint8_t *)json, length) == 1;
}

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
    struct ae_login *model = NULL;
    nx_window *window = NULL;
    nx_software_view *software = NULL;
    nx_font *font = NULL;
    nx_font_collection *fonts = NULL;
    aetheri_login_view *view = NULL;
    size_t bytes = 0;
    uint8_t *data = font_bytes(path, &bytes);
    unsigned requests = 0, frames = 0;
    int result = 1;
    const nx_window_desc desc = {.title_utf8 = "NexaUI login fixture", .width = 420, .height = 260};
    if (!data || nx_font_open(data, bytes, NULL, &font) != NX_OK ||
        nx_font_collection_create(NULL, &fonts) != NX_OK || nx_font_collection_add(fonts, font) != NX_OK ||
        ae_login_create(&model) || ae_login_begin(model, "fixture", 7) ||
        !consume_request(model, "{\"type\":\"create_session\",\"username\":\"fixture\"}") ||
        !receive_reply(model, "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"Fixture secret prompt\"}") ||
        nx_window_create(&desc, &window) != NX_OK || nx_software_view_open(window, &software) != NX_OK ||
        aetheri_login_view_create(model, fonts, window, &view) != NX_OK) goto done;

    nx_context *ctx = aetheri_login_view_context(view);
    int first = 1, pending_render = 0;
    for (;;) {
        nx_window_info info;
        size_t routed = 0;
        if (nx_window_poll(window, first ? 0 : 200) != NX_OK ||
            nx_window_get_info(window, &info) != NX_OK ||
            nx_window_route_events(window, ctx, &routed) != NX_OK) break;
        if (info.should_close) { result = requests == 2 ? 0 : 1; break; }
        if (first || routed) {
            first = 0;
            unsigned changes;
            do {
                if (++frames > 100 || aetheri_login_view_frame(view, info.width, info.height, &changes) != NX_OK)
                    goto done;
                if (nx_key_pressed(ctx, NX_KEY_ESCAPE)) {
                    result = requests == 2 ? 0 : 1;
                    goto done;
                }
                if (changes & AETHERI_LOGIN_REQUEST_QUEUED) {
                    if (requests == 0 && consume_request(model,
                        "{\"type\":\"post_auth_message_response\",\"response\":\"test\"}") &&
                        receive_reply(model, "{\"type\":\"auth_message\",\"auth_message_type\":\"visible\",\"auth_message\":\"Fixture visible prompt\"}")) {
                        ++requests;
                    } else if (requests == 1 && consume_request(model,
                        "{\"type\":\"post_auth_message_response\",\"response\":\"visible\"}") &&
                        receive_reply(model, "{\"type\":\"success\"}")) {
                        ++requests;
                    } else goto done;
                }
            } while (changes & AETHERI_LOGIN_REPAINT);
            if (nx_window_publish_semantics(window, ctx) != NX_OK) break;
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
    if (window && nx_window_publish_semantics(window, NULL) != NX_OK) result = 1;
    if (software && nx_software_view_close(&software) != NX_OK) result = 1;
    aetheri_login_view_destroy(view);
    if (window) nx_window_destroy(window);
    ae_login_close(&model);
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    free(data);
    printf("LOGIN_FIXTURE_EXIT result=%d requests=%u frames=%u\n", result, requests, frames);
    return result;
}
