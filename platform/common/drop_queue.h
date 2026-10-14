#ifndef NEXAUI_PLATFORM_DROP_QUEUE_H
#define NEXAUI_PLATFORM_DROP_QUEUE_H

#include "nexaui/nexaui.h"

/* Private, single-threaded queue shared by native window adapters. UTF-8
   paths are allocated by the adapter with the queue's allocator.
   append_batch takes ownership only on success; a failure leaves the queue
   and every caller-owned batch entry unchanged. */
typedef struct nx_platform_drop_entry {
    char *path;
    size_t length;
    nx_file_drop_info info;
} nx_platform_drop_entry;

typedef struct nx_platform_drop_queue {
    nx_allocator allocator;
    nx_platform_drop_entry *entries;
    size_t head, count, capacity;
} nx_platform_drop_queue;

void nx_platform_drop_queue_init(nx_platform_drop_queue *queue,
                                 nx_allocator allocator);
nx_result nx_platform_drop_queue_append_batch(nx_platform_drop_queue *queue,
                                               const nx_platform_drop_entry *batch,
                                               size_t count);
nx_result nx_platform_drop_queue_next(nx_platform_drop_queue *queue,
                                       char *path_utf8, size_t capacity,
                                       size_t *out_length,
                                       nx_file_drop_info *out_info,
                                       int *available);
void nx_platform_drop_queue_destroy(nx_platform_drop_queue *queue);

#endif
