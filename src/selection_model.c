#include "selection_model.h"
#include "context_identity.h"

#include <stdlib.h>
#include <string.h>

typedef struct nx_selection_index {
    uint64_t id;
    size_t ordinal;
} nx_selection_index;

typedef struct nx_selection_storage {
    void *allocation;
    size_t bytes, count, selected_count;
    nx_selection_item *items;
    nx_selection_index *index;
    uint64_t *selected_ids;
    uint64_t *memberships;
    unsigned char *selected, *scratch;
} nx_selection_storage;

struct nx_selection_model {
    nx_allocator allocator;
    nx_selection_storage storage;
    nx_selection_target target;
    size_t limit;
    uint64_t active_id;
    uint64_t membership_sequence;
    int busy;
};

static void *nx_selection_allocate(void *user, size_t bytes) {
    (void)user;
    return malloc(bytes);
}

static void nx_selection_deallocate(void *user, void *pointer, size_t bytes) {
    (void)user;
    (void)bytes;
    free(pointer);
}

static int nx_selection_segment(size_t *bytes, size_t count, size_t element,
                                  size_t alignment, size_t *offset) {
    size_t padding = (alignment - *bytes % alignment) % alignment;
    if (padding > SIZE_MAX - *bytes) return 0;
    size_t start = *bytes + padding;
    if (count > (SIZE_MAX - start) / element) return 0;
    *offset = start;
    *bytes = start + count * element;
    return 1;
}

static int nx_selection_compare(const void *left, const void *right) {
    const nx_selection_index *a = left, *b = right;
    return (a->id > b->id) - (a->id < b->id);
}

static size_t nx_selection_find(const nx_selection_storage *s, uint64_t id) {
    size_t lo = 0, hi = s->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2u;
        if (s->index[mid].id < id) lo = mid + 1u;
        else hi = mid;
    }
    return lo < s->count && s->index[lo].id == id ?
        s->index[lo].ordinal : SIZE_MAX;
}

static void nx_selection_release(nx_allocator a, nx_selection_storage *s) {
    if (s->allocation) a.deallocate(a.user, s->allocation, s->bytes);
    *s = (nx_selection_storage){0};
}

static nx_result nx_selection_storage_create(nx_allocator a,
    const nx_selection_item *items, size_t count, nx_selection_storage *s) {
    *s = (nx_selection_storage){0};
    if (!count) return NX_OK;
    size_t bytes = 0, item_offset, index_offset, ids_offset, membership_offset, selected_offset, scratch_offset;
    if (!nx_selection_segment(&bytes, count, sizeof(nx_selection_item),
                               _Alignof(nx_selection_item), &item_offset) ||
        !nx_selection_segment(&bytes, count, sizeof(nx_selection_index),
                               _Alignof(nx_selection_index), &index_offset) ||
        !nx_selection_segment(&bytes, count, sizeof(uint64_t),
                               _Alignof(uint64_t), &ids_offset) ||
        !nx_selection_segment(&bytes, count, sizeof(uint64_t),
                               _Alignof(uint64_t), &membership_offset) ||
        !nx_selection_segment(&bytes, count, 1u, 1u, &selected_offset) ||
        !nx_selection_segment(&bytes, count, 1u, 1u, &scratch_offset)) return NX_ERROR_LIMIT;
    unsigned char *allocation = a.allocate(a.user, bytes);
    if (!allocation) return NX_ERROR_MEMORY;
    memset(allocation, 0, bytes);
    s->allocation = allocation;
    s->bytes = bytes;
    s->count = count;
    s->items = (nx_selection_item *)(allocation + item_offset);
    s->index = (nx_selection_index *)(allocation + index_offset);
    s->selected_ids = (uint64_t *)(allocation + ids_offset);
    s->memberships = (uint64_t *)(allocation + membership_offset);
    s->selected = allocation + selected_offset;
    s->scratch = allocation + scratch_offset;
    for (size_t i = 0; i < count; ++i) {
        if (!items[i].id || (items[i].flags & ~NX_SELECTION_ITEM_DISABLED)) return NX_ERROR_ARGUMENT;
        s->items[i].id = items[i].id;
        s->items[i].flags = items[i].flags;
        s->index[i] = (nx_selection_index){items[i].id, i};
    }
    if (count > 1u) qsort(s->index, count, sizeof(*s->index), nx_selection_compare);
    for (size_t i = 1; i < count; ++i)
        if (s->index[i - 1u].id == s->index[i].id) return NX_ERROR_ARGUMENT;
    return NX_OK;
}

static void nx_selection_rebuild(nx_selection_storage *s) {
    s->selected_count = 0;
    for (size_t i = 0; i < s->count; ++i)
        if (s->selected[i]) s->selected_ids[s->selected_count++] = s->items[i].id;
}

nx_result nx_selection_model_create(size_t max_items, const nx_allocator *allocator,
                                      nx_selection_model **out_model) {
    if (!out_model) return NX_ERROR_ARGUMENT;
    *out_model = NULL;
    if (!max_items) return NX_ERROR_ARGUMENT;
    if (max_items > NX_SELECTION_MAX_ITEMS) return NX_ERROR_LIMIT;
    nx_allocator a = allocator ? *allocator : (nx_allocator){0};
    if ((a.allocate == NULL) != (a.deallocate == NULL)) return NX_ERROR_ARGUMENT;
    if (!a.allocate) a = (nx_allocator){NULL, nx_selection_allocate, nx_selection_deallocate};
    nx_selection_model *m = a.allocate(a.user, sizeof(*m));
    if (!m) return NX_ERROR_MEMORY;
    *m = (nx_selection_model){0};
    m->allocator = a;
    m->limit = max_items;
    m->target.revision = 1;
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
    m->target.identity = nx_native_next_identity();
    if (!m->target.identity) {
        a.deallocate(a.user, m, sizeof(*m));
        return NX_ERROR_LIMIT;
    }
#endif
    *out_model = m;
    return NX_OK;
}

void nx_selection_model_destroy(nx_selection_model *model) {
    if (!model || model->busy) return;
    model->busy = 1;
    nx_allocator a = model->allocator;
    nx_selection_release(a, &model->storage);
    a.deallocate(a.user, model, sizeof(*model));
}

nx_result nx_selection_model_set_items(nx_selection_model *model,
    const nx_selection_item *items, size_t count) {
    if (!model || (!items && count)) return NX_ERROR_ARGUMENT;
    if (model->busy) return NX_ERROR_STATE;
    if (count > model->limit || model->target.revision == UINT64_MAX) return NX_ERROR_LIMIT;
    model->busy = 1;
    nx_selection_storage next;
    nx_result result = nx_selection_storage_create(model->allocator, items, count, &next);
    if (result != NX_OK) {
        nx_selection_release(model->allocator, &next);
        model->busy = 0;
        return result;
    }
    uint64_t sequence = model->membership_sequence;
    for (size_t i = 0; i < next.count; ++i) {
        size_t old = nx_selection_find(&model->storage, next.items[i].id);
        next.selected[i] = old != SIZE_MAX && model->storage.selected[old];
        if (old != SIZE_MAX) next.memberships[i] = model->storage.memberships[old];
        else {
            if (sequence == UINT64_MAX) {
                nx_selection_release(model->allocator, &next);
                model->busy = 0;
                return NX_ERROR_LIMIT;
            }
            next.memberships[i] = ++sequence;
        }
    }
    nx_selection_rebuild(&next);
    size_t active = nx_selection_find(&next, model->active_id);
    uint64_t active_id = active != SIZE_MAX && !(next.items[active].flags & NX_SELECTION_ITEM_DISABLED)
        ? model->active_id : 0;
    nx_selection_storage old = model->storage;
    model->storage = next;
    model->active_id = active_id;
    model->membership_sequence = sequence;
    ++model->target.revision;
    nx_selection_release(model->allocator, &old);
    model->busy = 0;
    return NX_OK;
}

nx_result nx_selection_model_set_selected(nx_selection_model *model,
    const uint64_t *ids, size_t count) {
    if (!model || (!ids && count)) return NX_ERROR_ARGUMENT;
    if (model->busy) return NX_ERROR_STATE;
    nx_selection_storage *s = &model->storage;
    if (count > s->count || model->target.revision == UINT64_MAX) return NX_ERROR_LIMIT;
    if (s->count) memset(s->scratch, 0, s->count);
    for (size_t i = 0; i < count; ++i) {
        if (!ids[i]) return NX_ERROR_ARGUMENT;
        size_t at = nx_selection_find(s, ids[i]);
        if (at == SIZE_MAX) return NX_ERROR_STATE;
        if (s->scratch[at]) return NX_ERROR_ARGUMENT;
        s->scratch[at] = 1;
    }
    if (s->count) memcpy(s->selected, s->scratch, s->count);
    nx_selection_rebuild(s);
    ++model->target.revision;
    return NX_OK;
}

nx_result nx_selection_model_prepare(nx_selection_model *model,
    const nx_selection_change *changes, size_t count, int required, int *changed,
    size_t *out_count) {
    if (out_count) *out_count = 0;
    if (!changed) return NX_ERROR_ARGUMENT;
    *changed = 0;
    if (!model || (!changes && count) || (required != 0 && required != 1)) return NX_ERROR_ARGUMENT;
    if (model->busy) return NX_ERROR_STATE;
    if (count > NX_SELECTION_MAX_CHANGES) return NX_ERROR_LIMIT;
    nx_selection_storage *s = &model->storage;
    if (s->count) memcpy(s->scratch, s->selected, s->count);
    size_t selected_count = s->selected_count;
    for (size_t i = 0; i < count; ++i) {
        const nx_selection_change *c = &changes[i];
        if (!c->id || (c->action != NX_ACTION_SELECT &&
            c->action != NX_ACTION_ADD_SELECTION && c->action != NX_ACTION_DESELECT)) return NX_ERROR_ARGUMENT;
        size_t at = nx_selection_find(s, c->id);
        if (at == SIZE_MAX || (s->items[at].flags & NX_SELECTION_ITEM_DISABLED)) return NX_ERROR_STATE;
        if (c->action == NX_ACTION_SELECT) {
            memset(s->scratch, 0, s->count);
            s->scratch[at] = 1;
            selected_count = 1;
        } else if (c->action == NX_ACTION_ADD_SELECTION && !s->scratch[at]) {
            s->scratch[at] = 1;
            ++selected_count;
        } else if (c->action == NX_ACTION_DESELECT && s->scratch[at]) {
            s->scratch[at] = 0;
            --selected_count;
        }
    }
    if (required && !selected_count) return NX_ERROR_STATE;
    int different = s->count && memcmp(s->selected, s->scratch, s->count) != 0;
    if (out_count) *out_count = selected_count;
    if (!different) return NX_OK;
    if (model->target.revision == UINT64_MAX) return NX_ERROR_LIMIT;
    *changed = 1;
    return NX_OK;
}

nx_result nx_selection_model_prepare_set(nx_selection_model *model,
    const uint64_t *ids, size_t count, int required, int *changed,
    size_t *out_count) {
    if (out_count) *out_count = 0;
    if (!changed) return NX_ERROR_ARGUMENT;
    *changed = 0;
    if (!model || (!ids && count) || (required != 0 && required != 1)) return NX_ERROR_ARGUMENT;
    if (model->busy) return NX_ERROR_STATE;
    nx_selection_storage *s = &model->storage;
    if (count > s->count) return NX_ERROR_LIMIT;
    if (s->count) memset(s->scratch, 0, s->count);
    for (size_t i = 0; i < count; ++i) {
        if (!ids[i]) return NX_ERROR_ARGUMENT;
        size_t at = nx_selection_find(s, ids[i]);
        if (at == SIZE_MAX) return NX_ERROR_STATE;
        if (s->scratch[at]) return NX_ERROR_ARGUMENT;
        s->scratch[at] = 1;
    }
    if (required && !count) return NX_ERROR_STATE;
    for (size_t i = 0; i < s->count; ++i)
        if ((s->items[i].flags & NX_SELECTION_ITEM_DISABLED) &&
            s->scratch[i] != s->selected[i]) return NX_ERROR_STATE;
    int different = s->count && memcmp(s->selected, s->scratch, s->count) != 0;
    if (out_count) *out_count = count;
    if (!different) return NX_OK;
    if (model->target.revision == UINT64_MAX) return NX_ERROR_LIMIT;
    *changed = 1;
    return NX_OK;
}

void nx_selection_model_commit(nx_selection_model *model, int changed) {
    if (!changed) return;
    nx_selection_storage *s = &model->storage;
    memcpy(s->selected, s->scratch, s->count);
    nx_selection_rebuild(s);
    ++model->target.revision;
}

nx_result nx_selection_model_apply(nx_selection_model *model,
    const nx_selection_change *changes, size_t count, int required, int *changed) {
    nx_result result = nx_selection_model_prepare(model, changes, count, required, changed, NULL);
    if (result == NX_OK) nx_selection_model_commit(model, *changed);
    return result;
}

nx_result nx_selection_model_item(const nx_selection_model *model, uint64_t id,
    int staged, int *selected, int *disabled) {
    if (!model || !selected || !disabled) return NX_ERROR_ARGUMENT;
    if (model->busy) return NX_ERROR_STATE;
    size_t at = nx_selection_find(&model->storage, id);
    if (at == SIZE_MAX) return NX_ERROR_STATE;
    *selected = (staged ? model->storage.scratch : model->storage.selected)[at] != 0;
    *disabled = (model->storage.items[at].flags & NX_SELECTION_ITEM_DISABLED) != 0;
    return NX_OK;
}

nx_result nx_selection_model_set_active(nx_selection_model *model, uint64_t id) {
    if (!model) return NX_ERROR_ARGUMENT;
    if (model->busy) return NX_ERROR_STATE;
    if (id) {
        size_t at = nx_selection_find(&model->storage, id);
        if (at == SIZE_MAX || (model->storage.items[at].flags & NX_SELECTION_ITEM_DISABLED)) return NX_ERROR_STATE;
    }
    model->active_id = id;
    return NX_OK;
}

nx_result nx_selection_model_get(const nx_selection_model *model, nx_selection_view *out_view) {
    if (!out_view) return NX_ERROR_ARGUMENT;
    *out_view = (nx_selection_view){0};
    if (!model) return NX_ERROR_ARGUMENT;
    if (model->busy) return NX_ERROR_STATE;
    const nx_selection_storage *s = &model->storage;
    *out_view = (nx_selection_view){s->items, s->count, s->selected_ids, s->selected_count, model->active_id};
    return NX_OK;
}

nx_selection_target nx_selection_model_target(const nx_selection_model *model) {
    return model && !model->busy ? model->target : (nx_selection_target){0};
}

uint64_t nx_selection_model_membership(const nx_selection_model *model, uint64_t id) {
    if (!model || model->busy) return 0;
    size_t at = nx_selection_find(&model->storage, id);
    return at != SIZE_MAX ? model->storage.memberships[at] : 0;
}
