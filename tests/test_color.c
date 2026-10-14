#include "nexaui/nexaui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
    return 1; \
} } while (0)

static int close_color(nx_color a, nx_color b, float tolerance) {
    return fabsf(a.r - b.r) <= tolerance &&
           fabsf(a.g - b.g) <= tolerance &&
           fabsf(a.b - b.b) <= tolerance &&
           fabsf(a.a - b.a) <= tolerance;
}

static int test_conversions(void) {
    nx_hsv hsv = {9, 9, 9, 9};
    CHECK(nx_color_to_hsv((nx_color){1, 0, 0, 1}, &hsv) == NX_OK);
    CHECK(hsv.h == 0 && hsv.s == 1 && hsv.v == 1 && hsv.a == 1);
    CHECK(nx_color_to_hsv((nx_color){0, 1, 0, 0.5f}, &hsv) == NX_OK);
    CHECK(fabsf(hsv.h - 1.0f / 3.0f) < 0.00001f && hsv.a == 0.5f);
    CHECK(nx_color_to_hsv((nx_color){0.4f, 0.4f, 0.4f, 1}, &hsv) == NX_OK);
    CHECK(hsv.h == 0 && hsv.s == 0 && hsv.v == 0.4f);
    nx_hsv unchanged = hsv;
    CHECK(nx_color_to_hsv((nx_color){NAN, 0, 0, 1}, &hsv) == NX_ERROR_ARGUMENT);
    CHECK(memcmp(&hsv, &unchanged, sizeof(hsv)) == 0);
    nx_color rgb = {9, 9, 9, 9};
    CHECK(nx_color_from_hsv((nx_hsv){1, 1, 1, 0.25f}, &rgb) == NX_OK);
    CHECK(close_color(rgb, (nx_color){1, 0, 0, 0.25f}, 0.00001f));
    nx_color prior = rgb;
    CHECK(nx_color_from_hsv((nx_hsv){0, 1.1f, 1, 1}, &rgb) == NX_ERROR_ARGUMENT);
    CHECK(memcmp(&rgb, &prior, sizeof(rgb)) == 0);
    for (unsigned r = 0; r < 5; ++r)
        for (unsigned g = 0; g < 5; ++g)
            for (unsigned b = 0; b < 5; ++b) {
                nx_color source = {r / 4.0f, g / 4.0f, b / 4.0f, 0.6f};
                CHECK(nx_color_to_hsv(source, &hsv) == NX_OK);
                CHECK(nx_color_from_hsv(hsv, &rgb) == NX_OK);
                CHECK(close_color(source, rgb, 0.00001f));
            }
    CHECK(nx_color_parse_hex((nx_utf8_span){"#F0A", 4}, &rgb) == NX_OK);
    CHECK(close_color(rgb, (nx_color){1, 0, 170.0f / 255.0f, 1}, 0.00001f));
    CHECK(nx_color_parse_hex((nx_utf8_span){"#11223344", 9}, &rgb) == NX_OK);
    char hex[10] = "untouched";
    size_t length = 0;
    CHECK(nx_color_format_hex(rgb, 1, NULL, 0, &length) == NX_OK && length == 9);
    CHECK(nx_color_format_hex(rgb, 1, hex, 9, &length) == NX_ERROR_LIMIT);
    CHECK(memcmp(hex, "untouched", 9) == 0);
    CHECK(nx_color_format_hex(rgb, 1, hex, sizeof(hex), &length) == NX_OK);
    CHECK(length == 9 && strcmp(hex, "#11223344") == 0);
    prior = rgb;
    CHECK(nx_color_parse_hex((nx_utf8_span){"#12GG34", 7}, &rgb) ==
          NX_ERROR_ARGUMENT);
    CHECK(memcmp(&rgb, &prior, sizeof(rgb)) == 0);
    CHECK(nx_color_format_hex((nx_color){INFINITY, 0, 0, 1}, 0,
                              hex, sizeof(hex), &length) == NX_ERROR_ARGUMENT);
    return 0;
}

static int pointer_click(nx_context *ctx, float x, float y) {
    nx_event event = {0};
    event.button = NX_BUTTON_LEFT;
    event.x = x;
    event.y = y;
    event.type = NX_EVENT_POINTER_DOWN;
    if (nx_context_queue_event(ctx, &event) != NX_OK) return 0;
    event.type = NX_EVENT_POINTER_UP;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int key(nx_context *ctx, nx_key code) {
    nx_event event = {0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = code;
    if (nx_context_queue_event(ctx, &event) != NX_OK) return 0;
    event.type = NX_EVENT_KEY_UP;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static nx_color_picker_desc picker_desc(void) {
    nx_color_picker_desc d = {0};
    d.id = 10;
    d.square_id = 11;
    d.hue_id = 12;
    d.alpha_id = 13;
    d.preview_id = 14;
    d.name = (nx_utf8_span){"Color", 5};
    d.square_name = (nx_utf8_span){"Saturation and brightness", 25};
    d.hue_name = (nx_utf8_span){"Hue", 3};
    d.alpha_name = (nx_utf8_span){"Opacity", 7};
    d.preview_name = (nx_utf8_span){"Selected color", 14};
    d.bounds = (nx_rect){0, 0, 130, 90};
    d.square_bounds = (nx_rect){0, 0, 80, 60};
    d.hue_bounds = (nx_rect){0, 62, 80, 10};
    d.alpha_bounds = (nx_rect){0, 74, 80, 10};
    d.preview_bounds = (nx_rect){90, 0, 30, 30};
    d.show_alpha = 1;
    return d;
}

static nx_theme_selector_desc selector_desc(void) {
    nx_theme_selector_desc d = {0};
    d.id = 20;
    d.name = (nx_utf8_span){"Theme", 5};
    d.bounds = (nx_rect){0, 100, 150, 40};
    d.option_ids[0] = 21;
    d.option_ids[1] = 22;
    d.option_ids[2] = 23;
    d.option_names[0] = (nx_utf8_span){"Dark", 4};
    d.option_names[1] = (nx_utf8_span){"Light", 5};
    d.option_names[2] = (nx_utf8_span){"High contrast", 13};
    return d;
}

static int test_controls(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_color_picker_desc picker = picker_desc();
    nx_theme_selector_desc selector = selector_desc();
    nx_color_picker_state state = {0};
    nx_color color = {1, 0, 0, 1};
    nx_theme_kind theme = NX_THEME_DARK;
    int color_changed = -1, theme_changed = -1;
    CHECK(nx_begin_frame(ctx, 160, 150, 0) == NX_OK);
    CHECK(nx_color_picker(ctx, &picker, &state, &color, &color_changed) == NX_OK);
    CHECK(nx_theme_selector(ctx, &selector, &theme, &theme_changed) == NX_OK);
    CHECK(!color_changed && !theme_changed && state.initialized);
    CHECK(nx_end_frame(ctx) == NX_OK);
    uint32_t index = 0;
    nx_semantic_view view;
    CHECK(nx_semantics_find(ctx, 10, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
          view.role == NX_ROLE_GROUP && view.value.length == 9 &&
          memcmp(view.value.data, "#FF0000FF", 9) == 0);
    const nx_command *commands = NULL;
    size_t command_count = 0;
    uint8_t pixels[160u * 150u * 4u];
    CHECK(nx_frame_commands(ctx, &commands, &command_count) == NX_OK);
    CHECK(nx_headless_render(commands, command_count, pixels, 160, 150,
        160u * 4u, (nx_color){0, 0, 0, 0}) == NX_OK);
    CHECK(pixels[((size_t)15 * 160u + 105u) * 4u] > 200);

    CHECK(pointer_click(ctx, 40, 30));
    CHECK(nx_begin_frame(ctx, 160, 150, 0) == NX_OK);
    CHECK(nx_color_picker(ctx, &picker, &state, &color, &color_changed) == NX_OK);
    CHECK(color_changed && fabsf(color.r - 0.5f) < 0.001f &&
          fabsf(color.g - 0.25f) < 0.001f);
    CHECK(nx_theme_selector(ctx, &selector, &theme, &theme_changed) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(pointer_click(ctx, 27, 67));
    CHECK(nx_begin_frame(ctx, 160, 150, 0) == NX_OK);
    CHECK(nx_color_picker(ctx, &picker, &state, &color, &color_changed) == NX_OK);
    CHECK(color_changed && state.hue > 0.3f && state.hue < 0.4f);
    CHECK(nx_theme_selector(ctx, &selector, &theme, &theme_changed) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);

    CHECK(nx_focus_set(ctx, 13) == NX_OK);
    CHECK(key(ctx, NX_KEY_HOME));
    CHECK(nx_begin_frame(ctx, 160, 150, 0) == NX_OK);
    CHECK(nx_color_picker(ctx, &picker, &state, &color, &color_changed) == NX_OK);
    CHECK(color_changed && color.a == 0);
    CHECK(nx_theme_selector(ctx, &selector, &theme, &theme_changed) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    picker.disabled = 1;
    CHECK(key(ctx, NX_KEY_END));
    CHECK(nx_begin_frame(ctx, 160, 150, 0) == NX_OK);
    CHECK(nx_color_picker(ctx, &picker, &state, &color, &color_changed) == NX_OK);
    CHECK(!color_changed && color.a == 0);
    CHECK(nx_theme_selector(ctx, &selector, &theme, &theme_changed) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    picker.disabled = 0;

    CHECK(nx_focus_set(ctx, 21) == NX_OK);
    CHECK(key(ctx, NX_KEY_RIGHT));
    CHECK(nx_begin_frame(ctx, 160, 150, 0) == NX_OK);
    CHECK(nx_color_picker(ctx, &picker, &state, &color, &color_changed) == NX_OK);
    CHECK(nx_theme_selector(ctx, &selector, &theme, &theme_changed) == NX_OK);
    CHECK(theme_changed && theme == NX_THEME_LIGHT);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 22);
    CHECK(nx_semantics_find(ctx, 22, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
          (view.state & NX_SEM_CHECKED));
    nx_theme preset;
    nx_color current;
    CHECK(nx_theme_preset(theme, &preset) == NX_OK);
    CHECK(nx_context_set_theme(ctx, &preset) == NX_OK);
    CHECK(nx_theme_color(ctx, NX_COLOR_BACKGROUND, &current) == NX_OK);
    CHECK(close_color(current, preset.colors[NX_COLOR_BACKGROUND], 0));
    CHECK(key(ctx, NX_KEY_END));
    CHECK(nx_begin_frame(ctx, 160, 150, 0) == NX_OK);
    CHECK(nx_color_picker(ctx, &picker, &state, &color, &color_changed) == NX_OK);
    CHECK(nx_theme_selector(ctx, &selector, &theme, &theme_changed) == NX_OK);
    CHECK(theme_changed && theme == NX_THEME_HIGH_CONTRAST);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 23);
    CHECK(pointer_click(ctx, 20, 115));
    CHECK(nx_begin_frame(ctx, 160, 150, 0) == NX_OK);
    CHECK(nx_color_picker(ctx, &picker, &state, &color, &color_changed) == NX_OK);
    CHECK(nx_theme_selector(ctx, &selector, &theme, &theme_changed) == NX_OK);
    CHECK(theme_changed && theme == NX_THEME_DARK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    selector.disabled = 1;
    CHECK(key(ctx, NX_KEY_RIGHT));
    CHECK(nx_begin_frame(ctx, 160, 150, 0) == NX_OK);
    CHECK(nx_color_picker(ctx, &picker, &state, &color, &color_changed) == NX_OK);
    CHECK(nx_theme_selector(ctx, &selector, &theme, &theme_changed) == NX_OK);
    CHECK(!theme_changed && theme == NX_THEME_DARK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_context_destroy(ctx);
    return 0;
}

typedef struct failing_allocator {
    size_t fail_size;
    unsigned failures;
} failing_allocator;

static void *test_allocate(void *user, size_t size) {
    failing_allocator *allocator = (failing_allocator *)user;
    if (allocator->fail_size == size) {
        ++allocator->failures;
        return NULL;
    }
    return malloc(size);
}

static void test_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static int test_paint_retry(void) {
    failing_allocator allocator = {0};
    nx_context_desc config = {0};
    config.allocator = (nx_allocator){&allocator, test_allocate, test_deallocate};
    config.initial_command_capacity = 2;
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&config, &ctx) == NX_OK);
    nx_color_picker_desc desc = picker_desc();
    nx_color_picker_state state = {0};
    nx_color color = {1, 0, 0, 1};
    int changed = 0;
    nx_interaction_state region;
    CHECK(nx_begin_frame(ctx, 160, 100, 0) == NX_OK);
    CHECK(nx_interact_region(ctx, desc.square_id, desc.square_bounds,
                             NX_REGION_FOCUSABLE, &region) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(pointer_click(ctx, 40, 30));
    CHECK(nx_begin_frame(ctx, 160, 100, 0) == NX_OK);
    allocator.fail_size = 4u * sizeof(nx_command);
    CHECK(nx_color_picker(ctx, &desc, &state, &color, &changed) ==
          NX_ERROR_MEMORY);
    CHECK(allocator.failures == 1 && !changed && !state.initialized &&
          color.r == 1 && color.g == 0);
    allocator.fail_size = 0;
    CHECK(nx_color_picker(ctx, &desc, &state, &color, &changed) == NX_OK);
    CHECK(changed && fabsf(color.r - 0.5f) < 0.001f &&
          fabsf(color.g - 0.25f) < 0.001f);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count = 0;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 5);
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    CHECK(test_conversions() == 0);
    CHECK(test_controls() == 0);
    CHECK(test_paint_retry() == 0);
    return 0;
}
