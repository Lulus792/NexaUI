#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>

#include "nexaui/nexaui.h"
#include "window_internal.h"
#include "renderer.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 640u
#define HEIGHT 400u
#define FONT_LIMIT (64u * 1024u * 1024u)
#define GLYPH_CACHE (2u * 1024u * 1024u)

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 0; \
} } while (0)

typedef struct demo {
    nx_window *window;
    nx_context *ui;
    nx_renderer_driver renderer;
    uint8_t *font_bytes;
    nx_font *font;
    nx_font_collection *fonts;
    nx_text_layout *title, *hint, *button_label, *check_label, *count_label;
    unsigned presses, frames;
    int enabled;
} demo;

static nx_utf8_span span(const char *text) {
    return (nx_utf8_span){text, strlen(text)};
}

static void close_demo(demo *app) {
    nx_renderer_close(&app->renderer);
    nx_context_destroy(app->ui);
    nx_window_destroy(app->window);
    nx_text_layout_destroy(app->count_label);
    nx_text_layout_destroy(app->check_label);
    nx_text_layout_destroy(app->button_label);
    nx_text_layout_destroy(app->hint);
    nx_text_layout_destroy(app->title);
    nx_font_collection_destroy(app->fonts);
    nx_font_close(app->font);
    free(app->font_bytes);
}

static int make_layout(demo *app, const char *text, int ppem,
                       nx_text_layout **out) {
    return nx_text_layout_create(app->fonts, text, strlen(text), NX_BIDI_LTR,
                                 ppem, 500, out) == NX_OK;
}

static int update_count_label(demo *app) {
    char text[48];
    int length = snprintf(text, sizeof(text), "Presses: %u", app->presses);
    CHECK(length > 0 && (size_t)length < sizeof(text));
    nx_text_layout *next = NULL;
    CHECK(make_layout(app, text, 23, &next));
    nx_text_layout_destroy(app->count_label);
    app->count_label = next;
    return 1;
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
    CHECK(make_layout(app, "Buttons & input", 30, &app->title));
    CHECK(make_layout(app, "Click a control or focus it with Tab and press Space.",
                      15, &app->hint));
    CHECK(make_layout(app, "Add one", 19, &app->button_label));
    CHECK(make_layout(app, "Enable output", 18, &app->check_label));
    CHECK(update_count_label(app));
    return 1;
}

static int save_preview(const nx_command *commands, size_t count,
                        const char *path, nx_color background) {
    uint8_t *pixels = (uint8_t *)malloc((size_t)WIDTH * HEIGHT * 4u);
    CHECK(pixels != NULL);
    int okay = nx_headless_render(commands, count, pixels, WIDTH, HEIGHT,
                                  WIDTH * 4u, background) == NX_OK;
    FILE *file = okay ? fopen(path, "wb") : NULL;
    if (!file) okay = 0;
    if (okay) okay = fprintf(file, "P6\n%u %u\n255\n", WIDTH, HEIGHT) > 0;
    uint8_t row[WIDTH * 3u];
    for (size_t y = 0; okay && y < HEIGHT; ++y) {
        for (size_t x = 0; x < WIDTH; ++x) {
            const uint8_t *pixel = pixels + (y * WIDTH + x) * 4u;
            row[x * 3u] = pixel[0];
            row[x * 3u + 1u] = pixel[1];
            row[x * 3u + 2u] = pixel[2];
        }
        okay = fwrite(row, 1, sizeof(row), file) == sizeof(row);
    }
    if (file && fclose(file) != 0) okay = 0;
    free(pixels);
    return okay;
}

static int inspect_semantics(demo *app) {
    size_t count;
    uint32_t index;
    nx_semantic_view view;
    CHECK(nx_semantics_count(app->ui, &count) == NX_OK && count == 5);
    CHECK(nx_semantics_find(app->ui, 3, &index) == NX_OK &&
          nx_semantics_get(app->ui, index, &view) == NX_OK &&
          view.role == NX_ROLE_BUTTON && view.parent_id == 2);
    CHECK(nx_semantics_find(app->ui, 4, &index) == NX_OK &&
          nx_semantics_get(app->ui, index, &view) == NX_OK &&
          view.role == NX_ROLE_CHECKBOX && view.parent_id == 2 &&
          !!(view.state & NX_SEM_CHECKED) == !!app->enabled);
    CHECK(nx_semantics_find(app->ui, 5, &index) == NX_OK &&
          nx_semantics_get(app->ui, index, &view) == NX_OK &&
          view.role == NX_ROLE_PROGRESS && view.parent_id == 2);
    char expected[32];
    int length = snprintf(expected, sizeof(expected), "%u", app->presses);
    CHECK(length > 0 && (size_t)length < sizeof(expected) &&
          view.value.length == (size_t)length &&
          memcmp(view.value.data, expected, (size_t)length) == 0);
    return 1;
}

static int draw_frame(demo *app, nx_window_info info,
                      const char *preview_path) {
    size_t routed;
    CHECK(nx_window_route_events(app->window, app->ui, &routed) == NX_OK);
    CHECK(nx_begin_frame(app->ui, info.width, info.height, 1.0 / 60.0) == NX_OK);
    nx_color background, text, muted, accent, track;
    CHECK(nx_theme_color(app->ui, NX_COLOR_BACKGROUND, &background) == NX_OK);
    CHECK(nx_theme_color(app->ui, NX_COLOR_TEXT, &text) == NX_OK);
    CHECK(nx_theme_color(app->ui, NX_COLOR_TEXT_MUTED, &muted) == NX_OK);
    CHECK(nx_theme_color(app->ui, NX_COLOR_ACCENT, &accent) == NX_OK);
    CHECK(nx_theme_color(app->ui, NX_COLOR_SURFACE_RAISED, &track) == NX_OK);

    nx_semantic_desc root = {0};
    root.id = 1;
    root.role = NX_ROLE_WINDOW;
    root.bounds = (nx_rect){0, 0, info.width, info.height};
    root.name = span("Buttons and input example");
    CHECK(nx_semantics_add(app->ui, &root, NULL) == NX_OK);

    float card_width = info.width - 48.0f;
    if (card_width > 560.0f) card_width = 560.0f;
    if (card_width >= 500.0f && info.height >= 340.0f) {
        float x = (info.width - card_width) * 0.5f;
        float y = (info.height - 320.0f) * 0.5f;
        nx_container_desc panel = {0};
        panel.id = 2;
        panel.parent_id = 1;
        panel.bounds = (nx_rect){x, y, card_width, 320};
        panel.name = span("Interaction card");
        panel.label_layout = app->title;
        panel.label_origin = (nx_point){x + 30, y + 25};
        panel.glyph_cache_bytes = GLYPH_CACHE;
        CHECK(nx_panel(app->ui, &panel) == NX_OK);
        CHECK(nx_draw_text_layout(app->ui, app->hint,
                                  (nx_point){x + 30, y + 83}, muted,
                                  GLYPH_CACHE) == NX_OK);

        nx_control_desc button = {0};
        button.id = 3;
        button.parent_id = 2;
        button.bounds = (nx_rect){x + 30, y + 145, 200, 48};
        button.name = span("Add one");
        button.label_layout = app->button_label;
        button.label_origin = (nx_point){x + 91, y + 155};
        button.glyph_cache_bytes = GLYPH_CACHE;
        nx_interaction_state button_state;
        CHECK(nx_button(app->ui, &button, &button_state) == NX_OK);
        if (button_state.clicked) {
            app->presses += button_state.clicked;
            CHECK(update_count_label(app));
        }

        nx_control_desc checkbox = {0};
        checkbox.id = 4;
        checkbox.parent_id = 2;
        checkbox.bounds = (nx_rect){x + 272, y + 154, 250, 28};
        checkbox.name = span("Enable output");
        checkbox.label_layout = app->check_label;
        checkbox.label_origin = (nx_point){x + 309, y + 154};
        checkbox.glyph_cache_bytes = GLYPH_CACHE;
        nx_interaction_state check_state;
        CHECK(nx_checkbox(app->ui, &checkbox, &app->enabled, &check_state) == NX_OK);

        CHECK(nx_draw_text_layout(app->ui, app->count_label,
                                  (nx_point){x + 30, y + 230}, text,
                                  GLYPH_CACHE) == NX_OK);
        nx_rect progress = {x + 30, y + 275, card_width - 60, 12};
        char value[32];
        int length = snprintf(value, sizeof(value), "%u", app->presses);
        CHECK(length > 0 && (size_t)length < sizeof(value));
        nx_semantic_desc meter = {0};
        meter.id = 5;
        meter.parent_id = 2;
        meter.role = NX_ROLE_PROGRESS;
        meter.bounds = progress;
        meter.name = span("Press count");
        meter.value = (nx_utf8_span){value, (size_t)length};
        CHECK(nx_semantics_add(app->ui, &meter, NULL) == NX_OK);
        CHECK(nx_fill_round_rect(app->ui, progress, 6, track) == NX_OK);
        if (app->enabled && app->presses) {
            nx_rect fill = progress;
            fill.width *= (float)(app->presses < 10u ? app->presses : 10u) / 10.0f;
            if (fill.width > 0)
                CHECK(nx_fill_round_rect(app->ui, fill, 6, accent) == NX_OK);
        }
    }
    CHECK(nx_end_frame(app->ui) == NX_OK);
    if (card_width >= 500.0f && info.height >= 340.0f)
        CHECK(inspect_semantics(app));

    const nx_command *commands = NULL;
    size_t command_count = 0;
    CHECK(nx_frame_commands(app->ui, &commands, &command_count) == NX_OK);
    RECT client;
    CHECK(GetClientRect(nx_window_native_hwnd(app->window), &client));
    nx_render_target target = {(uint32_t)(client.right - client.left),
                               (uint32_t)(client.bottom - client.top), info.scale};
    nx_render_outcome outcome;
    CHECK(nx_renderer_run(&app->renderer, target, commands, command_count,
                          background, &outcome) == NX_OK);
    if (outcome == NX_RENDER_DEVICE_LOST)
        CHECK(nx_renderer_recover(&app->renderer) == NX_OK);
    if (preview_path)
        CHECK(save_preview(commands, command_count, preview_path, background));
    ++app->frames;
    (void)routed;
    return 1;
}

static int tick(demo *app, int smoke, const char *preview_path) {
    CHECK(nx_window_poll(app->window, smoke ? 0u : 16u) == NX_OK);
    nx_window_info info;
    CHECK(nx_window_get_info(app->window, &info) == NX_OK);
    if (info.should_close) return 1;
    if (info.width > 0 && info.height > 0)
        CHECK(draw_frame(app, info, preview_path));
    return 1;
}

static int post_click(HWND hwnd, float scale, int x, int y) {
    LPARAM at = MAKELPARAM((int)(x * scale), (int)(y * scale));
    return PostMessageW(hwnd, WM_MOUSEMOVE, 0, at) &&
           PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, at) &&
           PostMessageW(hwnd, WM_LBUTTONUP, 0, at);
}

static int smoke_run(demo *app, const char *preview_path) {
    HWND hwnd = nx_window_native_hwnd(app->window);
    nx_window_info info;
    CHECK(hwnd && nx_window_get_info(app->window, &info) == NX_OK);
    CHECK(tick(app, 1, NULL));
    CHECK(app->frames == 1 && app->presses == 0 && !app->enabled);
    int x = (int)((info.width - 560.0f) * 0.5f);
    int y = (int)((info.height - 320.0f) * 0.5f);
    CHECK(post_click(hwnd, info.scale, x + 130, y + 169));
    CHECK(tick(app, 1, NULL));
    CHECK(app->presses == 1 && !app->enabled);
    CHECK(post_click(hwnd, info.scale, x + 380, y + 168));
    CHECK(tick(app, 1, NULL));
    CHECK(app->presses == 1 && app->enabled);
    CHECK(nx_focus_set(app->ui, 3) == NX_OK);
    CHECK(PostMessageW(hwnd, WM_KEYDOWN, VK_SPACE, 0));
    CHECK(PostMessageW(hwnd, WM_KEYUP, VK_SPACE, 0));
    CHECK(tick(app, 1, NULL));
    CHECK(app->frames == 4 && app->presses == 2 && app->enabled);
    CHECK(nx_focus_set(app->ui, 4) == NX_OK);
    CHECK(PostMessageW(hwnd, WM_KEYDOWN, VK_SPACE, 0));
    CHECK(PostMessageW(hwnd, WM_KEYUP, VK_SPACE, 0));
    CHECK(tick(app, 1, NULL));
    CHECK(app->frames == 5 && app->presses == 2 && !app->enabled);
    CHECK(PostMessageW(hwnd, WM_KEYDOWN, VK_SPACE, 0));
    CHECK(PostMessageW(hwnd, WM_KEYUP, VK_SPACE, 0));
    CHECK(tick(app, 1, preview_path));
    CHECK(app->frames == 6 && app->presses == 2 && app->enabled);
    uint32_t index;
    nx_semantic_view view;
    CHECK(nx_semantics_find(app->ui, 4, &index) == NX_OK &&
          nx_semantics_get(app->ui, index, &view) == NX_OK &&
          (view.state & NX_SEM_FOCUSED));
    return 1;
}

static int run(const char *font_path, int smoke, const char *preview_path) {
    demo app = {0};
    nx_window_desc desc = {0};
    desc.title_utf8 = "NexaUI 02: Buttons and input";
    desc.width = WIDTH;
    desc.height = HEIGHT;
    desc.flags = smoke ? NX_WINDOW_HIDDEN : 0;
    int okay = nx_window_create(&desc, &app.window) == NX_OK &&
               nx_context_create(NULL, &app.ui) == NX_OK &&
               nx_d3d12_driver_open(app.window, &app.renderer) == NX_OK &&
               load_font(&app, font_path);
    if (okay && smoke) okay = smoke_run(&app, preview_path);
    while (okay && !smoke) {
        nx_window_info info;
        if (nx_window_get_info(app.window, &info) != NX_OK) {
            okay = 0;
            break;
        }
        if (info.should_close) break;
        okay = tick(&app, 0, NULL);
    }
    close_demo(&app);
    return okay;
}

int main(int argc, char **argv) {
    if (argc == 2) return run(argv[1], 0, NULL) ? 0 : 1;
    if (argc == 4 && strcmp(argv[1], "--smoke") == 0)
        return run(argv[2], 1, argv[3]) ? 0 : 1;
    fputs("usage: nexaui_02_buttons_and_input FONT_FILE | "
          "--smoke FONT_FILE OUTPUT.ppm\n", stderr);
    return 2;
}
