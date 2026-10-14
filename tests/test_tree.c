#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static void *fail_allocate(void *user, size_t size) {
    size_t *fail_size = (size_t *)user;
    if (*fail_size == size) {
        *fail_size = 0;
        return NULL;
    }
    return malloc(size);
}

static void fail_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static int test_flatten(void) {
    nx_tree_node nodes[] = {
        {SIZE_MAX, 1}, {0, 0}, {1, 0}, {0, 0},
        {SIZE_MAX, 0}, {4, 0}
    };
    size_t indices[6] = {99, 99, 99, 99, 99, 99};
    size_t count = 0;
    CHECK(nx_tree_flatten(nodes, 6, NULL, NULL, 0, &count) == NX_OK &&
          count == 4);
    CHECK(nx_tree_flatten(nodes, 6, NULL, indices, 3, &count) ==
          NX_ERROR_LIMIT && count == 4 && indices[0] == 99);
    CHECK(nx_tree_flatten(nodes, 6, NULL, indices, 6, &count) == NX_OK &&
          count == 4);
    size_t expected[] = {0, 1, 3, 4};
    CHECK(memcmp(indices, expected, sizeof(expected)) == 0);
    nodes[1].expanded = 1;
    CHECK(nx_tree_flatten(nodes, 6, NULL, indices, 6, &count) == NX_OK &&
          count == 5 && indices[2] == 2);
    nodes[5].parent_index = 0; /* Prior root is no longer an ancestor. */
    indices[0] = 99;
    CHECK(nx_tree_flatten(nodes, 6, NULL, indices, 6, &count) ==
          NX_ERROR_ARGUMENT && indices[0] == 99);
    nodes[5].parent_index = 4;
    size_t fail_size = 6u * 2u * sizeof(size_t);
    nx_allocator allocator = {&fail_size, fail_allocate, fail_deallocate};
    CHECK(nx_tree_flatten(nodes, 6, &allocator, indices, 6, &count) ==
          NX_ERROR_MEMORY && indices[0] == 99);
    return 0;
}

static nx_container_desc root_desc(void) {
    nx_container_desc desc = {0};
    desc.id = 10;
    desc.bounds = (nx_rect){0, 0, 120, 90};
    desc.name = (nx_utf8_span){"Files", 5};
    return desc;
}

static nx_tree_item_desc branch_desc(void) {
    nx_tree_item_desc desc = {0};
    desc.control.id = 11;
    desc.control.parent_id = 10;
    desc.control.bounds = (nx_rect){0, 0, 120, 24};
    desc.control.name = (nx_utf8_span){"Folder", 6};
    desc.expander_id = 12;
    desc.expander_bounds = (nx_rect){0, 0, 20, 24};
    desc.has_children = 1;
    return desc;
}

static nx_tree_item_desc leaf_desc(uint64_t id, uint64_t parent,
                                   float x, float y) {
    nx_tree_item_desc desc = {0};
    desc.control.id = id;
    desc.control.parent_id = parent;
    desc.control.bounds = (nx_rect){x, y, 120 - x, 24};
    desc.control.name = (nx_utf8_span){"File", 4};
    return desc;
}

static int click(nx_context *ctx, float x, float y) {
    nx_event event = {0};
    event.button = NX_BUTTON_LEFT;
    event.x = x;
    event.y = y;
    event.type = NX_EVENT_POINTER_DOWN;
    if (nx_context_queue_event(ctx, &event) != NX_OK) return 0;
    event.type = NX_EVENT_POINTER_UP;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int semantic(nx_context *ctx, uint64_t id, uint64_t parent,
                    nx_semantic_role role, uint32_t state_mask,
                    int expected_state) {
    uint32_t index;
    nx_semantic_view view;
    return nx_semantics_find(ctx, id, &index) == NX_OK &&
           nx_semantics_get(ctx, index, &view) == NX_OK &&
           view.parent_id == parent && view.role == role &&
           !!(view.state & state_mask) == !!expected_state;
}

static int test_interaction(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_container_desc root = root_desc();
    nx_tree_item_desc branch = branch_desc();
    nx_tree_item_desc child = leaf_desc(13, 11, 20, 24);
    nx_tree_item_desc sibling = leaf_desc(14, 10, 0, 48);
    int expanded = 0;
    uint64_t selected = 0;
    for (unsigned frame = 0; frame < 6; ++frame) {
        if (frame == 1) CHECK(click(ctx, 10, 10));
        if (frame == 2) CHECK(click(ctx, 40, 35));
        if (frame == 3) CHECK(click(ctx, 40, 60));
        if (frame == 4 || frame == 5) {
            CHECK(nx_focus_set(ctx, 11) == NX_OK);
            nx_event key = {0};
            key.type = NX_EVENT_KEY_DOWN;
            key.key = frame == 4 ? NX_KEY_LEFT : NX_KEY_RIGHT;
            CHECK(nx_context_queue_event(ctx, &key) == NX_OK);
        }
        CHECK(nx_begin_frame(ctx, 120, 90, 0) == NX_OK);
        CHECK(nx_tree(ctx, &root) == NX_OK);
        nx_interaction_state state;
        CHECK(nx_tree_item(ctx, &branch, &expanded, &selected,
                           &state) == NX_OK);
        if (expanded)
            CHECK(nx_tree_item(ctx, &child, NULL, &selected,
                               &state) == NX_OK);
        CHECK(nx_tree_item(ctx, &sibling, NULL, &selected,
                           &state) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        CHECK(expanded == (frame >= 1 && frame != 4));
        CHECK(selected == (frame < 2 ? 0u : frame == 2 ? 13u : 14u));
        CHECK(semantic(ctx, 10, 0, NX_ROLE_TREE, 0, 0));
        CHECK(semantic(ctx, 11, 10, NX_ROLE_TREE_ITEM,
                       NX_SEM_EXPANDED, expanded));
        CHECK(semantic(ctx, 12, 11, NX_ROLE_BUTTON, 0, 0));
        CHECK(semantic(ctx, 14, 10, NX_ROLE_TREE_ITEM,
                       NX_SEM_SELECTED, selected == 14));
        if (expanded)
            CHECK(semantic(ctx, 13, 11, NX_ROLE_TREE_ITEM,
                           NX_SEM_SELECTED, selected == 13));
        else {
            uint32_t index;
            CHECK(nx_semantics_find(ctx, 13, &index) == NX_ERROR_ARGUMENT);
        }
        if (frame == 3) {
            const nx_command *commands;
            size_t count;
            uint8_t pixels[120u * 90u * 4u];
            CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK);
            CHECK(nx_headless_render(commands, count, pixels, 120, 90,
                120u * 4u, (nx_color){0, 0, 0, 0}) == NX_OK);
            CHECK(pixels[(60u * 120u + 40u) * 4u + 3u] != 0);
        }
    }
    nx_context_destroy(ctx);
    return 0;
}

static int test_tree_table(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_table_column_desc column = {0};
    column.id = 31;
    column.name = (nx_utf8_span){"Name", 4};
    column.width = 120;
    column.minimum_width = 50;
    column.maximum_width = 120;
    nx_table_desc table = {0};
    table.id = 30;
    table.bounds = (nx_rect){0, 0, 120, 80};
    table.name = (nx_utf8_span){"Hierarchy", 9};
    table.header_height = 24;
    table.columns = &column;
    table.column_count = 1;
    nx_table_sort sort = {0};
    nx_container_desc row = {0};
    row.id = 32;
    row.parent_id = 30;
    row.bounds = (nx_rect){0, 24, 120, 24};
    row.name = (nx_utf8_span){"Row", 3};
    nx_container_desc cell = row;
    cell.id = 33;
    cell.parent_id = 32;
    nx_tree_item_desc item = leaf_desc(34, 33, 0, 24);
    uint64_t selected = 0;
    CHECK(nx_begin_frame(ctx, 120, 80, 0) == NX_OK);
    CHECK(nx_table_headers(ctx, &table, &sort) == NX_OK);
    CHECK(nx_table_row(ctx, &row) == NX_OK);
    CHECK(nx_table_cell(ctx, &cell) == NX_OK);
    nx_interaction_state state;
    CHECK(nx_tree_item(ctx, &item, NULL, &selected, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(semantic(ctx, 34, 33, NX_ROLE_TREE_ITEM, 0, 0));
    nx_context_destroy(ctx);
    return 0;
}

static int test_retry(void) {
    size_t fail_size = 4u * sizeof(nx_command);
    nx_context_desc config = {0};
    config.initial_command_capacity = 2;
    config.allocator = (nx_allocator){&fail_size, fail_allocate,
                                       fail_deallocate};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&config, &ctx) == NX_OK);
    nx_container_desc root = root_desc();
    nx_tree_item_desc branch = branch_desc();
    uint64_t selected = 0;
    int expanded = 0;
    nx_interaction_state state;
    CHECK(nx_begin_frame(ctx, 120, 90, 0) == NX_OK);
    CHECK(nx_tree(ctx, &root) == NX_OK);
    CHECK(nx_tree_item(ctx, &branch, &expanded, &selected,
                       &state) == NX_ERROR_MEMORY);
    CHECK(!expanded && !selected);
    CHECK(nx_tree_item(ctx, &branch, &expanded, &selected,
                       &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 3);
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_flatten() || test_interaction() || test_tree_table() ||
        test_retry()) return 1;
    puts("tree tests passed");
    return 0;
}
