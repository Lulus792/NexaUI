#ifndef EXAMPLE_AETHERI_SOURCE_BINDING_H
#define EXAMPLE_AETHERI_SOURCE_BINDING_H
#include "manifest_source.h"

typedef struct amh_source { const char *manifest; unsigned char expected[32]; } amh_source;
typedef struct amh_sources amh_sources;
/* Own copies of source paths/digests. No filesystem, UI or permission decisions.
   Items stay immutable/alive until all workers have joined and destroy is called. */
nx_result amh_sources_create(const amh_source *sources, size_t count, amh_sources **out);
void amh_sources_destroy(amh_sources *sources);
size_t amh_sources_count(const amh_sources *sources);
const amh_source *amh_sources_at(const amh_sources *sources, size_t index);
/* Caller serializes bind/take with its producer mutex. Bind before waking the
   worker; it takes the exact ticket's immutable source once, then may borrow it
   without a lock. Two bindings match the host's existing channel capacity.
   A later UI selection must never change a taken source. */
nx_result amh_sources_bind(amh_sources *sources, uint64_t ticket, size_t index);
const amh_source *amh_sources_take(amh_sources *sources, uint64_t ticket);
#endif
