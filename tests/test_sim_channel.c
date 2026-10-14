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
    uint64_t value, inverse;
} payload;

static payload *make_payload(uint64_t value) {
    payload *data = (payload *)malloc(sizeof(*data));
    if (data) *data = (payload){value, ~value};
    return data;
}

static int valid_payload(const nx_sim_message *message, uint64_t value) {
    if (!message->data || message->bytes != sizeof(payload)) return 0;
    const payload *data = (const payload *)message->data;
    return data->value == value && data->inverse == ~value;
}

static void dispose_payload(void *user, void *data, size_t bytes) {
    if (user) ++*(unsigned *)user;
    (void)bytes;
    free(data);
}

static int test_capacity_and_completion_order(void) {
    nx_sim_channel *channel = NULL;
    CHECK(nx_sim_channel_create(2, NULL, &channel) == NX_OK);
    nx_sim_message message = {0};
    CHECK(nx_sim_channel_take_command(channel, &message) == NX_OK &&
          !message.ticket);
    payload *a = make_payload(10), *b = make_payload(20), *c = make_payload(30);
    CHECK(a && b && c);
    unsigned disposals = 0;
    uint64_t first = 0, second = 0, third = 99;
    CHECK(nx_sim_channel_post_command(channel, a, sizeof(*a),
                                      dispose_payload, &disposals, &first) == NX_OK &&
          first == 1);
    CHECK(nx_sim_channel_post_command(channel, b, sizeof(*b),
                                      dispose_payload, &disposals, &second) == NX_OK &&
          second == 2);
    CHECK(nx_sim_channel_post_command(channel, c, sizeof(*c),
                                      dispose_payload, &disposals, &third) ==
          NX_ERROR_LIMIT && third == 0 && disposals == 0);
    CHECK(nx_sim_channel_post_completion(channel, first, NX_OK,
                                         NULL, 0, NULL, NULL) == NX_ERROR_STATE);
    CHECK(nx_sim_channel_take_command(channel, &message) == NX_OK &&
          message.ticket == first && valid_payload(&message, 10));
    CHECK(nx_sim_channel_destroy(channel) == NX_ERROR_STATE);
    nx_sim_message_dispose(&message);
    CHECK(!message.ticket && disposals == 1);
    nx_sim_message_dispose(&message);
    CHECK(disposals == 1);
    CHECK(nx_sim_channel_take_command(channel, &message) == NX_OK &&
          message.ticket == second && valid_payload(&message, 20));
    nx_sim_message_dispose(&message);
    CHECK(disposals == 2);
    payload *answer = make_payload(200);
    CHECK(answer != NULL);
    CHECK(nx_sim_channel_post_completion(channel, second, NX_ERROR_PLATFORM,
                                         answer, sizeof(*answer), dispose_payload,
                                         &disposals) == NX_OK);
    CHECK(nx_sim_channel_post_completion(channel, first, NX_OK,
                                         NULL, 0, NULL, NULL) == NX_OK);
    CHECK(nx_sim_channel_post_completion(channel, second, NX_OK,
                                         NULL, 0, NULL, NULL) == NX_ERROR_STATE);
    CHECK(nx_sim_channel_post_command(channel, c, sizeof(*c),
                                      dispose_payload, &disposals, &third) ==
          NX_ERROR_LIMIT && third == 0);
    CHECK(nx_sim_channel_take_completion(channel, &message) == NX_OK &&
          message.ticket == second && message.status == NX_ERROR_PLATFORM &&
          valid_payload(&message, 200));
    nx_sim_message_dispose(&message);
    CHECK(disposals == 3);
    CHECK(nx_sim_channel_post_command(channel, c, sizeof(*c),
                                      dispose_payload, &disposals, &third) == NX_OK &&
          third == 3);
    CHECK(nx_sim_channel_take_command(channel, &message) == NX_OK &&
          message.ticket == third && valid_payload(&message, 30));
    nx_sim_message_dispose(&message);
    CHECK(nx_sim_channel_post_completion(channel, third, NX_OK,
                                         NULL, 0, NULL, NULL) == NX_OK);
    CHECK(nx_sim_channel_take_completion(channel, &message) == NX_OK &&
          message.ticket == first && message.status == NX_OK);
    nx_sim_message_dispose(&message);
    CHECK(nx_sim_channel_take_completion(channel, &message) == NX_OK &&
          message.ticket == third && message.status == NX_OK);
    nx_sim_message_dispose(&message);
    CHECK(nx_sim_channel_take_completion(channel, &message) == NX_OK &&
          !message.ticket);
    CHECK(nx_sim_channel_destroy(channel) == NX_OK && disposals == 4);
    return 0;
}

static int test_shutdown_and_validation(void) {
    nx_sim_channel *channel = NULL;
    CHECK(nx_sim_channel_create(2, NULL, &channel) == NX_OK);
    unsigned disposals = 0;
    payload *a = make_payload(1), *b = make_payload(2);
    CHECK(a && b);
    uint64_t first, second;
    CHECK(nx_sim_channel_post_command(channel, a, sizeof(*a),
                                      dispose_payload, &disposals, &first) == NX_OK);
    CHECK(nx_sim_channel_post_command(channel, b, sizeof(*b),
                                      dispose_payload, &disposals, &second) == NX_OK);
    nx_sim_message message;
    CHECK(nx_sim_channel_take_command(channel, &message) == NX_OK);
    nx_sim_message_dispose(&message);
    CHECK(nx_sim_channel_destroy(channel) == NX_ERROR_STATE);
    CHECK(nx_sim_channel_post_completion(channel, first, NX_ERROR_STATE,
                                         NULL, 0, NULL, NULL) == NX_OK);
    CHECK(nx_sim_channel_post_completion(channel, second, NX_OK,
                                         NULL, 0, NULL, NULL) == NX_ERROR_STATE);
    CHECK(nx_sim_channel_post_completion(channel, first, NX_OK,
                                         NULL, 1, NULL, NULL) == NX_ERROR_ARGUMENT);
    CHECK(nx_sim_channel_post_completion(channel, 0, NX_OK,
                                         NULL, 0, NULL, NULL) == NX_ERROR_ARGUMENT);
    CHECK(nx_sim_channel_destroy(channel) == NX_OK && disposals == 2);

    CHECK(nx_sim_channel_create(1, NULL, &channel) == NX_OK);
    payload *request = make_payload(3), *reply = make_payload(4);
    CHECK(request && reply);
    uint64_t ticket = 0;
    CHECK(nx_sim_channel_post_command(channel, request, sizeof(*request),
                                      dispose_payload, &disposals, &ticket) == NX_OK);
    CHECK(nx_sim_channel_take_command(channel, &message) == NX_OK);
    nx_sim_message_dispose(&message);
    CHECK(nx_sim_channel_post_completion(channel, ticket, NX_OK, reply,
                    sizeof(*reply), dispose_payload, &disposals) == NX_OK);
    CHECK(nx_sim_channel_destroy(channel) == NX_OK && disposals == 4);
    return 0;
}

typedef struct worker_args {
    nx_sim_channel *channel;
    uint64_t count;
    int failed;
} worker_args;

static void worker_run(worker_args *args) {
    for (;;) {
        nx_sim_message command = {0};
        if (nx_sim_channel_take_command(args->channel, &command) != NX_OK) {
            args->failed = 1; return;
        }
        if (!command.ticket) {
#if defined(_WIN32)
            Sleep(0);
#else
            sched_yield();
#endif
            continue;
        }
        const payload *data = (const payload *)command.data;
        if (!data || command.bytes != sizeof(*data) ||
            data->value > args->count || data->inverse != ~data->value) {
            args->failed = 1; return;
        }
        uint64_t ticket = command.ticket;
        uint64_t value = data->value;
        nx_sim_message_dispose(&command);
        if (!value) {
            if (nx_sim_channel_post_completion(args->channel, ticket, NX_OK,
                                               NULL, 0, NULL, NULL) != NX_OK)
                args->failed = 1;
            return;
        }
        payload *answer = make_payload(value * 2u);
        if (!answer || nx_sim_channel_post_completion(args->channel, ticket,
                NX_OK, answer, sizeof(*answer), dispose_payload, NULL) != NX_OK) {
            free(answer);
            args->failed = 1; return;
        }
    }
}

#if defined(_WIN32)
static DWORD WINAPI worker_entry(LPVOID user) {
    worker_run((worker_args *)user);
    return 0;
}
static int start_thread(test_thread *thread, worker_args *args) {
    *thread = CreateThread(NULL, 0, worker_entry, args, 0, NULL);
    return *thread != NULL;
}
static int join_thread(test_thread thread) {
    DWORD result = WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return result == WAIT_OBJECT_0;
}
static void yield_thread(void) { Sleep(0); }
#else
static void *worker_entry(void *user) {
    worker_run((worker_args *)user);
    return NULL;
}
static int start_thread(test_thread *thread, worker_args *args) {
    return pthread_create(thread, NULL, worker_entry, args) == 0;
}
static int join_thread(test_thread thread) {
    return pthread_join(thread, NULL) == 0;
}
static void yield_thread(void) { sched_yield(); }
#endif

static int test_thread_handoff(void) {
    const uint64_t count = 10000;
    const uint64_t total = count + 2u;
    nx_sim_channel *channel = NULL;
    CHECK(nx_sim_channel_create(32, NULL, &channel) == NX_OK);
    worker_args args[2] = {{channel, count, 0}, {channel, count, 0}};
    test_thread threads[2];
    CHECK(start_thread(&threads[0], &args[0]));
    CHECK(start_thread(&threads[1], &args[1]));
    uint8_t *seen = (uint8_t *)calloc((size_t)total + 1u, 1u);
    CHECK(seen != NULL);
    uint64_t posted = 0, completed = 0;
    payload *next = NULL;
    while (completed < total) {
        int progressed = 0;
        if (posted < total) {
            if (!next) next = make_payload(posted < count ? posted + 1u : 0u);
            CHECK(next != NULL);
            uint64_t ticket = 0;
            nx_result result = nx_sim_channel_post_command(channel, next,
                sizeof(*next), dispose_payload, NULL, &ticket);
            CHECK(result == NX_OK || result == NX_ERROR_LIMIT);
            if (result == NX_OK) {
                CHECK(ticket == posted + 1u);
                ++posted;
                next = NULL;
                progressed = 1;
            }
        }
        nx_sim_message completion = {0};
        CHECK(nx_sim_channel_take_completion(channel, &completion) == NX_OK);
        if (completion.ticket) {
            CHECK(completion.ticket <= posted && !seen[completion.ticket] &&
                  completion.status == NX_OK);
            seen[completion.ticket] = 1;
            if (completion.ticket <= count)
                CHECK(valid_payload(&completion, completion.ticket * 2u));
            else CHECK(!completion.data);
            nx_sim_message_dispose(&completion);
            ++completed;
            progressed = 1;
        }
        if (!progressed) yield_thread();
    }
    CHECK(join_thread(threads[0]) && join_thread(threads[1]));
    CHECK(!args[0].failed && !args[1].failed);
    for (uint64_t ticket = 1; ticket <= total; ++ticket)
        CHECK(seen[ticket]);
    free(seen);
    CHECK(nx_sim_channel_destroy(channel) == NX_OK);
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

static int test_allocation_failures(void) {
    CHECK(nx_sim_channel_create(1, NULL, NULL) == NX_ERROR_ARGUMENT);
    nx_sim_channel *channel = (nx_sim_channel *)(uintptr_t)1;
    CHECK(nx_sim_channel_create(0, NULL, &channel) == NX_ERROR_LIMIT &&
          channel == NULL);
    CHECK(nx_sim_channel_create(SIZE_MAX, NULL, &channel) == NX_ERROR_LIMIT);
    nx_allocator invalid = {0};
    invalid.allocate = probe_allocate;
    CHECK(nx_sim_channel_create(1, &invalid, &channel) == NX_ERROR_ARGUMENT);
    for (unsigned fail_on = 1; fail_on <= 5; ++fail_on) {
        allocation_probe probe = {0, 0, fail_on};
        nx_allocator allocator = {&probe, probe_allocate, probe_deallocate};
        CHECK(nx_sim_channel_create(2, &allocator, &channel) == NX_ERROR_MEMORY);
        CHECK(channel == NULL && probe.calls == fail_on &&
              probe.frees == fail_on - 1u);
    }
    allocation_probe steady = {0, 0, 6};
    nx_allocator allocator = {&steady, probe_allocate, probe_deallocate};
    CHECK(nx_sim_channel_create(2, &allocator, &channel) == NX_OK);
    payload *data = make_payload(9);
    CHECK(data != NULL);
    uint64_t ticket = 0;
    CHECK(nx_sim_channel_post_command(channel, data, sizeof(*data),
                                      dispose_payload, NULL, &ticket) == NX_OK);
    nx_sim_message message = {0};
    CHECK(nx_sim_channel_take_command(channel, &message) == NX_OK &&
          valid_payload(&message, 9));
    nx_sim_message_dispose(&message);
    CHECK(nx_sim_channel_post_completion(channel, ticket, NX_OK,
                                         NULL, 0, NULL, NULL) == NX_OK);
    CHECK(nx_sim_channel_take_completion(channel, &message) == NX_OK &&
          message.ticket == ticket);
    nx_sim_message_dispose(&message);
    CHECK(nx_sim_channel_destroy(channel) == NX_OK);
    CHECK(steady.calls == 5 && steady.frees == 5);
    return 0;
}

int main(void) {
    if (test_capacity_and_completion_order()) return 1;
    if (test_shutdown_and_validation()) return 1;
    if (test_thread_handoff()) return 1;
    if (test_allocation_failures()) return 1;
    return 0;
}
