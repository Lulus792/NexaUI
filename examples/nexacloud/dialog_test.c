#include "scene.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "dialog %d: %s\n", __LINE__, #x); return 0; } } while (0)

static int frame(cloud_scene *s, float w) {
    return cloud_frame(s, w, 800, 0) && cloud_frame(s, w, 800, 0);
}

static int key(cloud_scene *s, nx_key k) {
    nx_event e = {0}; e.type = NX_EVENT_KEY_DOWN; e.key = k;
    CHECK(nx_context_queue_event(cloud_context(s), &e) == NX_OK);
    e.type = NX_EVENT_KEY_UP;
    CHECK(nx_context_queue_event(cloud_context(s), &e) == NX_OK);
    return 1;
}

static int activate(cloud_scene *s, uint64_t id, float w) {
    CHECK(nx_focus_set(cloud_context(s), id) == NX_OK);
    CHECK(frame(s, w)); CHECK(key(s, NX_KEY_ENTER)); CHECK(frame(s, w));
    return 1;
}

static int node(cloud_scene *s, uint64_t id, nx_semantic_view *v) {
    uint32_t index;
    return nx_semantics_find(cloud_context(s), id, &index) == NX_OK &&
        nx_semantics_get(cloud_context(s), index, v) == NX_OK;
}

static int run(cloud_scene *s, const char *which) {
    const int resize = !strcmp(which, "details-resize");
    float w = resize ? 800.0f : 1440.0f;
    CHECK(frame(s, w)); CHECK(activate(s, 11, w)); CHECK(activate(s, 1000, w));
    if (resize) {
        CHECK(activate(s, 203, w));
        CHECK(frame(s, 1440)); w = 1440;
    } else {
        uint64_t action = !strcmp(which, "share") ? 304 :
            !strcmp(which, "preview") ? 303 : 200;
        CHECK(activate(s, action, w));
    }
    nx_semantic_view modal, close;
    CHECK(node(s, 500, &modal)); CHECK(node(s, 502, &close));
    CHECK(!(close.state & NX_SEM_DISABLED));
    CHECK(close.bounds.x >= modal.bounds.x && close.bounds.y >= modal.bounds.y);
    CHECK(close.bounds.x + close.bounds.width <= modal.bounds.x + modal.bounds.width + 1);
    CHECK(close.bounds.y + close.bounds.height <= modal.bounds.y + modal.bounds.height + 1);
    /* A responsive transition must keep the open dialog usable, in both directions. */
    CHECK(frame(s, 800)); CHECK(frame(s, 1440));
    CHECK(key(s, NX_KEY_ESCAPE)); CHECK(frame(s, w));
    CHECK(!node(s, 500, &modal));
    CHECK(nx_focus_id(cloud_context(s)) == (resize ? 203u :
        !strcmp(which, "share") ? 304u : !strcmp(which, "preview") ? 303u : 200u));
    CHECK(activate(s, 304, w)); CHECK(node(s, 500, &modal));
    CHECK(activate(s, 502, w)); CHECK(!node(s, 500, &modal));
    CHECK(nx_focus_id(cloud_context(s)) == 304);
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) return 2;
    const char *cases[] = {"share", "preview", "upload", "details-resize"};
    unsigned executed = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (argc == 3 && strcmp(argv[2], cases[i])) continue;
        cloud_scene *s = cloud_create(argv[1]);
        if (!s) return 1;
        int okay = run(s, cases[i]); cloud_destroy(s);
        if (!okay) return 1;
        ++executed;
        printf("dialog %s: open, responsive resize, dismiss, focus return and reopen passed\n", cases[i]);
    }
    return executed ? 0 : 2;
}
