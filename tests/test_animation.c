#include "nexaui/nexaui.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static int near(double a, double b) {
    return fabs(a - b) < 1e-9;
}

static int test_retarget_and_frame_timing(void) {
    nx_context *ctx = NULL;
    nx_transition transition = {0};
    double value = -1;
    int active = -1, reduced = -1;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_context_get_reduced_motion(ctx, &reduced) == NX_OK && !reduced);
    CHECK(nx_transition_update(ctx, &transition, 0, 1, NX_EASING_LINEAR,
                               &value, &active) == NX_ERROR_STATE);
    CHECK(nx_begin_frame(ctx, 10, 10, 0) == NX_OK);
    CHECK(nx_transition_update(ctx, &transition, 0, 1, NX_EASING_LINEAR,
                               &value, &active) == NX_OK && near(value, 0) && !active);
    CHECK(nx_transition_update(ctx, &transition, 10, 1, NX_EASING_LINEAR,
                               &value, &active) == NX_OK && near(value, 0) && active);
    CHECK(nx_transition_update(ctx, &transition, 10, 1, NX_EASING_LINEAR,
                               &value, &active) == NX_OK && near(value, 0) && active);
    CHECK(nx_context_set_reduced_motion(ctx, 1) == NX_ERROR_STATE);
    CHECK(nx_end_frame(ctx) == NX_OK);

    CHECK(nx_begin_frame(ctx, 10, 10, 0.25) == NX_OK);
    CHECK(nx_transition_update(ctx, &transition, 10, 1, NX_EASING_LINEAR,
                               &value, &active) == NX_OK && near(value, 2.5) && active);
    CHECK(nx_transition_update(ctx, &transition, 10, 1, NX_EASING_LINEAR,
                               &value, &active) == NX_OK && near(value, 2.5) && active);
    CHECK(nx_end_frame(ctx) == NX_OK);

    CHECK(nx_begin_frame(ctx, 10, 10, 0.25) == NX_OK);
    CHECK(nx_transition_update(ctx, &transition, 0, 1, NX_EASING_SMOOTHSTEP,
                               &value, &active) == NX_OK && near(value, 5) && active);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 10, 10, 0.25) == NX_OK);
    CHECK(nx_transition_update(ctx, &transition, 0, 1, NX_EASING_SMOOTHSTEP,
                               &value, &active) == NX_OK && near(value, 4.21875) && active);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_context_set_reduced_motion(ctx, 1) == NX_OK);
    CHECK(nx_context_get_reduced_motion(ctx, &reduced) == NX_OK && reduced);
    CHECK(nx_begin_frame(ctx, 10, 10, 0.1) == NX_OK);
    CHECK(nx_transition_update(ctx, &transition, 0, 1, NX_EASING_SMOOTHSTEP,
                               &value, &active) == NX_OK && near(value, 0) && !active);
    CHECK(nx_end_frame(ctx) == NX_OK);

    CHECK(nx_context_set_reduced_motion(ctx, 0) == NX_OK);
    CHECK(nx_begin_frame(ctx, 10, 10, 0) == NX_OK);
    CHECK(nx_transition_update(ctx, &transition, 10, 0, NX_EASING_LINEAR,
                               &value, &active) == NX_OK && near(value, 10) && !active);
    CHECK(nx_transition_reset(&transition, -DBL_MAX) == NX_OK);
    CHECK(nx_transition_update(ctx, &transition, DBL_MAX, 1, NX_EASING_LINEAR,
                               &value, &active) == NX_OK && value == -DBL_MAX && active);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 10, 10, 0.5) == NX_OK);
    CHECK(nx_transition_update(ctx, &transition, DBL_MAX, 1, NX_EASING_LINEAR,
                               &value, &active) == NX_OK && near(value, 0) && active);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_context_destroy(ctx);
    return 0;
}

static int test_validation_and_independence(void) {
    nx_context *first = NULL, *second = NULL;
    nx_transition a = {0}, b = {0};
    double value = 0;
    int active = 0;
    CHECK(nx_context_create(NULL, &first) == NX_OK);
    CHECK(nx_context_create(NULL, &second) == NX_OK);
    CHECK(nx_context_set_reduced_motion(first, 2) == NX_ERROR_ARGUMENT);
    CHECK(nx_context_set_reduced_motion(first, 1) == NX_OK);
    CHECK(nx_begin_frame(first, 10, 10, 0) == NX_OK);
    CHECK(nx_begin_frame(second, 10, 10, 0) == NX_OK);
    CHECK(nx_transition_update(first, &a, 0, 1, NX_EASING_LINEAR,
                               &value, &active) == NX_OK);
    CHECK(nx_transition_update(second, &b, 0, 1, NX_EASING_LINEAR,
                               &value, &active) == NX_OK);
    CHECK(nx_transition_update(first, &a, 10, 1, NX_EASING_LINEAR,
                               &value, &active) == NX_OK && near(value, 10) && !active);
    CHECK(nx_transition_update(second, &b, 10, 1, NX_EASING_LINEAR,
                               &value, &active) == NX_OK && near(value, 0) && active);
    nx_transition saved = b;
    CHECK(nx_transition_update(second, &b, NAN, 1, NX_EASING_LINEAR,
                               &value, &active) == NX_ERROR_ARGUMENT);
    CHECK(nx_transition_update(second, &b, 10, -1, NX_EASING_LINEAR,
                               &value, &active) == NX_ERROR_ARGUMENT);
    CHECK(nx_transition_update(second, &b, 10, 1, (nx_easing)99,
                               &value, &active) == NX_ERROR_ARGUMENT);
    CHECK(b.value == saved.value && b.target == saved.target &&
          b.elapsed == saved.elapsed);
    CHECK(nx_transition_reset(&b, NAN) == NX_ERROR_ARGUMENT &&
          b.value == saved.value);
    CHECK(nx_end_frame(first) == NX_OK && nx_end_frame(second) == NX_OK);
    nx_context_destroy(first);
    nx_context_destroy(second);
    return 0;
}

static int test_visual_and_semantic_feedback(void) {
    nx_context *ctx = NULL;
    nx_transition width = {0};
    double value;
    int active;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_transition_reset(&width, 0) == NX_OK);
    for (unsigned frame = 0; frame < 3; ++frame) {
        if (frame == 2) CHECK(nx_context_set_reduced_motion(ctx, 1) == NX_OK);
        CHECK(nx_begin_frame(ctx, 10, 2, frame == 1 ? 0.5 : 0) == NX_OK);
        CHECK(nx_transition_update(ctx, &width, 10, 1, NX_EASING_LINEAR,
                                   &value, &active) == NX_OK);
        CHECK(near(value, frame == 0 ? 0 : frame == 1 ? 5 : 10));
        nx_semantic_desc progress = {0};
        progress.id = 1;
        progress.role = NX_ROLE_PROGRESS;
        progress.bounds = (nx_rect){0, 0, 10, 2};
        progress.name = (nx_utf8_span){"Load", 4};
        progress.value = (nx_utf8_span){"100%", 4};
        CHECK(nx_semantics_add(ctx, &progress, NULL) == NX_OK);
        CHECK(nx_fill_rect(ctx, (nx_rect){0, 0, (float)value, 2},
                           (nx_color){1, 0, 0, 1}) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        uint32_t index;
        nx_semantic_view view;
        CHECK(nx_semantics_find(ctx, 1, &index) == NX_OK);
        CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
              view.value.length == 4 && view.value.data[0] == '1');
        const nx_command *commands = NULL;
        size_t count = 0;
        uint8_t pixels[10u * 2u * 4u];
        CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK);
        CHECK(nx_headless_render(commands, count, pixels, 10, 2, 10u * 4u,
                                 (nx_color){0, 0, 0, 0}) == NX_OK);
        CHECK((pixels[(1u * 10u + 8u) * 4u + 3u] != 0) == (frame == 2));
    }
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_retarget_and_frame_timing()) return 1;
    if (test_validation_and_independence()) return 1;
    if (test_visual_and_semantic_feedback()) return 1;
    puts("animation tests passed");
    return 0;
}
