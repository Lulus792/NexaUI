#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static nx_widget_desc button_desc(uint64_t id, uint64_t parent, nx_rect rect) {
    nx_widget_desc desc = {0};
    desc.semantic.id = id;
    desc.semantic.parent_id = parent;
    desc.semantic.role = NX_ROLE_BUTTON;
    desc.semantic.bounds = rect;
    desc.semantic.name = (nx_utf8_span){"Action", 6};
    desc.semantic.actions = NX_ACTION_PRESS | NX_ACTION_FOCUS;
    desc.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    return desc;
}

static nx_event pointer(nx_event_type type, float x, float y) {
    nx_event event = {0};
    event.type = type;
    event.x = x;
    event.y = y;
    event.button = NX_BUTTON_LEFT;
    return event;
}

static int test_registration_click_and_disabled_state(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_rect rect = {10, 10, 30, 20};
    nx_widget_desc button = button_desc(2, 1, rect);
    nx_widget_desc label = {0};
    label.semantic.id = 3;
    label.semantic.parent_id = 1;
    label.semantic.role = NX_ROLE_TEXT;
    label.semantic.bounds = (nx_rect){0, 0, 10, 10};
    label.semantic.name = (nx_utf8_span){"Label", 5};
    for (unsigned frame = 0; frame < 3; ++frame) {
        if (frame) {
            nx_event down = pointer(NX_EVENT_POINTER_DOWN, 20, 20);
            nx_event up = pointer(NX_EVENT_POINTER_UP, 20, 20);
            CHECK(nx_context_queue_event(ctx, &down) == NX_OK);
            CHECK(nx_context_queue_event(ctx, &up) == NX_OK);
        }
        CHECK(nx_begin_frame(ctx, 50, 40, 0) == NX_OK);
        nx_semantic_desc root = {0};
        root.id = 1;
        root.role = NX_ROLE_WINDOW;
        root.bounds = (nx_rect){0, 0, 50, 40};
        CHECK(nx_semantics_add(ctx, &root, NULL) == NX_OK);
        nx_interaction_state label_state = {1, 1, 1, 1};
        CHECK(nx_widget_register(ctx, &label, &label_state) == NX_OK);
        CHECK(!label_state.hovered && !label_state.held &&
              !label_state.focused && !label_state.clicked);
        button.semantic.state = frame == 2 ? NX_SEM_DISABLED : 0;
        nx_interaction_state state;
        CHECK(nx_widget_register(ctx, &button, &state) == NX_OK);
        if (frame == 1) CHECK(state.clicked == 1 && state.focused);
        if (frame == 2) CHECK(!state.clicked && !state.focused && !state.held);
        CHECK(nx_end_frame(ctx) == NX_OK);
        size_t count;
        CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 3);
        nx_semantic_view view;
        CHECK(nx_semantics_get(ctx, 2, &view) == NX_OK);
        CHECK(view.id == 2 && view.role == NX_ROLE_BUTTON);
        if (frame == 1) CHECK(view.state & NX_SEM_FOCUSED);
        if (frame == 2) CHECK((view.state & NX_SEM_DISABLED) &&
                              !(view.state & NX_SEM_FOCUSED) && !nx_focus_id(ctx));
    }
    nx_context_destroy(ctx);
    return 0;
}

static int test_validation_and_region_rollback(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 30, 30, 0) == NX_OK);
    nx_widget_desc button = button_desc(2, 0, (nx_rect){0, 0, 10, 10});
    nx_interaction_state state = {1, 1, 1, 1};
    button.flags = NX_WIDGET_FOCUSABLE;
    CHECK(nx_widget_register(ctx, &button, &state) == NX_ERROR_ARGUMENT);
    CHECK(!state.clicked && !state.focused);
    button.flags = NX_WIDGET_INTERACTIVE;
    CHECK(nx_widget_register(ctx, &button, &state) == NX_ERROR_ARGUMENT);
    button = button_desc(2, 0, (nx_rect){0, 0, 10, 10});
    CHECK(nx_interact_region(ctx, 2, button.semantic.bounds, 0, &state) == NX_OK);
    CHECK(nx_widget_register(ctx, &button, &state) == NX_ERROR_STATE);
    CHECK(!state.clicked && !state.hovered);
    CHECK(nx_semantics_add(ctx, &button.semantic, NULL) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 1);
    nx_context_destroy(ctx);
    return 0;
}

typedef struct allocation_probe {
    unsigned calls, frees, fail_on;
} allocation_probe;

static void *probe_allocate(void *user, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    ++probe->calls;
    return probe->calls == probe->fail_on ? NULL : malloc(size);
}

static void probe_deallocate(void *user, void *pointer, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    (void)size;
    ++probe->frees;
    free(pointer);
}

static int test_allocation_rollback(void) {
    allocation_probe probe = {0};
    probe.fail_on = 6;
    nx_context_desc context_desc = {0};
    context_desc.allocator = (nx_allocator){&probe, probe_allocate, probe_deallocate};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&context_desc, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 20, 20, 0) == NX_OK);
    nx_widget_desc button = button_desc(1, 0, (nx_rect){0, 0, 10, 10});
    nx_interaction_state state;
    CHECK(nx_widget_register(ctx, &button, &state) == NX_ERROR_MEMORY);
    probe.fail_on = 0;
    CHECK(nx_widget_register(ctx, &button, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 1);
    nx_semantic_view view;
    CHECK(nx_semantics_get(ctx, 0, &view) == NX_OK);
    CHECK(view.name.length == 6);
    nx_context_destroy(ctx);
    CHECK(probe.frees == probe.calls - 1u);
    return 0;
}

int main(void) {
    if (test_registration_click_and_disabled_state()) return 1;
    if (test_validation_and_region_rollback()) return 1;
    if (test_allocation_rollback()) return 1;
    puts("widget tests passed");
    return 0;
}
