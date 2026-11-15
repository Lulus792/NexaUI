#include "nexaui/nexaui.h"
#include "event_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

typedef struct allocation_probe {
    unsigned calls, frees, fail_on;
    void *last_allocation, *watched;
    size_t watched_size;
    unsigned watched_frees;
    int watched_dirty;
} allocation_probe;

static int zero_bytes(const void *memory, size_t size) {
    const unsigned char *bytes = memory;
    for (size_t i = 0; i < size; ++i)
        if (bytes[i]) return 0;
    return 1;
}

static void *probe_allocate(void *user, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    if (++probe->calls == probe->fail_on) return NULL;
    probe->last_allocation = malloc(size);
    return probe->last_allocation;
}

static void probe_deallocate(void *user, void *pointer, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    if (pointer == probe->watched) {
        ++probe->watched_frees;
        if (size < probe->watched_size || !zero_bytes(pointer, probe->watched_size))
            probe->watched_dirty = 1;
        probe->watched = NULL;
    }
    ++probe->frees;
    free(pointer);
}

static nx_event event(nx_event_type type, float x);

static int test_native_input_retirement(void) {
    allocation_probe probe = {0};
    nx_allocator allocator = {&probe, probe_allocate, probe_deallocate};
    nx_platform_event_queue queue = {0};
    CHECK(nx_platform_event_queue_init(&queue, allocator, 2) == NX_OK);
    nx_platform_event_queue_push(&queue, event(NX_EVENT_TEXT, 1));
    nx_platform_event_queue_push(&queue, event(NX_EVENT_KEY_DOWN, 2));
    nx_event received;
    int available = 0;
    CHECK(nx_platform_event_queue_next(&queue, &received, &available) == NX_OK &&
          available && received.scalar == 'A');
    CHECK(zero_bytes(queue.events, sizeof(nx_event)));

    /* The surviving key and the new text wrap around the ring. */
    nx_platform_event_queue_push(&queue, event(NX_EVENT_TEXT, 3));
    probe.watched = queue.events;
    probe.watched_size = 2 * sizeof(nx_event);
    nx_platform_event_queue_push(&queue, event(NX_EVENT_KEY_UP, 4));
    CHECK(probe.watched_frees == 1 && !probe.watched_dirty);
    CHECK(queue.count == 3 && queue.events[0].type == NX_EVENT_KEY_DOWN &&
          queue.events[1].scalar == 'A' && queue.events[2].type == NX_EVENT_KEY_UP);

    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    size_t routed;
    CHECK(nx_platform_event_queue_route(&queue, ctx, &routed) == NX_OK && routed == 3);
    CHECK(zero_bytes(queue.events, 3 * sizeof(nx_event)));
    CHECK(nx_begin_frame(ctx, 20, 20, 0) == NX_OK);
    const nx_event *frame = NULL;
    size_t count;
    CHECK(nx_frame_events(ctx, &frame, &count) == NX_OK && count == 3 && frame[1].scalar == 'A');
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_context_destroy(ctx);

    /* Destruction with undelivered input also wipes active records. */
    nx_platform_event_queue_push(&queue, event(NX_EVENT_TEXT, 5));
    nx_platform_event_queue_push(&queue, event(NX_EVENT_TEXT, 6));
    probe.watched = queue.events;
    probe.watched_size = 4 * sizeof(nx_event);
    nx_platform_event_queue_destroy(&queue);
    CHECK(probe.watched_frees == 2 && !probe.watched_dirty);
    return 0;
}

static int test_context_input_retirement(void) {
    allocation_probe probe = {0};
    nx_context_desc desc = {0};
    desc.allocator = (nx_allocator){&probe, probe_allocate, probe_deallocate};
    desc.initial_event_capacity = 1;
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&desc, &ctx) == NX_OK);
    nx_event text = event(NX_EVENT_TEXT, 1);
    CHECK(nx_context_queue_event(ctx, &text) == NX_OK);
    probe.watched = probe.last_allocation;
    probe.watched_size = sizeof(nx_event);
    nx_event key = event(NX_EVENT_KEY_DOWN, 2);
    CHECK(nx_context_queue_event(ctx, &key) == NX_OK);
    CHECK(probe.watched_frees == 1 && !probe.watched_dirty);
    const void *frame_storage = probe.last_allocation;
    const nx_event *frame = NULL;
    size_t count;
    CHECK(nx_begin_frame(ctx, 20, 20, 0) == NX_OK && nx_end_frame(ctx) == NX_OK);
    CHECK(nx_frame_events(ctx, &frame, &count) == NX_OK && count == 2 &&
          frame[0].scalar == 'A' && frame[1].key == NX_KEY_A);
    CHECK(nx_begin_frame(ctx, -1, 20, 0) == NX_ERROR_ARGUMENT);
    CHECK(nx_frame_events(ctx, &frame, &count) == NX_OK && count == 2 && frame[0].scalar == 'A');
    CHECK(nx_begin_frame(ctx, 20, 20, 0) == NX_OK);
    CHECK(zero_bytes(frame_storage, 2 * sizeof(nx_event)));
    CHECK(nx_end_frame(ctx) == NX_OK);

    CHECK(nx_context_queue_event(ctx, &text) == NX_OK);
    probe.watched = (void *)frame_storage;
    probe.watched_size = sizeof(nx_event);
    nx_context_destroy(ctx);
    CHECK(probe.watched_frees == 2 && !probe.watched_dirty);
    return 0;
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
    CHECK(zero_bytes(queue.events, sizeof(nx_event)));
    CHECK(queue.events[queue.head].type == NX_EVENT_KEY_UP &&
          queue.events[queue.head].key == NX_KEY_A);
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

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "native-retirement")) return test_native_input_retirement();
    if (argc == 2 && !strcmp(argv[1], "context-retirement")) return test_context_input_retirement();
    if (argc != 1) return 2;
    if (test_order_and_routing() || test_sticky_allocation_failure() ||
        test_route_retry_after_context_growth_failure() ||
        test_native_input_retirement() || test_context_input_retirement()) return 1;
    puts("portable platform event queue tests passed");
    return 0;
}
