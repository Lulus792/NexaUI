/* Seed real Mesa CPU state, then observe its actual ELF finalizer. No CPUID or
   topology implementation is substituted; this isolates the ownership hook. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static void (*observe)(int);
static unsigned shutdown_seen, default_fini_seen;
static void cpu_checked_free(void *p) {
    assert(p && shutdown_seen && default_fini_seen);
    free(p);
    observe(2);
}
#define free cpu_checked_free
#include NEXAUI_MESA_CPU_SOURCE
#undef free

static void queue_shutdown(void) {
    assert(util_cpu_caps.L3_affinity_mask && util_cpu_caps.L3_affinity_mask[0][0] == 0xa5);
    shutdown_seen = 1; observe(1);
}
__attribute__((destructor)) static void default_finalizer(void) {
    assert(util_cpu_caps.L3_affinity_mask && util_cpu_caps.L3_affinity_mask[0][0] == 0xa5);
    default_fini_seen = 1; observe(3);
}
__attribute__((visibility("default"))) void cpu_fixture_seed(void (*callback)(int)) {
    observe = callback;
    util_cpu_caps.L3_affinity_mask = calloc(1, sizeof(util_affinity_mask));
    assert(util_cpu_caps.L3_affinity_mask);
    util_cpu_caps.L3_affinity_mask[0][0] = 0xa5;
    _util_cpu_caps_state.caps.L3_affinity_mask = util_cpu_caps.L3_affinity_mask;
    assert(atexit(queue_shutdown) == 0);
}
