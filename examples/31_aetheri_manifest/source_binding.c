#include "source_binding.h"
#include <stdlib.h>
#include <string.h>

struct amh_sources {
    amh_source *items;
    size_t count;
    struct { uint64_t ticket; size_t index; } pending[2];
};
void amh_sources_destroy(amh_sources *s) {
    if (!s) return;
    for (size_t i = 0; i < s->count; ++i) free((void *)s->items[i].manifest);
    free(s->items); free(s);
}
nx_result amh_sources_create(const amh_source *items, size_t count, amh_sources **out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out = NULL;
    if (!items || !count || count > SIZE_MAX / sizeof(*items)) return NX_ERROR_ARGUMENT;
    for (size_t i = 0; i < count; ++i) if (!items[i].manifest || !*items[i].manifest) return NX_ERROR_ARGUMENT;
    amh_sources *s = calloc(1, sizeof(*s));
    if (!s) return NX_ERROR_MEMORY;
    s->items = calloc(count, sizeof(*s->items));
    if (!s->items) { free(s); return NX_ERROR_MEMORY; }
    s->count = count;
    for (size_t i = 0; i < count; ++i) {
        size_t length = strlen(items[i].manifest);
        char *name = malloc(length + 1);
        if (!name) { amh_sources_destroy(s); return NX_ERROR_MEMORY; }
        memcpy(name, items[i].manifest, length + 1); s->items[i].manifest = name;
        memcpy(s->items[i].expected, items[i].expected, 32);
    }
    *out = s; return NX_OK;
}
size_t amh_sources_count(const amh_sources *s) { return s ? s->count : 0; }
const amh_source *amh_sources_at(const amh_sources *s, size_t index) { return s && index < s->count ? &s->items[index] : NULL; }
nx_result amh_sources_bind(amh_sources *s, uint64_t ticket, size_t index) {
    if (!s || !ticket || index >= s->count) return NX_ERROR_ARGUMENT;
    for (size_t i = 0; i < 2; ++i) if (s->pending[i].ticket == ticket) return NX_ERROR_STATE;
    for (size_t i = 0; i < 2; ++i) if (!s->pending[i].ticket) {
        s->pending[i].ticket = ticket; s->pending[i].index = index; return NX_OK;
    }
    return NX_ERROR_LIMIT;
}
const amh_source *amh_sources_take(amh_sources *s, uint64_t ticket) {
    if (!s || !ticket) return NULL;
    for (size_t i = 0; i < 2; ++i) if (s->pending[i].ticket == ticket) {
        size_t index = s->pending[i].index; s->pending[i].ticket = 0;
        return &s->items[index];
    }
    return NULL;
}
