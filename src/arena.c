#include "nexaui/nexaui.h"

#include <stdlib.h>
#include <string.h>

struct nx_arena {
    nx_allocator allocator;
    unsigned char *buffer;
    size_t capacity;
    size_t used;
};

static void *nx_arena_default_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_arena_default_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

nx_result nx_arena_create(const nx_allocator *allocator, size_t capacity,
                          nx_arena **out_arena) {
    if (!out_arena) return NX_ERROR_ARGUMENT;
    *out_arena = NULL;
    if (!capacity) return NX_ERROR_ARGUMENT;
    nx_allocator a = allocator ? *allocator : (nx_allocator){0};
    if ((a.allocate == NULL) != (a.deallocate == NULL)) return NX_ERROR_ARGUMENT;
    if (!a.allocate) {
        a.user = NULL;
        a.allocate = nx_arena_default_allocate;
        a.deallocate = nx_arena_default_deallocate;
    }
    nx_arena *arena = (nx_arena *)a.allocate(a.user, sizeof(nx_arena));
    if (!arena) return NX_ERROR_MEMORY;
    memset(arena, 0, sizeof(*arena));
    arena->allocator = a;
    arena->buffer = (unsigned char *)a.allocate(a.user, capacity);
    if (!arena->buffer) {
        a.deallocate(a.user, arena, sizeof(nx_arena));
        return NX_ERROR_MEMORY;
    }
    arena->capacity = capacity;
    *out_arena = arena;
    return NX_OK;
}

void nx_arena_destroy(nx_arena *arena) {
    if (!arena) return;
    nx_allocator a = arena->allocator;
    a.deallocate(a.user, arena->buffer, arena->capacity);
    a.deallocate(a.user, arena, sizeof(nx_arena));
}

nx_result nx_arena_alloc(nx_arena *arena, size_t size, size_t alignment,
                         void **out_pointer) {
    if (!out_pointer) return NX_ERROR_ARGUMENT;
    *out_pointer = NULL;
    size_t max_alignment = _Alignof(long double);
    if (_Alignof(void *) > max_alignment) max_alignment = _Alignof(void *);
    if (_Alignof(long long) > max_alignment) max_alignment = _Alignof(long long);
    if (!arena || !size || !alignment || (alignment & (alignment - 1u)) ||
        alignment > max_alignment) return NX_ERROR_ARGUMENT;
    if (arena->used > SIZE_MAX - (alignment - 1u)) return NX_ERROR_LIMIT;
    size_t aligned = (arena->used + alignment - 1u) & ~(alignment - 1u);
    if (aligned > arena->capacity || size > arena->capacity - aligned)
        return NX_ERROR_LIMIT;
    *out_pointer = arena->buffer + aligned;
    arena->used = aligned + size;
    return NX_OK;
}

void nx_arena_reset(nx_arena *arena) {
    if (arena) arena->used = 0;
}

size_t nx_arena_used(const nx_arena *arena) {
    return arena ? arena->used : 0;
}

size_t nx_arena_capacity(const nx_arena *arena) {
    return arena ? arena->capacity : 0;
}
