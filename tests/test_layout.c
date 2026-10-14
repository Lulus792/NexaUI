#include "nexaui/nexaui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static int near(float a, float b) { return fabsf(a - b) < 0.001f; }

static nx_layout_desc leaf(float width, float height) {
    nx_layout_desc d = nx_layout_default_desc(NX_LAYOUT_LEAF);
    d.width.preferred = width;
    d.width.has_preferred = 1;
    d.height.preferred = height;
    d.height.has_preferred = 1;
    return d;
}

static int test_growth_and_measure(void) {
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(NULL, 2, &layout) == NX_OK);
    nx_layout_desc row = nx_layout_default_desc(NX_LAYOUT_ROW);
    row.padding.left = row.padding.right = 10;
    row.gap = 5;
    uint32_t root, first, second;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &row, &root) == NX_OK);
    nx_layout_desc a = leaf(40, 20);
    a.width.grow = 1;
    a.width.maximum = 60;
    nx_layout_desc b = leaf(40, 20);
    b.width.grow = 1;
    CHECK(nx_layout_add(layout, root, &a, &first) == NX_OK);
    CHECK(nx_layout_add(layout, root, &b, &second) == NX_OK);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, 200, 50, &desired) == NX_OK);
    CHECK(near(desired.width, 105) && near(desired.height, 20));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 200, 50}) == NX_OK);
    nx_rect r1, r2;
    CHECK(nx_layout_get_rect(layout, first, &r1) == NX_OK);
    CHECK(nx_layout_get_rect(layout, second, &r2) == NX_OK);
    CHECK(near(r1.x, 10) && near(r1.width, 60));
    CHECK(near(r2.x, 75) && near(r2.width, 115));
    CHECK(near(r2.x + r2.width, 190));
    nx_layout_destroy(layout);
    return 0;
}

static int test_shrink_minimum_and_overflow(void) {
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    nx_layout_desc row = nx_layout_default_desc(NX_LAYOUT_ROW);
    nx_layout_desc item = leaf(50, 10);
    item.width.minimum = 30;
    uint32_t root, a, b;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &row, &root) == NX_OK);
    CHECK(nx_layout_add(layout, root, &item, &a) == NX_OK);
    CHECK(nx_layout_add(layout, root, &item, &b) == NX_OK);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, 40, 20, &desired) == NX_OK);
    CHECK(near(desired.width, 100));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 60, 20}) == NX_OK);
    nx_rect ra, rb;
    CHECK(nx_layout_get_rect(layout, a, &ra) == NX_OK);
    CHECK(nx_layout_get_rect(layout, b, &rb) == NX_OK);
    CHECK(near(ra.width, 30) && near(rb.x, 30) && near(rb.width, 30));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 40, 20}) == NX_OK);
    CHECK(nx_layout_get_rect(layout, b, &rb) == NX_OK);
    CHECK(near(rb.x, 30) && near(rb.x + rb.width, 60));
    nx_layout_destroy(layout);
    return 0;
}

static int test_nested_and_alignment(void) {
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    nx_layout_desc column = nx_layout_default_desc(NX_LAYOUT_COLUMN);
    column.padding = (nx_insets){4, 4, 4, 4};
    column.gap = 2;
    nx_layout_desc overlay = nx_layout_default_desc(NX_LAYOUT_OVERLAY);
    overlay.height.preferred = 30;
    overlay.height.has_preferred = 1;
    overlay.height.grow = 1;
    overlay.align_x = NX_ALIGN_STRETCH;
    nx_layout_desc item = leaf(10, 10);
    item.align_x = NX_ALIGN_CENTER;
    item.align_y = NX_ALIGN_END;
    uint32_t root, top, box;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &column, &root) == NX_OK);
    CHECK(nx_layout_add(layout, root, &overlay, &top) == NX_OK);
    CHECK(nx_layout_add(layout, top, &item, &box) == NX_OK);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, INFINITY, INFINITY, &desired) == NX_OK);
    CHECK(near(desired.width, 18) && near(desired.height, 38));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){5, 7, 100, 80}) == NX_OK);
    nx_rect rt, rb;
    CHECK(nx_layout_get_rect(layout, top, &rt) == NX_OK);
    CHECK(nx_layout_get_rect(layout, box, &rb) == NX_OK);
    CHECK(near(rt.x, 9) && near(rt.y, 11) && near(rt.height, 72));
    CHECK(near(rb.x, 50) && near(rb.y, 73));
    nx_layout_destroy(layout);
    return 0;
}

typedef struct failing_allocator { unsigned calls, frees; } failing_allocator;

static void *fail_alloc(void *user, size_t size) {
    failing_allocator *state = (failing_allocator *)user;
    ++state->calls;
    return state->calls > 2 ? NULL : malloc(size);
}

static void fail_free(void *user, void *pointer, size_t size) {
    failing_allocator *state = (failing_allocator *)user;
    (void)size;
    ++state->frees;
    free(pointer);
}

static int test_validation_and_reuse(void) {
    failing_allocator state = {0};
    nx_allocator allocator = {&state, fail_alloc, fail_free};
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(&allocator, 1, &layout) == NX_OK);
    nx_layout_desc row = nx_layout_default_desc(NX_LAYOUT_ROW);
    uint32_t root, child;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &row, &root) == NX_OK);
    nx_layout_desc bad = leaf(10, 10);
    bad.width.minimum = NAN;
    CHECK(nx_layout_add(layout, root, &bad, &child) == NX_ERROR_ARGUMENT);
    CHECK(child == NX_LAYOUT_NO_PARENT);
    bad = leaf(10, 10);
    CHECK(nx_layout_add(layout, root, &bad, &child) == NX_ERROR_MEMORY);
    CHECK(child == NX_LAYOUT_NO_PARENT);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, 100, 100, &desired) == NX_OK);
    CHECK(near(desired.width, 0) && near(desired.height, 0));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 100, 100}) == NX_OK);
    nx_layout_reset(layout);
    CHECK(nx_layout_get_rect(layout, root, &(nx_rect){0}) == NX_ERROR_ARGUMENT);
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &row, &root) == NX_OK);
    nx_layout_destroy(layout);
    CHECK(state.frees == 2);
    return 0;
}

static int test_layout_render_integration(void) {
    nx_layout *layout = NULL;
    nx_context *ui = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    CHECK(nx_context_create(NULL, &ui) == NX_OK);
    nx_layout_desc row = nx_layout_default_desc(NX_LAYOUT_ROW);
    nx_layout_desc item = leaf(1, 1);
    item.width.grow = 1;
    uint32_t root, left, right;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &row, &root) == NX_OK);
    CHECK(nx_layout_add(layout, root, &item, &left) == NX_OK);
    CHECK(nx_layout_add(layout, root, &item, &right) == NX_OK);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, 4, 1, &desired) == NX_OK);
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 4, 1}) == NX_OK);
    nx_rect left_rect, right_rect;
    CHECK(nx_layout_get_rect(layout, left, &left_rect) == NX_OK);
    CHECK(nx_layout_get_rect(layout, right, &right_rect) == NX_OK);
    CHECK(nx_begin_frame(ui, 4, 1, 0) == NX_OK);
    CHECK(nx_fill_rect(ui, left_rect, (nx_color){1, 0, 0, 1}) == NX_OK);
    CHECK(nx_fill_rect(ui, right_rect, (nx_color){0, 0, 1, 1}) == NX_OK);
    CHECK(nx_end_frame(ui) == NX_OK);
    const nx_command *commands;
    size_t count;
    CHECK(nx_frame_commands(ui, &commands, &count) == NX_OK);
    uint8_t pixels[16] = {0};
    CHECK(nx_headless_render(commands, count, pixels, 4, 1, 16,
                             (nx_color){0, 0, 0, 1}) == NX_OK);
    CHECK(pixels[0] == 255 && pixels[4] == 255);
    CHECK(pixels[8 + 2] == 255 && pixels[12 + 2] == 255);
    nx_context_destroy(ui);
    nx_layout_destroy(layout);
    return 0;
}

static int test_depth_and_parent_validation(void) {
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    nx_layout_desc row = nx_layout_default_desc(NX_LAYOUT_ROW);
    nx_layout_desc item = leaf(1, 1);
    uint32_t root, parent, child;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &row, &root) == NX_OK);
    parent = root;
    for (unsigned depth = 1; depth < NX_LAYOUT_MAX_DEPTH; ++depth) {
        CHECK(nx_layout_add(layout, parent, &row, &child) == NX_OK);
        parent = child;
    }
    CHECK(nx_layout_add(layout, parent, &row, &child) == NX_ERROR_LIMIT);
    CHECK(nx_layout_add(layout, root, &item, &child) == NX_OK);
    uint32_t invalid_child;
    CHECK(nx_layout_add(layout, child, &item, &invalid_child) == NX_ERROR_ARGUMENT);
    nx_layout_destroy(layout);
    return 0;
}

static int test_scroll_viewport_and_offsets(void) {
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    nx_layout_desc scroll = nx_layout_default_desc(NX_LAYOUT_SCROLL);
    scroll.padding = (nx_insets){2, 2, 2, 2};
    nx_layout_desc content = leaf(20, 100);
    content.margin.top = content.margin.bottom = 5;
    uint32_t root, child, extra;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &scroll, &root) == NX_OK);
    CHECK(nx_layout_add(layout, root, &content, &child) == NX_OK);
    CHECK(nx_layout_add(layout, root, &content, &extra) == NX_ERROR_ARGUMENT);
    CHECK(nx_layout_set_scroll(layout, root, 0, 100) == NX_OK);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, 30, 40, &desired) == NX_OK);
    CHECK(near(desired.width, 24) && near(desired.height, 40));
    nx_scroll_info info;
    CHECK(nx_layout_get_scroll_info(layout, root, &info) == NX_ERROR_STATE);
    CHECK(nx_layout_arrange(layout, root, (nx_rect){10, 20, 30, 40}) == NX_OK);
    CHECK(nx_layout_get_scroll_info(layout, root, &info) == NX_OK);
    CHECK(near(info.viewport.x, 12) && near(info.viewport.y, 22));
    CHECK(near(info.viewport.width, 26) && near(info.viewport.height, 36));
    CHECK(near(info.content_size.width, 20) && near(info.content_size.height, 110));
    CHECK(near(info.max_x, 0) && near(info.max_y, 74));
    CHECK(near(info.offset_x, 0) && near(info.offset_y, 74));
    nx_rect rect;
    CHECK(nx_layout_get_rect(layout, child, &rect) == NX_OK);
    CHECK(near(rect.x, 12) && near(rect.y, -47) && near(rect.height, 100));
    CHECK(nx_layout_set_scroll(layout, root, 0, 10) == NX_OK);
    CHECK(nx_layout_get_scroll_info(layout, root, &info) == NX_ERROR_STATE);
    CHECK(nx_layout_arrange(layout, root, (nx_rect){10, 20, 30, 40}) == NX_OK);
    CHECK(nx_layout_get_rect(layout, child, &rect) == NX_OK);
    CHECK(near(rect.y, 17));
    CHECK(nx_layout_set_scroll(layout, root, NAN, 0) == NX_ERROR_ARGUMENT);
    CHECK(nx_layout_set_scroll(layout, root, -1, 0) == NX_ERROR_ARGUMENT);
    CHECK(nx_layout_set_scroll(layout, child, 0, 0) == NX_ERROR_ARGUMENT);

    nx_layout_reset(layout);
    scroll.scroll_axes = NX_SCROLL_X | NX_SCROLL_Y;
    content = leaf(100, 80);
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &scroll, &root) == NX_OK);
    CHECK(nx_layout_add(layout, root, &content, &child) == NX_OK);
    CHECK(nx_layout_set_scroll(layout, root, 100, 100) == NX_OK);
    CHECK(nx_layout_measure(layout, root, 30, 40, &desired) == NX_OK);
    CHECK(near(desired.width, 30) && near(desired.height, 40));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 30, 40}) == NX_OK);
    CHECK(nx_layout_get_scroll_info(layout, root, &info) == NX_OK);
    CHECK(near(info.max_x, 74) && near(info.max_y, 44));
    CHECK(near(info.offset_x, 74) && near(info.offset_y, 44));
    CHECK(nx_layout_get_rect(layout, child, &rect) == NX_OK);
    CHECK(near(rect.x, -72) && near(rect.y, -42));
    nx_context *ui = NULL;
    CHECK(nx_context_create(NULL, &ui) == NX_OK);
    CHECK(nx_begin_frame(ui, 30, 40, 0) == NX_OK);
    CHECK(nx_push_clip(ui, info.viewport) == NX_OK);
    CHECK(nx_fill_rect(ui, rect, (nx_color){1, 0, 0, 1}) == NX_OK);
    CHECK(nx_pop_clip(ui) == NX_OK);
    CHECK(nx_end_frame(ui) == NX_OK);
    const nx_command *commands;
    size_t count;
    CHECK(nx_frame_commands(ui, &commands, &count) == NX_OK);
    uint8_t pixels[30 * 40 * 4] = {0};
    CHECK(nx_headless_render(commands, count, pixels, 30, 40, 30 * 4,
                             (nx_color){0, 0, 0, 1}) == NX_OK);
    CHECK(pixels[(3 * 30 + 3) * 4] == 255);
    CHECK(pixels[(1 * 30 + 3) * 4] == 0);
    CHECK(pixels[(3 * 30 + 1) * 4] == 0);
    nx_context_destroy(ui);
    nx_layout_destroy(layout);
    return 0;
}

static int test_wrapping_rows_and_columns(void) {
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    nx_layout_desc wrap = nx_layout_default_desc(NX_LAYOUT_WRAP_ROW);
    wrap.padding = (nx_insets){1, 1, 1, 1};
    wrap.gap = 2;
    wrap.line_gap = 3;
    uint32_t root, a, b, c, d;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &wrap, &root) == NX_OK);
    nx_layout_desc item = leaf(10, 5);
    item.margin.right = 1;
    CHECK(nx_layout_add(layout, root, &item, &a) == NX_OK);
    item = leaf(8, 8);
    CHECK(nx_layout_add(layout, root, &item, &b) == NX_OK);
    item = leaf(10, 4);
    CHECK(nx_layout_add(layout, root, &item, &c) == NX_OK);
    item = leaf(4, 6);
    CHECK(nx_layout_add(layout, root, &item, &d) == NX_OK);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, 25, INFINITY, &desired) == NX_OK);
    CHECK(near(desired.width, 23) && near(desired.height, 19));
    /* The assignment can differ from measurement; the measured breaks stay. */
    CHECK(nx_layout_arrange(layout, root, (nx_rect){5, 10, 100, 50}) == NX_OK);
    nx_rect ra, rb, rc, rd;
    CHECK(nx_layout_get_rect(layout, a, &ra) == NX_OK);
    CHECK(nx_layout_get_rect(layout, b, &rb) == NX_OK);
    CHECK(nx_layout_get_rect(layout, c, &rc) == NX_OK);
    CHECK(nx_layout_get_rect(layout, d, &rd) == NX_OK);
    CHECK(near(ra.x, 6) && near(ra.y, 11));
    CHECK(near(rb.x, 19) && near(rb.y, 11));
    CHECK(near(rc.x, 6) && near(rc.y, 22));
    CHECK(near(rd.x, 18) && near(rd.y, 22));
    CHECK(nx_layout_measure(layout, root, INFINITY, INFINITY, &desired) == NX_OK);
    CHECK(near(desired.width, 41) && near(desired.height, 10));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 41, 10}) == NX_OK);
    CHECK(nx_layout_get_rect(layout, d, &rd) == NX_OK);
    CHECK(near(rd.x, 36) && near(rd.y, 1));

    nx_layout_reset(layout);
    wrap = nx_layout_default_desc(NX_LAYOUT_WRAP_COLUMN);
    wrap.gap = 1;
    wrap.line_gap = 2;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &wrap, &root) == NX_OK);
    item = leaf(5, 10);
    CHECK(nx_layout_add(layout, root, &item, &a) == NX_OK);
    item = leaf(3, 8);
    CHECK(nx_layout_add(layout, root, &item, &b) == NX_OK);
    item = leaf(7, 10);
    CHECK(nx_layout_add(layout, root, &item, &c) == NX_OK);
    CHECK(nx_layout_measure(layout, root, INFINITY, 22, &desired) == NX_OK);
    CHECK(near(desired.width, 14) && near(desired.height, 19));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 30, 22}) == NX_OK);
    CHECK(nx_layout_get_rect(layout, b, &rb) == NX_OK);
    CHECK(nx_layout_get_rect(layout, c, &rc) == NX_OK);
    CHECK(near(rb.x, 0) && near(rb.y, 11));
    CHECK(near(rc.x, 7) && near(rc.y, 0));
    nx_layout_reset(layout);
    wrap.line_gap = NAN;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &wrap, &root) == NX_ERROR_ARGUMENT);
    nx_layout_destroy(layout);
    return 0;
}

static int test_wrap_line_flex_and_overflow(void) {
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    nx_layout_desc wrap = nx_layout_default_desc(NX_LAYOUT_WRAP_ROW);
    wrap.gap = 2;
    uint32_t root, a, b, c;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &wrap, &root) == NX_OK);
    nx_layout_desc item = leaf(10, 2);
    item.width.grow = 1;
    item.width.maximum = 20;
    item.width.minimum = 8;
    CHECK(nx_layout_add(layout, root, &item, &a) == NX_OK);
    CHECK(nx_layout_add(layout, root, &item, &b) == NX_OK);
    CHECK(nx_layout_add(layout, root, &item, &c) == NX_OK);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, 24, 20, &desired) == NX_OK);
    CHECK(near(desired.width, 22) && near(desired.height, 4));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 24, 20}) == NX_OK);
    nx_rect ra, rb, rc;
    CHECK(nx_layout_get_rect(layout, a, &ra) == NX_OK);
    CHECK(nx_layout_get_rect(layout, b, &rb) == NX_OK);
    CHECK(nx_layout_get_rect(layout, c, &rc) == NX_OK);
    CHECK(near(ra.width, 11) && near(rb.x, 13) && near(rb.width, 11));
    CHECK(near(rc.x, 0) && near(rc.y, 2) && near(rc.width, 20));
    CHECK(nx_layout_measure(layout, root, 5, 20, &desired) == NX_OK);
    CHECK(near(desired.width, 10) && near(desired.height, 6));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 5, 20}) == NX_OK);
    CHECK(nx_layout_get_rect(layout, a, &ra) == NX_OK);
    CHECK(nx_layout_get_rect(layout, c, &rc) == NX_OK);
    CHECK(near(ra.width, 8) && near(rc.y, 4) && near(rc.width, 8));
    CHECK(nx_layout_measure(layout, root, 40, 20, &desired) == NX_OK);
    CHECK(near(desired.width, 34) && near(desired.height, 2));
    nx_layout_destroy(layout);
    return 0;
}

static int test_wrapping_inside_scroll(void) {
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    nx_layout_desc scroll = nx_layout_default_desc(NX_LAYOUT_SCROLL);
    nx_layout_desc wrap = nx_layout_default_desc(NX_LAYOUT_WRAP_ROW);
    wrap.gap = 2;
    nx_layout_desc item = leaf(10, 6);
    uint32_t root, content, first, second, third;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &scroll, &root) == NX_OK);
    CHECK(nx_layout_add(layout, root, &wrap, &content) == NX_OK);
    CHECK(nx_layout_add(layout, content, &item, &first) == NX_OK);
    CHECK(nx_layout_add(layout, content, &item, &second) == NX_OK);
    CHECK(nx_layout_add(layout, content, &item, &third) == NX_OK);
    CHECK(nx_layout_set_scroll(layout, root, 0, 10) == NX_OK);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, 15, 10, &desired) == NX_OK);
    CHECK(near(desired.width, 10) && near(desired.height, 10));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 15, 10}) == NX_OK);
    nx_scroll_info info;
    CHECK(nx_layout_get_scroll_info(layout, root, &info) == NX_OK);
    CHECK(near(info.content_size.height, 18) && near(info.max_y, 8) &&
          near(info.offset_y, 8));
    nx_rect a, b, c;
    CHECK(nx_layout_get_rect(layout, first, &a) == NX_OK);
    CHECK(nx_layout_get_rect(layout, second, &b) == NX_OK);
    CHECK(nx_layout_get_rect(layout, third, &c) == NX_OK);
    CHECK(near(a.y, -8) && near(b.y, -2) && near(c.y, 4));
    nx_layout_destroy(layout);
    return 0;
}

static int test_grid_tracks_and_spans(void) {
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    nx_layout_desc grid = nx_layout_default_desc(NX_LAYOUT_GRID);
    grid.gap = 2;
    grid.line_gap = 3;
    uint32_t root, a, b, c, span, rejected;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &grid, &root) == NX_OK);
    nx_grid_track columns[] = {
        {NX_GRID_TRACK_FIXED, 20, 0, INFINITY},
        {NX_GRID_TRACK_AUTO, 0, 0, INFINITY},
        {NX_GRID_TRACK_FLEX, 1, 5, INFINITY}
    };
    nx_grid_track rows[] = {
        {NX_GRID_TRACK_AUTO, 0, 0, INFINITY},
        {NX_GRID_TRACK_FLEX, 1, 4, INFINITY}
    };
    nx_layout_desc item = leaf(30, 8);
    CHECK(nx_layout_add(layout, root, &item, &rejected) == NX_ERROR_ARGUMENT);
    CHECK(nx_layout_grid_configure(layout, root, columns, 3, rows, 2) == NX_OK);
    CHECK(nx_layout_add_grid(layout, root, &item, 0, 0, 1, 1, &a) == NX_OK);
    item = leaf(12, 10);
    CHECK(nx_layout_add_grid(layout, root, &item, 0, 1, 1, 1, &b) == NX_OK);
    item = leaf(6, 5);
    CHECK(nx_layout_add_grid(layout, root, &item, 0, 2, 1, 1, &c) == NX_OK);
    item = leaf(30, 7);
    CHECK(nx_layout_add_grid(layout, root, &item, 1, 1, 1, 2, &span) == NX_OK);
    CHECK(nx_layout_add_grid(layout, root, &item, 1, 2, 1, 2, &rejected) == NX_ERROR_ARGUMENT);
    CHECK(nx_layout_add_grid(layout, root, &item, 0, 0, 0, 1, &rejected) == NX_ERROR_ARGUMENT);
    CHECK(nx_layout_grid_configure(layout, root, columns, 3, rows, 2) == NX_ERROR_ARGUMENT);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, 100, 30, &desired) == NX_OK);
    CHECK(near(desired.width, 52) && near(desired.height, 20));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 100, 30}) == NX_OK);
    nx_rect ra, rb, rc, rs;
    CHECK(nx_layout_get_rect(layout, a, &ra) == NX_OK);
    CHECK(nx_layout_get_rect(layout, b, &rb) == NX_OK);
    CHECK(nx_layout_get_rect(layout, c, &rc) == NX_OK);
    CHECK(nx_layout_get_rect(layout, span, &rs) == NX_OK);
    CHECK(near(ra.width, 20) && near(rb.x, 22) && near(rc.x, 46));
    CHECK(near(rs.x, 22) && near(rs.y, 13) && near(rs.width, 30));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 30, 15}) == NX_OK);
    CHECK(nx_layout_get_rect(layout, c, &rc) == NX_OK);
    CHECK(near(rc.x, 25) && near(rc.width, 5));
    nx_layout_destroy(layout);
    return 0;
}

static int test_grid_auto_spans_and_nested_wrap(void) {
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    nx_layout_desc grid = nx_layout_default_desc(NX_LAYOUT_GRID);
    grid.gap = 1;
    uint32_t root, child;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &grid, &root) == NX_OK);
    nx_grid_track columns[] = {
        {NX_GRID_TRACK_AUTO, 0, 0, 4},
        {NX_GRID_TRACK_AUTO, 0, 0, INFINITY},
        {NX_GRID_TRACK_FIXED, 5, 0, INFINITY}
    };
    nx_grid_track rows[] = {{NX_GRID_TRACK_AUTO, 0, 0, INFINITY}};
    CHECK(nx_layout_grid_configure(layout, root, columns, 3, rows, 1) == NX_OK);
    nx_layout_desc item = leaf(21, 3);
    CHECK(nx_layout_add_grid(layout, root, &item, 0, 0, 1, 2, &child) == NX_OK);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, INFINITY, INFINITY, &desired) == NX_OK);
    CHECK(near(desired.width, 27) && near(desired.height, 3));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 27, 3}) == NX_OK);
    nx_rect rect;
    CHECK(nx_layout_get_rect(layout, child, &rect) == NX_OK);
    CHECK(near(rect.x, 0) && near(rect.width, 21));

    nx_layout_reset(layout);
    grid.gap = 0;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &grid, &root) == NX_OK);
    nx_grid_track fixed[] = {{NX_GRID_TRACK_FIXED, 15, 0, INFINITY}};
    CHECK(nx_layout_grid_configure(layout, root, fixed, 1, rows, 1) == NX_OK);
    nx_layout_desc wrap = nx_layout_default_desc(NX_LAYOUT_WRAP_ROW);
    wrap.gap = 2;
    CHECK(nx_layout_add_grid(layout, root, &wrap, 0, 0, 1, 1, &child) == NX_OK);
    item = leaf(10, 6);
    uint32_t a, b, c;
    CHECK(nx_layout_add(layout, child, &item, &a) == NX_OK);
    CHECK(nx_layout_add(layout, child, &item, &b) == NX_OK);
    CHECK(nx_layout_add(layout, child, &item, &c) == NX_OK);
    CHECK(nx_layout_measure(layout, root, INFINITY, INFINITY, &desired) == NX_OK);
    CHECK(near(desired.width, 15) && near(desired.height, 18));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 15, 18}) == NX_OK);
    CHECK(nx_layout_get_rect(layout, c, &rect) == NX_OK);
    CHECK(near(rect.y, 12));
    nx_layout_destroy(layout);
    return 0;
}

static int test_grid_configuration_errors(void) {
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    nx_layout_desc grid = nx_layout_default_desc(NX_LAYOUT_GRID);
    uint32_t root, child;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &grid, &root) == NX_OK);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, 10, 10, &desired) == NX_ERROR_STATE);
    nx_grid_track columns[] = {{NX_GRID_TRACK_AUTO, 0, 0, INFINITY}};
    nx_grid_track rows[] = {{NX_GRID_TRACK_FIXED, 10, 0, INFINITY}};
    CHECK(nx_layout_grid_configure(layout, root, columns, 0, rows, 1) == NX_ERROR_ARGUMENT);
    columns[0].maximum = NAN;
    CHECK(nx_layout_grid_configure(layout, root, columns, 1, rows, 1) == NX_ERROR_ARGUMENT);
    columns[0].maximum = INFINITY;
    CHECK(nx_layout_grid_configure(layout, root, columns, 1, rows, 1) == NX_OK);
    columns[0].minimum = 100;
    CHECK(nx_layout_measure(layout, root, 10, 10, &desired) == NX_OK);
    CHECK(near(desired.width, 0) && near(desired.height, 10));
    nx_layout_desc item = leaf(3, 3);
    CHECK(nx_layout_add_grid(layout, root, &item, 1, 0, 1, 1, &child) == NX_ERROR_ARGUMENT);
    CHECK(child == NX_LAYOUT_NO_PARENT);
    CHECK(nx_layout_add_grid(layout, root, &item, 0, 0, 1, 1, &child) == NX_OK);
    nx_layout_destroy(layout);
    return 0;
}

typedef struct grid_allocator_probe {
    unsigned calls, frees, fail_on;
} grid_allocator_probe;

static void *grid_probe_allocate(void *user, size_t size) {
    grid_allocator_probe *probe = (grid_allocator_probe *)user;
    ++probe->calls;
    return probe->calls == probe->fail_on ? NULL : malloc(size);
}

static void grid_probe_deallocate(void *user, void *pointer, size_t size) {
    grid_allocator_probe *probe = (grid_allocator_probe *)user;
    (void)size;
    ++probe->frees;
    free(pointer);
}

static int test_grid_allocation_rollback(void) {
    grid_allocator_probe probe = {0};
    probe.fail_on = 4;
    nx_allocator allocator = {&probe, grid_probe_allocate, grid_probe_deallocate};
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(&allocator, 1, &layout) == NX_OK);
    nx_layout_desc grid = nx_layout_default_desc(NX_LAYOUT_GRID);
    uint32_t root;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &grid, &root) == NX_OK);
    nx_grid_track track = {NX_GRID_TRACK_AUTO, 0, 0, INFINITY};
    CHECK(nx_layout_grid_configure(layout, root, &track, 1, &track, 1) == NX_ERROR_MEMORY);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, 10, 10, &desired) == NX_ERROR_STATE);
    probe.fail_on = 0;
    CHECK(nx_layout_grid_configure(layout, root, &track, 1, &track, 1) == NX_OK);
    probe.fail_on = 8;
    CHECK(nx_layout_grid_configure(layout, root, &track, 1, &track, 1) == NX_ERROR_MEMORY);
    CHECK(nx_layout_measure(layout, root, 10, 10, &desired) == NX_OK);
    nx_layout_reset(layout);
    nx_layout_destroy(layout);
    CHECK(probe.calls == 8 && probe.frees == 6);
    return 0;
}

static int test_split_bounds_and_resize(void) {
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    nx_layout_desc split = nx_layout_default_desc(NX_LAYOUT_SPLIT);
    split.padding = (nx_insets){2, 2, 2, 2};
    split.split_ratio = 0.25f;
    uint32_t root, first, second, rejected;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &split, &root) == NX_OK);
    nx_layout_desc a = leaf(40, 10);
    a.width.minimum = 20;
    a.width.maximum = 60;
    a.margin.left = 3;
    a.margin.right = 2;
    nx_layout_desc b = leaf(50, 10);
    b.width.minimum = 30;
    b.margin.right = 4;
    CHECK(nx_layout_add(layout, root, &a, &first) == NX_OK);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, 100, 40, &desired) == NX_ERROR_STATE);
    CHECK(nx_layout_add(layout, root, &b, &second) == NX_OK);
    CHECK(nx_layout_add(layout, root, &b, &rejected) == NX_ERROR_ARGUMENT);
    CHECK(nx_layout_measure(layout, root, 100, 40, &desired) == NX_OK);
    CHECK(near(desired.width, 109) && near(desired.height, 14));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 100, 40}) == NX_OK);
    nx_split_info info;
    CHECK(nx_layout_get_split_info(layout, root, &info) == NX_OK);
    CHECK(near(info.divider.x, 27) && near(info.divider.y, 2) &&
          near(info.divider.width, 6) && near(info.divider.height, 36));
    CHECK(near(info.first_extent, 25) && near(info.second_extent, 65));
    nx_rect ra, rb;
    CHECK(nx_layout_get_rect(layout, first, &ra) == NX_OK);
    CHECK(nx_layout_get_rect(layout, second, &rb) == NX_OK);
    CHECK(near(ra.x, 5) && near(ra.width, 20));
    CHECK(near(rb.x, 33) && near(rb.width, 61));
    CHECK(nx_layout_set_split(layout, root, 0.8f) == NX_OK);
    CHECK(nx_layout_get_split_info(layout, root, &info) == NX_ERROR_STATE);
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 100, 40}) == NX_OK);
    CHECK(nx_layout_get_split_info(layout, root, &info) == NX_OK);
    CHECK(near(info.first_extent, 56) && near(info.second_extent, 34));
    CHECK(near(info.divider.x, 58));
    CHECK(nx_layout_get_rect(layout, second, &rb) == NX_OK);
    CHECK(near(rb.width, 30));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 50, 40}) == NX_OK);
    CHECK(nx_layout_get_split_info(layout, root, &info) == NX_OK);
    CHECK(near(info.first_extent, 25) && near(info.second_extent, 34));
    CHECK(nx_layout_get_rect(layout, second, &rb) == NX_OK);
    CHECK(near(rb.x + rb.width, 63));
    CHECK(nx_layout_set_split(layout, root, NAN) == NX_ERROR_ARGUMENT);
    CHECK(nx_layout_set_split(layout, root, -0.1f) == NX_ERROR_ARGUMENT);
    CHECK(nx_layout_set_split(layout, first, 0.5f) == NX_ERROR_ARGUMENT);
    nx_layout_destroy(layout);
    return 0;
}

static int test_vertical_split_and_maxima(void) {
    nx_layout *layout = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    nx_layout_desc split = nx_layout_default_desc(NX_LAYOUT_SPLIT);
    split.split_axis = NX_SPLIT_Y;
    split.gap = 4;
    uint32_t root, first, second;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &split, &root) == NX_OK);
    nx_layout_desc item = leaf(10, 10);
    item.align_x = NX_ALIGN_STRETCH;
    item.height.minimum = 5;
    item.height.maximum = 20;
    CHECK(nx_layout_add(layout, root, &item, &first) == NX_OK);
    item.height.maximum = 15;
    CHECK(nx_layout_add(layout, root, &item, &second) == NX_OK);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, INFINITY, INFINITY, &desired) == NX_OK);
    CHECK(near(desired.width, 10) && near(desired.height, 24));
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 40, 50}) == NX_OK);
    nx_split_info info;
    CHECK(nx_layout_get_split_info(layout, root, &info) == NX_OK);
    CHECK(near(info.first_extent, 20) && near(info.second_extent, 15));
    CHECK(near(info.divider.y, 20) && near(info.divider.height, 4));
    nx_rect ra, rb;
    CHECK(nx_layout_get_rect(layout, first, &ra) == NX_OK);
    CHECK(nx_layout_get_rect(layout, second, &rb) == NX_OK);
    CHECK(near(ra.width, 40) && near(ra.height, 20));
    CHECK(near(rb.y, 24) && near(rb.height, 15));
    nx_layout_reset(layout);
    split.split_ratio = INFINITY;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &split, &root) == NX_ERROR_ARGUMENT);
    nx_layout_destroy(layout);
    return 0;
}

int main(void) {
    if (test_growth_and_measure()) return 1;
    if (test_shrink_minimum_and_overflow()) return 1;
    if (test_nested_and_alignment()) return 1;
    if (test_validation_and_reuse()) return 1;
    if (test_layout_render_integration()) return 1;
    if (test_depth_and_parent_validation()) return 1;
    if (test_scroll_viewport_and_offsets()) return 1;
    if (test_wrapping_rows_and_columns()) return 1;
    if (test_wrap_line_flex_and_overflow()) return 1;
    if (test_wrapping_inside_scroll()) return 1;
    if (test_grid_tracks_and_spans()) return 1;
    if (test_grid_auto_spans_and_nested_wrap()) return 1;
    if (test_grid_configuration_errors()) return 1;
    if (test_grid_allocation_rollback()) return 1;
    if (test_split_bounds_and_resize()) return 1;
    if (test_vertical_split_and_maxima()) return 1;
    puts("layout tests passed");
    return 0;
}
