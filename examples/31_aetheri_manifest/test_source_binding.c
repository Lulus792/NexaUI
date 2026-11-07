#include "source_binding.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "source-binding:%d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void) {
    char first[] = "first.manifest", second[] = "second.manifest";
    amh_source input[2] = {{first, {0}}, {second, {0}}}; input[0].expected[0] = 11; input[1].expected[0] = 22;
    amh_sources *sources = NULL;
    CHECK(amh_sources_create(input, 2, &sources) == NX_OK && amh_sources_count(sources) == 2);
    memset(first, 'x', sizeof(first) - 1); memset(second, 'y', sizeof(second) - 1); memset(input, 0, sizeof input);
    CHECK(!strcmp(amh_sources_at(sources, 0)->manifest, "first.manifest") && amh_sources_at(sources, 0)->expected[0] == 11);
    CHECK(amh_sources_bind(sources, 101, 0) == NX_OK && amh_sources_bind(sources, 102, 1) == NX_OK);
    CHECK(amh_sources_bind(sources, 101, 1) == NX_ERROR_STATE && amh_sources_bind(sources, 103, 0) == NX_ERROR_LIMIT);
    CHECK(!amh_sources_take(sources, 0) && !amh_sources_take(sources, 103));
    const amh_source *b = amh_sources_take(sources, 102), *a = amh_sources_take(sources, 101);
    CHECK(a && b && a != b && a->expected[0] == 11 && b->expected[0] == 22 &&
        !strcmp(a->manifest, "first.manifest") && !strcmp(b->manifest, "second.manifest"));
    CHECK(!amh_sources_take(sources, 101) && amh_sources_bind(sources, 104, 1) == NX_OK);
    CHECK(amh_sources_bind(sources, 105, 2) == NX_ERROR_ARGUMENT && amh_sources_take(sources, 104) == b);
    CHECK(amh_sources_bind(sources, 0, 0) == NX_ERROR_ARGUMENT && !amh_sources_at(sources, 2));
    amh_sources_destroy(sources); sources = NULL;
    CHECK(amh_sources_create(NULL, 0, &sources) == NX_ERROR_ARGUMENT && !sources);
    amh_source three[3] = {{"a", {1}}, {"b", {2}}, {"c", {3}}};
    CHECK(amh_sources_create(three, 3, &sources) == NX_OK && amh_sources_count(sources) == 3 &&
        amh_sources_bind(sources, 1, 2) == NX_OK && amh_sources_take(sources, 1)->expected[0] == 3);
    amh_sources_destroy(sources);
    puts("SOURCE_BINDING_COPIES_AND_OUT_OF_ORDER_VERIFIED"); return 0;
}
