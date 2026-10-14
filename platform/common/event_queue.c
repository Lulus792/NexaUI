#include "event_queue.h"

#include <stdint.h>

nx_result nx_platform_event_queue_init(nx_platform_event_queue *queue,
                                       nx_allocator allocator,
                                       size_t initial_capacity) {
    if (!queue || !allocator.allocate || !allocator.deallocate ||
        !initial_capacity) return NX_ERROR_ARGUMENT;
    if (initial_capacity > SIZE_MAX / sizeof(nx_event)) return NX_ERROR_LIMIT;
    *queue = (nx_platform_event_queue){0};
    nx_event *events = (nx_event *)allocator.allocate(
        allocator.user, initial_capacity * sizeof(nx_event));
    if (!events) return NX_ERROR_MEMORY;
    queue->allocator = allocator;
    queue->events = events;
    queue->capacity = initial_capacity;
    return NX_OK;
}

void nx_platform_event_queue_destroy(nx_platform_event_queue *queue) {
    if (!queue) return;
    if (queue->events)
        queue->allocator.deallocate(queue->allocator.user, queue->events,
                                     queue->capacity * sizeof(nx_event));
    *queue = (nx_platform_event_queue){0};
}

void nx_platform_event_queue_push(nx_platform_event_queue *queue,
                                  nx_event event) {
    if (!queue || !queue->events || queue->error != NX_OK) return;
    if (event.type == NX_EVENT_POINTER_MOVE && queue->count) {
        size_t last = (queue->head + queue->count - 1u) % queue->capacity;
        if (queue->events[last].type == NX_EVENT_POINTER_MOVE) {
            queue->events[last] = event;
            return;
        }
    }
    if (queue->count == queue->capacity) {
        if (queue->capacity > SIZE_MAX / 2u ||
            queue->capacity * 2u > SIZE_MAX / sizeof(nx_event)) {
            queue->error = NX_ERROR_LIMIT;
            return;
        }
        size_t next = queue->capacity * 2u;
        nx_event *events = (nx_event *)queue->allocator.allocate(
            queue->allocator.user, next * sizeof(nx_event));
        if (!events) {
            queue->error = NX_ERROR_MEMORY;
            return;
        }
        for (size_t i = 0; i < queue->count; ++i)
            events[i] = queue->events[(queue->head + i) % queue->capacity];
        queue->allocator.deallocate(queue->allocator.user, queue->events,
                                     queue->capacity * sizeof(nx_event));
        queue->events = events;
        queue->capacity = next;
        queue->head = 0;
    }
    size_t tail = (queue->head + queue->count) % queue->capacity;
    queue->events[tail] = event;
    ++queue->count;
}

nx_result nx_platform_event_queue_next(nx_platform_event_queue *queue,
                                       nx_event *out_event, int *available) {
    if (!out_event || !available) return NX_ERROR_ARGUMENT;
    *out_event = (nx_event){0};
    *available = 0;
    if (!queue || !queue->events) return NX_ERROR_ARGUMENT;
    if (!queue->count) return NX_OK;
    *out_event = queue->events[queue->head];
    queue->head = (queue->head + 1u) % queue->capacity;
    --queue->count;
    *available = 1;
    return NX_OK;
}

nx_result nx_platform_event_queue_route(nx_platform_event_queue *queue,
                                        nx_context *context,
                                        size_t *out_count) {
    if (!out_count) return NX_ERROR_ARGUMENT;
    *out_count = 0;
    if (!queue || !queue->events || !context) return NX_ERROR_ARGUMENT;
    while (queue->count) {
        nx_result result = nx_context_queue_event(context,
                                                   &queue->events[queue->head]);
        if (result != NX_OK) return result;
        queue->head = (queue->head + 1u) % queue->capacity;
        --queue->count;
        ++*out_count;
    }
    return queue->error;
}
