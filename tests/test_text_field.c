#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

#include "fixture_font.h"

static int queue_key(nx_context *ctx, nx_key key, uint32_t modifiers) {
    nx_event event = {0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = key;
    event.modifiers = modifiers;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int queue_text(nx_context *ctx, uint32_t scalar) {
    nx_event event = {0};
    event.type = NX_EVENT_TEXT;
    event.scalar = scalar;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int validate_one(nx_utf8_span text, void *user) {
    (void)user;
    return text.length == 1 && text.data[0] == '1';
}

static int expect_text(nx_text_field *field, const char *wanted) {
    nx_utf8_span text;
    return nx_text_field_text(field, &text) == NX_OK &&
           text.length == strlen(wanted) &&
           memcmp(text.data, wanted, text.length) == 0;
}

static int expect_semantic(nx_context *ctx, const char *wanted,
                           int invalid) {
    uint32_t index = UINT32_MAX;
    nx_semantic_view view;
    return nx_semantics_find(ctx, 1, &index) == NX_OK &&
           nx_semantics_get(ctx, index, &view) == NX_OK &&
           view.role == NX_ROLE_TEXT_FIELD &&
           view.value.length == strlen(wanted) &&
           memcmp(view.value.data, wanted, view.value.length) == 0 &&
           !!(view.state & NX_SEM_INVALID) == !!invalid;
}

static int caret_inside(nx_context *ctx, nx_rect bounds) {
    const nx_command *commands = NULL;
    size_t count = 0;
    if (nx_frame_commands(ctx, &commands, &count) != NX_OK) return 0;
    for (size_t i = 0; i < count; ++i) {
        if (commands[i].type != NX_COMMAND_LINE) continue;
        nx_point a = commands[i].points[0];
        nx_point b = commands[i].points[1];
        return a.x >= bounds.x && a.x <= bounds.x + bounds.width &&
               a.y >= bounds.y && b.y <= bounds.y + bounds.height;
    }
    return 0;
}

static int test_caret_scrolling(nx_font_collection *fonts) {
    nx_text_field *field = NULL;
    nx_context *ctx = NULL;
    const char *long_line = "11111111111111111111";
    CHECK(nx_text_field_create(long_line, strlen(long_line), 4096,
                               &field) == NX_OK);
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_text_field_desc desc = {0};
    desc.id = 1;
    desc.bounds = (nx_rect){0, 0, 40, 24};
    desc.name = (nx_utf8_span){"Scroll", 6};
    desc.fonts = fonts;
    desc.pixels_per_em = 20;
    desc.glyph_cache_bytes = 65536;
    nx_interaction_state state;
    int valid, submitted;
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_focus_set(ctx, 1) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(caret_inside(ctx, desc.bounds));
    nx_event press = {0};
    press.type = NX_EVENT_POINTER_DOWN;
    press.button = NX_BUTTON_LEFT;
    press.x = 6;
    press.y = 12;
    CHECK(nx_context_queue_event(ctx, &press) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t anchor, cursor;
    CHECK(nx_text_field_selection(field, &anchor, &cursor) == NX_OK);
    CHECK(anchor == cursor && cursor > 0);
    CHECK(queue_key(ctx, NX_KEY_HOME, NX_MOD_CONTROL));
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(caret_inside(ctx, desc.bounds));
    char lines[80];
    size_t length = 0;
    for (size_t i = 0; i < 20; ++i) {
        lines[length++] = '1';
        lines[length++] = '\n';
    }
    CHECK(nx_text_field_set_text(field, lines, length) == NX_OK);
    desc.multiline = 1;
    desc.bounds.height = 60;
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(caret_inside(ctx, desc.bounds));
    CHECK(queue_key(ctx, NX_KEY_HOME, NX_MOD_CONTROL));
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(caret_inside(ctx, desc.bounds));
    nx_context_destroy(ctx);
    nx_text_field_destroy(field);
    return 0;
}

typedef struct test_clipboard {
    char bytes[64];
    size_t length;
    unsigned reads, writes;
    int unavailable;
} test_clipboard;

static nx_result read_clipboard(void *user, char *bytes, size_t capacity,
                                size_t *out_length) {
    test_clipboard *clipboard = (test_clipboard *)user;
    ++clipboard->reads;
    *out_length = clipboard->length;
    if (clipboard->unavailable) return NX_ERROR_STATE;
    if (!bytes && !capacity) return NX_OK;
    if (capacity <= clipboard->length) return NX_ERROR_LIMIT;
    memcpy(bytes, clipboard->bytes, clipboard->length);
    bytes[clipboard->length] = '\0';
    return NX_OK;
}

static nx_result write_clipboard(void *user, const char *bytes,
                                 size_t length) {
    test_clipboard *clipboard = (test_clipboard *)user;
    ++clipboard->writes;
    if (clipboard->unavailable) return NX_ERROR_STATE;
    if (length >= sizeof(clipboard->bytes)) return NX_ERROR_LIMIT;
    memcpy(clipboard->bytes, bytes, length);
    clipboard->bytes[length] = '\0';
    clipboard->length = length;
    return NX_OK;
}

static int test_clipboard_shortcuts(nx_font_collection *fonts) {
    nx_text_field *field = NULL;
    nx_context *ctx = NULL;
    test_clipboard clipboard = {0};
    CHECK(nx_text_field_create("15", 2, 4096, &field) == NX_OK);
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_text_field_desc desc = {0};
    desc.id = 1;
    desc.bounds = (nx_rect){0, 0, 80, 60};
    desc.name = (nx_utf8_span){"Clipboard", 9};
    desc.fonts = fonts;
    desc.pixels_per_em = 20;
    desc.glyph_cache_bytes = 65536;
    desc.clipboard = (nx_text_clipboard){&clipboard, read_clipboard,
                                          write_clipboard};
    nx_interaction_state state;
    int valid, submitted;
    CHECK(nx_begin_frame(ctx, 100, 80, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_focus_set(ctx, 1) == NX_OK);
    CHECK(queue_key(ctx, NX_KEY_A, NX_MOD_CONTROL));
    CHECK(queue_key(ctx, NX_KEY_C, NX_MOD_CONTROL));
    CHECK(queue_key(ctx, NX_KEY_X, NX_MOD_CONTROL));
    CHECK(queue_key(ctx, NX_KEY_V, NX_MOD_CONTROL));
    CHECK(nx_begin_frame(ctx, 100, 80, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(expect_text(field, "15"));
    CHECK(clipboard.writes == 2 && clipboard.reads == 2);
    CHECK(clipboard.length == 2 && memcmp(clipboard.bytes, "15", 2) == 0);
    memcpy(clipboard.bytes, "5\r\n1", 4);
    clipboard.length = 4;
    CHECK(queue_key(ctx, NX_KEY_A, NX_MOD_CONTROL));
    CHECK(queue_key(ctx, NX_KEY_V, NX_MOD_CONTROL));
    CHECK(nx_begin_frame(ctx, 100, 80, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(expect_text(field, "5 1"));
    clipboard.unavailable = 1;
    CHECK(queue_key(ctx, NX_KEY_A, NX_MOD_CONTROL));
    CHECK(queue_key(ctx, NX_KEY_X, NX_MOD_CONTROL));
    CHECK(nx_begin_frame(ctx, 100, 80, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(expect_text(field, "5 1"));
    clipboard.unavailable = 0;
    memcpy(clipboard.bytes, "5\n1", 3);
    clipboard.length = 3;
    desc.multiline = 1;
    CHECK(nx_text_field_set_text(field, NULL, 0) == NX_OK);
    CHECK(queue_key(ctx, NX_KEY_V, NX_MOD_CONTROL));
    CHECK(nx_begin_frame(ctx, 100, 80, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(expect_text(field, "5\n1"));
    nx_context_destroy(ctx);
    nx_text_field_destroy(field);
    return 0;
}

static uint64_t glyph_image_hash(nx_context *ctx, size_t *out_bitmaps) {
    const nx_command *commands = NULL;
    size_t count = 0;
    if (nx_frame_commands(ctx, &commands, &count) != NX_OK) return 0;
    uint64_t hash = UINT64_C(1469598103934665603);
    *out_bitmaps = 0;
    for (size_t i = 0; i < count; ++i) {
        if (commands[i].type != NX_COMMAND_BITMAP) continue;
        ++*out_bitmaps;
        const nx_bitmap_view *bitmap = &commands[i].bitmap;
        hash = (hash ^ bitmap->width) * UINT64_C(1099511628211);
        hash = (hash ^ bitmap->height) * UINT64_C(1099511628211);
        for (uint32_t y = 0; y < bitmap->height; ++y)
            for (uint32_t x = 0; x < bitmap->width; ++x)
                hash = (hash ^ bitmap->pixels[y * bitmap->stride_bytes + x]) *
                       UINT64_C(1099511628211);
    }
    return hash;
}

static int test_password_field(nx_font_collection *fonts) {
    nx_text_field *field = NULL;
    nx_context *ctx = NULL;
    test_clipboard clipboard = {0};
    memcpy(clipboard.bytes, "5", 1);
    clipboard.length = 1;
    CHECK(nx_text_field_create_password("1\n", 2, &field) ==
          NX_ERROR_ARGUMENT && field == NULL);
    CHECK(nx_text_field_create_password("1", 1, &field) == NX_OK);
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_text_field_desc desc = {0};
    desc.id = 1;
    desc.bounds = (nx_rect){0, 0, 80, 24};
    desc.name = (nx_utf8_span){"Password", 8};
    desc.fonts = fonts;
    desc.pixels_per_em = 20;
    desc.glyph_cache_bytes = 65536;
    desc.password = 1;
    desc.clipboard = (nx_text_clipboard){&clipboard, read_clipboard,
                                          write_clipboard};
    nx_interaction_state state;
    int valid, submitted;
    desc.password = 0;
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_ERROR_ARGUMENT);
    CHECK(nx_end_frame(ctx) == NX_OK);
    desc.password = 1;
    nx_text_field *ordinary = NULL;
    CHECK(nx_text_field_create("1", 1, 0, &ordinary) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, ordinary, &state,
                               &valid, &submitted) == NX_ERROR_ARGUMENT);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_text_field_destroy(ordinary);
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count_a = 0, count_b = 0;
    uint64_t hash_a = glyph_image_hash(ctx, &count_a);
    CHECK(count_a > 0);
    uint32_t index = UINT32_MAX;
    nx_semantic_view view;
    CHECK(nx_semantics_find(ctx, 1, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK);
    CHECK((view.state & NX_SEM_PROTECTED) && view.value.length == 0);
    CHECK(nx_text_field_set_text(field, "\xe9\x81\x93", 3) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    uint64_t hash_b = glyph_image_hash(ctx, &count_b);
    CHECK(count_a == count_b && hash_a == hash_b);
    CHECK(nx_semantics_find(ctx, 1, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK);
    CHECK((view.state & NX_SEM_PROTECTED) && view.value.length == 0);
    CHECK(nx_focus_set(ctx, 1) == NX_OK);
    CHECK(queue_key(ctx, NX_KEY_A, NX_MOD_CONTROL));
    CHECK(queue_key(ctx, NX_KEY_C, NX_MOD_CONTROL));
    CHECK(queue_key(ctx, NX_KEY_X, NX_MOD_CONTROL));
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(expect_text(field, "\xe9\x81\x93"));
    CHECK(clipboard.writes == 0 && clipboard.reads == 0);
    CHECK(queue_key(ctx, NX_KEY_V, NX_MOD_CONTROL));
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(expect_text(field, "5"));
    CHECK(clipboard.writes == 0 && clipboard.reads == 2);
    CHECK(nx_text_field_set_text(field, "bad\n", 4) == NX_ERROR_ARGUMENT);
    CHECK(expect_text(field, "5"));
    CHECK(nx_semantics_find(ctx, 1, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK);
    CHECK((view.state & NX_SEM_PROTECTED) && view.value.length == 0);
    CHECK(queue_key(ctx, NX_KEY_Z, NX_MOD_CONTROL));
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(expect_text(field, "5"));
    CHECK(nx_text_field_set_text(field, "e\xcc\x81" "1", 4) == NX_OK);
    nx_event press = {0};
    press.type = NX_EVENT_POINTER_DOWN;
    press.button = NX_BUTTON_LEFT;
    press.x = 16;
    press.y = 12;
    CHECK(nx_context_queue_event(ctx, &press) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t anchor = 0, cursor = 0;
    CHECK(nx_text_field_selection(field, &anchor, &cursor) == NX_OK);
    CHECK(anchor == 3 && cursor == 3);
    size_t masked_glyphs = 0;
    (void)glyph_image_hash(ctx, &masked_glyphs);
    CHECK(masked_glyphs == 2);
    CHECK(queue_text(ctx, '5'));
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(expect_text(field, "e\xcc\x81" "51"));
    desc.multiline = 1;
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_ERROR_ARGUMENT);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_context_destroy(ctx);
    nx_text_field_destroy(field);
    return 0;
}

static int test_single_line(nx_font_collection *fonts) {
    nx_text_field *field = NULL;
    nx_context *ctx = NULL;
    CHECK(nx_text_field_create("1", 1, 4096, &field) == NX_OK);
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_text_field_desc desc = {0};
    desc.id = 1;
    desc.bounds = (nx_rect){0, 0, 80, 24};
    desc.name = (nx_utf8_span){"Text", 4};
    desc.fonts = fonts;
    desc.pixels_per_em = 20;
    desc.glyph_cache_bytes = 65536;
    desc.validate = validate_one;
    nx_interaction_state state;
    int valid, submitted;
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(valid && !submitted);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_focus_set(ctx, 1) == NX_OK);
    CHECK(queue_key(ctx, NX_KEY_A, NX_MOD_CONTROL));
    CHECK(queue_text(ctx, '5'));
    CHECK(queue_key(ctx, NX_KEY_ENTER, 0));
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(state.focused && !valid && submitted);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(expect_text(field, "5"));
    CHECK(expect_semantic(ctx, "5", 1));
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(!submitted);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_text_field_set_text(field, "1", 1) == NX_OK);
    CHECK(expect_text(field, "1"));
    CHECK(nx_text_field_set_text(field, "bad\n", 4) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_ERROR_ARGUMENT);
    nx_context_destroy(ctx);
    nx_text_field_destroy(field);
    return 0;
}

static int test_multiline(nx_font_collection *fonts) {
    nx_text_field *field = NULL;
    nx_context *ctx = NULL;
    CHECK(nx_text_field_create("1\n5", 3, 4096, &field) == NX_OK);
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_text_field_desc desc = {0};
    desc.id = 1;
    desc.bounds = (nx_rect){0, 0, 80, 500};
    desc.name = (nx_utf8_span){"Notes", 5};
    desc.fonts = fonts;
    desc.pixels_per_em = 20;
    desc.glyph_cache_bytes = 65536;
    desc.multiline = 1;
    nx_interaction_state state;
    int valid, submitted;
    CHECK(nx_begin_frame(ctx, 100, 500, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_focus_set(ctx, 1) == NX_OK);
    CHECK(queue_key(ctx, NX_KEY_ENTER, 0));
    CHECK(nx_begin_frame(ctx, 100, 500, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(valid && !submitted);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(expect_text(field, "1\n5\n"));
    CHECK(expect_semantic(ctx, "1\n5\n", 0));
    CHECK(queue_key(ctx, NX_KEY_UP, 0));
    CHECK(nx_begin_frame(ctx, 100, 500, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t anchor, cursor;
    CHECK(nx_text_field_selection(field, &anchor, &cursor) == NX_OK);
    CHECK(anchor == 2 && cursor == 2);
    CHECK(queue_key(ctx, NX_KEY_DOWN, NX_MOD_SHIFT));
    CHECK(nx_begin_frame(ctx, 100, 500, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_text_field_selection(field, &anchor, &cursor) == NX_OK);
    CHECK(anchor == 2 && cursor == 4);
    char many[80];
    size_t length = 0;
    for (size_t i = 0; i < 20; ++i) {
        many[length++] = '1';
        many[length++] = '\n';
    }
    CHECK(nx_text_field_set_text(field, many, length) == NX_OK);
    CHECK(queue_key(ctx, NX_KEY_A, NX_MOD_CONTROL));
    CHECK(nx_begin_frame(ctx, 100, 500, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_text_field_selection(field, &anchor, &cursor) == NX_OK);
    CHECK(anchor == 0 && cursor == length);
    const nx_command *commands;
    size_t count;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK);
    size_t selections = 0;
    for (size_t i = 0; i < count; ++i)
        if (commands[i].type == NX_COMMAND_FILL_RECT) ++selections;
    CHECK(selections >= 20);
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
    if (test_single_line(fonts) || test_multiline(fonts) ||
        test_caret_scrolling(fonts) ||
        test_clipboard_shortcuts(fonts) ||
        test_password_field(fonts)) return 1;
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    puts("text field tests passed");
    return 0;
}

