#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static nx_semantic_desc node(uint64_t id, uint64_t parent, nx_semantic_role role,
                             const char *name) {
    nx_semantic_desc d = {0};
    d.id = id;
    d.parent_id = parent;
    d.role = role;
    d.bounds = (nx_rect){0, 0, 100, 40};
    if (name) d.name = (nx_utf8_span){name, strlen(name)};
    return d;
}

static int test_tree_and_focus(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 200, 100, 0) == NX_OK);
    nx_semantic_desc root = node(1, 0, NX_ROLE_WINDOW, "Demo");
    nx_semantic_desc button = node(2, 1, NX_ROLE_BUTTON, "\xc3\x96" "ffnen");
    button.description = (nx_utf8_span){"Opens a file", 12};
    button.value = (nx_utf8_span){"Ready", 5};
    button.actions = NX_ACTION_PRESS | NX_ACTION_FOCUS;
    uint32_t root_index = UINT32_MAX, button_index = UINT32_MAX;
    CHECK(nx_semantics_add(ctx, &root, &root_index) == NX_OK && root_index == 0);
    CHECK(nx_semantics_add(ctx, &button, &button_index) == NX_OK && button_index == 1);
    nx_interaction_state interaction;
    CHECK(nx_interact_region(ctx, 2, button.bounds, NX_REGION_FOCUSABLE,
                              &interaction) == NX_OK);
    CHECK(nx_focus_set(ctx, 2) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count = 0;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 2);
    nx_semantic_view view;
    CHECK(nx_semantics_get(ctx, button_index, &view) == NX_OK);
    CHECK(view.id == 2 && view.parent_id == 1 && view.depth == 1);
    CHECK(view.role == NX_ROLE_BUTTON && (view.state & NX_SEM_FOCUSED));
    CHECK(view.actions == (NX_ACTION_PRESS | NX_ACTION_FOCUS));
    CHECK(view.name.length == button.name.length &&
          memcmp(view.name.data, button.name.data, button.name.length) == 0);
    CHECK(view.name.data[view.name.length] == 0);
    CHECK(view.description.length == 12 && view.value.length == 5);
    uint32_t found = UINT32_MAX;
    CHECK(nx_semantics_find(ctx, 2, &found) == NX_OK && found == button_index);
    CHECK(nx_begin_frame(ctx, 200, 100, 0) == NX_OK);
    CHECK(nx_semantics_count(ctx, &count) == NX_ERROR_STATE);
    CHECK(nx_semantics_add(ctx, &root, NULL) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 1);
    CHECK(nx_semantics_find(ctx, 2, &found) == NX_ERROR_ARGUMENT &&
          found == UINT32_MAX);
    nx_context_destroy(ctx);
    return 0;
}

static int test_validation_and_depth(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    nx_semantic_desc root = node(1, 0, NX_ROLE_PANEL, NULL);
    CHECK(nx_semantics_add(ctx, &root, NULL) == NX_OK);
    CHECK(nx_semantics_add(ctx, &root, NULL) == NX_ERROR_STATE);
    nx_semantic_desc bad = node(2, 1, NX_ROLE_BUTTON, NULL);
    CHECK(nx_semantics_add(ctx, &bad, NULL) == NX_ERROR_ARGUMENT);
    bad = node(2, 99, NX_ROLE_TEXT, "orphan");
    CHECK(nx_semantics_add(ctx, &bad, NULL) == NX_ERROR_ARGUMENT);
    const char invalid_utf8[] = "\xc0\x80";
    bad = node(2, 1, NX_ROLE_TEXT, NULL);
    bad.name = (nx_utf8_span){invalid_utf8, 2};
    CHECK(nx_semantics_add(ctx, &bad, NULL) == NX_ERROR_ARGUMENT);
    const char nul_name[] = {'A', 0, 'B'};
    bad.name = (nx_utf8_span){nul_name, 3};
    CHECK(nx_semantics_add(ctx, &bad, NULL) == NX_ERROR_ARGUMENT);
    bad = node(2, 1, NX_ROLE_TEXT, "valid");
    bad.state = NX_SEM_FOCUSED;
    CHECK(nx_semantics_add(ctx, &bad, NULL) == NX_ERROR_ARGUMENT);
    bad.state = NX_SEM_CHECKED | NX_SEM_MIXED;
    CHECK(nx_semantics_add(ctx, &bad, NULL) == NX_ERROR_ARGUMENT);
    uint64_t parent = 1;
    for (uint64_t depth = 1; depth < NX_SEMANTIC_MAX_DEPTH; ++depth) {
        nx_semantic_desc child = node(depth + 1, parent, NX_ROLE_PANEL, NULL);
        CHECK(nx_semantics_add(ctx, &child, NULL) == NX_OK);
        parent = child.id;
    }
    nx_semantic_desc too_deep = node(1000, parent, NX_ROLE_PANEL, NULL);
    CHECK(nx_semantics_add(ctx, &too_deep, NULL) == NX_ERROR_LIMIT);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == NX_SEMANTIC_MAX_DEPTH);
    nx_context_destroy(ctx);
    return 0;
}

typedef struct tracking { unsigned calls, frees, fail_after; } tracking;
static void *tracked_allocate(void *user, size_t size) {
    tracking *t = (tracking *)user;
    ++t->calls;
    if (t->fail_after && t->calls > t->fail_after) return NULL;
    return malloc(size);
}
static void tracked_deallocate(void *user, void *pointer, size_t size) {
    tracking *t = (tracking *)user;
    (void)size;
    ++t->frees;
    free(pointer);
}

static int test_allocation_and_reuse(void) {
    for (unsigned limit = 2; limit <= 4; ++limit) {
        tracking t = {0, 0, limit};
        nx_context_desc desc = {0};
        desc.allocator = (nx_allocator){&t, tracked_allocate, tracked_deallocate};
        nx_context *ctx = NULL;
        CHECK(nx_context_create(&desc, &ctx) == NX_OK);
        CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
        nx_semantic_desc d = node(1, 0, NX_ROLE_WINDOW, "Test");
        CHECK(nx_semantics_add(ctx, &d, NULL) == NX_ERROR_MEMORY);
        CHECK(nx_end_frame(ctx) == NX_OK);
        size_t count;
        CHECK(nx_semantics_count(ctx, &count) == NX_OK && !count);
        nx_context_destroy(ctx);
        CHECK(t.frees == limit);
    }
    tracking t = {0};
    nx_context_desc desc = {0};
    desc.allocator = (nx_allocator){&t, tracked_allocate, tracked_deallocate};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&desc, &ctx) == NX_OK);
    nx_semantic_desc d = node(1, 0, NX_ROLE_WINDOW, "Stable");
    for (unsigned frame = 0; frame < 100; ++frame) {
        CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
        CHECK(nx_semantics_add(ctx, &d, NULL) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        if (frame > 0) CHECK(t.calls == 5);
    }
    nx_context_destroy(ctx);
    CHECK(t.frees == 5);
    return 0;
}

static int test_many_nodes_and_string_ownership(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    char title[] = "Original";
    nx_semantic_desc root = node(1, 0, NX_ROLE_WINDOW, title);
    CHECK(nx_semantics_add(ctx, &root, NULL) == NX_OK);
    title[0] = 'X';
    for (uint64_t id = 2; id <= 1001; ++id) {
        nx_semantic_desc child = node(id, 1, NX_ROLE_CELL, NULL);
        CHECK(nx_semantics_add(ctx, &child, NULL) == NX_OK);
    }
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 1001);
    nx_semantic_view view;
    CHECK(nx_semantics_get(ctx, 0, &view) == NX_OK);
    CHECK(view.name.length == 8 && memcmp(view.name.data, "Original", 8) == 0);
    uint32_t index;
    CHECK(nx_semantics_find(ctx, 1001, &index) == NX_OK && index == 1000);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK);
    CHECK(view.parent_id == 1 && view.depth == 1 && view.role == NX_ROLE_CELL);
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_tree_and_focus()) return 1;
    if (test_validation_and_depth()) return 1;
    if (test_allocation_and_reuse()) return 1;
    if (test_many_nodes_and_string_ownership()) return 1;
    puts("semantics tests passed");
    return 0;
}
