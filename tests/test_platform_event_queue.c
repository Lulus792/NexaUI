#include "nexaui/nexaui.h"
#include "event_queue.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

typedef struct allocation_probe {
    unsigned calls, frees, fail_on;
} allocation_probe;

static void *probe_allocate(void *user, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    if (++probe->calls == probe->fail_on) return NULL;
    return malloc(size);
}

static void probe_deallocate(void *user, void *pointer, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    (void)size;
    ++probe->frees;
    free(pointer);
}

static nx_event event(nx_event_type type, float x) {
    nx_event value = {0};
    value.type = type;
    value.x = x;
    if (type == NX_EVENT_KEY_DOWN || type == NX_EVENT_KEY_UP)
        value.key = NX_KEY_A;
    if (type == NX_EVENT_TEXT) value.scalar = 'A';
    return value;
}

static int test_order_and_routing(void) {
    allocation_probe probe = {0};
    nx_allocator allocator = {&probe, probe_allocate, probe_deallocate};
    nx_platform_event_queue queue = {0};
    CHECK(nx_platform_event_queue_init(&queue, allocator, 2) == NX_OK);
    nx_platform_event_queue_push(&queue, event(NX_EVENT_POINTER_MOVE, 1));
    nx_platform_event_queue_push(&queue, event(NX_EVENT_POINTER_MOVE, 2));
    CHECK(queue.count == 1 && queue.events[queue.head].x == 2);
    nx_platform_event_queue_push(&queue, event(NX_EVENT_KEY_DOWN, 3));
    nx_event received;
    int available;
    CHECK(nx_platform_event_queue_next(&queue, &received, &available) == NX_OK &&
          available && received.type == NX_EVENT_POINTER_MOVE && received.x == 2);
    nx_platform_event_queue_push(&queue, event(NX_EVENT_KEY_UP, 4));
    CHECK(nx_platform_event_queue_next(&queue, &received, &available) == NX_OK &&
          available && received.type == NX_EVENT_KEY_DOWN);
    CHECK(nx_platform_event_queue_next(&queue, &received, &available) == NX_OK &&
          available && received.type == NX_EVENT_KEY_UP);
    CHECK(nx_platform_event_queue_next(&queue, &received, &available) == NX_OK &&
          !available);

    nx_platform_event_queue_push(&queue, event(NX_EVENT_KEY_DOWN, 5));
    nx_platform_event_queue_push(&queue, event(NX_EVENT_KEY_UP, 6));
    nx_platform_event_queue_push(&queue, event(NX_EVENT_TEXT, 7));
    CHECK(queue.capacity == 4 && queue.count == 3 && probe.calls == 2 &&
          probe.frees == 1);
    nx_context *ui = NULL;
    CHECK(nx_context_create(NULL, &ui) == NX_OK);
    size_t routed = 0;
    CHECK(nx_platform_event_queue_route(&queue, ui, &routed) == NX_OK &&
          routed == 3 && queue.count == 0);
    CHECK(nx_begin_frame(ui, 20, 20, 0) == NX_OK);
    const nx_event *events = NULL;
    size_t count = 0;
    CHECK(nx_frame_events(ui, &events, &count) == NX_OK && count == 3 &&
          events[0].x == 5 && events[1].x == 6 && events[2].x == 7);
    CHECK(nx_end_frame(ui) == NX_OK);
    nx_context_destroy(ui);
    nx_platform_event_queue_destroy(&queue);
    CHECK(probe.frees == 2);
    return 0;
}

static int test_sticky_allocation_failure(void) {
    allocation_probe probe = {0};
    probe.fail_on = 2;
    nx_allocator allocator = {&probe, probe_allocate, probe_deallocate};
    nx_platform_event_queue queue = {0};
    CHECK(nx_platform_event_queue_init(&queue, allocator, 1) == NX_OK);
    nx_platform_event_queue_push(&queue, event(NX_EVENT_KEY_DOWN, 1));
    nx_platform_event_queue_push(&queue, event(NX_EVENT_KEY_UP, 2));
    CHECK(queue.error == NX_ERROR_MEMORY && queue.count == 1);
    nx_platform_event_queue_push(&queue, event(NX_EVENT_TEXT, 3));
    CHECK(queue.count == 1 && probe.calls == 2);
    nx_event received;
    int available;
    CHECK(nx_platform_event_queue_next(&queue, &received, &available) == NX_OK &&
          available && received.type == NX_EVENT_KEY_DOWN);
    nx_context *ui = NULL;
    CHECK(nx_context_create(NULL, &ui) == NX_OK);
    size_t routed = 99;
    CHECK(nx_platform_event_queue_route(&queue, ui, &routed) == NX_ERROR_MEMORY &&
          routed == 0);
    nx_context_destroy(ui);
    nx_platform_event_queue_destroy(&queue);
    CHECK(probe.frees == 1);
    return 0;
}

static int test_route_retry_after_context_growth_failure(void) {
    allocation_probe queue_probe = {0}, context_probe = {0};
    nx_allocator queue_allocator = {&queue_probe, probe_allocate,
                                    probe_deallocate};
    nx_allocator context_allocator = {&context_probe, probe_allocate,
                                      probe_deallocate};
    nx_platform_event_queue queue = {0};
    CHECK(nx_platform_event_queue_init(&queue, queue_allocator, 2) == NX_OK);
    nx_platform_event_queue_push(&queue, event(NX_EVENT_KEY_DOWN, 1));
    nx_platform_event_queue_push(&queue, event(NX_EVENT_KEY_UP, 2));
    nx_context_desc desc = {0};
    desc.allocator = context_allocator;
    desc.initial_event_capacity = 1;
    nx_context *ui = NULL;
    CHECK(nx_context_create(&desc, &ui) == NX_OK);
    context_probe.fail_on = context_probe.calls + 2u;
    size_t routed = 99;
    CHECK(nx_platform_event_queue_route(&queue, ui, &routed) ==
          NX_ERROR_MEMORY && routed == 1 && queue.count == 1);
    context_probe.fail_on = 0;
    CHECK(nx_platform_event_queue_route(&queue, ui, &routed) == NX_OK &&
          routed == 1 && queue.count == 0);
    CHECK(nx_begin_frame(ui, 20, 20, 0) == NX_OK);
    const nx_event *events = NULL;
    size_t count = 0;
    CHECK(nx_frame_events(ui, &events, &count) == NX_OK && count == 2 &&
          events[0].type == NX_EVENT_KEY_DOWN &&
          events[1].type == NX_EVENT_KEY_UP);
    CHECK(nx_end_frame(ui) == NX_OK);
    nx_context_destroy(ui);
    nx_platform_event_queue_destroy(&queue);
    CHECK(queue_probe.frees == 1 && context_probe.calls > 2);
    return 0;
}

int main(void) {
    if (test_order_and_routing() || test_sticky_allocation_failure() ||
        test_route_retry_after_context_growth_failure()) return 1;
    puts("portable platform event queue tests passed");
    return 0;
}
