#include "nexaui/nexaui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 640u
#define HEIGHT 440u
#define FONT_LIMIT (64u * 1024u * 1024u)
#define GLYPH_CACHE (4u * 1024u * 1024u)

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 0; \
} } while (0)

typedef struct demo {
    nx_context *ui;
    uint8_t *font_bytes;
    nx_font *font;
    nx_font_collection *fonts;
    nx_text_field *project, *owner;
    nx_text_layout *title, *project_label, *owner_label;
    nx_text_layout *confirm_label, *submit_label, *status_labels[3];
    int project_valid, owner_valid, confirmed, submitted;
    unsigned frames;
} demo;

static nx_utf8_span span(const char *text) {
    return (nx_utf8_span){text, strlen(text)};
}

static void close_demo(demo *app) {
    for (size_t i = 0; i < 3; ++i)
        nx_text_layout_destroy(app->status_labels[i]);
    nx_text_layout_destroy(app->submit_label);
    nx_text_layout_destroy(app->confirm_label);
    nx_text_layout_destroy(app->owner_label);
    nx_text_layout_destroy(app->project_label);
    nx_text_layout_destroy(app->title);
    nx_text_field_destroy(app->owner);
    nx_text_field_destroy(app->project);
    nx_context_destroy(app->ui);
    nx_font_collection_destroy(app->fonts);
    nx_font_close(app->font);
    free(app->font_bytes);
}

static int make_layout(demo *app, const char *text, int ppem,
                       nx_text_layout **out) {
    return nx_text_layout_create(app->fonts, text, strlen(text), NX_BIDI_LTR,
                                 ppem, 500, out) == NX_OK;
}

static int load_font(demo *app, const char *path) {
    FILE *input = fopen(path, "rb");
    CHECK(input != NULL);
    int okay = fseek(input, 0, SEEK_END) == 0;
    long length = okay ? ftell(input) : -1;
    okay = okay && length > 0 && (unsigned long)length <= FONT_LIMIT &&
           fseek(input, 0, SEEK_SET) == 0;
    if (okay) app->font_bytes = (uint8_t *)malloc((size_t)length);
    okay = okay && app->font_bytes &&
           fread(app->font_bytes, 1, (size_t)length, input) == (size_t)length;
    if (fclose(input) != 0) okay = 0;
    CHECK(okay);
    CHECK(nx_font_open(app->font_bytes, (size_t)length, NULL, &app->font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &app->fonts) == NX_OK);
    CHECK(nx_font_collection_add(app->fonts, app->font) == NX_OK);
    CHECK(make_layout(app, "Create a project", 28, &app->title));
    CHECK(make_layout(app, "Project name (3+ characters)", 16,
                      &app->project_label));
    CHECK(make_layout(app, "Owner (2+ characters)", 16, &app->owner_label));
    CHECK(make_layout(app, "Confirm details", 17, &app->confirm_label));
    CHECK(make_layout(app, "Create", 18, &app->submit_label));
    CHECK(make_layout(app, "Complete both fields and confirm", 15,
                      &app->status_labels[0]));
    CHECK(make_layout(app, "Ready to create", 15, &app->status_labels[1]));
    CHECK(make_layout(app, "Project created", 15, &app->status_labels[2]));
    return 1;
}

static int valid_project(nx_utf8_span text, void *user) {
    (void)user;
    return text.length >= 3;
}

static int valid_owner(nx_utf8_span text, void *user) {
    (void)user;
    return text.length >= 2;
}

static int text_equals(nx_text_field *field, const char *expected) {
    nx_utf8_span value;
    return nx_text_field_text(field, &value) == NX_OK &&
           value.length == strlen(expected) &&
           memcmp(value.data, expected, value.length) == 0;
}

static int semantic_field(demo *app, uint64_t id, const char *expected,
                          int invalid) {
    uint32_t index;
    nx_semantic_view view;
    return nx_semantics_find(app->ui, id, &index) == NX_OK &&
           nx_semantics_get(app->ui, index, &view) == NX_OK &&
           view.role == NX_ROLE_TEXT_FIELD && view.parent_id == 2 &&
           !!(view.state & NX_SEM_INVALID) == !!invalid &&
           view.value.length == strlen(expected) &&
           (!view.value.length || memcmp(view.value.data, expected, view.value.length) == 0);
}

static int inspect(demo *app) {
    size_t count;
    uint32_t index;
    nx_semantic_view view;
    CHECK(nx_semantics_count(app->ui, &count) == NX_OK && count == 7);
    CHECK(nx_semantics_find(app->ui, 5, &index) == NX_OK &&
          nx_semantics_get(app->ui, index, &view) == NX_OK &&
          view.role == NX_ROLE_CHECKBOX && view.parent_id == 2 &&
          !!(view.state & NX_SEM_CHECKED) == !!app->confirmed);
    int ready = app->project_valid && app->owner_valid && app->confirmed;
    CHECK(nx_semantics_find(app->ui, 6, &index) == NX_OK &&
          nx_semantics_get(app->ui, index, &view) == NX_OK &&
          view.role == NX_ROLE_BUTTON && view.parent_id == 2 &&
          !!(view.state & NX_SEM_DISABLED) == !ready);
    CHECK(nx_semantics_find(app->ui, 7, &index) == NX_OK &&
          nx_semantics_get(app->ui, index, &view) == NX_OK &&
          view.role == NX_ROLE_STATUS && view.parent_id == 2);
    const char *expected = app->submitted ? "Project created" :
                           ready ? "Ready to create" :
                                   "Complete both fields and confirm";
    CHECK(view.value.length == strlen(expected) &&
          memcmp(view.value.data, expected, view.value.length) == 0);
    return 1;
}

static int draw_frame(demo *app) {
    CHECK(nx_begin_frame(app->ui, WIDTH, HEIGHT, 1.0 / 60.0) == NX_OK);
    nx_color background, text, muted;
    CHECK(nx_theme_color(app->ui, NX_COLOR_BACKGROUND, &background) == NX_OK);
    CHECK(nx_theme_color(app->ui, NX_COLOR_TEXT, &text) == NX_OK);
    CHECK(nx_theme_color(app->ui, NX_COLOR_TEXT_MUTED, &muted) == NX_OK);
    nx_semantic_desc window = {0};
    window.id = 1;
    window.role = NX_ROLE_WINDOW;
    window.bounds = (nx_rect){0, 0, WIDTH, HEIGHT};
    window.name = span("Project form example");
    CHECK(nx_semantics_add(app->ui, &window, NULL) == NX_OK);
    nx_container_desc panel = {0};
    panel.id = 2;
    panel.parent_id = 1;
    panel.bounds = (nx_rect){24, 24, 592, 392};
    panel.name = span("Create a project");
    panel.label_layout = app->title;
    panel.label_origin = (nx_point){44, 44};
    panel.glyph_cache_bytes = GLYPH_CACHE;
    CHECK(nx_panel(app->ui, &panel) == NX_OK);
    CHECK(nx_draw_text_layout(app->ui, app->project_label,
                              (nx_point){44, 96}, text, GLYPH_CACHE) == NX_OK);
    nx_text_field_desc project = {0};
    project.id = 3;
    project.parent_id = 2;
    project.bounds = (nx_rect){44, 125, 552, 42};
    project.name = span("Project name, at least three characters");
    project.fonts = app->fonts;
    project.pixels_per_em = 18;
    project.glyph_cache_bytes = GLYPH_CACHE;
    project.validate = valid_project;
    nx_interaction_state field_state;
    int field_submitted;
    CHECK(nx_text_field_widget(app->ui, &project, app->project, &field_state,
                               &app->project_valid, &field_submitted) == NX_OK);

    CHECK(nx_draw_text_layout(app->ui, app->owner_label,
                              (nx_point){44, 187}, text, GLYPH_CACHE) == NX_OK);
    nx_text_field_desc owner = {0};
    owner.id = 4;
    owner.parent_id = 2;
    owner.bounds = (nx_rect){44, 216, 552, 42};
    owner.name = span("Owner, at least two characters");
    owner.fonts = app->fonts;
    owner.pixels_per_em = 18;
    owner.glyph_cache_bytes = GLYPH_CACHE;
    owner.validate = valid_owner;
    CHECK(nx_text_field_widget(app->ui, &owner, app->owner, &field_state,
                               &app->owner_valid, &field_submitted) == NX_OK);

    nx_control_desc confirm = {0};
    confirm.id = 5;
    confirm.parent_id = 2;
    confirm.bounds = (nx_rect){44, 283, 250, 28};
    confirm.name = span("Confirm details");
    confirm.label_layout = app->confirm_label;
    confirm.label_origin = (nx_point){82, 283};
    confirm.glyph_cache_bytes = GLYPH_CACHE;
    nx_interaction_state confirm_state;
    CHECK(nx_checkbox(app->ui, &confirm, &app->confirmed,
                      &confirm_state) == NX_OK);

    nx_control_desc submit = {0};
    submit.id = 6;
    submit.parent_id = 2;
    submit.bounds = (nx_rect){44, 344, 160, 46};
    submit.name = span("Create project");
    submit.label_layout = app->submit_label;
    submit.label_origin = (nx_point){96, 353};
    submit.glyph_cache_bytes = GLYPH_CACHE;
    int ready = app->project_valid && app->owner_valid && app->confirmed;
    submit.disabled = !ready;
    nx_interaction_state submit_state;
    CHECK(nx_button(app->ui, &submit, &submit_state) == NX_OK);
    if (submit_state.clicked && ready) app->submitted = 1;

    const char *messages[] = {
        "Complete both fields and confirm", "Ready to create", "Project created"
    };
    int status = app->submitted ? 2 : ready ? 1 : 0;
    nx_semantic_desc status_node = {0};
    status_node.id = 7;
    status_node.parent_id = 2;
    status_node.role = NX_ROLE_STATUS;
    status_node.bounds = (nx_rect){230, 344, 350, 46};
    status_node.name = span("Form status");
    status_node.value = span(messages[status]);
    CHECK(nx_semantics_add(app->ui, &status_node, NULL) == NX_OK);
    CHECK(nx_draw_text_layout(app->ui, app->status_labels[status],
                              (nx_point){230, 355}, muted, GLYPH_CACHE) == NX_OK);
    CHECK(nx_end_frame(app->ui) == NX_OK);
    CHECK(inspect(app));
    ++app->frames;
    return 1;
}

static int queue_key(nx_context *ui, nx_key key, uint32_t modifiers) {
    nx_event event = {0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = key;
    event.modifiers = modifiers;
    return nx_context_queue_event(ui, &event) == NX_OK;
}

static int queue_key_up(nx_context *ui, nx_key key) {
    nx_event event = {0};
    event.type = NX_EVENT_KEY_UP;
    event.key = key;
    return nx_context_queue_event(ui, &event) == NX_OK;
}

static int queue_text(nx_context *ui, const char *text) {
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        nx_event event = {0};
        event.type = NX_EVENT_TEXT;
        event.scalar = *p;
        if (nx_context_queue_event(ui, &event) != NX_OK) return 0;
    }
    return 1;
}

static int queue_click(nx_context *ui, float x, float y) {
    nx_event down = {0}, up = {0};
    down.type = NX_EVENT_POINTER_DOWN;
    down.button = NX_BUTTON_LEFT;
    down.x = up.x = x;
    down.y = up.y = y;
    up.type = NX_EVENT_POINTER_UP;
    up.button = NX_BUTTON_LEFT;
    return nx_context_queue_event(ui, &down) == NX_OK &&
           nx_context_queue_event(ui, &up) == NX_OK;
}

static int save_frame(demo *app, const char *path) {
    const nx_command *commands = NULL;
    size_t count = 0;
    CHECK(nx_frame_commands(app->ui, &commands, &count) == NX_OK && count >= 10);
    uint8_t *pixels = (uint8_t *)malloc((size_t)WIDTH * HEIGHT * 4u);
    CHECK(pixels != NULL);
    nx_color background;
    int okay = nx_theme_color(app->ui, NX_COLOR_BACKGROUND, &background) == NX_OK &&
               nx_headless_render(commands, count, pixels, WIDTH, HEIGHT,
                                  WIDTH * 4u, background) == NX_OK;
    FILE *file = okay ? fopen(path, "wb") : NULL;
    if (!file) okay = 0;
    if (okay) okay = fprintf(file, "P6\n%u %u\n255\n", WIDTH, HEIGHT) > 0;
    for (size_t y = 0; okay && y < HEIGHT; ++y)
        for (size_t x = 0; okay && x < WIDTH; ++x)
            okay = fwrite(pixels + (y * WIDTH + x) * 4u, 1, 3, file) == 3;
    if (file && fclose(file) != 0) okay = 0;
    free(pixels);
    return okay;
}

static int run(const char *font_path, const char *prefix) {
    demo app = {0};
    const char *stage = "setup";
    int okay = nx_context_create(NULL, &app.ui) == NX_OK &&
               nx_text_field_create("A", 1, 4096, &app.project) == NX_OK &&
               nx_text_field_create("", 0, 4096, &app.owner) == NX_OK &&
               load_font(&app, font_path);
    char invalid_path[1024], submitted_path[1024];
    int a = snprintf(invalid_path, sizeof(invalid_path), "%s-invalid.ppm", prefix);
    int b = snprintf(submitted_path, sizeof(submitted_path), "%s-submitted.ppm", prefix);
    okay = okay && a > 0 && (size_t)a < sizeof(invalid_path) &&
           b > 0 && (size_t)b < sizeof(submitted_path);
    if (okay) {
        nx_theme theme;
        okay = nx_theme_preset(NX_THEME_LIGHT, &theme) == NX_OK &&
               nx_context_set_theme(app.ui, &theme) == NX_OK;
        stage = "initial invalid frame";
        if (okay) okay = draw_frame(&app) &&
                         !app.project_valid && !app.owner_valid &&
                         semantic_field(&app, 3, "A", 1) &&
                         semantic_field(&app, 4, "", 1) &&
                         save_frame(&app, invalid_path);
        stage = okay ? "disabled submit" : stage;
        if (okay) okay = queue_click(app.ui, 100, 365) &&
                         draw_frame(&app) && !app.submitted;
        stage = okay ? "project edit" : stage;
        if (okay) okay = nx_focus_set(app.ui, 3) == NX_OK &&
                         queue_key(app.ui, NX_KEY_A, NX_MOD_CONTROL) &&
                         queue_text(app.ui, "NexaUI") &&
                         draw_frame(&app) && app.project_valid &&
                         text_equals(app.project, "NexaUI");
        stage = okay ? "owner edit" : stage;
        if (okay) okay = nx_focus_set(app.ui, 4) == NX_OK &&
                         queue_text(app.ui, "Ada") &&
                         draw_frame(&app) && app.owner_valid &&
                         text_equals(app.owner, "Ada");
        stage = okay ? "confirmation" : stage;
        if (okay) okay = queue_click(app.ui, 145, 297) &&
                         draw_frame(&app) && app.confirmed;
        stage = okay ? "submission" : stage;
        if (okay) okay = nx_focus_set(app.ui, 6) == NX_OK &&
                         queue_key(app.ui, NX_KEY_SPACE, 0) &&
                         queue_key_up(app.ui, NX_KEY_SPACE) &&
                         draw_frame(&app) && app.submitted &&
                         semantic_field(&app, 3, "NexaUI", 0) &&
                         semantic_field(&app, 4, "Ada", 0) &&
                         save_frame(&app, submitted_path) &&
                         app.frames == 6;
    }
    if (okay) printf("created project NexaUI for Ada -> %s\n", submitted_path);
    else fprintf(stderr, "forms stage failed: %s\n", stage);
    close_demo(&app);
    return okay;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("usage: nexaui_04_forms FONT_FILE OUTPUT_PREFIX\n", stderr);
        return 2;
    }
    if (!run(argv[1], argv[2])) {
        fputs("forms example failed\n", stderr);
        return 1;
    }
    return 0;
}
