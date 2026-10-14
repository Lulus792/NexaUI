#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>

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

static nx_menu_item_desc item(uint64_t id, uint64_t parent,
                              nx_rect bounds, int disabled, int checkable) {
    nx_menu_item_desc desc = {0};
    desc.control.id = id;
    desc.control.parent_id = parent;
    desc.control.bounds = bounds;
    desc.control.name = (nx_utf8_span){"Action", 6};
    desc.control.disabled = disabled;
    desc.checkable = checkable;
    return desc;
}

static int key(nx_context *ctx, nx_key code) {
    nx_event event = {0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = code;
    if (nx_context_queue_event(ctx, &event) != NX_OK) return 0;
    event.type = NX_EVENT_KEY_UP;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int click(nx_context *ctx, float x, float y) {
    nx_event event = {0};
    event.type = NX_EVENT_POINTER_DOWN;
    event.button = NX_BUTTON_LEFT;
    event.x = x;
    event.y = y;
    if (nx_context_queue_event(ctx, &event) != NX_OK) return 0;
    event.type = NX_EVENT_POINTER_UP;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int semantic(nx_context *ctx, uint64_t id, uint64_t parent,
                    nx_semantic_role role, uint32_t mask, int set) {
    uint32_t index;
    nx_semantic_view view;
    return nx_semantics_find(ctx, id, &index) == NX_OK &&
           nx_semantics_get(ctx, index, &view) == NX_OK &&
           view.parent_id == parent && view.role == role &&
           !!(view.state & mask) == !!set;
}

static int test_menu_navigation(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_container_desc bar = container(1, 0, (nx_rect){0, 0, 200, 24},
                                       "Menu bar", 8);
    nx_menu_item_desc file = item(2, 1, (nx_rect){0, 0, 70, 24}, 0, 0);
    nx_menu_item_desc edit = item(3, 1, (nx_rect){70, 0, 70, 24}, 0, 0);
    nx_container_desc toolbar = container(4, 0,
        (nx_rect){0, 115, 200, 24}, "Tools", 5);
    nx_control_desc tool_button = {0};
    tool_button.id = 5;
    tool_button.parent_id = 4;
    tool_button.bounds = (nx_rect){5, 116, 60, 20};
    tool_button.name = (nx_utf8_span){"Save", 4};
    nx_container_desc status = container(6, 0,
        (nx_rect){0, 140, 200, 20}, "Ready", 5);
    nx_overlay_desc popup = {0};
    popup.panel = container(10, 0, (nx_rect){0, 24, 150, 90}, "File", 4);
    popup.backdrop_id = 11;
    popup.dismiss_on_escape = 1;
    nx_container_desc menu = container(12, 10,
        (nx_rect){0, 24, 150, 90}, "File commands", 13);
    nx_menu_item_desc open = item(13, 12, (nx_rect){0, 30, 150, 24}, 0, 1);
    nx_menu_item_desc unavailable = item(14, 12,
        (nx_rect){0, 56, 150, 24}, 1, 0);
    nx_menu_item_desc quit = item(15, 12,
        (nx_rect){0, 82, 150, 24}, 0, 0);
    int checked = 0;
    int show_menu = 0;
    for (unsigned frame = 0; frame < 11; ++frame) {
        if (frame == 1) CHECK(key(ctx, NX_KEY_RIGHT));
        if (frame == 2) CHECK(key(ctx, NX_KEY_LEFT));
        if (frame == 3) CHECK(click(ctx, 10, 10));
        if (frame == 4) CHECK(key(ctx, NX_KEY_DOWN));
        if (frame == 5) CHECK(key(ctx, NX_KEY_UP));
        if (frame == 6) CHECK(key(ctx, NX_KEY_END));
        if (frame == 7) CHECK(key(ctx, NX_KEY_HOME));
        if (frame == 8) CHECK(key(ctx, NX_KEY_ENTER));
        if (frame == 9) CHECK(key(ctx, NX_KEY_ESCAPE));
        CHECK(nx_begin_frame(ctx, 200, 160, 0) == NX_OK);
        CHECK(nx_menu_bar(ctx, &bar) == NX_OK);
        nx_interaction_state file_state, state;
        CHECK(nx_menu_item(ctx, &file, NULL, &file_state) == NX_OK);
        CHECK(nx_menu_item(ctx, &edit, NULL, &state) == NX_OK);
        if (file_state.clicked) show_menu = 1;
        CHECK(nx_toolbar(ctx, &toolbar) == NX_OK);
        CHECK(nx_button(ctx, &tool_button, &state) == NX_OK);
        CHECK(nx_status_bar(ctx, &status) == NX_OK);
        int dismissed = 0;
        if (show_menu) {
            CHECK(nx_popup_begin(ctx, &popup, &dismissed) == NX_OK);
            CHECK(nx_menu(ctx, &menu) == NX_OK);
            nx_interaction_state open_state;
            CHECK(nx_menu_item(ctx, &open, &checked, &open_state) == NX_OK);
            CHECK(nx_menu_item(ctx, &unavailable, NULL, &state) == NX_OK);
            CHECK(nx_menu_item(ctx, &quit, NULL, &state) == NX_OK);
            if (frame == 8) CHECK(open_state.clicked == 1 && checked == 1);
            CHECK(nx_overlay_end(ctx) == NX_OK);
            if (dismissed) show_menu = 0;
        }
        CHECK(nx_end_frame(ctx) == NX_OK);
        uint64_t expected_focus = frame == 0 ? 0u :
            frame == 1 ? 3u : frame == 2 || frame >= 9 ? 2u :
            frame == 3 || frame == 5 || frame == 7 || frame == 8 ? 13u : 15u;
        CHECK(nx_focus_id(ctx) == expected_focus);
        CHECK(semantic(ctx, 1, 0, NX_ROLE_MENU_BAR, 0, 0));
        CHECK(semantic(ctx, 2, 1, NX_ROLE_MENU_ITEM, 0, 0));
        CHECK(semantic(ctx, 4, 0, NX_ROLE_TOOLBAR, 0, 0));
        CHECK(semantic(ctx, 6, 0, NX_ROLE_STATUS, 0, 0));
        if (frame >= 3 && frame <= 9) {
            CHECK(semantic(ctx, 10, 0, NX_ROLE_POPUP, 0, 0));
            CHECK(semantic(ctx, 12, 10, NX_ROLE_MENU, 0, 0));
            CHECK(semantic(ctx, 13, 12, NX_ROLE_MENU_ITEM,
                           NX_SEM_CHECKED, checked));
            CHECK(semantic(ctx, 14, 12, NX_ROLE_MENU_ITEM,
                           NX_SEM_DISABLED, 1));
        }
        if (frame == 8) {
            const nx_command *commands;
            size_t count;
            uint8_t pixels[200u * 160u * 4u];
            CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK);
            CHECK(nx_headless_render(commands, count, pixels, 200, 160,
                200u * 4u, (nx_color){0, 0, 0, 0}) == NX_OK);
            CHECK(pixels[(40u * 200u + 20u) * 4u + 3u] != 0);
        }
        if (frame == 0) CHECK(nx_focus_set(ctx, 2) == NX_OK);
    }
    CHECK(checked == 1 && !show_menu);
    nx_context_destroy(ctx);
    return 0;
}

static void *fail_allocate(void *user, size_t size) {
    size_t *fail_size = (size_t *)user;
    if (*fail_size == size) {
        *fail_size = 0;
        return NULL;
    }
    return malloc(size);
}

static void fail_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static int test_paint_retry(void) {
    size_t fail_size = 4u * sizeof(nx_command);
    nx_context_desc config = {0};
    config.initial_command_capacity = 2;
    config.allocator = (nx_allocator){&fail_size, fail_allocate,
                                       fail_deallocate};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&config, &ctx) == NX_OK);
    nx_container_desc menu = container(1, 0,
        (nx_rect){0, 0, 100, 60}, "Menu", 4);
    nx_menu_item_desc entry = item(2, 1,
        (nx_rect){0, 0, 100, 24}, 0, 1);
    int checked = 0;
    nx_interaction_state state;
    CHECK(nx_begin_frame(ctx, 100, 60, 0) == NX_OK);
    CHECK(nx_menu(ctx, &menu) == NX_OK);
    CHECK(nx_menu_item(ctx, &entry, &checked, &state) == NX_ERROR_MEMORY);
    CHECK(!checked && !state.clicked);
    CHECK(nx_menu_item(ctx, &entry, &checked, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 2);
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_menu_navigation() || test_paint_retry()) return 1;
    puts("menu tests passed");
    return 0;
}
