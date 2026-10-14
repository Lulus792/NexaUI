#include "nexaui/nexaui.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct nx_tree_stack_entry {
    size_t index;
    int descendants_visible;
} nx_tree_stack_entry;

static void *nx_tree_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_tree_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static nx_result nx_tree_scan(const nx_tree_node *nodes, size_t count,
                              nx_tree_stack_entry *stack, size_t *indices,
                              size_t *out_count) {
    size_t depth = 0, visible_count = 0;
    for (size_t i = 0; i < count; ++i) {
        int visible;
        if (nodes[i].parent_index == SIZE_MAX) {
            depth = 0;
            visible = 1;
        } else {
            while (depth && stack[depth - 1u].index != nodes[i].parent_index)
                --depth;
            if (!depth) return NX_ERROR_ARGUMENT;
            visible = stack[depth - 1u].descendants_visible;
        }
        if (visible) {
            if (indices) indices[visible_count] = i;
            ++visible_count;
        }
        stack[depth++] = (nx_tree_stack_entry){i,
                                               visible && !!nodes[i].expanded};
    }
    *out_count = visible_count;
    return NX_OK;
}

nx_result nx_tree_flatten(const nx_tree_node *nodes, size_t count,
                          const nx_allocator *allocator, size_t *indices,
                          size_t capacity, size_t *out_visible_count) {
    if (!out_visible_count) return NX_ERROR_ARGUMENT;
    *out_visible_count = 0;
    if (count && !nodes) return NX_ERROR_ARGUMENT;
    if (count > SIZE_MAX / sizeof(nx_tree_stack_entry)) return NX_ERROR_LIMIT;
    nx_allocator a = allocator ? *allocator : (nx_allocator){0};
    if ((a.allocate == NULL) != (a.deallocate == NULL))
        return NX_ERROR_ARGUMENT;
    if (!a.allocate) {
        a.allocate = nx_tree_allocate;
        a.deallocate = nx_tree_deallocate;
        a.user = NULL;
    }
    if (!count) return NX_OK;
    size_t bytes = count * sizeof(nx_tree_stack_entry);
    nx_tree_stack_entry *stack = a.allocate(a.user, bytes);
    if (!stack) return NX_ERROR_MEMORY;
    size_t visible_count = 0;
    nx_result result = nx_tree_scan(nodes, count, stack, NULL, &visible_count);
    if (result == NX_OK) {
        *out_visible_count = visible_count;
        if (indices) {
            if (capacity < visible_count) result = NX_ERROR_LIMIT;
            else result = nx_tree_scan(nodes, count, stack, indices,
                                       &visible_count);
        }
    }
    a.deallocate(a.user, stack, bytes);
    return result;
}
