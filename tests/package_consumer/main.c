#include <nexaui/nexaui.h>
#include <nexaui/cell_grid.h>
#if defined(NEXAUI_PACKAGE_LINUX_WAYLAND)
#include <nexaui/wayland_host.h>
#include <nexaui/wayland_clipboard.h>
#endif
#include <string.h>

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

static int font_metadata(void) {
    uint8_t bytes[140]={0};bytes[1]=1;bytes[5]=2;
    memcpy(bytes+12,"name",4);bytes[23]=44;bytes[27]=26;
    memcpy(bytes+28,"OS/2",4);bytes[39]=72;bytes[43]=68;
    bytes[47]=1;bytes[49]=18;bytes[51]=3;bytes[53]=1;bytes[54]=4;bytes[55]=9;
    bytes[57]=1;bytes[59]=8;bytes[63]='T';bytes[65]='e';bytes[67]='s';bytes[69]='t';
    bytes[76]=1;bytes[77]=144;bytes[79]=5;bytes[135]=1;
    nx_font *font=NULL;if(nx_font_open(bytes,sizeof(bytes),NULL,&font)!=NX_OK)return 0;
    char name[5];size_t length=0;nx_font_style style;
    int okay=nx_font_name_utf8(font,1,name,sizeof(name),&length)==NX_OK && length==4 && !strcmp(name,"Test") &&
        nx_font_get_style(font,&style)==NX_OK && style.weight_class==400 && style.width_class==5 && style.slant==NX_FONT_SLANT_ITALIC;
    nx_font_close(font);return okay;
}
int main(int argc, char **argv) {
    if(!font_metadata())return 34;
    if (nx_window_fullscreen(NULL) != NX_ERROR_ARGUMENT) return 31;
    if (nx_numeric_field_set_ime_window(NULL, NULL) != NX_ERROR_ARGUMENT) return 28;
    if (nx_numeric_field_set_clipboard(NULL, NULL) != NX_ERROR_ARGUMENT) return 29;
    int numeric_valid = 1;
    if (nx_numeric_field_commit(NULL, 0, 10, &numeric_valid) != NX_ERROR_ARGUMENT || numeric_valid) return 30;
#if defined(NEXAUI_PACKAGE_LINUX_WAYLAND)
    nx_wayland_shm_host *host=(nx_wayland_shm_host *)1;
    if (nx_wayland_shm_host_create(NULL,NULL,NULL,NULL,&host)!=NX_ERROR_ARGUMENT || host) return 22;
    uint32_t host_events=99;
    if (nx_wayland_shm_host_poll(NULL,0,-1,&host_events)!=NX_ERROR_ARGUMENT || host_events) return 23;
    uint64_t clipboard_token=1;
    if (nx_wayland_clipboard_read_begin(NULL,1024,1000,0,&clipboard_token)!=NX_ERROR_ARGUMENT || clipboard_token) return 25;
    char *clipboard_bytes=(char *)1; size_t clipboard_length=1; int clipboard_ready=1;
    if (nx_wayland_clipboard_read_take(NULL,1,&clipboard_bytes,&clipboard_length,&clipboard_ready)!=NX_ERROR_ARGUMENT ||
        clipboard_bytes || clipboard_length || clipboard_ready) return 26;
    if (nx_wayland_clipboard_read_cancel(NULL,1)!=NX_ERROR_ARGUMENT) return 27;
    nx_wayland_clipboard_read_release(NULL,0);
#endif
    uint8_t scaled_pixels[4 * 4 * 4];
    nx_command scaled_box = {.type=NX_COMMAND_FILL_RECT, .rect={0,0,1,1},
                             .color={1,0,0,1}};
    if (nx_headless_render_scaled(&scaled_box, 1, scaled_pixels, 4, 4, 16,
                                  (nx_color){0,0,0,1}, 2) != NX_OK ||
        scaled_pixels[4] != 255 || scaled_pixels[8] != 0) return 21;
    const char *revision = "must be cleared";
    if (nx_get_source_revision(NULL) != NX_ERROR_ARGUMENT) return 19;
    nx_result revision_result = nx_get_source_revision(&revision);
#ifdef NEXAUI_PACKAGE_SOURCE_REVISION
    if (NEXAUI_PACKAGE_SOURCE_REVISION[0]) {
        if (revision_result != NX_OK || !revision ||
            strcmp(revision, NEXAUI_PACKAGE_SOURCE_REVISION)) return 19;
    } else if (revision_result != NX_ERROR_UNSUPPORTED || revision) return 19;
#else
    if ((revision_result != NX_OK || !revision) &&
        (revision_result != NX_ERROR_UNSUPPORTED || revision)) return 19;
#endif
    uint64_t features = 0;
    if (nx_get_build_features(NULL) != NX_ERROR_ARGUMENT ||
        nx_get_build_features(&features) != NX_OK)
        return 17;
    if (argc == 2 && (!strcmp(argv[1], "--expect-wayland-only") ||
                      !strcmp(argv[1], "--expect-wayland-only-no-atspi")) &&
        (!(features & NX_BUILD_WINDOW_WAYLAND) || !(features & NX_BUILD_LINUX_POLL_FD) || (features & NX_BUILD_WINDOW_X11)))
        return 18;
    if (argc == 2 && !strcmp(argv[1], "--expect-wayland-only-no-atspi") &&
        (features & NX_BUILD_LINUX_ATSPI_TRANSPORT)) return 24;
    nx_version version = {99, 99, 99};
    if (nx_get_version(NULL) != NX_ERROR_ARGUMENT ||
        nx_get_version(&version) != NX_OK ||
        version.major != NX_VERSION_MAJOR || version.minor != NX_VERSION_MINOR ||
        version.patch != NX_VERSION_PATCH)
        return 16;
#ifdef NEXAUI_PACKAGE_VERSION_MAJOR
    if (version.major != NEXAUI_PACKAGE_VERSION_MAJOR ||
        version.minor != NEXAUI_PACKAGE_VERSION_MINOR ||
        version.patch != NEXAUI_PACKAGE_VERSION_PATCH ||
        strcmp(NX_VERSION_STRING, NEXAUI_PACKAGE_VERSION_STRING))
        return 16;
#endif
    nx_pixel_size pixels={9,9};
    nx_result pixel_result=nx_window_get_pixel_size(NULL,&pixels);
    if ((pixel_result!=NX_ERROR_ARGUMENT && pixel_result!=NX_ERROR_UNSUPPORTED) || pixels.width || pixels.height)
        return 15;
#if defined(__linux__)
    uint32_t fd_events=99;
    nx_result fd_result=nx_window_poll_with_fd(NULL,0,0,&fd_events);
    if ((fd_result!=NX_ERROR_ARGUMENT && fd_result!=NX_ERROR_UNSUPPORTED) || fd_events) return 20;
#endif
    nx_text_stamp *stamp=0;nx_rect stamp_bounds;nx_glyph_mask_view mask;
    if(nx_text_stamp_create_scaled(0,1.5f,65536,0,&stamp)!=NX_ERROR_ARGUMENT||stamp||
       nx_text_stamp_bounds(0,&stamp_bounds)!=NX_ERROR_ARGUMENT||
       nx_glyph_cache_get_fractional(0,0,19.25,&mask)!=NX_ERROR_ARGUMENT||
       nx_draw_text_layout_scaled(0,0,(nx_point){0},(nx_color){1,1,1,1},1.5f,65536)!=NX_ERROR_ARGUMENT)
        return 13;
    nx_context *context = 0;
    nx_layout *layout = 0;
    if (nx_context_create(0, &context) != NX_OK) return 1;
    nx_text_document_result document_result={0};
    if(nx_text_document_register(NULL,NULL,NULL,NULL,NULL,&document_result)!=NX_ERROR_ARGUMENT || document_result.applied)
        return 32;
    nx_widget_desc document_widget={0};document_widget.semantic.id=9001;
    document_widget.semantic.role=NX_ROLE_TEXT_FIELD;document_widget.semantic.bounds=(nx_rect){0,0,8,8};
    document_widget.semantic.name=(nx_utf8_span){"External document",17};document_widget.semantic.state=NX_SEM_READONLY;
    nx_text_document_state document_state={{"external",8},11,1,0,8,0,1};
    nx_text_document_box document_box={0,8,{0,0,8,8}};
    nx_text_document_presentation presentation={&document_box,1,NULL,0,NULL,0};
    if(nx_text_document_set_presentation(NULL,9001,11,1,&presentation)!=NX_ERROR_ARGUMENT)return 33;
    if(nx_begin_frame(context,8,8,0)!=NX_OK ||
       nx_text_document_register(context,&document_widget,&document_state,NULL,NULL,&document_result)!=NX_OK ||
       nx_text_document_set_presentation(context,9001,11,1,&presentation)!=NX_OK ||
       nx_end_frame(context)!=NX_OK)return 32;
    nx_cell_view sample_cell={.foreground={1,1,1,1},.background={0,0,0,1},.span=1};
    nx_cell_grid_desc grid={.bounds={0,0,8,8},.cell_width=8,.cell_height=8,.baseline=6,
        .pixels_per_em=8,.cells=&sample_cell,.rows=1,.columns=1,.row_stride=1,.cell_capacity=1,
        .glyph_cache_bytes=1024,.mask_budget_bytes=1024,.cursor_color={1,1,1,1}};
    size_t grid_row=9,grid_column=9;
    if(nx_cell_grid_hit_test(&grid,1,1,&grid_row,&grid_column)!=NX_OK || grid_row || grid_column ||
       nx_begin_frame(context,8,8,0)!=NX_OK || nx_draw_cell_grid(context,&grid)!=NX_OK ||
       nx_end_frame(context)!=NX_OK) return 24;
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
