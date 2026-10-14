#include "nexaui/nexaui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; \
} } while (0)

typedef struct source {
    const nx_series_point *points;
    size_t count, calls;
    size_t failure_index;
} source;

static nx_result read_point(void *user, size_t index, nx_series_point *out) {
    source *data = (source *)user;
    ++data->calls;
    if (index >= data->count || index == data->failure_index)
        return NX_ERROR_STATE;
    *out = data->points[index];
    return NX_OK;
}

static int test_envelopes(void) {
    const nx_series_point points[] = {
        {-1, 99}, {0, 4}, {0.2, 8}, {0.3, -30}, {0.4, 8},
        {1, 2}, {1.2, 5}, {1.3, -4}, {2, 1}, {3, 88}
    };
    source data = {points, 10, 0, SIZE_MAX};
    nx_series_view view = {10, &data, read_point};
    nx_series_envelope output[2] = {0};
    size_t count = 99;
    CHECK(nx_series_decimate(&view, 0, 2, 2, output, 2, &count) == NX_OK);
    CHECK(count == 2 && output[0].column == 0 && output[1].column == 1);
    CHECK(output[0].sample_count == 4 && output[0].first.index == 1 &&
          output[0].last.index == 4 && output[0].minimum.index == 3 &&
          output[0].minimum.point.y == -30 && output[0].maximum.index == 2);
    CHECK(output[1].sample_count == 4 && output[1].first.index == 5 &&
          output[1].last.index == 8 && output[1].minimum.index == 7 &&
          output[1].maximum.index == 6);
    CHECK(output[1].last.point.x == 2);
    CHECK(data.calls < 40);
    return 0;
}

static int test_sparse_and_duplicates(void) {
    const nx_series_point points[] = {
        {1, 2}, {1, -2}, {1, 4}, {100, 3}, {100, 3}
    };
    source data = {points, 5, 0, SIZE_MAX};
    nx_series_view view = {5, &data, read_point};
    nx_series_envelope output[5] = {0};
    size_t count = 99;
    CHECK(nx_series_decimate(&view, 0, 100, 5, output, 5, &count) == NX_OK);
    CHECK(count == 2 && output[0].column == 0 && output[1].column == 4);
    CHECK(output[0].sample_count == 3 && output[0].first.index == 0 &&
          output[0].last.index == 2 && output[0].minimum.index == 1 &&
          output[0].maximum.index == 2);
    CHECK(output[1].sample_count == 2 && output[1].minimum.index == 3 &&
          output[1].maximum.index == 3);
    CHECK(nx_series_decimate(&view, 5, 6, 5, output, 5, &count) == NX_OK &&
          count == 0);
    return 0;
}

static int test_validation(void) {
    nx_series_point points[] = {{0, 0}, {1, 1}, {2, 2}};
    source data = {points, 3, 0, SIZE_MAX};
    nx_series_view view = {3, &data, read_point};
    nx_series_envelope output[4];
    memset(output, 0xA5, sizeof(output));
    nx_series_envelope old = output[0];
    size_t count = 99;
    CHECK(nx_series_decimate(&view, 0, 2, 4, output, 3, &count) ==
          NX_ERROR_LIMIT && count == 0 &&
          memcmp(&old, &output[0], sizeof(old)) == 0 && data.calls == 0);
    CHECK(nx_series_decimate(&view, 0, 2, 4, NULL, 4, &count) == NX_ERROR_LIMIT);
    CHECK(nx_series_decimate(&view, 0, 0, 4, output, 4, &count) ==
          NX_ERROR_ARGUMENT);
    CHECK(nx_series_decimate(&view, -INFINITY, 2, 4, output, 4, &count) ==
          NX_ERROR_ARGUMENT);
    CHECK(nx_series_decimate(&view, -1e308, 1e308, 4, output, 4, &count) ==
          NX_ERROR_ARGUMENT);
    CHECK(nx_series_decimate(&view, 0, 2, 0, output, 4, &count) ==
          NX_ERROR_ARGUMENT);
    CHECK(nx_series_decimate(NULL, 0, 2, 4, output, 4, &count) ==
          NX_ERROR_ARGUMENT);
    CHECK(nx_series_decimate(&view, 0, 2, 4, output, 4, NULL) ==
          NX_ERROR_ARGUMENT);
    data.failure_index = 1;
    CHECK(nx_series_decimate(&view, 0, 2, 4, output, 4, &count) ==
          NX_ERROR_STATE && count == 0);
    data.failure_index = SIZE_MAX;
    points[1].y = NAN;
    CHECK(nx_series_decimate(&view, 0, 2, 4, output, 4, &count) ==
          NX_ERROR_ARGUMENT && count == 0);
    points[1].y = 1;
    points[1].x = -1;
    CHECK(nx_series_decimate(&view, -2, 2, 4, output, 4, &count) ==
          NX_ERROR_ARGUMENT && count == 0);
    view.count = 0;
    data.calls = 0;
    CHECK(nx_series_decimate(&view, 0, 2, 4, NULL, 0, &count) == NX_OK &&
          count == 0 && data.calls == 0);
    return 0;
}

typedef struct linear_source { size_t count, calls; } linear_source;
static nx_result read_linear(void *user, size_t index, nx_series_point *out) {
    linear_source *source = (linear_source *)user;
    ++source->calls;
    if (index >= source->count) return NX_ERROR_STATE;
    out->x = (double)index;
    out->y = index == 500500 ? 1000000.0 : (double)(index % 17u);
    return NX_OK;
}

static int test_large_narrow_window(void) {
    linear_source data = {10000000u, 0};
    nx_series_view view = {data.count, &data, read_linear};
    nx_series_envelope output[100] = {0};
    size_t count = 0;
    CHECK(nx_series_decimate(&view, 500000, 500999, 100, output, 100,
                             &count) == NX_OK);
    CHECK(count == 100 && data.calls < 1100);
    CHECK(output[50].maximum.index == 500500 &&
          output[50].maximum.point.y == 1000000.0);
    return 0;
}

int main(void) {
    if (test_envelopes() || test_sparse_and_duplicates() ||
        test_validation() || test_large_narrow_window()) return 1;
    return 0;
}
