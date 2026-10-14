#ifndef NEXAUI_PLATFORM_EVENT_QUEUE_H
#define NEXAUI_PLATFORM_EVENT_QUEUE_H

#include "nexaui/nexaui.h"

/* Private, single-threaded queue shared by native window adapters. The OS
   adapter owns thread checks and event translation. A push failure is sticky;
   already queued events remain available and route retries leave the first
   undelivered event in place. */
typedef struct nx_platform_event_queue {
    nx_allocator allocator;
    nx_event *events;
    size_t head, count, capacity;
    nx_result error;
} nx_platform_event_queue;

nx_result nx_platform_event_queue_init(nx_platform_event_queue *queue,
                                       nx_allocator allocator,
                                       size_t initial_capacity);
void nx_platform_event_queue_destroy(nx_platform_event_queue *queue);
void nx_platform_event_queue_push(nx_platform_event_queue *queue,
                                  nx_event event);
nx_result nx_platform_event_queue_next(nx_platform_event_queue *queue,
                                       nx_event *out_event, int *available);
nx_result nx_platform_event_queue_route(nx_platform_event_queue *queue,
                                        nx_context *context,
                                        size_t *out_count);

#endif
