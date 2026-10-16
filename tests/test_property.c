#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; \
} } while (0)

static int click(nx_context *ctx, float x, float y) {
    nx_event event = {0};
    event.button = NX_BUTTON_LEFT;
    event.x = x; event.y = y;
    event.type = NX_EVENT_POINTER_DOWN;
    if (nx_context_queue_event(ctx, &event) != NX_OK) return 0;
    event.type = NX_EVENT_POINTER_UP;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int key(nx_context *ctx, nx_key code) {
    nx_event event = {0};
    event.key = code;
    event.type = NX_EVENT_KEY_DOWN;
    if (nx_context_queue_event(ctx, &event) != NX_OK) return 0;
    event.type = NX_EVENT_KEY_UP;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static nx_property_row_desc boolean_desc(void) {
    nx_property_row_desc desc = {0};
    desc.id = 11;
    desc.parent_id = 10;
    desc.bounds = (nx_rect){0, 0, 200, 32};
    desc.name = (nx_utf8_span){"Enabled", 7};
    desc.kind = NX_PROPERTY_BOOLEAN;
    return desc;
}

static nx_property_row_desc number_desc(void) {
    nx_property_row_desc desc = {0};
    desc.id = 12;
    desc.parent_id = 10;
    desc.bounds = (nx_rect){0, 36, 200, 32};
    desc.name = (nx_utf8_span){"Mass", 4};
    desc.kind = NX_PROPERTY_NUMBER;
    desc.number_value = 5;
    desc.minimum = 0;
    desc.maximum = 10;
    desc.step = 1;
    desc.unit = (nx_utf8_span){"kg", 2};
    return desc;
}

static nx_property_row_desc text_desc(void) {
    nx_property_row_desc desc = {0};
    desc.id = 13;
    desc.parent_id = 10;
    desc.bounds = (nx_rect){0, 72, 200, 32};
    desc.name = (nx_utf8_span){"Material", 8};
    desc.kind = NX_PROPERTY_TEXT;
    desc.text_value = (nx_utf8_span){"Steel", 5};
    return desc;
}

static int draw(nx_context *ctx, nx_property_row_desc *boolean,
                nx_property_row_desc *number, nx_property_row_desc *text,
                nx_property_action actions[3]) {
    nx_container_desc group = {0};
    group.id = 10;
    group.bounds = (nx_rect){0, 0, 200, 110};
    group.name = (nx_utf8_span){"Properties", 10};
    if (nx_group(ctx, &group) != NX_OK) return 0;
    return nx_property_row(ctx, boolean, &actions[0]) == NX_OK &&
           nx_property_row(ctx, number, &actions[1]) == NX_OK &&
           nx_property_row(ctx, text, &actions[2]) == NX_OK;
}

static int test_actions(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_property_row_desc boolean = boolean_desc();
    nx_property_row_desc number = number_desc();
    nx_property_row_desc text = text_desc();
    nx_property_action actions[3];
    for (unsigned frame = 0; frame < 7; ++frame) {
        if (frame == 1) CHECK(click(ctx, 180, 16));
        if (frame == 2) {
            boolean.boolean_value = 1; /* Application accepted the proposal. */
            CHECK(nx_focus_set(ctx, 12) == NX_OK);
            CHECK(key(ctx, NX_KEY_RIGHT));
        }
        if (frame == 3) {
            number.number_value = 6;
            CHECK(nx_focus_set(ctx, 12) == NX_OK);
            CHECK(key(ctx, NX_KEY_ENTER));
        }
        if (frame == 4) {
            CHECK(nx_focus_set(ctx, 13) == NX_OK);
            CHECK(key(ctx, NX_KEY_ENTER));
        }
        if (frame == 5) {
            boolean.read_only = 1;
            CHECK(click(ctx, 180, 16));
        }
        if (frame==6) {
            CHECK(nx_focus_set(ctx,12)==NX_OK);
            nx_event event={0}; event.type=NX_EVENT_KEY_DOWN_CONSUMED; event.key=NX_KEY_RIGHT;
            CHECK(nx_context_queue_event(ctx,&event)==NX_OK);
            event.type=NX_EVENT_KEY_UP; CHECK(nx_context_queue_event(ctx,&event)==NX_OK);
        }
        CHECK(nx_begin_frame(ctx, 200, 110, 0) == NX_OK);
        CHECK(draw(ctx, &boolean, &number, &text, actions));
        CHECK(nx_end_frame(ctx) == NX_OK);
        if (frame == 1) CHECK(actions[0].changed &&
                              actions[0].boolean_value == 1);
        if (frame == 2) CHECK(actions[1].changed &&
                              actions[1].number_value == 6);
        if (frame == 3) CHECK(actions[1].edit_requested &&
                              !actions[1].changed);
        if (frame == 4) CHECK(actions[2].edit_requested);
        if (frame == 5) CHECK(!actions[0].changed &&
                              !actions[0].edit_requested);
        if (frame==6) CHECK(nx_key_pressed(ctx,NX_KEY_RIGHT) && !actions[1].changed && actions[1].number_value==6);
        size_t count = 0;
        CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 4);
        uint32_t index = UINT32_MAX;
        nx_semantic_view view;
        CHECK(nx_semantics_find(ctx, 11, &index) == NX_OK);
        CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
              view.parent_id == 10 && view.role == NX_ROLE_CUSTOM &&
              view.value.length > 0);
        if (frame == 1) CHECK(view.value.length == 4 &&
                              memcmp(view.value.data, "true", 4) == 0);
        if (frame == 5) CHECK(view.state & NX_SEM_READONLY);
        CHECK(nx_semantics_find(ctx, 12, &index) == NX_OK);
        CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
              view.value.length >= 4);
        if (frame == 2) CHECK(memcmp(view.value.data, "6 kg", 4) == 0);
    }
    nx_context_destroy(ctx);
    return 0;
}

typedef struct allocation_probe {
    size_t calls, frees, fail_on;
} allocation_probe;
static void *probe_allocate(void *user, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    ++probe->calls;
    return probe->calls == probe->fail_on ? NULL : malloc(size);
}
static void probe_deallocate(void *user, void *data, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    (void)size;
    ++probe->frees;
    free(data);
}

static int test_rollback(void) {
    unsigned failures = 0;
    for (size_t offset = 1; offset <= 12; ++offset) {
        allocation_probe probe = {0};
        nx_context_desc context_desc = {0};
        context_desc.allocator = (nx_allocator){&probe, probe_allocate,
                                                 probe_deallocate};
        context_desc.initial_command_capacity = 1;
        nx_context *ctx = NULL;
        CHECK(nx_context_create(&context_desc, &ctx) == NX_OK);
        CHECK(nx_begin_frame(ctx, 200, 50, 0) == NX_OK);
        nx_container_desc group = {0};
        group.id = 10;
        group.bounds = (nx_rect){0, 0, 200, 50};
        group.name = (nx_utf8_span){"Properties", 10};
        CHECK(nx_group(ctx, &group) == NX_OK);
        nx_property_row_desc row = boolean_desc();
        nx_property_action action = {0};
        probe.fail_on = probe.calls + offset;
        nx_result result = nx_property_row(ctx, &row, &action);
        if (result == NX_ERROR_MEMORY) {
            ++failures;
            CHECK(!action.changed && !action.edit_requested);
            probe.fail_on = 0;
            CHECK(nx_property_row(ctx, &row, &action) == NX_OK);
        } else CHECK(result == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        size_t count = 0;
        CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 2);
        nx_context_destroy(ctx);
        CHECK(probe.frees == probe.calls -
                             (result == NX_ERROR_MEMORY ? 1u : 0u));
    }
    CHECK(failures >= 4);
    return 0;
}

static int test_validation(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_property_row_desc number = number_desc();
    nx_property_action action = {0};
    CHECK(nx_begin_frame(ctx, 200, 110, 0) == NX_OK);
    number.step = 0;
    CHECK(nx_property_row(ctx, &number, &action) == NX_ERROR_ARGUMENT);
    number = number_desc();
    number.number_value = 11;
    CHECK(nx_property_row(ctx, &number, &action) == NX_ERROR_ARGUMENT);
    number = number_desc();
    number.parent_id = 0;
    CHECK(nx_property_row(ctx, &number, &action) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_actions() || test_rollback() || test_validation()) return 1;
    return 0;
}
