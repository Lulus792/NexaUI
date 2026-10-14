#include "nexaui/nexaui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

typedef struct test_allocator {
    unsigned calls;
    unsigned frees;
    unsigned fail_after;
} test_allocator;

static void *test_allocate(void *user, size_t size) {
    test_allocator *state = (test_allocator *)user;
    ++state->calls;
    if (state->fail_after && state->calls > state->fail_after) return NULL;
    return malloc(size);
}

static void test_deallocate(void *user, void *pointer, size_t size) {
    test_allocator *state = (test_allocator *)user;
    (void)size;
    ++state->frees;
    free(pointer);
}

static int test_context_and_ids(void) {
    nx_context *a = NULL, *b = NULL;
    CHECK(nx_context_create(NULL, &a) == NX_OK);
    CHECK(nx_context_create(NULL, &b) == NX_OK);
    CHECK(a && b && a != b);
    CHECK(nx_end_frame(a) == NX_ERROR_STATE);
    CHECK(nx_begin_frame(a, 20, 10, 0.01) == NX_OK);
    CHECK(nx_begin_frame(a, 20, 10, 0.01) == NX_ERROR_STATE);
    uint64_t root_id = nx_id(a, "item", 4);
    CHECK(root_id != 0);
    CHECK(nx_push_id(a, "panel", 5) == NX_OK);
    uint64_t child_id = nx_id(a, "item", 4);
    CHECK(child_id != 0 && child_id != root_id);
    CHECK(nx_end_frame(a) == NX_ERROR_STATE);
    CHECK(nx_pop_id(a) == NX_OK);
    CHECK(nx_pop_id(a) == NX_ERROR_STATE);
    CHECK(nx_end_frame(a) == NX_OK);
    CHECK(nx_begin_frame(a, 20, 10, 0.01) == NX_OK);
    CHECK(nx_id(a, "item", 4) == root_id);
    CHECK(nx_begin_frame(b, 20, 10, 0.01) == NX_OK);
    CHECK(nx_id(b, "item", 4) == root_id);
    CHECK(nx_end_frame(b) == NX_OK);
    CHECK(nx_end_frame(a) == NX_OK);
    nx_context_destroy(a);
    nx_context_destroy(b);
    return 0;
}

static int test_commands_and_pixels(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 4, 2, 0.0) == NX_OK);
    CHECK(nx_fill_rect(ctx, (nx_rect){0, 0, 4, 2}, (nx_color){1, 0, 0, 1}) == NX_OK);
    CHECK(nx_push_clip(ctx, (nx_rect){1, 0, 2, 2}) == NX_OK);
    CHECK(nx_fill_rect(ctx, (nx_rect){0, 0, 4, 2}, (nx_color){0, 0, 1, 1}) == NX_OK);
    CHECK(nx_pop_clip(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    const nx_command *commands = NULL;
    size_t count = 0;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count == 4);
    uint8_t pixels[32] = {0};
    CHECK(nx_headless_render(commands, count, pixels, 4, 2, 16,
                             (nx_color){0, 0, 0, 0}) == NX_OK);
    const uint8_t red[4] = {255, 0, 0, 255};
    const uint8_t blue[4] = {0, 0, 255, 255};
    CHECK(memcmp(pixels, red, 4) == 0);
    CHECK(memcmp(pixels + 4, blue, 4) == 0);
    CHECK(memcmp(pixels + 8, blue, 4) == 0);
    CHECK(memcmp(pixels + 12, red, 4) == 0);
    CHECK(memcmp(pixels + 16, red, 4) == 0);
    nx_context_destroy(ctx);
    return 0;
}

static int test_validation_and_failure(void) {
    nx_context *ctx = NULL;
    nx_context_desc bad = {0};
    bad.allocator.allocate = test_allocate;
    CHECK(nx_context_create(&bad, &ctx) == NX_ERROR_ARGUMENT && !ctx);
    test_allocator state = {0};
    state.fail_after = 2;
    nx_context_desc desc = {{&state, test_allocate, test_deallocate}, 1};
    CHECK(nx_context_create(&desc, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, NAN, 1, 0) == NX_ERROR_ARGUMENT);
    CHECK(nx_begin_frame(ctx, 1, 1, 0) == NX_OK);
    CHECK(nx_fill_rect(ctx, (nx_rect){0, 0, 1, 1}, (nx_color){1, 1, 1, 1}) == NX_OK);
    CHECK(nx_fill_rect(ctx, (nx_rect){0, 0, 1, 1}, (nx_color){1, 1, 1, 1}) == NX_ERROR_MEMORY);
    CHECK(nx_fill_rect(ctx, (nx_rect){0, 0, -1, 1}, (nx_color){1, 1, 1, 1}) == NX_ERROR_ARGUMENT);
    CHECK(nx_end_frame(ctx) == NX_OK);
    const nx_command *commands;
    size_t count;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count == 1);
    nx_context_destroy(ctx);
    CHECK(state.frees == 2);

    uint8_t pixel[4] = {7, 8, 9, 10};
    nx_command malformed = {NX_COMMAND_POP_CLIP, {0}, {0}};
    CHECK(nx_headless_render(&malformed, 1, pixel, 1, 1, 4,
                             (nx_color){0, 0, 0, 0}) == NX_ERROR_COMMAND);
    CHECK(pixel[0] == 7 && pixel[1] == 8 && pixel[2] == 9 && pixel[3] == 10);
    CHECK(nx_headless_render(NULL, 0, pixel, 1, 1, 3,
                             (nx_color){0, 0, 0, 0}) == NX_ERROR_ARGUMENT);
    return 0;
}

static int test_alpha(void) {
    nx_command command = {NX_COMMAND_FILL_RECT, {0, 0, 1, 1}, {1, 0, 0, 0.5f}};
    uint8_t pixel[4] = {0};
    CHECK(nx_headless_render(&command, 1, pixel, 1, 1, 4,
                             (nx_color){0, 0, 1, 1}) == NX_OK);
    CHECK(pixel[0] == 128 && pixel[1] == 0 && pixel[2] == 127 && pixel[3] == 255);
    return 0;
}

static int test_utf8(void) {
    const char text[] = "A\xc3\xa4\xf0\x9f\x8c\x8d";
    size_t offset = 0;
    uint32_t scalar = 0;
    CHECK(nx_utf8_decode(text, sizeof(text) - 1u, &offset, &scalar) == NX_OK);
    CHECK(offset == 1 && scalar == 'A');
    CHECK(nx_utf8_decode(text, sizeof(text) - 1u, &offset, &scalar) == NX_OK);
    CHECK(offset == 3 && scalar == 0xe4u);
    CHECK(nx_utf8_decode(text, sizeof(text) - 1u, &offset, &scalar) == NX_OK);
    CHECK(offset == 7 && scalar == 0x1f30du);
    CHECK(nx_utf8_validate(text, sizeof(text) - 1u, &offset) == NX_OK && offset == 7);
    const char overlong[] = "\xc0\x80";
    offset = 0;
    CHECK(nx_utf8_validate(overlong, 2, &offset) == NX_ERROR_ARGUMENT && offset == 0);
    const char surrogate[] = "\xed\xa0\x80";
    CHECK(nx_utf8_validate(surrogate, 3, &offset) == NX_ERROR_ARGUMENT && offset == 0);
    const char too_high[] = "\xf4\x90\x80\x80";
    CHECK(nx_utf8_validate(too_high, 4, &offset) == NX_ERROR_ARGUMENT && offset == 0);
    const char truncated[] = "x\xe2\x82";
    CHECK(nx_utf8_validate(truncated, 3, &offset) == NX_ERROR_ARGUMENT && offset == 1);
    CHECK(nx_utf8_decode(truncated, 3, &offset, &scalar) == NX_ERROR_ARGUMENT && offset == 1);
    CHECK(nx_utf8_validate(NULL, 0, &offset) == NX_OK && offset == 0);
    return 0;
}

static int test_arena(void) {
    test_allocator allocator_state = {0};
    nx_allocator allocator = {&allocator_state, test_allocate, test_deallocate};
    nx_arena *arena = NULL;
    CHECK(nx_arena_create(&allocator, 64, &arena) == NX_OK);
    CHECK(nx_arena_capacity(arena) == 64 && nx_arena_used(arena) == 0);
    void *first = NULL, *second = NULL;
    CHECK(nx_arena_alloc(arena, 9, 8, &first) == NX_OK);
    CHECK(nx_arena_alloc(arena, 8, 8, &second) == NX_OK);
    CHECK((unsigned char *)second - (unsigned char *)first == 16);
    CHECK(nx_arena_used(arena) == 24);
    CHECK(nx_arena_alloc(arena, 64, 8, &second) == NX_ERROR_LIMIT && !second);
    CHECK(nx_arena_alloc(arena, 1, 3, &second) == NX_ERROR_ARGUMENT && !second);
    nx_arena_reset(arena);
    CHECK(nx_arena_used(arena) == 0);
    CHECK(nx_arena_alloc(arena, 9, 8, &second) == NX_OK && second == first);
    CHECK(allocator_state.calls == 2);
    nx_arena_destroy(arena);
    CHECK(allocator_state.frees == 2);
    return 0;
}

static int test_window_lifecycle(void) {
#if defined(_WIN32) || defined(__linux__)
    nx_window *window = NULL;
    nx_window_desc desc = {0};
    desc.title_utf8 = "NexaUI \xc3\xbc";
    desc.width = 320;
    desc.height = 200;
    desc.flags = NX_WINDOW_HIDDEN;
    CHECK(nx_window_create(&desc, NULL) == NX_ERROR_ARGUMENT);
    desc.title_utf8 = "\xc0\x80";
    CHECK(nx_window_create(&desc, &window) == NX_ERROR_ARGUMENT && !window);
    desc.title_utf8 = "NexaUI \xc3\xbc";
#if defined(__linux__)
    const char *display = getenv("DISPLAY");
    const char *wayland_display = getenv("WAYLAND_DISPLAY");
    if ((!display || !*display) &&
        (!wayland_display || !*wayland_display)) {
        CHECK(nx_window_create(&desc, &window) == NX_ERROR_PLATFORM && !window);
        return 0;
    }
#endif
    CHECK(nx_window_create(&desc, &window) == NX_OK && window);
    nx_window_info info = {0};
    CHECK(nx_window_get_info(window, &info) == NX_OK);
    CHECK(info.width > 0 && info.height > 0 && info.scale > 0);
    CHECK(!info.should_close);
    CHECK(nx_window_poll(window, 0) == NX_OK);
    nx_window_destroy(window);
#else
    nx_window *window = NULL;
    nx_window_desc desc = {0};
    desc.width = 320;
    desc.height = 200;
    CHECK(nx_window_create(&desc, &window) == NX_ERROR_UNSUPPORTED && !window);
#endif
    return 0;
}

int main(void) {
    if (test_context_and_ids()) return 1;
    if (test_commands_and_pixels()) return 1;
    if (test_validation_and_failure()) return 1;
    if (test_alpha()) return 1;
    if (test_utf8()) return 1;
    if (test_arena()) return 1;
    if (test_window_lifecycle()) return 1;
    puts("nexaui tests passed");
    return 0;
}
