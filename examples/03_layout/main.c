#include "nexaui/nexaui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_LIMIT (64u * 1024u * 1024u)
#define GLYPH_CACHE (2u * 1024u * 1024u)

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 0; \
} } while (0)

typedef struct demo {
    nx_context *ui;
    nx_layout *layout;
    uint8_t *font_bytes;
    nx_font *font;
    nx_font_collection *fonts;
    nx_text_layout *heading, *navigation, *content, *status, *action;
} demo;

static nx_utf8_span span(const char *text) {
    return (nx_utf8_span){text, strlen(text)};
}

static void close_demo(demo *app) {
    nx_text_layout_destroy(app->action);
    nx_text_layout_destroy(app->status);
    nx_text_layout_destroy(app->content);
    nx_text_layout_destroy(app->navigation);
    nx_text_layout_destroy(app->heading);
    nx_font_collection_destroy(app->fonts);
    nx_font_close(app->font);
    free(app->font_bytes);
    nx_layout_destroy(app->layout);
    nx_context_destroy(app->ui);
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
    CHECK(make_layout(app, "Adaptive workspace", 28, &app->heading));
    CHECK(make_layout(app, "Navigation", 20, &app->navigation));
    CHECK(make_layout(app, "Content", 20, &app->content));
    CHECK(make_layout(app, "Layout follows available space", 16, &app->status));
    CHECK(make_layout(app, "Action", 17, &app->action));
    return 1;
}

static nx_layout_desc leaf(float width, float height) {
    nx_layout_desc desc = nx_layout_default_desc(NX_LAYOUT_LEAF);
    desc.width.preferred = width;
    desc.width.has_preferred = 1;
    desc.height.preferred = height;
    desc.height.has_preferred = 1;
    return desc;
}

static int within(nx_rect child, nx_rect parent) {
    const float e = 0.01f;
    return child.x + e >= parent.x && child.y + e >= parent.y &&
           child.x + child.width <= parent.x + parent.width + e &&
           child.y + child.height <= parent.y + parent.height + e;
}

static int save_preview(nx_context *ui, unsigned width, unsigned height,
                        const char *path, nx_color background) {
    const nx_command *commands = NULL;
    size_t command_count = 0;
    CHECK(nx_frame_commands(ui, &commands, &command_count) == NX_OK &&
          command_count >= 10);
    uint8_t *pixels = (uint8_t *)malloc((size_t)width * height * 4u);
    CHECK(pixels != NULL);
    int okay = nx_headless_render(commands, command_count, pixels, width,
                                  height, width * 4u, background) == NX_OK;
    FILE *file = okay ? fopen(path, "wb") : NULL;
    if (!file) okay = 0;
    if (okay) okay = fprintf(file, "P6\n%u %u\n255\n", width, height) > 0;
    for (size_t y = 0; okay && y < height; ++y)
        for (size_t x = 0; okay && x < width; ++x)
            okay = fwrite(pixels + (y * width + x) * 4u, 1, 3, file) == 3;
    if (file && fclose(file) != 0) okay = 0;
    free(pixels);
    return okay;
}

static int panel(demo *app, uint64_t id, nx_rect bounds,
                 const char *name, nx_text_layout *label, int group) {
    nx_container_desc desc = {0};
    desc.id = id;
    desc.parent_id = 1;
    desc.bounds = bounds;
    desc.name = span(name);
    desc.label_layout = label;
    desc.label_origin = (nx_point){bounds.x + 18, bounds.y + 14};
    desc.glyph_cache_bytes = GLYPH_CACHE;
    return (group ? nx_group(app->ui, &desc) : nx_panel(app->ui, &desc)) == NX_OK;
}

static int draw_size(demo *app, unsigned width, unsigned height,
                     const char *path) {
    int narrow = width < 500;
    nx_layout_reset(app->layout);
    nx_layout_desc root_desc = nx_layout_default_desc(NX_LAYOUT_COLUMN);
    root_desc.padding = (nx_insets){20, 20, 20, 20};
    root_desc.gap = 14;
    nx_layout_desc body_desc = nx_layout_default_desc(
        narrow ? NX_LAYOUT_COLUMN : NX_LAYOUT_ROW);
    body_desc.gap = 14;
    body_desc.height.grow = 1;
    body_desc.align_x = NX_ALIGN_STRETCH;
    nx_layout_desc header_desc = leaf(200, 70);
    header_desc.align_x = NX_ALIGN_STRETCH;
    nx_layout_desc nav_desc = leaf(narrow ? 220 : 190, narrow ? 120 : 220);
    if (narrow) nav_desc.align_x = NX_ALIGN_STRETCH;
    else nav_desc.align_y = NX_ALIGN_STRETCH;
    nx_layout_desc main_desc = leaf(200, 190);
    main_desc.width.grow = 1;
    main_desc.height.grow = 1;
    main_desc.align_x = NX_ALIGN_STRETCH;
    main_desc.align_y = NX_ALIGN_STRETCH;
    nx_layout_desc footer_desc = leaf(200, 50);
    footer_desc.align_x = NX_ALIGN_STRETCH;
    uint32_t root, header, body, nav, main, footer;
    CHECK(nx_layout_add(app->layout, NX_LAYOUT_NO_PARENT, &root_desc, &root) == NX_OK);
    CHECK(nx_layout_add(app->layout, root, &header_desc, &header) == NX_OK);
    CHECK(nx_layout_add(app->layout, root, &body_desc, &body) == NX_OK);
    CHECK(nx_layout_add(app->layout, body, &nav_desc, &nav) == NX_OK);
    CHECK(nx_layout_add(app->layout, body, &main_desc, &main) == NX_OK);
    CHECK(nx_layout_add(app->layout, root, &footer_desc, &footer) == NX_OK);
    nx_size desired;
    CHECK(nx_layout_measure(app->layout, root, (float)width,
                            (float)height, &desired) == NX_OK);
    CHECK(nx_layout_arrange(app->layout, root,
                            (nx_rect){0, 0, (float)width, (float)height}) == NX_OK);
    nx_rect rh, rn, rm, rf;
    CHECK(nx_layout_get_rect(app->layout, header, &rh) == NX_OK);
    CHECK(nx_layout_get_rect(app->layout, nav, &rn) == NX_OK);
    CHECK(nx_layout_get_rect(app->layout, main, &rm) == NX_OK);
    CHECK(nx_layout_get_rect(app->layout, footer, &rf) == NX_OK);
    nx_rect viewport = {0, 0, (float)width, (float)height};
    CHECK(within(rh, viewport) && within(rn, viewport) &&
          within(rm, viewport) && within(rf, viewport));
    CHECK(narrow ? rn.y + rn.height < rm.y : rn.x + rn.width < rm.x);
    CHECK(rm.width >= 200 && rm.height >= 190);

    CHECK(nx_begin_frame(app->ui, (float)width, (float)height, 0) == NX_OK);
    nx_color background, accent, muted;
    CHECK(nx_theme_color(app->ui, NX_COLOR_BACKGROUND, &background) == NX_OK);
    CHECK(nx_theme_color(app->ui, NX_COLOR_ACCENT, &accent) == NX_OK);
    CHECK(nx_theme_color(app->ui, NX_COLOR_TEXT_MUTED, &muted) == NX_OK);
    nx_semantic_desc window = {0};
    window.id = 1;
    window.role = NX_ROLE_WINDOW;
    window.bounds = viewport;
    window.name = span("Adaptive workspace");
    CHECK(nx_semantics_add(app->ui, &window, NULL) == NX_OK);
    CHECK(panel(app, 2, rh, "Header", app->heading, 0));
    CHECK(panel(app, 3, rn, "Navigation", app->navigation, 1));
    CHECK(panel(app, 4, rm, "Content", app->content, 0));
    CHECK(panel(app, 5, rf, "Status", NULL, 1));
    CHECK(nx_fill_round_rect(app->ui,
        (nx_rect){rm.x + 18, rm.y + 60, rm.width - 36, 42}, 8, accent) == NX_OK);
    nx_control_desc button = {0};
    button.id = 6;
    button.parent_id = 4;
    button.bounds = (nx_rect){rm.x + 18, rm.y + 123, 150, 42};
    button.name = span("Action");
    button.label_layout = app->action;
    button.label_origin = (nx_point){rm.x + 65, rm.y + 131};
    button.glyph_cache_bytes = GLYPH_CACHE;
    nx_interaction_state state;
    CHECK(nx_button(app->ui, &button, &state) == NX_OK);
    CHECK(nx_draw_text_layout(app->ui, app->status,
        (nx_point){rf.x + 18, rf.y + 12}, muted, GLYPH_CACHE) == NX_OK);
    CHECK(nx_end_frame(app->ui) == NX_OK);
    size_t semantics_count;
    uint32_t index;
    nx_semantic_view view;
    CHECK(nx_semantics_count(app->ui, &semantics_count) == NX_OK &&
          semantics_count == 6);
    CHECK(nx_semantics_find(app->ui, 6, &index) == NX_OK &&
          nx_semantics_get(app->ui, index, &view) == NX_OK &&
          view.parent_id == 4 && view.role == NX_ROLE_BUTTON &&
          within(view.bounds, rm));
    CHECK(save_preview(app->ui, width, height, path, background));
    printf("%ux%u: navigation (%.0f,%.0f %.0fx%.0f), content "
           "(%.0f,%.0f %.0fx%.0f)\n", width, height,
           rn.x, rn.y, rn.width, rn.height,
           rm.x, rm.y, rm.width, rm.height);
    return 1;
}

static int run(const char *font_path, const char *prefix) {
    demo app = {0};
    int okay = nx_context_create(NULL, &app.ui) == NX_OK &&
               nx_layout_create(NULL, 0, &app.layout) == NX_OK &&
               load_font(&app, font_path);
    if (okay) {
        char wide[1024], narrow[1024];
        int a = snprintf(wide, sizeof(wide), "%s-wide.ppm", prefix);
        int b = snprintf(narrow, sizeof(narrow), "%s-narrow.ppm", prefix);
        okay = a > 0 && (size_t)a < sizeof(wide) &&
               b > 0 && (size_t)b < sizeof(narrow) &&
               draw_size(&app, 720, 400, wide) &&
               draw_size(&app, 360, 620, narrow);
    }
    close_demo(&app);
    return okay;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("usage: nexaui_03_layout FONT_FILE OUTPUT_PREFIX\n", stderr);
        return 2;
    }
    return run(argv[1], argv[2]) ? 0 : 1;
}
