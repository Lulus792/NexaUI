#include "nexaui/nexaui.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void *nx_table_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_table_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

nx_result nx_table_sort_indices(size_t count, nx_table_compare_fn compare,
                                 void *user, int descending,
                                 const nx_allocator *allocator,
                                 size_t *indices, size_t capacity) {
    if (capacity < count || (count && !indices) ||
        (count > 1u && !compare)) return NX_ERROR_ARGUMENT;
    if (count > SIZE_MAX / sizeof(size_t)) return NX_ERROR_LIMIT;
    nx_allocator a = allocator ? *allocator : (nx_allocator){0};
    if ((a.allocate == NULL) != (a.deallocate == NULL))
        return NX_ERROR_ARGUMENT;
    if (!a.allocate) {
        a.allocate = nx_table_allocate;
        a.deallocate = nx_table_deallocate;
        a.user = NULL;
    }
    if (!count) return NX_OK;
    size_t bytes = count * sizeof(size_t);
    size_t *order = a.allocate(a.user, bytes);
    if (!order) return NX_ERROR_MEMORY;
    size_t *scratch = a.allocate(a.user, bytes);
    if (!scratch) {
        a.deallocate(a.user, order, bytes);
        return NX_ERROR_MEMORY;
    }
    for (size_t i = 0; i < count; ++i) order[i] = i;
    for (size_t width = 1u; width < count;
         width = width > count / 2u ? count : width * 2u) {
        for (size_t start = 0; start < count;) {
            size_t middle = count - start < width ? count : start + width;
            size_t end = count - middle < width ? count : middle + width;
            size_t left = start, right = middle;
            for (size_t out = start; out < end; ++out) {
                int choose_left;
                if (right == end) choose_left = 1;
                else if (left == middle) choose_left = 0;
                else {
                    int relation = compare(order[left], order[right], user);
                    choose_left = descending ? relation >= 0 : relation <= 0;
                }
                scratch[out] = choose_left ? order[left++] : order[right++];
            }
            start = end;
        }
        size_t *swap = order;
        order = scratch;
        scratch = swap;
    }
    memcpy(indices, order, bytes);
    a.deallocate(a.user, scratch, bytes);
    a.deallocate(a.user, order, bytes);
    return NX_OK;
}
