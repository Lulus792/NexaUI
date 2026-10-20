/* Exercise the actual pinned Mesa allocator and u_mm implementation. Test-only
   wrappers count/fail real calloc/mmap/munmap calls; no allocator substitute. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static void *allocations[4096];
static size_t live_allocations, live_maps, calloc_calls, fail_calloc_at;
static int fail_mmap_once, fail_munmap_once;
static void *last_mapping;
static size_t last_mapping_size;

static void *counted_calloc(size_t n, size_t size) {
    ++calloc_calls;
    if (calloc_calls == fail_calloc_at) { errno = ENOMEM; return NULL; }
    void *p = calloc(n, size);
    if (p) {
        size_t i = 0;
        while (i < 4096 && allocations[i]) ++i;
        assert(i < 4096); allocations[i] = p; ++live_allocations;
    }
    return p;
}
static void counted_free(void *p) {
    if (p) {
        size_t i = 0;
        while (i < 4096 && allocations[i] != p) ++i;
        assert(i < 4096 && live_allocations); allocations[i] = NULL; --live_allocations;
    }
    free(p);
}
static void *counted_mmap(void *p, size_t n, int prot, int flags, int fd, off_t offset) {
    if (fail_mmap_once) { fail_mmap_once = 0; errno = ENOMEM; return MAP_FAILED; }
    void *result = mmap(p, n, prot, flags, fd, offset);
    if (result != MAP_FAILED) {
        assert(live_maps == 0); ++live_maps; last_mapping = result; last_mapping_size = n;
    }
    return result;
}
static int counted_munmap(void *p, size_t n) {
    assert(live_maps == 1 && p == last_mapping && n == last_mapping_size);
    if (fail_munmap_once) { fail_munmap_once = 0; errno = ENOMEM; return -1; }
    int result = munmap(p, n);
    if (result == 0) --live_maps;
    return result;
}

#define calloc counted_calloc
#define free counted_free
#define mmap counted_mmap
#define munmap counted_munmap
#include NEXAUI_MESA_MM_SOURCE
#include NEXAUI_MESA_RTASM_SOURCE
#undef calloc
#undef free
#undef mmap
#undef munmap

static void idle(void) {
    assert(live_allocations == 0 && live_maps == 0);
    assert(exec_heap == NULL && exec_mem == NULL);
    if (last_mapping) {
        unsigned char residency[4096]; errno = 0;
        assert(mincore(last_mapping, last_mapping_size, residency) == -1 && errno == ENOMEM);
    }
}
static void execute(void) {
    unsigned char *p = rtasm_exec_malloc(64);
    assert(p && (uintptr_t)p % 32 == 0);
#if defined(__x86_64__)
    const unsigned char code[] = {0xb8,42,0,0,0,0xc3};
    /* UBSan may inspect the bytes immediately before an indirect-call target
       to distinguish instrumented functions. Keep that read inside this mapping. */
    memset(p, 0, 64); unsigned char *entry = p + 32;
    memcpy(entry, code, sizeof(code));
    __builtin___clear_cache((char *)entry, (char *)entry + sizeof(code));
    int (*function)(void); _Static_assert(sizeof(function) == sizeof(p), "native function pointer");
    memcpy(&function, &entry, sizeof(function)); assert(function() == 42);
#else
    memset(p, 42, 64); assert(p[63] == 42);
#endif
    rtasm_exec_free(p); idle();
}
static void *worker(void *input) {
    unsigned char byte = (unsigned char)(uintptr_t)input;
    for (unsigned run = 0; run < 64; ++run) {
        unsigned char *a = rtasm_exec_malloc(96), *b = rtasm_exec_malloc(160);
        assert(a && b); memset(a, byte, 96); memset(b, byte + 1, 160);
        rtasm_exec_free(a);
        for (unsigned i = 0; i < 160; ++i) assert(b[i] == (unsigned char)(byte + 1));
        rtasm_exec_free(b);
    }
    return NULL;
}
int main(void) {
    idle(); execute();
    void *a = rtasm_exec_malloc(64), *b = rtasm_exec_malloc(128);
    assert(a && b); memset(b, 0x5a, 128); rtasm_exec_free(a);
    assert(live_maps == 1 && exec_heap && exec_mem);
    for (unsigned i = 0; i < 128; ++i) assert(((unsigned char *)b)[i] == 0x5a);
    rtasm_exec_free(b); idle();
    assert(rtasm_exec_malloc(0) == NULL); assert(rtasm_exec_malloc(SIZE_MAX) == NULL);
    rtasm_exec_free(NULL); idle();
    for (size_t nth = 1; nth <= 3; ++nth) {
        fail_calloc_at = calloc_calls + nth;
        assert(rtasm_exec_malloc(64) == NULL); fail_calloc_at = 0; idle(); execute();
    }
    fail_mmap_once = 1; assert(rtasm_exec_malloc(64) == NULL); idle(); execute();
    a = rtasm_exec_malloc(64); assert(a); fail_calloc_at = calloc_calls + 1;
    assert(rtasm_exec_malloc(64) == NULL); fail_calloc_at = 0;
    assert(live_maps == 1); rtasm_exec_free(a); idle();
    a = rtasm_exec_malloc(64); assert(a); fail_munmap_once = 1; rtasm_exec_free(a);
    assert(live_maps == 1 && exec_heap && exec_mem);
    b = rtasm_exec_malloc(64); assert(b); rtasm_exec_free(b); idle();
    pthread_t threads[8];
    for (uintptr_t i = 0; i < 8; ++i) assert(pthread_create(&threads[i], NULL, worker, (void *)(i + 1)) == 0);
    for (unsigned i = 0; i < 8; ++i) assert(pthread_join(threads[i], NULL) == 0);
    idle();
    puts("Mesa rtasm lifetime: real executable memory, live-owner preservation, OOM/retry, unmap retry, 8x64 concurrent ownership pairs; live allocations/maps=0");
    return 0;
}
