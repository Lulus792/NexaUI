#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 400u
#define HEIGHT 230u
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; \
} } while (0)
#define SPAN(text) (nx_utf8_span){text, sizeof(text) - 1u}
#define STATIC_SPAN(text) {text, sizeof(text) - 1u}

static int baseline(const char *name, const uint8_t *rgba, int update) {
    char path[512], header[64];
    int path_length = snprintf(path, sizeof(path), "%s/%s.ppm",
                               NEXAUI_TELEMETRY_BASELINE_DIR, name);
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
            okay = fwrite(rgba + i * 4u, 1, 3, file) == 3;
    } else {
        char actual[64];
        okay = fread(actual, 1, (size_t)header_length, file) ==
               (size_t)header_length &&
               memcmp(actual, header, (size_t)header_length) == 0;
        for (size_t i = 0; okay && i < (size_t)WIDTH * HEIGHT; ++i) {
            uint8_t rgb[3];
            if (fread(rgb, 1, 3, file) != 3 ||
                memcmp(rgb, rgba + i * 4u, 3) != 0 ||
                rgba[i * 4u + 3u] != 255) {
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

static nx_telemetry_metric metrics[4] = {
    {11, STATIC_SPAN("Velocity"), STATIC_SPAN("m/s"), {0}, 32, 0, 50, NX_TELEMETRY_NORMAL},
    {12, STATIC_SPAN("Memory"), STATIC_SPAN("MiB"), {0}, 64, 0, 128, NX_TELEMETRY_NORMAL},
    {13, STATIC_SPAN("Temperature"), STATIC_SPAN("C"), STATIC_SPAN("High"), 83, 0, 100,
     NX_TELEMETRY_WARNING},
    {14, STATIC_SPAN("Pressure"), STATIC_SPAN("kPa"), STATIC_SPAN("Critical"), 97, 0, 100,
     NX_TELEMETRY_CRITICAL}
};

static nx_profiler_entry entries[4] = {
    {21, STATIC_SPAN("Frame"), 15.2, 2.0, 1, 0},
    {22, STATIC_SPAN("Physics"), 8.1, 3.4, 1, 1},
    {23, STATIC_SPAN("Collisions"), 4.2, 4.2, 120, 2},
    {24, STATIC_SPAN("Render"), 3.8, 2.9, 1, 1}
};

static int run_scene(nx_theme_kind kind, const char *name, int update) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_theme theme;
    CHECK(nx_theme_preset(kind, &theme) == NX_OK);
    CHECK(nx_context_set_theme(ctx, &theme) == NX_OK);
    nx_telemetry_panel_desc telemetry = {0};
    telemetry.id = 1;
    telemetry.bounds = (nx_rect){8, 8, 188, 214};
    telemetry.name = SPAN("Telemetry");
    telemetry.metrics = metrics;
    telemetry.metric_count = 4;
    telemetry.row_height = 40;
    nx_profiler_panel_desc profiler = {0};
    profiler.id = 2;
    profiler.bounds = (nx_rect){204, 8, 188, 214};
    profiler.name = SPAN("Profiler");
    profiler.entries = entries;
    profiler.entry_count = 4;
    profiler.frame_ms = 16.7;
    profiler.row_height = 40;
    CHECK(nx_begin_frame(ctx, WIDTH, HEIGHT, 0) == NX_OK);
    nx_telemetry_metric saved = metrics[2];
    metrics[2].maximum = 0;
    CHECK(nx_telemetry_panel(ctx, &telemetry) == NX_ERROR_ARGUMENT);
    metrics[2] = saved;
    CHECK(nx_telemetry_panel(ctx, &telemetry) == NX_OK);
    double self_ms = entries[2].self_ms;
    entries[2].self_ms = 10;
    CHECK(nx_profiler_panel(ctx, &profiler) == NX_ERROR_ARGUMENT);
    entries[2].self_ms = self_ms;
    CHECK(nx_profiler_panel(ctx, &profiler) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t semantic_count = 0;
    CHECK(nx_semantics_count(ctx, &semantic_count) == NX_OK &&
          semantic_count == 10);
    uint32_t index = UINT32_MAX;
    nx_semantic_view view;
    CHECK(nx_semantics_find(ctx, 13, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
          view.role == NX_ROLE_PROGRESS && view.parent_id == 1 &&
          view.description.length == 4 &&
          memcmp(view.description.data, "High", 4) == 0 &&
          view.value.length > 0 &&
          strstr(view.value.data, "range 0 to 100") != NULL);
    CHECK(nx_semantics_find(ctx, 23, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
          view.role == NX_ROLE_LIST_ITEM && view.parent_id == 2 &&
          view.value.length > 0 && view.description.length == 7 &&
          memcmp(view.description.data, "level 2", 7) == 0);
    const nx_command *commands = NULL;
    size_t command_count = 0;
    CHECK(nx_frame_commands(ctx, &commands, &command_count) == NX_OK &&
          command_count > 30);
    uint8_t *pixels = malloc((size_t)WIDTH * HEIGHT * 4u);
    CHECK(pixels != NULL);
    CHECK(nx_headless_render(commands, command_count, pixels,
        WIDTH, HEIGHT, WIDTH * 4u, (nx_color){0, 0, 0, 1}) == NX_OK);
    CHECK(baseline(name, pixels, update));
    free(pixels);
    nx_context_destroy(ctx);
    return 0;
}

typedef struct allocation_probe { size_t calls, frees, fail_on; } allocation_probe;
static void *probe_allocate(void *user, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    ++probe->calls;
    if (probe->calls == probe->fail_on) return NULL;
    return malloc(size);
}
static void probe_deallocate(void *user, void *data, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    (void)size;
    ++probe->frees;
    free(data);
}

static int stress_updates(void) {
    const size_t count = 100000u;
    nx_telemetry_metric *many_metrics = calloc(count, sizeof(*many_metrics));
    nx_profiler_entry *many_entries = calloc(count, sizeof(*many_entries));
    CHECK(many_metrics && many_entries);
    for (size_t i = count - 4u; i < count; ++i) {
        many_metrics[i] = (nx_telemetry_metric){100u + i, SPAN("Value"),
            SPAN("u"), {0}, 0, 0, 100, NX_TELEMETRY_NORMAL};
        many_entries[i] = (nx_profiler_entry){200u + i, SPAN("Task"),
            0, 0, 1, (uint32_t)(i - (count - 4u))};
    }
    allocation_probe probe = {0};
    nx_context_desc context_desc = {0};
    context_desc.allocator = (nx_allocator){&probe, probe_allocate,
                                             probe_deallocate};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&context_desc, &ctx) == NX_OK);
    nx_telemetry_panel_desc telemetry = {0};
    telemetry.id = 1;
    telemetry.bounds = (nx_rect){0, 0, 180, 188};
    telemetry.name = SPAN("Telemetry");
    telemetry.metrics = many_metrics;
    telemetry.metric_count = count;
    telemetry.first_row = count - 4u;
    telemetry.row_height = 40;
    nx_profiler_panel_desc profiler = {0};
    profiler.id = 2;
    profiler.bounds = (nx_rect){180, 0, 180, 188};
    profiler.name = SPAN("Profiler");
    profiler.entries = many_entries;
    profiler.entry_count = count;
    profiler.first_row = count - 4u;
    profiler.frame_ms = 16.7;
    profiler.row_height = 40;
    size_t warm_calls = 0;
    for (unsigned frame = 0; frame < 10000u; ++frame) {
        many_metrics[count - 4u].value = (double)(frame % 101u);
        many_entries[count - 4u].inclusive_ms = (double)(frame % 17u);
        many_entries[count - 4u].self_ms = (double)(frame % 17u) * 0.5;
        CHECK(nx_begin_frame(ctx, 360, 188, 0) == NX_OK);
        CHECK(nx_telemetry_panel(ctx, &telemetry) == NX_OK);
        CHECK(nx_profiler_panel(ctx, &profiler) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        if (frame == 0) warm_calls = probe.calls;
        if (frame > 0) CHECK(probe.calls == warm_calls);
        if (frame % 1000u == 0u) {
            size_t nodes = 0;
            CHECK(nx_semantics_count(ctx, &nodes) == NX_OK && nodes == 10);
            uint32_t index = UINT32_MAX;
            nx_semantic_view view;
            CHECK(nx_semantics_find(ctx, 100u + count - 4u, &index) == NX_OK);
            CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
                  view.value.length > 0 &&
                  strtod(view.value.data, NULL) ==
                      (double)(frame % 101u));
        }
    }
    nx_context_destroy(ctx);
    CHECK(probe.calls == probe.frees);
    free(many_metrics);
    free(many_entries);
    return 0;
}

static int test_allocation_rollback(void) {
    unsigned failures = 0;
    for (unsigned panel = 0; panel < 2; ++panel) {
        for (size_t offset = 1; offset <= 12; ++offset) {
            allocation_probe probe = {0};
            nx_context_desc context_desc = {0};
            context_desc.allocator = (nx_allocator){&probe, probe_allocate,
                                                     probe_deallocate};
            context_desc.initial_command_capacity = 1;
            nx_context *ctx = NULL;
            CHECK(nx_context_create(&context_desc, &ctx) == NX_OK);
            nx_telemetry_panel_desc telemetry = {0};
            telemetry.id = 1;
            telemetry.bounds = (nx_rect){0, 0, 180, 120};
            telemetry.name = SPAN("Telemetry");
            telemetry.metrics = metrics;
            telemetry.metric_count = 4;
            telemetry.row_height = 40;
            nx_profiler_panel_desc profiler = {0};
            profiler.id = 2;
            profiler.bounds = (nx_rect){0, 0, 180, 120};
            profiler.name = SPAN("Profiler");
            profiler.entries = entries;
            profiler.entry_count = 4;
            profiler.frame_ms = 16.7;
            profiler.row_height = 40;
            CHECK(nx_begin_frame(ctx, 180, 120, 0) == NX_OK);
            probe.fail_on = probe.calls + offset;
            nx_result result = panel ? nx_profiler_panel(ctx, &profiler) :
                                       nx_telemetry_panel(ctx, &telemetry);
            if (result == NX_ERROR_MEMORY) {
                ++failures;
                probe.fail_on = 0;
                CHECK((panel ? nx_profiler_panel(ctx, &profiler) :
                               nx_telemetry_panel(ctx, &telemetry)) == NX_OK);
            } else CHECK(result == NX_OK);
            CHECK(nx_end_frame(ctx) == NX_OK);
            size_t count = 0;
            CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 3);
            nx_context_destroy(ctx);
            CHECK(probe.frees == probe.calls -
                                 (result == NX_ERROR_MEMORY ? 1u : 0u));
        }
    }
    CHECK(failures >= 8);
    return 0;
}

static int test_empty_panels(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_telemetry_panel_desc telemetry = {0};
    telemetry.id = 1;
    telemetry.bounds = (nx_rect){0, 0, 100, 60};
    telemetry.name = SPAN("Telemetry");
    nx_profiler_panel_desc profiler = {0};
    profiler.id = 2;
    profiler.bounds = (nx_rect){100, 0, 100, 60};
    profiler.name = SPAN("Profiler");
    profiler.frame_ms = 16.7;
    CHECK(nx_begin_frame(ctx, 200, 60, 0) == NX_OK);
    CHECK(nx_telemetry_panel(ctx, &telemetry) == NX_OK);
    CHECK(nx_profiler_panel(ctx, &profiler) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count = 0;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 2);
    nx_semantic_view view;
    CHECK(nx_semantics_get(ctx, 0, &view) == NX_OK &&
          view.description.length == 12 &&
          memcmp(view.description.data, "No telemetry", 12) == 0);
    CHECK(nx_semantics_get(ctx, 1, &view) == NX_OK &&
          view.role == NX_ROLE_LIST && view.description.length == 20);
    nx_context_destroy(ctx);
    return 0;
}

static int font_preview(const char *font_path, const char *output_path,
                        int large_text) {
    int status = 1;
    FILE *input = fopen(font_path, "rb"), *output = NULL;
    uint8_t *font_bytes = NULL, *pixels = NULL;
    nx_font *font = NULL;
    nx_font_collection *collection = NULL;
    nx_context *ctx = NULL;
    if (!input || fseek(input, 0, SEEK_END) != 0) goto done;
    long length = ftell(input);
    if (length <= 0 || length > 64L * 1024L * 1024L ||
        fseek(input, 0, SEEK_SET) != 0) goto done;
    font_bytes = malloc((size_t)length);
    pixels = malloc((size_t)WIDTH * HEIGHT * 4u);
    if (!font_bytes || !pixels ||
        fread(font_bytes, 1, (size_t)length, input) != (size_t)length ||
        nx_font_open(font_bytes, (size_t)length, NULL, &font) != NX_OK ||
        nx_font_collection_create(NULL, &collection) != NX_OK ||
        nx_font_collection_add(collection, font) != NX_OK ||
        nx_context_create(NULL, &ctx) != NX_OK)
        goto done;
    nx_telemetry_panel_desc telemetry = {0};
    telemetry.id = 1;
    telemetry.bounds = (nx_rect){8, 8, 188, 214};
    telemetry.name = SPAN("Telemetry");
    telemetry.metrics = metrics;
    telemetry.metric_count = 4;
    telemetry.first_row = large_text ? 2u : 0u;
    telemetry.row_height = 40;
    telemetry.fonts = collection;
    telemetry.pixels_per_em = large_text ? 24 : 12;
    telemetry.glyph_cache_bytes = 2u * 1024u * 1024u;
    nx_profiler_panel_desc profiler = {0};
    profiler.id = 2;
    profiler.bounds = (nx_rect){204, 8, 188, 214};
    profiler.name = SPAN("Profiler");
    profiler.entries = entries;
    profiler.entry_count = 4;
    profiler.frame_ms = 16.7;
    profiler.row_height = 40;
    profiler.fonts = collection;
    profiler.pixels_per_em = large_text ? 24 : 12;
    profiler.glyph_cache_bytes = 2u * 1024u * 1024u;
    nx_theme theme;
    const nx_command *commands = NULL;
    size_t command_count = 0;
    if (nx_theme_preset(NX_THEME_DARK, &theme) != NX_OK ||
        nx_context_set_theme(ctx, &theme) != NX_OK ||
        nx_begin_frame(ctx, WIDTH, HEIGHT, 0) != NX_OK ||
        nx_telemetry_panel(ctx, &telemetry) != NX_OK ||
        nx_profiler_panel(ctx, &profiler) != NX_OK ||
        nx_end_frame(ctx) != NX_OK ||
        nx_frame_commands(ctx, &commands, &command_count) != NX_OK ||
        nx_headless_render(commands, command_count, pixels,
            WIDTH, HEIGHT, WIDTH * 4u, (nx_color){0, 0, 0, 1}) != NX_OK)
        goto done;
    output = fopen(output_path, "wb");
    if (!output || fprintf(output, "P6\n%u %u\n255\n", WIDTH, HEIGHT) < 0)
        goto done;
    for (size_t i = 0; i < (size_t)WIDTH * HEIGHT; ++i)
        if (fwrite(pixels + i * 4u, 1, 3, output) != 3) goto done;
    status = 0;
done:
    if (output && fclose(output) != 0) status = 1;
    nx_context_destroy(ctx);
    nx_font_collection_destroy(collection);
    nx_font_close(font);
    free(font_bytes);
    free(pixels);
    if (input) fclose(input);
    return status;
}

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "--font-preview") == 0)
        return font_preview(argv[2], argv[3], 0);
    if (argc == 4 && strcmp(argv[1], "--font-preview-large") == 0)
        return font_preview(argv[2], argv[3], 1);
    int update = argc == 2 && strcmp(argv[1], "--update-baselines") == 0;
    if (run_scene(NX_THEME_DARK, "telemetry_dark", update) ||
        run_scene(NX_THEME_LIGHT, "telemetry_light", update) ||
        run_scene(NX_THEME_HIGH_CONTRAST, "telemetry_high_contrast", update) ||
        stress_updates() || test_empty_panels() ||
        test_allocation_rollback()) return 1;
    return 0;
}
