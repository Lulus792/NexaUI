#include "nexaui/nexaui.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static int test_spacer(void) {
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    uint32_t node = UINT32_MAX;
    CHECK(nx_layout_spacer(layout, NX_LAYOUT_NO_PARENT, -1, 2,
                           &node) == NX_ERROR_ARGUMENT);
    CHECK(nx_layout_spacer(layout, NX_LAYOUT_NO_PARENT, 3, 2,
                           &node) == NX_OK);
    nx_size desired = {0};
    CHECK(nx_layout_measure(layout, node, 100, 100, &desired) == NX_OK);
    CHECK(desired.width == 3 && desired.height == 2);
    CHECK(nx_layout_arrange(layout, node, (nx_rect){0, 0, 3, 2}) == NX_OK);
    nx_rect rect;
    CHECK(nx_layout_get_rect(layout, node, &rect) == NX_OK);
    CHECK(rect.width == 3 && rect.height == 2);
    nx_layout_destroy(layout);
    return 0;
}

static int test_image_separator_and_rollback(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    uint8_t bitmap_pixels[8] = {255, 0, 0, 255, 0, 255, 0, 255};
    nx_bitmap_view bitmap = {bitmap_pixels, 2, 1, 8, NX_BITMAP_RGBA8};
    nx_utf8_span name = {"Logo", 4};
    nx_color white = {1, 1, 1, 1};
    CHECK(nx_begin_frame(ctx, 4, 3, 0) == NX_OK);
    nx_bitmap_view invalid = bitmap;
    invalid.stride_bytes = 7;
    CHECK(nx_image(ctx, 11, 0, (nx_rect){0, 0, 4, 2}, name,
                   invalid, white) == NX_ERROR_ARGUMENT);
    CHECK(nx_image(ctx, 11, 0, (nx_rect){0, 0, 4, 2}, name,
                   bitmap, white) == NX_OK);
    CHECK(nx_separator(ctx, 12, 0, (nx_rect){0, 2, 4, 1},
                       (nx_color){2, 0, 0, 1}) == NX_ERROR_ARGUMENT);
    CHECK(nx_separator(ctx, 12, 0, (nx_rect){0, 2, 4, 1},
                       (nx_color){0, 0, 1, 1}) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t semantic_count = 0;
    CHECK(nx_semantics_count(ctx, &semantic_count) == NX_OK &&
          semantic_count == 2);
    nx_semantic_view view;
    CHECK(nx_semantics_get(ctx, 0, &view) == NX_OK &&
          view.role == NX_ROLE_IMAGE && view.id == 11 &&
          view.name.length == 4 && memcmp(view.name.data, "Logo", 4) == 0);
    CHECK(nx_semantics_get(ctx, 1, &view) == NX_OK &&
          view.role == NX_ROLE_SEPARATOR && view.id == 12);
    const nx_command *commands = NULL;
    size_t command_count = 0;
    CHECK(nx_frame_commands(ctx, &commands, &command_count) == NX_OK &&
          command_count == 2);
    CHECK(commands[0].type == NX_COMMAND_BITMAP &&
          commands[1].type == NX_COMMAND_FILL_RECT);
    bitmap_pixels[0] = 200;
    uint8_t pixels[4 * 3 * 4] = {0};
    CHECK(nx_headless_render(commands, command_count, pixels, 4, 3, 16,
                             (nx_color){0, 0, 0, 0}) == NX_OK);
    CHECK(memcmp(pixels, (uint8_t[]){200, 0, 0, 255}, 4) == 0);
    CHECK(memcmp(pixels + 2 * 4, (uint8_t[]){0, 255, 0, 255}, 4) == 0);
    CHECK(memcmp(pixels + 2 * 16, (uint8_t[]){0, 0, 255, 255}, 4) == 0);
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_spacer()) return 1;
    if (test_image_separator_and_rollback()) return 1;
    puts("display widget tests passed");
    return 0;
}
