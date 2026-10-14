#include "nexaui/nexaui.h"

#include <stdio.h>
#include <string.h>

#define WIDTH 180u
#define HEIGHT 140u
#define PIXELS ((size_t)WIDTH * HEIGHT)

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static int click(nx_context *ctx, nx_rect bounds) {
    nx_event event = {0};
    event.button = NX_BUTTON_LEFT;
    event.x = bounds.x + bounds.width * 0.5f;
    event.y = bounds.y + bounds.height * 0.5f;
    event.type = NX_EVENT_POINTER_DOWN;
    if (nx_context_queue_event(ctx, &event) != NX_OK) return 0;
    event.type = NX_EVENT_POINTER_UP;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int checked_semantic(nx_context *ctx, uint64_t id,
                            nx_semantic_role role, int checked) {
    uint32_t index = UINT32_MAX;
    nx_semantic_view view;
    return nx_semantics_find(ctx, id, &index) == NX_OK &&
           nx_semantics_get(ctx, index, &view) == NX_OK &&
           view.parent_id == 1 && view.role == role &&
           !!(view.state & NX_SEM_CHECKED) == !!checked;
}

/* PPM keeps the reviewed reference readable without an image codec dependency. */
static int baseline(const char *name, const uint8_t *rgba,
                    int update, int report) {
    char path[512], header[64];
    int path_length = snprintf(path, sizeof(path), "%s/%s.ppm",
                               NEXAUI_VISUAL_BASELINE_DIR, name);
    int header_length = snprintf(header, sizeof(header), "P6\n%u %u\n255\n",
                                 WIDTH, HEIGHT);
    if (path_length < 0 || (size_t)path_length >= sizeof(path) ||
        header_length < 0 || (size_t)header_length >= sizeof(header)) return 0;
    FILE *file = fopen(path, update ? "wb" : "rb");
    if (!file) {
        if (report) fprintf(stderr, "Cannot open baseline: %s\n", path);
        return 0;
    }
    int okay = 1;
    if (update) {
        okay = fwrite(header, 1, (size_t)header_length, file) ==
               (size_t)header_length;
        for (size_t i = 0; okay && i < PIXELS; ++i)
            okay = fwrite(rgba + i * 4u, 1, 3, file) == 3;
    } else {
        char actual_header[64];
        okay = fread(actual_header, 1, (size_t)header_length, file) ==
               (size_t)header_length &&
               memcmp(actual_header, header, (size_t)header_length) == 0;
        for (size_t i = 0; okay && i < PIXELS; ++i) {
            uint8_t rgb[3];
            if (fread(rgb, 1, 3, file) != 3 ||
                memcmp(rgb, rgba + i * 4u, 3) != 0 || rgba[i * 4u + 3u] != 255) {
                if (report) fprintf(stderr, "%s differs at pixel (%u,%u)\n",
                                    name, (unsigned)(i % WIDTH),
                                    (unsigned)(i / WIDTH));
                okay = 0;
            }
        }
        if (okay && fgetc(file) != EOF) okay = 0;
    }
    if (fclose(file) != 0) okay = 0;
    return okay;
}

static int run_scene(nx_theme_kind kind, const char *theme_name, int update) {
    nx_layout *layout = NULL;
    nx_context *ctx = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_layout_desc column = nx_layout_default_desc(NX_LAYOUT_COLUMN);
    column.padding = (nx_insets){12, 12, 12, 12};
    column.gap = 8;
    nx_layout_desc leaf = nx_layout_default_desc(NX_LAYOUT_LEAF);
    leaf.width.preferred = 138;
    leaf.width.has_preferred = 1;
    leaf.height.preferred = 24;
    leaf.height.has_preferred = 1;
    uint32_t root, nodes[4];
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &column, &root) == NX_OK);
    for (unsigned i = 0; i < 4; ++i) {
        nx_layout_desc item = leaf;
        item.height.preferred = i == 0 ? 24.0f : i == 3 ? 12.0f : 20.0f;
        CHECK(nx_layout_add(layout, root, &item, &nodes[i]) == NX_OK);
    }
    nx_size desired;
    nx_rect bounds[4];
    CHECK(nx_layout_measure(layout, root, WIDTH - 16, HEIGHT - 16,
                            &desired) == NX_OK);
    CHECK(nx_layout_arrange(layout, root,
                            (nx_rect){8, 8, WIDTH - 16, HEIGHT - 16}) == NX_OK);
    for (unsigned i = 0; i < 4; ++i)
        CHECK(nx_layout_get_rect(layout, nodes[i], &bounds[i]) == NX_OK);
    CHECK(bounds[0].y < bounds[1].y && bounds[1].y < bounds[2].y &&
          bounds[2].y < bounds[3].y &&
          bounds[3].y + bounds[3].height <= HEIGHT - 8);

    nx_theme theme;
    CHECK(nx_theme_preset(kind, &theme) == NX_OK);
    CHECK(nx_context_set_theme(ctx, &theme) == NX_OK);
    nx_control_desc button = {0}, checkbox = {0}, toggle = {0};
    button.id = 2; button.parent_id = 1; button.bounds = bounds[0];
    button.name = (nx_utf8_span){"Apply", 5};
    checkbox.id = 3; checkbox.parent_id = 1; checkbox.bounds = bounds[1];
    checkbox.name = (nx_utf8_span){"Confirm", 7};
    toggle.id = 4; toggle.parent_id = 1; toggle.bounds = bounds[2];
    toggle.name = (nx_utf8_span){"Enabled", 7};
    nx_value_control_desc slider = {0};
    slider.control.id = 5; slider.control.parent_id = 1;
    slider.control.bounds = bounds[3];
    slider.control.name = (nx_utf8_span){"Level", 5};
    slider.minimum = 0; slider.maximum = 100; slider.step = 5;
    int checked = 0, on = 0;
    double value = 35;
    for (unsigned frame = 0; frame < 4; ++frame) {
        if (frame == 1) CHECK(click(ctx, bounds[1]));
        if (frame == 2) CHECK(click(ctx, bounds[2]));
        if (frame == 3) CHECK(click(ctx, bounds[0]));
        CHECK(nx_begin_frame(ctx, WIDTH, HEIGHT, 0) == NX_OK);
        nx_color background;
        CHECK(nx_theme_color(ctx, NX_COLOR_BACKGROUND, &background) == NX_OK);
        CHECK(nx_fill_rect(ctx, (nx_rect){0, 0, WIDTH, HEIGHT}, background) == NX_OK);
        nx_container_desc panel = {0};
        panel.id = 1;
        panel.bounds = (nx_rect){8, 8, WIDTH - 16, HEIGHT - 16};
        panel.name = (nx_utf8_span){"Controls", 8};
        CHECK(nx_panel(ctx, &panel) == NX_OK);
        nx_interaction_state button_state, check_state, toggle_state, slider_state;
        CHECK(nx_button(ctx, &button, &button_state) == NX_OK);
        CHECK(nx_checkbox(ctx, &checkbox, &checked, &check_state) == NX_OK);
        CHECK(nx_toggle(ctx, &toggle, &on, &toggle_state) == NX_OK);
        CHECK(nx_slider(ctx, &slider, &value, &slider_state) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        CHECK(checked == (frame >= 1) && on == (frame >= 2));
        CHECK(button_state.clicked == (frame == 3));
        CHECK(checked_semantic(ctx, 3, NX_ROLE_CHECKBOX, checked));
        CHECK(checked_semantic(ctx, 4, NX_ROLE_SWITCH, on));
        size_t semantic_count;
        CHECK(nx_semantics_count(ctx, &semantic_count) == NX_OK &&
              semantic_count == 5);
        if (frame == 0 || frame == 3) {
            const nx_command *commands;
            size_t command_count;
            uint8_t pixels[PIXELS * 4u];
            CHECK(nx_frame_commands(ctx, &commands, &command_count) == NX_OK &&
                  command_count > 8);
            CHECK(nx_headless_render(commands, command_count, pixels, WIDTH,
                                     HEIGHT, WIDTH * 4u, background) == NX_OK);
            if (frame == 0) {
                size_t y = (size_t)(bounds[1].y + bounds[1].height * 0.5f);
                size_t edge = (y * WIDTH + (size_t)bounds[1].x) * 4u;
                size_t panel = (y * WIDTH + (size_t)(bounds[1].x +
                                       bounds[1].height + 5.0f)) * 4u;
                CHECK(memcmp(pixels + edge, pixels + panel, 3) != 0);
            }
            char name[64];
            int length = snprintf(name, sizeof(name), "%s-%s", theme_name,
                                  frame == 0 ? "idle" : "active");
            CHECK(length > 0 && (size_t)length < sizeof(name));
            CHECK(baseline(name, pixels, update, 1));
            if (kind == NX_THEME_LIGHT && frame == 0 && !update) {
                pixels[0] ^= 1u;
                CHECK(!baseline(name, pixels, 0, 0));
            }
        }
    }
    nx_context_destroy(ctx);
    nx_layout_destroy(layout);
    return 0;
}

int main(int argc, char **argv) {
    int update = argc == 2 && strcmp(argv[1], "--update-baselines") == 0;
    if (argc > 1 && !update) return 2;
    if (run_scene(NX_THEME_LIGHT, "light", update)) return 1;
    if (run_scene(NX_THEME_DARK, "dark", update)) return 1;
    if (run_scene(NX_THEME_HIGH_CONTRAST, "high-contrast", update)) return 1;
    return 0;
}
