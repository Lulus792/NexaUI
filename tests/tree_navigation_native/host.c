/* Real ordinary Wayland window, using installed public SDK APIs only. */
#include "tree_navigation_fixture.h"
#include "nexaui/software.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(x) do { if (!(x)) { \
    fprintf(stderr, "tree-host:%d: %s\n", __LINE__, #x); okay = 0; goto cleanup; \
} } while (0)

int main(int argc, char **argv) {
    int okay = 1, dirty = 1, done = 0, announced = 0;
    FILE *file = NULL;
    unsigned char *bytes = NULL;
    nx_font *font = NULL;
    nx_font_collection *fonts = NULL;
    nx_context *ctx = NULL;
    nx_window *window = NULL;
    nx_software_view *renderer = NULL;
    nx_text_layout *labels[7] = {0};
    uint64_t keys = 0;
    model m = {1, 1, 0, 0, 15, 31, labels};
    const unsigned ids[7] = {11, 13, 15, 16, 17, 31, 32};
    setvbuf(stdout, NULL, _IONBF, 0);
    REQUIRE(argc == 2 && (file = fopen(argv[1], "rb")) != NULL);
    REQUIRE(!fseek(file, 0, SEEK_END));
    long length = ftell(file);
    REQUIRE(length > 0 && length < 64000000 && !fseek(file, 0, SEEK_SET));
    bytes = malloc((size_t)length);
    REQUIRE(bytes && fread(bytes, 1, (size_t)length, file) == (size_t)length);
    int closed = fclose(file); file = NULL;
    REQUIRE(!closed && nx_font_open(bytes, (size_t)length, NULL, &font) == NX_OK);
    REQUIRE(nx_font_collection_create(NULL, &fonts) == NX_OK &&
        nx_font_collection_add(fonts, font) == NX_OK && nx_context_create(NULL, &ctx) == NX_OK);
    for (unsigned i = 0; i < 7; ++i) {
        char name[32];
        int n = snprintf(name, sizeof(name), "Node %u", ids[i]);
        REQUIRE(n > 0 && (size_t)n < sizeof(name) && nx_text_layout_create(fonts,
            name, (size_t)n, NX_BIDI_LTR, 16, 92, &labels[i]) == NX_OK);
    }
    const char *title = "NexaUI native tree navigation";
    nx_window_desc desc = {title, 320, 240, 0, {0}};
    REQUIRE(nx_window_create(&desc, &window) == NX_OK && nx_software_view_open(window, &renderer) == NX_OK);
    while (!done) {
        nx_window_info info;
        REQUIRE(nx_window_get_info(window, &info) == NX_OK);
        if (info.should_close) break;
        if (dirty) {
            REQUIRE(nx_begin_frame(ctx, info.width, info.height, 0) == NX_OK);
            nx_semantic_desc root = {0};
            root.id = 1; root.role = NX_ROLE_WINDOW;
            root.name = (nx_utf8_span){title, strlen(title)};
            root.bounds = (nx_rect){0, 0, info.width, info.height};
            REQUIRE(nx_semantics_add(ctx, &root, NULL) == NX_OK &&
                tree_navigation_rows(ctx, &m, 1) && nx_end_frame(ctx) == NX_OK);
            nx_result published = nx_window_publish_semantics(window, ctx);
            REQUIRE(published == NX_OK || published == NX_ERROR_UNSUPPORTED);
            const nx_command *commands = NULL; size_t count = 0;
            nx_color background; nx_software_view_outcome outcome;
            REQUIRE(nx_frame_commands(ctx, &commands, &count) == NX_OK &&
                nx_theme_color(ctx, NX_COLOR_BACKGROUND, &background) == NX_OK &&
                nx_software_view_render(renderer, commands, count, background, &outcome) == NX_OK);
            dirty = outcome != NX_SOFTWARE_VIEW_READY;
            printf("VIEW focus=%" PRIu64 " root=%d child=%d selected=%" PRIu64 " other=%" PRIu64 " keys=%" PRIu64 "\n",
                nx_focus_id(ctx), m.root_open, m.child_open, m.selected, m.other_selected, keys);
            if (!dirty && !announced) { puts("TREE_READY"); announced = 1; }
        }
        REQUIRE(nx_window_poll(window, UINT32_MAX) == NX_OK);
        nx_event event; int available;
        do {
            REQUIRE(nx_window_next_event(window, &event, &available) == NX_OK);
            if (available) {
                if (event.type == NX_EVENT_KEY_DOWN || event.type == NX_EVENT_KEY_DOWN_CONSUMED ||
                    event.type == NX_EVENT_KEY_UP) ++keys;
                if (event.type == NX_EVENT_KEY_DOWN && event.key == NX_KEY_ESCAPE) done = 1;
                REQUIRE(nx_context_queue_event(ctx, &event) == NX_OK);
                dirty = 1;
            }
        } while (available);
        size_t routed = 0;
        REQUIRE(nx_window_route_events(window, ctx, &routed) == NX_OK);
        if (routed) dirty = 1;
    }
    printf("TREE_CLOSED selected=%" PRIu64 " other=%" PRIu64 "\n", m.selected, m.other_selected);
cleanup:
    if (window) {
        nx_result r = nx_window_publish_semantics(window, NULL);
        if (r != NX_OK && r != NX_ERROR_UNSUPPORTED) okay = 0;
    }
    if (renderer && nx_software_view_close(&renderer) != NX_OK) okay = 0;
    nx_context_destroy(ctx);
    if (window) nx_window_destroy(window);
    for (unsigned i = 0; i < 7; ++i) nx_text_layout_destroy(labels[i]);
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    if (file) fclose(file);
    free(bytes);
    return okay ? 0 : 1;
}
