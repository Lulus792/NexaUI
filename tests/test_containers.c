#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static nx_container_desc container(uint64_t id, uint64_t parent,
                                    nx_rect bounds, const char *name,
                                    size_t length) {
    nx_container_desc desc = {0};
    desc.id = id;
    desc.parent_id = parent;
    desc.bounds = bounds;
    desc.name = (nx_utf8_span){name, length};
    return desc;
}

static int click(nx_context *ctx, float x, float y) {
    nx_event event = {0};
    event.button = NX_BUTTON_LEFT;
    event.x = x;
    event.y = y;
    event.type = NX_EVENT_POINTER_DOWN;
    if (nx_context_queue_event(ctx, &event) != NX_OK) return 0;
    event.type = NX_EVENT_POINTER_UP;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int semantic(nx_context *ctx, uint64_t id, uint64_t parent,
                    nx_semantic_role role, uint32_t state_mask,
                    int expected_state) {
    uint32_t index = UINT32_MAX;
    nx_semantic_view view;
    return nx_semantics_find(ctx, id, &index) == NX_OK &&
           nx_semantics_get(ctx, index, &view) == NX_OK &&
           view.parent_id == parent && view.role == role &&
           !!(view.state & state_mask) == !!expected_state;
}

static int test_nested_layout(void) {
    nx_layout *layout = NULL;
    nx_context *ctx = NULL;
    CHECK(nx_layout_create(NULL, 0, &layout) == NX_OK);
    nx_layout_desc column = nx_layout_default_desc(NX_LAYOUT_COLUMN);
    column.padding = (nx_insets){4, 4, 4, 4};
    nx_layout_desc panel_layout = column;
    panel_layout.width.preferred = 100;
    panel_layout.width.has_preferred = 1;
    panel_layout.height.preferred = 70;
    panel_layout.height.has_preferred = 1;
    nx_layout_desc group_layout = column;
    group_layout.width.preferred = 80;
    group_layout.width.has_preferred = 1;
    group_layout.height.preferred = 40;
    group_layout.height.has_preferred = 1;
    nx_layout_desc leaf = nx_layout_default_desc(NX_LAYOUT_LEAF);
    leaf.width.preferred = 20;
    leaf.width.has_preferred = 1;
    leaf.height.preferred = 10;
    leaf.height.has_preferred = 1;
    uint32_t root, panel_node, group_node, button_node;
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &column, &root) == NX_OK);
    CHECK(nx_layout_add(layout, root, &panel_layout, &panel_node) == NX_OK);
    CHECK(nx_layout_add(layout, panel_node, &group_layout, &group_node) == NX_OK);
    CHECK(nx_layout_add(layout, group_node, &leaf, &button_node) == NX_OK);
    nx_size desired;
    CHECK(nx_layout_measure(layout, root, 120, 90, &desired) == NX_OK);
    CHECK(nx_layout_arrange(layout, root,
                            (nx_rect){0, 0, 120, 90}) == NX_OK);
    nx_rect panel_bounds, group_bounds, button_bounds;
    CHECK(nx_layout_get_rect(layout, panel_node, &panel_bounds) == NX_OK);
    CHECK(nx_layout_get_rect(layout, group_node, &group_bounds) == NX_OK);
    CHECK(nx_layout_get_rect(layout, button_node, &button_bounds) == NX_OK);
    CHECK(panel_bounds.x < group_bounds.x &&
          group_bounds.x < button_bounds.x);
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 120, 90, 0) == NX_OK);
    nx_container_desc panel = container(1, 0, panel_bounds, "Panel", 5);
    nx_container_desc group = container(2, 1, group_bounds, "Group", 5);
    CHECK(nx_panel(ctx, &panel) == NX_OK);
    CHECK(nx_group(ctx, &group) == NX_OK);
    nx_control_desc button = {0};
    button.id = 3;
    button.parent_id = 2;
    button.bounds = button_bounds;
    button.name = (nx_utf8_span){"Run", 3};
    nx_interaction_state state;
    CHECK(nx_button(ctx, &button, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(semantic(ctx, 1, 0, NX_ROLE_PANEL, 0, 0));
    CHECK(semantic(ctx, 2, 1, NX_ROLE_GROUP, 0, 0));
    CHECK(semantic(ctx, 3, 2, NX_ROLE_BUTTON, 0, 0));
    uint32_t index;
    nx_semantic_view view;
    CHECK(nx_semantics_find(ctx, 3, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK && view.depth == 2);
    const nx_command *commands;
    size_t count;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count >= 5);
    uint8_t pixels[120u * 90u * 4u];
    CHECK(nx_headless_render(commands, count, pixels, 120, 90,
                             120u * 4u, (nx_color){0, 0, 0, 0}) == NX_OK);
    CHECK(pixels[((size_t)panel_bounds.y * 120u +
                  (size_t)panel_bounds.x) * 4u + 3u] != 0);
    nx_context_destroy(ctx);
    nx_layout_destroy(layout);
    return 0;
}

static int test_collapsible(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_collapsible_desc desc = {0};
    desc.container = container(10, 0, (nx_rect){0, 0, 100, 70},
                                "Details", 7);
    desc.header_id = 11;
    desc.header_bounds = (nx_rect){0, 0, 100, 20};
    int expanded = 0;
    for (unsigned frame = 0; frame < 4; ++frame) {
        if (frame == 1 || frame == 2 || frame == 3)
            CHECK(click(ctx, 10, 10));
        if (frame == 3) desc.disabled = 1;
        CHECK(nx_begin_frame(ctx, 100, 70, 0) == NX_OK);
        nx_interaction_state state;
        CHECK(nx_collapsible_section(ctx, &desc, &expanded, &state) == NX_OK);
        CHECK(expanded == (frame == 1));
        if (frame == 1) {
            nx_control_desc child = {0};
            child.id = 12;
            child.parent_id = 10;
            child.bounds = (nx_rect){4, 25, 40, 15};
            child.name = (nx_utf8_span){"Child", 5};
            CHECK(nx_button(ctx, &child, &state) == NX_OK);
        }
        CHECK(nx_end_frame(ctx) == NX_OK);
        size_t count;
        CHECK(nx_semantics_count(ctx, &count) == NX_OK &&
              count == (frame == 1 ? 3u : 2u));
        CHECK(semantic(ctx, 10, 0, NX_ROLE_GROUP, NX_SEM_EXPANDED,
                       expanded));
        CHECK(semantic(ctx, 11, 10, NX_ROLE_BUTTON, NX_SEM_EXPANDED,
                       expanded));
        CHECK(semantic(ctx, 11, 10, NX_ROLE_BUTTON, NX_SEM_DISABLED,
                       desc.disabled));
    }
    nx_context_destroy(ctx);
    return 0;
}

static int test_tabs(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_container_desc bar = container(20, 0, (nx_rect){0, 0, 100, 24},
                                       "Pages", 5);
    nx_control_desc first = {0}, second = {0};
    first.id = 21;
    first.parent_id = 20;
    first.bounds = (nx_rect){0, 0, 45, 24};
    first.name = (nx_utf8_span){"First", 5};
    second.id = 22;
    second.parent_id = 20;
    second.bounds = (nx_rect){50, 0, 45, 24};
    second.name = (nx_utf8_span){"Second", 6};
    uint64_t selected = 21;
    for (unsigned frame = 0; frame < 4; ++frame) {
        if (frame == 1) CHECK(click(ctx, 60, 10));
        if (frame == 2) {
            CHECK(nx_focus_set(ctx, 21) == NX_OK);
            nx_event key = {0};
            key.key = NX_KEY_ENTER;
            key.type = NX_EVENT_KEY_DOWN;
            CHECK(nx_context_queue_event(ctx, &key) == NX_OK);
            key.type = NX_EVENT_KEY_UP;
            CHECK(nx_context_queue_event(ctx, &key) == NX_OK);
        }
        if (frame == 3) {
            second.disabled = 1;
            CHECK(click(ctx, 60, 10));
        }
        CHECK(nx_begin_frame(ctx, 100, 80, 0) == NX_OK);
        CHECK(nx_group(ctx, &bar) == NX_OK);
        nx_interaction_state state;
        CHECK(nx_tab(ctx, &first, &selected, &state) == NX_OK);
        CHECK(nx_tab(ctx, &second, &selected, &state) == NX_OK);
        nx_container_desc panel = container(selected == 21 ? 30 : 31, 20,
            (nx_rect){0, 30, 100, 50}, selected == 21 ? "First" : "Second",
            selected == 21 ? 5u : 6u);
        CHECK(nx_tab_panel(ctx, &panel) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        CHECK(selected == (frame == 1 ? 22u : 21u));
        CHECK(semantic(ctx, 21, 20, NX_ROLE_TAB, NX_SEM_SELECTED,
                       selected == 21));
        CHECK(semantic(ctx, 22, 20, NX_ROLE_TAB, NX_SEM_SELECTED,
                       selected == 22));
        CHECK(semantic(ctx, panel.id, 20, NX_ROLE_TAB_PANEL, 0, 0));
        uint32_t absent;
        CHECK(nx_semantics_find(ctx, selected == 21 ? 31 : 30,
                                &absent) == NX_ERROR_ARGUMENT);
        const nx_command *commands;
        size_t count, fills = 0;
        nx_color tab_colors[2] = {{0}, {0}};
        CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK);
        for (size_t i = 0; i < count; ++i)
            if (commands[i].type == NX_COMMAND_FILL_ROUND_RECT) {
                if (fills == 1 || fills == 2)
                    tab_colors[fills - 1u] = commands[i].color;
                ++fills;
            }
        CHECK(fills >= 4 &&
              memcmp(&tab_colors[0], &tab_colors[1],
                     sizeof(nx_color)) != 0);
    }
    nx_context_destroy(ctx);
    return 0;
}

typedef struct failure_probe {
    size_t fail_size;
    int fail_once;
} failure_probe;

static void *probe_allocate(void *user, size_t size) {
    failure_probe *probe = (failure_probe *)user;
    if (probe->fail_once && size == probe->fail_size) {
        probe->fail_once = 0;
        return NULL;
    }
    return malloc(size);
}

static void probe_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static int test_section_paint_retry(void) {
    failure_probe probe = {4u * sizeof(nx_command), 1};
    nx_context_desc context_desc = {0};
    context_desc.initial_command_capacity = 2;
    context_desc.allocator = (nx_allocator){&probe, probe_allocate,
                                             probe_deallocate};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&context_desc, &ctx) == NX_OK);
    nx_collapsible_desc desc = {0};
    desc.container = container(10, 0, (nx_rect){0, 0, 100, 70},
                                "Details", 7);
    desc.header_id = 11;
    desc.header_bounds = (nx_rect){0, 0, 100, 20};
    int expanded = 1;
    nx_interaction_state state;
    CHECK(nx_begin_frame(ctx, 100, 70, 0) == NX_OK);
    CHECK(nx_collapsible_section(ctx, &desc, &expanded, &state) ==
          NX_ERROR_MEMORY);
    CHECK(expanded == 1 && !state.clicked);
    CHECK(nx_collapsible_section(ctx, &desc, &expanded, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 2);
    CHECK(semantic(ctx, 10, 0, NX_ROLE_GROUP, NX_SEM_EXPANDED, 1));
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_nested_layout() || test_collapsible() || test_tabs() ||
        test_section_paint_retry()) return 1;
    puts("container tests passed");
    return 0;
}
