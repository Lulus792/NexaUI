#include "scene.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "reflow %d: %s\n", __LINE__, #x); return 0; } } while (0)

static int frame(cloud_scene *s, float w, float h) {
    /* Preference activation applies next frame; that frame's measured focus
       reveal is visible in the following one. Native runners repaint too. */
    return cloud_frame(s, w, h, 0) && cloud_frame(s, w, h, 0) && cloud_frame(s, w, h, 0);
}

static int node(cloud_scene *s, uint64_t id, nx_semantic_view *view) {
    uint32_t index;
    return nx_semantics_find(cloud_context(s), id, &index) == NX_OK &&
        nx_semantics_get(cloud_context(s), index, view) == NX_OK;
}

static int key(cloud_scene *s, nx_key code) {
    nx_event event = {0}; event.type = NX_EVENT_KEY_DOWN; event.key = code;
    CHECK(nx_context_queue_event(cloud_context(s), &event) == NX_OK);
    event.type = NX_EVENT_KEY_UP;
    CHECK(nx_context_queue_event(cloud_context(s), &event) == NX_OK); return 1;
}

static int activate(cloud_scene *s, uint64_t id, float w, float h) {
    CHECK(nx_focus_set(cloud_context(s), id) == NX_OK);
    CHECK(frame(s, w, h)); CHECK(key(s, NX_KEY_ENTER)); CHECK(frame(s, w, h)); return 1;
}

static int contained(nx_rect inner, nx_rect outer) {
    return inner.x >= outer.x - 1 && inner.y >= outer.y - 1 &&
        inner.x + inner.width <= outer.x + outer.width + 1 &&
        inner.y + inner.height <= outer.y + outer.height + 1;
}

/* Raw text must fit every ancestor horizontally, and fit its content panels
   vertically up to the first scroll viewport. Scrolling may intentionally
   place that viewport's children outside its vertical visible interval. */
static int horizontal_text(cloud_scene *s) {
    size_t count; CHECK(nx_semantics_count(cloud_context(s), &count) == NX_OK);
    for (size_t i = 0; i < count; ++i) {
        nx_semantic_view text; CHECK(nx_semantics_get(cloud_context(s), (uint32_t)i, &text) == NX_OK);
        if (text.role != NX_ROLE_TEXT) continue;
        uint64_t parent = text.parent_id;
        int scroll_child = 0;
        while (parent) {
            nx_semantic_view ancestor; CHECK(node(s, parent, &ancestor));
            if (ancestor.role == NX_ROLE_SCROLL_AREA) scroll_child = 1;
            if (text.bounds.x < ancestor.bounds.x - 1 ||
                text.bounds.x + text.bounds.width > ancestor.bounds.x + ancestor.bounds.width + 1) {
                fprintf(stderr, "text overflows ancestor %llu: %.*s (x=%.1f width=%.1f, parent %.1f/%.1f)\n",
                    (unsigned long long)parent, (int)text.name.length, text.name.data,
                    text.bounds.x, text.bounds.width, ancestor.bounds.x, ancestor.bounds.width);
                return 0;
            }
            if (!scroll_child && (text.bounds.y < ancestor.bounds.y - 1 ||
                text.bounds.y + text.bounds.height > ancestor.bounds.y + ancestor.bounds.height + 1)) {
                fprintf(stderr, "text overflows content panel %llu vertically: %.*s\n",
                    (unsigned long long)parent, (int)text.name.length, text.name.data);
                return 0;
            }
            parent = ancestor.parent_id;
        }
    }
    return 1;
}

static int consume_bitmaps(cloud_scene *s) {
    const nx_command *commands; size_t count;
    CHECK(nx_frame_commands(cloud_context(s), &commands, &count) == NX_OK);
    volatile unsigned checksum = 0;
    for (size_t i = 0; i < count; ++i) if (commands[i].type == NX_COMMAND_BITMAP) {
        nx_bitmap_view b = commands[i].bitmap;
        size_t pixel_bytes = b.format == NX_BITMAP_A8 ? 1u : 4u;
        for (uint32_t y = 0; y < b.height; ++y)
            for (size_t x = 0; x < (size_t)b.width * pixel_bytes; ++x)
                checksum += ((const unsigned char *)b.pixels)[(size_t)y * b.stride_bytes + x];
    }
    (void)checksum; return 1;
}

static int visible(cloud_scene *s, uint64_t id, uint64_t viewport) {
    nx_semantic_view action, area; CHECK(node(s, id, &action)); CHECK(node(s, viewport, &area));
    if (!contained(action.bounds, area.bounds)) {
        fprintf(stderr, "control %llu %.1f/%.1f %.1fx%.1f outside viewport %llu %.1f/%.1f %.1fx%.1f\n",
            (unsigned long long)id, action.bounds.x, action.bounds.y, action.bounds.width, action.bounds.height,
            (unsigned long long)viewport, area.bounds.x, area.bounds.y, area.bounds.width, area.bounds.height);
        return 0;
    }
    return 1;
}

static int capture(cloud_scene *s, const char *prefix, const char *name, float w, float h) {
    char path[1024]; CHECK(snprintf(path, sizeof(path), "%s-%s.ppm", prefix, name) < (int)sizeof(path));
    CHECK(cloud_export(s, path, w, h)); CHECK(horizontal_text(s)); return 1;
}

static int run(cloud_scene *s, const char *prefix) {
    CHECK(!cloud_set_text_scale(s, NAN) && !cloud_set_text_scale(s, INFINITY));
    CHECK(!cloud_set_text_scale(s, .99f) && !cloud_set_text_scale(s, 2.01f));
    CHECK(cloud_text_scale(s) == 1);
    CHECK(frame(s, 800, 640));
    CHECK(activate(s, 15, 800, 640));
    CHECK(nx_focus_set(cloud_context(s), 401) == NX_OK); CHECK(frame(s, 800, 640));
    for (uint64_t id = 402; id <= 404; ++id) {
        CHECK(key(s, NX_KEY_TAB)); CHECK(frame(s, 800, 640));
        CHECK(nx_focus_id(cloud_context(s)) == id); CHECK(visible(s, id, 410));
    }
    CHECK(activate(s, 404, 800, 640)); CHECK(cloud_text_scale(s) == 1.5f);
    CHECK(visible(s, 404, 410)); CHECK(horizontal_text(s));
    CHECK(activate(s, 404, 800, 640)); CHECK(cloud_text_scale(s) == 2);
    CHECK(nx_focus_id(cloud_context(s)) == 404); CHECK(visible(s, 404, 410));
    CHECK(capture(s, prefix, "settings-200", 800, 640));
    CHECK(activate(s, 401, 800, 640)); CHECK(capture(s, prefix, "light-200", 800, 640));
    CHECK(activate(s, 401, 800, 640));
    CHECK(activate(s, 403, 800, 640)); CHECK(horizontal_text(s)); CHECK(visible(s, 502, 500));
    CHECK(key(s, NX_KEY_ESCAPE)); CHECK(frame(s, 800, 640)); CHECK(nx_focus_id(cloud_context(s)) == 403);
    CHECK(activate(s, 10, 800, 640)); CHECK(capture(s, prefix, "overview-200", 800, 640));
    CHECK(nx_focus_set(cloud_context(s), 101) == NX_OK); CHECK(key(s, NX_KEY_END)); CHECK(frame(s, 800, 640));
    CHECK(capture(s, prefix, "quick-actions-200", 800, 640)); CHECK(visible(s, 174, 100));
    CHECK(activate(s, 12, 800, 640)); CHECK(horizontal_text(s));
    CHECK(nx_focus_set(cloud_context(s), 604) == NX_OK); CHECK(frame(s, 800, 640)); CHECK(visible(s, 604, 610));
    CHECK(activate(s, 604, 800, 640)); CHECK(horizontal_text(s));
    CHECK(activate(s, 11, 800, 640)); CHECK(capture(s, prefix, "files-200", 800, 640));
    CHECK(activate(s, 1000, 800, 640)); CHECK(activate(s, 203, 800, 640));
    CHECK(capture(s, prefix, "details-200", 800, 640)); CHECK(visible(s, 502, 500));
    CHECK(nx_focus_set(cloud_context(s), 306) == NX_OK); CHECK(frame(s, 800, 640)); CHECK(visible(s, 306, 310));
    CHECK(capture(s, prefix, "description-200", 800, 640));
    CHECK(key(s, NX_KEY_ESCAPE)); CHECK(frame(s, 800, 640)); CHECK(nx_focus_id(cloud_context(s)) == 203);
    CHECK(activate(s, 42, 800, 640)); CHECK(capture(s, prefix, "list-200", 800, 640));
    CHECK(activate(s, 200, 800, 640)); CHECK(horizontal_text(s)); CHECK(visible(s, 502, 500));
    CHECK(key(s, NX_KEY_ESCAPE)); CHECK(frame(s, 800, 640)); CHECK(nx_focus_id(cloud_context(s)) == 200);
    /* Actual minimum, and a smaller resize fallback, rather than only 800px. */
    CHECK(frame(s, 480, 400)); CHECK(horizontal_text(s));
    CHECK(activate(s, 203, 480, 400)); CHECK(horizontal_text(s)); CHECK(visible(s, 502, 500));
    CHECK(key(s, NX_KEY_ESCAPE)); CHECK(frame(s, 480, 400));
    CHECK(activate(s, 15, 480, 400)); CHECK(visible(s, 15, 35)); CHECK(horizontal_text(s));
    CHECK(activate(s, 404, 480, 400)); CHECK(cloud_text_scale(s) == 1); CHECK(visible(s, 404, 410));
    const float sizes[][2] = {{480,400}, {640,480}, {800,640}, {1440,1040}, {2560,1600}};
    for (unsigned scale = 0; scale < 3; ++scale) {
        CHECK(cloud_set_text_scale(s, 1 + .5f * (float)scale));
        for (unsigned size = 0; size < 5; ++size) {
            for (uint64_t page = 10; page < 16; ++page) {
                CHECK(frame(s, sizes[size][0], sizes[size][1]));
                CHECK(activate(s, page, sizes[size][0], sizes[size][1]));
                CHECK(horizontal_text(s)); CHECK(consume_bitmaps(s));
            }
        }
    }
    /* More than the 512-entry cache capacity, using distinct widths. All
       command streams are consumed before the next frame may evict stamps. */
    CHECK(cloud_set_text_scale(s, 2));
    CHECK(nx_focus_set(cloud_context(s), 404) == NX_OK);
    for (unsigned i = 0; i < 120; ++i) {
        CHECK(frame(s, 640.0f + (float)i, 480)); CHECK(horizontal_text(s)); CHECK(consume_bitmaps(s));
        CHECK(visible(s, 404, 410));
    }
    CHECK(frame(s, 470, 380));
    puts("NexaCloud text reflow: preference, focus reveal, modal return, horizontal containment and resize cache passed.");
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    cloud_scene *s = cloud_create(argv[1]);
    if (!s) return 1;
    int okay = run(s, argv[2]); cloud_destroy(s); return okay ? 0 : 1;
}
