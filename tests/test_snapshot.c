#include "nexaui/nexaui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HANDLE test_thread;
#else
#include <pthread.h>
#include <sched.h>
typedef pthread_t test_thread;
#endif

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

typedef struct payload {
    uint64_t sequence, inverse;
    uint64_t samples[8];
} payload;

typedef struct producer_args {
    nx_snapshot_exchange *exchange;
    uint64_t count;
    size_t *disposals;
    int failed;
} producer_args;

static payload *make_payload(uint64_t sequence) {
    payload *data = (payload *)malloc(sizeof(*data));
    if (!data) return NULL;
    data->sequence = sequence;
    data->inverse = ~sequence;
    for (unsigned i = 0; i < 8; ++i) data->samples[i] = sequence ^ (uint64_t)i;
    return data;
}

static int valid_payload(const nx_snapshot_view *view) {
    if (!view->data || view->bytes != sizeof(payload)) return 0;
    const payload *data = (const payload *)view->data;
    if (data->inverse != ~data->sequence) return 0;
    for (unsigned i = 0; i < 8; ++i)
        if (data->samples[i] != (data->sequence ^ (uint64_t)i)) return 0;
    return 1;
}

static void dispose_payload(void *user, void *data, size_t bytes) {
    if (user) ++*(size_t *)user;
    (void)bytes;
    free(data);
}

static void producer_run(producer_args *args) {
    for (uint64_t i = 1; i <= args->count; ++i) {
        payload *data = make_payload(i);
        if (!data) { args->failed = 1; return; }
        if (nx_snapshot_exchange_publish(args->exchange, data, sizeof(*data),
                                         dispose_payload, args->disposals) != NX_OK) {
            free(data); /* Failed publish did not transfer ownership. */
            args->failed = 1;
            return;
        }
    }
}

#if defined(_WIN32)
static DWORD WINAPI producer_entry(LPVOID user) {
    producer_run((producer_args *)user);
    return 0;
}
static int start_thread(test_thread *thread, producer_args *args) {
    *thread = CreateThread(NULL, 0, producer_entry, args, 0, NULL);
    return *thread != NULL;
}
static int join_thread(test_thread thread) {
    DWORD result = WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return result == WAIT_OBJECT_0;
}
static void yield_thread(void) { Sleep(0); }
#else
static void *producer_entry(void *user) {
    producer_run((producer_args *)user);
    return NULL;
}
static int start_thread(test_thread *thread, producer_args *args) {
    return pthread_create(thread, NULL, producer_entry, args) == 0;
}
static int join_thread(test_thread thread) {
    return pthread_join(thread, NULL) == 0;
}
static void yield_thread(void) { sched_yield(); }
#endif

static int test_pinned_lifetime(void) {
    nx_snapshot_exchange *exchange = NULL;
    CHECK(nx_snapshot_exchange_create(NULL, &exchange) == NX_OK);
    nx_snapshot_view view = {0};
    CHECK(nx_snapshot_exchange_acquire(exchange, &view) == NX_OK && !view.data);
    CHECK(nx_snapshot_exchange_release(exchange, &view) == NX_ERROR_STATE);
    size_t disposals = 0;
    payload *initial = make_payload(0);
    CHECK(initial != NULL);
    CHECK(nx_snapshot_exchange_publish(exchange, initial, sizeof(*initial),
                                       dispose_payload, &disposals) == NX_OK);
    CHECK(nx_snapshot_exchange_publish(exchange, initial, sizeof(*initial),
                                       dispose_payload, &disposals) == NX_ERROR_ARGUMENT);
    CHECK(nx_snapshot_exchange_acquire(exchange, &view) == NX_OK);
    CHECK(valid_payload(&view) && ((const payload *)view.data)->sequence == 0);
    nx_snapshot_view stale = view;
    nx_snapshot_view another = {0};
    CHECK(nx_snapshot_exchange_acquire(exchange, &another) == NX_ERROR_STATE);
    CHECK(!another.data);
    CHECK(nx_snapshot_exchange_destroy(exchange) == NX_ERROR_STATE);
    producer_args args = {exchange, 20000, &disposals, 0};
    test_thread thread;
    CHECK(start_thread(&thread, &args));
    CHECK(join_thread(thread) && !args.failed);
    CHECK(valid_payload(&view) && ((const payload *)view.data)->sequence == 0);
    CHECK(disposals == 19999u);
    CHECK(nx_snapshot_exchange_release(exchange, &view) == NX_OK && !view.data);
    CHECK(disposals == 20000u);
    CHECK(nx_snapshot_exchange_acquire(exchange, &view) == NX_OK);
    CHECK(valid_payload(&view) &&
          ((const payload *)view.data)->sequence == args.count);
    CHECK(nx_snapshot_exchange_release(exchange, &stale) == NX_ERROR_STATE);
    CHECK(valid_payload(&view));
    CHECK(nx_snapshot_exchange_release(exchange, &view) == NX_OK);
    CHECK(nx_snapshot_exchange_destroy(exchange) == NX_OK);
    CHECK(disposals == 20001u);
    return 0;
}

static int test_concurrent_reads(void) {
    nx_snapshot_exchange *exchange = NULL;
    CHECK(nx_snapshot_exchange_create(NULL, &exchange) == NX_OK);
    payload *initial = make_payload(0);
    CHECK(initial != NULL);
    CHECK(nx_snapshot_exchange_publish(exchange, initial, sizeof(*initial),
                                       dispose_payload, NULL) == NX_OK);
    producer_args args = {exchange, 20000, NULL, 0};
    test_thread thread;
    CHECK(start_thread(&thread, &args));
    uint64_t last = 0;
    for (unsigned i = 0; i < 100000; ++i) {
        nx_snapshot_view view = {0};
        CHECK(nx_snapshot_exchange_acquire(exchange, &view) == NX_OK);
        CHECK(valid_payload(&view));
        uint64_t sequence = ((const payload *)view.data)->sequence;
        CHECK(sequence >= last && sequence <= args.count);
        last = sequence;
        CHECK(nx_snapshot_exchange_release(exchange, &view) == NX_OK);
        if ((i & 63u) == 0u) yield_thread();
    }
    CHECK(join_thread(thread) && !args.failed);
    nx_snapshot_view latest = {0};
    CHECK(nx_snapshot_exchange_acquire(exchange, &latest) == NX_OK);
    CHECK(valid_payload(&latest) &&
          ((const payload *)latest.data)->sequence == args.count);
    CHECK(nx_snapshot_exchange_release(exchange, &latest) == NX_OK);
    CHECK(nx_snapshot_exchange_destroy(exchange) == NX_OK);
    return 0;
}

typedef struct allocation_probe {
    unsigned calls, frees, fail_on;
} allocation_probe;

static void *probe_allocate(void *user, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    ++probe->calls;
    return probe->calls == probe->fail_on ? NULL : malloc(size);
}

static void probe_deallocate(void *user, void *data, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    ++probe->frees;
    (void)size;
    free(data);
}

static int test_validation_and_allocation(void) {
    CHECK(nx_snapshot_exchange_create(NULL, NULL) == NX_ERROR_ARGUMENT);
    nx_snapshot_exchange *exchange = (nx_snapshot_exchange *)(uintptr_t)1;
    nx_allocator invalid = {0};
    invalid.allocate = probe_allocate;
    CHECK(nx_snapshot_exchange_create(&invalid, &exchange) == NX_ERROR_ARGUMENT &&
          exchange == NULL);
    for (unsigned fail_on = 1; fail_on <= 2; ++fail_on) {
        allocation_probe probe = {0, 0, fail_on};
        nx_allocator allocator = {&probe, probe_allocate, probe_deallocate};
        CHECK(nx_snapshot_exchange_create(&allocator, &exchange) == NX_ERROR_MEMORY);
        CHECK(exchange == NULL && probe.calls == fail_on &&
              probe.frees == fail_on - 1u);
    }
    CHECK(nx_snapshot_exchange_create(NULL, &exchange) == NX_OK);
    payload *data = make_payload(7);
    CHECK(data != NULL);
    CHECK(nx_snapshot_exchange_publish(exchange, data, sizeof(*data),
                                       NULL, NULL) == NX_ERROR_ARGUMENT);
    CHECK(nx_snapshot_exchange_publish(NULL, data, sizeof(*data),
                                       dispose_payload, NULL) == NX_ERROR_ARGUMENT);
    free(data);
    nx_snapshot_view view = {0};
    CHECK(nx_snapshot_exchange_acquire(NULL, &view) == NX_ERROR_ARGUMENT);
    CHECK(nx_snapshot_exchange_acquire(exchange, NULL) == NX_ERROR_ARGUMENT);
    CHECK(nx_snapshot_exchange_release(exchange, NULL) == NX_ERROR_ARGUMENT);
    CHECK(nx_snapshot_exchange_destroy(exchange) == NX_OK);
    return 0;
}

int main(void) {
    if (test_pinned_lifetime()) return 1;
    if (test_concurrent_reads()) return 1;
    if (test_validation_and_allocation()) return 1;
    return 0;
}
