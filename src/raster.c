#include "nexaui/nexaui.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define NX_RASTER_MAX_EDGES 262144u
#define NX_RASTER_MAX_CURVE_DEPTH 16u
#define NX_RASTER_SAMPLES 4u
#define NX_RASTER_MAX_INTERSECTION_TESTS UINT64_C(64000000)

typedef struct nx_raster_point {
    double x, y;
} nx_raster_point;

typedef struct nx_raster_edge {
    double x0, y0, x1, y1;
} nx_raster_edge;

typedef struct nx_raster_crossing {
    double x;
    int winding;
} nx_raster_crossing;

typedef struct nx_raster_build {
    nx_allocator allocator;
    nx_raster_edge *edges;
    size_t edge_count, edge_capacity;
    double scale, baseline_x, baseline_y;
} nx_raster_build;

static void *nx_raster_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_raster_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static nx_raster_point nx_raster_midpoint(nx_raster_point a, nx_raster_point b) {
    return (nx_raster_point){(a.x + b.x) * 0.5, (a.y + b.y) * 0.5};
}

static nx_result nx_raster_append_edge(nx_raster_build *build,
                                        nx_raster_point a, nx_raster_point b) {
    if (a.y == b.y) return NX_OK;
    if (build->edge_count >= NX_RASTER_MAX_EDGES) return NX_ERROR_LIMIT;
    if (build->edge_count == build->edge_capacity) {
        size_t next_capacity = build->edge_capacity
                               ? build->edge_capacity * 2u : 64u;
        if (next_capacity > NX_RASTER_MAX_EDGES)
            next_capacity = NX_RASTER_MAX_EDGES;
        size_t bytes = next_capacity * sizeof(nx_raster_edge);
        nx_raster_edge *next = build->allocator.allocate(build->allocator.user,
                                                         bytes);
        if (!next) return NX_ERROR_MEMORY;
        if (build->edge_count)
            memcpy(next, build->edges,
                   build->edge_count * sizeof(nx_raster_edge));
        if (build->edges)
            build->allocator.deallocate(build->allocator.user, build->edges,
                build->edge_capacity * sizeof(nx_raster_edge));
        build->edges = next;
        build->edge_capacity = next_capacity;
    }
    build->edges[build->edge_count++] = (nx_raster_edge){a.x, a.y, b.x, b.y};
    return NX_OK;
}

static nx_result nx_raster_flatten_quadratic(nx_raster_build *build,
                                              nx_raster_point a,
                                              nx_raster_point control,
                                              nx_raster_point b,
                                              unsigned depth) {
    nx_raster_point middle = nx_raster_midpoint(a, b);
    if (fabs(control.x - middle.x) <= 0.25 &&
        fabs(control.y - middle.y) <= 0.25)
        return nx_raster_append_edge(build, a, b);
    if (depth >= NX_RASTER_MAX_CURVE_DEPTH) return NX_ERROR_LIMIT;
    nx_raster_point left = nx_raster_midpoint(a, control);
    nx_raster_point right = nx_raster_midpoint(control, b);
    nx_raster_point center = nx_raster_midpoint(left, right);
    nx_result result = nx_raster_flatten_quadratic(build, a, left,
                                                    center, depth + 1u);
    if (result != NX_OK) return result;
    return nx_raster_flatten_quadratic(build, center, right, b, depth + 1u);
}

static nx_raster_point nx_raster_map_point(const nx_raster_build *build,
                                           const nx_glyph_point *point) {
    return (nx_raster_point){
        build->baseline_x + (double)point->x * build->scale,
        build->baseline_y - (double)point->y * build->scale
    };
}

static nx_result nx_raster_append_contour(nx_raster_build *build,
                                           const nx_glyph_point *points,
                                           size_t first, size_t last) {
    nx_raster_point start;
    if (points[first].on_curve)
        start = nx_raster_map_point(build, &points[first]);
    else if (points[last].on_curve)
        start = nx_raster_map_point(build, &points[last]);
    else
        start = nx_raster_midpoint(nx_raster_map_point(build, &points[last]),
                                    nx_raster_map_point(build, &points[first]));
    nx_raster_point current = start, control = {0};
    int pending_control = 0;
    for (size_t i = first; i <= last; ++i) {
        nx_raster_point point = nx_raster_map_point(build, &points[i]);
        nx_result result;
        if (points[i].on_curve) {
            result = pending_control
                     ? nx_raster_flatten_quadratic(build, current, control,
                                                   point, 0u)
                     : nx_raster_append_edge(build, current, point);
            if (result != NX_OK) return result;
            current = point;
            pending_control = 0;
        } else if (pending_control) {
            nx_raster_point implied = nx_raster_midpoint(control, point);
            result = nx_raster_flatten_quadratic(build, current, control,
                                                  implied, 0u);
            if (result != NX_OK) return result;
            current = implied;
            control = point;
        } else {
            control = point;
            pending_control = 1;
        }
    }
    return pending_control
           ? nx_raster_flatten_quadratic(build, current, control, start, 0u)
           : nx_raster_append_edge(build, current, start);
}

static int nx_raster_compare_crossings(const void *left, const void *right) {
    double a = ((const nx_raster_crossing *)left)->x;
    double b = ((const nx_raster_crossing *)right)->x;
    return a < b ? -1 : a > b ? 1 : 0;
}

static void nx_raster_fill_span(uint8_t *row, uint32_t width,
                                double left, double right) {
    double sample_width = (double)width * NX_RASTER_SAMPLES;
    double first = left * NX_RASTER_SAMPLES - 0.5;
    double finish = right * NX_RASTER_SAMPLES - 0.5;
    if (finish <= 0.0 || first >= sample_width) return;
    if (first < 0.0) first = 0.0;
    if (finish > sample_width) finish = sample_width;
    size_t begin = (size_t)ceil(first);
    size_t end = (size_t)ceil(finish);
    for (size_t sample = begin; sample < end; ++sample)
        ++row[sample / NX_RASTER_SAMPLES];
}

static nx_result nx_raster_validate_outline(const nx_glyph_outline_buffer *outline,
                                             double scale, double baseline_x,
                                             double baseline_y) {
    if (!outline || !isfinite(scale) || scale <= 0.0 ||
        !isfinite(baseline_x) || !isfinite(baseline_y) ||
        fabs(baseline_x) > 1e9 || fabs(baseline_y) > 1e9)
        return NX_ERROR_ARGUMENT;
    if (outline->point_count > 65536u ||
        outline->contour_count > outline->point_count ||
        outline->point_count > outline->point_capacity ||
        outline->contour_count > outline->contour_capacity ||
        (outline->point_count && !outline->points) ||
        (outline->contour_count && !outline->contour_ends) ||
        ((outline->point_count == 0u) != (outline->contour_count == 0u)))
        return NX_ERROR_ARGUMENT;
    size_t first = 0;
    for (size_t i = 0; i < outline->contour_count; ++i) {
        size_t end = outline->contour_ends[i];
        if (end < first || end >= outline->point_count)
            return NX_ERROR_ARGUMENT;
        first = end + 1u;
    }
    if (first != outline->point_count) return NX_ERROR_ARGUMENT;
    for (size_t i = 0; i < outline->point_count; ++i) {
        const nx_glyph_point *point = &outline->points[i];
        double x = baseline_x + (double)point->x * scale;
        double y = baseline_y - (double)point->y * scale;
        if (point->on_curve > 1u) return NX_ERROR_ARGUMENT;
        if (!isfinite(x) || !isfinite(y) ||
            fabs(x) > 1e9 || fabs(y) > 1e9) return NX_ERROR_LIMIT;
    }
    return NX_OK;
}

nx_result nx_rasterize_glyph_a8(const nx_glyph_outline_buffer *outline,
                                double pixels_per_unit,
                                double baseline_x, double baseline_y,
                                uint8_t *pixels, uint32_t width,
                                uint32_t height, size_t stride_bytes,
                                size_t pixel_buffer_size,
                                const nx_allocator *allocator) {
    nx_result result = nx_raster_validate_outline(outline, pixels_per_unit,
                                                   baseline_x, baseline_y);
    if (result != NX_OK) return result;
    if (width > NX_GLYPH_RASTER_MAX_DIMENSION ||
        height > NX_GLYPH_RASTER_MAX_DIMENSION ||
        (uint64_t)width * height > NX_GLYPH_RASTER_MAX_PIXELS)
        return NX_ERROR_LIMIT;
    if (stride_bytes < width ||
        (width && height && (!pixels ||
         ((size_t)(height - 1u) > (SIZE_MAX - width) / stride_bytes) ||
         (size_t)(height - 1u) * stride_bytes + width > pixel_buffer_size)))
        return NX_ERROR_ARGUMENT;
    if (!width || !height) return NX_OK;
    nx_allocator selected = allocator ? *allocator : (nx_allocator){0};
    if ((selected.allocate == NULL) != (selected.deallocate == NULL))
        return NX_ERROR_ARGUMENT;
    if (!selected.allocate) {
        selected.allocate = nx_raster_allocate;
        selected.deallocate = nx_raster_deallocate;
        selected.user = NULL;
    }
    nx_raster_build build = {0};
    build.allocator = selected;
    build.scale = pixels_per_unit;
    build.baseline_x = baseline_x;
    build.baseline_y = baseline_y;
    size_t first = 0;
    for (size_t i = 0; i < outline->contour_count; ++i) {
        size_t last = outline->contour_ends[i];
        result = nx_raster_append_contour(&build, outline->points, first, last);
        if (result != NX_OK) break;
        first = last + 1u;
    }
    nx_raster_crossing *crossings = NULL;
    if (result == NX_OK &&
        (uint64_t)height * NX_RASTER_SAMPLES * build.edge_count >
            NX_RASTER_MAX_INTERSECTION_TESTS)
        result = NX_ERROR_LIMIT;
    if (result == NX_OK && build.edge_count) {
        crossings = selected.allocate(selected.user,
                                      build.edge_count * sizeof(*crossings));
        if (!crossings) result = NX_ERROR_MEMORY;
    }
    if (result == NX_OK) {
        for (uint32_t y = 0; y < height; ++y) {
            uint8_t *row = pixels + (size_t)y * stride_bytes;
            memset(row, 0, width);
            for (unsigned sub_y = 0; sub_y < NX_RASTER_SAMPLES; ++sub_y) {
                double sample_y = y + (sub_y + 0.5) / NX_RASTER_SAMPLES;
                size_t count = 0;
                for (size_t edge_index = 0; edge_index < build.edge_count;
                     ++edge_index) {
                    const nx_raster_edge *edge = &build.edges[edge_index];
                    if ((edge->y0 <= sample_y && sample_y < edge->y1) ||
                        (edge->y1 <= sample_y && sample_y < edge->y0)) {
                        crossings[count].x = edge->x0 +
                            (sample_y - edge->y0) *
                            (edge->x1 - edge->x0) / (edge->y1 - edge->y0);
                        crossings[count].winding = edge->y0 < edge->y1 ? 1 : -1;
                        ++count;
                    }
                }
                if (count > 1u)
                    qsort(crossings, count, sizeof(*crossings),
                          nx_raster_compare_crossings);
                int winding = 0;
                double previous_x = 0.0;
                for (size_t i = 0; i < count;) {
                    double x = crossings[i].x;
                    if (winding) nx_raster_fill_span(row, width, previous_x, x);
                    do {
                        winding += crossings[i].winding;
                        ++i;
                    } while (i < count && crossings[i].x == x);
                    previous_x = x;
                }
            }
            for (uint32_t x = 0; x < width; ++x)
                row[x] = (uint8_t)(((unsigned)row[x] * 255u + 8u) / 16u);
        }
    }
    if (crossings)
        selected.deallocate(selected.user, crossings,
                            build.edge_count * sizeof(*crossings));
    if (build.edges)
        selected.deallocate(selected.user, build.edges,
                            build.edge_capacity * sizeof(nx_raster_edge));
    return result;
}
