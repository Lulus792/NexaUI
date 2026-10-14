#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 760u
#define HEIGHT 340u
#define FONT_LIMIT (64u * 1024u * 1024u)
#define GLYPH_CACHE (4u * 1024u * 1024u)

typedef struct scene_snapshot {
    int light_enabled;
    double light_intensity;
} scene_snapshot;

typedef struct inspector {
    nx_context *ui;
    nx_font_collection *fonts;
    nx_text_layout *labels[3];
    nx_text_layout *headings[2];
    uint64_t selected_id;
    int scene_expanded;
} inspector;

static nx_utf8_span span(const char *text) {
    return (nx_utf8_span){text, strlen(text)};
}

static int save_frame(nx_context *ui, const char *path) {
    const nx_command *commands = NULL;
    size_t count = 0;
    if (nx_frame_commands(ui, &commands, &count) != NX_OK) return 0;
    uint8_t *pixels = malloc((size_t)WIDTH * HEIGHT * 4u);
    if (!pixels) return 0;
    int okay = nx_headless_render(commands, count, pixels, WIDTH, HEIGHT,
                                  WIDTH * 4u, (nx_color){0.04f, 0.06f, 0.09f, 1}) == NX_OK;
    FILE *file = okay ? fopen(path, "wb") : NULL;
    if (!file) okay = 0;
    if (okay && fprintf(file, "P6\n%u %u\n255\n", WIDTH, HEIGHT) < 0) okay = 0;
    for (size_t i = 0; okay && i < (size_t)WIDTH * HEIGHT; ++i)
        if (fwrite(pixels + i * 4u, 1, 3, file) != 3) okay = 0;
    if (file && fclose(file) != 0) okay = 0;
    free(pixels);
    return okay;
}

static int click(nx_context *ui, float x, float y) {
    nx_event event = {0};
    event.button = NX_BUTTON_LEFT;
    event.x = x;
    event.y = y;
    event.type = NX_EVENT_POINTER_DOWN;
    if (nx_context_queue_event(ui, &event) != NX_OK) return 0;
    event.type = NX_EVENT_POINTER_UP;
    return nx_context_queue_event(ui, &event) == NX_OK;
}

static int key(nx_context *ui, nx_key code) {
    nx_event event = {0};
    event.key = code;
    event.type = NX_EVENT_KEY_DOWN;
    if (nx_context_queue_event(ui, &event) != NX_OK) return 0;
    event.type = NX_EVENT_KEY_UP;
    return nx_context_queue_event(ui, &event) == NX_OK;
}

static nx_result draw_hierarchy(inspector *app) {
    nx_container_desc tree = {0};
    tree.id = 10;
    tree.bounds = (nx_rect){16, 54, 320, 266};
    tree.name = span("Scene hierarchy");
    nx_result result = nx_tree(app->ui, &tree);
    if (result != NX_OK) return result;
    nx_tree_node nodes[3] = {
        {SIZE_MAX, app->scene_expanded}, {0, 0}, {0, 0}
    };
    size_t visible[3], count = 0;
    result = nx_tree_flatten(nodes, 3, NULL, visible, 3, &count);
    if (result != NX_OK) return result;
    static const char *names[3] = {"Scene", "Cube", "Light"};
    for (size_t row = 0; row < count; ++row) {
        size_t index = visible[row];
        nx_tree_item_desc item = {0};
        item.control.id = 101u + index;
        item.control.parent_id = tree.id;
        item.control.bounds = (nx_rect){24, 82 + (float)row * 40, 304, 36};
        item.control.name = span(names[index]);
        item.control.label_layout = app->labels[index];
        item.control.label_origin = (nx_point){index ? 75 : 65,
                                                88 + (float)row * 40};
        item.control.glyph_cache_bytes = GLYPH_CACHE;
        item.has_children = index == 0;
        if (item.has_children) {
            item.expander_id = 201;
            item.expander_bounds = (nx_rect){32, 89, 24, 22};
        }
        nx_interaction_state state;
        result = nx_tree_item(app->ui, &item, &app->scene_expanded,
                              &app->selected_id, &state);
        if (result != NX_OK) return result;
    }
    return NX_OK;
}

static nx_result draw_properties(inspector *app,
                                 const scene_snapshot *snapshot,
                                 nx_property_action *intensity_action) {
    nx_container_desc group = {0};
    group.id = 20;
    group.bounds = (nx_rect){352, 54, 392, 266};
    group.name = span("Properties");
    nx_result result = nx_group(app->ui, &group);
    if (result != NX_OK) return result;
    nx_property_row_desc row = {0};
    row.parent_id = group.id;
    row.bounds = (nx_rect){364, 82, 368, 36};
    row.fonts = app->fonts;
    row.pixels_per_em = 14;
    row.glyph_cache_bytes = GLYPH_CACHE;
    nx_property_action action;
    if (app->selected_id == 103) {
        row.id = 301;
        row.name = span("Enabled");
        row.kind = NX_PROPERTY_BOOLEAN;
        row.boolean_value = snapshot->light_enabled;
        result = nx_property_row(app->ui, &row, &action);
        if (result != NX_OK) return result;
        row.id = 302;
        row.bounds.y += 44;
        row.name = span("Intensity");
        row.kind = NX_PROPERTY_NUMBER;
        row.minimum = 0;
        row.maximum = 10;
        row.step = 1;
        row.number_value = snapshot->light_intensity;
        row.unit = span("cd");
        result = nx_property_row(app->ui, &row, intensity_action);
        if (result != NX_OK) return result;
        row.id = 303;
        row.bounds.y += 44;
        row.name = span("Type");
        row.kind = NX_PROPERTY_TEXT;
        row.unit = (nx_utf8_span){0};
        row.text_value = span("Point");
        row.read_only = 1;
        return nx_property_row(app->ui, &row, &action);
    }
    row.id = 304;
    row.name = span("Material");
    row.kind = NX_PROPERTY_TEXT;
    row.text_value = span("Steel");
    row.read_only = 1;
    return nx_property_row(app->ui, &row, &action);
}

static int draw_frame(inspector *app, const scene_snapshot *snapshot,
                      nx_property_action *intensity_action,
                      const char *output) {
    *intensity_action = (nx_property_action){0};
    if (nx_begin_frame(app->ui, WIDTH, HEIGHT, 1.0 / 60.0) != NX_OK) return 0;
    nx_color surface, title;
    if (nx_theme_color(app->ui, NX_COLOR_SURFACE, &surface) != NX_OK ||
        nx_theme_color(app->ui, NX_COLOR_TEXT, &title) != NX_OK ||
        nx_fill_rect(app->ui, (nx_rect){0, 0, WIDTH, HEIGHT}, surface) != NX_OK ||
        nx_draw_text_layout(app->ui, app->headings[0],
                            (nx_point){24, 16}, title, GLYPH_CACHE) != NX_OK ||
        nx_draw_text_layout(app->ui, app->headings[1],
                            (nx_point){364, 16}, title, GLYPH_CACHE) != NX_OK ||
        draw_hierarchy(app) != NX_OK ||
        draw_properties(app, snapshot, intensity_action) != NX_OK ||
        nx_end_frame(app->ui) != NX_OK) return 0;
    return !output || save_frame(app->ui, output);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: nexaui_inspector_headless FONT_FILE OUTPUT.ppm\n");
        return 2;
    }
    int status = 1;
    FILE *input = fopen(argv[1], "rb");
    uint8_t *font_bytes = NULL;
    nx_font *font = NULL;
    nx_font_collection *fonts = NULL;
    inspector app = {0};
    if (!input || fseek(input, 0, SEEK_END) != 0) goto cleanup;
    long length = ftell(input);
    if (length <= 0 || (unsigned long)length > FONT_LIMIT ||
        fseek(input, 0, SEEK_SET) != 0) goto cleanup;
    font_bytes = malloc((size_t)length);
    if (!font_bytes ||
        fread(font_bytes, 1, (size_t)length, input) != (size_t)length ||
        nx_font_open(font_bytes, (size_t)length, NULL, &font) != NX_OK ||
        nx_font_collection_create(NULL, &fonts) != NX_OK ||
        nx_font_collection_add(fonts, font) != NX_OK ||
        nx_context_create(NULL, &app.ui) != NX_OK) goto cleanup;
    app.fonts = fonts;
    static const char *labels[3] = {"Scene", "Cube", "Light"};
    static const char *headings[2] = {"Scene Hierarchy", "Inspector"};
    for (size_t i = 0; i < 3; ++i)
        if (nx_text_layout_create(fonts, labels[i], strlen(labels[i]),
                                  NX_BIDI_AUTO, 16, 240, &app.labels[i]) != NX_OK)
            goto cleanup;
    for (size_t i = 0; i < 2; ++i)
        if (nx_text_layout_create(fonts, headings[i], strlen(headings[i]),
                                  NX_BIDI_AUTO, 20, 280, &app.headings[i]) != NX_OK)
            goto cleanup;
    nx_theme theme;
    if (nx_theme_preset(NX_THEME_DARK, &theme) != NX_OK ||
        nx_context_set_theme(app.ui, &theme) != NX_OK) goto cleanup;
    app.selected_id = 102;
    app.scene_expanded = 1;
    scene_snapshot snapshot = {1, 4};
    nx_property_action action;
    if (!draw_frame(&app, &snapshot, &action, NULL) ||
        !click(app.ui, 130, 180) ||
        !draw_frame(&app, &snapshot, &action, NULL) ||
        app.selected_id != 103 ||
        nx_focus_set(app.ui, 302) != NX_OK ||
        !key(app.ui, NX_KEY_RIGHT) ||
        !draw_frame(&app, &snapshot, &action, NULL) ||
        !action.changed || action.number_value != 5) goto cleanup;
    printf("proposed Intensity command: %.0f cd\n", action.number_value);
    snapshot.light_intensity = action.number_value;
    if (!draw_frame(&app, &snapshot, &action, argv[2])) goto cleanup;
    printf("wrote %s (selected Light, Intensity %.0f cd)\n",
           argv[2], snapshot.light_intensity);
    status = 0;
cleanup:
    if (status) fprintf(stderr, "inspector example failed\n");
    for (size_t i = 0; i < 3; ++i) nx_text_layout_destroy(app.labels[i]);
    for (size_t i = 0; i < 2; ++i) nx_text_layout_destroy(app.headings[i]);
    nx_context_destroy(app.ui);
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    free(font_bytes);
    if (input) fclose(input);
    return status;
}
