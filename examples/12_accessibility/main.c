#include "nexaui/nexaui.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 0; \
} } while (0)

typedef struct demo {
    nx_context *ui;
    int checked;
    unsigned opened;
} demo;

static nx_utf8_span span(const char *s) {
    return (nx_utf8_span){s, strlen(s)};
}

static nx_control_desc control(uint64_t id, nx_rect bounds,
                               const char *name) {
    nx_control_desc desc = {0};
    desc.id = id;
    desc.parent_id = 1;
    desc.bounds = bounds;
    desc.name = span(name);
    return desc;
}

static int frame(demo *app) {
    CHECK(nx_begin_frame(app->ui, 360, 180, 1.0 / 60.0) == NX_OK);
    nx_semantic_desc root = {0};
    root.id = 1;
    root.role = NX_ROLE_WINDOW;
    root.bounds = (nx_rect){0, 0, 360, 180};
    root.name = span("Accessibility demo");
    CHECK(nx_semantics_add(app->ui, &root, NULL) == NX_OK);

    nx_interaction_state state;
    nx_control_desc open = control(2, (nx_rect){20, 25, 145, 40},
                                   "Open details");
    CHECK(nx_button(app->ui, &open, &state) == NX_OK);
    app->opened += state.clicked;
    nx_control_desc checkbox = control(3, (nx_rect){20, 78, 175, 35},
                                       "Email updates");
    CHECK(nx_checkbox(app->ui, &checkbox, &app->checked, &state) == NX_OK);
    nx_control_desc disabled = control(4, (nx_rect){205, 25, 135, 40},
                                       "Delete data");
    disabled.disabled = 1;
    CHECK(nx_button(app->ui, &disabled, &state) == NX_OK);
    CHECK(state.clicked == 0);

    nx_semantic_desc status = {0};
    status.id = 5;
    status.parent_id = 1;
    status.role = NX_ROLE_STATUS;
    status.bounds = (nx_rect){20, 125, 320, 30};
    status.name = span("Result");
    status.value = span(app->opened ? "Details opened" : "Ready");
    CHECK(nx_semantics_add(app->ui, &status, NULL) == NX_OK);
    CHECK(nx_end_frame(app->ui) == NX_OK);

    size_t count = 0;
    CHECK(nx_semantics_count(app->ui, &count) == NX_OK && count == 5);
    for (uint32_t i = 0; i < 5; ++i) {
        nx_semantic_view view;
        CHECK(nx_semantics_get(app->ui, i, &view) == NX_OK);
        CHECK(view.id == (uint64_t)i + 1u);
        CHECK(view.parent_id == (i ? 1u : 0u));
        CHECK(view.depth == (i ? 1u : 0u));
        CHECK(view.name.length > 0);
    }
    nx_semantic_view check_view, disabled_view, status_view;
    CHECK(nx_semantics_get(app->ui, 2, &check_view) == NX_OK);
    CHECK(check_view.role == NX_ROLE_CHECKBOX);
    CHECK(!!(check_view.state & NX_SEM_CHECKED) == !!app->checked);
    CHECK(!!(check_view.state & NX_SEM_FOCUSED) ==
          (nx_focus_id(app->ui) == 3));
    CHECK((check_view.actions & (NX_ACTION_PRESS | NX_ACTION_FOCUS)) ==
          (NX_ACTION_PRESS | NX_ACTION_FOCUS));
    CHECK(nx_semantics_get(app->ui, 3, &disabled_view) == NX_OK);
    CHECK(disabled_view.role == NX_ROLE_BUTTON &&
          (disabled_view.state & NX_SEM_DISABLED));
    CHECK(nx_semantics_get(app->ui, 4, &status_view) == NX_OK);
    CHECK(status_view.role == NX_ROLE_STATUS && status_view.value.length > 0);
    return 1;
}

static int key(nx_context *ui, nx_event_type type, nx_key code) {
    nx_event event = {0};
    event.type = type;
    event.key = code;
    CHECK(nx_context_queue_event(ui, &event) == NX_OK);
    return 1;
}

static int dump_tree(nx_context *ui) {
    static const char *roles[] = {"window", "button", "checkbox",
                                  "button", "status"};
    for (uint32_t i = 0; i < 5; ++i) {
        nx_semantic_view view;
        CHECK(nx_semantics_get(ui, i, &view) == NX_OK);
        printf("%*s%s: %.*s", (int)view.depth * 2, "", roles[i],
               (int)view.name.length, view.name.data);
        if (view.value.length)
            printf(" = %.*s", (int)view.value.length, view.value.data);
        if (view.state & NX_SEM_CHECKED) fputs(" [checked]", stdout);
        if (view.state & NX_SEM_DISABLED) fputs(" [disabled]", stdout);
        if (view.state & NX_SEM_FOCUSED) fputs(" [focused]", stdout);
        putchar('\n');
    }
    return 1;
}

int main(void) {
    demo app = {0};
    if (nx_context_create(NULL, &app.ui) != NX_OK) return 1;
    int ok = frame(&app);
    if (ok) ok = key(app.ui, NX_EVENT_KEY_DOWN, NX_KEY_TAB) &&
                 frame(&app) && nx_focus_id(app.ui) == 2;
    if (ok) ok = key(app.ui, NX_EVENT_KEY_DOWN, NX_KEY_TAB) &&
                 frame(&app) && nx_focus_id(app.ui) == 3;
    if (ok) ok = key(app.ui, NX_EVENT_KEY_DOWN, NX_KEY_SPACE) &&
                 key(app.ui, NX_EVENT_KEY_UP, NX_KEY_SPACE) &&
                 frame(&app) && app.checked == 1;
    if (ok) ok = key(app.ui, NX_EVENT_KEY_DOWN, NX_KEY_TAB) &&
                 frame(&app) && nx_focus_id(app.ui) == 2;
    if (ok) ok = key(app.ui, NX_EVENT_KEY_DOWN, NX_KEY_ENTER) &&
                 key(app.ui, NX_EVENT_KEY_UP, NX_KEY_ENTER) &&
                 frame(&app) && app.opened == 1;
    if (ok) ok = dump_tree(app.ui);
    nx_context_destroy(app.ui);
    if (!ok) fputs("accessibility example failed\n", stderr);
    return ok ? 0 : 1;
}
