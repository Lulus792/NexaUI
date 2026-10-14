#include "nexaui/nexaui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static int near(double a, double b) { return fabs(a - b) < 0.0001; }

typedef struct visit_probe {
    size_t count, fail_at, first, last;
    uint64_t first_key, last_key;
    double first_y, last_y;
} visit_probe;

static nx_result record_row(void *user, const nx_virtual_row *row) {
    visit_probe *probe = (visit_probe *)user;
    if (probe->count == probe->fail_at) return NX_ERROR_STATE;
    if (!probe->count) {
        probe->first = row->index;
        probe->first_key = row->key;
        probe->first_y = row->viewport_y;
    }
    probe->last = row->index;
    probe->last_key = row->key;
    probe->last_y = row->viewport_y;
    ++probe->count;
    return NX_OK;
}

static int test_virtual_million(void) {
    nx_virtual_list *list = NULL;
    CHECK(nx_virtual_list_create(1000000u, 20.0, NULL, &list) == NX_OK);
    nx_virtual_range range;
    CHECK(nx_virtual_list_range(list, 5000000.0, 100.0, 2, &range) == NX_OK);
    CHECK(range.first == 249998u && range.end == 250007u);
    CHECK(near(range.first_offset, 4999960.0));
    CHECK(near(range.total_extent, 20000000.0));
    CHECK(near(range.effective_offset, 5000000.0));
    visit_probe probe = {0};
    probe.fail_at = SIZE_MAX;
    nx_virtual_range visited_range;
    size_t visited;
    CHECK(nx_virtual_list_visit(list, 5000000.0, 100.0, 2, 8,
                                record_row, &probe, &visited_range,
                                &visited) == NX_ERROR_LIMIT);
    CHECK(!visited && !probe.count && visited_range.first == 249998u &&
          visited_range.end == 250007u);
    CHECK(nx_virtual_list_visit(list, 5000000.0, 100.0, 2, 9,
                                record_row, &probe, &visited_range,
                                &visited) == NX_OK);
    CHECK(visited == 9 && probe.count == 9 &&
          probe.first == 249998u && probe.last == 250006u &&
          probe.first_key == 249999u && probe.last_key == 250007u &&
          near(probe.first_y, -40.0) && near(probe.last_y, 120.0));
    probe = (visit_probe){0};
    probe.fail_at = 2;
    CHECK(nx_virtual_list_visit(list, 5000000.0, 100.0, 2, 9,
                                record_row, &probe, &visited_range,
                                &visited) == NX_ERROR_STATE);
    CHECK(visited == 2 && probe.count == 2 &&
          visited_range.first == 249998u);
    double position;
    CHECK(nx_virtual_list_offset_at(list, 250000u, &position) == NX_OK &&
          near(position, 5000000.0));
    CHECK(nx_virtual_list_set_height(list, 250000u, 45.0) == NX_OK);
    CHECK(nx_virtual_list_offset_at(list, 250001u, &position) == NX_OK &&
          near(position, 5000045.0));
    CHECK(nx_virtual_list_range(list, 5000000.0, 100.0, 2, &range) == NX_OK);
    CHECK(range.first == 249998u && range.end == 250006u);
    CHECK(near(range.total_extent, 20000025.0));
    CHECK(nx_virtual_list_range(list, 1e30, 100.0, SIZE_MAX,
                                &range) == NX_OK);
    CHECK(range.end == 1000000u && range.first >= 999700u);
    CHECK(range.end - range.first <= 261u);
    CHECK(near(range.effective_offset, 19999925.0));
    CHECK(nx_virtual_list_set_height(list, 250000u, 0.0) == NX_ERROR_ARGUMENT);
    CHECK(nx_virtual_list_set_height(list, 250000u, NAN) == NX_ERROR_ARGUMENT);
    CHECK(nx_virtual_list_range(list, -1.0, 100.0, 0,
                                &range) == NX_ERROR_ARGUMENT);
    CHECK(nx_virtual_list_offset_at(list, 1000001u, &position) ==
          NX_ERROR_ARGUMENT);
    nx_virtual_list_destroy(list);
    CHECK(nx_virtual_list_create(0, 12.0, NULL, &list) == NX_OK);
    CHECK(nx_virtual_list_range(list, 0, 50, 4, &range) == NX_OK &&
          range.first == 0 && range.end == 0 && range.total_extent == 0);
    probe = (visit_probe){0};
    probe.fail_at = SIZE_MAX;
    CHECK(nx_virtual_list_visit(list, 0, 50, 4, 0,
                                record_row, &probe, &visited_range,
                                &visited) == NX_OK && !visited && !probe.count);
    nx_virtual_list_destroy(list);
    return 0;
}

static nx_scroll_area_desc scroll_desc(uint64_t id, uint64_t parent,
                                        nx_rect bounds, nx_size content) {
    nx_scroll_area_desc desc = {0};
    desc.id = id;
    desc.parent_id = parent;
    desc.viewport = bounds;
    desc.name = (nx_utf8_span){"Scroll", 6};
    desc.content_size = content;
    desc.axes = NX_SCROLL_Y;
    return desc;
}

static int wheel(nx_context *ctx, float x, float y, float delta) {
    nx_event event = {0};
    event.type = NX_EVENT_WHEEL;
    event.x = x;
    event.y = y;
    event.delta_y = delta;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

typedef struct build_rows {
    nx_context *ctx;
    uint64_t parent_id;
    size_t count;
} build_rows;

static nx_result build_row_button(void *user, const nx_virtual_row *row) {
    build_rows *build = (build_rows *)user;
    nx_control_desc desc = {0};
    desc.id = row->key;
    desc.parent_id = build->parent_id;
    desc.bounds = (nx_rect){0, (float)row->viewport_y,
                             100, (float)row->height};
    desc.name = (nx_utf8_span){"Row", 3};
    nx_interaction_state state;
    nx_result result = nx_button(build->ctx, &desc, &state);
    if (result == NX_OK) ++build->count;
    return result;
}

static int test_virtual_visit_build(void) {
    nx_virtual_list *list = NULL;
    nx_context *ctx = NULL;
    CHECK(nx_virtual_list_create(10, 20.0, NULL, &list) == NX_OK);
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_scroll_area_desc desc = scroll_desc(100, 0,
        (nx_rect){0, 0, 100, 40}, (nx_size){100, 200});
    float x = 0, y = 50;
    for (unsigned frame = 0; frame < 2; ++frame) {
        if (frame == 1) CHECK(wheel(ctx, 10, 10, -1));
        CHECK(nx_begin_frame(ctx, 100, 40, 0) == NX_OK);
        nx_scroll_info info;
        CHECK(nx_scroll_area_begin(ctx, &desc, &x, &y, &info) == NX_OK);
        CHECK(near(y, frame ? 90.0 : 50.0));
        build_rows build = {ctx, 100, 0};
        nx_virtual_range range;
        size_t visited;
        CHECK(nx_virtual_list_visit(list, y, 40, 1, 4,
                                    build_row_button, &build, &range,
                                    &visited) == NX_ERROR_LIMIT);
        CHECK(!visited && !build.count &&
              range.first == (frame ? 3u : 1u) &&
              range.end == (frame ? 8u : 6u));
        CHECK(nx_virtual_list_visit(list, y, 40, 1, 5,
                                    build_row_button, &build, &range,
                                    &visited) == NX_OK);
        CHECK(visited == 5 && build.count == 5);
        CHECK(nx_scroll_area_end(ctx) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        size_t count;
        uint32_t index;
        CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 6);
        CHECK(nx_semantics_find(ctx, frame ? 4 : 2, &index) == NX_OK);
        CHECK(nx_semantics_find(ctx, frame ? 2 : 8, &index) ==
              NX_ERROR_ARGUMENT);
    }
    nx_context_destroy(ctx);
    nx_virtual_list_destroy(list);
    return 0;
}

static int pointer(nx_context *ctx, nx_event_type type, float x, float y) {
    nx_event event = {0};
    event.type = type;
    event.button = NX_BUTTON_LEFT;
    event.x = x;
    event.y = y;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int touch(nx_context *ctx, nx_event_type type, uint32_t id,
                 float x, float y) {
    nx_event event = {0};
    event.type = type;
    event.contact_id = id;
    event.x = x;
    event.y = y;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int test_touch_scroll_capture(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_scroll_area_desc outer = scroll_desc(1, 0,
        (nx_rect){0, 0, 100, 100}, (nx_size){500, 500});
    outer.axes = NX_SCROLL_X | NX_SCROLL_Y;
    nx_scroll_area_desc inner = scroll_desc(2, 1,
        (nx_rect){20, 20, 50, 50}, (nx_size){50, 300});
    float ox = 0, oy = 0, ix = 0, iy = 0;
    const float inner_y[] = {0, 0, 20, 40, 50, 50, 50, 50, 50, 50, 50,
                             50, 50, 50, 50, 50, 50, 50};
    const float outer_xy[] = {0, 0, 0, 0, 0, 0, 20, 20, 20, 20, 20,
                              20, 20, 30, 30, 30, 40, 40};
    for (unsigned frame = 0; frame < sizeof(inner_y) / sizeof(inner_y[0]); ++frame) {
        if (frame == 1) CHECK(touch(ctx, NX_EVENT_TOUCH_DOWN, 11, 30, 30));
        if (frame == 2) CHECK(touch(ctx, NX_EVENT_TOUCH_MOVE, 11, 30, 10));
        if (frame == 3) {
            CHECK(touch(ctx, NX_EVENT_TOUCH_DOWN, 22, 5, 5));
            CHECK(touch(ctx, NX_EVENT_TOUCH_MOVE, 22, 5, -20));
            CHECK(touch(ctx, NX_EVENT_TOUCH_MOVE, 11, 30, -10));
        }
        if (frame == 4) CHECK(touch(ctx, NX_EVENT_TOUCH_UP, 11, 30, -20));
        if (frame == 5) CHECK(touch(ctx, NX_EVENT_TOUCH_DOWN, 33, 5, 5));
        if (frame == 6) CHECK(touch(ctx, NX_EVENT_TOUCH_MOVE, 33, -15, -15));
        if (frame == 7) CHECK(touch(ctx, NX_EVENT_TOUCH_CANCEL, 33, 0, 0));
        if (frame == 8) CHECK(touch(ctx, NX_EVENT_TOUCH_MOVE, 33, -40, -40));
        if (frame == 9) {
            outer.disabled = 1;
            CHECK(touch(ctx, NX_EVENT_TOUCH_DOWN, 44, 5, 5));
        }
        if (frame == 10) CHECK(touch(ctx, NX_EVENT_TOUCH_MOVE, 44, -20, -20));
        if (frame == 11) outer.disabled = 0;
        if (frame == 12) CHECK(touch(ctx, NX_EVENT_TOUCH_DOWN, 0, 5, 5));
        if (frame == 13) CHECK(touch(ctx, NX_EVENT_TOUCH_MOVE, 0, -5, -5));
        if (frame == 14) CHECK(touch(ctx, NX_EVENT_TOUCH_UP, 0, -5, -5));
        if (frame == 15) CHECK(touch(ctx, NX_EVENT_TOUCH_DOWN, 55, 5, 5));
        if (frame == 16) {
            CHECK(touch(ctx, NX_EVENT_TOUCH_MOVE, 55, -5, -5));
            nx_event focus = {0};
            focus.type = NX_EVENT_FOCUS;
            focus.focused = 0;
            CHECK(nx_context_queue_event(ctx, &focus) == NX_OK);
        }
        if (frame == 17) CHECK(touch(ctx, NX_EVENT_TOUCH_MOVE, 55, -15, -15));
        CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
        nx_scroll_info info;
        CHECK(nx_scroll_area_begin(ctx, &outer, &ox, &oy, &info) == NX_OK);
        CHECK(nx_scroll_area_begin(ctx, &inner, &ix, &iy, &info) == NX_OK);
        CHECK(near(iy, inner_y[frame]) && near(ix, 0));
        CHECK(near(ox, outer_xy[frame]) && near(oy, outer_xy[frame]));
        CHECK(nx_scroll_area_end(ctx) == NX_OK);
        CHECK(nx_scroll_area_end(ctx) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
    }
    nx_event invalid = {0};
    invalid.type = NX_EVENT_TOUCH_DOWN;
    invalid.x = NAN;
    CHECK(nx_context_queue_event(ctx, &invalid) == NX_ERROR_ARGUMENT);
    nx_context_destroy(ctx);
    return 0;
}

static int test_touch_tap_vs_scroll(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_scroll_area_desc desc = scroll_desc(1, 0,
        (nx_rect){0, 0, 100, 100}, (nx_size){100, 500});
    float x = 0, y = 100;
    const float expected_y[] = {100, 100, 100, 100, 100, 100,
                                100, 80, 80, 80, 80, 80, 80,
                                80, 80, 80, 72, 72, 72, 72};
    for (unsigned frame = 0; frame < sizeof(expected_y) / sizeof(expected_y[0]); ++frame) {
        if (frame == 1) CHECK(touch(ctx, NX_EVENT_TOUCH_DOWN, 7, 10, 10));
        if (frame == 2) CHECK(touch(ctx, NX_EVENT_TOUCH_UP, 7, 10, 10));
        if (frame == 3) CHECK(touch(ctx, NX_EVENT_TOUCH_DOWN, 8, 10, 10));
        if (frame == 4) CHECK(touch(ctx, NX_EVENT_TOUCH_MOVE, 8, 10, 15));
        if (frame == 5) CHECK(touch(ctx, NX_EVENT_TOUCH_UP, 8, 10, 15));
        if (frame == 6) CHECK(touch(ctx, NX_EVENT_TOUCH_DOWN, 9, 10, 10));
        if (frame == 7) CHECK(touch(ctx, NX_EVENT_TOUCH_MOVE, 9, 10, 30));
        if (frame == 8) CHECK(touch(ctx, NX_EVENT_TOUCH_UP, 9, 10, 30));
        if (frame == 9) CHECK(touch(ctx, NX_EVENT_TOUCH_DOWN, 10, 10, 10));
        if (frame == 10) CHECK(touch(ctx, NX_EVENT_TOUCH_UP, 10, 110, 10));
        if (frame == 11) CHECK(touch(ctx, NX_EVENT_TOUCH_DOWN, 11, 130, 10));
        if (frame == 12) CHECK(touch(ctx, NX_EVENT_TOUCH_UP, 11, 130, 10));
        if (frame == 13) CHECK(touch(ctx, NX_EVENT_TOUCH_DOWN, 12, 10, 10));
        if (frame == 14) CHECK(touch(ctx, NX_EVENT_TOUCH_CANCEL, 12, 0, 0));
        if (frame == 15) CHECK(touch(ctx, NX_EVENT_TOUCH_DOWN, 13, 10, 10));
        if (frame == 16) CHECK(touch(ctx, NX_EVENT_TOUCH_MOVE, 13, 10, 18));
        if (frame == 17) CHECK(touch(ctx, NX_EVENT_TOUCH_UP, 13, 10, 18));
        if (frame == 18) CHECK(touch(ctx, NX_EVENT_TOUCH_DOWN, 14, 10, 10));
        if (frame == 19) CHECK(touch(ctx, NX_EVENT_TOUCH_UP, 14, 10, 10));
        CHECK(nx_begin_frame(ctx, 200, 100, 0) == NX_OK);
        nx_input_snapshot input;
        CHECK(nx_frame_input(ctx, &input) == NX_OK);
        CHECK(input.buttons_down == 0 && input.buttons_pressed == 0 &&
              input.buttons_released == 0);
        nx_scroll_info info;
        CHECK(nx_scroll_area_begin(ctx, &desc, &x, &y, &info) == NX_OK);
        CHECK(near(y, expected_y[frame]));
        nx_control_desc child = {0};
        child.id = 2;
        child.parent_id = 1;
        child.bounds = (nx_rect){0, 0, 100, 100};
        child.name = (nx_utf8_span){"Child", 5};
        child.disabled = frame >= 18;
        nx_interaction_state child_state;
        CHECK(nx_button(ctx, &child, &child_state) == NX_OK);
        CHECK(child_state.clicked == (frame == 2 || frame == 5));
        if (frame == 2 || frame == 5)
            CHECK(child_state.pointer_pressed && child_state.pointer_released);
        CHECK(nx_scroll_area_end(ctx) == NX_OK);
        nx_control_desc outside = {0};
        outside.id = 3;
        outside.bounds = (nx_rect){120, 0, 50, 30};
        outside.name = (nx_utf8_span){"Outside", 7};
        nx_interaction_state outside_state;
        CHECK(nx_button(ctx, &outside, &outside_state) == NX_OK);
        CHECK(outside_state.clicked == (frame == 12));
        CHECK(nx_end_frame(ctx) == NX_OK);
        if (frame == 2 || frame == 12) {
            uint32_t index;
            nx_semantic_view view;
            CHECK(nx_semantics_find(ctx, frame == 2 ? 2 : 3,
                                    &index) == NX_OK);
            CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
                  (view.state & NX_SEM_FOCUSED));
        }
    }
    nx_context_destroy(ctx);
    return 0;
}

static int key(nx_context *ctx, nx_key code) {
    nx_event event = {0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = code;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int test_scrollbar_drag_keyboard(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_scroll_area_desc desc = scroll_desc(1, 0,
        (nx_rect){0, 0, 100, 100}, (nx_size){100, 500});
    desc.thumb_y_id = 3;
    float x = 0, y = 0;
    const float expected[] = {0, 0, 200, 400, 400, 360, 260, 0, 400};
    for (unsigned frame = 0; frame < sizeof(expected) / sizeof(expected[0]); ++frame) {
        if (frame == 1) CHECK(pointer(ctx, NX_EVENT_POINTER_DOWN, 97, 10));
        if (frame == 2) CHECK(pointer(ctx, NX_EVENT_POINTER_MOVE, 97, 50));
        if (frame == 3) CHECK(pointer(ctx, NX_EVENT_POINTER_MOVE, 97, 90));
        if (frame == 4) CHECK(pointer(ctx, NX_EVENT_POINTER_UP, 97, 90));
        if (frame == 5) CHECK(key(ctx, NX_KEY_UP));
        if (frame == 6) CHECK(key(ctx, NX_KEY_PAGE_UP));
        if (frame == 7) CHECK(key(ctx, NX_KEY_HOME));
        if (frame == 8) CHECK(key(ctx, NX_KEY_END));
        CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
        nx_scroll_info info;
        CHECK(nx_scroll_area_begin(ctx, &desc, &x, &y, &info) == NX_OK);
        CHECK(near(y, expected[frame]) && near(info.offset_y, y));
        nx_control_desc child = {0};
        child.id = 2;
        child.parent_id = 1;
        child.bounds = (nx_rect){0, 0, 100, 100};
        child.name = (nx_utf8_span){"Child", 5};
        nx_interaction_state state;
        CHECK(nx_button(ctx, &child, &state) == NX_OK);
        CHECK(nx_scroll_area_end(ctx) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        uint32_t index;
        nx_semantic_view view;
        CHECK(nx_semantics_find(ctx, 3, &index) == NX_OK);
        CHECK(nx_semantics_get(ctx, index, &view) == NX_OK);
        CHECK(view.role == NX_ROLE_SLIDER && view.parent_id == 1);
        CHECK(view.value.length > 0);
    }
    nx_context_destroy(ctx);
    return 0;
}

static int test_horizontal_scrollbar_and_disabled(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_scroll_area_desc desc = scroll_desc(1, 0,
        (nx_rect){0, 0, 100, 100}, (nx_size){500, 100});
    desc.axes = NX_SCROLL_X;
    desc.thumb_x_id = 4;
    float x = 0, y = 0;
    const float expected[] = {0, 0, 200, 240, 240, 240, 400};
    for (unsigned frame = 0; frame < sizeof(expected) / sizeof(expected[0]); ++frame) {
        if (frame == 1) CHECK(pointer(ctx, NX_EVENT_POINTER_DOWN, 10, 97));
        if (frame == 2) CHECK(pointer(ctx, NX_EVENT_POINTER_MOVE, 50, 97));
        if (frame == 3) {
            CHECK(pointer(ctx, NX_EVENT_POINTER_UP, 50, 97));
            CHECK(key(ctx, NX_KEY_RIGHT));
        }
        if (frame == 4) {
            desc.disabled = 1;
            CHECK(key(ctx, NX_KEY_RIGHT));
        }
        if (frame == 5) desc.disabled = 0;
        if (frame == 6) CHECK(key(ctx, NX_KEY_END));
        CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
        nx_scroll_info info;
        CHECK(nx_scroll_area_begin(ctx, &desc, &x, &y, &info) == NX_OK);
        CHECK(near(x, expected[frame]) && near(y, 0));
        CHECK(nx_scroll_area_end(ctx) == NX_OK);
        if (frame == 5) CHECK(nx_focus_set(ctx, 4) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
    }
    nx_context_destroy(ctx);
    return 0;
}

static int test_scroll_area_and_layout(void) {
    nx_context *ctx = NULL;
    nx_layout *layout = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    nx_layout_desc scroll_layout = nx_layout_default_desc(NX_LAYOUT_SCROLL);
    nx_layout_desc content_layout = nx_layout_default_desc(NX_LAYOUT_LEAF);
    content_layout.width.preferred = 100;
    content_layout.width.has_preferred = 1;
    content_layout.height.preferred = 500;
    content_layout.height.has_preferred = 1;
    uint32_t root, content;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &scroll_layout,
                        &root) == NX_OK);
    CHECK(nx_layout_add(layout, root, &content_layout, &content) == NX_OK);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, 100, 100, &desired) == NX_OK);
    nx_scroll_area_desc desc = scroll_desc(1, 0, (nx_rect){0, 0, 100, 100},
                                            (nx_size){100, 500});
    float x = 0, y = 0;
    for (unsigned frame = 0; frame < 4; ++frame) {
        if (frame == 1) CHECK(wheel(ctx, 10, 10, -2));
        if (frame == 2) CHECK(wheel(ctx, 110, 10, -2));
        if (frame == 3) CHECK(wheel(ctx, 10, 10, 3));
        CHECK(nx_begin_frame(ctx, 120, 100, 0) == NX_OK);
        nx_scroll_info info;
        CHECK(nx_scroll_area_begin(ctx, &desc, &x, &y, &info) == NX_OK);
        CHECK(near(y, frame == 1 || frame == 2 ? 80.0 : 0.0));
        CHECK(near(info.max_y, 400.0) && near(info.offset_y, y));
        CHECK(nx_layout_set_scroll(layout, root, x, y) == NX_OK);
        CHECK(nx_layout_arrange(layout, root,
                                (nx_rect){0, 0, 100, 100}) == NX_OK);
        nx_rect child_rect;
        CHECK(nx_layout_get_rect(layout, content, &child_rect) == NX_OK);
        CHECK(near(child_rect.y, -y));
        nx_control_desc child = {0};
        child.id = 2;
        child.parent_id = 1;
        child.bounds = (nx_rect){0, 0, 100, 30};
        child.name = (nx_utf8_span){"Child", 5};
        nx_interaction_state state;
        CHECK(nx_button(ctx, &child, &state) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_ERROR_STATE);
        CHECK(nx_scroll_area_end(ctx) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        uint32_t index;
        nx_semantic_view view;
        CHECK(nx_semantics_find(ctx, 1, &index) == NX_OK);
        CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
              view.role == NX_ROLE_SCROLL_AREA);
    }
    CHECK(nx_scroll_area_end(ctx) == NX_ERROR_STATE);
    nx_layout_destroy(layout);
    nx_context_destroy(ctx);
    return 0;
}

static int test_nested_wheel_routing(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_scroll_area_desc outer = scroll_desc(1, 0,
        (nx_rect){0, 0, 100, 100}, (nx_size){100, 500});
    nx_scroll_area_desc inner = scroll_desc(2, 1,
        (nx_rect){20, 20, 50, 50}, (nx_size){50, 300});
    float outer_x = 0, outer_y = 0, inner_x = 0, inner_y = 0;
    for (unsigned frame = 0; frame < 4; ++frame) {
        if (frame == 1) CHECK(wheel(ctx, 30, 30, -1));
        if (frame == 2) {
            inner.disabled = 1;
            CHECK(wheel(ctx, 5, 5, -1));
        }
        if (frame == 3) {
            CHECK(wheel(ctx, 30, 30, -1));
        }
        CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
        nx_scroll_info info;
        CHECK(nx_scroll_area_begin(ctx, &outer, &outer_x, &outer_y,
                                   &info) == NX_OK);
        CHECK(nx_scroll_area_begin(ctx, &inner, &inner_x, &inner_y,
                                   &info) == NX_OK);
        CHECK(nx_scroll_area_end(ctx) == NX_OK);
        CHECK(nx_scroll_area_end(ctx) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        CHECK(near(inner_y, frame >= 1 ? 40.0 : 0.0));
        CHECK(near(outer_y, frame >= 2 ?
                   (frame == 3 ? 80.0 : 40.0) : 0.0));
    }
    nx_context_destroy(ctx);
    return 0;
}

typedef struct failure_probe {
    size_t fail_size;
    int fail_once;
} failure_probe;

static void *probe_allocate(void *user, size_t size) {
    failure_probe *probe = (failure_probe *)user;
    if (probe->fail_once && size == probe->fail_size) {
        probe->fail_once = 0;
        return NULL;
    }
    return malloc(size);
}

static void probe_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

typedef struct measured_rows {
    const uint64_t *keys;
    const double *heights;
    size_t fail_at;
} measured_rows;

static nx_result measure_row(void *user, size_t index,
                              uint64_t *out_key, double *out_height) {
    measured_rows *rows = (measured_rows *)user;
    if (index == rows->fail_at) return NX_ERROR_STATE;
    *out_key = rows->keys[index];
    *out_height = rows->heights[index];
    return NX_OK;
}

static int test_virtual_rebuild_anchor(void) {
    failure_probe probe = {0};
    nx_allocator allocator = {&probe, probe_allocate, probe_deallocate};
    nx_virtual_list *list = NULL;
    CHECK(nx_virtual_list_create(5, 10.0, &allocator, &list) == NX_OK);
    double offset = 25.0;
    const uint64_t inserted_keys[] = {9, 1, 2, 3, 4, 5};
    const double inserted_heights[] = {12, 10, 30, 10, 10, 10};
    measured_rows rows = {inserted_keys, inserted_heights, SIZE_MAX};
    CHECK(nx_virtual_list_rebuild(list, 6, measure_row, &rows,
                                  20.0, &offset) == NX_OK);
    CHECK(near(offset, 57.0)); /* Key 3 stays five pixels into its row. */
    double position;
    CHECK(nx_virtual_list_offset_at(list, 3, &position) == NX_OK &&
          near(position, 52.0));
    nx_virtual_range range;
    CHECK(nx_virtual_list_range(list, offset, 20, 0, &range) == NX_OK &&
          range.first == 3 && near(range.effective_offset, 57.0));

    const uint64_t removed_keys[] = {9, 1, 2, 4, 5};
    const double removed_heights[] = {12, 10, 30, 10, 10};
    rows = (measured_rows){removed_keys, removed_heights, SIZE_MAX};
    CHECK(nx_virtual_list_rebuild(list, 5, measure_row, &rows,
                                  20.0, &offset) == NX_OK);
    CHECK(near(offset, 52.0)); /* Missing key falls back to clamped offset. */

    const uint64_t bad_keys[] = {9, 1, 1, 4, 5};
    rows.keys = bad_keys;
    CHECK(nx_virtual_list_rebuild(list, 5, measure_row, &rows,
                                  20.0, &offset) == NX_ERROR_ARGUMENT);
    CHECK(near(offset, 52.0));
    CHECK(nx_virtual_list_offset_at(list, 5, &position) == NX_OK &&
          near(position, 72.0));
    const double bad_heights[] = {12, 0, 30, 10, 10};
    rows.keys = removed_keys;
    rows.heights = bad_heights;
    CHECK(nx_virtual_list_rebuild(list, 5, measure_row, &rows,
                                  20.0, &offset) == NX_ERROR_ARGUMENT);
    CHECK(near(offset, 52.0));
    rows.heights = removed_heights;
    rows.keys = removed_keys;
    rows.fail_at = 3;
    CHECK(nx_virtual_list_rebuild(list, 5, measure_row, &rows,
                                  20.0, &offset) == NX_ERROR_STATE);
    CHECK(near(offset, 52.0));
    rows.fail_at = SIZE_MAX;
    probe.fail_size = 7u * sizeof(uint64_t);
    probe.fail_once = 1;
    CHECK(nx_virtual_list_rebuild(list, 7, measure_row, &rows,
                                  20.0, &offset) == NX_ERROR_MEMORY);
    CHECK(near(offset, 52.0));
    CHECK(nx_virtual_list_offset_at(list, 5, &position) == NX_OK &&
          near(position, 72.0));

    const uint64_t reordered_keys[] = {4, 9, 1, 2, 5};
    const double reordered_heights[] = {10, 12, 10, 30, 10};
    rows = (measured_rows){reordered_keys, reordered_heights, SIZE_MAX};
    CHECK(nx_virtual_list_rebuild(list, 5, measure_row, &rows,
                                  20.0, &offset) == NX_OK);
    CHECK(near(offset, 0.0)); /* Former top row key 4 moved to index zero. */
    visit_probe probe_rows = {0};
    probe_rows.fail_at = SIZE_MAX;
    nx_virtual_range visited_range;
    size_t visited;
    CHECK(nx_virtual_list_visit(list, offset, 20, 0, 2,
                                record_row, &probe_rows, &visited_range,
                                &visited) == NX_OK);
    CHECK(visited == 2 && probe_rows.first_key == 4 &&
          probe_rows.last_key == 9);

    CHECK(nx_virtual_list_rebuild(list, 0, NULL, NULL,
                                  20.0, &offset) == NX_OK);
    CHECK(near(offset, 0.0));
    CHECK(nx_virtual_list_range(list, 0, 20, 0, &range) == NX_OK &&
          range.end == 0 && near(range.total_extent, 0.0));
    nx_virtual_list_destroy(list);
    return 0;
}

static int test_scroll_failure_retry(void) {
    failure_probe probe = {4u * sizeof(nx_command), 1};
    nx_context_desc context_desc = {0};
    context_desc.initial_command_capacity = 2;
    context_desc.allocator = (nx_allocator){&probe, probe_allocate,
                                             probe_deallocate};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&context_desc, &ctx) == NX_OK);
    nx_scroll_area_desc desc = scroll_desc(1, 0,
        (nx_rect){0, 0, 100, 100}, (nx_size){100, 500});
    float x = 0, y = 10;
    nx_scroll_info info;
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    CHECK(nx_scroll_area_begin(ctx, &desc, &x, &y, &info) ==
          NX_ERROR_MEMORY);
    CHECK(near(y, 10.0) && info.viewport.width == 0);
    CHECK(nx_scroll_area_begin(ctx, &desc, &x, &y, &info) == NX_OK);
    CHECK(nx_push_clip(ctx, (nx_rect){0, 0, 50, 50}) == NX_OK);
    CHECK(nx_scroll_area_end(ctx) == NX_ERROR_STATE);
    CHECK(nx_pop_clip(ctx) == NX_OK);
    CHECK(nx_scroll_area_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 1);
    nx_context_destroy(ctx);
    return 0;
}

static int test_scroll_end_failure_retry(void) {
    failure_probe probe = {8u * sizeof(nx_command), 1};
    nx_context_desc context_desc = {0};
    context_desc.initial_command_capacity = 4;
    context_desc.allocator = (nx_allocator){&probe, probe_allocate,
                                             probe_deallocate};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&context_desc, &ctx) == NX_OK);
    nx_scroll_area_desc desc = scroll_desc(1, 0,
        (nx_rect){0, 0, 100, 100}, (nx_size){100, 500});
    desc.thumb_y_id = 3;
    float x = 0, y = 0;
    nx_scroll_info info;
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    CHECK(nx_scroll_area_begin(ctx, &desc, &x, &y, &info) == NX_OK);
    CHECK(nx_scroll_area_end(ctx) == NX_ERROR_MEMORY);
    CHECK(nx_scroll_area_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 2);
    const nx_command *commands;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count == 5);
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_virtual_million() || test_virtual_rebuild_anchor() ||
        test_virtual_visit_build() ||
        test_touch_scroll_capture() || test_touch_tap_vs_scroll() ||
        test_scroll_area_and_layout() ||
        test_nested_wheel_routing() || test_scroll_failure_retry() ||
        test_scrollbar_drag_keyboard() ||
        test_horizontal_scrollbar_and_disabled() ||
        test_scroll_end_failure_retry()) return 1;
    puts("scroll and virtual list tests passed");
    return 0;
}
