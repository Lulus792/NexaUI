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

static int test_clipboard_shortcuts(nx_font_collection *fonts, uint32_t primary) {
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
    CHECK(queue_key(ctx, NX_KEY_A, primary));
    CHECK(queue_key(ctx, NX_KEY_C, primary));
    CHECK(queue_key(ctx, NX_KEY_X, primary));
    CHECK(queue_key(ctx, NX_KEY_V, primary));
    CHECK(nx_begin_frame(ctx, 100, 80, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(expect_text(field, "15"));
    CHECK(clipboard.writes == 2 && clipboard.reads == 2);
    CHECK(clipboard.length == 2 && memcmp(clipboard.bytes, "15", 2) == 0);
    memcpy(clipboard.bytes, "5\r\n1", 4);
    clipboard.length = 4;
    CHECK(queue_key(ctx, NX_KEY_A, primary));
    CHECK(queue_key(ctx, NX_KEY_V, primary));
    CHECK(nx_begin_frame(ctx, 100, 80, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(expect_text(field, "5 1"));
    clipboard.unavailable = 1;
    CHECK(queue_key(ctx, NX_KEY_A, primary));
    CHECK(queue_key(ctx, NX_KEY_X, primary));
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
    CHECK(queue_key(ctx, NX_KEY_V, primary));
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

static int command_frame(nx_context *ctx,nx_text_field_desc *desc,nx_text_field *field,uint32_t command) {
    nx_event event={0}; event.type=NX_EVENT_TEXT_COMMAND; event.text_command=command;
    event.modifiers=NX_MOD_SUPER|NX_MOD_ALT; /* Selector semantics do not rewrite modifiers. */
    nx_interaction_state state; int valid=0,submitted=0;
    return nx_context_queue_event(ctx,&event)==NX_OK && nx_begin_frame(ctx,200,120,0)==NX_OK &&
        nx_text_field_widget(ctx,desc,field,&state,&valid,&submitted)==NX_OK && nx_end_frame(ctx)==NX_OK;
}
static int physical_key_frame(nx_context *ctx,nx_text_field_desc *desc,nx_text_field *field,nx_key key,uint32_t modifiers) {
    nx_event event={0}; event.type=NX_EVENT_KEY_DOWN; event.key=key; event.modifiers=modifiers;
    nx_interaction_state state; int valid=0,submitted=0;
    if (nx_context_queue_event(ctx,&event)!=NX_OK) return 0;
    event.type=NX_EVENT_KEY_UP;
    return nx_context_queue_event(ctx,&event)==NX_OK && nx_begin_frame(ctx,200,120,0)==NX_OK &&
        nx_text_field_widget(ctx,desc,field,&state,&valid,&submitted)==NX_OK && nx_end_frame(ctx)==NX_OK;
}
static int field_caret(nx_context *ctx,float *x,float *y) {
    const nx_command *commands=NULL; size_t count=0;
    if (nx_frame_commands(ctx,&commands,&count)!=NX_OK) return 0;
    for (size_t i=0;i<count;++i) if (commands[i].type==NX_COMMAND_LINE) {
        *x=commands[i].points[0].x; *y=commands[i].points[0].y; return 1;
    }
    return 0;
}
static int field_pixel_red(nx_context *ctx,unsigned x,unsigned y) {
    const nx_command *commands=NULL; size_t count=0; uint8_t pixels[200*120*4];
    if (nx_frame_commands(ctx,&commands,&count)!=NX_OK ||
        nx_headless_render(commands,count,pixels,200,120,200*4,(nx_color){0,0,0,1})!=NX_OK) return -1;
    return pixels[(y*200+x)*4];
}
static int test_native_text_commands(nx_font_collection *fonts) {
    nx_context *ctx=NULL; nx_text_field *field=NULL;
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && nx_text_field_create("111",3,4096,&field)==NX_OK);
    nx_text_field_desc desc={0}; desc.id=1; desc.bounds=(nx_rect){0,0,80,80};
    desc.name=(nx_utf8_span){"Commands",8}; desc.fonts=fonts; desc.pixels_per_em=20; desc.glyph_cache_bytes=65536;
    nx_interaction_state state; int valid=0,submitted=0; size_t anchor=0,cursor=0;
    CHECK(nx_begin_frame(ctx,200,120,0)==NX_OK && nx_text_field_widget(ctx,&desc,field,&state,&valid,&submitted)==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK && nx_focus_set(ctx,1)==NX_OK);
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_MOVE_LEFT));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && anchor==2 && cursor==2);
    nx_event consumed={0}; consumed.type=NX_EVENT_KEY_DOWN_CONSUMED; consumed.key=NX_KEY_BACKSPACE;
    CHECK(nx_context_queue_event(ctx,&consumed)==NX_OK);
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_LINE_START));
    CHECK(expect_text(field,"111"));
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_MOVE_RIGHT|NX_TEXT_COMMAND_EXTEND_SELECTION));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && anchor==0 && cursor==1);
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_DELETE_BACKWARD) && expect_text(field,"11"));
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_UNDO) && expect_text(field,"111"));
    CHECK(nx_text_field_set_text(field,"\xd7\x90\xd7\x91",4)==NX_OK);
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_MOVE_RIGHT));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && cursor==2);
    float x=0,y=0; CHECK(field_caret(ctx,&x,&y) && x==16);
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_MOVE_LEFT));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && cursor==4);
    CHECK(physical_key_frame(ctx,&desc,field,NX_KEY_RIGHT,0));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && cursor==2);
    CHECK(nx_text_field_set_text(field,"1\xd7\x90\xd7\x91" "1",6)==NX_OK);
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_DOCUMENT_START));
    float previous_x=4;
    for (unsigned i=0;i<16;++i) {
        CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_MOVE_RIGHT));
        CHECK(field_caret(ctx,&x,&y) && x>=previous_x); previous_x=x;
    }
    CHECK(x==52); /* Bidi visual end is independent of the document's byte end. */
    for (unsigned i=0;i<16;++i) {
        CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_MOVE_LEFT));
        CHECK(field_caret(ctx,&x,&y) && x<=previous_x); previous_x=x;
    }
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && cursor==0 && x==4);
    /* Spaces provide actual Unicode wrap opportunities; a run of digits is
       deliberately unbreakable under the layout contract. */
    CHECK(nx_text_field_set_text(field,"1 1 1",5)==NX_OK);
    desc.multiline=1; desc.bounds.width=32;
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_LINE_START));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && cursor==4);
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_MOVE_UP));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && cursor==2);
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_LINE_END));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && cursor==4);
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_MOVE_RIGHT));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && cursor==4);
    CHECK(field_caret(ctx,&x,&y) && x==4 && y==44); /* Same byte, next soft-wrapped line. */
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_LINE_END));
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_DELETE_LINE_START) && expect_text(field,"1 1 "));
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_UNDO) && expect_text(field,"1 1 1"));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && anchor==5 && cursor==5);
    CHECK(nx_text_field_set_text(field,"1\n1\n1\n1\n1\n1",11)==NX_OK);
    desc.bounds.height=40;
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_DOCUMENT_START));
    CHECK(field_caret(ctx,&x,&y) && y==4);
    /* A one-pixel line centered at logical x=4 covers pixel column 3 under
       the renderer's half-open pixel-center convention. */
    int caret_red=field_pixel_red(ctx,3,5); CHECK(caret_red>0);
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_PAGE_DOWN));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && anchor==0 && cursor==0);
    CHECK(field_caret(ctx,&x,&y) && y<0); /* Recorded line is now outside the viewport clip. */
    int clipped_red=field_pixel_red(ctx,3,5);
    CHECK(clipped_red>=0 && clipped_red<caret_red);
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_PAGE_UP));
    CHECK(field_caret(ctx,&x,&y) && y==4);
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_MOVE_PAGE_DOWN));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && anchor==4 && cursor==4);
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_DOCUMENT_START));
    CHECK(physical_key_frame(ctx,&desc,field,NX_KEY_PAGE_DOWN,0));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK);
#if defined(__APPLE__)
    CHECK(anchor==0 && cursor==0); /* Native Mac page key scrolls the viewport. */
#else
    CHECK(anchor==4 && cursor==4); /* Default PC editor page key moves the caret. */
#endif
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_DOCUMENT_START));
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_PAGE_DOWN|NX_TEXT_COMMAND_EXTEND_SELECTION));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && anchor==0 && cursor==4);
    test_clipboard clipboard={0}; desc.multiline=0; desc.bounds.width=80;
    desc.clipboard=(nx_text_clipboard){&clipboard,read_clipboard,write_clipboard};
    CHECK(nx_text_field_set_text(field,"15",2)==NX_OK);
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_SELECT_ALL));
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_COPY) && clipboard.writes==1 && !strcmp(clipboard.bytes,"15"));
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_CUT) && expect_text(field,""));
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_PASTE) && expect_text(field,"15"));
    nx_text_field_destroy(field); field=NULL;
    CHECK(nx_text_field_create_password("A\xf0\x9f\x8c\x8d",5,&field)==NX_OK); desc.password=1;
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_DOCUMENT_START));
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_MOVE_RIGHT));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && cursor==1);
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_MOVE_RIGHT));
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && cursor==5);
    unsigned writes=clipboard.writes;
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_SELECT_ALL));
    CHECK(command_frame(ctx,&desc,field,NX_TEXT_COMMAND_COPY) && command_frame(ctx,&desc,field,NX_TEXT_COMMAND_CUT));
    CHECK(clipboard.writes==writes && expect_text(field,"A\xf0\x9f\x8c\x8d"));
    nx_text_field_destroy(field); nx_context_destroy(ctx); return 0;
}

int main(void) {
    uint8_t bytes[512];
    font_fixture(bytes);
    nx_font *font = NULL;
    nx_font_collection *fonts = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &fonts) == NX_OK);
    CHECK(nx_font_collection_add(fonts, font) == NX_OK);
    if (test_native_text_commands(fonts)) return 1;
    if (test_single_line(fonts) || test_multiline(fonts) ||
        test_caret_scrolling(fonts) ||
        test_clipboard_shortcuts(fonts, NX_MOD_CONTROL) ||
        test_password_field(fonts)) return 1;
#if defined(__APPLE__)
    if (test_clipboard_shortcuts(fonts, NX_MOD_SUPER)) return 1;
#endif
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    puts("text field tests passed");
    return 0;
}

