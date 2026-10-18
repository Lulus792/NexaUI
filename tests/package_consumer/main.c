#include <nexaui/nexaui.h>

static nx_result sample_series(void *user, size_t index,
                               nx_series_point *out) {
    const nx_series_point *points = (const nx_series_point *)user;
    *out = points[index];
    return NX_OK;
}

typedef struct consumer_surface {
    uint8_t pixels[16];
    uint32_t width, height;
} consumer_surface;

static nx_result consumer_resize(void *user, uint32_t width, uint32_t height) {
    consumer_surface *surface = user;
    surface->width = width;
    surface->height = height;
    return NX_OK;
}

static nx_result consumer_acquire(void *user, nx_bitmap_view *out) {
    consumer_surface *surface = user;
    *out = (nx_bitmap_view){surface->pixels, surface->width,
                            surface->height, surface->width * 4u,
                            NX_BITMAP_RGBA8};
    return NX_OK;
}

static void consumer_release(void *user) { (void)user; }

int main(void) {
    nx_text_stamp *stamp=0;nx_rect stamp_bounds;nx_glyph_mask_view mask;
    if(nx_text_stamp_create_scaled(0,1.5f,65536,0,&stamp)!=NX_ERROR_ARGUMENT||stamp||
       nx_text_stamp_bounds(0,&stamp_bounds)!=NX_ERROR_ARGUMENT||
       nx_glyph_cache_get_fractional(0,0,19.25,&mask)!=NX_ERROR_ARGUMENT||
       nx_draw_text_layout_scaled(0,0,(nx_point){0},(nx_color){1,1,1,1},1.5f,65536)!=NX_ERROR_ARGUMENT)
        return 13;
    nx_context *context = 0;
    nx_layout *layout = 0;
    if (nx_context_create(0, &context) != NX_OK) return 1;
    float text_density=0;
    if (nx_context_get_text_raster_scale(context,&text_density)!=NX_OK || text_density!=1 ||
        nx_context_set_text_raster_scale(context,1.5f)!=NX_OK ||
        nx_context_get_text_raster_scale(context,&text_density)!=NX_OK || text_density!=1.5f)
        return 14;
    if (nx_layout_create(0, 0, &layout) != NX_OK) {
        nx_context_destroy(context);
        return 2;
    }
    nx_layout_desc desc = nx_layout_default_desc(NX_LAYOUT_LEAF);
    uint32_t root;
    nx_size desired;
    if (nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &desc, &root) != NX_OK ||
        nx_layout_measure(layout, root, 100, 100, &desired) != NX_OK ||
        nx_layout_arrange(layout, root, (nx_rect){0, 0, 100, 100}) != NX_OK) {
        nx_layout_destroy(layout);
        nx_context_destroy(context);
        return 3;
    }
    nx_semantic_desc semantic = {0};
    semantic.id = 1;
    semantic.role = NX_ROLE_WINDOW;
    semantic.bounds = (nx_rect){0, 0, 100, 100};
    semantic.name = (nx_utf8_span){"Consumer", 8};
    nx_semantic_view view;
    if (nx_begin_frame(context, 100, 100, 0) != NX_OK ||
        nx_semantics_add(context, &semantic, 0) != NX_OK ||
        nx_end_frame(context) != NX_OK ||
        nx_semantics_get(context, 0, &view) != NX_OK || view.id != 1) {
        nx_layout_destroy(layout);
        nx_context_destroy(context);
        return 4;
    }
    uint8_t line_breaks[4];
    size_t next = 0;
    if (nx_line_breaks("a b", 3, line_breaks, sizeof(line_breaks)) != NX_OK ||
        line_breaks[1] != NX_LINE_BREAK_NONE ||
        line_breaks[2] != NX_LINE_BREAK_OPTIONAL ||
        line_breaks[3] != NX_LINE_BREAK_MANDATORY ||
        nx_grapheme_next("e\xcc\x81", 3, 0, &next) != NX_OK || next != 3 ||
        nx_word_next("hello world", 11, 0, &next) != NX_OK || next != 5) {
        nx_layout_destroy(layout);
        nx_context_destroy(context);
        return 5;
    }
    nx_snapshot_exchange *exchange = 0;
    if (nx_snapshot_exchange_create(0, &exchange) != NX_OK ||
        nx_snapshot_exchange_destroy(exchange) != NX_OK) {
        nx_layout_destroy(layout);
        nx_context_destroy(context);
        return 6;
    }
    nx_sim_channel *channel = 0;
    if (nx_sim_channel_create(2, 0, &channel) != NX_OK ||
        nx_sim_channel_destroy(channel) != NX_OK) {
        nx_layout_destroy(layout);
        nx_context_destroy(context);
        return 7;
    }
    nx_series_point points[2] = {{0, 1}, {1, 9}};
    nx_series_view series = {2, points, sample_series};
    nx_series_envelope envelopes[2];
    size_t envelope_count = 0;
    if (nx_series_decimate(&series, 0, 1, 2, envelopes, 2,
                           &envelope_count) != NX_OK ||
        envelope_count != 2 || envelopes[1].maximum.point.y != 9) {
        nx_layout_destroy(layout);
        nx_context_destroy(context);
        return 8;
    }
    nx_series_envelope plot_scratch[100];
    nx_plot_desc plot = {0};
    nx_plot_state plot_state = {0};
    plot.id = 2;
    plot.bounds = (nx_rect){0, 0, 100, 100};
    plot.name = (nx_utf8_span){"Consumer plot", 13};
    plot.series = series;
    plot.x_max = 1;
    plot.y_max = 10;
    plot.scratch = plot_scratch;
    plot.scratch_capacity = 100;
    if (nx_begin_frame(context, 100, 100, 0) != NX_OK ||
        nx_plot(context, &plot, &plot_state) != NX_OK ||
        nx_end_frame(context) != NX_OK) {
        nx_layout_destroy(layout);
        nx_context_destroy(context);
        return 9;
    }
    nx_telemetry_metric metric = {0};
    metric.id = 5;
    metric.name = (nx_utf8_span){"Speed", 5};
    metric.unit = (nx_utf8_span){"m/s", 3};
    metric.value = 2;
    metric.maximum = 10;
    nx_telemetry_panel_desc telemetry = {0};
    telemetry.id = 3;
    telemetry.bounds = (nx_rect){0, 0, 50, 100};
    telemetry.name = (nx_utf8_span){"Telemetry", 9};
    telemetry.metrics = &metric;
    telemetry.metric_count = 1;
    nx_profiler_entry entry = {0};
    entry.id = 6;
    entry.name = (nx_utf8_span){"Tick", 4};
    entry.inclusive_ms = 2;
    entry.self_ms = 1;
    entry.calls = 1;
    nx_profiler_panel_desc profiler = {0};
    profiler.id = 4;
    profiler.bounds = (nx_rect){50, 0, 50, 100};
    profiler.name = (nx_utf8_span){"Profiler", 8};
    profiler.entries = &entry;
    profiler.entry_count = 1;
    profiler.frame_ms = 16.7;
    if (nx_begin_frame(context, 100, 100, 0) != NX_OK ||
        nx_telemetry_panel(context, &telemetry) != NX_OK ||
        nx_profiler_panel(context, &profiler) != NX_OK ||
        nx_end_frame(context) != NX_OK) {
        nx_layout_destroy(layout);
        nx_context_destroy(context);
        return 10;
    }
    nx_property_row_desc property = {0};
    nx_property_action property_action;
    property.id = 7;
    property.bounds = (nx_rect){0, 0, 100, 32};
    property.name = (nx_utf8_span){"Enabled", 7};
    property.kind = NX_PROPERTY_BOOLEAN;
    property.boolean_value = 1;
    if (nx_begin_frame(context, 100, 100, 0) != NX_OK ||
        nx_property_row(context, &property, &property_action) != NX_OK ||
        nx_end_frame(context) != NX_OK) {
        nx_layout_destroy(layout);
        nx_context_destroy(context);
        return 11;
    }
    consumer_surface image = {{0}, 0, 0};
    nx_viewport_source source = {&image, consumer_resize,
                                  consumer_acquire, consumer_release};
    nx_viewport_surface *surface = 0;
    nx_viewport_desc viewport = {0};
    nx_interaction_state interaction;
    viewport.id = 8;
    viewport.bounds = (nx_rect){0, 0, 2, 2};
    viewport.name = (nx_utf8_span){"View", 4};
    viewport.scale = 1;
    if (nx_viewport_surface_create(&source, 0, &surface) != NX_OK ||
        nx_begin_frame(context, 100, 100, 0) != NX_OK ||
        nx_viewport(context, surface, &viewport, &interaction) != NX_OK ||
        nx_end_frame(context) != NX_OK ||
        nx_viewport_surface_complete(surface) != NX_OK ||
        nx_viewport_surface_destroy(surface) != NX_OK) {
        nx_layout_destroy(layout);
        nx_context_destroy(context);
        return 12;
    }
    nx_layout_destroy(layout);
    nx_context_destroy(context);
    return 0;
}
