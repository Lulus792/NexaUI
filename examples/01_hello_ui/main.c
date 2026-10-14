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
#define HEIGHT 360u
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
    nx_text_layout *title, *subtitle;
    unsigned frames;
} demo;

static nx_utf8_span span(const char *text) {
    return (nx_utf8_span){text, strlen(text)};
}

static void close_demo(demo *app) {
    nx_renderer_close(&app->renderer);
    nx_context_destroy(app->ui);
    nx_window_destroy(app->window);
    nx_text_layout_destroy(app->subtitle);
    nx_text_layout_destroy(app->title);
    nx_font_collection_destroy(app->fonts);
    nx_font_close(app->font);
    free(app->font_bytes);
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
    if (!okay) return 0;
    static const char heading[] = "Hello, NexaUI";
    static const char detail[] = "A native window, a UI context, one command stream.";
    CHECK(nx_font_open(app->font_bytes, (size_t)length, NULL, &app->font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &app->fonts) == NX_OK);
    CHECK(nx_font_collection_add(app->fonts, app->font) == NX_OK);
    CHECK(nx_text_layout_create(app->fonts, heading, sizeof(heading) - 1u,
                                NX_BIDI_LTR, 32, 460, &app->title) == NX_OK);
    CHECK(nx_text_layout_create(app->fonts, detail, sizeof(detail) - 1u,
                                NX_BIDI_LTR, 16, 460, &app->subtitle) == NX_OK);
    return 1;
}

static int save_preview(const nx_command *commands, size_t count,
                        const char *path) {
    uint8_t *pixels = (uint8_t *)malloc((size_t)WIDTH * HEIGHT * 4u);
    CHECK(pixels != NULL);
    int okay = nx_headless_render(commands, count, pixels, WIDTH, HEIGHT,
        WIDTH * 4u, (nx_color){0.055f, 0.075f, 0.12f, 1}) == NX_OK;
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

static int draw_frame(demo *app, nx_window_info info, const char *preview_path) {
    size_t routed = 0;
    CHECK(nx_window_route_events(app->window, app->ui, &routed) == NX_OK);
    CHECK(nx_begin_frame(app->ui, info.width, info.height, 1.0 / 60.0) == NX_OK);
    nx_semantic_desc root = {0};
    root.id = 1;
    root.role = NX_ROLE_WINDOW;
    root.bounds = (nx_rect){0, 0, info.width, info.height};
    root.name = span("Hello UI example");
    CHECK(nx_semantics_add(app->ui, &root, NULL) == NX_OK);

    float card_width = info.width - 48.0f;
    if (card_width > 540.0f) card_width = 540.0f;
    int show_card = card_width > 160.0f && info.height > 190.0f;
    if (show_card) {
        float card_x = (info.width - card_width) * 0.5f;
        float card_y = (info.height - 220.0f) * 0.5f;
        if (card_y < 18.0f) card_y = 18.0f;
        nx_rect card = {card_x, card_y, card_width, 220};
        CHECK(nx_fill_round_rect(app->ui, card, 20,
                                (nx_color){0.15f, 0.19f, 0.27f, 1}) == NX_OK);
        CHECK(nx_stroke_rect(app->ui, card, 2,
                             (nx_color){0.31f, 0.39f, 0.51f, 1}) == NX_OK);
        CHECK(nx_fill_round_rect(app->ui,
                (nx_rect){card_x + 28, card_y + 32, 58, 58}, 14,
                (nx_color){0.22f, 0.65f, 0.95f, 1}) == NX_OK);
        CHECK(nx_fill_triangle(app->ui,
                (nx_point){card_x + 43, card_y + 73},
                (nx_point){card_x + 57, card_y + 43},
                (nx_point){card_x + 71, card_y + 73},
                (nx_color){1, 1, 1, 1}) == NX_OK);
        CHECK(nx_fill_rect(app->ui,
                (nx_rect){card_x + 28, card_y + 142, card_width - 56, 2},
                (nx_color){0.31f, 0.39f, 0.51f, 1}) == NX_OK);
        if (app->title) {
            CHECK(nx_label(app->ui, 2, 1,
                    (nx_rect){card_x + 104, card_y + 31, card_width - 132, 56},
                    app->title, (nx_point){card_x + 104, card_y + 43},
                    (nx_color){0.96f, 0.98f, 1, 1}, GLYPH_CACHE) == NX_OK);
            CHECK(nx_label(app->ui, 3, 1,
                    (nx_rect){card_x + 28, card_y + 157, card_width - 56, 42},
                    app->subtitle, (nx_point){card_x + 28, card_y + 164},
                    (nx_color){0.72f, 0.78f, 0.87f, 1}, GLYPH_CACHE) == NX_OK);
        } else {
            nx_semantic_desc title = {0};
            title.id = 2;
            title.parent_id = 1;
            title.role = NX_ROLE_TEXT;
            title.bounds = (nx_rect){card_x + 104, card_y + 31,
                                      card_width - 132, 56};
            title.name = span("Hello, NexaUI");
            CHECK(nx_semantics_add(app->ui, &title, NULL) == NX_OK);
        }
    }
    CHECK(nx_end_frame(app->ui) == NX_OK);
    size_t semantic_count = 0;
    CHECK(nx_semantics_count(app->ui, &semantic_count) == NX_OK &&
          semantic_count == (size_t)(show_card ? (app->title ? 3 : 2) : 1));
    if (show_card) {
        uint32_t index = UINT32_MAX;
        nx_semantic_view title_view;
        CHECK(nx_semantics_find(app->ui, 2, &index) == NX_OK &&
              nx_semantics_get(app->ui, index, &title_view) == NX_OK &&
              title_view.role == NX_ROLE_TEXT && title_view.parent_id == 1);
    }
    const nx_command *commands = NULL;
    size_t command_count = 0;
    CHECK(nx_frame_commands(app->ui, &commands, &command_count) == NX_OK &&
          (!show_card || command_count >= 5));
    RECT client;
    CHECK(GetClientRect(nx_window_native_hwnd(app->window), &client));
    nx_render_target target = {(uint32_t)(client.right - client.left),
                               (uint32_t)(client.bottom - client.top), info.scale};
    nx_render_outcome outcome;
    CHECK(nx_renderer_run(&app->renderer, target, commands, command_count,
                           (nx_color){0.055f, 0.075f, 0.12f, 1},
                           &outcome) == NX_OK);
    if (outcome == NX_RENDER_DEVICE_LOST)
        CHECK(nx_renderer_recover(&app->renderer) == NX_OK);
    if (preview_path) CHECK(save_preview(commands, command_count, preview_path));
    ++app->frames;
    (void)routed;
    return 1;
}

static int run(int smoke, const char *font_path, const char *preview_path) {
    demo app = {0};
    nx_window_desc desc = {0};
    desc.title_utf8 = "NexaUI 01: Hello UI";
    desc.width = WIDTH;
    desc.height = HEIGHT;
    desc.flags = smoke ? NX_WINDOW_HIDDEN : 0;
    int okay = nx_window_create(&desc, &app.window) == NX_OK &&
               nx_context_create(NULL, &app.ui) == NX_OK &&
               nx_d3d12_driver_open(app.window, &app.renderer) == NX_OK;
    if (okay && font_path) okay = load_font(&app, font_path);
    while (okay) {
        if (nx_window_poll(app.window, smoke ? 0u : 16u) != NX_OK) {
            okay = 0;
            break;
        }
        nx_window_info info;
        if (nx_window_get_info(app.window, &info) != NX_OK) {
            okay = 0;
            break;
        }
        if (info.should_close) break;
        if (info.width > 0 && info.height > 0 &&
            !draw_frame(&app, info, smoke && app.frames == 0 ? preview_path : NULL)) {
            okay = 0;
            break;
        }
        if (smoke && app.frames >= 2) break;
    }
    if (smoke && app.frames != 2) okay = 0;
    close_demo(&app);
    return okay;
}

int main(int argc, char **argv) {
    int smoke = argc > 1 && strcmp(argv[1], "--smoke") == 0;
    const char *font_path = NULL, *preview_path = NULL;
    if (smoke) {
        if (argc > 4) goto usage;
        if (argc >= 3) font_path = argv[2];
        if (argc == 4) preview_path = argv[3];
    } else {
        if (argc > 2) goto usage;
        if (argc == 2) font_path = argv[1];
    }
    if (!run(smoke, font_path, preview_path)) {
        fputs("hello UI example failed\n", stderr);
        return 1;
    }
    return 0;
usage:
    fputs("usage: nexaui_01_hello_ui [FONT_FILE] | --smoke [FONT_FILE [OUTPUT.ppm]]\n",
          stderr);
    return 2;
}
