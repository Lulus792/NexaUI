#include "drop_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

typedef struct allocation_probe {
    size_t calls, live, fail_on;
} allocation_probe;

static void *probe_allocate(void *user, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    if (++probe->calls == probe->fail_on) return NULL;
    void *pointer = malloc(size);
    if (pointer) ++probe->live;
    return pointer;
}

static void probe_deallocate(void *user, void *pointer, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    (void)size;
    if (pointer) --probe->live;
    free(pointer);
}

static nx_platform_drop_entry make_entry(nx_allocator allocator,
                                         const char *path, uint32_t index) {
    nx_platform_drop_entry entry = {0};
    entry.length = strlen(path);
    entry.path = (char *)allocator.allocate(allocator.user, entry.length + 1u);
    if (entry.path) memcpy(entry.path, path, entry.length + 1u);
    entry.info.index = index;
    entry.info.count = 12;
    return entry;
}

static int test_order_and_buffer_contract(void) {
    allocation_probe probe = {0};
    nx_allocator allocator = {&probe, probe_allocate, probe_deallocate};
    nx_platform_drop_queue queue = {0};
    nx_platform_drop_queue_init(&queue, allocator);
    nx_platform_drop_entry first[3] = {
        make_entry(allocator, "alpha", 0),
        make_entry(allocator, "caf\xc3\xa9", 1),
        make_entry(allocator, "gamma", 2)
    };
    CHECK(first[0].path && first[1].path && first[2].path);
    CHECK(nx_platform_drop_queue_append_batch(&queue, first, 3) == NX_OK);
    size_t length = 99;
    nx_file_drop_info info = {0};
    int available = 0;
    CHECK(nx_platform_drop_queue_next(&queue, NULL, 0, &length, &info,
                                       &available) == NX_OK &&
          available && length == 5 && info.index == 0 && queue.count == 3);
    char short_buffer[5] = {'x', 'x', 'x', 'x', 'x'};
    CHECK(nx_platform_drop_queue_next(&queue, short_buffer,
                                       sizeof(short_buffer), &length, &info,
                                       &available) == NX_ERROR_LIMIT &&
          available && length == 5 && short_buffer[0] == 'x' &&
          queue.count == 3);
    char path[32];
    CHECK(nx_platform_drop_queue_next(&queue, path, sizeof(path), &length,
                                       &info, &available) == NX_OK &&
          available && strcmp(path, "alpha") == 0 && queue.count == 2);

    nx_platform_drop_entry more[8];
    for (uint32_t i = 0; i < 8; ++i) {
        char name[16];
        snprintf(name, sizeof(name), "path-%u", i);
        more[i] = make_entry(allocator, name, i + 3u);
        CHECK(more[i].path);
    }
    CHECK(nx_platform_drop_queue_append_batch(&queue, more, 8) == NX_OK);
    CHECK(queue.capacity >= 10 && queue.count == 10);
    CHECK(nx_platform_drop_queue_next(&queue, path, sizeof(path), &length,
                                       &info, &available) == NX_OK &&
          available && strcmp(path, "caf\xc3\xa9") == 0 && info.index == 1);
    CHECK(nx_platform_drop_queue_next(&queue, path, sizeof(path), &length,
                                       &info, &available) == NX_OK &&
          available && strcmp(path, "gamma") == 0 && info.index == 2);
    for (uint32_t i = 0; i < 8; ++i) {
        char expected[16];
        snprintf(expected, sizeof(expected), "path-%u", i);
        CHECK(nx_platform_drop_queue_next(&queue, path, sizeof(path), &length,
                                           &info, &available) == NX_OK &&
              available && strcmp(path, expected) == 0 &&
              info.index == i + 3u && info.count == 12);
    }
    CHECK(nx_platform_drop_queue_next(&queue, path, sizeof(path), &length,
                                       &info, &available) == NX_OK &&
          !available && length == 0 && info.count == 0);
    nx_platform_drop_queue_destroy(&queue);
    CHECK(probe.live == 0);
    return 0;
}

static int test_failed_batch_keeps_prior_and_ownership(void) {
    allocation_probe probe = {0};
    nx_allocator allocator = {&probe, probe_allocate, probe_deallocate};
    nx_platform_drop_queue queue = {0};
    nx_platform_drop_queue_init(&queue, allocator);
    nx_platform_drop_entry prior = make_entry(allocator, "prior", 0);
    CHECK(prior.path);
    CHECK(nx_platform_drop_queue_append_batch(&queue, &prior, 1) == NX_OK);
    nx_platform_drop_entry batch[8];
    for (uint32_t i = 0; i < 8; ++i) {
        batch[i] = make_entry(allocator, "new", i + 1u);
        CHECK(batch[i].path);
    }
    size_t live_before = probe.live;
    probe.fail_on = probe.calls + 1u;
    CHECK(nx_platform_drop_queue_append_batch(&queue, batch, 8) ==
          NX_ERROR_MEMORY);
    CHECK(queue.count == 1 && queue.capacity == 8 &&
          probe.live == live_before);
    for (size_t i = 0; i < 8; ++i)
        allocator.deallocate(allocator.user, batch[i].path,
                             batch[i].length + 1u);
    char path[16];
    size_t length = 0;
    nx_file_drop_info info = {0};
    int available = 0;
    CHECK(nx_platform_drop_queue_next(&queue, path, sizeof(path), &length,
                                       &info, &available) == NX_OK &&
          available && strcmp(path, "prior") == 0);
    nx_platform_drop_entry pending = make_entry(allocator, "pending", 9);
    CHECK(pending.path);
    CHECK(nx_platform_drop_queue_append_batch(&queue, &pending, 1) == NX_OK);
    nx_platform_drop_queue_destroy(&queue);
    CHECK(probe.live == 0);
    return 0;
}

int main(void) {
    if (test_order_and_buffer_contract() ||
        test_failed_batch_keeps_prior_and_ownership()) return 1;
    puts("portable platform drop queue tests passed");
    return 0;
}
