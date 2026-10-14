#include "nexaui/nexaui.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 800u
#define HEIGHT 500u
#define FONT_LIMIT (64u * 1024u * 1024u)
#define GLYPH_CACHE (4u * 1024u * 1024u)

enum label_index { TITLE, MOVE, SCENE, ASSETS, INSPECTOR, LABEL_COUNT };

typedef struct editor_app {
    nx_context *ui;
    nx_layout *layout;
    uint32_t root, left, right;
    nx_font_collection *fonts;
    nx_text_layout *labels[LABEL_COUNT];
    nx_dock_node nodes[8];
    nx_dock_tab tabs[8];
    nx_dock_model dock;
    float ratio;
    int moved;
    nx_split_info split;
} editor_app;

static nx_utf8_span span(const char *text) {
    return (nx_utf8_span){text, strlen(text)};
}

static const char *panel_name(uint64_t id) {
    if (id == 100) return "Scene";
    if (id == 101) return "Assets";
    return "Inspector";
}

static enum label_index panel_label(uint64_t id) {
    if (id == 100) return SCENE;
    if (id == 101) return ASSETS;
    return INSPECTOR;
}

static nx_dock_node *stack_node(editor_app *app, uint64_t id) {
    for (size_t i = 0; i < app->dock.node_count; ++i)
        if (app->dock.nodes[i].id == id) return &app->dock.nodes[i];
    return NULL;
}

static nx_result draw_stack(editor_app *app, uint64_t stack_id,
                             nx_rect bounds) {
    nx_dock_node *stack = stack_node(app, stack_id);
    if (!stack || stack->kind != NX_DOCK_STACK) return NX_ERROR_STATE;
    nx_container_desc group = {0};
    group.id = stack_id;
    group.parent_id = 1;
    group.bounds = bounds;
    group.name = span(stack_id == 10 ? "Left dock" : "Right dock");
    nx_result result = nx_group(app->ui, &group);
    if (result != NX_OK) { fprintf(stderr, "group %llu: %d\n", (unsigned long long)stack_id, result); return result; }
    uint64_t selected = stack->selected_panel_id;
    size_t position = 0;
    for (size_t i = 0; i < app->dock.tab_count; ++i) {
        const nx_dock_tab *tab = &app->dock.tabs[i];
        if (tab->stack_id != stack_id) continue;
        nx_control_desc desc = {0};
        desc.id = tab->panel_id;
        desc.parent_id = stack_id;
        desc.bounds = (nx_rect){bounds.x + 10 + 112 * (float)position,
                                bounds.y + 10, 104, 30};
        desc.name = span(panel_name(tab->panel_id));
        desc.label_layout = app->labels[panel_label(tab->panel_id)];
        desc.label_origin = (nx_point){desc.bounds.x + 10,
                                       desc.bounds.y + 5};
        desc.glyph_cache_bytes = GLYPH_CACHE;
        nx_interaction_state state;
        result = nx_tab(app->ui, &desc, &selected, &state);
        if (result != NX_OK) { fprintf(stderr, "tab %llu: %d\n", (unsigned long long)tab->panel_id, result); return result; }
        ++position;
    }
    if (selected != stack->selected_panel_id) {
        result = nx_dock_select(&app->dock, selected);
        if (result != NX_OK) return result;
    }
    nx_container_desc panel = {0};
    panel.id = 1000 + selected;
    panel.parent_id = stack_id;
    panel.bounds = (nx_rect){bounds.x + 10, bounds.y + 50,
                             bounds.width - 20, bounds.height - 60};
    panel.name = span(panel_name(selected));
    panel.label_layout = app->labels[panel_label(selected)];
    panel.label_origin = (nx_point){panel.bounds.x + 18,
                                    panel.bounds.y + 15};
    panel.glyph_cache_bytes = GLYPH_CACHE;
    result = nx_tab_panel(app->ui, &panel);
    if (result != NX_OK) { fprintf(stderr, "panel %llu: %d (%.1f %.1f)\n", (unsigned long long)selected, result, panel.bounds.width, panel.bounds.height); return result; }
    nx_color accent, surface;
    result = nx_theme_color(app->ui, selected == 100 ? NX_COLOR_ACCENT :
                           selected == 101 ? NX_COLOR_SUCCESS :
                           NX_COLOR_WARNING, &accent);
    if (result != NX_OK) return result;
    result = nx_theme_color(app->ui, NX_COLOR_SURFACE_RAISED, &surface);
    if (result != NX_OK) return result;
    result = nx_fill_round_rect(app->ui,
        (nx_rect){panel.bounds.x + 18, panel.bounds.y + 50,
                  panel.bounds.width - 36, 8}, 4, accent);
    if (result != NX_OK) return result;
    for (size_t line = 0; line < 4; ++line) {
        result = nx_fill_round_rect(app->ui,
            (nx_rect){panel.bounds.x + 18,
                      panel.bounds.y + 80 + 30 * (float)line,
                      panel.bounds.width - 60 - 26 * (float)line, 14},
            4, line == 1 ? accent : surface);
        if (result != NX_OK) return result;
    }
    return NX_OK;
}

static nx_result draw_frame(editor_app *app) {
    nx_result result = nx_layout_set_split(app->layout, app->root, app->ratio);
    if (result != NX_OK) return result;
    result = nx_layout_arrange(app->layout, app->root,
                               (nx_rect){24, 108, 752, 360});
    if (result != NX_OK) return result;
    nx_rect left, right;
    result = nx_layout_get_rect(app->layout, app->left, &left);
    if (result != NX_OK) return result;
    result = nx_layout_get_rect(app->layout, app->right, &right);
    if (result != NX_OK) return result;
    result = nx_layout_get_split_info(app->layout, app->root, &app->split);
    if (result != NX_OK) return result;
    result = nx_begin_frame(app->ui, WIDTH, HEIGHT, 1.0 / 60.0);
    if (result != NX_OK) return result;
    nx_color background, text;
    result = nx_theme_color(app->ui, NX_COLOR_BACKGROUND, &background);
    if (result != NX_OK) return result;
    result = nx_theme_color(app->ui, NX_COLOR_TEXT, &text);
    if (result != NX_OK) return result;
    result = nx_fill_rect(app->ui, (nx_rect){0, 0, WIDTH, HEIGHT}, background);
    if (result != NX_OK) return result;
    nx_semantic_desc root = {0};
    root.id = 1;
    root.role = NX_ROLE_WINDOW;
    root.bounds = (nx_rect){0, 0, WIDTH, HEIGHT};
    root.name = span("Docking editor");
    result = nx_semantics_add(app->ui, &root, NULL);
    if (result != NX_OK) return result;
    result = nx_draw_text_layout(app->ui, app->labels[TITLE],
                                 (nx_point){24, 20}, text, GLYPH_CACHE);
    if (result != NX_OK) return result;
    nx_control_desc move = {0};
    move.id = 300;
    move.parent_id = 1;
    move.bounds = (nx_rect){24, 58, 170, 34};
    move.name = span("Move Assets right");
    move.label_layout = app->labels[MOVE];
    move.label_origin = (nx_point){35, 65};
    move.glyph_cache_bytes = GLYPH_CACHE;
    move.disabled = app->moved;
    nx_interaction_state state;
    result = nx_button(app->ui, &move, &state);
    if (result != NX_OK) return result;
    if (state.clicked) {
        result = nx_dock_move(&app->dock, 101, 12);
        if (result != NX_OK) return result;
        result = nx_dock_select(&app->dock, 101);
        if (result != NX_OK) return result;
        app->moved = 1;
    }
    result = draw_stack(app, 10, left);
    if (result != NX_OK) return result;
    result = draw_stack(app, 12, right);
    if (result != NX_OK) return result;
    nx_splitter_desc splitter = {0};
    splitter.id = 500;
    splitter.parent_id = 1;
    splitter.name = span("Resize docks");
    splitter.axis = NX_SPLIT_X;
    splitter.split = app->split;
    splitter.minimum_ratio = 0.25f;
    splitter.maximum_ratio = 0.75f;
    result = nx_splitter(app->ui, &splitter, &app->ratio, &state);
    if (result != NX_OK) { fprintf(stderr, "splitter: %d\n", result); return result; }
    result = nx_end_frame(app->ui);
    if (result != NX_OK) { fprintf(stderr, "end frame: %d\n", result); return result; }
    result = nx_dock_set_ratio(&app->dock, 11, app->ratio);
    if (result != NX_OK) fprintf(stderr, "dock ratio: %d\n", result);
    return result;
}

static int queue_pointer(nx_context *ui, nx_event_type type,
                         float x, float y) {
    nx_event event = {0};
    event.type = type;
    event.button = NX_BUTTON_LEFT;
    event.x = x;
    event.y = y;
    return nx_context_queue_event(ui, &event) == NX_OK;
}

static int save_frame(nx_context *ui, const char *path) {
    const nx_command *commands = NULL;
    size_t count = 0;
    if (nx_frame_commands(ui, &commands, &count) != NX_OK) return 0;
    uint8_t *pixels = malloc((size_t)WIDTH * HEIGHT * 4u);
    if (!pixels) return 0;
    int okay = nx_headless_render(commands, count, pixels, WIDTH, HEIGHT,
                                  WIDTH * 4u, (nx_color){1, 1, 1, 1}) == NX_OK;
    FILE *file = okay ? fopen(path, "wb") : NULL;
    if (!file) okay = 0;
    if (okay && fprintf(file, "P6\n%u %u\n255\n", WIDTH, HEIGHT) < 0)
        okay = 0;
    for (size_t i = 0; okay && i < (size_t)WIDTH * HEIGHT; ++i)
        if (fwrite(pixels + 4u * i, 1, 3, file) != 3) okay = 0;
    if (file && fclose(file) != 0) okay = 0;
    free(pixels);
    return okay;
}

static int load_font(const char *path, uint8_t **out_bytes,
                     nx_font **out_font, nx_font_collection **out_fonts) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    int okay = 0;
    if (fseek(file, 0, SEEK_END) != 0) goto done;
    long length = ftell(file);
    if (length <= 0 || (unsigned long)length > FONT_LIMIT ||
        fseek(file, 0, SEEK_SET) != 0) goto done;
    *out_bytes = malloc((size_t)length);
    if (!*out_bytes || fread(*out_bytes, 1, (size_t)length, file) !=
                       (size_t)length ||
        nx_font_open(*out_bytes, (size_t)length, NULL, out_font) != NX_OK ||
        nx_font_collection_create(NULL, out_fonts) != NX_OK ||
        nx_font_collection_add(*out_fonts, *out_font) != NX_OK) goto done;
    okay = 1;
done:
    fclose(file);
    return okay;
}

static int save_snapshot(const nx_dock_model *model, const char *path) {
    uint8_t bytes[512], roundtrip[512];
    size_t length = 0, restored_length = 0;
    nx_dock_node nodes[8];
    nx_dock_tab tabs[8];
    nx_dock_model restored = {0};
    restored.nodes = nodes;
    restored.node_capacity = 8;
    restored.tabs = tabs;
    restored.tab_capacity = 8;
    if (nx_dock_encode(model, bytes, sizeof(bytes), &length) != NX_OK ||
        nx_dock_decode(&restored, bytes, length) != NX_OK ||
        nx_dock_validate(&restored) != NX_OK ||
        nx_dock_encode(&restored, roundtrip, sizeof(roundtrip),
                       &restored_length) != NX_OK ||
        length != restored_length || memcmp(bytes, roundtrip, length) != 0)
        return 0;
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    int okay = fwrite(bytes, 1, length, file) == length;
    if (fclose(file) != 0) okay = 0;
    return okay;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: nexaui_docking_editor FONT_FILE OUTPUT.ppm SNAPSHOT.nxdock\n");
        return 2;
    }
    int status = 1;
    uint8_t *font_bytes = NULL;
    nx_font *font = NULL;
    editor_app app = {0};
    if (!load_font(argv[1], &font_bytes, &font, &app.fonts) ||
        nx_context_create(NULL, &app.ui) != NX_OK ||
        nx_layout_create(NULL, 3, &app.layout) != NX_OK)
        goto cleanup;
    static const char *names[LABEL_COUNT] = {
        "Docking editor", "Move Assets right", "Scene", "Assets", "Inspector"
    };
    for (size_t i = 0; i < LABEL_COUNT; ++i)
        if (nx_text_layout_create(app.fonts, names[i], strlen(names[i]),
                NX_BIDI_AUTO, i == TITLE ? 22 : 16, 220,
                &app.labels[i]) != NX_OK) goto cleanup;
    nx_theme theme;
    if (nx_theme_preset(NX_THEME_DARK, &theme) != NX_OK ||
        nx_context_set_theme(app.ui, &theme) != NX_OK) goto cleanup;
    nx_layout_desc split = nx_layout_default_desc(NX_LAYOUT_SPLIT);
    split.split_axis = NX_SPLIT_X;
    split.split_ratio = 0.42f;
    split.gap = 8;
    nx_layout_desc leaf = nx_layout_default_desc(NX_LAYOUT_LEAF);
    leaf.align_y = NX_ALIGN_STRETCH;
    if (nx_layout_add(app.layout, NX_LAYOUT_NO_PARENT, &split, &app.root) != NX_OK ||
        nx_layout_add(app.layout, app.root, &leaf, &app.left) != NX_OK ||
        nx_layout_add(app.layout, app.root, &leaf, &app.right) != NX_OK)
        goto cleanup;
    nx_size desired;
    if (nx_layout_measure(app.layout, app.root, 752, 360, &desired) != NX_OK ||
        nx_dock_init(&app.dock, app.nodes, 8, app.tabs, 8, 10, 100) != NX_OK ||
        nx_dock_add_tab(&app.dock, 10, 101) != NX_OK ||
        nx_dock_split(&app.dock, 10, 11, 12, 200,
                      NX_SPLIT_X, 0.42f, 1) != NX_OK ||
        nx_dock_validate(&app.dock) != NX_OK) {
        fprintf(stderr, "model setup failed\n");
        goto cleanup;
    }
    app.ratio = 0.42f;
    nx_result result = draw_frame(&app);
    if (result != NX_OK) {
        fprintf(stderr, "initial frame: %d\n", result);
        goto cleanup;
    }
    if (!queue_pointer(app.ui, NX_EVENT_POINTER_DOWN, 100, 75) ||
        !queue_pointer(app.ui, NX_EVENT_POINTER_UP, 100, 75)) goto cleanup;
    result = draw_frame(&app);
    if (result != NX_OK || !app.moved ||
        nx_dock_validate(&app.dock) != NX_OK) {
        fprintf(stderr, "move frame: %d, moved=%d\n", result, app.moved);
        goto cleanup;
    }
    float divider_x = app.split.divider.x + app.split.divider.width * 0.5f;
    float divider_y = app.split.divider.y + app.split.divider.height * 0.5f;
    if (!queue_pointer(app.ui, NX_EVENT_POINTER_DOWN, divider_x, divider_y) ||
        !queue_pointer(app.ui, NX_EVENT_POINTER_MOVE, divider_x + 70,
                       divider_y)) goto cleanup;
    result = draw_frame(&app);
    if (result != NX_OK || app.ratio <= 0.42f) {
        fprintf(stderr, "drag frame: %d, ratio=%f\n", result, app.ratio);
        goto cleanup;
    }
    if (!queue_pointer(app.ui, NX_EVENT_POINTER_UP, divider_x + 70,
                       divider_y)) goto cleanup;
    result = draw_frame(&app);
    if (result != NX_OK || nx_dock_validate(&app.dock) != NX_OK) {
        fprintf(stderr, "final frame: %d\n", result);
        goto cleanup;
    }
    nx_dock_node *left = stack_node(&app, 10);
    nx_dock_node *right = stack_node(&app, 12);
    if (!left || !right || left->selected_panel_id != 100 ||
        right->selected_panel_id != 101 || app.dock.tab_count != 3 ||
        !save_frame(app.ui, argv[2]) ||
        !save_snapshot(&app.dock, argv[3]))
        goto cleanup;
    printf("Moved Assets right; split %.2f; wrote %s and %s\n",
           app.ratio, argv[2], argv[3]);
    status = 0;
cleanup:
    if (status) fprintf(stderr, "docking editor example failed\n");
    for (size_t i = 0; i < LABEL_COUNT; ++i)
        nx_text_layout_destroy(app.labels[i]);
    nx_layout_destroy(app.layout);
    nx_context_destroy(app.ui);
    nx_font_collection_destroy(app.fonts);
    nx_font_close(font);
    free(font_bytes);
    return status;
}
