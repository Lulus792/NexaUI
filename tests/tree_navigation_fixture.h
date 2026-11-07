#ifndef NEXAUI_TEST_TREE_NAVIGATION_FIXTURE_H
#define NEXAUI_TEST_TREE_NAVIGATION_FIXTURE_H

#include "nexaui/nexaui.h"
#include <stdio.h>

/* Shared test data, not an application hierarchy or filesystem model. */
typedef struct model {
    int root_open, child_open, child_disabled, child_missing;
    uint64_t selected, other_selected;
    nx_text_layout *const *labels; /* Optional borrowed native-fixture labels. */
} model;

static inline int labeled_item(nx_context *ctx, uint64_t id, uint64_t parent, float y,
    int *expanded, int disabled, uint64_t *selected, const nx_text_layout *label) {
    nx_tree_item_desc d = {0};
    char name[32];
    int length = snprintf(name, sizeof(name), "Node %llu", (unsigned long long)id);
    if (length <= 0 || (size_t)length >= sizeof(name)) return 0;
    d.control.id = id;
    d.control.parent_id = parent;
    d.control.name = (nx_utf8_span){name, (size_t)length};
    d.control.bounds = (nx_rect){0, y, 120, 24};
    d.control.disabled = disabled;
    d.control.label_layout = label;
    d.control.label_origin = (nx_point){24, y + 3};
    d.control.glyph_cache_bytes = 65536;
    if (expanded) {
        d.has_children = 1;
        d.expander_id = id + 1;
        d.expander_bounds = (nx_rect){0, y, 20, 24};
    }
    nx_interaction_state state;
    return nx_tree_item(ctx, &d, expanded, selected, &state) == NX_OK;
}

static inline int item(nx_context *ctx, uint64_t id, uint64_t parent, float y,
    int *expanded, int disabled, uint64_t *selected) {
    return labeled_item(ctx, id, parent, y, expanded, disabled, selected, NULL);
}

static inline int tree_navigation_rows(nx_context *ctx, model *m, uint64_t parent) {
    nx_container_desc tree = {0};
    tree.id = 10;
    tree.parent_id = parent;
    tree.name = (nx_utf8_span){"Hierarchy", 9};
    tree.bounds = (nx_rect){0, 0, 120, 120};
#define LABEL(i) (m->labels ? m->labels[i] : NULL)
    if (nx_tree(ctx, &tree) != NX_OK ||
        !labeled_item(ctx, 11, 10, 0, &m->root_open, 0, &m->selected, LABEL(0))) return 0;
    if (m->root_open) {
        if (!labeled_item(ctx, 13, 11, 24, &m->child_open, m->child_disabled, &m->selected, LABEL(1))) return 0;
        if (m->child_open && !m->child_missing &&
            !labeled_item(ctx, 15, 13, 48, NULL, 0, &m->selected, LABEL(2))) return 0;
        if (!labeled_item(ctx, 16, 11, 72, NULL, 0, &m->selected, LABEL(3))) return 0;
    }
    if (!labeled_item(ctx, 17, 10, 96, NULL, 0, &m->selected, LABEL(4))) return 0;
    tree.id = 30;
    tree.bounds.y = 120;
    tree.bounds.height = 48;
    if (nx_tree(ctx, &tree) != NX_OK ||
        !labeled_item(ctx, 31, 30, 120, NULL, 0, &m->other_selected, LABEL(5)) ||
        !labeled_item(ctx, 32, 30, 144, NULL, 0, &m->other_selected, LABEL(6))) return 0;
#undef LABEL
    return 1;
}
#endif
