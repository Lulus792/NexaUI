#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
    return 1; \
} } while (0)

#include "search_font_fixture.h"

static int key(nx_context *ctx, nx_key code) {
    nx_event e = {0};
    e.type = NX_EVENT_KEY_DOWN;
    e.key = code;
    if (nx_context_queue_event(ctx, &e) != NX_OK) return 0;
    e.type = NX_EVENT_KEY_UP;
    return nx_context_queue_event(ctx, &e) == NX_OK;
}

static int pointer_click(nx_context *ctx, float x, float y) {
    nx_event e = {0};
    e.button = NX_BUTTON_LEFT;
    e.x = x;
    e.y = y;
    e.type = NX_EVENT_POINTER_DOWN;
    if (nx_context_queue_event(ctx, &e) != NX_OK) return 0;
    e.type = NX_EVENT_POINTER_UP;
    return nx_context_queue_event(ctx, &e) == NX_OK;
}

static int text_event(nx_context *ctx, uint32_t scalar) {
    nx_event e = {0};
    e.type = NX_EVENT_TEXT;
    e.scalar = scalar;
    return nx_context_queue_event(ctx, &e) == NX_OK;
}

static int test_filter(void) {
    const nx_search_option options[] = {
        {.id=1, .name={"Alpha", 5}},
        {.id=2, .name={"beta", 4}, .disabled=1},
        {.id=3, .name={"Alpine", 6}},
        {.id=4, .name={"Grün", 5}}
    };
    size_t indices[4] = {99, 99, 99, 99}, count = 0;
    CHECK(nx_search_filter(options, 4, (nx_utf8_span){"AL", 2},
                           indices, 1, &count) == NX_ERROR_LIMIT);
    CHECK(count == 2 && indices[0] == 99);
    CHECK(nx_search_filter(options, 4, (nx_utf8_span){"AL", 2},
                           indices, 4, &count) == NX_OK);
    CHECK(count == 2 && indices[0] == 0 && indices[1] == 2);
    CHECK(nx_search_filter(options, 4, (nx_utf8_span){"Ü", 2},
                           indices, 4, &count) == NX_OK && count == 0);
    CHECK(nx_search_filter(options, 4, (nx_utf8_span){"ün", 3},
                           indices, 4, &count) == NX_OK);
    CHECK(count == 1 && indices[0] == 3);
    CHECK(nx_search_filter(options, 4, (nx_utf8_span){"\xff", 1},
                           indices, 4, &count) == NX_ERROR_ARGUMENT);
    CHECK(count == 0);
    return 0;
}

static nx_search_popup_desc popup_desc(nx_font_collection *fonts,
                                        const nx_search_option *options) {
    nx_search_popup_desc d = {0};
    d.overlay.panel.id = 10;
    d.overlay.panel.bounds = (nx_rect){20, 10, 180, 130};
    d.overlay.panel.name = (nx_utf8_span){"Commands", 8};
    d.overlay.backdrop_id = 11;
    d.overlay.dismiss_on_outside = 1;
    d.overlay.dismiss_on_escape = 1;
    d.query.id = 21;
    d.query.parent_id = 10;
    d.query.bounds = (nx_rect){30, 20, 160, 24};
    d.query.name = (nx_utf8_span){"Search", 6};
    d.query.fonts = fonts;
    d.query.pixels_per_em = 12;
    d.query.glyph_cache_bytes = 4096;
    d.list_id = 22;
    d.empty_id = 23;
    d.list_name = (nx_utf8_span){"Results", 7};
    d.empty_name = (nx_utf8_span){"No results", 10};
    d.list_bounds = (nx_rect){30, 52, 160, 72};
    d.row_height = 24;
    d.options = options;
    d.option_count = 3;
    return d;
}

static int test_popups(nx_font_collection *fonts) {
    const nx_search_option options[] = {
        {.id=101, .name={"Alpha", 5}},
        {.id=102, .name={"Beta", 4}, .disabled=1},
        {.id=103, .name={"Gamma", 5}}
    };
    nx_search_popup_desc desc = popup_desc(fonts, options);
    nx_text_field *field = NULL;
    nx_context *ctx = NULL;
    CHECK(nx_text_field_create("", 0, 1024, &field) == NX_OK);
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_search_state state = {0};
    state.query = field;
    int dismissed = 0;
    uint64_t activated = 0;
    nx_font_collection *empty_fonts = NULL;
    CHECK(nx_font_collection_create(NULL, &empty_fonts) == NX_OK);
    nx_search_popup_desc invalid = desc;
    invalid.query.fonts = empty_fonts;
    CHECK(nx_begin_frame(ctx, 220, 150, 0) == NX_OK);
    CHECK(nx_command_palette(ctx, &invalid, &state, &dismissed,
                              &activated) != NX_OK);
    CHECK(nx_command_palette(ctx, &desc, &state, &dismissed,
                              &activated) == NX_OK);
    CHECK(!dismissed && !activated && state.active_id == 101);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 21);
    uint32_t index = 0;
    nx_semantic_view view;
    CHECK(nx_semantics_find(ctx, 22, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
          view.role == NX_ROLE_LIST);
    const nx_command *commands = NULL;
    size_t command_count = 0;
    uint8_t pixels[220u * 150u * 4u];
    CHECK(nx_frame_commands(ctx, &commands, &command_count) == NX_OK);
    CHECK(nx_headless_render(commands, command_count, pixels, 220, 150,
        220u * 4u, (nx_color){0, 0, 0, 0}) == NX_OK);
    CHECK(pixels[((size_t)64 * 220u + 45u) * 4u + 3u] != 0);

    CHECK(text_event(ctx, 'g'));
    CHECK(nx_begin_frame(ctx, 220, 150, 0) == NX_OK);
    CHECK(nx_command_palette(ctx, &invalid, &state, &dismissed,
                              &activated) != NX_OK);
    CHECK(nx_command_palette(ctx, &desc, &state, &dismissed,
                              &activated) == NX_OK);
    nx_utf8_span draft;
    CHECK(nx_text_field_text(field, &draft) == NX_OK &&
          draft.length == 1 && draft.data[0] == 'g');
    CHECK(state.active_id == 103 && !activated);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_font_collection_destroy(empty_fonts);
    CHECK(nx_text_field_set_text(field, "", 0) == NX_OK);
    state.active_id = 0;

    CHECK(key(ctx, NX_KEY_DOWN));
    CHECK(nx_begin_frame(ctx, 220, 150, 0) == NX_OK);
    CHECK(nx_command_palette(ctx, &desc, &state, &dismissed,
                              &activated) == NX_OK);
    CHECK(!dismissed && state.active_id == 103 && !activated);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 21);

    CHECK(text_event(ctx, 'g'));
    CHECK(nx_begin_frame(ctx, 220, 150, 0) == NX_OK);
    CHECK(nx_command_palette(ctx, &desc, &state, &dismissed,
                              &activated) == NX_OK);
    CHECK(!dismissed && state.active_id == 103 && !activated);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_semantics_find(ctx, 101, &index) == NX_ERROR_ARGUMENT);
    CHECK(nx_semantics_find(ctx, 103, &index) == NX_OK);

    CHECK(key(ctx, NX_KEY_ENTER));
    CHECK(nx_begin_frame(ctx, 220, 150, 0) == NX_OK);
    CHECK(nx_command_palette(ctx, &desc, &state, &dismissed,
                              &activated) == NX_OK);
    CHECK(dismissed && activated == 103 && state.selected_id == 0);
    CHECK(nx_end_frame(ctx) == NX_OK);

    CHECK(nx_text_field_set_text(field, "", 0) == NX_OK);
    state.active_id = 0;
    state.selected_id = 101;
    CHECK(nx_begin_frame(ctx, 220, 150, 0) == NX_OK);
    CHECK(nx_searchable_select(ctx, &desc, &state, &dismissed,
                                &activated) == NX_OK);
    CHECK(!dismissed && state.active_id == 101);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 21);
    CHECK(key(ctx, NX_KEY_DOWN));
    CHECK(key(ctx, NX_KEY_ENTER));
    CHECK(nx_begin_frame(ctx, 220, 150, 0) == NX_OK);
    CHECK(nx_searchable_select(ctx, &desc, &state, &dismissed,
                                &activated) == NX_OK);
    CHECK(dismissed && activated == 103 && state.selected_id == 103);
    CHECK(nx_end_frame(ctx) == NX_OK);

    CHECK(nx_text_field_set_text(field, "zzz", 3) == NX_OK);
    state.active_id = 0;
    CHECK(nx_begin_frame(ctx, 220, 150, 0) == NX_OK);
    CHECK(nx_searchable_select(ctx, &desc, &state, &dismissed,
                                &activated) == NX_OK);
    CHECK(!dismissed && !activated && !state.active_id &&
          state.selected_id == 103);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_semantics_find(ctx, 23, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
          view.role == NX_ROLE_TEXT);
    CHECK(key(ctx, NX_KEY_ESCAPE));
    CHECK(nx_begin_frame(ctx, 220, 150, 0) == NX_OK);
    CHECK(nx_searchable_select(ctx, &desc, &state, &dismissed,
                                &activated) == NX_OK && dismissed && !activated);
    CHECK(nx_end_frame(ctx) == NX_OK);

    CHECK(nx_text_field_set_text(field, "", 0) == NX_OK);
    state.active_id = 0;
    CHECK(nx_begin_frame(ctx, 220, 150, 0) == NX_OK);
    CHECK(nx_searchable_select(ctx, &desc, &state, &dismissed,
                                &activated) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(pointer_click(ctx, 45, 88));
    CHECK(nx_begin_frame(ctx, 220, 150, 0) == NX_OK);
    CHECK(nx_searchable_select(ctx, &desc, &state, &dismissed,
                                &activated) == NX_OK);
    CHECK(!dismissed && !activated && state.selected_id == 103);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(pointer_click(ctx, 45, 64));
    CHECK(nx_begin_frame(ctx, 220, 150, 0) == NX_OK);
    CHECK(nx_searchable_select(ctx, &desc, &state, &dismissed,
                                &activated) == NX_OK);
    CHECK(dismissed && activated == 101 && state.selected_id == 101);
    CHECK(nx_end_frame(ctx) == NX_OK);

    desc.list_bounds.height = 48;
    state.active_id = 0;
    state.first_visible = 0;
    CHECK(nx_begin_frame(ctx, 220, 150, 0) == NX_OK);
    CHECK(nx_searchable_select(ctx, &desc, &state, &dismissed,
                                &activated) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(key(ctx, NX_KEY_UP));
    CHECK(nx_begin_frame(ctx, 220, 150, 0) == NX_OK);
    CHECK(nx_searchable_select(ctx, &desc, &state, &dismissed,
                                &activated) == NX_OK);
    CHECK(state.active_id == 103 && state.first_visible == 1);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_semantics_find(ctx, 103, &index) == NX_OK);
    CHECK(nx_semantics_find(ctx, 101, &index) == NX_ERROR_ARGUMENT);
    nx_event wheel = {0};
    wheel.type = NX_EVENT_WHEEL;
    wheel.x = 45;
    wheel.y = 65;
    wheel.delta_y = 1;
    CHECK(nx_context_queue_event(ctx, &wheel) == NX_OK);
    CHECK(nx_begin_frame(ctx, 220, 150, 0) == NX_OK);
    CHECK(nx_searchable_select(ctx, &desc, &state, &dismissed,
                                &activated) == NX_OK);
    CHECK(state.first_visible == 0);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_semantics_find(ctx, 101, &index) == NX_OK);
    CHECK(nx_semantics_find(ctx, 103, &index) == NX_ERROR_ARGUMENT);
    nx_context_destroy(ctx);
    nx_text_field_destroy(field);
    return 0;
}

int main(void) {
    uint8_t bytes[512];
    font_fixture(bytes);
    nx_font *font = NULL;
    nx_font_collection *fonts = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &fonts) == NX_OK);
    CHECK(nx_font_collection_add(fonts, font) == NX_OK);
    CHECK(test_filter() == 0);
    CHECK(test_popups(fonts) == 0);
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    return 0;
}
