#include "nexaui/nexaui.h"

#include <math.h>

static nx_result sample_at(const nx_series_view *view, size_t index,
                           nx_series_point *point) {
    nx_result result = view->sample(view->user, index, point);
    if (result != NX_OK) return result;
    return isfinite(point->x) && isfinite(point->y) ? NX_OK : NX_ERROR_ARGUMENT;
}

static nx_result bound_index(const nx_series_view *view, double x,
                             int upper, size_t *out_index) {
    size_t low = 0, high = view->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2u;
        nx_series_point point;
        nx_result result = sample_at(view, middle, &point);
        if (result != NX_OK) return result;
        if (point.x < x || (upper && point.x == x)) low = middle + 1u;
        else high = middle;
    }
    *out_index = low;
    return NX_OK;
}

nx_result nx_series_decimate(const nx_series_view *view,
                             double x_min, double x_max, uint32_t columns,
                             nx_series_envelope *out_envelopes,
                             size_t capacity, size_t *out_count) {
    if (!out_count) return NX_ERROR_ARGUMENT;
    *out_count = 0;
    double span = x_max - x_min;
    if (!view || !view->sample || !columns || !isfinite(x_min) ||
        !isfinite(x_max) || !isfinite(span) || span <= 0.0)
        return NX_ERROR_ARGUMENT;
    if (!view->count) return NX_OK;
    if (!out_envelopes || capacity < columns) return NX_ERROR_LIMIT;

    size_t begin = 0, end = 0;
    nx_result result = bound_index(view, x_min, 0, &begin);
    if (result != NX_OK) return result;
    result = bound_index(view, x_max, 1, &end);
    if (result != NX_OK) return result;
    if (end < begin) return NX_ERROR_ARGUMENT;

    size_t used = 0;
    double previous_x = 0.0;
    for (size_t index = begin; index < end; ++index) {
        nx_series_point point;
        result = sample_at(view, index, &point);
        if (result != NX_OK) return result;
        if (point.x < x_min || point.x > x_max ||
            (index != begin && point.x < previous_x)) return NX_ERROR_ARGUMENT;
        previous_x = point.x;

        double position = (point.x - x_min) / span;
        uint32_t column = position >= 1.0 ? columns - 1u :
                          (uint32_t)(position * (double)columns);
        nx_series_mark mark = {index, point};
        if (!used || out_envelopes[used - 1u].column != column) {
            nx_series_envelope *envelope = &out_envelopes[used++];
            envelope->column = column;
            envelope->sample_count = 1u;
            envelope->first = envelope->last = envelope->minimum =
                envelope->maximum = mark;
        } else {
            nx_series_envelope *envelope = &out_envelopes[used - 1u];
            if (envelope->sample_count == SIZE_MAX) return NX_ERROR_LIMIT;
            ++envelope->sample_count;
            envelope->last = mark;
            if (point.y < envelope->minimum.point.y) envelope->minimum = mark;
            if (point.y > envelope->maximum.point.y) envelope->maximum = mark;
        }
    }
    *out_count = used;
    return NX_OK;
}
