#include "nexaui/nexaui.h"

#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

typedef struct model {
    int root_open, child_open, child_disabled, child_missing;
    uint64_t selected, other_selected;
} model;

static int item(nx_context *ctx, uint64_t id, uint64_t parent, float y,
                 int *expanded, int disabled, uint64_t *selected) {
    nx_tree_item_desc d = {0};
    d.control.id = id;
    d.control.parent_id = parent;
    d.control.name = (nx_utf8_span){"Item", 4};
    d.control.bounds = (nx_rect){0, y, 120, 24};
    d.control.disabled = disabled;
    if (expanded) {
        d.has_children = 1;
        d.expander_id = id + 1;
        d.expander_bounds = (nx_rect){0, y, 20, 24};
    }
    nx_interaction_state state;
    return nx_tree_item(ctx, &d, expanded, selected, &state) == NX_OK;
}

static int frame(nx_context *ctx, model *m) {
    if (nx_begin_frame(ctx, 240, 180, 0) != NX_OK) return 0;
    nx_container_desc tree = {0};
    tree.id = 10;
    tree.name = (nx_utf8_span){"Hierarchy", 9};
    tree.bounds = (nx_rect){0, 0, 120, 120};
    if (nx_tree(ctx, &tree) != NX_OK ||
        !item(ctx, 11, 10, 0, &m->root_open, 0, &m->selected)) return 0;
    if (m->root_open) {
        if (!item(ctx, 13, 11, 24, &m->child_open, m->child_disabled, &m->selected)) return 0;
        if (m->child_open && !m->child_missing &&
            !item(ctx, 15, 13, 48, NULL, 0, &m->selected)) return 0;
        if (!item(ctx, 16, 11, 72, NULL, 0, &m->selected)) return 0;
    }
    if (!item(ctx, 17, 10, 96, NULL, 0, &m->selected)) return 0;
    tree.id = 30;
    tree.bounds.y = 120;
    tree.bounds.height = 48;
    if (nx_tree(ctx, &tree) != NX_OK ||
        !item(ctx, 31, 30, 120, NULL, 0, &m->other_selected) ||
        !item(ctx, 32, 30, 144, NULL, 0, &m->other_selected)) return 0;
    return nx_end_frame(ctx) == NX_OK;
}

static int key(nx_context *ctx, nx_key code, int consumed, int repeat) {
    nx_event e = {0};
    e.key = code;
    e.repeat = repeat;
    e.type = consumed ? NX_EVENT_KEY_DOWN_CONSUMED : NX_EVENT_KEY_DOWN;
    if (nx_context_queue_event(ctx, &e) != NX_OK) return 0;
    e.type = NX_EVENT_KEY_UP;
    return nx_context_queue_event(ctx, &e) == NX_OK;
}

static int focused(nx_context *ctx, uint64_t id) {
    uint32_t index;
    nx_semantic_view view;
    return nx_focus_id(ctx) == id && nx_semantics_find(ctx, id, &index) == NX_OK &&
        nx_semantics_get(ctx, index, &view) == NX_OK && (view.state & NX_SEM_FOCUSED);
}

static int test_navigation(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    model m = {1, 1, 0, 0, 15, 31};
    CHECK(frame(ctx, &m));
    CHECK(nx_focus_set(ctx, 11) == NX_OK);
    const struct { nx_key key; uint64_t focus; int root, child; } steps[] = {
        {NX_KEY_DOWN, 13, 1, 1}, {NX_KEY_RIGHT, 15, 1, 1},
        {NX_KEY_UP, 13, 1, 1}, {NX_KEY_RIGHT, 15, 1, 1},
        {NX_KEY_LEFT, 13, 1, 1}, {NX_KEY_LEFT, 13, 1, 0},
        {NX_KEY_DOWN, 16, 1, 0}, {NX_KEY_UP, 13, 1, 0},
        {NX_KEY_LEFT, 11, 1, 0}, {NX_KEY_RIGHT, 13, 1, 0},
        {NX_KEY_HOME, 11, 1, 0}, {NX_KEY_END, 17, 1, 0},
        {NX_KEY_DOWN, 17, 1, 0}, {NX_KEY_UP, 16, 1, 0},
        {NX_KEY_HOME, 11, 1, 0}, {NX_KEY_UP, 11, 1, 0},
        {NX_KEY_LEFT, 11, 0, 0}, {NX_KEY_RIGHT, 11, 1, 0},
        {NX_KEY_RIGHT, 13, 1, 0}
    };
    for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); ++i) {
        CHECK(key(ctx, steps[i].key, 0, 0));
        CHECK(frame(ctx, &m));
        if (nx_focus_id(ctx) != steps[i].focus)
            fprintf(stderr, "tree navigation step %zu: focus %llu, expected %llu\n", i,
                (unsigned long long)nx_focus_id(ctx), (unsigned long long)steps[i].focus);
        CHECK(focused(ctx, steps[i].focus));
        CHECK(m.root_open == steps[i].root && m.child_open == steps[i].child);
        CHECK(m.selected == 15 && m.other_selected == 31);
    }
    /* Independent trees never share directional navigation or selection. */
    CHECK(nx_focus_set(ctx, 31) == NX_OK);
    const nx_key other_keys[] = {NX_KEY_DOWN, NX_KEY_END, NX_KEY_DOWN, NX_KEY_HOME, NX_KEY_UP};
    const uint64_t other_focus[] = {32, 32, 32, 31, 31};
    for (size_t i = 0; i < 5; ++i) {
        CHECK(key(ctx, other_keys[i], 0, 0) && frame(ctx, &m));
        CHECK(focused(ctx, other_focus[i]) && m.selected == 15 && m.other_selected == 31);
    }
    nx_context_destroy(ctx);
    return 0;
}

static int test_disabled_and_unloaded(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    model m = {1, 1, 1, 0, 15, 31};
    CHECK(frame(ctx, &m) && nx_focus_set(ctx, 11) == NX_OK);
    CHECK(key(ctx, NX_KEY_DOWN, 0, 0) && frame(ctx, &m) && focused(ctx, 16));
    CHECK(key(ctx, NX_KEY_LEFT, 0, 0) && frame(ctx, &m) && focused(ctx, 11));
    CHECK(key(ctx, NX_KEY_RIGHT, 0, 0) && frame(ctx, &m) && focused(ctx, 16));
    CHECK(key(ctx, NX_KEY_UP, 0, 0) && frame(ctx, &m) && focused(ctx, 11));
    m.child_disabled = 0;
    m.child_missing = 1;
    m.child_open = 0;
    CHECK(frame(ctx, &m) && nx_focus_set(ctx, 13) == NX_OK);
    CHECK(key(ctx, NX_KEY_RIGHT, 0, 0) && frame(ctx, &m) && focused(ctx, 13) && m.child_open);
    /* An expanded branch with no published child cannot invent a focus target. */
    CHECK(key(ctx, NX_KEY_RIGHT, 0, 0) && frame(ctx, &m) && focused(ctx, 13));
    CHECK(key(ctx, NX_KEY_DOWN, 0, 0) && frame(ctx, &m) && focused(ctx, 16));
    m.child_missing = 0;
    CHECK(frame(ctx, &m) && nx_focus_set(ctx, 13) == NX_OK);
    CHECK(key(ctx, NX_KEY_RIGHT, 0, 0) && frame(ctx, &m) && focused(ctx, 15));
    CHECK(m.selected == 15 && m.other_selected == 31);
    nx_context_destroy(ctx);
    return 0;
}

static int test_batches(void) {
    for (unsigned scenario = 0; scenario < 5; ++scenario) {
        nx_context *ctx = NULL;
        CHECK(nx_context_create(NULL, &ctx) == NX_OK);
        model m = {1, 1, 0, 0, 15, 31};
        CHECK(frame(ctx, &m) && nx_focus_set(ctx, scenario == 4 ? 12 : 11) == NX_OK);
        uint64_t expected = 11;
        if (scenario == 0) {
            CHECK(key(ctx, NX_KEY_LEFT, 0, 0) && key(ctx, NX_KEY_DOWN, 0, 0));
            expected = 17; /* Descendants are already hidden for this batch. */
        } else if (scenario == 1) {
            CHECK(key(ctx, NX_KEY_RIGHT, 0, 0) && key(ctx, NX_KEY_RIGHT, 0, 0));
            CHECK(key(ctx, NX_KEY_LEFT, 0, 0) && key(ctx, NX_KEY_LEFT, 0, 0));
            CHECK(key(ctx, NX_KEY_DOWN, 0, 0));
            expected = 16;
        } else if (scenario == 2) {
            CHECK(key(ctx, NX_KEY_DOWN, 0, 1) && key(ctx, NX_KEY_DOWN, 0, 1));
            expected = 15;
        } else if (scenario == 3) {
            CHECK(key(ctx, NX_KEY_DOWN, 1, 0)); /* Consumed by an input method. */
        } else {
            CHECK(key(ctx, NX_KEY_DOWN, 0, 0) && key(ctx, NX_KEY_ENTER, 0, 0));
            expected = 13;
        }
        CHECK(frame(ctx, &m) && focused(ctx, expected));
        CHECK(m.root_open == (scenario != 0) && m.child_open == (scenario != 1));
        CHECK(m.selected == (scenario == 4 ? 13u : 15u) && m.other_selected == 31);
        nx_context_destroy(ctx);
    }
    return 0;
}

static int table_frame(nx_context *ctx, uint64_t *selected) {
    if (nx_begin_frame(ctx, 240, 180, 0) != NX_OK) return 0;
    nx_table_column_desc column = {0};
    column.id = 101;
    column.name = (nx_utf8_span){"Name", 4};
    column.width = column.maximum_width = 120;
    column.minimum_width = 20;
    nx_table_desc table = {0};
    table.id = 100;
    table.name = (nx_utf8_span){"Hierarchy table", 15};
    table.bounds = (nx_rect){0, 0, 120, 96};
    table.header_height = 24;
    table.columns = &column;
    table.column_count = 1;
    nx_table_sort sort = {0};
    if (nx_table_headers(ctx, &table, &sort) != NX_OK) return 0;
    for (unsigned i = 0; i < 2; ++i) {
        nx_container_desc row = {0};
        row.id = 102 + i * 10;
        row.parent_id = 100;
        row.name = (nx_utf8_span){"Row", 3};
        row.bounds = (nx_rect){0, 24.0f + i * 24.0f, 120, 24};
        if (nx_table_row(ctx, &row) != NX_OK) return 0;
        row.parent_id = row.id;
        ++row.id;
        if (nx_table_cell(ctx, &row) != NX_OK ||
            !item(ctx, 104 + i * 10, row.id, row.bounds.y, NULL, 0, selected)) return 0;
    }
    return nx_end_frame(ctx) == NX_OK;
}

static int modal_frame(nx_context *ctx, int show, uint64_t *selected) {
    if (nx_begin_frame(ctx, 240, 180, 0) != NX_OK) return 0;
    nx_container_desc tree = {0};
    tree.id = 30;
    tree.name = (nx_utf8_span){"Background", 10};
    tree.bounds = (nx_rect){0, 0, 120, 48};
    if (nx_tree(ctx, &tree) != NX_OK ||
        !item(ctx, 31, 30, 0, NULL, 0, selected)) return 0;
    if (show) {
        nx_overlay_desc modal = {0};
        modal.panel.id = 40;
        modal.panel.name = (nx_utf8_span){"Review", 6};
        modal.panel.bounds = (nx_rect){0, 0, 160, 120};
        modal.backdrop_id = 41;
        int dismissed = 0;
        if (nx_modal_begin(ctx, &modal, &dismissed) != NX_OK || dismissed) return 0;
        tree.id = 50;
        tree.parent_id = 40;
        if (nx_tree(ctx, &tree) != NX_OK ||
            !item(ctx, 51, 50, 0, NULL, 0, selected) ||
            !item(ctx, 52, 50, 24, NULL, 0, selected) ||
            nx_overlay_end(ctx) != NX_OK) return 0;
    }
    return nx_end_frame(ctx) == NX_OK;
}

static int test_table_and_modal(void) {
    nx_context *ctx = NULL;
    uint64_t selected = 104;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(table_frame(ctx, &selected) && nx_focus_set(ctx, 104) == NX_OK);
    CHECK(key(ctx, NX_KEY_DOWN, 0, 0) && table_frame(ctx, &selected) && focused(ctx, 114));
    CHECK(key(ctx, NX_KEY_DOWN, 0, 0) && table_frame(ctx, &selected) && focused(ctx, 114));
    CHECK(key(ctx, NX_KEY_HOME, 0, 0) && table_frame(ctx, &selected) && focused(ctx, 104));
    CHECK(key(ctx, NX_KEY_END, 0, 0) && table_frame(ctx, &selected) && focused(ctx, 114));
    CHECK(selected == 104);
    nx_context_destroy(ctx);
    ctx = NULL;
    selected = 31;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(modal_frame(ctx, 0, &selected) && nx_focus_set(ctx, 31) == NX_OK);
    CHECK(modal_frame(ctx, 1, &selected) && focused(ctx, 51));
    CHECK(key(ctx, NX_KEY_END, 0, 0) && modal_frame(ctx, 1, &selected) && focused(ctx, 52));
    CHECK(key(ctx, NX_KEY_DOWN, 0, 0) && modal_frame(ctx, 1, &selected) && focused(ctx, 52));
    CHECK(nx_focus_set(ctx, 31) == NX_ERROR_ARGUMENT);
    CHECK(key(ctx, NX_KEY_HOME, 0, 0) && modal_frame(ctx, 1, &selected) && focused(ctx, 51));
    CHECK(modal_frame(ctx, 0, &selected) && focused(ctx, 31) && selected == 31);
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_navigation() || test_disabled_and_unloaded() || test_batches() ||
        test_table_and_modal()) return 1;
    puts("tree navigation tests passed");
    return 0;
}
