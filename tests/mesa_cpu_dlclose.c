#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned shutdown_seen, default_fini_seen, freed;
static void observe(int event) {
    if (event == 1) { assert(!freed); ++shutdown_seen; }
    else if (event == 3) { assert(!freed); ++default_fini_seen; }
    else if (event == 2) { assert(shutdown_seen == 1 && default_fini_seen == 1 && !freed); ++freed; }
    else abort();
    printf("CPU_EVENT %d\n", event); fflush(stdout);
}
int main(int argc, char **argv) {
    assert(argc == 2 || (argc == 3 && strcmp(argv[2], "--exit-scope") == 0));
    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { fprintf(stderr, "%s\n", dlerror()); return 2; }
    void *symbol = dlsym(library, "cpu_fixture_seed"); assert(symbol);
    void (*seed)(void (*)(int)); _Static_assert(sizeof(seed) == sizeof(symbol), "POSIX dlsym");
    memcpy(&seed, &symbol, sizeof(seed)); seed(observe);
    if (argc == 3) { puts("CPU fixture: checking process-exit scope"); return 0; }
    assert(dlclose(library) == 0);
    printf("CPU_CLOSE shutdown=%u default_fini=%u freed=%u\n", shutdown_seen, default_fini_seen, freed);
    if (!shutdown_seen && !default_fini_seen) return 77; /* libc did not unload; not a passing test */
    return shutdown_seen == 1 && default_fini_seen == 1 && freed == 1 ? 0 : 1;
}
