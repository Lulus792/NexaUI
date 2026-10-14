#include "nexaui/nexaui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HANDLE demo_thread;
#else
#include <pthread.h>
typedef pthread_t demo_thread;
#endif

#define WIDTH 960u
#define HEIGHT 600u
#define SAMPLE_COUNT 128u
#define FONT_LIMIT (64u * 1024u * 1024u)
#define GLYPH_CACHE (2u * 1024u * 1024u)

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 0; \
} } while (0)

typedef struct simulation_snapshot {
    unsigned sequence;
    nx_series_point points[SAMPLE_COUNT];
    nx_telemetry_metric metrics[4];
    nx_profiler_entry profiler[4];
} simulation_snapshot;

typedef struct simulation_request {
    unsigned publications;
} simulation_request;

typedef struct worker_args {
    nx_snapshot_exchange *exchange;
    nx_sim_channel *channel;
    size_t *snapshot_disposals;
    nx_result result;
} worker_args;

typedef struct demo_font {
    uint8_t *bytes;
    nx_font *face;
    nx_font_collection *collection;
} demo_font;

static nx_utf8_span span(const char *text) {
    return (nx_utf8_span){text, strlen(text)};
}

static void dispose_owned(void *user, void *data, size_t bytes) {
    if (user) ++*(size_t *)user;
    (void)bytes;
    free(data);
}

static void close_font(demo_font *font) {
    nx_font_collection_destroy(font->collection);
    nx_font_close(font->face);
    free(font->bytes);
    *font = (demo_font){0};
}

static int open_font(const char *path, demo_font *font) {
    FILE *input = fopen(path, "rb");
    if (!input) return 0;
    int okay = fseek(input, 0, SEEK_END) == 0;
    long length = okay ? ftell(input) : -1;
    okay = okay && length > 0 && (unsigned long)length <= FONT_LIMIT &&
           fseek(input, 0, SEEK_SET) == 0;
    if (okay) font->bytes = (uint8_t *)malloc((size_t)length);
    okay = okay && font->bytes &&
           fread(font->bytes, 1, (size_t)length, input) == (size_t)length;
    if (fclose(input) != 0) okay = 0;
    if (okay) okay = nx_font_open(font->bytes, (size_t)length,
                                  NULL, &font->face) == NX_OK &&
                     nx_font_collection_create(NULL, &font->collection) == NX_OK &&
                     nx_font_collection_add(font->collection, font->face) == NX_OK;
    if (!okay) close_font(font);
    return okay;
}

static simulation_snapshot *make_snapshot(unsigned sequence) {
    simulation_snapshot *snapshot = (simulation_snapshot *)malloc(sizeof(*snapshot));
    if (!snapshot) return NULL;
    snapshot->sequence = sequence;
    double position = 1.0, velocity = 0.0;
    unsigned total = SAMPLE_COUNT + sequence * 16u;
    for (unsigned step = 0; step < total; ++step) {
        /* Semi-implicit Euler, dt=0.04 s, unit mass/spring and damping 0.08. */
        velocity += (-position - 0.08 * velocity) * 0.04;
        position += velocity * 0.04;
        if (step >= total - SAMPLE_COUNT)
            snapshot->points[step - (total - SAMPLE_COUNT)] =
                (nx_series_point){(double)step * 0.04, position};
    }
    double speed = velocity < 0 ? -velocity : velocity;
    double energy = 0.5 * (position * position + velocity * velocity);
    snapshot->metrics[0] = (nx_telemetry_metric){11, span("Position"),
        span("m"), {0}, position, -1.2, 1.2, NX_TELEMETRY_NORMAL};
    snapshot->metrics[1] = (nx_telemetry_metric){12, span("Speed"),
        span("m/s"), {0}, speed, 0, 1.2, NX_TELEMETRY_NORMAL};
    snapshot->metrics[2] = (nx_telemetry_metric){13, span("Energy"),
        span("J"), {0}, energy, 0, 1.2, NX_TELEMETRY_NORMAL};
    snapshot->metrics[3] = (nx_telemetry_metric){14, span("Steps"),
        span("count"), {0}, (double)total, 0, 512, NX_TELEMETRY_NORMAL};
    snapshot->profiler[0] = (nx_profiler_entry){21, span("Sample frame"),
                                                9.0, 2.0, 1, 0};
    snapshot->profiler[1] = (nx_profiler_entry){22, span("Integrate"),
                                                5.0, 5.0, total, 1};
    snapshot->profiler[2] = (nx_profiler_entry){23, span("Snapshot"),
                                                2.0, 1.0, 1, 1};
    snapshot->profiler[3] = (nx_profiler_entry){24, span("Publish"),
                                                1.0, 1.0, 1, 2};
    return snapshot;
}

static nx_result sample(void *user, size_t index, nx_series_point *out) {
    const simulation_snapshot *snapshot = (const simulation_snapshot *)user;
    if (!out || index >= SAMPLE_COUNT) return NX_ERROR_ARGUMENT;
    *out = snapshot->points[index];
    return NX_OK;
}

static void worker_run(worker_args *args) {
    nx_sim_message command = {0};
    args->result = nx_sim_channel_take_command(args->channel, &command);
    if (args->result != NX_OK || !command.ticket) {
        args->result = NX_ERROR_STATE;
        return;
    }
    uint64_t ticket = command.ticket;
    unsigned publications = 0;
    if (command.bytes == sizeof(simulation_request) && command.data)
        publications = ((const simulation_request *)command.data)->publications;
    nx_sim_message_dispose(&command);
    args->result = publications > 0 && publications <= 16u ?
                   NX_OK : NX_ERROR_ARGUMENT;
    for (unsigned sequence = 1; args->result == NX_OK &&
         sequence <= publications; ++sequence) {
        simulation_snapshot *snapshot = make_snapshot(sequence);
        if (!snapshot) {
            args->result = NX_ERROR_MEMORY;
            break;
        }
        args->result = nx_snapshot_exchange_publish(args->exchange, snapshot,
            sizeof(*snapshot), dispose_owned, args->snapshot_disposals);
        if (args->result != NX_OK) free(snapshot);
    }
    nx_result completion = nx_sim_channel_post_completion(args->channel,
        ticket, args->result, NULL, 0, NULL, NULL);
    if (completion != NX_OK) args->result = completion;
}

#if defined(_WIN32)
static DWORD WINAPI worker_entry(LPVOID user) {
    worker_run((worker_args *)user);
    return 0;
}
static int start_thread(demo_thread *thread, worker_args *args) {
    *thread = CreateThread(NULL, 0, worker_entry, args, 0, NULL);
    return *thread != NULL;
}
static int join_thread(demo_thread thread) {
    DWORD result = WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return result == WAIT_OBJECT_0;
}
#else
static void *worker_entry(void *user) {
    worker_run((worker_args *)user);
    return NULL;
}
static int start_thread(demo_thread *thread, worker_args *args) {
    return pthread_create(thread, NULL, worker_entry, args) == 0;
}
static int join_thread(demo_thread thread) {
    return pthread_join(thread, NULL) == 0;
}
#endif

static int draw_dashboard(nx_context *ui, const simulation_snapshot *snapshot,
                          nx_series_envelope *scratch,
                          const nx_font_collection *fonts) {
    CHECK(nx_begin_frame(ui, WIDTH, HEIGHT, 1.0 / 60.0) == NX_OK);
    nx_semantic_desc root = {0};
    root.id = 1;
    root.role = NX_ROLE_WINDOW;
    root.bounds = (nx_rect){0, 0, WIDTH, HEIGHT};
    root.name = span("Simulation dashboard");
    CHECK(nx_semantics_add(ui, &root, NULL) == NX_OK);

    nx_plot_desc plot = {0};
    plot.id = 2;
    plot.parent_id = 1;
    plot.bounds = (nx_rect){24, 20, 912, 250};
    plot.name = span("Damped oscillator position");
    plot.series = (nx_series_view){SAMPLE_COUNT, (void *)snapshot, sample};
    plot.x_min = snapshot->points[0].x;
    plot.x_max = snapshot->points[SAMPLE_COUNT - 1u].x;
    plot.y_min = -1.1;
    plot.y_max = 1.1;
    plot.scratch = scratch;
    plot.scratch_capacity = WIDTH;
    plot.fonts = fonts;
    plot.pixels_per_em = 14;
    plot.glyph_cache_bytes = GLYPH_CACHE;
    nx_plot_state plot_state = {0};
    CHECK(nx_plot(ui, &plot, &plot_state) == NX_OK);

    nx_telemetry_panel_desc telemetry = {0};
    telemetry.id = 3;
    telemetry.parent_id = 1;
    telemetry.bounds = (nx_rect){24, 290, 438, 284};
    telemetry.name = span("Telemetry");
    telemetry.metrics = snapshot->metrics;
    telemetry.metric_count = 4;
    telemetry.row_height = 52;
    telemetry.fonts = fonts;
    telemetry.pixels_per_em = 15;
    telemetry.glyph_cache_bytes = GLYPH_CACHE;
    CHECK(nx_telemetry_panel(ui, &telemetry) == NX_OK);

    nx_profiler_panel_desc profiler = {0};
    profiler.id = 4;
    profiler.parent_id = 1;
    profiler.bounds = (nx_rect){498, 290, 438, 284};
    profiler.name = span("Illustrative timings");
    profiler.entries = snapshot->profiler;
    profiler.entry_count = 4;
    profiler.frame_ms = 16.7;
    profiler.row_height = 52;
    profiler.fonts = fonts;
    profiler.pixels_per_em = 15;
    profiler.glyph_cache_bytes = GLYPH_CACHE;
    CHECK(nx_profiler_panel(ui, &profiler) == NX_OK);
    CHECK(nx_end_frame(ui) == NX_OK);

    size_t count = 0;
    uint32_t index = UINT32_MAX;
    nx_semantic_view view;
    CHECK(nx_semantics_count(ui, &count) == NX_OK && count == 12);
    CHECK(nx_semantics_find(ui, 13, &index) == NX_OK &&
          nx_semantics_get(ui, index, &view) == NX_OK &&
          view.parent_id == 3 && view.role == NX_ROLE_PROGRESS);
    CHECK(nx_semantics_find(ui, 22, &index) == NX_OK &&
          nx_semantics_get(ui, index, &view) == NX_OK &&
          view.parent_id == 4 && view.role == NX_ROLE_LIST_ITEM);
    return 1;
}

static int save_image(nx_context *ui, uint8_t *pixels,
                      const char *prefix, const char *suffix,
                      uint64_t *out_hash) {
    const nx_command *commands = NULL;
    size_t count = 0;
    CHECK(nx_frame_commands(ui, &commands, &count) == NX_OK && count > 30);
    CHECK(nx_headless_render(commands, count, pixels, WIDTH, HEIGHT,
                             WIDTH * 4u, (nx_color){0.06f, 0.08f, 0.12f, 1}) == NX_OK);
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < (size_t)WIDTH * HEIGHT * 4u; ++i) {
        hash ^= pixels[i];
        hash *= UINT64_C(1099511628211);
    }
    char path[512];
    int length = snprintf(path, sizeof(path), "%s-%s.ppm", prefix, suffix);
    CHECK(length > 0 && (size_t)length < sizeof(path));
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    int okay = fprintf(file, "P6\n%u %u\n255\n", WIDTH, HEIGHT) > 0;
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
    if (fclose(file) != 0) okay = 0;
    CHECK(okay);
    *out_hash = hash;
    return 1;
}

static int run(const char *prefix, const char *font_path) {
    nx_snapshot_exchange *exchange = NULL;
    nx_sim_channel *channel = NULL;
    nx_context *ui = NULL;
    nx_snapshot_view old = {0}, latest = {0};
    nx_series_envelope *scratch =
        (nx_series_envelope *)malloc(WIDTH * sizeof(*scratch));
    uint8_t *pixels = (uint8_t *)malloc((size_t)WIDTH * HEIGHT * 4u);
    size_t snapshot_disposals = 0, request_disposals = 0;
    demo_font font = {0};
    int okay = scratch && pixels &&
               nx_snapshot_exchange_create(NULL, &exchange) == NX_OK &&
               nx_sim_channel_create(1, NULL, &channel) == NX_OK &&
               nx_context_create(NULL, &ui) == NX_OK;
    if (okay && font_path) okay = open_font(font_path, &font);
    simulation_snapshot *initial = okay ? make_snapshot(0) : NULL;
    if (okay) okay = initial != NULL;
    if (okay) {
        if (nx_snapshot_exchange_publish(exchange, initial, sizeof(*initial),
            dispose_owned, &snapshot_disposals) != NX_OK) {
            free(initial);
            okay = 0;
        }
    }
    if (okay) okay = nx_snapshot_exchange_acquire(exchange, &old) == NX_OK &&
                     old.data && old.bytes == sizeof(simulation_snapshot);

    simulation_request *request = okay ?
        (simulation_request *)malloc(sizeof(*request)) : NULL;
    if (okay) okay = request != NULL;
    uint64_t ticket = 0;
    if (okay) {
        request->publications = 16;
        if (nx_sim_channel_post_command(channel, request, sizeof(*request),
            dispose_owned, &request_disposals, &ticket) != NX_OK) {
            free(request);
            okay = 0;
        }
    }
    worker_args args = {exchange, channel, &snapshot_disposals, NX_OK};
    demo_thread thread;
    int started = 0;
    if (okay) okay = started = start_thread(&thread, &args);
    if (started) okay = join_thread(thread) && okay;
    nx_sim_message completion = {0};
    if (okay) okay = args.result == NX_OK &&
                     nx_sim_channel_take_completion(channel, &completion) == NX_OK &&
                     completion.ticket == ticket && completion.status == NX_OK;
    nx_sim_message_dispose(&completion);

    uint64_t early_hash = 0, late_hash = 0;
    if (okay) {
        const simulation_snapshot *snapshot =
            (const simulation_snapshot *)old.data;
        okay = snapshot->sequence == 0 && snapshot_disposals == 15u &&
               draw_dashboard(ui, snapshot, scratch, font.collection) &&
               save_image(ui, pixels, prefix, "early", &early_hash);
    }
    if (old.data) {
        if (nx_snapshot_exchange_release(exchange, &old) != NX_OK) okay = 0;
    }
    if (okay) okay = snapshot_disposals == 16u &&
                     nx_snapshot_exchange_acquire(exchange, &latest) == NX_OK &&
                     latest.data && latest.bytes == sizeof(simulation_snapshot);
    if (okay) {
        const simulation_snapshot *snapshot =
            (const simulation_snapshot *)latest.data;
        okay = snapshot->sequence == 16u &&
               draw_dashboard(ui, snapshot, scratch, font.collection) &&
               save_image(ui, pixels, prefix, "late", &late_hash) &&
               early_hash != late_hash;
    }
    if (latest.data) {
        if (nx_snapshot_exchange_release(exchange, &latest) != NX_OK) okay = 0;
    }
    if (channel && nx_sim_channel_destroy(channel) != NX_OK) okay = 0;
    if (exchange && nx_snapshot_exchange_destroy(exchange) != NX_OK) okay = 0;
    if (okay) okay = snapshot_disposals == 17u && request_disposals == 1u;
    nx_context_destroy(ui);
    close_font(&font);
    free(pixels);
    free(scratch);
    if (okay) puts("worker published 16 immutable snapshots; pinned view and dashboard verified");
    return okay;
}

int main(int argc, char **argv) {
    if ((argc != 2 && argc != 3) || !argv[1][0]) {
        fputs("usage: nexaui_simulation_dashboard OUTPUT_PREFIX [FONT_FILE]\n", stderr);
        return 2;
    }
    if (!run(argv[1], argc == 3 ? argv[2] : NULL)) {
        fputs("simulation dashboard example failed\n", stderr);
        return 1;
    }
    return 0;
}
