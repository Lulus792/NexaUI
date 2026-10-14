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

#define WIDTH 1180u
#define HEIGHT 700u
#define CAPACITY 512u
#define SCRATCH_CAPACITY 16384u
#define FONT_LIMIT (64u * 1024u * 1024u)
#define GLYPH_CACHE (4u * 1024u * 1024u)

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 0; \
} } while (0)

typedef struct showcase {
    nx_window *window;
    nx_context *ui;
    nx_renderer_driver renderer;
    uint8_t *font_bytes;
    nx_font *font;
    nx_font_collection *fonts;
    nx_text_layout *title, *plot_title, *inspector_title, *small_layout;
    nx_text_layout *buttons[4], *tree_labels[3], *status_layout;
    nx_series_envelope *scratch;
    nx_series_point points[CAPACITY];
    size_t head, count;
    uint64_t next_x, steps;
    uint64_t last_tick_ms;
    double position, velocity, gain;
    double accumulated_seconds;
    unsigned pending_steps;
    nx_plot_state plot_state;
    uint64_t selected_id;
    int expanded, running;
    unsigned frames;
} showcase;

static nx_utf8_span span(const char *text) {
    return (nx_utf8_span){text, strlen(text)};
}

static void close_showcase(showcase *app) {
    nx_renderer_close(&app->renderer);
    nx_context_destroy(app->ui);
    nx_window_destroy(app->window);
    nx_text_layout_destroy(app->status_layout);
    for (size_t i = 0; i < 3; ++i)
        nx_text_layout_destroy(app->tree_labels[i]);
    for (size_t i = 0; i < 4; ++i)
        nx_text_layout_destroy(app->buttons[i]);
    nx_text_layout_destroy(app->inspector_title);
    nx_text_layout_destroy(app->plot_title);
    nx_text_layout_destroy(app->small_layout);
    nx_text_layout_destroy(app->title);
    nx_font_collection_destroy(app->fonts);
    nx_font_close(app->font);
    free(app->font_bytes);
    free(app->scratch);
}

static int make_layout(showcase *app, const char *text, int ppem,
                       nx_text_layout **out) {
    return nx_text_layout_create(app->fonts, text, strlen(text), NX_BIDI_LTR,
                                 ppem, 700, out) == NX_OK;
}

static int load_font(showcase *app, const char *path) {
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
    CHECK(make_layout(app, "NexaUI Studio", 27, &app->title));
    CHECK(make_layout(app, "Position over time", 21, &app->plot_title));
    CHECK(make_layout(app, "Inspector", 21, &app->inspector_title));
    CHECK(make_layout(app, "Resize window to see the dashboard", 18,
                      &app->small_layout));
    static const char *button_names[4] = {"Pause", "Run", "Step", "Reset"};
    for (size_t i = 0; i < 4; ++i)
        CHECK(make_layout(app, button_names[i], 17, &app->buttons[i]));
    static const char *tree_names[3] = {"Scene", "Oscillator", "Signal"};
    for (size_t i = 0; i < 3; ++i)
        CHECK(make_layout(app, tree_names[i], 16, &app->tree_labels[i]));
    return 1;
}

static void append_point(showcase *app) {
    size_t slot = (app->head + app->count) % CAPACITY;
    if (app->count == CAPACITY) {
        slot = app->head;
        app->head = (app->head + 1u) % CAPACITY;
    } else {
        ++app->count;
    }
    app->points[slot] = (nx_series_point){(double)app->next_x++, app->position};
}

static void reset_model(showcase *app) {
    app->position = 1.0;
    app->velocity = 0.0;
    app->steps = 0;
    app->head = app->count = 0;
    app->next_x = 0;
    append_point(app);
    append_point(app);
}

/* Illustrative unit-mass spring with damping, semi-implicit Euler, dt=0.04 s. */
static void advance_model(showcase *app) {
    const double dt = 0.04;
    app->velocity += (-app->gain * app->position - 0.08 * app->velocity) * dt;
    app->position += app->velocity * dt;
    ++app->steps;
    append_point(app);
}

static nx_result sample(void *user, size_t index, nx_series_point *out) {
    const showcase *app = (const showcase *)user;
    if (!out || index >= app->count) return NX_ERROR_ARGUMENT;
    *out = app->points[(app->head + index) % CAPACITY];
    return NX_OK;
}

static int update_status(showcase *app, float width) {
    char text[160];
    int length;
    if (width < 650.0f)
        length = snprintf(text, sizeof(text), "%s   Step %llu   x %+.2f m",
                          app->running ? "Running" : "Paused",
                          (unsigned long long)app->steps, app->position);
    else
        length = snprintf(text, sizeof(text),
                          "%s   Step %llu   Position %+.3f m   Velocity %+.3f m/s",
                          app->running ? "Running" : "Paused",
                          (unsigned long long)app->steps,
                          app->position, app->velocity);
    CHECK(length > 0 && (size_t)length < sizeof(text));
    nx_text_layout *next = NULL;
    CHECK(make_layout(app, text, 15, &next));
    nx_text_layout_destroy(app->status_layout);
    app->status_layout = next;
    return 1;
}

static int button(showcase *app, uint64_t id, uint64_t parent,
                  nx_rect bounds, const char *name, nx_text_layout *label,
                  int *clicked) {
    nx_control_desc desc = {0};
    desc.id = id;
    desc.parent_id = parent;
    desc.bounds = bounds;
    desc.name = span(name);
    desc.label_layout = label;
    desc.label_origin = (nx_point){bounds.x + 24, bounds.y + 10};
    desc.glyph_cache_bytes = GLYPH_CACHE;
    nx_interaction_state state;
    CHECK(nx_button(app->ui, &desc, &state) == NX_OK);
    *clicked = state.clicked != 0;
    return 1;
}

static int draw_tree(showcase *app, nx_rect bounds) {
    nx_container_desc tree = {0};
    tree.id = 30;
    tree.parent_id = 1;
    tree.bounds = bounds;
    tree.name = span("Scene hierarchy");
    CHECK(nx_tree(app->ui, &tree) == NX_OK);
    nx_tree_node nodes[3] = {
        {SIZE_MAX, app->expanded}, {0, 0}, {0, 0}
    };
    size_t visible[3], count = 0;
    CHECK(nx_tree_flatten(nodes, 3, NULL, visible, 3, &count) == NX_OK);
    static const char *names[3] = {"Scene", "Oscillator", "Signal"};
    for (size_t row = 0; row < count; ++row) {
        size_t item_index = visible[row];
        nx_tree_item_desc item = {0};
        item.control.id = 101u + item_index;
        item.control.parent_id = 30;
        item.control.bounds = (nx_rect){bounds.x + 8,
            bounds.y + 34 + (float)row * 42, bounds.width - 16, 36};
        item.control.name = span(names[item_index]);
        item.control.label_layout = app->tree_labels[item_index];
        item.control.label_origin = (nx_point){bounds.x +
            (item_index ? 48.0f : 42.0f), bounds.y + 42 + (float)row * 42};
        item.control.glyph_cache_bytes = GLYPH_CACHE;
        item.has_children = item_index == 0;
        if (item.has_children) {
            item.expander_id = 201;
            item.expander_bounds = (nx_rect){bounds.x + 10, bounds.y + 41, 22, 22};
        }
        nx_interaction_state state;
        CHECK(nx_tree_item(app->ui, &item, &app->expanded,
                           &app->selected_id, &state) == NX_OK);
    }
    return 1;
}

static int draw_inspector(showcase *app, nx_rect bounds) {
    nx_container_desc group = {0};
    group.id = 40;
    group.parent_id = 1;
    group.bounds = bounds;
    group.name = span("Inspector");
    group.label_layout = app->inspector_title;
    group.label_origin = (nx_point){bounds.x + 16, bounds.y + 17};
    group.glyph_cache_bytes = GLYPH_CACHE;
    CHECK(nx_group(app->ui, &group) == NX_OK);
    nx_property_row_desc property = {0};
    property.parent_id = 40;
    property.bounds = (nx_rect){bounds.x + 12, bounds.y + 62,
                                 bounds.width - 24, 38};
    property.fonts = app->fonts;
    property.pixels_per_em = 14;
    property.glyph_cache_bytes = GLYPH_CACHE;
    char samples[32];
    if (app->selected_id == 103) {
        int length = snprintf(samples, sizeof(samples), "%zu", app->count);
        CHECK(length > 0 && (size_t)length < sizeof(samples));
        property.id = 47;
        property.name = span("Samples");
        property.kind = NX_PROPERTY_TEXT;
        property.text_value = (nx_utf8_span){samples, (size_t)length};
        property.read_only = 1;
    } else if (app->selected_id == 101) {
        property.id = 48;
        property.name = span("Objects");
        property.kind = NX_PROPERTY_TEXT;
        property.text_value = span("2");
        property.read_only = 1;
    } else {
        property.id = 41;
        property.name = span("Spring gain");
        property.kind = NX_PROPERTY_NUMBER;
        property.number_value = app->gain;
        property.minimum = 0.2;
        property.maximum = 4.0;
        property.step = 0.1;
    }
    nx_property_action action;
    CHECK(nx_property_row(app->ui, &property, &action) == NX_OK);
    if (action.changed) app->gain = action.number_value;

    nx_telemetry_metric metrics[4] = {0};
    metrics[0].id = 43;
    metrics[0].name = span("Position");
    metrics[0].unit = span("m");
    metrics[0].value = app->position;
    metrics[0].minimum = -2;
    metrics[0].maximum = 2;
    metrics[1].id = 44;
    metrics[1].name = span("Velocity");
    metrics[1].unit = span("m/s");
    metrics[1].value = app->velocity;
    metrics[1].minimum = -4;
    metrics[1].maximum = 4;
    metrics[2].id = 45;
    metrics[2].name = span("Energy");
    metrics[2].unit = span("J");
    metrics[2].value = 0.5 * (app->velocity * app->velocity +
                              app->gain * app->position * app->position);
    metrics[2].minimum = 0;
    metrics[2].maximum = 3;
    metrics[3].id = 46;
    metrics[3].name = span("Steps");
    metrics[3].value = (double)app->steps;
    metrics[3].minimum = 0;
    metrics[3].maximum = 1000;
    nx_telemetry_panel_desc telemetry = {0};
    telemetry.id = 42;
    telemetry.parent_id = 40;
    telemetry.bounds = (nx_rect){bounds.x + 12, bounds.y + 132,
                                  bounds.width - 24, 240};
    telemetry.name = span("Live telemetry");
    telemetry.metrics = metrics;
    telemetry.metric_count = 4;
    telemetry.fonts = app->fonts;
    telemetry.pixels_per_em = 14;
    telemetry.glyph_cache_bytes = GLYPH_CACHE;
    CHECK(nx_telemetry_panel(app->ui, &telemetry) == NX_OK);
    return 1;
}

static int draw_frame(showcase *app, nx_window_info info,
                      const char *preview_path) {
    size_t routed;
    CHECK(nx_window_route_events(app->window, app->ui, &routed) == NX_OK);
    CHECK(nx_begin_frame(app->ui, info.width, info.height, 1.0 / 60.0) == NX_OK);
    nx_cursor_shape cursor = NX_CURSOR_DEFAULT;
    nx_color background, accent;
    CHECK(nx_theme_color(app->ui, NX_COLOR_BACKGROUND, &background) == NX_OK);
    CHECK(nx_theme_color(app->ui, NX_COLOR_ACCENT, &accent) == NX_OK);
    nx_semantic_desc root = {0};
    root.id = 1;
    root.role = NX_ROLE_WINDOW;
    root.bounds = (nx_rect){0, 0, info.width, info.height};
    root.name = span("NexaUI Studio simulation dashboard");
    CHECK(nx_semantics_add(app->ui, &root, NULL) == NX_OK);
    int full = info.width >= 1000 && info.height >= 570;
    int usable = info.width >= 480 && info.height >= 420;
    if (usable) {
        nx_container_desc toolbar = {0};
        toolbar.id = 10;
        toolbar.parent_id = 1;
        toolbar.bounds = (nx_rect){20, 20, info.width - 40, 64};
        toolbar.name = span("Simulation controls");
        CHECK(nx_toolbar(app->ui, &toolbar) == NX_OK);
        if (info.width >= 760)
            CHECK(nx_draw_text_layout(app->ui, app->title,
                      (nx_point){38, 35}, accent, GLYPH_CACHE) == NX_OK);
        float bx = info.width >= 760 ? 388.0f : 38.0f;
        int pause = 0, step = 0, reset = 0;
        CHECK(button(app, 11, 10, (nx_rect){bx, 32, 102, 40},
                     app->running ? "Pause" : "Run",
                     app->buttons[app->running ? 0 : 1], &pause));
        CHECK(button(app, 12, 10, (nx_rect){bx + 116, 32, 102, 40},
                     "Step", app->buttons[2], &step));
        CHECK(button(app, 13, 10, (nx_rect){bx + 232, 32, 102, 40},
                     "Reset", app->buttons[3], &reset));
        if (pause) app->running = !app->running;
        if (step) app->running = 0;
        if (reset) reset_model(app);
        else if (step) advance_model(app);
        else if (app->running)
            for (unsigned i = 0; i < app->pending_steps; ++i)
                advance_model(app);
        if (!app->running || reset) app->accumulated_seconds = 0;

        float work_y = 102.0f;
        float work_h = info.height - 172.0f;
        nx_rect plot_panel = {full ? 230.0f : 20.0f, work_y,
                              full ? info.width - 520.0f : info.width - 40.0f,
                              work_h};
        if (full) {
            CHECK(draw_tree(app, (nx_rect){20, work_y, 190, work_h}));
            CHECK(draw_inspector(app, (nx_rect){info.width - 270, work_y,
                                                250, work_h}));
        }
        nx_container_desc panel = {0};
        panel.id = 20;
        panel.parent_id = 1;
        panel.bounds = plot_panel;
        panel.name = span("Signal view");
        panel.label_layout = app->plot_title;
        panel.label_origin = (nx_point){plot_panel.x + 20, plot_panel.y + 17};
        panel.glyph_cache_bytes = GLYPH_CACHE;
        CHECK(nx_panel(app->ui, &panel) == NX_OK);
        nx_plot_desc plot = {0};
        plot.id = 21;
        plot.parent_id = 20;
        plot.bounds = (nx_rect){plot_panel.x + 18, plot_panel.y + 64,
                                plot_panel.width - 36, plot_panel.height - 88};
        if (plot.bounds.width > 16000) plot.bounds.width = 16000;
        plot.name = span("Oscillator position over time");
        plot.series = (nx_series_view){app->count, app, sample};
        plot.x_min = app->next_x > 180 ? (double)(app->next_x - 180) : 0.0;
        plot.x_max = (double)app->next_x;
        plot.y_min = -1.3;
        plot.y_max = 1.3;
        plot.scratch = app->scratch;
        plot.scratch_capacity = SCRATCH_CAPACITY;
        plot.fonts = app->fonts;
        plot.pixels_per_em = 12;
        plot.glyph_cache_bytes = GLYPH_CACHE;
        CHECK(nx_plot(app->ui, &plot, &app->plot_state) == NX_OK);
        nx_input_snapshot input;
        CHECK(nx_frame_input(app->ui, &input) == NX_OK);
        if (input.pointer_x >= plot.bounds.x &&
            input.pointer_x < plot.bounds.x + plot.bounds.width &&
            input.pointer_y >= plot.bounds.y &&
            input.pointer_y < plot.bounds.y + plot.bounds.height)
            cursor = NX_CURSOR_CROSSHAIR;

        CHECK(update_status(app, info.width));
        nx_container_desc status = {0};
        status.id = 50;
        status.parent_id = 1;
        status.bounds = (nx_rect){20, info.height - 52, info.width - 40, 32};
        status.name = span("Simulation status");
        status.label_layout = app->status_layout;
        status.label_origin = (nx_point){status.bounds.x + 12,
                                         status.bounds.y + 6};
        status.glyph_cache_bytes = GLYPH_CACHE;
        CHECK(nx_status_bar(app->ui, &status) == NX_OK);
    } else {
        nx_color text_color;
        CHECK(nx_theme_color(app->ui, NX_COLOR_TEXT, &text_color) == NX_OK);
        CHECK(nx_draw_text_layout(app->ui, app->small_layout,
              (nx_point){20, 20}, text_color, GLYPH_CACHE) == NX_OK);
        nx_semantic_desc message = {0};
        message.id = 60;
        message.parent_id = 1;
        message.role = NX_ROLE_STATUS;
        message.bounds = (nx_rect){20, 20, info.width > 20 ? info.width - 20 : 1,
                                    info.height > 40 ? 24 : info.height};
        message.name = span("Window size");
        message.value = span("Resize window to see the dashboard");
        CHECK(nx_semantics_add(app->ui, &message, NULL) == NX_OK);
    }
    CHECK(nx_end_frame(app->ui) == NX_OK);
    CHECK(nx_window_set_cursor(app->window, cursor) == NX_OK);
    if (usable) {
        uint32_t index;
        nx_semantic_view view;
        CHECK(nx_semantics_find(app->ui, 21, &index) == NX_OK &&
              nx_semantics_get(app->ui, index, &view) == NX_OK &&
              view.parent_id == 20 && view.role == NX_ROLE_CUSTOM);
        CHECK(nx_semantics_find(app->ui, 50, &index) == NX_OK &&
              nx_semantics_get(app->ui, index, &view) == NX_OK &&
              view.role == NX_ROLE_STATUS);
    }
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
    if (preview_path) {
        unsigned image_width = (unsigned)info.width;
        unsigned image_height = (unsigned)info.height;
        CHECK(image_width > 0 && image_height > 0 &&
              image_width <= 16384u && image_height <= 16384u &&
              (size_t)image_width * image_height <= SIZE_MAX / 4u);
        uint8_t *pixels = (uint8_t *)malloc((size_t)image_width *
                                             image_height * 4u);
        CHECK(pixels != NULL);
        int okay = nx_headless_render(commands, command_count, pixels,
                                      image_width, image_height,
                                      image_width * 4u,
                                      background) == NX_OK;
        FILE *file = okay ? fopen(preview_path, "wb") : NULL;
        if (!file) okay = 0;
        if (okay) okay = fprintf(file, "P6\n%u %u\n255\n",
                                 image_width, image_height) > 0;
        for (size_t y = 0; okay && y < image_height; ++y)
            for (size_t x = 0; okay && x < image_width; ++x)
                okay = fwrite(pixels + (y * image_width + x) * 4u,
                              1, 3, file) == 3;
        if (file && fclose(file) != 0) okay = 0;
        free(pixels);
        CHECK(okay);
    }
    ++app->frames;
    (void)routed;
    return 1;
}

static int tick(showcase *app, int smoke, const char *preview_path) {
    CHECK(nx_window_poll(app->window, smoke ? 0u : 16u) == NX_OK);
    uint64_t now = GetTickCount64();
    uint64_t elapsed = app->last_tick_ms ? now - app->last_tick_ms : 0;
    app->last_tick_ms = now;
    if (elapsed > 250u) elapsed = 250u;
    if (smoke) {
        app->pending_steps = 1;
    } else {
        app->accumulated_seconds += (double)elapsed / 1000.0;
        app->pending_steps = (unsigned)(app->accumulated_seconds / 0.04);
        if (app->pending_steps > 6u) app->pending_steps = 6u;
        app->accumulated_seconds -= 0.04 * app->pending_steps;
    }
    nx_window_info info;
    CHECK(nx_window_get_info(app->window, &info) == NX_OK);
    if (info.should_close) return 1;
    if (info.width > 0 && info.height > 0)
        CHECK(draw_frame(app, info, preview_path));
    return 1;
}

static int post_click(HWND hwnd, float scale, int x, int y) {
    LPARAM point = MAKELPARAM((int)(x * scale), (int)(y * scale));
    return PostMessageW(hwnd, WM_MOUSEMOVE, 0, point) &&
           PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, point) &&
           PostMessageW(hwnd, WM_LBUTTONUP, 0, point);
}

static int smoke_run(showcase *app, const char *preview_path,
                     const char *detail_path, const char *compact_path) {
    HWND hwnd = nx_window_native_hwnd(app->window);
    nx_window_info info;
    CHECK(hwnd && nx_window_get_info(app->window, &info) == NX_OK);
    CHECK(tick(app, 1, preview_path));
    uint64_t initial = app->steps;
    CHECK(initial > 100 && app->running);
    CHECK(post_click(hwnd, info.scale, 435, 52) && tick(app, 1, NULL));
    CHECK(!app->running && app->steps == initial);
    CHECK(post_click(hwnd, info.scale, 550, 52) && tick(app, 1, NULL));
    CHECK(!app->running && app->steps == initial + 1u);
    CHECK(nx_focus_set(app->ui, 13) == NX_OK &&
          PostMessageW(hwnd, WM_KEYDOWN, VK_SPACE, 0) &&
          PostMessageW(hwnd, WM_KEYUP, VK_SPACE, 0) &&
          tick(app, 1, NULL));
    CHECK(!app->running && app->steps == 0 && app->count == 2);
    CHECK(nx_focus_set(app->ui, 41) == NX_OK &&
          PostMessageW(hwnd, WM_KEYDOWN, VK_RIGHT, 0) &&
          tick(app, 1, NULL));
    CHECK(app->gain > 1.0 && app->gain < 1.2);
    CHECK(post_click(hwnd, info.scale, 100, 244) && tick(app, 1, detail_path));
    CHECK(app->selected_id == 103);
    uint32_t index;
    nx_semantic_view view;
    CHECK(nx_semantics_find(app->ui, 103, &index) == NX_OK &&
          nx_semantics_get(app->ui, index, &view) == NX_OK &&
          (view.state & NX_SEM_SELECTED));
    CHECK(nx_semantics_find(app->ui, 47, &index) == NX_OK &&
          nx_semantics_get(app->ui, index, &view) == NX_OK &&
          view.role == NX_ROLE_CUSTOM &&
          view.name.length == 7 && memcmp(view.name.data, "Samples", 7) == 0);
    HCURSOR prior_cursor = GetCursor();
    CHECK(PostMessageW(hwnd, WM_MOUSEMOVE, 0,
                       MAKELPARAM((int)(350 * info.scale),
                                  (int)(250 * info.scale))) &&
          tick(app, 1, NULL));
    CHECK(SendMessageW(hwnd, WM_SETCURSOR, (WPARAM)hwnd,
                       MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)) == TRUE &&
          GetCursor() == (HCURSOR)LoadImageW(NULL, IDC_CROSS, IMAGE_CURSOR,
              0, 0, LR_DEFAULTSIZE | LR_SHARED));
    CHECK(PostMessageW(hwnd, WM_MOUSEMOVE, 0,
                       MAKELPARAM((int)(40 * info.scale),
                                  (int)(40 * info.scale))) &&
          tick(app, 1, NULL));
    CHECK(SendMessageW(hwnd, WM_SETCURSOR, (WPARAM)hwnd,
                       MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)) == TRUE &&
          GetCursor() == (HCURSOR)LoadImageW(NULL, IDC_ARROW, IMAGE_CURSOR,
              0, 0, LR_DEFAULTSIZE | LR_SHARED));
    SetCursor(prior_cursor);
    CHECK(SetWindowPos(hwnd, NULL, 0, 0, 800, 560,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    CHECK(tick(app, 1, compact_path));
    CHECK(nx_window_get_info(app->window, &info) == NX_OK &&
          info.width >= 480 && info.width < 1000 && info.height >= 420);
    CHECK(nx_semantics_find(app->ui, 21, &index) == NX_OK &&
          nx_semantics_find(app->ui, 30, &index) != NX_OK &&
          nx_semantics_find(app->ui, 40, &index) != NX_OK);
    return 1;
}

static int run(const char *font_path, int smoke, const char *preview_path,
               const char *detail_path, const char *compact_path) {
    showcase app = {0};
    app.scratch = (nx_series_envelope *)calloc(SCRATCH_CAPACITY,
                                                sizeof(*app.scratch));
    app.gain = 1.0;
    app.running = 1;
    app.expanded = 1;
    app.selected_id = 102;
    reset_model(&app);
    for (unsigned i = 0; i < 160; ++i) advance_model(&app);
    nx_window_desc desc = {0};
    desc.title_utf8 = "NexaUI Studio";
    desc.width = WIDTH;
    desc.height = HEIGHT;
    desc.flags = smoke ? NX_WINDOW_HIDDEN : 0;
    int okay = app.scratch &&
               nx_window_create(&desc, &app.window) == NX_OK &&
               nx_context_create(NULL, &app.ui) == NX_OK &&
               nx_d3d12_driver_open(app.window, &app.renderer) == NX_OK &&
               load_font(&app, font_path);
    if (okay && smoke)
        okay = smoke_run(&app, preview_path, detail_path, compact_path);
    while (okay && !smoke) {
        nx_window_info info;
        if (nx_window_get_info(app.window, &info) != NX_OK) {
            okay = 0;
            break;
        }
        if (info.should_close) break;
        okay = tick(&app, 0, NULL);
    }
    close_showcase(&app);
    return okay;
}

int main(int argc, char **argv) {
    if (argc == 2) return run(argv[1], 0, NULL, NULL, NULL) ? 0 : 1;
    if (argc == 6 && strcmp(argv[1], "--smoke") == 0)
        return run(argv[2], 1, argv[3], argv[4], argv[5]) ? 0 : 1;
    fputs("usage: nexaui_showcase FONT_FILE | "
          "--smoke FONT_FILE WIDE.ppm DETAIL.ppm COMPACT.ppm\n",
          stderr);
    return 2;
}
