#include "view.h"
#include "fixture_font.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "login view check at line %d: %s\n", __LINE__, #condition); \
    goto done; \
} } while (0)

static int update(aetheri_login_view *view, unsigned expected) {
    nx_context *ctx = aetheri_login_view_context(view);
    unsigned changes = 0;
    if (aetheri_login_view_frame(view, 320, 240, &changes) != NX_OK ||
        (changes != expected && (expected || changes != AETHERI_LOGIN_REPAINT)))
        return 0;
    if (changes & AETHERI_LOGIN_REPAINT) {
        if (aetheri_login_view_frame(view, 320, 240, &changes) != NX_OK ||
            (changes & AETHERI_LOGIN_REQUEST_QUEUED))
            return 0;
        if (changes && (aetheri_login_view_frame(view, 320, 240, &changes) != NX_OK || changes))
            return 0;
    }
    const nx_command *commands = NULL;
    size_t count = 0;
    static uint8_t pixels[320u * 240u * 4u];
    return nx_frame_commands(ctx, &commands, &count) == NX_OK && count &&
        nx_headless_render(commands, count, pixels, 320, 240, 320u * 4u,
                           (nx_color){0, 0, 0, 1}) == NX_OK;
}

static int event(nx_context *ctx, nx_event e) {
    return nx_context_queue_event(ctx, &e) == NX_OK;
}

static int key(nx_context *ctx, nx_key k) {
    return event(ctx, (nx_event){.type = NX_EVENT_KEY_DOWN, .key = k}) &&
           event(ctx, (nx_event){.type = NX_EVENT_KEY_UP, .key = k});
}

static int type(nx_context *ctx, const char *text) {
    for (; *text; ++text)
        if (!event(ctx, (nx_event){.type = NX_EVENT_TEXT, .scalar = (unsigned char)*text}))
            return 0;
    return 1;
}

static int same(nx_utf8_span span, const char *expected) {
    size_t length = strlen(expected);
    return span.length == length && (!length || !memcmp(span.data, expected, length));
}

static nx_semantic_view node(nx_context *ctx, const char *name) {
    size_t count = 0;
    if (nx_semantics_count(ctx, &count) != NX_OK) return (nx_semantic_view){0};
    for (uint32_t i = 0; i < count; ++i) {
        nx_semantic_view n;
        if (nx_semantics_get(ctx, i, &n) == NX_OK && same(n.name, name)) return n;
    }
    return (nx_semantic_view){0};
}

static int status(struct ae_login *model, enum ae_login_phase phase) {
    struct ae_login_status observation;
    return !ae_login_status(model, &observation) && observation.phase == phase;
}

static int send_queued(struct ae_login *model, const char *expected) {
    const uint8_t *bytes = NULL;
    size_t size = 0;
    uint32_t length = 0;
    if (ae_login_output(model, &bytes, &size) || size < 5) return 0;
    memcpy(&length, bytes, 4);
    return length == size - 4 && length == strlen(expected) &&
        !memcmp(expected, bytes + 4, length) && !ae_login_sent(model, size);
}

static int reply(struct ae_login *model, const char *body) {
    uint32_t length = (uint32_t)strlen(body);
    return !ae_login_receive(model, (const uint8_t *)&length, 4) &&
        ae_login_receive(model, (const uint8_t *)body, length) == 1;
}

static int press(nx_context *ctx, const char *name) {
    nx_semantic_view button = node(ctx, name);
    return button.id && nx_semantics_request_action(ctx, button.id, NX_ACTION_PRESS) == NX_OK;
}

enum { LONG_WIDTH = 240, LONG_HEIGHT = 200, LONG_BYTES = LONG_WIDTH * LONG_HEIGHT * 4 };

static int long_frame(aetheri_login_view *view, uint8_t pixels[LONG_BYTES]) {
    unsigned changes = 0;
    nx_result result = aetheri_login_view_frame(view, LONG_WIDTH, LONG_HEIGHT, &changes);
    if (result == NX_OK && changes == AETHERI_LOGIN_REPAINT)
        result = aetheri_login_view_frame(view, LONG_WIDTH, LONG_HEIGHT, &changes);
    const nx_command *commands = NULL;
    size_t count = 0;
    return result == NX_OK && !changes &&
        nx_frame_commands(aetheri_login_view_context(view), &commands, &count) == NX_OK &&
        nx_headless_render(commands, count, pixels, LONG_WIDTH, LONG_HEIGHT,
            LONG_WIDTH * 4u, (nx_color){0, 0, 0, 1}) == NX_OK;
}

static int long_prompt(nx_font_collection *fonts) {
    char prompt[AE_LOGIN_TEXT_MAX + 1];
    char body[AE_LOGIN_TEXT_MAX * 2 + 128];
    static uint8_t pixels[2][LONG_BYTES], top[2][LONG_BYTES];
    const char *question = "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"";
    struct ae_login *models[2] = {0};
    aetheri_login_view *views[2] = {0};
    nx_rect reading_bounds = {0};
    int result = 1;
    for (unsigned variant = 0; variant < 2; ++variant) {
        size_t used = strlen(question);
        memcpy(body, question, used);
        for (size_t i = 0; i < AE_LOGIN_TEXT_MAX - 1; ++i) {
            /* Enough hard wraps to require scrolling, with a final glyph
               that changes pixels only when it comes inside the clip. */
            char c = i % 32u == 31u ? '\n' : '1';
            if (i == AE_LOGIN_TEXT_MAX - 2) c = '\n';
            prompt[i] = c;
            if (c == '\n') {
                body[used++] = '\\';
                body[used++] = 'n';
            } else body[used++] = c;
        }
        prompt[AE_LOGIN_TEXT_MAX - 1] = variant ? '1' : ' ';
        prompt[AE_LOGIN_TEXT_MAX] = 0;
        body[used++] = prompt[AE_LOGIN_TEXT_MAX - 1];
        memcpy(body + used, "\"}", 3);

        struct ae_login **model = &models[variant];
        aetheri_login_view **view = &views[variant];
        CHECK(!ae_login_create(model) && !ae_login_begin(*model, "account", 7));
        CHECK(send_queued(*model, "{\"type\":\"create_session\",\"username\":\"account\"}"));
        CHECK(reply(*model, body));
        CHECK(aetheri_login_view_create(*model, fonts, NULL, view) == NX_OK);
        nx_context *ctx = aetheri_login_view_context(*view);
        CHECK(event(ctx, (nx_event){.type = NX_EVENT_FOCUS, .focused = 1}));
        CHECK(long_frame(*view, top[variant]));
        nx_semantic_view scroll = node(ctx, "Login message");
        nx_semantic_view bar = node(ctx, "Vertical scrollbar");
        nx_semantic_view text = node(ctx, prompt);
        CHECK(scroll.id && scroll.role == NX_ROLE_SCROLL_AREA && bar.parent_id == scroll.id);
        CHECK(text.id && text.parent_id == scroll.id && nx_focus_id(ctx) == bar.id);
        CHECK(same(bar.value, "0%"));
        reading_bounds = scroll.bounds;
        CHECK(key(ctx, NX_KEY_PAGE_DOWN) && long_frame(*view, pixels[variant]));
        CHECK(node(ctx, "Vertical scrollbar").bounds.y > bar.bounds.y);
        CHECK(key(ctx, NX_KEY_PAGE_UP) && long_frame(*view, pixels[variant]));
        CHECK(!memcmp(top[variant], pixels[variant], LONG_BYTES));
        CHECK(key(ctx, NX_KEY_END) && long_frame(*view, pixels[variant]));
        CHECK(same(node(ctx, "Vertical scrollbar").value, "100%"));
        CHECK(nx_focus_id(ctx) == bar.id && status(*model, AE_LOGIN_PROMPT));
    }
    CHECK(!memcmp(top[0], top[1], LONG_BYTES));
    unsigned distinct = 0;
    for (unsigned y = 0; y < LONG_HEIGHT; ++y) {
        for (unsigned x = 0; x < LONG_WIDTH; ++x) {
            size_t offset = ((size_t)y * LONG_WIDTH + x) * 4u;
            if (!memcmp(pixels[0] + offset, pixels[1] + offset, 4)) continue;
            CHECK((float)x >= reading_bounds.x && (float)y >= reading_bounds.y &&
                (float)x < reading_bounds.x + reading_bounds.width &&
                (float)y < reading_bounds.y + reading_bounds.height);
            ++distinct;
        }
    }
    CHECK(distinct > 0);
    nx_context *ctx = aetheri_login_view_context(views[1]);
    CHECK(key(ctx, NX_KEY_HOME) && long_frame(views[1], pixels[1]));
    CHECK(!memcmp(top[1], pixels[1], LONG_BYTES));
    const char *order[] = {"Secret response", "Answer", "Cancel", "Vertical scrollbar"};
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); ++i) {
        CHECK(key(ctx, NX_KEY_TAB) && long_frame(views[1], pixels[1]));
        CHECK(nx_focus_id(ctx) == node(ctx, order[i]).id);
    }
    CHECK(key(ctx, NX_KEY_END) && long_frame(views[1], pixels[1]));
    unsigned changes;
    CHECK(aetheri_login_view_frame(views[1], 640, 3000, &changes) == NX_OK);
    CHECK(changes == AETHERI_LOGIN_REPAINT);
    CHECK(!node(ctx, "Vertical scrollbar").id && nx_focus_id(ctx) == node(ctx, "Secret response").id);
    CHECK(long_frame(views[1], pixels[1]));
    CHECK(same(node(ctx, "Vertical scrollbar").value, "0%"));
    CHECK(nx_focus_id(ctx) == node(ctx, "Secret response").id);
    CHECK(press(ctx, "Cancel") && update(views[1],
        AETHERI_LOGIN_REPAINT | AETHERI_LOGIN_REQUEST_QUEUED));
    CHECK(send_queued(models[1], "{\"type\":\"cancel_session\"}"));
    CHECK(reply(models[1], "{\"type\":\"success\"}") && long_frame(views[1], pixels[1]));
    CHECK(!node(ctx, "Vertical scrollbar").id && nx_focus_id(ctx) == node(ctx, "Account name").id);
    result = 0;
done:
    for (unsigned i = 0; i < 2; ++i) {
        aetheri_login_view_destroy(views[i]);
        ae_login_close(&models[i]);
    }
    if (!result) puts("Maximum prompt: keyboard navigation, clipped final glyph and control order passed");
    return result;
}

int main(void) {
    static const char secret[] = "synthetic-123";
    const unsigned queued = AETHERI_LOGIN_REPAINT | AETHERI_LOGIN_REQUEST_QUEUED;
    uint8_t font_data[512];
    nx_font *font = NULL;
    nx_font_collection *fonts = NULL;
    struct ae_login *model = NULL;
    aetheri_login_view *view = NULL;
    int result = 1;
    font_fixture(font_data);
    CHECK(nx_font_open(font_data, sizeof(font_data), NULL, &font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &fonts) == NX_OK);
    CHECK(nx_font_collection_add(fonts, font) == NX_OK);

    CHECK(aetheri_login_view_create(NULL, fonts, NULL, &view) == NX_OK);
    CHECK(update(view, 0));
    nx_context *ctx = aetheri_login_view_context(view);
    CHECK(node(ctx, "Login service unavailable.").id && !node(ctx, "Continue").id);
    aetheri_login_view_destroy(view);
    view = NULL;

    CHECK(!ae_login_create(&model));
    CHECK(aetheri_login_view_create(model, fonts, NULL, &view) == NX_OK);
    ctx = aetheri_login_view_context(view);
    CHECK(event(ctx, (nx_event){.type = NX_EVENT_FOCUS, .focused = 1}));
    CHECK(update(view, 0));
    CHECK(nx_focus_id(ctx) == node(ctx, "Account name").id);
    CHECK(key(ctx, NX_KEY_ENTER) && update(view, AETHERI_LOGIN_REPAINT));
    CHECK(status(model, AE_LOGIN_IDLE));
    CHECK(node(ctx, "Check your response and try again.").id);
    CHECK(type(ctx, "account") && key(ctx, NX_KEY_ENTER) && update(view, queued));
    CHECK(node(ctx, "Sending request.").id && !node(ctx, "Continue").id);
    CHECK(send_queued(model, "{\"type\":\"create_session\",\"username\":\"account\"}"));
    CHECK(update(view, 0) && node(ctx, "Waiting for the login service.").id);

    CHECK(reply(model, "{\"type\":\"auth_message\",\"auth_message_type\":\"info\",\"auth_message\":\"Test information\"}"));
    CHECK(update(view, 0) && node(ctx, "Test information").id);
    CHECK(status(model, AE_LOGIN_PROMPT) && !node(ctx, "Response").id);
    CHECK(press(ctx, "Acknowledge") && update(view, queued));
    CHECK(send_queued(model, "{\"type\":\"post_auth_message_response\",\"response\":null}"));

    CHECK(reply(model, "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"Test secret question\"}"));
    CHECK(update(view, 0));
    nx_semantic_view field = node(ctx, "Secret response");
    CHECK(field.id && (field.state & NX_SEM_PROTECTED) && nx_focus_id(ctx) == field.id);
    CHECK(type(ctx, secret) && update(view, 0));
    field = node(ctx, "Secret response");
    CHECK(field.value.length == 0 && (field.state & NX_SEM_PROTECTED));
    CHECK(nx_semantics_request_text_value(ctx, field.id, (nx_utf8_span){"x", 1}) != NX_OK);
    CHECK(key(ctx, NX_KEY_ENTER) && update(view, queued));
    CHECK(!node(ctx, "Secret response").id && !node(ctx, secret).id);
    CHECK(send_queued(model, "{\"type\":\"post_auth_message_response\",\"response\":\"synthetic-123\"}"));

    CHECK(reply(model, "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"Test secret question\"}"));
    CHECK(update(view, 0) && node(ctx, "Secret response").id != field.id);
    CHECK(key(ctx, NX_KEY_ENTER) && update(view, queued));
    CHECK(send_queued(model, "{\"type\":\"post_auth_message_response\",\"response\":\"\"}"));

    CHECK(reply(model, "{\"type\":\"auth_message\",\"auth_message_type\":\"visible\",\"auth_message\":\"Test visible question\"}"));
    CHECK(update(view, 0));
    CHECK(node(ctx, "Response").id && node(ctx, "Response").id != field.id);
    CHECK(type(ctx, "visible") && update(view, 0));
    CHECK(same(node(ctx, "Response").value, "visible"));
    CHECK(press(ctx, "Answer") && update(view, queued));
    CHECK(send_queued(model, "{\"type\":\"post_auth_message_response\",\"response\":\"visible\"}"));
    CHECK(reply(model, "{\"type\":\"success\"}") && update(view, 0));
    CHECK(status(model, AE_LOGIN_AUTHENTICATED) && node(ctx, "Start session").id);
    CHECK(press(ctx, "Start session") && update(view, queued));
    CHECK(send_queued(model, "{\"type\":\"start_session\",\"cmd\":[\"/usr/libexec/aetheri/aetheri-session\"],\"env\":[\"XDG_SESSION_TYPE=wayland\",\"XDG_CURRENT_DESKTOP=Aetheri\",\"XDG_SESSION_DESKTOP=Aetheri\"]}"));
    CHECK(reply(model, "{\"type\":\"success\"}") && update(view, 0));
    CHECK(node(ctx, "The login service accepted the session request.").id);
    CHECK(!node(ctx, "Start session").id && key(ctx, NX_KEY_ENTER) && update(view, 0));
    CHECK(status(model, AE_LOGIN_START_ACKNOWLEDGED));
    aetheri_login_view_destroy(view);
    view = NULL;
    ae_login_close(&model);

    CHECK(!ae_login_create(&model));
    CHECK(!ae_login_begin(model, "account", 7));
    CHECK(send_queued(model, "{\"type\":\"create_session\",\"username\":\"account\"}"));
    CHECK(reply(model, "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"Test secret question\"}"));
    CHECK(aetheri_login_view_create(model, fonts, NULL, &view) == NX_OK);
    ctx = aetheri_login_view_context(view);
    CHECK(event(ctx, (nx_event){.type = NX_EVENT_FOCUS, .focused = 1}));
    CHECK(update(view, 0) && type(ctx, secret) && update(view, 0));
    CHECK(press(ctx, "Cancel") && update(view, queued));
    CHECK(send_queued(model, "{\"type\":\"cancel_session\"}"));
    CHECK(reply(model, "{\"type\":\"success\"}") && update(view, 0));
    CHECK(status(model, AE_LOGIN_IDLE) && same(node(ctx, "Account name").value, ""));
    CHECK(type(ctx, "account") && key(ctx, NX_KEY_ENTER) && update(view, queued));
    CHECK(send_queued(model, "{\"type\":\"create_session\",\"username\":\"account\"}"));
    CHECK(reply(model, "{\"type\":\"error\",\"error_type\":\"auth_error\",\"description\":\"Denied\"}"));
    CHECK(update(view, 0) && status(model, AE_LOGIN_DENIED));
    CHECK(!node(ctx, "Continue").id && !node(ctx, "Start session").id);
    CHECK(press(ctx, "Cancel") && update(view, queued));
    CHECK(send_queued(model, "{\"type\":\"cancel_session\"}"));
    CHECK(reply(model, "{\"type\":\"success\"}") && update(view, 0));
    CHECK(type(ctx, "account") && key(ctx, NX_KEY_ENTER) && update(view, queued));
    CHECK(send_queued(model, "{\"type\":\"create_session\",\"username\":\"account\"}"));
    CHECK(reply(model, "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"Test secret question\"}"));
    CHECK(update(view, 0) && type(ctx, secret) && update(view, 0));
    CHECK(press(ctx, "Answer"));
    ae_login_interrupted(model);
    CHECK(update(view, 0));
    CHECK(status(model, AE_LOGIN_UNKNOWN) && !node(ctx, "Secret response").id);
    CHECK(!node(ctx, "Answer").id && !node(ctx, "Cancel").id);
    CHECK(node(ctx, "Login state unknown. Close this view to leave the conversation.").id);
    CHECK(!long_prompt(fonts));
    result = 0;
done:
    aetheri_login_view_destroy(view);
    ae_login_close(&model);
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    if (!result) puts("Original login view: explicit requests, protected semantics, cancel and loss passed");
    return result;
}
