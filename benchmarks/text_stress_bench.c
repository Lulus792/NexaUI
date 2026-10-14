#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_ms(void) {
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) != TIME_UTC) return -1.0;
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static int compare_double(const void *left, const void *right) {
    double a = *(const double *)left, b = *(const double *)right;
    return (a > b) - (a < b);
}

static int run(size_t minimum_length) {
    const char unit[] = "a\xcc\x81 \xe4\xb8\xad \xf0\x9f\x99\x82 \xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\n";
    const size_t unit_length = sizeof(unit) - 1u;
    size_t repeats = (minimum_length + unit_length - 1u) / unit_length;
    size_t length = repeats * unit_length;
    char *input = malloc(length + 1u);
    if (!input) return 1;
    for (size_t i = 0; i < repeats; ++i)
        memcpy(input + i * unit_length, unit, unit_length);
    input[length] = '\0';

    double samples[5];
    for (size_t sample = 0; sample < 5u; ++sample) {
        nx_text_buffer *buffer = NULL;
        if (nx_text_buffer_create(input, length, 0, &buffer) != NX_OK) {
            free(input);
            return 1;
        }
        double start = now_ms();
        if (start < 0) return 1;
        for (size_t edit = 0; edit < 32u; ++edit) {
            if (nx_text_buffer_move_cursor(buffer, NX_TEXT_MOVE_DOCUMENT_START, 0) != NX_OK ||
                nx_text_buffer_replace(buffer, "X", 1u) != NX_OK ||
                nx_text_buffer_delete_backward(buffer) != NX_OK) return 1;
        }
        samples[sample] = (now_ms() - start) / 32.0;
        const char *result = NULL;
        size_t result_length = 0;
        if (nx_text_buffer_text(buffer, &result, &result_length) != NX_OK ||
            result_length != length || memcmp(result, input, length) != 0) return 1;
        nx_text_buffer_destroy(buffer);
    }
    free(input);
    qsort(samples, 5u, sizeof(samples[0]), compare_double);
    printf("%zu bytes, 32 insert+delete pairs, 5 runs: min %.3f ms, median %.3f ms, max %.3f ms per pair\n",
           length, samples[0], samples[2], samples[4]);
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
    return run(4096u) || run(32768u) || run(131072u);
}
