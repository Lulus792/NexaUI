/* Compare the compiled direct classifiers with the existing general range
 * lookup over the entire code-point domain, including surrogate code points.
 * Including this private TU exposes its unchanged range reference without
 * adding a test API or linking a second copy into the library. */
#include "../src/unicode_data.c"
#include <stdio.h>
#include <stdlib.h>

/* Keep comparisons active in Release builds that define NDEBUG. */
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(EXIT_FAILURE); \
} } while (0)

#define CHECK_PROPERTY(name, scalar) \
    CHECK(nx_ucd_##name(scalar) == nx_ucd_find(nx_ucd_##name##_ranges, \
        sizeof(nx_ucd_##name##_ranges) / sizeof(nx_ucd_##name##_ranges[0]), scalar))
static void check_scalar(uint32_t scalar) {
    CHECK_PROPERTY(gcb, scalar);
    CHECK_PROPERTY(wb, scalar);
    CHECK_PROPERTY(lb, scalar);
    CHECK_PROPERTY(gc, scalar);
    CHECK_PROPERTY(eaw, scalar);
    CHECK_PROPERTY(ep, scalar);
    CHECK_PROPERTY(incb, scalar);
    CHECK_PROPERTY(di, scalar);
    CHECK_PROPERTY(bidi, scalar);
    CHECK_PROPERTY(script, scalar);
}
int main(void) {
    for (uint32_t scalar = 0; scalar <= UINT32_C(0x10ffff); ++scalar)
        check_scalar(scalar);
    check_scalar(UINT32_C(0x110000));
    check_scalar(UINT32_MAX);
    puts("all ten Unicode classifiers match the general range lookup for all code points and out-of-range controls");
    return 0;
}
