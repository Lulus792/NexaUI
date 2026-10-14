#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static nx_control_desc button(uint64_t id, nx_rect bounds) {
    nx_control_desc desc = {0};
    desc.id = id;
    desc.bounds = bounds;
    desc.name = (nx_utf8_span){"Action", 6};
    return desc;
}

static nx_overlay_desc overlay(uint64_t id, uint64_t backdrop_id) {
    nx_overlay_desc desc = {0};
    desc.panel.id = id;
    desc.panel.bounds = (nx_rect){40, 20, 120, 80};
    desc.panel.name = (nx_utf8_span){"Actions", 7};
    desc.backdrop_id = backdrop_id;
    desc.dismiss_on_outside = 1;
    desc.dismiss_on_escape = 1;
    return desc;
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

static int key(nx_context *ctx, nx_key code, uint32_t modifiers) {
    nx_event event = {0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = code;
    event.modifiers = modifiers;
    if (nx_context_queue_event(ctx, &event) != NX_OK) return 0;
    event.type = NX_EVENT_KEY_UP;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int test_modal_popup_tooltip(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_control_desc underlay = button(1, (nx_rect){0, 0, 200, 120});
    nx_control_desc first = button(12, (nx_rect){50, 30, 60, 20});
    first.parent_id = 10;
    nx_control_desc second = button(13, (nx_rect){50, 60, 60, 20});
    second.parent_id = 10;
    nx_overlay_desc modal = overlay(10, 11);
    nx_interaction_state state;
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_focus_set(ctx, 1) == NX_OK);

    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK);
    int dismissed = 1;
    CHECK(nx_modal_begin(ctx, &modal, &dismissed) == NX_OK && !dismissed);
    CHECK(nx_end_frame(ctx) == NX_ERROR_STATE);
    CHECK(nx_button(ctx, &first, &state) == NX_OK);
    CHECK(nx_button(ctx, &second, &state) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_focus_id(ctx) == 12);
    uint32_t index;
    nx_semantic_view view;
    CHECK(nx_semantics_find(ctx, 10, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
          view.role == NX_ROLE_DIALOG);
    const nx_command *commands;
    size_t command_count;
    uint8_t pixels[200u * 120u * 4u];
    CHECK(nx_frame_commands(ctx, &commands, &command_count) == NX_OK);
    CHECK(nx_headless_render(commands, command_count, pixels, 200, 120,
        200u * 4u, (nx_color){0, 0, 0, 0}) == NX_OK);
    CHECK(pixels[(10u * 200u + 10u) * 4u + 3u] != 0);
    CHECK(memcmp(&pixels[(10u * 200u + 10u) * 4u],
                 &pixels[(25u * 200u + 45u) * 4u], 4u) != 0);

    CHECK(key(ctx, NX_KEY_TAB, NX_MOD_SHIFT));
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK && !state.clicked);
    CHECK(nx_modal_begin(ctx, &modal, &dismissed) == NX_OK && !dismissed);
    CHECK(nx_button(ctx, &first, &state) == NX_OK);
    CHECK(nx_button(ctx, &second, &state) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_focus_id(ctx) == 13);
    CHECK(nx_focus_set(ctx, 1) == NX_ERROR_ARGUMENT);
    CHECK(key(ctx, NX_KEY_TAB, 0));
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK);
    CHECK(nx_modal_begin(ctx, &modal, &dismissed) == NX_OK && !dismissed);
    CHECK(nx_button(ctx, &first, &state) == NX_OK);
    CHECK(nx_button(ctx, &second, &state) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 12);

    CHECK(click(ctx, 10, 10));
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK && !state.clicked);
    CHECK(nx_modal_begin(ctx, &modal, &dismissed) == NX_OK && dismissed);
    CHECK(nx_button(ctx, &first, &state) == NX_OK);
    CHECK(nx_button(ctx, &second, &state) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 1);

    CHECK(click(ctx, 10, 10));
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK && state.clicked == 1);
    CHECK(nx_end_frame(ctx) == NX_OK);

    nx_container_desc tooltip = {0};
    tooltip.id = 30;
    tooltip.bounds = (nx_rect){5, 5, 50, 20};
    tooltip.name = (nx_utf8_span){"Hint", 4};
    nx_overlay_desc popup = overlay(20, 21);
    nx_control_desc popup_child = button(22, (nx_rect){50, 30, 60, 20});
    popup_child.parent_id = 20;
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK);
    CHECK(nx_tooltip(ctx, &tooltip) == NX_OK);
    CHECK(nx_popup_begin(ctx, &popup, &dismissed) == NX_OK && !dismissed);
    CHECK(nx_button(ctx, &popup_child, &state) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 22);
    CHECK(nx_semantics_find(ctx, 30, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
          view.role == NX_ROLE_TOOLTIP);
    CHECK(nx_semantics_find(ctx, 20, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
          view.role == NX_ROLE_POPUP);
    CHECK(key(ctx, NX_KEY_ESCAPE, 0));
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK);
    CHECK(nx_popup_begin(ctx, &popup, &dismissed) == NX_OK && dismissed);
    CHECK(nx_button(ctx, &popup_child, &state) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 1);
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK);
    CHECK(nx_popup_begin(ctx, &popup, &dismissed) == NX_OK && !dismissed);
    CHECK(nx_button(ctx, &popup_child, &state) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 22);
    CHECK(nx_overlay_close(ctx) == NX_OK && nx_focus_id(ctx) == 1);
    CHECK(click(ctx, 10, 10));
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK && state.clicked == 1);
    CHECK(nx_end_frame(ctx) == NX_OK);
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

static int test_failure_retry(void) {
    size_t fail_size = 4u * sizeof(nx_command);
    nx_context_desc config = {0};
    config.initial_command_capacity = 2;
    config.allocator = (nx_allocator){&fail_size, fail_allocate,
                                       fail_deallocate};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&config, &ctx) == NX_OK);
    nx_overlay_desc modal = overlay(10, 11);
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    int dismissed = 1;
    CHECK(nx_modal_begin(ctx, &modal, &dismissed) == NX_ERROR_MEMORY);
    CHECK(!dismissed);
    CHECK(nx_modal_begin(ctx, &modal, &dismissed) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 10);
    size_t count;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 1);
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_modal_popup_tooltip() || test_failure_retry()) return 1;
    puts("overlay tests passed");
    return 0;
}
