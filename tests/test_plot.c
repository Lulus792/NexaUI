#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 220u
#define HEIGHT 140u
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; \
} } while (0)

static nx_result sample(void *user, size_t index, nx_series_point *out) {
    *out = ((const nx_series_point *)user)[index];
    return NX_OK;
}

static int baseline(const char *name, const uint8_t *rgba, int update) {
    char path[512], header[64];
    int path_length = snprintf(path, sizeof(path), "%s/%s.ppm",
                               NEXAUI_PLOT_BASELINE_DIR, name);
    int header_length = snprintf(header, sizeof(header), "P6\n%u %u\n255\n",
                                 WIDTH, HEIGHT);
    if (path_length <= 0 || (size_t)path_length >= sizeof(path) ||
        header_length <= 0 || (size_t)header_length >= sizeof(header)) return 0;
    FILE *file = fopen(path, update ? "wb" : "rb");
    if (!file) return 0;
    int okay = 1;
    if (update) {
        okay = fwrite(header, 1, (size_t)header_length, file) ==
               (size_t)header_length;
        for (size_t i = 0; okay && i < (size_t)WIDTH * HEIGHT; ++i)
            okay = fwrite(rgba + 4u * i, 1, 3, file) == 3;
    } else {
        char actual[64];
        okay = fread(actual, 1, (size_t)header_length, file) ==
               (size_t)header_length &&
               memcmp(actual, header, (size_t)header_length) == 0;
        for (size_t i = 0; okay && i < (size_t)WIDTH * HEIGHT; ++i) {
            uint8_t rgb[3];
            if (fread(rgb, 1, 3, file) != 3 ||
                memcmp(rgb, rgba + 4u * i, 3) != 0 || rgba[4u * i + 3u] != 255) {
                fprintf(stderr, "%s differs at (%u,%u)\n", name,
                        (unsigned)(i % WIDTH), (unsigned)(i / WIDTH));
                okay = 0;
            }
        }
        if (okay && fgetc(file) != EOF) okay = 0;
    }
    if (fclose(file) != 0) okay = 0;
    return okay;
}

static int render(nx_context *ctx, uint8_t *pixels) {
    const nx_command *commands = NULL;
    size_t count = 0;
    if (nx_frame_commands(ctx, &commands, &count) != NX_OK) return 0;
    return nx_headless_render(commands, count, pixels, WIDTH, HEIGHT,
                             WIDTH * 4u, (nx_color){0, 0, 0, 1}) == NX_OK;
}

static int queue_pointer(nx_context *ctx, float x, float y) {
    nx_event event = {0};
    event.type = NX_EVENT_POINTER_MOVE;
    event.x = x; event.y = y;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int queue_key(nx_context *ctx, nx_key key) {
    nx_event event = {0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = key;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int run_theme(nx_theme_kind kind, const char *name, int update) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_theme theme;
    CHECK(nx_theme_preset(kind, &theme) == NX_OK);
    CHECK(nx_context_set_theme(ctx, &theme) == NX_OK);
    nx_series_point points[] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 1}, {4, 9}, {5, 2},
        {6, -2}, {7, 5}, {8, 4}, {9, 7}, {10, 1}
    };
    nx_series_envelope scratch[220];
    nx_plot_desc desc = {0};
    desc.id = 1;
    desc.bounds = (nx_rect){8, 8, 204, 124};
    desc.name = (nx_utf8_span){"Telemetry", 9};
    desc.series = (nx_series_view){11, points, sample};
    desc.x_min = 0; desc.x_max = 10;
    desc.y_min = -2; desc.y_max = 10;
    desc.scratch = scratch;
    desc.scratch_capacity = 220;
    nx_plot_state state = {0};
    uint8_t pixels[WIDTH * HEIGHT * 4u];
    CHECK(nx_begin_frame(ctx, WIDTH, HEIGHT, 0) == NX_OK);
    nx_plot_desc invalid = desc;
    invalid.scratch_capacity = 1;
    CHECK(nx_plot(ctx, &invalid, &state) == NX_ERROR_LIMIT &&
          !state.cursor_valid);
    CHECK(nx_plot(ctx, &desc, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(render(ctx, pixels));
    CHECK(baseline(name, pixels, update));
    uint8_t initial[sizeof(pixels)];
    memcpy(initial, pixels, sizeof(initial));

    CHECK(queue_pointer(ctx, 110, 70));
    CHECK(nx_begin_frame(ctx, WIDTH, HEIGHT, 0) == NX_OK);
    CHECK(nx_plot(ctx, &desc, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(state.cursor_valid && state.cursor_x > 4 && state.cursor_x < 6);
    uint32_t index = UINT32_MAX;
    nx_semantic_view semantic;
    CHECK(nx_semantics_find(ctx, 1, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &semantic) == NX_OK &&
          semantic.role == NX_ROLE_CUSTOM && semantic.value.length >= 3 &&
          memcmp(semantic.value.data, "x=", 2) == 0);
    CHECK(render(ctx, pixels) && memcmp(initial, pixels, sizeof(pixels)) != 0);

    double pointer_x = state.cursor_x;
    CHECK(nx_focus_set(ctx, 1) == NX_OK);
    CHECK(queue_key(ctx, NX_KEY_RIGHT));
    CHECK(nx_begin_frame(ctx, WIDTH, HEIGHT, 0) == NX_OK);
    CHECK(nx_plot(ctx, &desc, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(state.interaction.focused && state.cursor_x > pointer_x);
    CHECK(nx_semantics_find(ctx, 1, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &semantic) == NX_OK &&
          (semantic.state & NX_SEM_FOCUSED));

    desc.disabled = 1;
    pointer_x = state.cursor_x;
    CHECK(queue_pointer(ctx, 175, 70));
    CHECK(nx_begin_frame(ctx, WIDTH, HEIGHT, 0) == NX_OK);
    CHECK(nx_plot(ctx, &desc, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(state.cursor_x == pointer_x);
    CHECK(nx_semantics_find(ctx, 1, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &semantic) == NX_OK &&
          (semantic.state & NX_SEM_DISABLED));
    nx_context_destroy(ctx);
    return 0;
}

typedef struct allocation_probe {
    size_t calls, frees, fail_on;
} allocation_probe;

static void *probe_allocate(void *user, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    ++probe->calls;
    return probe->calls == probe->fail_on ? NULL : malloc(size);
}

static void probe_deallocate(void *user, void *data, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    (void)size;
    ++probe->frees;
    free(data);
}

static int test_allocation_rollback(void) {
    const nx_series_point points[] = {{0, 0}, {1, 1}, {2, 0}};
    unsigned failures = 0;
    for (size_t offset = 1; offset <= 12; ++offset) {
        allocation_probe probe = {0};
        nx_context_desc context_desc = {0};
        context_desc.allocator = (nx_allocator){&probe, probe_allocate,
                                                 probe_deallocate};
        context_desc.initial_command_capacity = 1;
        nx_context *ctx = NULL;
        CHECK(nx_context_create(&context_desc, &ctx) == NX_OK);
        nx_series_envelope scratch[64];
        nx_plot_desc plot = {0};
        plot.id = 1;
        plot.bounds = (nx_rect){4, 4, 60, 40};
        plot.name = (nx_utf8_span){"Plot", 4};
        plot.series = (nx_series_view){3, (void *)points, sample};
        plot.x_max = 2;
        plot.y_max = 1;
        plot.scratch = scratch;
        plot.scratch_capacity = 64;
        nx_plot_state state = {0};
        CHECK(nx_begin_frame(ctx, 70, 50, 0) == NX_OK);
        probe.fail_on = probe.calls + offset;
        nx_result result = nx_plot(ctx, &plot, &state);
        if (result == NX_ERROR_MEMORY) {
            ++failures;
            CHECK(!state.cursor_valid && !state.interaction.focused);
            probe.fail_on = 0;
            CHECK(nx_plot(ctx, &plot, &state) == NX_OK);
        } else CHECK(result == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        size_t count = 0;
        CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 1);
        nx_context_destroy(ctx);
        CHECK(probe.frees == probe.calls -
                             (result == NX_ERROR_MEMORY ? 1u : 0u));
    }
    CHECK(failures >= 5);
    return 0;
}

static int test_resize_and_empty(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    const nx_series_point points[] = {{0, 0}, {1, 1}, {2, 0}};
    nx_series_envelope scratch[400];
    nx_plot_desc plot = {0};
    plot.id = 10;
    plot.bounds = (nx_rect){2, 2, 16, 16};
    plot.name = (nx_utf8_span){"Small plot", 10};
    plot.series = (nx_series_view){0, (void *)points, sample};
    plot.x_max = 2;
    plot.y_max = 1;
    plot.scratch = scratch;
    plot.scratch_capacity = 400;
    nx_plot_state state = {0};
    CHECK(nx_begin_frame(ctx, 20, 20, 0) == NX_OK);
    CHECK(nx_plot(ctx, &plot, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    plot.bounds = (nx_rect){4, 4, 392, 232};
    plot.series.count = 3;
    plot.y_min = -1;
    CHECK(nx_begin_frame(ctx, 400, 240, 0) == NX_OK);
    nx_plot_desc invalid = plot;
    invalid.y_max = invalid.y_min;
    CHECK(nx_plot(ctx, &invalid, &state) == NX_ERROR_ARGUMENT);
    CHECK(nx_plot(ctx, &plot, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    const nx_command *commands = NULL;
    size_t count = 0;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count > 15);
    nx_context_destroy(ctx);
    return 0;
}

int main(int argc, char **argv) {
    int update = argc == 2 && strcmp(argv[1], "--update-baselines") == 0;
    if (run_theme(NX_THEME_DARK, "plot_dark", update) ||
        run_theme(NX_THEME_LIGHT, "plot_light", update) ||
        run_theme(NX_THEME_HIGH_CONTRAST, "plot_high_contrast", update) ||
        test_allocation_rollback() || test_resize_and_empty())
        return 1;
    return 0;
}
