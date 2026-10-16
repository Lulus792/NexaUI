#include "nexaui/nexaui.h"

#include <stdint.h>
#include <stdlib.h>

static void require(int condition) {
    if (!condition) abort();
}

static void check_buffer(nx_text_buffer *buffer) {
    const char *text = NULL;
    size_t length = 0, anchor = 0, cursor = 0;
    require(nx_text_buffer_text(buffer, &text, &length) == NX_OK);
    require(nx_utf8_validate(text, length, NULL) == NX_OK);
    require(nx_text_buffer_selection(buffer, &anchor, &cursor) == NX_OK);
    require(anchor <= length && cursor <= length);
    nx_utf8_span selected = {0};
    require(nx_text_buffer_selected_span(buffer, &selected) == NX_OK);
    require(selected.length == (anchor < cursor ? cursor - anchor : anchor - cursor));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 512u) return 0;
    const char *bytes = (const char *)data;
    size_t bad = 0;
    nx_result valid = nx_utf8_validate(bytes, size, &bad);
    if (valid != NX_OK) {
        require(bad < size);
        return 0;
    }
    require(bad == size);

    size_t offset = 0;
    while (offset < size) {
        size_t next = 0;
        require(nx_grapheme_next(bytes, size, offset, &next) == NX_OK);
        require(next > offset && next <= size);
        offset = next;
    }
    offset = 0;
    while (offset < size) {
        size_t next = 0;
        require(nx_word_next(bytes, size, offset, &next) == NX_OK);
        require(next > offset && next <= size);
        offset = next;
    }
    uint8_t breaks[513] = {0};
    require(nx_line_breaks(bytes, size, breaks, sizeof(breaks)) == NX_OK);
    require(breaks[size] == NX_LINE_BREAK_MANDATORY);

    nx_text_buffer *buffer = NULL;
    require(nx_text_buffer_create(bytes, size, 2048u, &buffer) == NX_OK);
    check_buffer(buffer);
    const nx_text_move moves[] = {
        NX_TEXT_MOVE_DOCUMENT_START, NX_TEXT_MOVE_WORD_FORWARD,
        NX_TEXT_MOVE_GRAPHEME_FORWARD, NX_TEXT_MOVE_LINE_END,
        NX_TEXT_MOVE_GRAPHEME_BACKWARD
    };
    for (size_t i = 0; i < sizeof(moves) / sizeof(moves[0]); ++i) {
        require(nx_text_buffer_move_cursor(buffer, moves[i], (int)(i & 1u)) == NX_OK);
        check_buffer(buffer);
    }
    require(nx_text_buffer_replace(buffer, "\xE2\x82\xAC", 3u) == NX_OK);
    check_buffer(buffer);
    require(nx_text_buffer_delete_backward(buffer) == NX_OK);
    check_buffer(buffer);
    require(nx_text_buffer_delete_forward(buffer) == NX_OK);
    check_buffer(buffer);
    if (nx_text_buffer_can_undo(buffer)) {
        require(nx_text_buffer_undo(buffer) == NX_OK);
        check_buffer(buffer);
        if (nx_text_buffer_can_redo(buffer)) {
            require(nx_text_buffer_redo(buffer) == NX_OK);
            check_buffer(buffer);
        }
    }
    for (size_t i=0;i<size && i<16;++i) {
        nx_event event={0}; int handled=0;
        event.type=NX_EVENT_TEXT_COMMAND;
        event.text_command=1u+data[i]%(NX_TEXT_COMMAND_COUNT-1u);
        if (event.text_command<=NX_TEXT_COMMAND_DOCUMENT_END && (data[i]&1u))
            event.text_command|=NX_TEXT_COMMAND_EXTEND_SELECTION;
        require(nx_text_buffer_apply_event(buffer,&event,&handled)==NX_OK);
        check_buffer(buffer);
    }
    nx_text_buffer_destroy(buffer);
    return 0;
}
