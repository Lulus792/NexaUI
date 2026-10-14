#include "drop_queue.h"

#include <stdint.h>
#include <string.h>

void nx_platform_drop_queue_init(nx_platform_drop_queue *queue,
                                 nx_allocator allocator) {
    if (!queue) return;
    *queue = (nx_platform_drop_queue){0};
    queue->allocator = allocator;
}

nx_result nx_platform_drop_queue_append_batch(nx_platform_drop_queue *queue,
                                               const nx_platform_drop_entry *batch,
                                               size_t count) {
    if (!queue || !queue->allocator.allocate || !queue->allocator.deallocate ||
        (!batch && count)) return NX_ERROR_ARGUMENT;
    if (!count) return NX_OK;
    if (count > SIZE_MAX / sizeof(*batch) ||
        queue->count > SIZE_MAX - count) return NX_ERROR_LIMIT;
    for (size_t i = 0; i < count; ++i)
        if (!batch[i].path || batch[i].length == SIZE_MAX)
            return NX_ERROR_ARGUMENT;
    size_t needed = queue->count + count;
    if (needed > queue->capacity) {
        size_t capacity = queue->capacity ? queue->capacity : 8u;
        while (capacity < needed) {
            if (capacity > SIZE_MAX / 2u) return NX_ERROR_LIMIT;
            capacity *= 2u;
        }
        if (capacity > SIZE_MAX / sizeof(*batch)) return NX_ERROR_LIMIT;
        nx_platform_drop_entry *grown = (nx_platform_drop_entry *)
            queue->allocator.allocate(queue->allocator.user,
                                      capacity * sizeof(*grown));
        if (!grown) return NX_ERROR_MEMORY;
        for (size_t i = 0; i < queue->count; ++i)
            grown[i] = queue->entries[(queue->head + i) % queue->capacity];
        if (queue->entries)
            queue->allocator.deallocate(queue->allocator.user,
                                         queue->entries,
                                         queue->capacity * sizeof(*grown));
        queue->entries = grown;
        queue->capacity = capacity;
        queue->head = 0;
    }
    for (size_t i = 0; i < count; ++i)
        queue->entries[(queue->head + queue->count + i) % queue->capacity] =
            batch[i];
    queue->count = needed;
    return NX_OK;
}

nx_result nx_platform_drop_queue_next(nx_platform_drop_queue *queue,
                                       char *path_utf8, size_t capacity,
                                       size_t *out_length,
                                       nx_file_drop_info *out_info,
                                       int *available) {
    if (!out_length || !out_info || !available || (!path_utf8 && capacity))
        return NX_ERROR_ARGUMENT;
    *out_length = 0;
    *out_info = (nx_file_drop_info){0};
    *available = 0;
    if (!queue || !queue->allocator.deallocate) return NX_ERROR_ARGUMENT;
    if (!queue->count) return NX_OK;
    nx_platform_drop_entry *entry = &queue->entries[queue->head];
    *available = 1;
    *out_length = entry->length;
    *out_info = entry->info;
    if (!path_utf8) return NX_OK;
    if (capacity <= entry->length) return NX_ERROR_LIMIT;
    memcpy(path_utf8, entry->path, entry->length + 1u);
    queue->allocator.deallocate(queue->allocator.user, entry->path,
                                 entry->length + 1u);
    queue->head = (queue->head + 1u) % queue->capacity;
    --queue->count;
    return NX_OK;
}

void nx_platform_drop_queue_destroy(nx_platform_drop_queue *queue) {
    if (!queue) return;
    for (size_t i = 0; i < queue->count; ++i) {
        nx_platform_drop_entry *entry = &queue->entries[
            (queue->head + i) % queue->capacity];
        queue->allocator.deallocate(queue->allocator.user,
                                     entry->path, entry->length + 1u);
    }
    if (queue->entries)
        queue->allocator.deallocate(queue->allocator.user, queue->entries,
                                     queue->capacity * sizeof(*queue->entries));
    *queue = (nx_platform_drop_queue){0};
}
