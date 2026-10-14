#include "nexaui/nexaui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static double channel(double value) {
    return value <= 0.04045 ? value / 12.92 : pow((value + 0.055) / 1.055, 2.4);
}

static double luminance(nx_color color) {
    return 0.2126 * channel(color.r) +
           0.7152 * channel(color.g) +
           0.0722 * channel(color.b);
}

static double contrast(nx_color a, nx_color b) {
    double x = luminance(a), y = luminance(b);
    return (fmax(x, y) + 0.05) / (fmin(x, y) + 0.05);
}

static int same_color(nx_color a, nx_color b) {
    return fabsf(a.r - b.r) < 0.0001f && fabsf(a.g - b.g) < 0.0001f &&
           fabsf(a.b - b.b) < 0.0001f && fabsf(a.a - b.a) < 0.0001f;
}

static int test_presets_and_contrast(void) {
    for (int kind = NX_THEME_DARK; kind <= NX_THEME_HIGH_CONTRAST; ++kind) {
        nx_theme theme;
        CHECK(nx_theme_preset((nx_theme_kind)kind, &theme) == NX_OK);
        CHECK(theme.color_mask == (UINT64_C(1) << NX_COLOR_TOKEN_COUNT) - 1u);
        CHECK(theme.metric_mask == (UINT64_C(1) << NX_METRIC_TOKEN_COUNT) - 1u);
        CHECK(contrast(theme.colors[NX_COLOR_BACKGROUND],
                       theme.colors[NX_COLOR_TEXT]) >= 4.5);
        CHECK(contrast(theme.colors[NX_COLOR_SURFACE],
                       theme.colors[NX_COLOR_TEXT]) >= 4.5);
        CHECK(contrast(theme.colors[NX_COLOR_ACCENT],
                       theme.colors[NX_COLOR_ACCENT_TEXT]) >= 4.5);
        CHECK(contrast(theme.colors[NX_COLOR_SELECTION],
                       theme.colors[NX_COLOR_SELECTION_TEXT]) >= 4.5);
        CHECK(theme.metrics[NX_METRIC_FONT_SIZE] > 0);
        CHECK(theme.metrics[NX_METRIC_LINE_HEIGHT] >= theme.metrics[NX_METRIC_FONT_SIZE]);
    }
    nx_theme dark, high;
    CHECK(nx_theme_preset(NX_THEME_DARK, &dark) == NX_OK);
    CHECK(nx_theme_preset(NX_THEME_HIGH_CONTRAST, &high) == NX_OK);
    CHECK(high.metrics[NX_METRIC_FOCUS_WIDTH] > dark.metrics[NX_METRIC_FOCUS_WIDTH]);
    CHECK(high.metrics[NX_METRIC_DISABLED_OPACITY] == 1.0f);
    CHECK(nx_theme_preset((nx_theme_kind)99, &dark) == NX_ERROR_ARGUMENT);
    return 0;
}

static int test_overrides_and_validation(void) {
    nx_theme dark, patch = {0};
    CHECK(nx_theme_preset(NX_THEME_DARK, &dark) == NX_OK);
    nx_color accent = {0.8f, 0.2f, 0.3f, 1};
    CHECK(nx_theme_set_color(&patch, NX_COLOR_ACCENT, accent) == NX_OK);
    CHECK(nx_theme_set_metric(&patch, NX_METRIC_RADIUS_MD, 12) == NX_OK);
    nx_theme merged;
    CHECK(nx_theme_resolve(&dark, &patch, &merged) == NX_OK);
    CHECK(same_color(merged.colors[NX_COLOR_ACCENT], accent));
    CHECK(same_color(merged.colors[NX_COLOR_TEXT], dark.colors[NX_COLOR_TEXT]));
    CHECK(merged.metrics[NX_METRIC_RADIUS_MD] == 12);
    CHECK(nx_theme_resolve(&dark, &patch, &dark) == NX_OK);
    CHECK(same_color(dark.colors[NX_COLOR_ACCENT], accent));
    CHECK(nx_theme_set_color(&patch, NX_COLOR_ERROR,
                             (nx_color){NAN, 0, 0, 1}) == NX_ERROR_ARGUMENT);
    CHECK(nx_theme_set_color(&patch, (nx_color_token)99, accent) == NX_ERROR_ARGUMENT);
    CHECK(nx_theme_set_metric(&patch, NX_METRIC_DISABLED_OPACITY, 1.1f) == NX_ERROR_ARGUMENT);
    CHECK(nx_theme_set_metric(&patch, NX_METRIC_FONT_SIZE, 0) == NX_ERROR_ARGUMENT);
    CHECK(nx_theme_set_metric(&patch, (nx_metric_token)99, 1) == NX_ERROR_ARGUMENT);
    CHECK(nx_theme_resolve(&dark, &patch, &merged) == NX_OK);
    patch.color_mask |= UINT64_C(1) << 63;
    CHECK(nx_theme_resolve(&dark, &patch, &merged) == NX_ERROR_ARGUMENT);
    return 0;
}

static int test_context_switch_scopes_and_render(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_theme light, high;
    CHECK(nx_theme_preset(NX_THEME_LIGHT, &light) == NX_OK);
    CHECK(nx_theme_preset(NX_THEME_HIGH_CONTRAST, &high) == NX_OK);
    CHECK(nx_context_set_theme(ctx, &light) == NX_OK);
    nx_color base;
    CHECK(nx_theme_color(ctx, NX_COLOR_ACCENT, &base) == NX_OK);
    CHECK(same_color(base, light.colors[NX_COLOR_ACCENT]));
    CHECK(nx_begin_frame(ctx, 1, 1, 0) == NX_OK);
    nx_theme outer = {0}, inner = {0};
    nx_color red = {1, 0, 0, 1}, green = {0, 1, 0, 1};
    CHECK(nx_theme_set_color(&outer, NX_COLOR_ACCENT, red) == NX_OK);
    CHECK(nx_theme_set_metric(&outer, NX_METRIC_SPACE_MD, 18) == NX_OK);
    CHECK(nx_theme_set_color(&inner, NX_COLOR_ACCENT, green) == NX_OK);
    CHECK(nx_push_theme(ctx, &outer) == NX_OK);
    nx_color color;
    float metric;
    CHECK(nx_theme_color(ctx, NX_COLOR_ACCENT, &color) == NX_OK);
    CHECK(same_color(color, red));
    CHECK(nx_theme_metric(ctx, NX_METRIC_SPACE_MD, &metric) == NX_OK && metric == 18);
    CHECK(nx_push_theme(ctx, &inner) == NX_OK);
    CHECK(nx_theme_color(ctx, NX_COLOR_ACCENT, &color) == NX_OK);
    CHECK(same_color(color, green));
    CHECK(nx_theme_metric(ctx, NX_METRIC_SPACE_MD, &metric) == NX_OK && metric == 18);
    CHECK(nx_pop_theme(ctx) == NX_OK);
    CHECK(nx_theme_color(ctx, NX_COLOR_ACCENT, &color) == NX_OK);
    CHECK(same_color(color, red));
    CHECK(nx_context_set_theme(ctx, &high) == NX_ERROR_STATE);
    CHECK(nx_end_frame(ctx) == NX_ERROR_STATE);
    CHECK(nx_pop_theme(ctx) == NX_OK);
    CHECK(nx_theme_color(ctx, NX_COLOR_ACCENT, &color) == NX_OK);
    CHECK(same_color(color, base));
    CHECK(nx_fill_rect(ctx, (nx_rect){0, 0, 1, 1}, color) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    const nx_command *commands;
    size_t count;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK);
    uint8_t pixel[4] = {0};
    CHECK(nx_headless_render(commands, count, pixel, 1, 1, 4,
                             (nx_color){0, 0, 0, 1}) == NX_OK);
    CHECK(abs((int)pixel[0] - 36) <= 1 && abs((int)pixel[1] - 87) <= 1 &&
          abs((int)pixel[2] - 166) <= 1);
    CHECK(nx_context_set_theme(ctx, &high) == NX_OK);
    CHECK(nx_theme_color(ctx, NX_COLOR_FOCUS, &color) == NX_OK);
    CHECK(same_color(color, high.colors[NX_COLOR_FOCUS]));
    nx_theme invalid = {0};
    invalid.color_mask = UINT64_C(1) << 63;
    CHECK(nx_context_set_theme(ctx, &invalid) == NX_ERROR_ARGUMENT);
    CHECK(nx_theme_color(ctx, NX_COLOR_FOCUS, &color) == NX_OK);
    CHECK(same_color(color, high.colors[NX_COLOR_FOCUS]));
    CHECK(nx_begin_frame(ctx, 1, 1, 0) == NX_OK);
    nx_theme empty = {0};
    for (unsigned i = 0; i < NX_MAX_THEME_DEPTH; ++i)
        CHECK(nx_push_theme(ctx, &empty) == NX_OK);
    CHECK(nx_push_theme(ctx, &empty) == NX_ERROR_LIMIT);
    for (unsigned i = 0; i < NX_MAX_THEME_DEPTH; ++i)
        CHECK(nx_pop_theme(ctx) == NX_OK);
    CHECK(nx_pop_theme(ctx) == NX_ERROR_STATE);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_context_set_theme(ctx, NULL) == NX_OK);
    CHECK(nx_theme_color(ctx, NX_COLOR_BACKGROUND, &color) == NX_OK);
    CHECK(color.r < 0.1f);
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_presets_and_contrast()) return 1;
    if (test_overrides_and_validation()) return 1;
    if (test_context_switch_scopes_and_render()) return 1;
    puts("theme tests passed");
    return 0;
}
