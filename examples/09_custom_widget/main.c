#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 240u
#define HEIGHT 100u

typedef struct demo_state {
    unsigned progress;
} demo_state;

static int save_frame(nx_context *ui, const char *prefix, unsigned frame) {
    char path[512];
    int length = snprintf(path, sizeof(path), "%s_%u.ppm", prefix, frame);
    if (length < 0 || (size_t)length >= sizeof(path)) return 0;
    const nx_command *commands = NULL;
    size_t count = 0;
    if (nx_frame_commands(ui, &commands, &count) != NX_OK) return 0;
    uint8_t *pixels = (uint8_t *)malloc((size_t)WIDTH * HEIGHT * 4u);
    if (!pixels) return 0;
    int okay = nx_headless_render(commands, count, pixels, WIDTH, HEIGHT,
                                 WIDTH * 4u, (nx_color){1, 1, 1, 1}) == NX_OK;
    FILE *file = okay ? fopen(path, "wb") : NULL;
    if (!file) okay = 0;
    if (okay && fprintf(file, "P6\n%u %u\n255\n", WIDTH, HEIGHT) < 0) okay = 0;
    for (size_t i = 0; okay && i < (size_t)WIDTH * HEIGHT; ++i)
        if (fwrite(pixels + i * 4u, 1, 3, file) != 3) okay = 0;
    if (file && fclose(file) != 0) okay = 0;
    free(pixels);
    return okay;
}

static nx_result draw_action(nx_context *ui, uint64_t parent_id,
                             nx_rect bounds, demo_state *model) {
    nx_widget_desc widget = {0};
    widget.semantic.id = nx_id(ui, "advance", sizeof("advance") - 1u);
    widget.semantic.parent_id = parent_id;
    widget.semantic.role = NX_ROLE_BUTTON;
    widget.semantic.bounds = bounds;
    widget.semantic.name = (nx_utf8_span){"Advance progress", 16};
    widget.semantic.actions = NX_ACTION_PRESS | NX_ACTION_FOCUS;
    widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    nx_interaction_state state;
    nx_result result = nx_widget_register(ui, &widget, &state);
    if (result != NX_OK) return result;
    if (state.clicked) model->progress = (model->progress + 25u) % 125u;
    nx_color accent, pressed, focus, icon;
    float radius, focus_width;
    if (nx_theme_color(ui, NX_COLOR_ACCENT, &accent) != NX_OK ||
        nx_theme_color(ui, NX_COLOR_PRESSED, &pressed) != NX_OK ||
        nx_theme_color(ui, NX_COLOR_FOCUS, &focus) != NX_OK ||
        nx_theme_color(ui, state.held ? NX_COLOR_TEXT :
                       NX_COLOR_ACCENT_TEXT, &icon) != NX_OK ||
        nx_theme_metric(ui, NX_METRIC_RADIUS_MD, &radius) != NX_OK ||
        nx_theme_metric(ui, NX_METRIC_FOCUS_WIDTH, &focus_width) != NX_OK)
        return NX_ERROR_STATE;
    result = nx_fill_round_rect(ui, bounds, radius, state.held ? pressed : accent);
    if (result != NX_OK) return result;
    if (state.focused) result = nx_stroke_rect(ui, bounds, focus_width, focus);
    if (result != NX_OK) return result;
    float cx = bounds.x + bounds.width * 0.5f;
    float cy = bounds.y + bounds.height * 0.5f;
    return nx_fill_triangle(ui, (nx_point){cx - 5, cy - 8},
                            (nx_point){cx - 5, cy + 8},
                            (nx_point){cx + 8, cy}, icon);
}

static nx_result draw_progress(nx_context *ui, uint64_t parent_id,
                               nx_rect bounds, const demo_state *model) {
    char value[32];
    int length = snprintf(value, sizeof(value), "%u percent", model->progress);
    if (length < 0 || (size_t)length >= sizeof(value)) return NX_ERROR_LIMIT;
    nx_widget_desc widget = {0};
    widget.semantic.id = nx_id(ui, "progress", sizeof("progress") - 1u);
    widget.semantic.parent_id = parent_id;
    widget.semantic.role = NX_ROLE_PROGRESS;
    widget.semantic.bounds = bounds;
    widget.semantic.name = (nx_utf8_span){"Completion", 10};
    widget.semantic.value = (nx_utf8_span){value, (size_t)length};
    nx_interaction_state state;
    nx_result result = nx_widget_register(ui, &widget, &state);
    if (result != NX_OK) return result;
    nx_color track, fill;
    float radius;
    if (nx_theme_color(ui, NX_COLOR_SURFACE_RAISED, &track) != NX_OK ||
        nx_theme_color(ui, NX_COLOR_ACCENT, &fill) != NX_OK ||
        nx_theme_metric(ui, NX_METRIC_RADIUS_SM, &radius) != NX_OK)
        return NX_ERROR_STATE;
    result = nx_fill_round_rect(ui, bounds, radius, track);
    if (result != NX_OK) return result;
    nx_rect filled = bounds;
    filled.width *= (float)model->progress / 100.0f;
    return nx_fill_round_rect(ui, filled, radius, fill);
}

static int draw_frame(nx_context *ui, nx_rect action, nx_rect progress,
                       demo_state *model, const char *prefix, unsigned frame) {
    if (nx_begin_frame(ui, WIDTH, HEIGHT, 1.0 / 60.0) != NX_OK) return 0;
    uint64_t root_id = nx_id(ui, "window", sizeof("window") - 1u);
    uint64_t progress_id = nx_id(ui, "progress", sizeof("progress") - 1u);
    nx_semantic_desc root = {0};
    root.id = root_id;
    root.role = NX_ROLE_WINDOW;
    root.bounds = (nx_rect){0, 0, WIDTH, HEIGHT};
    root.name = (nx_utf8_span){"Custom widget demo", 18};
    if (nx_semantics_add(ui, &root, NULL) != NX_OK ||
        draw_action(ui, root_id, action, model) != NX_OK ||
        draw_progress(ui, root_id, progress, model) != NX_OK ||
        nx_end_frame(ui) != NX_OK) return 0;
    uint32_t progress_index = UINT32_MAX;
    nx_semantic_view view;
    if (nx_semantics_find(ui, progress_id, &progress_index) != NX_OK ||
        nx_semantics_get(ui, progress_index, &view) != NX_OK ||
        view.role != NX_ROLE_PROGRESS || !view.value.data) return 0;
    char expected[32];
    int length = snprintf(expected, sizeof(expected), "%u percent", model->progress);
    if (length < 0 || (size_t)length >= sizeof(expected) ||
        view.value.length != (size_t)length ||
        memcmp(view.value.data, expected, (size_t)length) != 0) return 0;
    printf("frame %u: semantic %u = %.*s\n", frame, progress_index,
           (int)view.value.length, view.value.data);
    return save_frame(ui, prefix, frame);
}

int main(int argc, char **argv) {
    const char *prefix = argc > 1 ? argv[1] : "custom_widget";
    nx_layout *layout = NULL;
    nx_context *ui = NULL;
    int okay = nx_layout_create(NULL, 0, &layout) == NX_OK &&
               nx_context_create(NULL, &ui) == NX_OK;
    nx_rect action = {0}, progress = {0};
    if (okay) {
        nx_layout_desc column = nx_layout_default_desc(NX_LAYOUT_COLUMN);
        column.padding = (nx_insets){10, 10, 10, 10};
        column.gap = 10;
        nx_layout_desc button = nx_layout_default_desc(NX_LAYOUT_LEAF);
        button.width.preferred = 180;
        button.width.has_preferred = 1;
        button.height.preferred = 40;
        button.height.has_preferred = 1;
        nx_layout_desc bar = button;
        bar.height.preferred = 12;
        uint32_t root, button_node, bar_node;
        nx_size desired;
        okay = nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &column, &root) == NX_OK &&
               nx_layout_add(layout, root, &button, &button_node) == NX_OK &&
               nx_layout_add(layout, root, &bar, &bar_node) == NX_OK &&
               nx_layout_measure(layout, root, WIDTH, HEIGHT, &desired) == NX_OK &&
               nx_layout_arrange(layout, root, (nx_rect){20, 8, 200, 84}) == NX_OK &&
               nx_layout_get_rect(layout, button_node, &action) == NX_OK &&
               nx_layout_get_rect(layout, bar_node, &progress) == NX_OK;
    }
    nx_theme light;
    if (okay) okay = nx_theme_preset(NX_THEME_LIGHT, &light) == NX_OK &&
                     nx_context_set_theme(ui, &light) == NX_OK;
    demo_state model = {0};
    if (okay) okay = draw_frame(ui, action, progress, &model, prefix, 0);
    nx_event event = {0};
    event.type = NX_EVENT_POINTER_DOWN;
    event.button = NX_BUTTON_LEFT;
    event.x = action.x + action.width / 2.0f;
    event.y = action.y + action.height / 2.0f;
    if (okay) okay = nx_context_queue_event(ui, &event) == NX_OK;
    if (okay) okay = draw_frame(ui, action, progress, &model, prefix, 1);
    event.type = NX_EVENT_POINTER_UP;
    if (okay) okay = nx_context_queue_event(ui, &event) == NX_OK;
    if (okay) okay = draw_frame(ui, action, progress, &model, prefix, 2);
    if (okay) okay = model.progress == 25;
    event = (nx_event){0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = NX_KEY_SPACE;
    if (okay) okay = nx_context_queue_event(ui, &event) == NX_OK;
    event.type = NX_EVENT_KEY_UP;
    if (okay) okay = nx_context_queue_event(ui, &event) == NX_OK;
    if (okay) okay = draw_frame(ui, action, progress, &model, prefix, 3);
    if (okay) okay = model.progress == 50;
    nx_context_destroy(ui);
    nx_layout_destroy(layout);
    if (!okay) fprintf(stderr, "Custom widget example failed\n");
    return okay ? 0 : 1;
}
