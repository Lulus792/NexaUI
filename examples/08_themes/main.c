#include "nexaui/nexaui.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 640u
#define HEIGHT 430u
#define FONT_LIMIT (64u * 1024u * 1024u)
#define GLYPH_CACHE (4u * 1024u * 1024u)

enum label_index {
    TITLE, PRIMARY, CHECKBOX, DISABLED, ACCENT, SUCCESS, ERROR,
    SCOPED, LABEL_COUNT
};

typedef struct theme_app {
    nx_context *ui;
    nx_font_collection *fonts;
    nx_text_layout *labels[LABEL_COUNT];
    nx_theme_kind selected;
    int changed;
    int checked;
} theme_app;

static nx_utf8_span span(const char *text) {
    return (nx_utf8_span){text, strlen(text)};
}

static int same_color(nx_color a, nx_color b) {
    return fabsf(a.r - b.r) < 0.0001f &&
           fabsf(a.g - b.g) < 0.0001f &&
           fabsf(a.b - b.b) < 0.0001f &&
           fabsf(a.a - b.a) < 0.0001f;
}

static nx_result draw_label(theme_app *app, enum label_index index,
                            float x, float y, nx_color color) {
    return nx_draw_text_layout(app->ui, app->labels[index],
                                (nx_point){x, y}, color, GLYPH_CACHE);
}

static nx_result draw_frame(theme_app *app) {
    nx_result result = nx_begin_frame(app->ui, WIDTH, HEIGHT, 1.0 / 60.0);
    if (result != NX_OK) return result;
    nx_color background, text, accent, success, error;
    result = nx_theme_color(app->ui, NX_COLOR_BACKGROUND, &background);
    if (result != NX_OK) return result;
    result = nx_theme_color(app->ui, NX_COLOR_TEXT, &text);
    if (result != NX_OK) return result;
    result = nx_theme_color(app->ui, NX_COLOR_ACCENT, &accent);
    if (result != NX_OK) return result;
    result = nx_theme_color(app->ui, NX_COLOR_SUCCESS, &success);
    if (result != NX_OK) return result;
    result = nx_theme_color(app->ui, NX_COLOR_ERROR, &error);
    if (result != NX_OK) return result;
    result = nx_fill_rect(app->ui, (nx_rect){0, 0, WIDTH, HEIGHT}, background);
    if (result != NX_OK) return result;
    result = draw_label(app, TITLE, 24, 26, text);
    if (result != NX_OK) return result;
    nx_container_desc panel = {0};
    panel.id = 10;
    panel.bounds = (nx_rect){24, 80, 592, 320};
    panel.name = span("Theme showcase");
    result = nx_group(app->ui, &panel);
    if (result != NX_OK) return result;
    nx_theme_selector_desc selector = {0};
    selector.id = 20;
    selector.parent_id = panel.id;
    selector.name = span("Theme");
    selector.bounds = (nx_rect){40, 103, 560, 68};
    selector.option_ids[0] = 21;
    selector.option_ids[1] = 22;
    selector.option_ids[2] = 23;
    selector.option_names[0] = span("Dark");
    selector.option_names[1] = span("Light");
    selector.option_names[2] = span("High contrast");
    selector.fonts = app->fonts;
    selector.pixels_per_em = 16;
    selector.glyph_cache_bytes = GLYPH_CACHE;
    result = nx_theme_selector(app->ui, &selector,
                                &app->selected, &app->changed);
    if (result != NX_OK) return result;
    nx_control_desc button = {0};
    button.id = 30;
    button.parent_id = panel.id;
    button.bounds = (nx_rect){44, 202, 165, 40};
    button.name = span("Run action");
    button.label_layout = app->labels[PRIMARY];
    button.label_origin = (nx_point){58, 211};
    button.glyph_cache_bytes = GLYPH_CACHE;
    nx_interaction_state state;
    result = nx_button(app->ui, &button, &state);
    if (result != NX_OK) return result;
    nx_control_desc checkbox = {0};
    checkbox.id = 31;
    checkbox.parent_id = panel.id;
    checkbox.bounds = (nx_rect){245, 204, 166, 36};
    checkbox.name = span("Notifications");
    checkbox.label_layout = app->labels[CHECKBOX];
    checkbox.label_origin = (nx_point){288, 210};
    checkbox.glyph_cache_bytes = GLYPH_CACHE;
    result = nx_checkbox(app->ui, &checkbox, &app->checked, &state);
    if (result != NX_OK) return result;
    nx_control_desc disabled = {0};
    disabled.id = 32;
    disabled.parent_id = panel.id;
    disabled.bounds = (nx_rect){446, 202, 140, 40};
    disabled.name = span("Disabled action");
    disabled.label_layout = app->labels[DISABLED];
    disabled.label_origin = (nx_point){461, 211};
    disabled.glyph_cache_bytes = GLYPH_CACHE;
    disabled.disabled = 1;
    result = nx_button(app->ui, &disabled, &state);
    if (result != NX_OK) return result;

    const nx_color swatches[3] = {accent, success, error};
    const enum label_index names[3] = {ACCENT, SUCCESS, ERROR};
    for (size_t i = 0; i < 3; ++i) {
        float x = 44 + 184 * (float)i;
        result = draw_label(app, names[i], x, 265, text);
        if (result != NX_OK) return result;
        result = nx_fill_round_rect(app->ui,
            (nx_rect){x, 291, 152, 20}, 4, swatches[i]);
        if (result != NX_OK) return result;
    }
    result = draw_label(app, SCOPED, 44, 330, text);
    if (result != NX_OK) return result;
    nx_theme override = {0};
    nx_color purple = {0.55f, 0.26f, 0.79f, 1};
    result = nx_theme_set_color(&override, NX_COLOR_ACCENT, purple);
    if (result != NX_OK) return result;
    result = nx_push_theme(app->ui, &override);
    if (result != NX_OK) return result;
    nx_color scoped;
    result = nx_theme_color(app->ui, NX_COLOR_ACCENT, &scoped);
    if (result != NX_OK) return result;
    result = nx_fill_round_rect(app->ui,
        (nx_rect){218, 337, 368, 18}, 4, scoped);
    if (result != NX_OK) return result;
    result = nx_pop_theme(app->ui);
    if (result != NX_OK) return result;
    nx_color restored;
    result = nx_theme_color(app->ui, NX_COLOR_ACCENT, &restored);
    if (result != NX_OK || !same_color(scoped, purple) ||
        !same_color(restored, accent)) return NX_ERROR_STATE;
    return nx_end_frame(app->ui);
}

static int save_frame(nx_context *ui, const char *prefix,
                      const char *suffix) {
    char path[1024];
    int length = snprintf(path, sizeof(path), "%s_%s.ppm", prefix, suffix);
    if (length < 0 || (size_t)length >= sizeof(path)) return 0;
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

static int checked_option(nx_context *ui, uint64_t id) {
    uint32_t index;
    nx_semantic_view view;
    return nx_semantics_find(ui, id, &index) == NX_OK &&
           nx_semantics_get(ui, index, &view) == NX_OK &&
           view.role == NX_ROLE_RADIO && !!(view.state & NX_SEM_CHECKED);
}

static int apply_theme(theme_app *app) {
    nx_theme preset;
    return nx_theme_preset(app->selected, &preset) == NX_OK &&
           nx_context_set_theme(app->ui, &preset) == NX_OK;
}

static int queue_key(nx_context *ui, nx_key key) {
    nx_event event = {0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = key;
    return nx_context_queue_event(ui, &event) == NX_OK;
}

static int queue_click(nx_context *ui, float x, float y) {
    nx_event event = {0};
    event.type = NX_EVENT_POINTER_DOWN;
    event.button = NX_BUTTON_LEFT;
    event.x = x;
    event.y = y;
    if (nx_context_queue_event(ui, &event) != NX_OK) return 0;
    event.type = NX_EVENT_POINTER_UP;
    return nx_context_queue_event(ui, &event) == NX_OK;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: nexaui_themes_example FONT_FILE OUTPUT_PREFIX\n");
        return 2;
    }
    int status = 1;
    uint8_t *font_bytes = NULL;
    nx_font *font = NULL;
    theme_app app = {0};
    app.selected = NX_THEME_DARK;
    app.checked = 1;
    if (!load_font(argv[1], &font_bytes, &font, &app.fonts) ||
        nx_context_create(NULL, &app.ui) != NX_OK) goto cleanup;
    static const char *names[LABEL_COUNT] = {
        "Theme switcher", "Run action", "Notifications", "Disabled",
        "Accent", "Success", "Error", "Scoped accent"
    };
    for (size_t i = 0; i < LABEL_COUNT; ++i)
        if (nx_text_layout_create(app.fonts, names[i], strlen(names[i]),
                NX_BIDI_AUTO, i == TITLE ? 22 : 16, 220,
                &app.labels[i]) != NX_OK) goto cleanup;
    if (!apply_theme(&app) || draw_frame(&app) != NX_OK ||
        app.changed || !checked_option(app.ui, 21) ||
        !save_frame(app.ui, argv[2], "dark") ||
        nx_focus_set(app.ui, 21) != NX_OK ||
        !queue_key(app.ui, NX_KEY_RIGHT) ||
        draw_frame(&app) != NX_OK ||
        !app.changed || app.selected != NX_THEME_LIGHT ||
        !checked_option(app.ui, 22) ||
        !apply_theme(&app) || draw_frame(&app) != NX_OK ||
        app.changed || !save_frame(app.ui, argv[2], "light") ||
        !queue_key(app.ui, NX_KEY_END) ||
        draw_frame(&app) != NX_OK ||
        !app.changed || app.selected != NX_THEME_HIGH_CONTRAST ||
        !checked_option(app.ui, 23) ||
        !apply_theme(&app) || draw_frame(&app) != NX_OK ||
        app.changed || !save_frame(app.ui, argv[2], "high_contrast") ||
        !queue_click(app.ui, 60, 120) ||
        draw_frame(&app) != NX_OK ||
        !app.changed || app.selected != NX_THEME_DARK ||
        !checked_option(app.ui, 21) || !app.checked)
        goto cleanup;
    printf("Wrote %s_{dark,light,high_contrast}.ppm\n", argv[2]);
    status = 0;
cleanup:
    if (status) fprintf(stderr, "themes example failed\n");
    for (size_t i = 0; i < LABEL_COUNT; ++i)
        nx_text_layout_destroy(app.labels[i]);
    nx_context_destroy(app.ui);
    nx_font_collection_destroy(app.fonts);
    nx_font_close(font);
    free(font_bytes);
    return status;
}
