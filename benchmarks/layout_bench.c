#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct accounting {
    size_t alloc_calls, free_calls, live_bytes, peak_bytes;
} accounting;

static void *count_allocate(void *user, size_t size) {
    accounting *a = (accounting *)user;
    void *pointer = malloc(size);
    if (pointer) {
        ++a->alloc_calls;
        a->live_bytes += size;
        if (a->live_bytes > a->peak_bytes) a->peak_bytes = a->live_bytes;
    }
    return pointer;
}

static void count_deallocate(void *user, void *pointer, size_t size) {
    accounting *a = (accounting *)user;
    if (pointer) {
        ++a->free_calls;
        a->live_bytes -= size;
        free(pointer);
    }
}

static double now_ms(void) {
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) != TIME_UTC) return -1.0;
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static int run(size_t count) {
    accounting stats = {0};
    nx_allocator allocator = {&stats, count_allocate, count_deallocate};
    nx_layout *layout = NULL;
    if (nx_layout_create(&allocator, count + 1u, &layout) != NX_OK) return 1;
    nx_layout_desc root_desc = nx_layout_default_desc(NX_LAYOUT_ROW);
    nx_layout_desc leaf_desc = nx_layout_default_desc(NX_LAYOUT_LEAF);
    leaf_desc.width.has_preferred = leaf_desc.height.has_preferred = 1;
    leaf_desc.width.preferred = 10;
    leaf_desc.height.preferred = 14;
    uint32_t root, child;
    if (nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &root_desc, &root) != NX_OK)
        return 1;
    for (size_t i = 0; i < count; ++i)
        if (nx_layout_add(layout, root, &leaf_desc, &child) != NX_OK) return 1;
    nx_size desired;
    if (nx_layout_measure(layout, root, 1920, 1080, &desired) != NX_OK ||
        nx_layout_arrange(layout, root, (nx_rect){0, 0, 1920, 1080}) != NX_OK)
        return 1;
    size_t warm_allocations = stats.alloc_calls;
    double start = now_ms();
    if (start < 0) return 1;
    const unsigned iterations = 20;
    for (unsigned i = 0; i < iterations; ++i)
        if (nx_layout_measure(layout, root, 1920, 1080, &desired) != NX_OK ||
            nx_layout_arrange(layout, root, (nx_rect){0, 0, 1920, 1080}) != NX_OK)
            return 1;
    double elapsed = now_ms() - start;
    printf("%zu nodes: %.3f ms/measure+arrange, %zu allocations in steady state, peak %zu bytes\n",
           count, elapsed / iterations, stats.alloc_calls - warm_allocations,
           stats.peak_bytes);
    nx_layout_destroy(layout);
    if (stats.live_bytes || stats.alloc_calls != stats.free_calls) return 1;
    return 0;
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
