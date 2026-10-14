#include "nexaui/nexaui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static nx_event pointer(nx_event_type type, float x, float y) {
    nx_event event = {0};
    event.type = type;
    event.x = x;
    event.y = y;
    event.button = NX_BUTTON_LEFT;
    return event;
}

static nx_event key_event(nx_event_type type, nx_key key, uint32_t modifiers) {
    nx_event event = {0};
    event.type = type;
    event.key = key;
    event.modifiers = modifiers;
    return event;
}

static int register_pair(nx_context *ctx, nx_interaction_state *a,
                         nx_interaction_state *b) {
    if (nx_interact_region(ctx, 1, (nx_rect){0, 0, 30, 30},
                           NX_REGION_FOCUSABLE, a) != NX_OK) return 0;
    if (nx_interact_region(ctx, 2, (nx_rect){10, 10, 30, 30},
                           NX_REGION_FOCUSABLE, b) != NX_OK) return 0;
    return 1;
}

static int test_overlap_click_and_capture(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_interaction_state a, b;
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    CHECK(register_pair(ctx, &a, &b));
    CHECK(!a.clicked && !b.clicked);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_event down = pointer(NX_EVENT_POINTER_DOWN, 20, 20);
    nx_event up = pointer(NX_EVENT_POINTER_UP, 20, 20);
    CHECK(nx_context_queue_event(ctx, &down) == NX_OK);
    CHECK(nx_context_queue_event(ctx, &up) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    CHECK(register_pair(ctx, &a, &b));
    CHECK(!a.clicked && b.clicked == 1 && b.hovered && b.focused);
    CHECK(nx_focus_id(ctx) == 2);
    CHECK(nx_end_frame(ctx) == NX_OK);

    CHECK(nx_context_queue_event(ctx, &down) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    CHECK(register_pair(ctx, &a, &b));
    CHECK(b.held && !b.clicked);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_event move = pointer(NX_EVENT_POINTER_MOVE, 90, 90);
    CHECK(nx_context_queue_event(ctx, &move) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    CHECK(register_pair(ctx, &a, &b));
    CHECK(b.held && !b.hovered);
    CHECK(nx_end_frame(ctx) == NX_OK);
    up.x = up.y = 90;
    CHECK(nx_context_queue_event(ctx, &up) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    CHECK(register_pair(ctx, &a, &b));
    CHECK(!b.held && !b.clicked);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_context_destroy(ctx);
    return 0;
}

static int test_tab_and_keyboard_activation(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_interaction_state a, b, disabled;
    nx_rect ra = {0, 0, 10, 10}, rb = {20, 0, 10, 10}, rc = {40, 0, 10, 10};
    for (unsigned frame = 0; frame < 5; ++frame) {
        if (frame == 1 || frame == 2 || frame == 3) {
            nx_event tab = key_event(NX_EVENT_KEY_DOWN, NX_KEY_TAB,
                                      frame == 3 ? NX_MOD_SHIFT : 0);
            CHECK(nx_context_queue_event(ctx, &tab) == NX_OK);
        }
        if (frame == 4) {
            nx_event down = key_event(NX_EVENT_KEY_DOWN, NX_KEY_ENTER, 0);
            nx_event up = key_event(NX_EVENT_KEY_UP, NX_KEY_ENTER, 0);
            CHECK(nx_context_queue_event(ctx, &down) == NX_OK);
            CHECK(nx_context_queue_event(ctx, &up) == NX_OK);
        }
        CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
        CHECK(nx_interact_region(ctx, 10, ra, NX_REGION_FOCUSABLE, &a) == NX_OK);
        CHECK(nx_interact_region(ctx, 20, rb, NX_REGION_FOCUSABLE, &b) == NX_OK);
        CHECK(nx_interact_region(ctx, 30, rc,
                                  NX_REGION_FOCUSABLE | NX_REGION_DISABLED,
                                  &disabled) == NX_OK);
        CHECK(!disabled.focused && !disabled.clicked);
        if (frame == 1) CHECK(a.focused && nx_focus_id(ctx) == 10);
        if (frame == 2) CHECK(b.focused && nx_focus_id(ctx) == 20);
        if (frame == 3) CHECK(a.focused && nx_focus_id(ctx) == 10);
        if (frame == 4) CHECK(a.clicked == 1 && !b.clicked);
        CHECK(nx_end_frame(ctx) == NX_OK);
    }
    CHECK(nx_focus_set(ctx, 30) == NX_ERROR_ARGUMENT);
    CHECK(nx_focus_set(ctx, 20) == NX_OK && nx_focus_id(ctx) == 20);
    CHECK(nx_focus_set(ctx, 0) == NX_OK && !nx_focus_id(ctx));
    nx_context_destroy(ctx);
    return 0;
}

static int test_disabled_overlay_and_lifetime(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_rect area = {0, 0, 20, 20};
    nx_interaction_state below, above;
    CHECK(nx_begin_frame(ctx, 30, 30, 0) == NX_OK);
    CHECK(nx_interact_region(ctx, 1, area, NX_REGION_FOCUSABLE, &below) == NX_OK);
    CHECK(nx_interact_region(ctx, 2, area,
                              NX_REGION_FOCUSABLE | NX_REGION_DISABLED,
                              &above) == NX_OK);
    CHECK(nx_interact_region(ctx, 1, area, 0, &below) == NX_ERROR_STATE);
    CHECK(nx_interact_region(ctx, 3, (nx_rect){NAN, 0, 1, 1}, 0,
                              &above) == NX_ERROR_ARGUMENT);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_event down = pointer(NX_EVENT_POINTER_DOWN, 5, 5);
    nx_event up = pointer(NX_EVENT_POINTER_UP, 5, 5);
    CHECK(nx_context_queue_event(ctx, &down) == NX_OK);
    CHECK(nx_context_queue_event(ctx, &up) == NX_OK);
    CHECK(nx_begin_frame(ctx, 30, 30, 0) == NX_OK);
    CHECK(nx_interact_region(ctx, 1, area, NX_REGION_FOCUSABLE, &below) == NX_OK);
    CHECK(nx_interact_region(ctx, 2, area,
                              NX_REGION_FOCUSABLE | NX_REGION_DISABLED,
                              &above) == NX_OK);
    CHECK(!below.clicked && !above.clicked && !nx_focus_id(ctx));
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_focus_set(ctx, 1) == NX_OK);
    CHECK(nx_begin_frame(ctx, 30, 30, 0) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(!nx_focus_id(ctx));
    nx_context_destroy(ctx);
    return 0;
}

static int test_many_regions(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_rect area = {0, 0, 10, 10};
    nx_interaction_state state;
    for (unsigned frame = 0; frame < 2; ++frame) {
        if (frame == 1) {
            nx_event down = pointer(NX_EVENT_POINTER_DOWN, 5, 5);
            nx_event up = pointer(NX_EVENT_POINTER_UP, 5, 5);
            CHECK(nx_context_queue_event(ctx, &down) == NX_OK);
            CHECK(nx_context_queue_event(ctx, &up) == NX_OK);
        }
        CHECK(nx_begin_frame(ctx, 20, 20, 0) == NX_OK);
        for (uint64_t id = 1; id <= 1000; ++id) {
            CHECK(nx_interact_region(ctx, id, area, 0, &state) == NX_OK);
            if (frame == 1 && id == 1000) CHECK(state.clicked == 1);
            else CHECK(!state.clicked);
        }
        CHECK(nx_end_frame(ctx) == NX_OK);
    }
    nx_context_destroy(ctx);
    return 0;
}

typedef struct fail_state { unsigned calls, frees, fail_after; } fail_state;
static void *fail_allocate(void *user, size_t size) {
    fail_state *state = (fail_state *)user;
    ++state->calls;
    return state->calls > state->fail_after ? NULL : malloc(size);
}
static void fail_deallocate(void *user, void *pointer, size_t size) {
    fail_state *state = (fail_state *)user;
    (void)size;
    ++state->frees;
    free(pointer);
}

static int test_region_allocation_failure(void) {
    for (unsigned limit = 2; limit <= 3; ++limit) {
        fail_state state = {0, 0, limit};
        nx_context_desc desc = {0};
        desc.allocator = (nx_allocator){&state, fail_allocate, fail_deallocate};
        nx_context *ctx = NULL;
        CHECK(nx_context_create(&desc, &ctx) == NX_OK);
        CHECK(nx_begin_frame(ctx, 10, 10, 0) == NX_OK);
        nx_interaction_state interaction;
        CHECK(nx_interact_region(ctx, 1, (nx_rect){0, 0, 5, 5}, 0,
                                  &interaction) == NX_ERROR_MEMORY);
        CHECK(nx_end_frame(ctx) == NX_OK);
        nx_context_destroy(ctx);
        CHECK(state.frees == limit);
    }
    return 0;
}

static int test_multiple_activations(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_interaction_state state;
    nx_rect area = {0, 0, 10, 10};
    CHECK(nx_begin_frame(ctx, 20, 20, 0) == NX_OK);
    CHECK(nx_interact_region(ctx, 1, area, NX_REGION_FOCUSABLE, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    for (unsigned i = 0; i < 2; ++i) {
        nx_event down = pointer(NX_EVENT_POINTER_DOWN, 5, 5);
        nx_event up = pointer(NX_EVENT_POINTER_UP, 5, 5);
        CHECK(nx_context_queue_event(ctx, &down) == NX_OK);
        CHECK(nx_context_queue_event(ctx, &up) == NX_OK);
    }
    CHECK(nx_begin_frame(ctx, 20, 20, 0) == NX_OK);
    CHECK(nx_interact_region(ctx, 1, area, NX_REGION_FOCUSABLE, &state) == NX_OK);
    CHECK(state.clicked == 2);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_context_destroy(ctx);
    return 0;
}

static int test_nested_clip_hit_regions(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_interaction_state inner, outer;
    nx_rect area = {0, 0, 20, 20};
    for (unsigned frame = 0; frame < 4; ++frame) {
        if (frame == 1 || frame == 2 || frame == 3) {
            float x = frame == 1 ? 12.0f : frame == 2 ? 8.0f : 4.0f;
            nx_event down = pointer(NX_EVENT_POINTER_DOWN, x, 4);
            nx_event up = pointer(NX_EVENT_POINTER_UP, x, 4);
            CHECK(nx_context_queue_event(ctx, &down) == NX_OK);
            CHECK(nx_context_queue_event(ctx, &up) == NX_OK);
        }
        CHECK(nx_begin_frame(ctx, 20, 20, 0) == NX_OK);
        CHECK(nx_push_clip(ctx, (nx_rect){0, 0, 10, 10}) == NX_OK);
        CHECK(nx_push_clip(ctx, (nx_rect){0, 0, 6, 6}) == NX_OK);
        CHECK(nx_interact_region(ctx, 1, area, NX_REGION_FOCUSABLE, &inner) == NX_OK);
        CHECK(nx_pop_clip(ctx) == NX_OK);
        CHECK(nx_interact_region(ctx, 2, (nx_rect){7, 0, 13, 20},
                                 NX_REGION_FOCUSABLE, &outer) == NX_OK);
        CHECK(nx_pop_clip(ctx) == NX_OK);
        if (frame == 1) CHECK(!inner.clicked && !outer.clicked);
        if (frame == 2) CHECK(!inner.clicked && outer.clicked == 1);
        if (frame == 3) CHECK(inner.clicked == 1 && !outer.clicked);
        CHECK(nx_end_frame(ctx) == NX_OK);
    }
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_overlap_click_and_capture()) return 1;
    if (test_tab_and_keyboard_activation()) return 1;
    if (test_disabled_overlay_and_lifetime()) return 1;
    if (test_many_regions()) return 1;
    if (test_region_allocation_failure()) return 1;
    if (test_multiple_activations()) return 1;
    if (test_nested_clip_hit_regions()) return 1;
    puts("interaction tests passed");
    return 0;
}
