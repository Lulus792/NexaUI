#include "nexaui/nexaui.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

typedef struct scene {
    int open, disabled, ancestor_disabled, omit, reparent, leaf, custom;
    uint64_t selected;
    unsigned clicks;
} scene;

static int frame(nx_context *ctx, scene *s) {
    if (nx_begin_frame(ctx, 240, 160, 0) != NX_OK) return 0;
    nx_semantic_desc ancestor = {0};
    ancestor.id = 1; ancestor.role = NX_ROLE_GROUP;
    ancestor.bounds = (nx_rect){0, 0, 240, 160};
    ancestor.state = s->ancestor_disabled ? NX_SEM_DISABLED : 0;
    if (nx_semantics_add(ctx, &ancestor, NULL) != NX_OK) return 0;
    nx_container_desc tree = {0};
    tree.id = s->reparent ? 20 : 10; tree.parent_id = 1;
    tree.bounds = ancestor.bounds; tree.name = (nx_utf8_span){"Hierarchy", 9};
    if (nx_tree(ctx, &tree) != NX_OK) return 0;
    nx_interaction_state state = {0};
    if (!s->omit) {
        if (s->custom) {
            nx_widget_desc d = {0};
            d.semantic.id = 11; d.semantic.parent_id = tree.id;
            d.semantic.role = NX_ROLE_BUTTON; d.semantic.bounds = (nx_rect){0, 0, 120, 24};
            d.semantic.name = (nx_utf8_span){"Custom", 6};
            d.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_EXPAND;
            d.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
            if (nx_widget_register(ctx, &d, &state) != NX_OK) return 0;
        } else {
            nx_tree_item_desc d = {0};
            d.control.id = 11; d.control.parent_id = tree.id;
            d.control.name = (nx_utf8_span){"Branch", 6};
            d.control.bounds = (nx_rect){0, 0, 120, 24}; d.control.disabled = s->disabled;
            d.has_children = !s->leaf; d.expander_id = s->leaf ? 0 : 12;
            d.expander_bounds = (nx_rect){0, 0, 20, 24};
            if (nx_tree_item(ctx, &d, s->leaf ? NULL : &s->open, &s->selected, &state) != NX_OK) return 0;
        }
    }
    s->clicks = state.clicked;
    nx_tree_item_desc sibling = {0};
    sibling.control.id = 14; sibling.control.parent_id = tree.id;
    sibling.control.name = (nx_utf8_span){"Sibling", 7};
    sibling.control.bounds = (nx_rect){0, 48, 120, 24};
    return nx_tree_item(ctx, &sibling, NULL, &s->selected, &state) == NX_OK && nx_end_frame(ctx) == NX_OK;
}

static int expanded(nx_context *ctx, uint64_t id, int expected) {
    uint32_t index; nx_semantic_view v;
    return nx_semantics_find(ctx, id, &index) == NX_OK && nx_semantics_get(ctx, index, &v) == NX_OK &&
        !!(v.state & NX_SEM_EXPANDED) == !!expected;
}

static int test_tree(void) {
    nx_context *ctx = NULL; scene s = {0}; s.selected = 14;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK && frame(ctx, &s));
    CHECK(nx_focus_set(ctx, 14) == NX_OK);
    CHECK(nx_semantics_request_action(ctx, 11, NX_ACTION_EXPAND) == NX_OK);
    CHECK(nx_semantics_request_action(ctx, 11, NX_ACTION_EXPAND) == NX_ERROR_LIMIT);
    CHECK(nx_semantics_request_action(ctx, 11, NX_ACTION_COLLAPSE) == NX_ERROR_UNSUPPORTED);
    CHECK(nx_semantics_request_action(ctx, 12, NX_ACTION_EXPAND) == NX_OK);
    s.open = 1; /* A state-setting request must not toggle an already updated model. */
    CHECK(frame(ctx, &s) && s.open && !s.clicks && s.selected == 14 && nx_focus_id(ctx) == 14);
    CHECK(expanded(ctx, 11, 1) && expanded(ctx, 12, 1));
    CHECK(nx_semantics_request_action(ctx, 11, NX_ACTION_COLLAPSE) == NX_OK);
    CHECK(frame(ctx, &s) && !s.open && !s.clicks && s.selected == 14);
    CHECK(expanded(ctx, 11, 0) && expanded(ctx, 12, 0));
    CHECK(nx_semantics_request_action(ctx, 12, NX_ACTION_EXPAND) == NX_OK);
    CHECK(frame(ctx, &s) && s.open && !s.clicks && s.selected == 14);
    CHECK(nx_semantics_request_action(ctx, 12, NX_ACTION_COLLAPSE) == NX_OK);
    CHECK(frame(ctx, &s) && !s.open && !s.clicks && s.selected == 14);
    CHECK(nx_semantics_request_action(ctx, 14, NX_ACTION_EXPAND) == NX_ERROR_UNSUPPORTED);
    nx_context_destroy(ctx);
    return 0;
}

static int test_expiry(void) {
    for (unsigned variant = 0; variant < 6; ++variant) {
        nx_context *ctx = NULL; scene s = {0}; s.selected = 14;
        CHECK(nx_context_create(NULL, &ctx) == NX_OK && frame(ctx, &s));
        CHECK(nx_semantics_request_action(ctx, 11, NX_ACTION_EXPAND) == NX_OK);
        s.disabled = variant == 0; s.ancestor_disabled = variant == 1;
        s.omit = variant == 2; s.reparent = variant == 3;
        s.leaf = variant == 4; s.custom = variant == 5;
        CHECK(frame(ctx, &s) && !s.open && !s.clicks && s.selected == 14);
        if (variant != 3) CHECK(nx_semantics_request_action(ctx, 11, NX_ACTION_EXPAND) ==
            (variant < 3 ? NX_ERROR_STATE : NX_ERROR_UNSUPPORTED));
        s = (scene){0}; s.selected = 14;
        CHECK(frame(ctx, &s) && !s.open && !s.clicks && s.selected == 14);
        nx_context_destroy(ctx);
    }
    return 0;
}

static int section_frame(nx_context *ctx, int *open, int disabled) {
    if (nx_begin_frame(ctx, 160, 100, 0) != NX_OK) return 0;
    nx_collapsible_desc d = {0};
    d.container.id = 10; d.container.name = (nx_utf8_span){"Details", 7};
    d.container.bounds = (nx_rect){0, 0, 160, 100};
    d.header_id = 11; d.header_bounds = (nx_rect){0, 0, 160, 24}; d.disabled = disabled;
    nx_interaction_state state;
    return nx_collapsible_section(ctx, &d, open, &state) == NX_OK && !state.clicked && nx_end_frame(ctx) == NX_OK;
}

static int test_section(void) {
    nx_context *ctx = NULL; int open = 0;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK && section_frame(ctx, &open, 0));
    CHECK(nx_semantics_request_action(ctx, 11, NX_ACTION_EXPAND) == NX_OK);
    open = 1;
    CHECK(section_frame(ctx, &open, 0) && open && expanded(ctx, 10, 1) && expanded(ctx, 11, 1));
    CHECK(nx_semantics_request_action(ctx, 11, NX_ACTION_COLLAPSE) == NX_OK);
    CHECK(section_frame(ctx, &open, 1) && open);
    CHECK(nx_semantics_request_action(ctx, 11, NX_ACTION_COLLAPSE) == NX_ERROR_STATE);
    CHECK(section_frame(ctx, &open, 0) && open);
    CHECK(nx_semantics_request_action(ctx, 11, NX_ACTION_COLLAPSE) == NX_OK);
    CHECK(section_frame(ctx, &open, 0) && !open && expanded(ctx, 10, 0) && expanded(ctx, 11, 0));
    nx_context_destroy(ctx);
    return 0;
}

static void *fail_once(void *user, size_t bytes) {
    int *armed = user;
    if (*armed) { *armed = 0; return NULL; }
    return malloc(bytes);
}
static void release(void *user, void *pointer, size_t bytes) {
    (void)user; (void)bytes; free(pointer);
}

static int test_paint_retry(void) {
    int armed = 0; nx_context *ctx = NULL;
    nx_context_desc config = {0};
    config.allocator = (nx_allocator){&armed, fail_once, release};
    config.initial_command_capacity = 8;
    scene s = {0}; s.selected = 14;
    CHECK(nx_context_create(&config, &ctx) == NX_OK && frame(ctx, &s));
    CHECK(nx_semantics_request_action(ctx, 11, NX_ACTION_EXPAND) == NX_OK);
    CHECK(nx_begin_frame(ctx, 240, 160, 0) == NX_OK);
    nx_semantic_desc parent = {0}; parent.id = 1; parent.role = NX_ROLE_GROUP;
    parent.bounds = (nx_rect){0, 0, 240, 160};
    CHECK(nx_semantics_add(ctx, &parent, NULL) == NX_OK);
    nx_container_desc tree = {0}; tree.id = 10; tree.parent_id = 1;
    tree.name = (nx_utf8_span){"Hierarchy", 9}; tree.bounds = parent.bounds;
    CHECK(nx_tree(ctx, &tree) == NX_OK);
    /* Fill until an actual reserve fails, without depending on private capacity
       or allocation sizes. The next paint must reserve again. */
    nx_result result = NX_OK; armed = 1;
    for (unsigned i = 0; i < 4096 && result == NX_OK; ++i)
        result = nx_fill_rect(ctx, (nx_rect){0, 0, 1, 1}, (nx_color){0, 0, 0, 1});
    CHECK(result == NX_ERROR_MEMORY && !armed);
    nx_tree_item_desc d = {0};
    d.control.id = 11; d.control.parent_id = 10; d.control.name = (nx_utf8_span){"Branch", 6};
    d.control.bounds = (nx_rect){0, 0, 120, 24}; d.has_children = 1;
    d.expander_id = 12; d.expander_bounds = (nx_rect){0, 0, 20, 24};
    nx_interaction_state state; armed = 1;
    CHECK(nx_tree_item(ctx, &d, &s.open, &s.selected, &state) == NX_ERROR_MEMORY && !armed);
    CHECK(!s.open && s.selected == 14 && !state.clicked);
    CHECK(nx_tree_item(ctx, &d, &s.open, &s.selected, &state) == NX_OK);
    CHECK(s.open && s.selected == 14 && !state.clicked && nx_end_frame(ctx) == NX_OK);
    CHECK(expanded(ctx, 11, 1) && expanded(ctx, 12, 1));
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_tree() || test_expiry() || test_section() || test_paint_retry()) return 1;
    puts("expansion action tests passed");
    return 0;
}
