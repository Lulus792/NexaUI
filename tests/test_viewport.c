#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; \
} } while (0)

typedef struct probe {
    uint8_t pixels[64u * 64u * 4u];
    uint32_t width, height;
    unsigned resizes, acquires, releases;
    int in_use, fail_resize, fail_acquire, bad_bitmap;
} probe;

static nx_result resize_surface(void *user, uint32_t width, uint32_t height) {
    probe *p = user;
    ++p->resizes;
    if (p->fail_resize) return NX_ERROR_PLATFORM;
    if (width > 64 || height > 64 || p->in_use) return NX_ERROR_STATE;
    p->width = width;
    p->height = height;
    for (size_t i = 0; i < (size_t)width * height; ++i) {
        p->pixels[i * 4u + 0u] = 20;
        p->pixels[i * 4u + 1u] = 80;
        p->pixels[i * 4u + 2u] = 180;
        p->pixels[i * 4u + 3u] = 255;
    }
    return NX_OK;
}

static nx_result acquire_surface(void *user, nx_bitmap_view *out) {
    probe *p = user;
    ++p->acquires;
    if (p->fail_acquire) return NX_ERROR_PLATFORM;
    if (p->in_use || !p->width || !p->height) return NX_ERROR_STATE;
    p->in_use = 1;
    *out = (nx_bitmap_view){p->pixels,
        p->width + (unsigned)p->bad_bitmap, p->height,
        (size_t)p->width * 4u, NX_BITMAP_RGBA8};
    return NX_OK;
}

static void release_surface(void *user) {
    probe *p = user;
    ++p->releases;
    p->in_use = 0;
}

static nx_viewport_desc viewport_desc(void) {
    nx_viewport_desc desc = {0};
    desc.id = 101;
    desc.bounds = (nx_rect){10, 10, 40, 30};
    desc.name = (nx_utf8_span){"3D scene", 8};
    desc.scale = 1.5;
    return desc;
}

static int test_lifecycle(void) {
    probe p = {0};
    nx_viewport_source source = {&p, resize_surface, acquire_surface,
                                  release_surface};
    nx_viewport_surface *surface = NULL;
    nx_context *ctx = NULL;
    CHECK(nx_viewport_surface_create(&source, NULL, &surface) == NX_OK);
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_viewport_desc desc = viewport_desc();
    nx_interaction_state state;
    CHECK(nx_begin_frame(ctx, 80, 60, 0) == NX_OK);
    CHECK(nx_viewport(ctx, surface, &desc, &state) == NX_OK);
    CHECK(p.width == 60 && p.height == 45 && p.resizes == 1 &&
          p.acquires == 1 && p.in_use);
    CHECK(nx_viewport(ctx, surface, &desc, &state) == NX_ERROR_STATE);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_viewport_surface_destroy(surface) == NX_ERROR_STATE);
    uint32_t index = UINT32_MAX;
    nx_semantic_view semantic;
    CHECK(nx_semantics_find(ctx, desc.id, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &semantic) == NX_OK &&
          semantic.role == NX_ROLE_CUSTOM &&
          (semantic.actions & NX_ACTION_FOCUS));
    const nx_command *commands = NULL;
    size_t count = 0;
    uint8_t output[80u * 60u * 4u];
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count >= 3);
    CHECK(nx_headless_render(commands, count, output, 80, 60, 80u * 4u,
                             (nx_color){0, 0, 0, 1}) == NX_OK);
    size_t center = ((size_t)25 * 80u + 30u) * 4u;
    CHECK(output[center] == 20 && output[center + 1u] == 80 &&
          output[center + 2u] == 180);
    CHECK(nx_viewport_surface_complete(surface) == NX_OK &&
          p.releases == 1 && !p.in_use);
    CHECK(nx_viewport_surface_complete(surface) == NX_ERROR_STATE);
    CHECK(nx_begin_frame(ctx, 80, 60, 0) == NX_OK);
    CHECK(nx_viewport(ctx, surface, &desc, &state) == NX_OK);
    CHECK(p.resizes == 1 && p.acquires == 2);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_viewport_surface_complete(surface) == NX_OK);
    desc.scale = 1.0;
    CHECK(nx_begin_frame(ctx, 80, 60, 0) == NX_OK);
    CHECK(nx_viewport(ctx, surface, &desc, &state) == NX_OK);
    CHECK(p.width == 40 && p.height == 30 && p.resizes == 2);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_viewport_surface_complete(surface) == NX_OK);
    desc.bounds.width = 0;
    CHECK(nx_begin_frame(ctx, 80, 60, 0) == NX_OK);
    CHECK(nx_viewport(ctx, surface, &desc, &state) == NX_OK);
    CHECK(p.width == 0 && p.height == 0 && p.resizes == 3 &&
          p.acquires == 3);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 0);
    desc.bounds.width = 40;
    CHECK(nx_begin_frame(ctx, 80, 60, 0) == NX_OK);
    CHECK(nx_viewport(ctx, surface, &desc, &state) == NX_OK);
    CHECK(p.resizes == 4 && p.acquires == 4);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_viewport_surface_complete(surface) == NX_OK);
    CHECK(nx_viewport_surface_destroy(surface) == NX_OK);
    nx_context_destroy(ctx);
    return 0;
}

static int test_failures(void) {
    probe p = {0};
    nx_viewport_source source = {&p, resize_surface, acquire_surface,
                                  release_surface};
    nx_viewport_surface *surface = NULL;
    nx_context *ctx = NULL;
    CHECK(nx_viewport_surface_create(&source, NULL, &surface) == NX_OK);
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_viewport_desc desc = viewport_desc();
    nx_interaction_state state;
    size_t count;
    CHECK(nx_begin_frame(ctx, 80, 60, 0) == NX_OK);
    p.fail_resize = 1;
    CHECK(nx_viewport(ctx, surface, &desc, &state) == NX_ERROR_PLATFORM);
    p.fail_resize = 0;
    p.fail_acquire = 1;
    CHECK(nx_viewport(ctx, surface, &desc, &state) == NX_ERROR_PLATFORM);
    CHECK(p.resizes == 2 && p.acquires == 1 && p.releases == 0);
    p.fail_acquire = 0;
    p.bad_bitmap = 1;
    CHECK(nx_viewport(ctx, surface, &desc, &state) == NX_ERROR_ARGUMENT);
    CHECK(p.releases == 1 && !p.in_use);
    p.bad_bitmap = 0;
    CHECK(nx_viewport(ctx, surface, &desc, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 1);
    CHECK(nx_viewport_surface_complete(surface) == NX_OK);
    desc.bounds.width = 20000;
    CHECK(nx_begin_frame(ctx, 80, 60, 0) == NX_OK);
    CHECK(nx_viewport(ctx, surface, &desc, &state) == NX_ERROR_LIMIT);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_viewport_surface_destroy(surface) == NX_OK);
    nx_context_destroy(ctx);
    return 0;
}

typedef struct allocation_probe {
    unsigned allocations, frees;
    int fail;
} allocation_probe;

static void *allocate(void *user, size_t size) {
    allocation_probe *p = user;
    ++p->allocations;
    return p->fail && p->allocations == (unsigned)p->fail ? NULL : malloc(size);
}

static void deallocate(void *user, void *pointer, size_t size) {
    allocation_probe *p = user;
    (void)size;
    ++p->frees;
    free(pointer);
}

static int test_create_failure(void) {
    allocation_probe p = {0};
    nx_allocator allocator = {&p, allocate, deallocate};
    probe image = {0};
    nx_viewport_source source = {&image, resize_surface, acquire_surface,
                                  release_surface};
    nx_viewport_surface *surface = (nx_viewport_surface *)1;
    p.fail = 1;
    CHECK(nx_viewport_surface_create(&source, &allocator, &surface) ==
          NX_ERROR_MEMORY && surface == NULL);
    p.fail = 0;
    CHECK(nx_viewport_surface_create(&source, &allocator, &surface) == NX_OK);
    CHECK(nx_viewport_surface_destroy(surface) == NX_OK);
    CHECK(p.allocations == 2 && p.frees == 1);
    return 0;
}

static int test_allocation_retry(void) {
    unsigned failures = 0, released_after_acquire = 0;
    for (unsigned offset = 1; offset <= 16; ++offset) {
        allocation_probe allocations = {0};
        nx_allocator allocator = {&allocations, allocate, deallocate};
        nx_context_desc context_desc = {0};
        context_desc.allocator = allocator;
        context_desc.initial_command_capacity = 1;
        nx_context *ctx = NULL;
        CHECK(nx_context_create(&context_desc, &ctx) == NX_OK);
        probe p = {0};
        nx_viewport_source source = {&p, resize_surface, acquire_surface,
                                      release_surface};
        nx_viewport_surface *surface = NULL;
        CHECK(nx_viewport_surface_create(&source, &allocator, &surface) == NX_OK);
        CHECK(nx_begin_frame(ctx, 80, 60, 0) == NX_OK);
        nx_viewport_desc desc = viewport_desc();
        nx_interaction_state state;
        unsigned prior_acquires = p.acquires, prior_releases = p.releases;
        unsigned fail_at = allocations.allocations + offset;
        allocations.fail = (int)fail_at;
        nx_result result = nx_viewport(ctx, surface, &desc, &state);
        if (result == NX_ERROR_MEMORY) {
            ++failures;
            CHECK(!p.in_use);
            if (p.acquires > prior_acquires) {
                CHECK(p.releases == prior_releases + 1u);
                ++released_after_acquire;
            }
            allocations.fail = 0;
            CHECK(nx_viewport(ctx, surface, &desc, &state) == NX_OK);
        } else CHECK(result == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        size_t count = 0;
        CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 1);
        CHECK(nx_viewport_surface_complete(surface) == NX_OK);
        CHECK(nx_viewport_surface_destroy(surface) == NX_OK);
        nx_context_destroy(ctx);
        CHECK(allocations.frees == allocations.allocations -
              (result == NX_ERROR_MEMORY ? 1u : 0u));
    }
    CHECK(failures >= 4 && released_after_acquire >= 1);
    return 0;
}

int main(void) {
    return test_lifecycle() || test_failures() || test_create_failure() ||
           test_allocation_retry();
}
