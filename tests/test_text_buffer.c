#include "nexaui/nexaui.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static int expect_text(const nx_text_buffer *buffer,
                       const char *expected, size_t expected_length,
                       size_t anchor, size_t cursor) {
    const char *text = NULL;
    size_t length = 0, got_anchor = SIZE_MAX, got_cursor = SIZE_MAX;
    CHECK(nx_text_buffer_text(buffer, &text, &length) == NX_OK);
    CHECK(length == expected_length &&
          memcmp(text, expected, length) == 0 && text[length] == '\0');
    CHECK(nx_text_buffer_selection(buffer, &got_anchor, &got_cursor) == NX_OK);
    CHECK(got_anchor == anchor && got_cursor == cursor);
    return 0;
}

static int test_edit_and_history(void) {
    nx_text_buffer *buffer = NULL;
    CHECK(nx_text_buffer_create("a\xcc\x81" "b", 4, 4096, &buffer) == NX_OK);
    CHECK(expect_text(buffer, "a\xcc\x81" "b", 4, 4, 4) == 0);
    CHECK(nx_text_buffer_set_selection(buffer, 1, 4) == NX_ERROR_ARGUMENT);
    CHECK(nx_text_buffer_move_cursor(buffer, NX_TEXT_MOVE_GRAPHEME_BACKWARD,
                                      0) == NX_OK);
    CHECK(expect_text(buffer, "a\xcc\x81" "b", 4, 3, 3) == 0);
    CHECK(nx_text_buffer_delete_backward(buffer) == NX_OK);
    CHECK(expect_text(buffer, "b", 1, 0, 0) == 0);
    CHECK(nx_text_buffer_can_undo(buffer) && !nx_text_buffer_can_redo(buffer));
    CHECK(nx_text_buffer_undo(buffer) == NX_OK);
    CHECK(expect_text(buffer, "a\xcc\x81" "b", 4, 3, 3) == 0);
    CHECK(nx_text_buffer_redo(buffer) == NX_OK);
    CHECK(expect_text(buffer, "b", 1, 0, 0) == 0);
    CHECK(nx_text_buffer_replace(buffer, "A", 1) == NX_OK);
    CHECK(expect_text(buffer, "Ab", 2, 1, 1) == 0);
    CHECK(nx_text_buffer_undo(buffer) == NX_OK);
    CHECK(expect_text(buffer, "b", 1, 0, 0) == 0);
    CHECK(nx_text_buffer_can_redo(buffer));
    CHECK(nx_text_buffer_replace(buffer, "X", 1) == NX_OK);
    CHECK(expect_text(buffer, "Xb", 2, 1, 1) == 0);
    CHECK(!nx_text_buffer_can_redo(buffer));
    CHECK(nx_text_buffer_undo(buffer) == NX_OK);
    CHECK(expect_text(buffer, "b", 1, 0, 0) == 0);
    CHECK(nx_text_buffer_undo(buffer) == NX_OK);
    CHECK(expect_text(buffer, "a\xcc\x81" "b", 4, 3, 3) == 0);
    CHECK(nx_text_buffer_undo(buffer) == NX_ERROR_STATE);
    nx_text_buffer_destroy(buffer);
    CHECK(nx_text_buffer_create("abcd", 4, 4096, &buffer) == NX_OK);
    CHECK(nx_text_buffer_set_selection(buffer, 3, 1) == NX_OK);
    CHECK(nx_text_buffer_replace(buffer, "X", 1) == NX_OK);
    CHECK(expect_text(buffer, "aXd", 3, 2, 2) == 0);
    CHECK(nx_text_buffer_undo(buffer) == NX_OK);
    CHECK(expect_text(buffer, "abcd", 4, 3, 1) == 0);
    CHECK(nx_text_buffer_redo(buffer) == NX_OK);
    CHECK(expect_text(buffer, "aXd", 3, 2, 2) == 0);
    nx_text_buffer_destroy(buffer);
    return 0;
}

static int test_ime_delete_surrounding(void) {
    nx_text_buffer *buffer = NULL;
    const char *original = "a\xc3\xa9" "bcd";
    CHECK(nx_text_buffer_create(original, 6, 4096, &buffer) == NX_OK);
    CHECK(nx_text_buffer_set_selection(buffer, 5, 3) == NX_OK);
    CHECK(nx_text_buffer_delete_surrounding(buffer, 2, 1) == NX_OK);
    CHECK(expect_text(buffer, "a", 1, 1, 1) == 0);
    CHECK(nx_text_buffer_undo(buffer) == NX_OK);
    CHECK(expect_text(buffer, original, 6, 5, 3) == 0);
    CHECK(nx_text_buffer_delete_surrounding(buffer, 1, 0) == NX_ERROR_ARGUMENT);
    CHECK(expect_text(buffer, original, 6, 5, 3) == 0);
    CHECK(nx_text_buffer_delete_surrounding(buffer, 1, 2) == NX_ERROR_ARGUMENT);
    CHECK(expect_text(buffer, original, 6, 5, 3) == 0);
    CHECK(nx_text_buffer_set_selection(buffer, 3, 3) == NX_OK);
    CHECK(nx_text_buffer_delete_surrounding(buffer, 1, 0) == NX_ERROR_ARGUMENT);
    CHECK(expect_text(buffer, original, 6, 3, 3) == 0);
    CHECK(nx_text_buffer_delete_surrounding(buffer, 2, 0) == NX_OK);
    CHECK(expect_text(buffer, "abcd", 4, 1, 1) == 0);
    nx_text_buffer_destroy(buffer);
    return 0;
}

static int test_ime_delete_relative(void) {
    nx_text_buffer *buffer = NULL;
    const char *original = "a\xc3\xa9" "bcd";
    CHECK(nx_text_buffer_create(original, 6, 4096, &buffer) == NX_OK);
    CHECK(nx_text_buffer_set_selection(buffer, 3, 3) == NX_OK);
    CHECK(nx_text_buffer_delete_relative(buffer, -1, 1) ==
          NX_ERROR_ARGUMENT);
    CHECK(nx_text_buffer_delete_relative(buffer, -2, 1) ==
          NX_ERROR_ARGUMENT);
    CHECK(nx_text_buffer_delete_relative(buffer, INT32_MIN, 1) ==
          NX_ERROR_ARGUMENT);
    CHECK(expect_text(buffer, original, 6, 3, 3) == 0);
    CHECK(nx_text_buffer_delete_relative(buffer, -2, 2) == NX_OK);
    CHECK(expect_text(buffer, "abcd", 4, 1, 1) == 0);
    CHECK(nx_text_buffer_undo(buffer) == NX_OK);
    CHECK(expect_text(buffer, original, 6, 3, 3) == 0);
    CHECK(nx_text_buffer_delete_relative(buffer, 1, 1) == NX_OK);
    CHECK(expect_text(buffer, "a\xc3\xa9" "bd", 5, 4, 4) == 0);
    CHECK(nx_text_buffer_undo(buffer) == NX_OK);
    CHECK(nx_text_buffer_set_selection(buffer, 3, 5) == NX_OK);
    CHECK(nx_text_buffer_delete_relative(buffer, 0, 1) == NX_OK);
    CHECK(expect_text(buffer, "a\xc3\xa9" "bc", 5, 5, 5) == 0);
    nx_text_buffer_destroy(buffer);
    return 0;
}

static int test_navigation_and_selection(void) {
    const char *value = "one two\r\n\xce\xb1\xe2\x80\xa8" "end";
    nx_text_buffer *buffer = NULL;
    CHECK(nx_text_buffer_create(value, strlen(value), 4096,
                                &buffer) == NX_OK);
    CHECK(nx_text_buffer_move_cursor(buffer, NX_TEXT_MOVE_DOCUMENT_START,
                                      0) == NX_OK);
    CHECK(nx_text_buffer_move_cursor(buffer, NX_TEXT_MOVE_WORD_FORWARD,
                                      0) == NX_OK);
    CHECK(expect_text(buffer, value, strlen(value), 3, 3) == 0);
    CHECK(nx_text_buffer_move_cursor(buffer, NX_TEXT_MOVE_WORD_FORWARD,
                                      1) == NX_OK);
    CHECK(expect_text(buffer, value, strlen(value), 3, 4) == 0);
    CHECK(nx_text_buffer_move_cursor(buffer, NX_TEXT_MOVE_GRAPHEME_BACKWARD,
                                      0) == NX_OK);
    CHECK(expect_text(buffer, value, strlen(value), 3, 3) == 0);
    CHECK(nx_text_buffer_move_cursor(buffer, NX_TEXT_MOVE_LINE_END,
                                      0) == NX_OK);
    CHECK(expect_text(buffer, value, strlen(value), 7, 7) == 0);
    CHECK(nx_text_buffer_move_cursor(buffer, NX_TEXT_MOVE_GRAPHEME_FORWARD,
                                      0) == NX_OK);
    CHECK(expect_text(buffer, value, strlen(value), 9, 9) == 0);
    CHECK(nx_text_buffer_move_cursor(buffer, NX_TEXT_MOVE_LINE_END,
                                      0) == NX_OK);
    CHECK(expect_text(buffer, value, strlen(value), 11, 11) == 0);
    CHECK(nx_text_buffer_move_cursor(buffer, NX_TEXT_MOVE_GRAPHEME_FORWARD,
                                      0) == NX_OK);
    CHECK(expect_text(buffer, value, strlen(value), 14, 14) == 0);
    CHECK(nx_text_buffer_move_cursor(buffer, NX_TEXT_MOVE_LINE_START,
                                      0) == NX_OK);
    CHECK(expect_text(buffer, value, strlen(value), 14, 14) == 0);
    CHECK(nx_text_buffer_move_cursor(buffer, NX_TEXT_MOVE_DOCUMENT_END,
                                      1) == NX_OK);
    CHECK(nx_text_buffer_delete_forward(buffer) == NX_OK);
    CHECK(expect_text(buffer, "one two\r\n\xce\xb1\xe2\x80\xa8", 14,
                      14, 14) == 0);
    nx_text_buffer_destroy(buffer);
    return 0;
}

static int test_boundaries_and_failures(void) {
    nx_text_buffer *buffer = NULL;
    CHECK(nx_text_buffer_create("", 0, 0, &buffer) == NX_OK);
    CHECK(nx_text_buffer_replace(buffer, "\xcc\x81", 2) == NX_OK);
    CHECK(expect_text(buffer, "\xcc\x81", 2, 2, 2) == 0);
    CHECK(nx_text_buffer_set_selection(buffer, 0, 0) == NX_OK);
    CHECK(nx_text_buffer_replace(buffer, "a", 1) == NX_OK);
    CHECK(expect_text(buffer, "a\xcc\x81", 3, 3, 3) == 0);
    CHECK(!nx_text_buffer_can_undo(buffer));
    CHECK(nx_text_buffer_replace(buffer, "\xc0\xaf", 2) == NX_ERROR_ARGUMENT);
    CHECK(expect_text(buffer, "a\xcc\x81", 3, 3, 3) == 0);
    CHECK(nx_text_buffer_delete_forward(buffer) == NX_OK);
    CHECK(expect_text(buffer, "a\xcc\x81", 3, 3, 3) == 0);
    nx_text_buffer_destroy(buffer);
    buffer = (nx_text_buffer *)1;
    CHECK(nx_text_buffer_create("\xc0", 1, 10, &buffer) ==
          NX_ERROR_ARGUMENT && buffer == NULL);
    const char binary[] = {'a', 0, 'b'};
    CHECK(nx_text_buffer_create(binary, sizeof(binary), 0, &buffer) == NX_OK);
    CHECK(expect_text(buffer, binary, sizeof(binary), 3, 3) == 0);
    nx_text_buffer_destroy(buffer);
    return 0;
}

static int test_history_budget_and_emoji(void) {
    nx_text_buffer *buffer = NULL;
    CHECK(nx_text_buffer_create("", 0, 100u, &buffer) == NX_OK);
    CHECK(nx_text_buffer_replace(buffer, "a", 1) == NX_OK);
    CHECK(nx_text_buffer_replace(buffer, "b", 1) == NX_OK);
    CHECK(expect_text(buffer, "ab", 2, 2, 2) == 0);
    CHECK(nx_text_buffer_undo(buffer) == NX_OK);
    CHECK(expect_text(buffer, "a", 1, 1, 1) == 0);
    CHECK(!nx_text_buffer_can_undo(buffer));
    char large[64];
    memset(large, 'z', sizeof(large));
    CHECK(nx_text_buffer_replace(buffer, large, sizeof(large)) == NX_OK);
    CHECK(!nx_text_buffer_can_undo(buffer) &&
          !nx_text_buffer_can_redo(buffer));
    nx_text_buffer_destroy(buffer);

    const char *emoji = "\xf0\x9f\x91\xa9\xe2\x80\x8d"
                        "\xf0\x9f\x92\xbb" "x";
    CHECK(nx_text_buffer_create(emoji, 12, 4096u, &buffer) == NX_OK);
    CHECK(nx_text_buffer_delete_backward(buffer) == NX_OK);
    CHECK(expect_text(buffer, emoji, 11, 11, 11) == 0);
    CHECK(nx_text_buffer_delete_backward(buffer) == NX_OK);
    CHECK(expect_text(buffer, "", 0, 0, 0) == 0);
    CHECK(nx_text_buffer_undo(buffer) == NX_OK);
    CHECK(expect_text(buffer, emoji, 11, 11, 11) == 0);
    nx_text_buffer_destroy(buffer);
    return 0;
}

static int test_event_bridge_and_selected_span(void) {
    nx_text_buffer *buffer = NULL;
    CHECK(nx_text_buffer_create("one two", 7, 4096u, &buffer) == NX_OK);
    int handled = -1;
    nx_event event = {0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = NX_KEY_HOME;
    event.modifiers = NX_MOD_CONTROL;
    CHECK(nx_text_buffer_apply_event(buffer, &event, &handled) == NX_OK &&
          handled);
    CHECK(expect_text(buffer, "one two", 7, 0, 0) == 0);
    event.key = NX_KEY_RIGHT;
    CHECK(nx_text_buffer_apply_event(buffer, &event, &handled) == NX_OK &&
          handled);
    CHECK(expect_text(buffer, "one two", 7, 3, 3) == 0);
    event.modifiers = NX_MOD_CONTROL | NX_MOD_SHIFT;
    CHECK(nx_text_buffer_apply_event(buffer, &event, &handled) == NX_OK &&
          handled);
    nx_utf8_span span = {0};
    CHECK(nx_text_buffer_selected_span(buffer, &span) == NX_OK);
    CHECK(span.length == 1 && span.data[0] == ' ');
    event.key = NX_KEY_A;
    event.modifiers = NX_MOD_CONTROL;
    CHECK(nx_text_buffer_apply_event(buffer, &event, &handled) == NX_OK &&
          handled);
    CHECK(expect_text(buffer, "one two", 7, 0, 7) == 0);
    event.type = NX_EVENT_TEXT;
    event.modifiers = 0;
    event.scalar = 0x1f600u;
    CHECK(nx_text_buffer_apply_event(buffer, &event, &handled) == NX_OK &&
          handled);
    CHECK(expect_text(buffer, "\xf0\x9f\x98\x80", 4, 4, 4) == 0);
    event.type = NX_EVENT_KEY_DOWN;
    event.key = NX_KEY_Z;
    event.modifiers = NX_MOD_CONTROL;
    CHECK(nx_text_buffer_apply_event(buffer, &event, &handled) == NX_OK &&
          handled);
    CHECK(expect_text(buffer, "one two", 7, 0, 7) == 0);
    event.modifiers = NX_MOD_CONTROL | NX_MOD_SHIFT;
    CHECK(nx_text_buffer_apply_event(buffer, &event, &handled) == NX_OK &&
          handled);
    CHECK(expect_text(buffer, "\xf0\x9f\x98\x80", 4, 4, 4) == 0);
    event.key = NX_KEY_ENTER;
    event.modifiers = 0;
    CHECK(nx_text_buffer_apply_event(buffer, &event, &handled) == NX_OK &&
          handled);
    CHECK(expect_text(buffer, "\xf0\x9f\x98\x80\n", 5, 5, 5) == 0);
    event.type = NX_EVENT_TEXT;
    event.scalar = 'x';
    event.modifiers = NX_MOD_CONTROL;
    CHECK(nx_text_buffer_apply_event(buffer, &event, &handled) == NX_OK &&
          !handled);
    CHECK(expect_text(buffer, "\xf0\x9f\x98\x80\n", 5, 5, 5) == 0);
    event.scalar = 0xd800u;
    event.modifiers = 0;
    CHECK(nx_text_buffer_apply_event(buffer, &event, &handled) ==
          NX_ERROR_ARGUMENT && !handled);
    event.type = NX_EVENT_FOCUS;
    CHECK(nx_text_buffer_apply_event(buffer, &event, &handled) == NX_OK &&
          !handled);
    nx_context *context = NULL;
    CHECK(nx_context_create(NULL, &context) == NX_OK);
    nx_event select_all = {0}, insert = {0};
    select_all.type = NX_EVENT_KEY_DOWN;
    select_all.key = NX_KEY_A;
    select_all.modifiers = NX_MOD_CONTROL;
    insert.type = NX_EVENT_TEXT;
    insert.scalar = 'K';
    CHECK(nx_context_queue_event(context, &select_all) == NX_OK);
    CHECK(nx_context_queue_event(context, &insert) == NX_OK);
    CHECK(nx_begin_frame(context, 100.0f, 40.0f, 0.0) == NX_OK);
    const nx_event *events = NULL;
    size_t count = 0;
    CHECK(nx_frame_events(context, &events, &count) == NX_OK && count == 2);
    for (size_t i = 0; i < count; ++i)
        CHECK(nx_text_buffer_apply_event(buffer, &events[i], &handled) ==
              NX_OK && handled);
    CHECK(expect_text(buffer, "K", 1, 1, 1) == 0);
    CHECK(nx_end_frame(context) == NX_OK);
    nx_context_destroy(context);
    nx_text_buffer_destroy(buffer);
    CHECK(nx_text_buffer_create("abc def", 7, 4096u, &buffer) == NX_OK);
    event = (nx_event){0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = NX_KEY_BACKSPACE;
    event.modifiers = NX_MOD_CONTROL;
    CHECK(nx_text_buffer_apply_event(buffer, &event, &handled) == NX_OK &&
          handled);
    CHECK(expect_text(buffer, "abc ", 4, 4, 4) == 0);
    event.type = NX_EVENT_TEXT;
    event.scalar = 0x00e9u;
    event.modifiers = NX_MOD_CONTROL | NX_MOD_ALT;
    CHECK(nx_text_buffer_apply_event(buffer, &event, &handled) == NX_OK &&
          handled);
    CHECK(expect_text(buffer, "abc \xc3\xa9", 6, 6, 6) == 0);
    nx_text_buffer_destroy(buffer);
    return 0;
}

static int test_sensitive_buffer(void) {
    nx_text_buffer *buffer = NULL;
    CHECK(nx_text_buffer_create_sensitive("a\xcc\x81" "b", 4,
                                          &buffer) == NX_OK);
    CHECK(expect_text(buffer, "a\xcc\x81" "b", 4, 4, 4) == 0);
    CHECK(nx_text_buffer_delete_backward(buffer) == NX_OK);
    CHECK(expect_text(buffer, "a\xcc\x81", 3, 3, 3) == 0);
    CHECK(!nx_text_buffer_can_undo(buffer) &&
          !nx_text_buffer_can_redo(buffer));
    CHECK(nx_text_buffer_undo(buffer) == NX_ERROR_STATE);
    nx_event event = {0};
    event.type = NX_EVENT_TEXT;
    event.scalar = 0x9053u;
    int handled = 0;
    CHECK(nx_text_buffer_apply_event(buffer, &event, &handled) == NX_OK &&
          handled);
    CHECK(expect_text(buffer, "a\xcc\x81" "\xe9\x81\x93", 6,
                      6, 6) == 0);
    CHECK(nx_text_buffer_set_selection(buffer, 0, 6) == NX_OK);
    CHECK(nx_text_buffer_replace(buffer, "x", 1) == NX_OK);
    CHECK(expect_text(buffer, "x", 1, 1, 1) == 0);
    CHECK(!nx_text_buffer_can_undo(buffer));
    CHECK(nx_text_buffer_replace(buffer, "\xc0", 1) ==
          NX_ERROR_ARGUMENT);
    CHECK(expect_text(buffer, "x", 1, 1, 1) == 0);
    nx_text_buffer_destroy(buffer);
    buffer = (nx_text_buffer *)1;
    CHECK(nx_text_buffer_create_sensitive("\xc0", 1, &buffer) ==
          NX_ERROR_ARGUMENT && buffer == NULL);
    return 0;
}

static int test_platform_primary_shortcuts(void) {
    nx_text_buffer *buffer = NULL;
    CHECK(nx_text_buffer_create("alpha",5,4096,&buffer) == NX_OK);
    CHECK(nx_text_buffer_replace(buffer,"!",1) == NX_OK);
    nx_event event = {0}; event.type = NX_EVENT_KEY_DOWN; event.key = NX_KEY_Z; event.modifiers = NX_MOD_SUPER;
    int handled;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled) == NX_OK);
#if defined(__APPLE__)
    CHECK(handled && expect_text(buffer,"alpha",5,5,5) == 0);
    event.modifiers |= NX_MOD_SHIFT;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled) == NX_OK && handled);
    CHECK(expect_text(buffer,"alpha!",6,6,6) == 0);
    event.key = NX_KEY_A; event.modifiers = NX_MOD_SUPER;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled) == NX_OK && handled);
    CHECK(expect_text(buffer,"alpha!",6,0,6) == 0);
#else
    CHECK(!handled && expect_text(buffer,"alpha!",6,6,6) == 0);
    event.key = NX_KEY_A;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled) == NX_OK && !handled);
    CHECK(expect_text(buffer,"alpha!",6,6,6) == 0);
#endif
    CHECK(nx_text_buffer_set_selection(buffer,6,6) == NX_OK);
    event.modifiers = NX_MOD_SUPER | NX_MOD_CONTROL;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled) == NX_OK && !handled);
    event.modifiers = NX_MOD_SUPER | NX_MOD_ALT;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled) == NX_OK && !handled);
    CHECK(expect_text(buffer,"alpha!",6,6,6) == 0);
    nx_text_buffer_destroy(buffer); return 0;
}

static int test_text_commands(void) {
    nx_text_buffer *buffer=NULL;
    CHECK(nx_text_buffer_create("alpha beta",10,4096,&buffer)==NX_OK);
    nx_event event={0}; int handled=0;
    event.type=NX_EVENT_TEXT_COMMAND; event.modifiers=NX_MOD_SUPER|NX_MOD_ALT;
    event.text_command=NX_TEXT_COMMAND_DELETE_WORD_BACKWARD;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled)==NX_OK && handled);
    CHECK(expect_text(buffer,"alpha ",6,6,6)==0);
    event.text_command=NX_TEXT_COMMAND_UNDO;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled)==NX_OK && handled);
    CHECK(expect_text(buffer,"alpha beta",10,10,10)==0);
    event.text_command=NX_TEXT_COMMAND_PARAGRAPH_START;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled)==NX_OK && handled);
    event.text_command=NX_TEXT_COMMAND_MOVE_WORD_FORWARD|NX_TEXT_COMMAND_EXTEND_SELECTION;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled)==NX_OK && handled);
    CHECK(expect_text(buffer,"alpha beta",10,0,5)==0);
    event.text_command=NX_TEXT_COMMAND_DELETE_FORWARD;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled)==NX_OK && handled);
    CHECK(expect_text(buffer," beta",5,0,0)==0);
    event.type=NX_EVENT_KEY_DOWN_CONSUMED; event.key=NX_KEY_BACKSPACE;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled)==NX_OK && !handled);
    CHECK(expect_text(buffer," beta",5,0,0)==0);
    event.type=NX_EVENT_TEXT_COMMAND; event.text_command=NX_TEXT_COMMAND_DELETE_FORWARD|NX_TEXT_COMMAND_EXTEND_SELECTION;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled)==NX_ERROR_ARGUMENT && !handled);
    CHECK(expect_text(buffer," beta",5,0,0)==0);
    event.text_command=NX_TEXT_COMMAND_MOVE_LEFT;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled)==NX_OK && !handled); /* Needs visual layout. */
    nx_text_buffer_destroy(buffer); buffer=NULL;
    const char unicode[]="A\xf0\x9f\x8c\x8d" "e\xcc\x81";
    CHECK(nx_text_buffer_create(unicode,sizeof(unicode)-1,4096,&buffer)==NX_OK);
    event.text_command=NX_TEXT_COMMAND_DOCUMENT_START;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled)==NX_OK && handled);
    event.text_command=NX_TEXT_COMMAND_MOVE_FORWARD;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled)==NX_OK && handled);
    event.text_command=NX_TEXT_COMMAND_MOVE_FORWARD|NX_TEXT_COMMAND_EXTEND_SELECTION;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled)==NX_OK && handled);
    CHECK(expect_text(buffer,unicode,8,1,5)==0);
    event.text_command=NX_TEXT_COMMAND_DELETE_BACKWARD;
    CHECK(nx_text_buffer_apply_event(buffer,&event,&handled)==NX_OK && handled);
    CHECK(expect_text(buffer,"Ae\xcc\x81",4,1,1)==0);
    nx_text_buffer_destroy(buffer); return 0;
}

int main(void) {
    if (test_text_commands()) return 1;
    if (test_edit_and_history()) return 1;
    if (test_ime_delete_surrounding()) return 1;
    if (test_ime_delete_relative()) return 1;
    if (test_navigation_and_selection()) return 1;
    if (test_boundaries_and_failures()) return 1;
    if (test_history_budget_and_emoji()) return 1;
    if (test_event_bridge_and_selected_span()) return 1;
    if (test_sensitive_buffer()) return 1;
    if (test_platform_primary_shortcuts()) return 1;
    puts("text buffer tests passed");
    return 0;
}
