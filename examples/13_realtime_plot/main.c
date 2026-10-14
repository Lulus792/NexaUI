#include "nexaui/nexaui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 640u
#define HEIGHT 320u
#define CAPACITY 2048u

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 0; \
} } while (0)

typedef struct series_ring {
    nx_series_point points[CAPACITY];
    size_t head, count;
    uint64_t next_x;
} series_ring;

typedef struct demo {
    nx_context *ui;
    series_ring series;
    nx_series_envelope *scratch;
    uint8_t *pixels;
    nx_plot_state plot_state;
} demo;

static void append_point(series_ring *ring) {
    uint64_t x = ring->next_x++;
    unsigned phase = (unsigned)(x % 80u);
    double y = phase < 40u ? (double)phase / 8.0 : (double)(80u - phase) / 8.0;
    if (x % 73u == 0u) y += 5.0;
    size_t slot = (ring->head + ring->count) % CAPACITY;
    if (ring->count == CAPACITY) {
        slot = ring->head;
        ring->head = (ring->head + 1u) % CAPACITY;
    } else {
        ++ring->count;
    }
    ring->points[slot] = (nx_series_point){(double)x, y};
}

static nx_result sample(void *user, size_t index, nx_series_point *out) {
    const series_ring *ring = (const series_ring *)user;
    if (index >= ring->count || !out) return NX_ERROR_ARGUMENT;
    *out = ring->points[(ring->head + index) % CAPACITY];
    return NX_OK;
}

static int draw_frame(demo *app) {
    nx_series_point first, last;
    CHECK(sample(&app->series, 0, &first) == NX_OK);
    CHECK(sample(&app->series, app->series.count - 1u, &last) == NX_OK);
    CHECK(first.x == (double)(app->series.next_x - app->series.count));
    CHECK(last.x == (double)(app->series.next_x - 1u));

    nx_plot_desc plot = {0};
    plot.id = 1;
    plot.bounds = (nx_rect){24, 24, 592, 272};
    plot.name = (nx_utf8_span){"Live signal", 11};
    plot.series = (nx_series_view){app->series.count, &app->series, sample};
    plot.x_min = app->series.next_x > 300u ?
                 (double)(app->series.next_x - 300u) : 0.0;
    plot.x_max = (double)app->series.next_x;
    plot.y_min = -1;
    plot.y_max = 12;
    plot.scratch = app->scratch;
    plot.scratch_capacity = WIDTH;
    CHECK(nx_begin_frame(app->ui, WIDTH, HEIGHT, 1.0 / 60.0) == NX_OK);
    CHECK(nx_plot(app->ui, &plot, &app->plot_state) == NX_OK);
    CHECK(nx_end_frame(app->ui) == NX_OK);
    size_t count = 0;
    CHECK(nx_semantics_count(app->ui, &count) == NX_OK && count == 1);
    return 1;
}

static int save_image(demo *app, const char *prefix, const char *suffix,
                      uint64_t *out_hash) {
    char path[512];
    int length = snprintf(path, sizeof(path), "%s-%s.ppm", prefix, suffix);
    CHECK(length > 0 && (size_t)length < sizeof(path));
    const nx_command *commands = NULL;
    size_t count = 0;
    CHECK(nx_frame_commands(app->ui, &commands, &count) == NX_OK && count > 0);
    CHECK(nx_headless_render(commands, count, app->pixels, WIDTH, HEIGHT,
                             WIDTH * 4u, (nx_color){0.06f, 0.08f, 0.12f, 1}) == NX_OK);
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < (size_t)WIDTH * HEIGHT * 4u; ++i) {
        hash ^= app->pixels[i];
        hash *= UINT64_C(1099511628211);
    }
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    int okay = fprintf(file, "P6\n%u %u\n255\n", WIDTH, HEIGHT) > 0;
    uint8_t row[WIDTH * 3u];
    for (size_t y = 0; okay && y < HEIGHT; ++y) {
        for (size_t x = 0; x < WIDTH; ++x) {
            const uint8_t *pixel = app->pixels + (y * WIDTH + x) * 4u;
            row[x * 3u] = pixel[0];
            row[x * 3u + 1u] = pixel[1];
            row[x * 3u + 2u] = pixel[2];
        }
        okay = fwrite(row, 1, sizeof(row), file) == sizeof(row);
    }
    if (fclose(file) != 0) okay = 0;
    CHECK(okay);
    *out_hash = hash;
    return 1;
}

static int queue_input(nx_context *ui, nx_event_type type,
                       nx_key key, float x, float y) {
    nx_event event = {0};
    event.type = type;
    event.key = key;
    event.x = x;
    event.y = y;
    CHECK(nx_context_queue_event(ui, &event) == NX_OK);
    return 1;
}

static int run(const char *prefix) {
    demo app = {0};
    app.scratch = (nx_series_envelope *)malloc(WIDTH * sizeof(*app.scratch));
    app.pixels = (uint8_t *)malloc((size_t)WIDTH * HEIGHT * 4u);
    int okay = app.scratch && app.pixels &&
               nx_context_create(NULL, &app.ui) == NX_OK;
    uint64_t early_hash = 0, late_hash = 0, cursor_hash = 0;
    for (unsigned frame = 0; okay && frame < 40u; ++frame) {
        for (unsigned sample_index = 0; sample_index < 60u; ++sample_index)
            append_point(&app.series);
        okay = draw_frame(&app);
        if (okay && frame == 4u)
            okay = save_image(&app, prefix, "early", &early_hash);
        if (okay && frame == 39u)
            okay = save_image(&app, prefix, "late", &late_hash);
    }
    if (okay) okay = app.series.count == CAPACITY &&
                     app.series.head == 2400u - CAPACITY &&
                     early_hash != late_hash;
    if (okay) okay = queue_input(app.ui, NX_EVENT_POINTER_MOVE,
                                NX_KEY_UNKNOWN, 320, 150) && draw_frame(&app) &&
                     app.plot_state.cursor_valid;
    double pointer_x = app.plot_state.cursor_x;
    if (okay) okay = nx_focus_set(app.ui, 1) == NX_OK &&
                     queue_input(app.ui, NX_EVENT_KEY_DOWN, NX_KEY_RIGHT, 0, 0) &&
                     draw_frame(&app) && app.plot_state.cursor_x > pointer_x &&
                     app.plot_state.interaction.focused;
    if (okay) {
        uint32_t index = UINT32_MAX;
        nx_semantic_view view;
        okay = nx_semantics_find(app.ui, 1, &index) == NX_OK &&
               nx_semantics_get(app.ui, index, &view) == NX_OK &&
               view.role == NX_ROLE_CUSTOM && (view.state & NX_SEM_FOCUSED) &&
               view.value.length >= 2 && memcmp(view.value.data, "x=", 2) == 0 &&
               save_image(&app, prefix, "cursor", &cursor_hash) &&
               cursor_hash != late_hash;
    }
    if (okay)
        printf("%zu samples retained, viewport advanced, cursor x=%.2f\n",
               app.series.count, app.plot_state.cursor_x);
    nx_context_destroy(app.ui);
    free(app.pixels);
    free(app.scratch);
    return okay;
}

int main(int argc, char **argv) {
    if (argc != 2 || !argv[1][0]) {
        fputs("usage: nexaui_realtime_plot OUTPUT_PREFIX\n", stderr);
        return 2;
    }
    if (!run(argv[1])) {
        fputs("realtime plot example failed\n", stderr);
        return 1;
    }
    return 0;
}
