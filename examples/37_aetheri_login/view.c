#include "view.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define LOGIN_CACHE_BYTES 65536u

struct aetheri_login_view {
    struct ae_login *conversation;
    const nx_font_collection *fonts;
    nx_window *ime_window;
    nx_context *context;
    nx_text_field *field;
    struct ae_login_status previous;
    uint64_t generation;
    int rejected;
};

static nx_utf8_span span(const char *text) {
    return (nx_utf8_span){text, strlen(text)};
}

static void *allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void deallocate(void *user, void *data, size_t size) {
    (void)user;
    /* The private context may own copies of credential input events. */
    volatile unsigned char *bytes = data;
    while (size--) *bytes++ = 0;
    free(data);
}

nx_result aetheri_login_view_create(struct ae_login *conversation,
    const nx_font_collection *fonts, nx_window *ime_window,
    aetheri_login_view **out_view) {
    if (!out_view) return NX_ERROR_ARGUMENT;
    *out_view = NULL;
    if (!fonts) return NX_ERROR_ARGUMENT;
    aetheri_login_view *view = calloc(1, sizeof(*view));
    if (!view) return NX_ERROR_MEMORY;
    nx_context_desc desc = {0};
    desc.allocator = (nx_allocator){NULL, allocate, deallocate};
    nx_result result = nx_context_create(&desc, &view->context);
    if (result != NX_OK) {
        free(view);
        return result;
    }
    view->conversation = conversation;
    view->fonts = fonts;
    view->ime_window = ime_window;
    *out_view = view;
    return NX_OK;
}

void aetheri_login_view_destroy(aetheri_login_view *view) {
    if (!view) return;
    nx_context_destroy(view->context);
    nx_text_field_destroy(view->field);
    free(view);
}

nx_context *aetheri_login_view_context(aetheri_login_view *view) {
    return view ? view->context : NULL;
}

static void clear_draft(aetheri_login_view *view) {
    nx_text_field_destroy(view->field);
    view->field = NULL;
}

static nx_utf8_span message(const struct ae_login_status *status, int available) {
    if (!available) return span("Login service unavailable.");
    switch (status->phase) {
    case AE_LOGIN_IDLE: return span("Enter your account name.");
    case AE_LOGIN_SENDING: return span("Sending request.");
    case AE_LOGIN_WAITING: return span("Waiting for the login service.");
    case AE_LOGIN_PROMPT:
        if (status->text_size) return (nx_utf8_span){status->text, status->text_size};
        return span("The login service requests a response.");
    case AE_LOGIN_AUTHENTICATED: return span("Identity confirmed. You can request a session.");
    case AE_LOGIN_DENIED: return span("Login denied. Cancel this attempt before trying again.");
    case AE_LOGIN_START_ACKNOWLEDGED: return span("The login service accepted the session request.");
    default: return span("Login state unknown. Close this view to leave the conversation.");
    }
}

static nx_result label(aetheri_login_view *view, uint64_t id,
    nx_rect bounds, nx_utf8_span text) {
    nx_text_layout *layout = NULL;
    nx_result result = nx_text_layout_create(view->fonts, text.data, text.length,
                                              NX_BIDI_AUTO, 16, bounds.width, &layout);
    if (result == NX_OK) {
        nx_color color;
        result = nx_theme_color(view->context, NX_COLOR_TEXT, &color);
        if (result == NX_OK)
            result = nx_label(view->context, id, 0, bounds, layout,
                              (nx_point){bounds.x, bounds.y}, color, LOGIN_CACHE_BYTES);
    }
    nx_text_layout_destroy(layout);
    return result;
}

static nx_result button(aetheri_login_view *view, uint64_t id, nx_rect bounds,
    const char *name, int *clicked) {
    nx_text_layout *layout = NULL;
    nx_utf8_span text = span(name);
    nx_result result = nx_text_layout_create(view->fonts, text.data, text.length,
                                              NX_BIDI_AUTO, 15, bounds.width - 16, &layout);
    if (result == NX_OK) {
        nx_control_desc desc = {0};
        desc.id = id;
        desc.bounds = bounds;
        desc.name = text;
        desc.label_layout = layout;
        desc.label_origin = (nx_point){bounds.x + 8, bounds.y + 9};
        desc.glyph_cache_bytes = LOGIN_CACHE_BYTES;
        nx_interaction_state state;
        result = nx_button(view->context, &desc, &state);
        if (result == NX_OK) *clicked = state.clicked != 0;
    }
    nx_text_layout_destroy(layout);
    return result;
}

static int has_field(const struct ae_login_status *status) {
    return status->phase == AE_LOGIN_IDLE ||
        (status->phase == AE_LOGIN_PROMPT &&
         (status->prompt == AE_LOGIN_VISIBLE || status->prompt == AE_LOGIN_SECRET));
}

static int valid_draft(nx_utf8_span draft, void *user) {
    size_t limit = *(const size_t *)user;
    return draft.length <= limit && (!draft.length || !memchr(draft.data, 0, draft.length));
}

nx_result aetheri_login_view_frame(aetheri_login_view *view, float width,
    float height, unsigned *out_changes) {
    if (!view || !out_changes || !isfinite(width) || !isfinite(height) ||
        width < 240 || height < 200) return NX_ERROR_ARGUMENT;
    *out_changes = 0;
    struct ae_login_status status = {.phase = AE_LOGIN_UNKNOWN};
    if (view->conversation && ae_login_status(view->conversation, &status))
        return NX_ERROR_STATE;
    int entering = !view->generation || status.phase != view->previous.phase ||
        status.prompt != view->previous.prompt || status.operation != view->previous.operation;
    if (entering) {
        clear_draft(view);
        if (view->generation >= UINT64_MAX / 8u) return NX_ERROR_LIMIT;
        ++view->generation;
        view->previous = (struct ae_login_status){.phase = status.phase,
            .prompt = status.prompt, .operation = status.operation};
        view->rejected = 0;
        (void)nx_focus_set(view->context, 0);
    }

    int secret = status.phase == AE_LOGIN_PROMPT && status.prompt == AE_LOGIN_SECRET;
    if (has_field(&status) && !view->field) {
        nx_result result = secret ? nx_text_field_create_password("", 0, &view->field) :
            nx_text_field_create("", 0, 0, &view->field);
        if (result != NX_OK) return result;
    }

    nx_result result = nx_begin_frame(view->context, width, height, 0);
    if (result != NX_OK) return result;
    uint64_t base = view->generation * 8u;
    float inner_width = width - 32;
    float controls_y = height - 60;
    nx_rect message_bounds = {16, 16, inner_width, controls_y - 78};
    result = nx_push_clip(view->context, message_bounds);
    if (result == NX_OK) {
        result = label(view, base + 1, message_bounds,
                       message(&status, view->conversation != NULL));
        nx_result popped = nx_pop_clip(view->context);
        if (result == NX_OK) result = popped;
    }

    int submitted = 0, valid = 1, proceed = 0, cancel = 0;
    uint64_t first_focus = 0;
    if (result == NX_OK && view->field) {
        nx_text_field_desc desc = {0};
        size_t limit = status.phase == AE_LOGIN_IDLE ? 256u : AE_LOGIN_RESPONSE_MAX;
        desc.id = base + 2;
        desc.bounds = (nx_rect){16, controls_y - 52, inner_width, 36};
        desc.name = span(status.phase == AE_LOGIN_IDLE ? "Account name" :
                         secret ? "Secret response" : "Response");
        desc.fonts = view->fonts;
        desc.pixels_per_em = 16;
        desc.glyph_cache_bytes = LOGIN_CACHE_BYTES;
        desc.password = secret;
        desc.ime_window = view->ime_window;
        desc.validate = valid_draft;
        desc.validate_user = &limit;
        nx_interaction_state state;
        result = nx_text_field_widget(view->context, &desc, view->field,
                                      &state, &valid, &submitted);
        first_focus = desc.id;
    }
    const char *primary = NULL;
    if (status.phase == AE_LOGIN_IDLE) primary = "Continue";
    if (status.phase == AE_LOGIN_PROMPT) primary = view->field ? "Answer" : "Acknowledge";
    if (status.phase == AE_LOGIN_AUTHENTICATED) primary = "Start session";
    if (result == NX_OK && primary) {
        result = button(view, base + 3, (nx_rect){16, controls_y, 132, 36},
                        primary, &proceed);
        if (!first_focus) first_focus = base + 3;
    }
    if (result == NX_OK && (status.phase == AE_LOGIN_PROMPT ||
        status.phase == AE_LOGIN_AUTHENTICATED || status.phase == AE_LOGIN_DENIED)) {
        result = button(view, base + 4, (nx_rect){156, controls_y, 68, 36},
                        "Cancel", &cancel);
        if (!first_focus) first_focus = base + 4;
    }
    if (result == NX_OK && view->rejected) {
        result = label(view, base + 5, (nx_rect){16, controls_y + 38, inner_width, 22},
                       span("Check your response and try again."));
    }
    if (result == NX_OK && first_focus && !nx_focus_id(view->context)) {
        result = nx_focus_set(view->context, first_focus);
        if (result == NX_OK) *out_changes |= AETHERI_LOGIN_REPAINT;
    }
    nx_result ended = nx_end_frame(view->context);
    if (result != NX_OK) return result;
    if (ended != NX_OK) return ended;
    if (entering || (!cancel && !submitted && !proceed)) return NX_OK;

    int accepted;
    nx_utf8_span draft = {0};
    if (view->field && !cancel) {
        if (!valid) {
            if (!view->rejected) *out_changes = AETHERI_LOGIN_REPAINT;
            view->rejected = 1;
            return NX_OK;
        }
        result = nx_text_field_text(view->field, &draft);
        if (result != NX_OK) return result;
    }
    const char *response = draft.data ? draft.data : "";
    if (cancel) accepted = ae_login_cancel(view->conversation);
    else if (status.phase == AE_LOGIN_IDLE)
        accepted = ae_login_begin(view->conversation, response, draft.length);
    else if (status.phase == AE_LOGIN_PROMPT)
        accepted = ae_login_answer(view->conversation, view->field ? response : NULL,
                                   draft.length);
    else accepted = ae_login_start(view->conversation);
    if (view->rejected != (accepted != 0)) *out_changes = AETHERI_LOGIN_REPAINT;
    view->rejected = accepted != 0;
    if (!accepted) {
        clear_draft(view);
        /* A host can send and receive another prompt before its next frame. */
        view->previous.phase = AE_LOGIN_SENDING;
        view->previous.prompt = AE_LOGIN_NO_PROMPT;
        *out_changes = AETHERI_LOGIN_REPAINT | AETHERI_LOGIN_REQUEST_QUEUED;
    }
    return NX_OK;
}
