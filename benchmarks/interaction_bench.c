#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct accounting { size_t calls, frees, live, peak; } accounting;

static void *count_allocate(void *user, size_t size) {
    accounting *a = (accounting *)user;
    void *pointer = malloc(size);
    if (pointer) {
        ++a->calls;
        a->live += size;
        if (a->live > a->peak) a->peak = a->live;
    }
    return pointer;
}

static void count_deallocate(void *user, void *pointer, size_t size) {
    accounting *a = (accounting *)user;
    if (pointer) {
        ++a->frees;
        a->live -= size;
        free(pointer);
    }
}

static double now_ms(void) {
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) != TIME_UTC) return -1;
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static int frame(nx_context *ui, size_t count) {
    if (nx_begin_frame(ui, 1920, 1080, 1.0 / 60.0) != NX_OK) return 0;
    for (size_t i = 0; i < count; ++i) {
        nx_rect rect = {(float)(i % 100u) * 20.0f,
                        (float)(i / 100u) * 20.0f, 18, 18};
        nx_interaction_state state;
        if (nx_interact_region(ui, (uint64_t)i + 1u, rect,
                               NX_REGION_FOCUSABLE, &state) != NX_OK) return 0;
    }
    return nx_end_frame(ui) == NX_OK;
}

static int run(size_t count) {
    accounting a = {0};
    nx_context_desc desc = {0};
    desc.allocator = (nx_allocator){&a, count_allocate, count_deallocate};
    desc.initial_region_capacity = count;
    nx_context *ui = NULL;
    if (nx_context_create(&desc, &ui) != NX_OK) return 1;
    if (!frame(ui, count) || !frame(ui, count)) return 1;
    size_t warmed_calls = a.calls;
    double start = now_ms();
    if (start < 0) return 1;
    const unsigned iterations = 20;
    for (unsigned i = 0; i < iterations; ++i)
        if (!frame(ui, count)) return 1;
    double elapsed = now_ms() - start;
    printf("%zu regions: %.3f ms/frame, %zu allocations in steady state, peak %zu bytes\n",
           count, elapsed / iterations, a.calls - warmed_calls, a.peak);
    nx_context_destroy(ui);
    return a.live || a.calls != a.frees;
}

int main(void) {
#if defined(_MSC_VER)
    printf("compiler=MSVC %d ", _MSC_VER);
#elif defined(__clang__)
    printf("compiler=Clang %s ", __clang_version__);
#elif defined(__GNUC__)
    printf("compiler=GCC %s ", __VERSION__);
#endif
#if defined(_WIN32)
    puts("os=Windows");
#elif defined(__APPLE__)
    puts("os=macOS");
#elif defined(__linux__)
    puts("os=Linux");
#else
    puts("os=unknown");
#endif
    if (run(1000) || run(10000) || run(100000)) return 1;
    return 0;
}
