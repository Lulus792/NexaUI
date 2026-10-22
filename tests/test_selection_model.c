#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static int selection(nx_selection_model *m, const uint64_t *expected, size_t count, uint64_t active) {
    nx_selection_view v;
    CHECK(nx_selection_model_get(m, &v) == NX_OK);
    CHECK(v.selected_count == count && v.active_id == active);
    CHECK(!count || memcmp(v.selected_ids, expected, count * sizeof(*expected)) == 0);
    return 0;
}

static int operations_and_active(void) {
    nx_selection_model *m = NULL;
    nx_selection_item items[] = {{30, 0}, {10, 0}, {20, 0}, {99, NX_SELECTION_ITEM_DISABLED}};
    CHECK(nx_selection_model_create(8, NULL, &m) == NX_OK);
    CHECK(nx_selection_model_set_items(m, items, 4) == NX_OK);
    CHECK(nx_selection_model_set_active(m, 20) == NX_OK);
    int changed = 9;
    nx_selection_change add[] = {{10, NX_ACTION_ADD_SELECTION}, {30, NX_ACTION_ADD_SELECTION}};
    CHECK(nx_selection_model_apply(m, add, 2, 0, &changed) == NX_OK && changed == 1);
    const uint64_t two[] = {30, 10};
    CHECK(selection(m, two, 2, 20) == 0);
    CHECK(nx_selection_model_apply(m, add, 2, 0, &changed) == NX_OK && changed == 0);
    nx_selection_change choose = {30, NX_ACTION_SELECT};
    CHECK(nx_selection_model_apply(m, &choose, 1, 0, &changed) == NX_OK && changed == 1);
    const uint64_t one[] = {30};
    CHECK(selection(m, one, 1, 20) == 0); /* Already selected in a larger set: exclusive. */
    CHECK(nx_selection_model_apply(m, &choose, 1, 1, &changed) == NX_OK && changed == 0);
    nx_selection_change remove = {30, NX_ACTION_DESELECT};
    CHECK(nx_selection_model_apply(m, &remove, 1, 1, &changed) == NX_ERROR_STATE && changed == 0);
    CHECK(selection(m, one, 1, 20) == 0);
    nx_selection_change replace[] = {{30, NX_ACTION_DESELECT}, {10, NX_ACTION_ADD_SELECTION}};
    CHECK(nx_selection_model_apply(m, replace, 2, 1, &changed) == NX_OK && changed == 1);
    const uint64_t ten[] = {10};
    CHECK(selection(m, ten, 1, 20) == 0);
    remove.id = 10;
    CHECK(nx_selection_model_apply(m, &remove, 1, 0, &changed) == NX_OK && changed == 1);
    CHECK(selection(m, NULL, 0, 20) == 0);
    CHECK(nx_selection_model_apply(m, &remove, 1, 0, &changed) == NX_OK && changed == 0);
    CHECK(nx_selection_model_set_active(m, 99) == NX_ERROR_STATE);
    CHECK(nx_selection_model_set_active(m, 0) == NX_OK);
    CHECK(selection(m, NULL, 0, 0) == 0);
    nx_selection_model_destroy(m);
    return 0;
}

static int catalog_and_virtual_membership(void) {
    nx_selection_model *m = NULL;
    nx_selection_item items[] = {{100, 0}, {200, 0}, {300, 0}, {400, 0}};
    CHECK(nx_selection_model_create(4, NULL, &m) == NX_OK);
    CHECK(nx_selection_model_set_items(m, items, 4) == NX_OK);
    const uint64_t initial[] = {400, 100};
    CHECK(nx_selection_model_set_selected(m, initial, 2) == NX_OK);
    CHECK(nx_selection_model_set_active(m, 200) == NX_OK);
    const uint64_t ordered[] = {100, 400};
    CHECK(selection(m, ordered, 2, 200) == 0);
    /* A paint range is deliberately absent from this model. Reorder the complete
       logical catalog while both selected IDs remain outside a hypothetical range. */
    nx_selection_item reordered[] = {{400, 0}, {300, 0}, {200, 0}, {100, 0}};
    CHECK(nx_selection_model_set_items(m, reordered, 4) == NX_OK);
    CHECK(selection(m, initial, 2, 200) == 0);
    nx_selection_view view;
    CHECK(nx_selection_model_get(m, &view) == NX_OK);
    CHECK(nx_selection_model_set_items(m, view.items, view.item_count) == NX_OK);
    CHECK(nx_selection_model_get(m, &view) == NX_OK);
    CHECK(nx_selection_model_set_selected(m, view.selected_ids, view.selected_count) == NX_OK);
    CHECK(selection(m, initial, 2, 200) == 0);
    nx_selection_item removed[] = {{100, 0}, {300, 0}};
    CHECK(nx_selection_model_set_items(m, removed, 2) == NX_OK);
    const uint64_t survivor[] = {100};
    CHECK(selection(m, survivor, 1, 0) == 0);
    CHECK(nx_selection_model_set_items(m, items, 4) == NX_OK);
    CHECK(selection(m, survivor, 1, 0) == 0); /* Reinserted400 is not selected again. */
    CHECK(nx_selection_model_set_active(m, 100) == NX_OK);
    items[0].flags = NX_SELECTION_ITEM_DISABLED;
    CHECK(nx_selection_model_set_items(m, items, 4) == NX_OK);
    CHECK(selection(m, survivor, 1, 0) == 0); /* Disabled selected member remains logical. */
    nx_selection_change action = {100, NX_ACTION_DESELECT};
    int changed = 3;
    CHECK(nx_selection_model_apply(m, &action, 1, 0, &changed) == NX_ERROR_STATE && changed == 0);
    CHECK(nx_selection_model_set_selected(m, survivor, 1) == NX_OK);
    CHECK(nx_selection_model_set_items(m, NULL, 0) == NX_OK);
    CHECK(selection(m, NULL, 0, 0) == 0);
    nx_selection_model_destroy(m);
    return 0;
}

static int invalid_atomic_batches(void) {
    nx_selection_model *m = NULL;
    nx_selection_item items[] = {{1, 0}, {2, 0}, {3, NX_SELECTION_ITEM_DISABLED}};
    CHECK(nx_selection_model_create(3, NULL, &m) == NX_OK);
    CHECK(nx_selection_model_set_items(m, items, 3) == NX_OK);
    const uint64_t initial[] = {1};
    CHECK(nx_selection_model_set_selected(m, initial, 1) == NX_OK);
    CHECK(nx_selection_model_set_active(m, 2) == NX_OK);
    nx_selection_change changes[] = {{2, NX_ACTION_SELECT}, {77, NX_ACTION_ADD_SELECTION}};
    int changed = 9;
    CHECK(nx_selection_model_apply(m, changes, 2, 0, &changed) == NX_ERROR_STATE && changed == 0);
    CHECK(selection(m, initial, 1, 2) == 0);
    changes[1] = (nx_selection_change){3, NX_ACTION_ADD_SELECTION};
    CHECK(nx_selection_model_apply(m, changes, 2, 0, &changed) == NX_ERROR_STATE && changed == 0);
    changes[1] = (nx_selection_change){2, NX_ACTION_PRESS};
    CHECK(nx_selection_model_apply(m, changes, 2, 0, &changed) == NX_ERROR_ARGUMENT && changed == 0);
    CHECK(selection(m, initial, 1, 2) == 0);
    const uint64_t duplicate[] = {2, 2}, missing[] = {2, 77}, zero[] = {0};
    CHECK(nx_selection_model_set_selected(m, duplicate, 2) == NX_ERROR_ARGUMENT);
    CHECK(nx_selection_model_set_selected(m, missing, 2) == NX_ERROR_STATE);
    CHECK(nx_selection_model_set_selected(m, zero, 1) == NX_ERROR_ARGUMENT);
    CHECK(selection(m, initial, 1, 2) == 0);
    nx_selection_item malformed[] = {{1, 0}, {1, 0}};
    CHECK(nx_selection_model_set_items(m, malformed, 2) == NX_ERROR_ARGUMENT);
    malformed[1] = (nx_selection_item){2, 2};
    CHECK(nx_selection_model_set_items(m, malformed, 2) == NX_ERROR_ARGUMENT);
    malformed[1] = (nx_selection_item){0, 0};
    CHECK(nx_selection_model_set_items(m, malformed, 2) == NX_ERROR_ARGUMENT);
    CHECK(selection(m, initial, 1, 2) == 0);
    CHECK(nx_selection_model_set_items(m, (const nx_selection_item *)(uintptr_t)1, SIZE_MAX) == NX_ERROR_LIMIT);
    CHECK(nx_selection_model_apply(m, (const nx_selection_change *)(uintptr_t)1, 65, 0, &changed) == NX_ERROR_LIMIT && changed == 0);
    CHECK(nx_selection_model_apply(m, NULL, 1, 0, &changed) == NX_ERROR_ARGUMENT && changed == 0);
    CHECK(nx_selection_model_apply(m, NULL, 0, 2, &changed) == NX_ERROR_ARGUMENT && changed == 0);
    CHECK(nx_selection_model_apply(m, NULL, 0, 1, &changed) == NX_OK && changed == 0);
    CHECK(selection(m, initial, 1, 2) == 0);
    nx_selection_model_destroy(m);
    return 0;
}

typedef struct tracker {
    size_t calls, live, fail_at, allocate_checks, release_checks;
    nx_selection_model *model;
    int arm_allocate, arm_release, callback_failed;
} tracker;

static void check_reentry(tracker *t) {
    nx_selection_view v = {0};
    int changed = 1;
    nx_selection_change change = {1, NX_ACTION_SELECT};
    if (nx_selection_model_get(t->model, &v) != NX_ERROR_STATE ||
        v.items || v.item_count || v.selected_ids || v.selected_count || v.active_id ||
        nx_selection_model_set_items(t->model, NULL, 0) != NX_ERROR_STATE ||
        nx_selection_model_set_selected(t->model, NULL, 0) != NX_ERROR_STATE ||
        nx_selection_model_apply(t->model, &change, 1, 0, &changed) != NX_ERROR_STATE || changed ||
        nx_selection_model_set_active(t->model, 0) != NX_ERROR_STATE) t->callback_failed = 1;
}

static void *tracked_allocate(void *user, size_t bytes) {
    tracker *t = user;
    ++t->calls;
    if (t->model && t->arm_allocate) { ++t->allocate_checks; check_reentry(t); }
    if (t->calls == t->fail_at) return NULL;
    void *p = malloc(bytes);
    if (p) ++t->live;
    return p;
}

static void tracked_release(void *user, void *pointer, size_t bytes) {
    tracker *t = user;
    (void)bytes;
    if (t->model && t->arm_release) { ++t->release_checks; check_reentry(t); }
    --t->live;
    free(pointer);
}

static int allocation_and_reentry(void) {
    tracker t = {0};
    nx_allocator a = {&t, tracked_allocate, tracked_release};
    nx_selection_model *m = (nx_selection_model *)(uintptr_t)1;
    t.fail_at = 1;
    CHECK(nx_selection_model_create(8, &a, &m) == NX_ERROR_MEMORY && !m && t.live == 0);
    t.fail_at = 0;
    CHECK(nx_selection_model_create(8, &a, &m) == NX_OK);
    t.model = m;
    nx_selection_item old[] = {{1, 0}, {2, 0}}, next[] = {{3, 0}, {2, 0}, {1, 0}};
    CHECK(nx_selection_model_set_items(m, old, 2) == NX_OK);
    const uint64_t initial[] = {1};
    CHECK(nx_selection_model_set_selected(m, initial, 1) == NX_OK);
    CHECK(nx_selection_model_set_active(m, 2) == NX_OK);
    t.arm_allocate = t.arm_release = 1;
    t.fail_at = t.calls + 1u;
    CHECK(nx_selection_model_set_items(m, next, 3) == NX_ERROR_MEMORY);
    CHECK(selection(m, initial, 1, 2) == 0 && t.live == 2 && !t.callback_failed);
    t.fail_at = 0;
    CHECK(nx_selection_model_set_items(m, next, 3) == NX_OK);
    CHECK(selection(m, initial, 1, 2) == 0 && t.live == 2);
    CHECK(t.allocate_checks == 2 && t.release_checks == 1 && !t.callback_failed);
    size_t calls = t.calls;
    t.fail_at = calls + 1u;
    nx_selection_change add = {2, NX_ACTION_ADD_SELECTION};
    int changed;
    CHECK(nx_selection_model_apply(m, &add, 1, 0, &changed) == NX_OK && changed == 1 && t.calls == calls);
    const uint64_t both[] = {2, 1};
    CHECK(selection(m, both, 2, 2) == 0);
    t.arm_allocate = t.arm_release = 0;
    nx_selection_model_destroy(m);
    CHECK(t.live == 0);
    return 0;
}

static int maximum_and_validation(void) {
    nx_selection_model *m = NULL;
    CHECK(nx_selection_model_create(0, NULL, &m) == NX_ERROR_ARGUMENT && !m);
    CHECK(nx_selection_model_create((size_t)NX_SELECTION_MAX_ITEMS + 1u, NULL, &m) == NX_ERROR_LIMIT && !m);
    nx_allocator partial = {NULL, tracked_allocate, NULL};
    CHECK(nx_selection_model_create(1, &partial, &m) == NX_ERROR_ARGUMENT && !m);
    CHECK(nx_selection_model_create(NX_SELECTION_MAX_ITEMS, NULL, &m) == NX_OK);
    size_t count = NX_SELECTION_MAX_ITEMS;
    nx_selection_item *items = malloc(count * sizeof(*items));
    uint64_t *ids = malloc(count * sizeof(*ids));
    CHECK(items && ids);
    for (size_t i = 0; i < count; ++i) { items[i] = (nx_selection_item){(uint64_t)i + 1u, 0}; ids[i] = count - i; }
    CHECK(nx_selection_model_set_items(m, items, count) == NX_OK);
    CHECK(nx_selection_model_set_selected(m, ids, count) == NX_OK);
    nx_selection_view v;
    CHECK(nx_selection_model_get(m, &v) == NX_OK && v.selected_count == count);
    for (size_t i = 0; i < count; ++i) CHECK(v.selected_ids[i] == i + 1u);
    nx_selection_change remove[NX_SELECTION_MAX_CHANGES];
    for (size_t i = 0; i < NX_SELECTION_MAX_CHANGES; ++i) remove[i] = (nx_selection_change){(uint64_t)i + 1u, NX_ACTION_DESELECT};
    int changed;
    CHECK(nx_selection_model_apply(m, remove, NX_SELECTION_MAX_CHANGES, 1, &changed) == NX_OK && changed == 1);
    CHECK(nx_selection_model_get(m, &v) == NX_OK && v.selected_count == count - NX_SELECTION_MAX_CHANGES && v.selected_ids[0] == 65);
    free(ids);
    free(items);
    nx_selection_model_destroy(m);
    nx_selection_model_destroy(NULL);
    return 0;
}

int main(void) {
    if (operations_and_active() || catalog_and_virtual_membership() || invalid_atomic_batches() ||
        allocation_and_reentry() || maximum_and_validation()) return 1;
    puts("multi-selection catalog/set/active, atomic batches, bounds, OOM and allocator reentry passed");
    return 0;
}
