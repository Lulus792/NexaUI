#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct series_source {
    nx_series_point *points;
    size_t count, calls;
} series_source;

static nx_result sample(void *user, size_t index, nx_series_point *out) {
    series_source *source = (series_source *)user;
    ++source->calls;
    if (index >= source->count) return NX_ERROR_STATE;
    *out = source->points[index];
    return NX_OK;
}

static double now_ms(void) {
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) != TIME_UTC) return -1.0;
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static int compare_double(const void *a, const void *b) {
    double left = *(const double *)a, right = *(const double *)b;
    return (left > right) - (left < right);
}

static int run_case(nx_series_view *view, series_source *source,
                    const char *name, double x_min, double x_max) {
    enum { COLUMNS = 1920, ITERATIONS = 21 };
    nx_series_envelope output[COLUMNS];
    double samples[ITERATIONS];
    size_t result_count = 0;
    for (unsigned iteration = 0; iteration < ITERATIONS + 3u; ++iteration) {
        source->calls = 0;
        double start = now_ms();
        if (start < 0 || nx_series_decimate(view, x_min, x_max, COLUMNS,
                output, COLUMNS, &result_count) != NX_OK) return 1;
        double elapsed = now_ms() - start;
        if (elapsed < 0 || !result_count || source->calls > source->count + 50u)
            return 1;
        if (iteration >= 3u) samples[iteration - 3u] = elapsed;
    }
    qsort(samples, ITERATIONS, sizeof(samples[0]), compare_double);
    printf("%s: samples=%zu columns=%d output=%zu calls=%zu "
           "min/median/max=%.3f/%.3f/%.3f ms\n",
           name, source->count, COLUMNS, result_count, source->calls,
           samples[0], samples[ITERATIONS / 2], samples[ITERATIONS - 1]);
    return 0;
}

int main(void) {
#if defined(_MSC_VER)
    printf("compiler=MSVC %d ", _MSC_VER);
#elif defined(__clang__)
    printf("compiler=Clang %s ", __clang_version__);
#elif defined(__GNUC__)
    printf("compiler=GCC %s ", __VERSION__);
#endif
#if defined(_WIN32)
    puts("os=Windows");
#elif defined(__APPLE__)
    puts("os=macOS");
#elif defined(__linux__)
    puts("os=Linux");
#else
    puts("os=unknown");
#endif
    const size_t point_count = 1000000u;
    if (point_count > SIZE_MAX / sizeof(nx_series_point)) return 1;
    nx_series_point *points = malloc(point_count * sizeof(*points));
    if (!points) return 1;
    for (size_t i = 0; i < point_count; ++i) {
        points[i].x = (double)i;
        points[i].y = (double)(i % 101u) - 50.0;
    }
    points[500500].y = 1000000.0;
    series_source source = {points, point_count, 0};
    nx_series_view view = {point_count, &source, sample};
    int failed = run_case(&view, &source, "full", 0, 999999) ||
                 run_case(&view, &source, "10k viewport", 500000, 509999);
    free(points);
    return failed;
}
