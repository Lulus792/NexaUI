#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "search_font_fixture.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static nx_event pointer(nx_event_type type, float x, float y) {
    nx_event event = {0};
    event.type = type;
    event.button = NX_BUTTON_LEFT;
    event.x = x;
    event.y = y;
    return event;
}

static int queue_click(nx_context *ctx, float x, float y) {
    nx_event down = pointer(NX_EVENT_POINTER_DOWN, x, y);
    nx_event up = pointer(NX_EVENT_POINTER_UP, x, y);
    return nx_context_queue_event(ctx, &down) == NX_OK &&
           nx_context_queue_event(ctx, &up) == NX_OK;
}

static nx_control_desc control(uint64_t id, uint64_t parent,
                                nx_rect bounds, const char *name,
                                size_t name_length) {
    nx_control_desc desc = {0};
    desc.id = id;
    desc.parent_id = parent;
    desc.bounds = bounds;
    desc.name = (nx_utf8_span){name, name_length};
    return desc;
}

static int semantic_checked(nx_context *ctx, uint64_t id,
                            nx_semantic_role role, int checked) {
    uint32_t index = UINT32_MAX;
    nx_semantic_view view;
    return nx_semantics_find(ctx, id, &index) == NX_OK &&
           nx_semantics_get(ctx, index, &view) == NX_OK &&
           view.role == role &&
           !!(view.state & NX_SEM_CHECKED) == !!checked;
}

static int test_controls(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_control_desc button = control(1, 0, (nx_rect){0, 0, 20, 10}, "Run", 3);
    nx_control_desc checkbox = control(2, 0, (nx_rect){0, 20, 10, 10}, "Check", 5);
    nx_control_desc radio_a = control(3, 10, (nx_rect){0, 40, 10, 10}, "First", 5);
    nx_control_desc radio_b = control(4, 10, (nx_rect){20, 40, 10, 10}, "Second", 6);
    nx_control_desc toggle = control(5, 0, (nx_rect){0, 60, 20, 10}, "Enable", 6);
    int checked = 0, on = 0;
    uint64_t selected = 0;
    uint8_t selected_radio_pixel[4] = {0};
    for (unsigned frame = 0; frame < 8; ++frame) {
        if (frame == 1) CHECK(queue_click(ctx, 5, 5));
        if (frame == 2) CHECK(queue_click(ctx, 5, 25));
        if (frame == 3) CHECK(queue_click(ctx, 5, 45));
        if (frame == 4) CHECK(queue_click(ctx, 25, 45));
        if (frame == 5) {
            CHECK(nx_focus_set(ctx, 5) == NX_OK);
            nx_event down = {0}, up = {0};
            down.type = NX_EVENT_KEY_DOWN;
            down.key = NX_KEY_SPACE;
            up.type = NX_EVENT_KEY_UP;
            up.key = NX_KEY_SPACE;
            CHECK(nx_context_queue_event(ctx, &down) == NX_OK);
            CHECK(nx_context_queue_event(ctx, &up) == NX_OK);
        }
        if (frame == 6) {
            toggle.disabled = 1;
            CHECK(queue_click(ctx, 5, 65));
        }
        if (frame == 7) {
            CHECK(queue_click(ctx, 5, 25));
            CHECK(queue_click(ctx, 5, 25));
        }
        CHECK(nx_begin_frame(ctx, 60, 80, 0) == NX_OK);
        nx_interaction_state button_state, check_state, radio_a_state;
        nx_interaction_state radio_b_state, toggle_state;
        CHECK(nx_button(ctx, &button, &button_state) == NX_OK);
        CHECK(nx_checkbox(ctx, &checkbox, &checked, &check_state) == NX_OK);
        nx_semantic_desc group = {0};
        group.id = 10;
        group.role = NX_ROLE_GROUP;
        group.bounds = (nx_rect){0, 35, 40, 20};
        CHECK(nx_semantics_add(ctx, &group, NULL) == NX_OK);
        CHECK(nx_radio(ctx, &radio_a, 100, &selected, &radio_a_state) == NX_OK);
        CHECK(nx_radio(ctx, &radio_b, 200, &selected, &radio_b_state) == NX_OK);
        CHECK(nx_toggle(ctx, &toggle, &on, &toggle_state) == NX_OK);
        if (frame == 1) CHECK(button_state.clicked == 1 && button_state.focused);
        if (frame == 2) CHECK(check_state.clicked == 1 && checked == 1);
        if (frame == 3) CHECK(radio_a_state.clicked == 1 && selected == 100);
        if (frame == 4) CHECK(radio_b_state.clicked == 1 && selected == 200);
        if (frame == 5) CHECK(toggle_state.clicked == 1 && on == 1);
        if (frame == 6) CHECK(!toggle_state.clicked && !toggle_state.focused && on);
        if (frame == 7) CHECK(check_state.clicked == 2 && checked == 1);
        CHECK(nx_end_frame(ctx) == NX_OK);
        size_t count = 0;
        CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 6);
        CHECK(semantic_checked(ctx, 1, NX_ROLE_BUTTON, 0));
        CHECK(semantic_checked(ctx, 2, NX_ROLE_CHECKBOX, checked));
        CHECK(semantic_checked(ctx, 3, NX_ROLE_RADIO, selected == 100));
        CHECK(semantic_checked(ctx, 4, NX_ROLE_RADIO, selected == 200));
        CHECK(semantic_checked(ctx, 5, NX_ROLE_SWITCH, on));
        if (frame == 6) {
            uint32_t index;
            nx_semantic_view view;
            CHECK(nx_semantics_find(ctx, 5, &index) == NX_OK);
            CHECK(nx_semantics_get(ctx, index, &view) == NX_OK);
            CHECK((view.state & NX_SEM_DISABLED) &&
                  !(view.state & NX_SEM_FOCUSED));
        }
        const nx_command *commands = NULL;
        size_t command_count = 0;
        CHECK(nx_frame_commands(ctx, &commands, &command_count) == NX_OK &&
              command_count >= 5);
        uint8_t pixels[60u * 80u * 4u];
        CHECK(nx_headless_render(commands, command_count, pixels, 60, 80,
                                 60u * 4u, (nx_color){0, 0, 0, 0}) == NX_OK);
        CHECK(pixels[((size_t)5 * 60u + 10u) * 4u + 3u] != 0);
        if (frame == 3)
            memcpy(selected_radio_pixel,
                   pixels + ((size_t)45 * 60u + 5u) * 4u, 4u);
        if (frame == 4) {
            CHECK(memcmp(selected_radio_pixel,
                         pixels + ((size_t)45 * 60u + 5u) * 4u, 4u) != 0);
            CHECK(memcmp(selected_radio_pixel,
                         pixels + ((size_t)45 * 60u + 25u) * 4u, 4u) == 0);
        }
    }
    nx_context_destroy(ctx);
    return 0;
}

typedef struct allocation_probe {
    unsigned calls, frees, fail_on;
    size_t failed_size;
} allocation_probe;

static void *probe_allocate(void *user, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    ++probe->calls;
    if (probe->calls == probe->fail_on) {
        probe->failed_size = size;
        return NULL;
    }
    return malloc(size);
}

static void probe_deallocate(void *user, void *pointer, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    (void)size;
    ++probe->frees;
    free(pointer);
}

static int test_paint_failure_rollback(void) {
    allocation_probe probe = {0};
    probe.fail_on = 7;
    nx_context_desc context_desc = {0};
    context_desc.allocator = (nx_allocator){&probe, probe_allocate,
                                             probe_deallocate};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&context_desc, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 20, 20, 0) == NX_OK);
    nx_control_desc desc = control(1, 0, (nx_rect){0, 0, 10, 10}, "Do", 2);
    nx_interaction_state state = {1, 1, 1, 1};
    CHECK(nx_button(ctx, &desc, &state) == NX_ERROR_MEMORY);
    CHECK(!state.clicked && !state.focused);
    probe.fail_on = 0;
    CHECK(nx_button(ctx, &desc, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t semantic_count = 0, command_count = 0;
    const nx_command *commands = NULL;
    CHECK(nx_semantics_count(ctx, &semantic_count) == NX_OK &&
          semantic_count == 1);
    CHECK(nx_frame_commands(ctx, &commands, &command_count) == NX_OK &&
          command_count == 1);
    nx_context_destroy(ctx);
    CHECK(probe.frees == probe.calls - 1u);
    return 0;
}

static int test_animated_toggle(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_transition visual = {0};
    nx_control_desc desc = control(30, 0, (nx_rect){0, 0, 20, 10}, "Motion", 6);
    desc.switch_transition = &visual;
    desc.switch_duration_seconds = 0.2;
    int on = 0;
    for (unsigned frame = 0; frame < 4; ++frame) {
        if (frame == 1) CHECK(queue_click(ctx, 5, 5));
        if (frame == 3) CHECK(nx_context_set_reduced_motion(ctx, 1) == NX_OK);
        CHECK(nx_begin_frame(ctx, 20, 10, frame == 2 ? 0.1 : 0) == NX_OK);
        nx_interaction_state state;
        CHECK(nx_toggle(ctx, &desc, &on, &state) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        CHECK(semantic_checked(ctx, 30, NX_ROLE_SWITCH, frame != 0));
        const nx_command *commands = NULL;
        size_t count = 0;
        CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count >= 2);
        float expected_x = frame < 2 ? 0.0f : frame == 2 ? 5.5f : 11.0f;
        CHECK(commands[1].type == NX_COMMAND_FILL_ROUND_RECT &&
              commands[1].rect.x == expected_x);
        CHECK(on == (frame != 0));
        CHECK(visual.active == (frame == 1 || frame == 2));
    }
    nx_context_destroy(ctx);
    return 0;
}

static int test_animated_toggle_failure_retry(void) {
    allocation_probe probe = {0};
    nx_context_desc context_desc = {0};
    context_desc.allocator = (nx_allocator){&probe, probe_allocate,
                                             probe_deallocate};
    context_desc.initial_command_capacity = 1;
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&context_desc, &ctx) == NX_OK);
    nx_transition visual = {0};
    nx_control_desc desc = control(40, 0, (nx_rect){0, 0, 20, 10}, "Retry", 5);
    desc.switch_transition = &visual;
    desc.switch_duration_seconds = 0.2;
    int on = 0;
    nx_interaction_state state;
    for (unsigned frame = 0; frame < 2; ++frame) {
        CHECK(nx_begin_frame(ctx, 20, 10, 0) == NX_OK);
        CHECK(nx_toggle(ctx, &desc, &on, &state) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
    }
    CHECK(queue_click(ctx, 5, 5));
    nx_transition saved = visual;
    probe.fail_on = probe.calls + 1u;
    CHECK(nx_begin_frame(ctx, 20, 10, 0) == NX_OK);
    CHECK(nx_toggle(ctx, &desc, &on, &state) == NX_ERROR_MEMORY);
    CHECK(probe.failed_size == 4u * sizeof(nx_command));
    CHECK(!on && visual.value == saved.value &&
          visual.target == saved.target && visual.active == saved.active);
    probe.fail_on = 0;
    CHECK(nx_toggle(ctx, &desc, &on, &state) == NX_OK && on && visual.active);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(semantic_checked(ctx, 40, NX_ROLE_SWITCH, 1));
    nx_context_destroy(ctx);
    CHECK(probe.frees == probe.calls - 1u);
    return 0;
}

static int test_button_accent_text(void) {
    uint8_t font_bytes[512];
    font_fixture(font_bytes);
    nx_font *font = NULL;
    nx_font_collection *fonts = NULL;
    nx_text_layout *label = NULL;
    nx_context *ctx = NULL;
    CHECK(nx_font_open(font_bytes, sizeof(font_bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &fonts) == NX_OK);
    CHECK(nx_font_collection_add(fonts, font) == NX_OK);
    CHECK(nx_text_layout_create(fonts, "0", 1, NX_BIDI_LTR,
                                18, 80, &label) == NX_OK);
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_control_desc button = control(1, 0,
        (nx_rect){0, 0, 100, 30}, "Action", 6);
    button.label_layout = label;
    button.label_origin = (nx_point){10, 5};
    button.glyph_cache_bytes = 65536;
    for (int kind = NX_THEME_DARK; kind <= NX_THEME_HIGH_CONTRAST; ++kind) {
        nx_theme theme;
        CHECK(nx_theme_preset((nx_theme_kind)kind, &theme) == NX_OK);
        CHECK(nx_context_set_theme(ctx, &theme) == NX_OK);
        CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
        nx_interaction_state state;
        CHECK(nx_button(ctx, &button, &state) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        const nx_command *commands = NULL;
        size_t count = 0;
        CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK);
        int glyphs = 0;
        nx_color expected = theme.colors[NX_COLOR_ACCENT_TEXT];
        for (size_t i = 0; i < count; ++i) {
            if (commands[i].type != NX_COMMAND_BITMAP ||
                commands[i].bitmap.format != NX_BITMAP_A8) continue;
            ++glyphs;
            CHECK(commands[i].color.r == expected.r &&
                  commands[i].color.g == expected.g &&
                  commands[i].color.b == expected.b &&
                  commands[i].color.a == expected.a);
        }
        CHECK(glyphs > 0);
    }
    nx_context_destroy(ctx);
    nx_text_layout_destroy(label);
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    return 0;
}

int main(void) {
    if (test_controls()) return 1;
    if (test_button_accent_text()) return 1;
    if (test_paint_failure_rollback()) return 1;
    if (test_animated_toggle()) return 1;
    if (test_animated_toggle_failure_retry()) return 1;
    puts("control tests passed");
    return 0;
}
