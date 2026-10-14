#include "internal.h"

#include <string.h>

#define NX_RGB(r, g, b) {(r) / 255.0f, (g) / 255.0f, (b) / 255.0f, 1.0f}
#define NX_RGBA(r, g, b, a) {(r) / 255.0f, (g) / 255.0f, (b) / 255.0f, (a)}

_Static_assert(NX_COLOR_TOKEN_COUNT < 64 && NX_METRIC_TOKEN_COUNT < 64,
               "Theme masks require fewer than 64 tokens per group");

static const nx_color nx_palettes[4][NX_COLOR_TOKEN_COUNT] = {
    {
        NX_RGB(18, 21, 27), NX_RGB(28, 33, 41), NX_RGB(38, 44, 54),
        NX_RGB(243, 245, 248), NX_RGB(181, 190, 202), NX_RGB(84, 95, 109),
        NX_RGB(128, 175, 255), NX_RGB(11, 18, 32), NX_RGB(48, 55, 67),
        NX_RGB(62, 75, 94), NX_RGB(41, 46, 55), NX_RGB(142, 152, 165),
        NX_RGB(255, 200, 87), NX_RGB(255, 139, 139), NX_RGB(122, 220, 165),
        NX_RGB(255, 210, 122), NX_RGB(48, 91, 147), NX_RGB(255, 255, 255),
        NX_RGBA(0, 0, 0, 0.45f)
    },
    {
        NX_RGB(245, 247, 250), NX_RGB(255, 255, 255), NX_RGB(233, 238, 243),
        NX_RGB(24, 34, 47), NX_RGB(82, 97, 112), NX_RGB(156, 169, 183),
        NX_RGB(36, 87, 166), NX_RGB(255, 255, 255), NX_RGB(228, 236, 247),
        NX_RGB(209, 224, 244), NX_RGB(232, 236, 240), NX_RGB(100, 114, 130),
        NX_RGB(139, 74, 0), NX_RGB(179, 38, 30), NX_RGB(23, 111, 67),
        NX_RGB(138, 90, 0), NX_RGB(200, 220, 255), NX_RGB(24, 34, 47),
        NX_RGBA(0, 0, 0, 0.20f)
    },
    {
        NX_RGB(0, 0, 0), NX_RGB(0, 0, 0), NX_RGB(17, 17, 17),
        NX_RGB(255, 255, 255), NX_RGB(255, 255, 255), NX_RGB(255, 255, 255),
        NX_RGB(255, 235, 59), NX_RGB(0, 0, 0), NX_RGB(51, 51, 51),
        NX_RGB(85, 85, 85), NX_RGB(0, 0, 0), NX_RGB(221, 221, 221),
        NX_RGB(0, 255, 255), NX_RGB(255, 112, 112), NX_RGB(130, 255, 130),
        NX_RGB(255, 235, 59), NX_RGB(255, 235, 59), NX_RGB(0, 0, 0),
        NX_RGBA(0, 0, 0, 0.0f)
    },
    {
        NX_RGB(2, 15, 28), NX_RGB(4, 25, 42), NX_RGB(7, 34, 56),
        NX_RGB(237, 244, 255), NX_RGB(169, 191, 216), NX_RGB(22, 65, 94),
        NX_RGB(0, 195, 255), NX_RGB(0, 18, 31), NX_RGB(9, 46, 75),
        NX_RGB(12, 61, 96), NX_RGB(13, 32, 47), NX_RGB(134, 155, 176),
        NX_RGB(93, 222, 255), NX_RGB(255, 126, 136), NX_RGB(0, 224, 179),
        NX_RGB(255, 207, 108), NX_RGB(5, 47, 81), NX_RGB(211, 246, 255),
        NX_RGBA(0, 3, 12, 0.5f)
    }
};

static const float nx_metrics[4][NX_METRIC_TOKEN_COUNT] = {
    {4, 8, 12, 20, 32, 1, 2, 4, 8, 14, 4, 14, 20, 0.55f},
    {4, 8, 12, 20, 32, 1, 2, 4, 8, 14, 4, 14, 20, 0.55f},
    {4, 8, 12, 20, 36, 2, 3, 0, 0, 0, 0, 16, 23, 1.0f},
    {4, 8, 16, 24, 44, 1, 2, 6, 10, 0, 0, 15, 22, 0.6f}
};

static int nx_valid_metric(nx_metric_token token, float value) {
    if (!isfinite(value) || value < 0) return 0;
    if (token == NX_METRIC_DISABLED_OPACITY) return value <= 1;
    if (token == NX_METRIC_CONTROL_HEIGHT || token == NX_METRIC_FONT_SIZE ||
        token == NX_METRIC_LINE_HEIGHT) return value > 0;
    return 1;
}

static int nx_valid_theme(const nx_theme *theme) {
    if (!theme ||
        (theme->color_mask >> NX_COLOR_TOKEN_COUNT) ||
        (theme->metric_mask >> NX_METRIC_TOKEN_COUNT)) return 0;
    for (unsigned i = 0; i < NX_COLOR_TOKEN_COUNT; ++i)
        if ((theme->color_mask & (UINT64_C(1) << i)) &&
            !nx_valid_color(theme->colors[i])) return 0;
    for (unsigned i = 0; i < NX_METRIC_TOKEN_COUNT; ++i)
        if ((theme->metric_mask & (UINT64_C(1) << i)) &&
            !nx_valid_metric((nx_metric_token)i, theme->metrics[i])) return 0;
    return 1;
}

nx_result nx_theme_preset(nx_theme_kind kind, nx_theme *out_theme) {
    if (!out_theme || kind < NX_THEME_DARK || kind > NX_THEME_MIDNIGHT)
        return NX_ERROR_ARGUMENT;
    nx_theme theme = {0};
    memcpy(theme.colors, nx_palettes[kind - 1], sizeof(theme.colors));
    memcpy(theme.metrics, nx_metrics[kind - 1], sizeof(theme.metrics));
    theme.color_mask = (UINT64_C(1) << NX_COLOR_TOKEN_COUNT) - 1u;
    theme.metric_mask = (UINT64_C(1) << NX_METRIC_TOKEN_COUNT) - 1u;
    *out_theme = theme;
    return NX_OK;
}

nx_result nx_theme_set_color(nx_theme *theme, nx_color_token token,
                             nx_color color) {
    if (!theme || token < 0 || token >= NX_COLOR_TOKEN_COUNT ||
        !nx_valid_color(color)) return NX_ERROR_ARGUMENT;
    theme->colors[token] = color;
    theme->color_mask |= UINT64_C(1) << token;
    return NX_OK;
}

nx_result nx_theme_set_metric(nx_theme *theme, nx_metric_token token,
                              float value) {
    if (!theme || token < 0 || token >= NX_METRIC_TOKEN_COUNT ||
        !nx_valid_metric(token, value)) return NX_ERROR_ARGUMENT;
    theme->metrics[token] = value;
    theme->metric_mask |= UINT64_C(1) << token;
    return NX_OK;
}

nx_result nx_theme_resolve(const nx_theme *parent,
                           const nx_theme *overrides, nx_theme *out_theme) {
    if (!out_theme || !nx_valid_theme(parent) || !nx_valid_theme(overrides))
        return NX_ERROR_ARGUMENT;
    nx_theme result = *parent;
    for (unsigned i = 0; i < NX_COLOR_TOKEN_COUNT; ++i)
        if (overrides->color_mask & (UINT64_C(1) << i))
            result.colors[i] = overrides->colors[i];
    for (unsigned i = 0; i < NX_METRIC_TOKEN_COUNT; ++i)
        if (overrides->metric_mask & (UINT64_C(1) << i))
            result.metrics[i] = overrides->metrics[i];
    result.color_mask |= overrides->color_mask;
    result.metric_mask |= overrides->metric_mask;
    *out_theme = result;
    return NX_OK;
}
