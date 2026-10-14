#include "nexaui/nexaui.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct nx_virtual_list {
    nx_allocator allocator;
    size_t count;
    uint64_t *keys;
    double *heights;
    double *tree; /* One-based Fenwick partial sums. */
    double total;
};

static void *nx_vlist_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_vlist_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static size_t nx_vlist_lowbit(size_t value) {
    return value & (~value + 1u);
}

nx_result nx_virtual_list_create(size_t count, double default_height,
                                  const nx_allocator *allocator,
                                  nx_virtual_list **out_list) {
    if (!out_list) return NX_ERROR_ARGUMENT;
    *out_list = NULL;
    if (!isfinite(default_height) || default_height <= 0.0)
        return NX_ERROR_ARGUMENT;
    if (count > (SIZE_MAX / sizeof(double)) - 1u ||
        count > SIZE_MAX / sizeof(uint64_t) ||
        !isfinite((double)count * default_height)) return NX_ERROR_LIMIT;
    nx_allocator a = allocator ? *allocator : (nx_allocator){0};
    if ((a.allocate == NULL) != (a.deallocate == NULL))
        return NX_ERROR_ARGUMENT;
    if (!a.allocate) {
        a.allocate = nx_vlist_allocate;
        a.deallocate = nx_vlist_deallocate;
        a.user = NULL;
    }
    nx_virtual_list *list = a.allocate(a.user, sizeof(*list));
    if (!list) return NX_ERROR_MEMORY;
    *list = (nx_virtual_list){0};
    list->allocator = a;
    list->count = count;
    list->total = (double)count * default_height;
    if (count) {
        list->keys = a.allocate(a.user, count * sizeof(uint64_t));
        list->heights = a.allocate(a.user, count * sizeof(double));
        list->tree = a.allocate(a.user, (count + 1u) * sizeof(double));
        if (!list->keys || !list->heights || !list->tree) {
            nx_virtual_list_destroy(list);
            return NX_ERROR_MEMORY;
        }
        list->tree[0] = 0.0;
        for (size_t i = 1; i <= count; ++i) {
            list->keys[i - 1u] = (uint64_t)i;
            list->heights[i - 1u] = default_height;
            list->tree[i] = (double)nx_vlist_lowbit(i) * default_height;
        }
    }
    *out_list = list;
    return NX_OK;
}

void nx_virtual_list_destroy(nx_virtual_list *list) {
    if (!list) return;
    nx_allocator a = list->allocator;
    if (list->tree)
        a.deallocate(a.user, list->tree,
                     (list->count + 1u) * sizeof(double));
    if (list->heights)
        a.deallocate(a.user, list->heights,
                     list->count * sizeof(double));
    if (list->keys)
        a.deallocate(a.user, list->keys,
                     list->count * sizeof(uint64_t));
    a.deallocate(a.user, list, sizeof(*list));
}

nx_result nx_virtual_list_set_height(nx_virtual_list *list,
                                      size_t index, double height) {
    if (!list || index >= list->count || !isfinite(height) || height <= 0.0)
        return NX_ERROR_ARGUMENT;
    double delta = height - list->heights[index];
    double total = list->total + delta;
    if (!isfinite(delta) || !isfinite(total) || total <= 0.0)
        return NX_ERROR_LIMIT;
    for (size_t i = index + 1u; i <= list->count;) {
        double next = list->tree[i] + delta;
        if (!isfinite(next) || next <= 0.0) return NX_ERROR_LIMIT;
        size_t low = nx_vlist_lowbit(i);
        if (low > list->count - i) break;
        i += low;
    }
    list->heights[index] = height;
    list->total = total;
    for (size_t i = index + 1u; i <= list->count;) {
        list->tree[i] += delta;
        size_t low = nx_vlist_lowbit(i);
        if (low > list->count - i) break;
        i += low;
    }
    return NX_OK;
}

static double nx_vlist_prefix(const nx_virtual_list *list, size_t index) {
    double sum = 0.0;
    while (index) {
        sum += list->tree[index];
        index -= nx_vlist_lowbit(index);
    }
    return sum;
}

nx_result nx_virtual_list_offset_at(const nx_virtual_list *list,
                                     size_t index, double *out_offset) {
    if (!out_offset) return NX_ERROR_ARGUMENT;
    *out_offset = 0.0;
    if (!list || index > list->count) return NX_ERROR_ARGUMENT;
    *out_offset = nx_vlist_prefix(list, index);
    return NX_OK;
}

/* Largest row boundary whose prefix is <= target, or < target. */
static size_t nx_vlist_find_prefix(const nx_virtual_list *list,
                                   double target, int strict) {
    size_t bit = 1u;
    while (bit <= list->count / 2u) bit <<= 1u;
    size_t index = 0u;
    double sum = 0.0;
    while (bit) {
        size_t next = index + bit;
        if (next <= list->count) {
            double candidate = sum + list->tree[next];
            if (strict ? candidate < target : candidate <= target) {
                index = next;
                sum = candidate;
            }
        }
        bit >>= 1u;
    }
    return index;
}

nx_result nx_virtual_list_range(const nx_virtual_list *list,
                                 double offset, double viewport_height,
                                 size_t overscan_rows,
                                 nx_virtual_range *out_range) {
    if (!out_range) return NX_ERROR_ARGUMENT;
    *out_range = (nx_virtual_range){0};
    if (!list || !isfinite(offset) || offset < 0.0 ||
        !isfinite(viewport_height) || viewport_height <= 0.0)
        return NX_ERROR_ARGUMENT;
    out_range->total_extent = list->total;
    if (!list->count) return NX_OK;
    double maximum = fmax(0.0, list->total - viewport_height);
    double effective = fmin(offset, maximum);
    double end_offset = fmin(list->total, effective + viewport_height);
    size_t first = nx_vlist_find_prefix(list, effective, 0);
    size_t end = nx_vlist_find_prefix(list, end_offset, 1);
    if (first >= list->count) first = list->count - 1u;
    end = end >= list->count ? list->count : end + 1u;
    if (overscan_rows > 256u) overscan_rows = 256u;
    first = first > overscan_rows ? first - overscan_rows : 0u;
    end = overscan_rows >= list->count - end ? list->count :
          end + overscan_rows;
    out_range->first = first;
    out_range->end = end;
    out_range->first_offset = nx_vlist_prefix(list, first);
    out_range->effective_offset = effective;
    return NX_OK;
}

nx_result nx_virtual_list_visit(const nx_virtual_list *list,
                                 double offset, double viewport_height,
                                 size_t overscan_rows, size_t max_rows,
                                 nx_virtual_list_visit_fn visit, void *user,
                                 nx_virtual_range *out_range,
                                 size_t *out_visited) {
    if (!out_range || !out_visited) return NX_ERROR_ARGUMENT;
    *out_range = (nx_virtual_range){0};
    *out_visited = 0;
    if (!visit) return NX_ERROR_ARGUMENT;
    nx_result result = nx_virtual_list_range(list, offset, viewport_height,
                                              overscan_rows, out_range);
    if (result != NX_OK) return result;
    if (out_range->end - out_range->first > max_rows) return NX_ERROR_LIMIT;
    double content_y = out_range->first_offset;
    for (size_t i = out_range->first; i < out_range->end; ++i) {
        nx_virtual_row row = {i, list->keys[i], content_y,
                              content_y - out_range->effective_offset,
                              list->heights[i]};
        result = visit(user, &row);
        if (result != NX_OK) return result;
        ++*out_visited;
        content_y += row.height;
    }
    return NX_OK;
}

static size_t nx_vlist_key_slot(uint64_t key, size_t capacity) {
    key ^= key >> 33u;
    key *= UINT64_C(0xff51afd7ed558ccd);
    key ^= key >> 33u;
    key *= UINT64_C(0xc4ceb9fe1a85ec53);
    key ^= key >> 33u;
    return (size_t)key & (capacity - 1u);
}

nx_result nx_virtual_list_rebuild(nx_virtual_list *list, size_t count,
                                   nx_virtual_list_row_fn row, void *user,
                                   double viewport_height, double *io_offset) {
    if (!list || !io_offset || (count && !row) ||
        !isfinite(*io_offset) || *io_offset < 0.0 ||
        !isfinite(viewport_height) || viewport_height <= 0.0)
        return NX_ERROR_ARGUMENT;
    if (count > (SIZE_MAX / sizeof(double)) - 1u ||
        count > SIZE_MAX / sizeof(uint64_t) ||
        count > SIZE_MAX / 2u)
        return NX_ERROR_LIMIT;
    size_t capacity = 1u;
    if (count) {
        while (capacity < count * 2u) {
            if (capacity > SIZE_MAX / 2u) return NX_ERROR_LIMIT;
            capacity *= 2u;
        }
        if (capacity > SIZE_MAX / sizeof(size_t)) return NX_ERROR_LIMIT;
    }
    double old_maximum = fmax(0.0, list->total - viewport_height);
    double old_offset = fmin(*io_offset, old_maximum);
    uint64_t anchor_key = 0;
    double anchor_within = 0.0;
    if (list->count) {
        size_t index = nx_vlist_find_prefix(list, old_offset, 0);
        if (index >= list->count) index = list->count - 1u;
        anchor_key = list->keys[index];
        anchor_within = fmax(0.0,
                             old_offset - nx_vlist_prefix(list, index));
    }
    nx_allocator a = list->allocator;
    uint64_t *keys = NULL;
    double *heights = NULL, *tree = NULL;
    size_t *slots = NULL;
    nx_result result = NX_OK;
    double total = 0.0, anchor_position = old_offset;
    int anchor_found = 0;
    if (count) {
        keys = a.allocate(a.user, count * sizeof(uint64_t));
        heights = a.allocate(a.user, count * sizeof(double));
        tree = a.allocate(a.user, (count + 1u) * sizeof(double));
        slots = a.allocate(a.user, capacity * sizeof(size_t));
        if (!keys || !heights || !tree || !slots) {
            result = NX_ERROR_MEMORY;
            goto cleanup;
        }
        memset(slots, 0, capacity * sizeof(size_t));
        tree[0] = 0.0;
    }
    for (size_t i = 0; i < count; ++i) {
        uint64_t key = 0;
        double height = 0.0;
        result = row(user, i, &key, &height);
        if (result != NX_OK) goto cleanup;
        if (!key || !isfinite(height) || height <= 0.0) {
            result = NX_ERROR_ARGUMENT;
            goto cleanup;
        }
        size_t slot = nx_vlist_key_slot(key, capacity);
        while (slots[slot]) {
            if (keys[slots[slot] - 1u] == key) {
                result = NX_ERROR_ARGUMENT;
                goto cleanup;
            }
            slot = (slot + 1u) & (capacity - 1u);
        }
        slots[slot] = i + 1u;
        keys[i] = key;
        heights[i] = height;
        tree[i + 1u] = height;
        if (key == anchor_key) {
            anchor_position = total + fmin(anchor_within, height);
            anchor_found = 1;
        }
        total += height;
        if (!isfinite(total)) {
            result = NX_ERROR_LIMIT;
            goto cleanup;
        }
    }
    for (size_t i = 1u; i <= count; ++i) {
        size_t low = nx_vlist_lowbit(i);
        if (low > count - i) continue;
        size_t next = i + low;
        double sum = tree[next] + tree[i];
        if (!isfinite(sum) || sum <= 0.0) {
            result = NX_ERROR_LIMIT;
            goto cleanup;
        }
        tree[next] = sum;
    }
    double next_offset = anchor_found ? anchor_position : old_offset;
    next_offset = fmin(next_offset, fmax(0.0, total - viewport_height));
    if (list->tree)
        a.deallocate(a.user, list->tree,
                     (list->count + 1u) * sizeof(double));
    if (list->heights)
        a.deallocate(a.user, list->heights,
                     list->count * sizeof(double));
    if (list->keys)
        a.deallocate(a.user, list->keys,
                     list->count * sizeof(uint64_t));
    list->count = count;
    list->keys = keys;
    list->heights = heights;
    list->tree = tree;
    list->total = total;
    *io_offset = next_offset;
    keys = NULL;
    heights = tree = NULL;
cleanup:
    if (slots) a.deallocate(a.user, slots, capacity * sizeof(size_t));
    if (tree) a.deallocate(a.user, tree, (count + 1u) * sizeof(double));
    if (heights) a.deallocate(a.user, heights, count * sizeof(double));
    if (keys) a.deallocate(a.user, keys, count * sizeof(uint64_t));
    return result;
}
