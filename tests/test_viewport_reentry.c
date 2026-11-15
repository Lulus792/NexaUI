#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "viewport reentry phase %d line %d: %s\n", phase, __LINE__, #x); \
    goto done; \
} } while (0)

enum { ON_RESIZE = 1, ON_ACQUIRE, ON_COMPLETE, ON_ROLLBACK, ON_SUSPEND };
typedef struct producer {
    nx_viewport_surface *surface, *other_surface;
    nx_context *other_context;
    nx_viewport_desc desc;
    uint8_t pixels[8u * 8u * 4u];
    uint32_t width, height;
    unsigned releases, attempts;
    int phase, leased, malformed;
    nx_result destroy_result, complete_result, draw_result, independent_result;
} producer;

static nx_result render(nx_context *context) {
    const nx_command *commands = NULL;
    size_t count = 0;
    uint8_t pixels[8u * 8u * 4u];
    nx_result result = nx_end_frame(context);
    if (result == NX_OK) result = nx_frame_commands(context, &commands, &count);
    if (result == NX_OK) result = nx_headless_render(commands, count, pixels,
        8, 8, 8u * 4u, (nx_color){0, 0, 0, 1});
    return result;
}

static void reenter(producer *p, int phase) {
    if (p->phase != phase || p->attempts) return;
    ++p->attempts;
    p->destroy_result = nx_viewport_surface_destroy(p->surface);
    /* The old implementation frees during resize/acquire/rollback. Return an
       ordinary callback failure in that case, avoiding a diagnostic UAF here. */
    if (p->destroy_result == NX_OK) { p->surface = NULL; return; }
    p->complete_result = nx_viewport_surface_complete(p->surface);
    /* Bound the old completion callback's recursive release to one reentry. */
    if (p->complete_result != NX_ERROR_STATE) return;
    nx_interaction_state state;
    p->draw_result = nx_viewport(p->other_context, p->surface, &p->desc, &state);
    if (p->draw_result != NX_ERROR_STATE) return;
    /* The guard is local to this surface. Another surface/context can still
       acquire, render and complete on the same owner thread. */
    p->independent_result = nx_viewport(p->other_context, p->other_surface,
                                       &p->desc, &state);
    if (p->independent_result == NX_OK)
        p->independent_result = render(p->other_context);
    if (p->independent_result == NX_OK)
        p->independent_result = nx_viewport_surface_complete(p->other_surface);
}

static nx_result resize(void *user, uint32_t width, uint32_t height) {
    producer *p = user;
    reenter(p, width && height ? ON_RESIZE : ON_SUSPEND);
    if (!p->surface) return NX_ERROR_PLATFORM;
    if (p->leased || width > 8 || height > 8) return NX_ERROR_STATE;
    p->width = width; p->height = height;
    for (size_t i = 0; i < sizeof(p->pixels); i += 4) {
        p->pixels[i] = 20; p->pixels[i + 1] = 80;
        p->pixels[i + 2] = 180; p->pixels[i + 3] = 255;
    }
    return NX_OK;
}

static nx_result acquire(void *user, nx_bitmap_view *bitmap) {
    producer *p = user;
    reenter(p, ON_ACQUIRE);
    if (!p->surface) return NX_ERROR_PLATFORM;
    if (p->leased || !p->width || !p->height) return NX_ERROR_STATE;
    p->leased = 1;
    *bitmap = (nx_bitmap_view){p->pixels,
        p->width + (unsigned)p->malformed, p->height,
        (size_t)p->width * 4u, NX_BITMAP_RGBA8};
    return NX_OK;
}

static void release(void *user) {
    producer *p = user;
    ++p->releases;
    reenter(p, p->malformed ? ON_ROLLBACK : ON_COMPLETE);
    p->leased = 0;
}

static int run(int phase) {
    int result = 1;
    nx_context *context = NULL, *other = NULL;
    producer p = {0}, independent = {0};
    p.desc = (nx_viewport_desc){101, 0, {0, 0, 4, 4}, {"Content", 7}, 1, 0};
    p.destroy_result = p.complete_result = p.draw_result =
        p.independent_result = NX_ERROR_COMMAND;
    nx_viewport_source source = {&p, resize, acquire, release};
    nx_viewport_source other_source = {&independent, resize, acquire, release};
    CHECK(nx_context_create(NULL, &context) == NX_OK);
    CHECK(nx_context_create(NULL, &other) == NX_OK);
    CHECK(nx_viewport_surface_create(&source, NULL, &p.surface) == NX_OK);
    CHECK(nx_viewport_surface_create(&other_source, NULL,
                                     &independent.surface) == NX_OK);
    p.other_surface = independent.surface; p.other_context = other;
    nx_interaction_state state;
    if (phase == ON_SUSPEND) {
        CHECK(nx_begin_frame(context, 8, 8, 0) == NX_OK);
        CHECK(nx_viewport(context, p.surface, &p.desc, &state) == NX_OK);
        CHECK(render(context) == NX_OK);
        CHECK(nx_viewport_surface_complete(p.surface) == NX_OK);
    }
    p.phase = phase; p.malformed = phase == ON_ROLLBACK;
    nx_viewport_desc desc = p.desc;
    if (phase == ON_SUSPEND) desc.bounds.width = 0;
    CHECK(nx_begin_frame(context, 8, 8, 0) == NX_OK);
    CHECK(nx_begin_frame(other, 8, 8, 0) == NX_OK);
    nx_result drawn = nx_viewport(context, p.surface, &desc, &state);
    CHECK(p.surface != NULL);
    if (phase == ON_ROLLBACK) {
        CHECK(drawn == NX_ERROR_ARGUMENT && p.releases == 1 && !p.leased);
        CHECK(render(context) == NX_OK);
        size_t count = 1;
        CHECK(nx_semantics_count(context, &count) == NX_OK && count == 0);
        p.malformed = 0;
        CHECK(nx_begin_frame(context, 8, 8, 0) == NX_OK);
        CHECK(nx_viewport(context, p.surface, &desc, &state) == NX_OK);
    } else CHECK(drawn == NX_OK);
    CHECK(render(context) == NX_OK);
    if (phase != ON_SUSPEND)
        CHECK(nx_viewport_surface_complete(p.surface) == NX_OK);
    CHECK(p.attempts == 1);
    CHECK(p.destroy_result == NX_ERROR_STATE);
    CHECK(p.complete_result == NX_ERROR_STATE);
    CHECK(p.draw_result == NX_ERROR_STATE);
    CHECK(p.independent_result == NX_OK && independent.releases == 1);
    CHECK(p.releases == (phase == ON_ROLLBACK ? 2u : 1u) && !p.leased);
    /* Both successful and failed callback paths leave the wrapper reusable. */
    CHECK(nx_begin_frame(context, 8, 8, 0) == NX_OK);
    CHECK(nx_viewport(context, p.surface, &p.desc, &state) == NX_OK);
    CHECK(render(context) == NX_OK);
    CHECK(nx_viewport_surface_complete(p.surface) == NX_OK);
    result = 0;
done:
    if (p.surface) {
        (void)nx_viewport_surface_complete(p.surface);
        if (nx_viewport_surface_destroy(p.surface) != NX_OK) result = 1;
    }
    if (independent.surface) {
        (void)nx_viewport_surface_complete(independent.surface);
        if (nx_viewport_surface_destroy(independent.surface) != NX_OK) result = 1;
    }
    nx_context_destroy(other); nx_context_destroy(context);
    if (!result) printf("viewport callback phase %d: guarded, independent, reusable\n", phase);
    return result;
}

int main(int argc, char **argv) {
    if (argc == 2) {
        char *end = NULL;
        long phase = strtol(argv[1], &end, 10);
        if (!end || *end || phase < ON_RESIZE || phase > ON_SUSPEND) return 2;
        return run((int)phase);
    }
    if (argc != 1) return 2;
    for (int phase = ON_RESIZE; phase <= ON_SUSPEND; ++phase)
        if (run(phase)) return 1;
    return 0;
}
