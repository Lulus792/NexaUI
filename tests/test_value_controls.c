#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static nx_event pointer(nx_event_type type, float x, float y) {
    nx_event event = {0};
    event.type = type;
    event.button = NX_BUTTON_LEFT;
    event.x = x;
    event.y = y;
    return event;
}

static int queue_pointer(nx_context *ctx, nx_event_type type,
                         float x, float y) {
    nx_event event = pointer(type, x, y);
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int queue_key(nx_context *ctx, nx_key key) {
    nx_event down = {0}, up = {0};
    down.type = NX_EVENT_KEY_DOWN;
    down.key = key;
    up.type = NX_EVENT_KEY_UP;
    up.key = key;
    return nx_context_queue_event(ctx, &down) == NX_OK &&
           nx_context_queue_event(ctx, &up) == NX_OK;
}

static int semantic_value(nx_context *ctx, uint64_t id, const char *expected,
                          int disabled) {
    uint32_t index = UINT32_MAX;
    nx_semantic_view view;
    size_t length = strlen(expected);
    return nx_semantics_find(ctx, id, &index) == NX_OK &&
           nx_semantics_get(ctx, index, &view) == NX_OK &&
           view.role == NX_ROLE_SLIDER && view.value.length == length &&
           memcmp(view.value.data, expected, length) == 0 &&
           !!(view.state & NX_SEM_DISABLED) == !!disabled;
}

static int test_slider_and_drag(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_value_control_desc slider = {0}, drag = {0};
    slider.control.id = 1;
    slider.control.bounds = (nx_rect){0, 0, 100, 10};
    slider.control.name = (nx_utf8_span){"Slider", 6};
    slider.minimum = 0;
    slider.maximum = 100;
    slider.step = 5;
    drag = slider;
    drag.control.id = 2;
    drag.control.bounds = (nx_rect){0, 20, 100, 10};
    drag.control.name = (nx_utf8_span){"Drag value", 10};
    drag.units_per_pixel = 2;
    double slider_value = 0, drag_value = 50;
    const char *slider_expected[] = {"0", "75", "100", "100", "100",
                                     "55", "0", "100", "100"};
    const char *drag_expected[] = {"50", "50", "50", "70", "0",
                                   "0", "0", "0", "0"};
    for (unsigned frame = 0; frame < 9; ++frame) {
        if (frame == 1) {
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_DOWN, 10, 5));
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_MOVE, 75, 5));
        }
        if (frame == 2) {
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_MOVE, 150, 5));
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_UP, 150, 5));
        }
        if (frame == 3) {
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_DOWN, 50, 25));
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_MOVE, 60, 25));
        }
        if (frame == 4) {
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_MOVE, 30, 25));
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_UP, -10, 25));
        }
        if (frame == 5) {
            slider_value = 50;
            CHECK(nx_focus_set(ctx, 1) == NX_OK);
            CHECK(queue_key(ctx, NX_KEY_RIGHT));
        }
        if (frame == 6) CHECK(queue_key(ctx, NX_KEY_HOME));
        if (frame == 7) CHECK(queue_key(ctx, NX_KEY_END));
        if (frame == 8) {
            slider.control.disabled = 1;
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_DOWN, 25, 5));
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_UP, 25, 5));
        }
        CHECK(nx_begin_frame(ctx, 160, 40, 0) == NX_OK);
        nx_interaction_state slider_state, drag_state;
        if (frame == 0) {
            nx_value_control_desc invalid = slider;
            invalid.maximum = invalid.minimum;
            CHECK(nx_slider(ctx, &invalid, &slider_value,
                            &slider_state) == NX_ERROR_ARGUMENT);
            invalid = drag;
            invalid.units_per_pixel = 0;
            CHECK(nx_drag_value(ctx, &invalid, &drag_value,
                                &drag_state) == NX_ERROR_ARGUMENT);
        }
        CHECK(nx_slider(ctx, &slider, &slider_value, &slider_state) == NX_OK);
        CHECK(nx_drag_value(ctx, &drag, &drag_value, &drag_state) == NX_OK);
        if (frame == 1) CHECK(slider_state.pointer_pressed &&
                              slider_state.held &&
                              slider_state.drag_delta_x == 65 &&
                              slider_state.pointer_x == 75);
        if (frame == 2) CHECK(slider_state.pointer_released &&
                              !slider_state.held &&
                              slider_state.drag_delta_x == 75 &&
                              slider_state.pointer_x == 150);
        if (frame == 3) CHECK(drag_state.pointer_pressed &&
                              drag_state.held && drag_state.drag_delta_x == 10);
        if (frame == 4) CHECK(drag_state.pointer_released &&
                              drag_state.drag_delta_x == -70 &&
                              drag_state.pointer_x == -10);
        if (frame == 8) CHECK(!slider_state.pointer_pressed &&
                              !slider_state.pointer_released &&
                              !slider_state.focused);
        CHECK(nx_end_frame(ctx) == NX_OK);
        CHECK(semantic_value(ctx, 1, slider_expected[frame], frame == 8));
        CHECK(semantic_value(ctx, 2, drag_expected[frame], 0));
        const nx_command *commands = NULL;
        size_t count = 0;
        CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count >= 4);
        uint8_t pixels[160u * 40u * 4u];
        CHECK(nx_headless_render(commands, count, pixels, 160, 40,
                                 160u * 4u, (nx_color){0, 0, 0, 0}) == NX_OK);
        CHECK(pixels[((size_t)5 * 160u + 50u) * 4u + 3u] != 0);
    }
    nx_context_destroy(ctx);
    return 0;
}

typedef struct allocation_probe {
    size_t fail_size;
    unsigned allocations, deallocations, failures;
} allocation_probe;

static void *probe_allocate(void *user, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    if (size == probe->fail_size) {
        ++probe->failures;
        return NULL;
    }
    ++probe->allocations;
    return malloc(size);
}

static void probe_deallocate(void *user, void *pointer, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    (void)size;
    ++probe->deallocations;
    free(pointer);
}

static int test_semantic_value_growth_failure(void) {
    allocation_probe probe = {0};
    nx_context_desc context_desc = {0};
    context_desc.allocator = (nx_allocator){&probe, probe_allocate,
                                             probe_deallocate};
    context_desc.initial_semantic_text_capacity = 4;
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&context_desc, &ctx) == NX_OK);
    nx_value_control_desc slider = {0};
    slider.control.id = 1;
    slider.control.bounds = (nx_rect){0, 0, 10, 10};
    slider.control.name = (nx_utf8_span){"S", 1};
    slider.minimum = 0;
    slider.maximum = 10;
    slider.step = 1;
    double value = 0;
    nx_interaction_state state;
    CHECK(nx_begin_frame(ctx, 20, 20, 0) == NX_OK);
    CHECK(nx_slider(ctx, &slider, &value, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(queue_pointer(ctx, NX_EVENT_POINTER_DOWN, 9, 5));
    CHECK(queue_pointer(ctx, NX_EVENT_POINTER_UP, 9, 5));
    CHECK(nx_begin_frame(ctx, 20, 20, 0) == NX_OK);
    probe.fail_size = 8;
    CHECK(nx_slider(ctx, &slider, &value, &state) == NX_ERROR_MEMORY);
    CHECK(probe.failures == 1 && value == 0 && !state.clicked);
    probe.fail_size = 0;
    CHECK(nx_slider(ctx, &slider, &value, &state) == NX_OK && value == 9);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count = 0;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 1);
    CHECK(semantic_value(ctx, 1, "9", 0));
    nx_context_destroy(ctx);
    CHECK(probe.allocations == probe.deallocations);
    return 0;
}

static int test_nondivisible_endpoint(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_value_control_desc slider = {0};
    slider.control.id = 3;
    slider.control.bounds = (nx_rect){0, 0, 20, 10};
    slider.control.name = (nx_utf8_span){"Range", 5};
    slider.minimum = 0;
    slider.maximum = 10;
    slider.step = 3;
    double value = 0;
    nx_interaction_state state;
    CHECK(nx_begin_frame(ctx, 30, 20, 0) == NX_OK);
    CHECK(nx_slider(ctx, &slider, &value, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_focus_set(ctx, 3) == NX_OK);
    CHECK(queue_key(ctx, NX_KEY_END));
    CHECK(nx_begin_frame(ctx, 30, 20, 0) == NX_OK);
    CHECK(nx_slider(ctx, &slider, &value, &state) == NX_OK && value == 10);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(semantic_value(ctx, 3, "10", 0));
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_slider_and_drag()) return 1;
    if (test_semantic_value_growth_failure()) return 1;
    if (test_nondivisible_endpoint()) return 1;
    puts("value control tests passed");
    return 0;
}
