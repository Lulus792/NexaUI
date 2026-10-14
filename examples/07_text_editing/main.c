#include "nexaui/nexaui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 680u
#define HEIGHT 420u
#define FONT_LIMIT (64u * 1024u * 1024u)
#define GLYPH_CACHE (4u * 1024u * 1024u)

typedef struct editor_app {
    nx_context *ui;
    nx_font_collection *fonts;
    nx_text_field *project;
    nx_text_field *notes;
    nx_text_layout *title;
    nx_text_layout *project_label;
    nx_text_layout *notes_label;
    int project_valid, project_submitted;
    int notes_valid, notes_submitted;
} editor_app;

static nx_utf8_span span(const char *text) {
    return (nx_utf8_span){text, strlen(text)};
}

static int validate_project(nx_utf8_span text, void *user) {
    (void)user;
    return text.length >= 3;
}

static int text_equals(nx_text_field *field, const char *expected) {
    nx_utf8_span text;
    return nx_text_field_text(field, &text) == NX_OK &&
           text.length == strlen(expected) &&
           memcmp(text.data, expected, text.length) == 0;
}

static int semantic_text_equals(nx_context *ui, uint64_t id,
                                const char *expected, int invalid) {
    uint32_t index;
    nx_semantic_view view;
    return nx_semantics_find(ui, id, &index) == NX_OK &&
           nx_semantics_get(ui, index, &view) == NX_OK &&
           view.role == NX_ROLE_TEXT_FIELD &&
           !!(view.state & NX_SEM_INVALID) == !!invalid &&
           view.value.length == strlen(expected) &&
           memcmp(view.value.data, expected, view.value.length) == 0;
}

static int queue_key(nx_context *ui, nx_key key, uint32_t modifiers) {
    nx_event event = {0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = key;
    event.modifiers = modifiers;
    return nx_context_queue_event(ui, &event) == NX_OK;
}

static int queue_text(nx_context *ui, const char *ascii) {
    for (const unsigned char *p = (const unsigned char *)ascii; *p; ++p) {
        nx_event event = {0};
        event.type = NX_EVENT_TEXT;
        event.scalar = *p;
        if (nx_context_queue_event(ui, &event) != NX_OK) return 0;
    }
    return 1;
}

static nx_result draw_frame(editor_app *app) {
    nx_result result = nx_begin_frame(app->ui, WIDTH, HEIGHT, 1.0 / 60.0);
    if (result != NX_OK) return result;
    nx_color surface, text;
    result = nx_theme_color(app->ui, NX_COLOR_SURFACE, &surface);
    if (result != NX_OK) return result;
    result = nx_theme_color(app->ui, NX_COLOR_TEXT, &text);
    if (result != NX_OK) return result;
    result = nx_fill_rect(app->ui, (nx_rect){0, 0, WIDTH, HEIGHT}, surface);
    if (result != NX_OK) return result;
    result = nx_draw_text_layout(app->ui, app->title,
                                  (nx_point){26, 22}, text, GLYPH_CACHE);
    if (result != NX_OK) return result;
    nx_container_desc panel = {0};
    panel.id = 10;
    panel.bounds = (nx_rect){24, 66, 632, 324};
    panel.name = span("Editor form");
    result = nx_group(app->ui, &panel);
    if (result != NX_OK) return result;
    result = nx_draw_text_layout(app->ui, app->project_label,
        (nx_point){44, 84}, text, GLYPH_CACHE);
    if (result != NX_OK) return result;
    nx_text_field_desc project = {0};
    project.id = 101;
    project.parent_id = panel.id;
    project.bounds = (nx_rect){44, 113, 592, 42};
    project.name = span("Project name");
    project.fonts = app->fonts;
    project.pixels_per_em = 18;
    project.glyph_cache_bytes = GLYPH_CACHE;
    project.validate = validate_project;
    nx_interaction_state state;
    result = nx_text_field_widget(app->ui, &project, app->project, &state,
                                  &app->project_valid,
                                  &app->project_submitted);
    if (result != NX_OK) return result;
    result = nx_draw_text_layout(app->ui, app->notes_label,
        (nx_point){44, 178}, text, GLYPH_CACHE);
    if (result != NX_OK) return result;
    nx_text_field_desc notes = {0};
    notes.id = 102;
    notes.parent_id = panel.id;
    notes.bounds = (nx_rect){44, 207, 592, 148};
    notes.name = span("Notes");
    notes.fonts = app->fonts;
    notes.pixels_per_em = 18;
    notes.glyph_cache_bytes = GLYPH_CACHE;
    notes.multiline = 1;
    result = nx_text_field_widget(app->ui, &notes, app->notes, &state,
                                  &app->notes_valid, &app->notes_submitted);
    if (result != NX_OK) return result;
    return nx_end_frame(app->ui);
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

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: nexaui_text_editing_example FONT_FILE OUTPUT.ppm\n");
        return 2;
    }
    int status = 1;
    uint8_t *font_bytes = NULL;
    nx_font *font = NULL;
    editor_app app = {0};
    if (!load_font(argv[1], &font_bytes, &font, &app.fonts) ||
        nx_context_create(NULL, &app.ui) != NX_OK ||
        nx_text_field_create("A", 1, 4096, &app.project) != NX_OK ||
        nx_text_field_create("Plan\nReview", 11, 4096, &app.notes) != NX_OK ||
        nx_text_layout_create(app.fonts, "Text editing", 12,
            NX_BIDI_AUTO, 22, 300, &app.title) != NX_OK ||
        nx_text_layout_create(app.fonts, "Project name", 12,
            NX_BIDI_AUTO, 16, 200, &app.project_label) != NX_OK ||
        nx_text_layout_create(app.fonts, "Notes", 5,
            NX_BIDI_AUTO, 16, 200, &app.notes_label) != NX_OK)
        goto cleanup;
    nx_theme theme;
    if (nx_theme_preset(NX_THEME_LIGHT, &theme) != NX_OK ||
        nx_context_set_theme(app.ui, &theme) != NX_OK ||
        draw_frame(&app) != NX_OK || app.project_valid ||
        !semantic_text_equals(app.ui, 101, "A", 1) ||
        nx_focus_set(app.ui, 101) != NX_OK ||
        !queue_key(app.ui, NX_KEY_A, NX_MOD_CONTROL) ||
        !queue_text(app.ui, "NexaUI") ||
        !queue_key(app.ui, NX_KEY_ENTER, 0) ||
        draw_frame(&app) != NX_OK ||
        !app.project_valid || !app.project_submitted ||
        !text_equals(app.project, "NexaUI") ||
        !semantic_text_equals(app.ui, 101, "NexaUI", 0) ||
        nx_focus_set(app.ui, 102) != NX_OK ||
        !queue_key(app.ui, NX_KEY_END, NX_MOD_CONTROL) ||
        !queue_key(app.ui, NX_KEY_ENTER, 0) ||
        !queue_text(app.ui, "Ready") ||
        draw_frame(&app) != NX_OK ||
        !text_equals(app.notes, "Plan\nReview\nReady") ||
        !queue_text(app.ui, "!") ||
        draw_frame(&app) != NX_OK ||
        !text_equals(app.notes, "Plan\nReview\nReady!") ||
        !queue_key(app.ui, NX_KEY_Z, NX_MOD_CONTROL) ||
        draw_frame(&app) != NX_OK ||
        !text_equals(app.notes, "Plan\nReview\nReady") ||
        !queue_key(app.ui, NX_KEY_Y, NX_MOD_CONTROL) ||
        draw_frame(&app) != NX_OK ||
        !text_equals(app.notes, "Plan\nReview\nReady!") ||
        !semantic_text_equals(app.ui, 102,
                               "Plan\nReview\nReady!", 0) ||
        !save_frame(app.ui, argv[2])) goto cleanup;
    printf("Project: NexaUI; notes: Plan / Review / Ready! -> %s\n", argv[2]);
    status = 0;
cleanup:
    if (status) fprintf(stderr, "text editing example failed\n");
    nx_text_layout_destroy(app.notes_label);
    nx_text_layout_destroy(app.project_label);
    nx_text_layout_destroy(app.title);
    nx_text_field_destroy(app.notes);
    nx_text_field_destroy(app.project);
    nx_context_destroy(app.ui);
    nx_font_collection_destroy(app.fonts);
    nx_font_close(font);
    free(font_bytes);
    return status;
}
