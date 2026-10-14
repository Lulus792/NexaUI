#include "nexaui/nexaui.h"
#include "secure_zero.h"
#include "unicode_data.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct nx_text_record {
    struct nx_text_record *next;
    size_t allocation_size;
    size_t start, removed_length, inserted_length;
    size_t anchor_before, cursor_before;
    size_t anchor_after, cursor_after;
    char data[1]; /* Removed bytes followed by inserted bytes. */
} nx_text_record;

struct nx_text_buffer {
    char *bytes;
    uint8_t *grapheme, *word;
    size_t length, anchor, cursor;
    size_t history_limit, history_used;
    nx_text_record *undo, *redo;
    int sensitive;
};

typedef struct nx_text_state {
    char *bytes;
    uint8_t *grapheme, *word;
    size_t length;
} nx_text_state;

static void nx_tb_free_state(nx_text_state *state, int sensitive) {
    if (sensitive && state->bytes)
        nx_private_secure_zero(state->bytes, state->length + 1u);
    free(state->bytes);
    free(state->grapheme);
    free(state->word);
    *state = (nx_text_state){0};
}

static nx_result nx_tb_build_state(char *bytes, size_t length, int sensitive,
                                   nx_text_state *out_state) {
    nx_text_state next = {0};
    next.bytes = bytes;
    next.length = length;
    next.grapheme = calloc(length + 1u, 1u);
    next.word = calloc(length + 1u, 1u);
    if (!next.grapheme || !next.word) {
        nx_tb_free_state(&next, sensitive);
        return NX_ERROR_MEMORY;
    }
    next.grapheme[0] = next.word[0] = 1u;
    size_t offset = 0;
    while (offset < length) {
        size_t following = 0;
        nx_result result = nx_grapheme_next(bytes, length, offset,
                                            &following);
        if (result != NX_OK || following <= offset) {
            nx_tb_free_state(&next, sensitive);
            return result == NX_OK ? NX_ERROR_STATE : result;
        }
        next.grapheme[following] = 1u;
        offset = following;
    }
    offset = 0;
    while (offset < length) {
        size_t following = 0;
        nx_result result = nx_word_next(bytes, length, offset,
                                        &following);
        if (result != NX_OK || following <= offset) {
            nx_tb_free_state(&next, sensitive);
            return result == NX_OK ? NX_ERROR_STATE : result;
        }
        next.word[following] = 1u;
        offset = following;
    }
    *out_state = next;
    return NX_OK;
}

static void nx_tb_commit(nx_text_buffer *buffer, nx_text_state *next,
                          size_t anchor, size_t cursor) {
    nx_text_state old = {buffer->bytes, buffer->grapheme, buffer->word,
                         buffer->length};
    buffer->bytes = next->bytes;
    buffer->grapheme = next->grapheme;
    buffer->word = next->word;
    buffer->length = next->length;
    buffer->anchor = anchor;
    buffer->cursor = cursor;
    *next = (nx_text_state){0};
    nx_tb_free_state(&old, buffer->sensitive);
}

static void nx_tb_clear_records(nx_text_buffer *buffer,
                                 nx_text_record **stack) {
    nx_text_record *record = *stack;
    *stack = NULL;
    while (record) {
        nx_text_record *next = record->next;
        buffer->history_used -= record->allocation_size;
        if (buffer->sensitive)
            nx_private_secure_zero(record, record->allocation_size);
        free(record);
        record = next;
    }
}

static void nx_tb_evict_oldest(nx_text_buffer *buffer) {
    nx_text_record **link = &buffer->undo;
    while ((*link)->next) link = &(*link)->next;
    nx_text_record *oldest = *link;
    *link = NULL;
    buffer->history_used -= oldest->allocation_size;
    if (buffer->sensitive)
        nx_private_secure_zero(oldest, oldest->allocation_size);
    free(oldest);
}

static nx_result nx_tb_create(const char *bytes, size_t length,
                              size_t history_bytes, int sensitive,
                              nx_text_buffer **out_buffer) {
    if (!out_buffer) return NX_ERROR_ARGUMENT;
    *out_buffer = NULL;
    if ((!bytes && length) || length == SIZE_MAX ||
        nx_utf8_validate(bytes, length, NULL) != NX_OK)
        return NX_ERROR_ARGUMENT;
    nx_text_buffer *buffer = calloc(1u, sizeof(*buffer));
    if (!buffer) return NX_ERROR_MEMORY;
    char *copy = malloc(length + 1u);
    if (!copy) { free(buffer); return NX_ERROR_MEMORY; }
    if (length) memcpy(copy, bytes, length);
    copy[length] = '\0';
    nx_text_state state = {0};
    nx_result result = nx_tb_build_state(copy, length, sensitive, &state);
    if (result != NX_OK) { free(buffer); return result; }
    buffer->bytes = state.bytes;
    buffer->grapheme = state.grapheme;
    buffer->word = state.word;
    buffer->length = length;
    buffer->anchor = buffer->cursor = length;
    buffer->history_limit = sensitive ? 0u : history_bytes;
    buffer->sensitive = sensitive;
    *out_buffer = buffer;
    return NX_OK;
}

nx_result nx_text_buffer_create(const char *bytes, size_t length,
                                 size_t history_bytes,
                                 nx_text_buffer **out_buffer) {
    return nx_tb_create(bytes, length, history_bytes, 0, out_buffer);
}

nx_result nx_text_buffer_create_sensitive(const char *bytes, size_t length,
                                           nx_text_buffer **out_buffer) {
    return nx_tb_create(bytes, length, 0u, 1, out_buffer);
}

void nx_text_buffer_destroy(nx_text_buffer *buffer) {
    if (!buffer) return;
    nx_tb_clear_records(buffer, &buffer->undo);
    nx_tb_clear_records(buffer, &buffer->redo);
    nx_text_state state = {buffer->bytes, buffer->grapheme, buffer->word,
                           buffer->length};
    nx_tb_free_state(&state, buffer->sensitive);
    if (buffer->sensitive) nx_private_secure_zero(buffer, sizeof(*buffer));
    free(buffer);
}

nx_result nx_text_buffer_text(const nx_text_buffer *buffer,
                               const char **out_bytes, size_t *out_length) {
    if (!buffer || !out_bytes || !out_length) return NX_ERROR_ARGUMENT;
    *out_bytes = buffer->bytes;
    *out_length = buffer->length;
    return NX_OK;
}

nx_result nx_text_buffer_selection(const nx_text_buffer *buffer,
                                    size_t *out_anchor,
                                    size_t *out_cursor) {
    if (!buffer || !out_anchor || !out_cursor) return NX_ERROR_ARGUMENT;
    *out_anchor = buffer->anchor;
    *out_cursor = buffer->cursor;
    return NX_OK;
}

nx_result nx_text_buffer_selected_span(const nx_text_buffer *buffer,
                                        nx_utf8_span *out_span) {
    if (!buffer || !out_span) return NX_ERROR_ARGUMENT;
    size_t start = buffer->anchor < buffer->cursor ?
                   buffer->anchor : buffer->cursor;
    size_t end = buffer->anchor > buffer->cursor ?
                 buffer->anchor : buffer->cursor;
    *out_span = (nx_utf8_span){buffer->bytes + start, end - start};
    return NX_OK;
}

nx_result nx_text_buffer_set_selection(nx_text_buffer *buffer,
                                        size_t anchor, size_t cursor) {
    if (!buffer || anchor > buffer->length || cursor > buffer->length ||
        !buffer->grapheme[anchor] || !buffer->grapheme[cursor])
        return NX_ERROR_ARGUMENT;
    buffer->anchor = anchor;
    buffer->cursor = cursor;
    return NX_OK;
}

static nx_result nx_tb_prepare_replace(const nx_text_buffer *buffer,
                                        size_t start, size_t removed_length,
                                        const char *inserted,
                                        size_t inserted_length,
                                        nx_text_state *out_state) {
    if (start > buffer->length ||
        removed_length > buffer->length - start ||
        inserted_length > SIZE_MAX - (buffer->length - removed_length) - 1u)
        return NX_ERROR_LIMIT;
    size_t next_length = buffer->length - removed_length + inserted_length;
    char *bytes = malloc(next_length + 1u);
    if (!bytes) return NX_ERROR_MEMORY;
    if (start) memcpy(bytes, buffer->bytes, start);
    if (inserted_length) memcpy(bytes + start, inserted, inserted_length);
    size_t suffix = buffer->length - start - removed_length;
    if (suffix)
        memcpy(bytes + start + inserted_length,
               buffer->bytes + start + removed_length, suffix);
    bytes[next_length] = '\0';
    return nx_tb_build_state(bytes, next_length, buffer->sensitive, out_state);
}

static nx_result nx_tb_edit(nx_text_buffer *buffer, size_t start,
                            size_t removed_length, const char *inserted,
                            size_t inserted_length) {
    if (!removed_length && !inserted_length) return NX_OK;
    if (removed_length > SIZE_MAX - inserted_length ||
        removed_length + inserted_length >
            SIZE_MAX - sizeof(nx_text_record))
        return NX_ERROR_LIMIT;
    size_t record_size = sizeof(nx_text_record) + removed_length +
                         inserted_length;
    nx_text_record *record = NULL;
    if (record_size <= buffer->history_limit) {
        record = malloc(record_size);
        if (!record) return NX_ERROR_MEMORY;
        memset(record, 0, sizeof(*record));
        record->allocation_size = record_size;
        record->start = start;
        record->removed_length = removed_length;
        record->inserted_length = inserted_length;
        record->anchor_before = buffer->anchor;
        record->cursor_before = buffer->cursor;
        if (removed_length)
            memcpy(record->data, buffer->bytes + start, removed_length);
        if (inserted_length)
            memcpy(record->data + removed_length, inserted,
                   inserted_length);
    }
    nx_text_state next = {0};
    nx_result result = nx_tb_prepare_replace(buffer, start, removed_length,
                                              inserted, inserted_length,
                                              &next);
    if (result != NX_OK) {
        if (record && buffer->sensitive)
            nx_private_secure_zero(record, record_size);
        free(record);
        return result;
    }
    size_t cursor = start + inserted_length;
    while (cursor < next.length && !next.grapheme[cursor]) ++cursor;
    if (record) record->anchor_after = record->cursor_after = cursor;
    nx_tb_commit(buffer, &next, cursor, cursor);
    nx_tb_clear_records(buffer, &buffer->redo);
    if (record) {
        while (buffer->undo &&
               buffer->history_used >
                   buffer->history_limit - record_size)
            nx_tb_evict_oldest(buffer);
        record->next = buffer->undo;
        buffer->undo = record;
        buffer->history_used += record_size;
    } else nx_tb_clear_records(buffer, &buffer->undo);
    return NX_OK;
}

nx_result nx_text_buffer_replace(nx_text_buffer *buffer,
                                  const char *bytes, size_t length) {
    if (!buffer || (!bytes && length) ||
        nx_utf8_validate(bytes, length, NULL) != NX_OK)
        return NX_ERROR_ARGUMENT;
    size_t start = buffer->anchor < buffer->cursor ?
                   buffer->anchor : buffer->cursor;
    size_t end = buffer->anchor > buffer->cursor ?
                 buffer->anchor : buffer->cursor;
    return nx_tb_edit(buffer, start, end - start, bytes, length);
}

nx_result nx_text_buffer_delete_surrounding(nx_text_buffer *buffer,
    size_t before, size_t after) {
    if (!buffer) return NX_ERROR_ARGUMENT;
    size_t selection_start = buffer->anchor < buffer->cursor ?
                             buffer->anchor : buffer->cursor;
    size_t selection_end = buffer->anchor > buffer->cursor ?
                           buffer->anchor : buffer->cursor;
    if (before > selection_start || after > buffer->length - selection_end)
        return NX_ERROR_ARGUMENT;
    size_t start = selection_start - before;
    size_t end = selection_end + after;
    if ((start < buffer->length &&
         ((unsigned char)buffer->bytes[start] & 0xc0u) == 0x80u) ||
        (end < buffer->length &&
         ((unsigned char)buffer->bytes[end] & 0xc0u) == 0x80u))
        return NX_ERROR_ARGUMENT;
    return nx_tb_edit(buffer, start, end - start, NULL, 0);
}

nx_result nx_text_buffer_delete_relative(nx_text_buffer *buffer,
    int32_t index, size_t length) {
    if (!buffer || !length) return NX_ERROR_ARGUMENT;
    size_t start;
    if (index < 0) {
        uint64_t before = (uint64_t)-(int64_t)index;
        if (before > buffer->cursor) return NX_ERROR_ARGUMENT;
        start = buffer->cursor - (size_t)before;
    } else {
        size_t after = (size_t)index;
        if (after > buffer->length - buffer->cursor)
            return NX_ERROR_ARGUMENT;
        start = buffer->cursor + after;
    }
    if (length > buffer->length - start ||
        (start < buffer->length &&
         ((unsigned char)buffer->bytes[start] & 0xc0u) == 0x80u) ||
        (start + length < buffer->length &&
         ((unsigned char)buffer->bytes[start + length] & 0xc0u) == 0x80u))
        return NX_ERROR_ARGUMENT;
    return nx_tb_edit(buffer, start, length, NULL, 0);
}

static size_t nx_tb_prior(const uint8_t *boundaries, size_t start) {
    while (start && !boundaries[--start]) {}
    return start;
}

static size_t nx_tb_next(const uint8_t *boundaries, size_t length,
                          size_t start) {
    while (start < length && !boundaries[++start]) {}
    return start;
}

static int nx_tb_hard_break(uint32_t scalar) {
    uint8_t lb = nx_ucd_lb(scalar);
    return lb == NX_UCD_LB_BK || lb == NX_UCD_LB_CR ||
           lb == NX_UCD_LB_LF || lb == NX_UCD_LB_NL;
}

static size_t nx_tb_line_edge(const nx_text_buffer *buffer,
                               size_t cursor, int end_edge) {
    size_t start = 0, offset = 0;
    while (offset < buffer->length) {
        size_t hard_start = offset;
        uint32_t scalar = 0;
        (void)nx_utf8_decode(buffer->bytes, buffer->length, &offset,
                             &scalar);
        if (!nx_tb_hard_break(scalar)) continue;
        if (scalar == '\r' && offset < buffer->length &&
            buffer->bytes[offset] == '\n') ++offset;
        if (cursor <= hard_start)
            return end_edge ? hard_start : start;
        if (cursor < offset)
            return end_edge ? hard_start : start;
        start = offset;
    }
    return end_edge ? buffer->length : start;
}

nx_result nx_text_buffer_move_cursor(nx_text_buffer *buffer,
                                      nx_text_move move,
                                      int extend_selection) {
    if (!buffer || move < NX_TEXT_MOVE_GRAPHEME_BACKWARD ||
        move > NX_TEXT_MOVE_DOCUMENT_END)
        return NX_ERROR_ARGUMENT;
    size_t cursor = buffer->cursor;
    int backward = move == NX_TEXT_MOVE_GRAPHEME_BACKWARD ||
                   move == NX_TEXT_MOVE_WORD_BACKWARD;
    int forward = move == NX_TEXT_MOVE_GRAPHEME_FORWARD ||
                  move == NX_TEXT_MOVE_WORD_FORWARD;
    if (!extend_selection && buffer->anchor != cursor &&
        (backward || forward)) {
        cursor = backward ? (buffer->anchor < cursor ? buffer->anchor :
                             cursor) : (buffer->anchor > cursor ?
                                        buffer->anchor : cursor);
    } else switch (move) {
    case NX_TEXT_MOVE_GRAPHEME_BACKWARD:
        cursor = nx_tb_prior(buffer->grapheme, cursor); break;
    case NX_TEXT_MOVE_GRAPHEME_FORWARD:
        cursor = nx_tb_next(buffer->grapheme, buffer->length, cursor); break;
    case NX_TEXT_MOVE_WORD_BACKWARD:
        do { cursor = nx_tb_prior(buffer->word, cursor); }
        while (cursor && !buffer->grapheme[cursor]);
        break;
    case NX_TEXT_MOVE_WORD_FORWARD:
        do { cursor = nx_tb_next(buffer->word, buffer->length, cursor); }
        while (cursor < buffer->length && !buffer->grapheme[cursor]);
        break;
    case NX_TEXT_MOVE_LINE_START:
        cursor = nx_tb_line_edge(buffer, cursor, 0); break;
    case NX_TEXT_MOVE_LINE_END:
        cursor = nx_tb_line_edge(buffer, cursor, 1); break;
    case NX_TEXT_MOVE_DOCUMENT_START: cursor = 0; break;
    case NX_TEXT_MOVE_DOCUMENT_END: cursor = buffer->length; break;
    default: return NX_ERROR_ARGUMENT;
    }
    if (!extend_selection) buffer->anchor = cursor;
    buffer->cursor = cursor;
    return NX_OK;
}

nx_result nx_text_buffer_delete_backward(nx_text_buffer *buffer) {
    if (!buffer) return NX_ERROR_ARGUMENT;
    size_t start = buffer->anchor < buffer->cursor ?
                   buffer->anchor : buffer->cursor;
    size_t end = buffer->anchor > buffer->cursor ?
                 buffer->anchor : buffer->cursor;
    if (start == end) start = nx_tb_prior(buffer->grapheme, start);
    return nx_tb_edit(buffer, start, end - start, NULL, 0);
}

nx_result nx_text_buffer_delete_forward(nx_text_buffer *buffer) {
    if (!buffer) return NX_ERROR_ARGUMENT;
    size_t start = buffer->anchor < buffer->cursor ?
                   buffer->anchor : buffer->cursor;
    size_t end = buffer->anchor > buffer->cursor ?
                 buffer->anchor : buffer->cursor;
    if (start == end)
        end = nx_tb_next(buffer->grapheme, buffer->length, end);
    return nx_tb_edit(buffer, start, end - start, NULL, 0);
}

int nx_text_buffer_can_undo(const nx_text_buffer *buffer) {
    return buffer && buffer->undo != NULL;
}

int nx_text_buffer_can_redo(const nx_text_buffer *buffer) {
    return buffer && buffer->redo != NULL;
}

static nx_result nx_tb_replay(nx_text_buffer *buffer, int redo) {
    if (!buffer) return NX_ERROR_ARGUMENT;
    nx_text_record **from = redo ? &buffer->redo : &buffer->undo;
    nx_text_record **to = redo ? &buffer->undo : &buffer->redo;
    nx_text_record *record = *from;
    if (!record) return NX_ERROR_STATE;
    size_t remove_length = redo ? record->removed_length :
                                  record->inserted_length;
    size_t insert_length = redo ? record->inserted_length :
                                  record->removed_length;
    const char *expected = record->data + (redo ? 0u :
                                           record->removed_length);
    const char *inserted = record->data + (redo ?
                                           record->removed_length : 0u);
    if (record->start > buffer->length ||
        remove_length > buffer->length - record->start ||
        memcmp(buffer->bytes + record->start, expected,
               remove_length) != 0) return NX_ERROR_STATE;
    nx_text_state next = {0};
    nx_result result = nx_tb_prepare_replace(buffer, record->start,
                                              remove_length, inserted,
                                              insert_length, &next);
    if (result != NX_OK) return result;
    size_t anchor = redo ? record->anchor_after : record->anchor_before;
    size_t cursor = redo ? record->cursor_after : record->cursor_before;
    if (anchor > next.length || cursor > next.length ||
        !next.grapheme[anchor] || !next.grapheme[cursor]) {
        nx_tb_free_state(&next, buffer->sensitive);
        return NX_ERROR_STATE;
    }
    nx_tb_commit(buffer, &next, anchor, cursor);
    *from = record->next;
    record->next = *to;
    *to = record;
    return NX_OK;
}

nx_result nx_text_buffer_undo(nx_text_buffer *buffer) {
    return nx_tb_replay(buffer, 0);
}

nx_result nx_text_buffer_redo(nx_text_buffer *buffer) {
    return nx_tb_replay(buffer, 1);
}

static size_t nx_tb_encode_scalar(uint32_t scalar, char bytes[4]) {
    if (scalar < 0x80u) {
        bytes[0] = (char)scalar;
        return 1u;
    }
    if (scalar < 0x800u) {
        bytes[0] = (char)(0xc0u | (scalar >> 6u));
        bytes[1] = (char)(0x80u | (scalar & 0x3fu));
        return 2u;
    }
    if (scalar < 0x10000u) {
        bytes[0] = (char)(0xe0u | (scalar >> 12u));
        bytes[1] = (char)(0x80u | ((scalar >> 6u) & 0x3fu));
        bytes[2] = (char)(0x80u | (scalar & 0x3fu));
        return 3u;
    }
    bytes[0] = (char)(0xf0u | (scalar >> 18u));
    bytes[1] = (char)(0x80u | ((scalar >> 12u) & 0x3fu));
    bytes[2] = (char)(0x80u | ((scalar >> 6u) & 0x3fu));
    bytes[3] = (char)(0x80u | (scalar & 0x3fu));
    return 4u;
}

static size_t nx_tb_word_prior(const nx_text_buffer *buffer,
                                size_t cursor) {
    do { cursor = nx_tb_prior(buffer->word, cursor); }
    while (cursor && !buffer->grapheme[cursor]);
    return cursor;
}

static size_t nx_tb_word_next(const nx_text_buffer *buffer,
                               size_t cursor) {
    do { cursor = nx_tb_next(buffer->word, buffer->length, cursor); }
    while (cursor < buffer->length && !buffer->grapheme[cursor]);
    return cursor;
}

nx_result nx_text_buffer_apply_event(nx_text_buffer *buffer,
                                      const nx_event *event,
                                      int *out_handled) {
    if (!buffer || !event || !out_handled) return NX_ERROR_ARGUMENT;
    *out_handled = 0;
    nx_result result = NX_OK;
    if (event->type == NX_EVENT_TEXT) {
        uint32_t scalar = event->scalar;
        if (scalar > 0x10ffffu ||
            (scalar >= 0xd800u && scalar <= 0xdfffu))
            return NX_ERROR_ARGUMENT;
        if (scalar < 0x20u || scalar == 0x7fu ||
            (event->modifiers & NX_MOD_SUPER) ||
            ((event->modifiers & NX_MOD_CONTROL) &&
             !(event->modifiers & NX_MOD_ALT)))
            return NX_OK;
        char bytes[4];
        size_t length = nx_tb_encode_scalar(scalar, bytes);
        result = nx_text_buffer_replace(buffer, bytes, length);
        if (buffer->sensitive) nx_private_secure_zero(bytes, sizeof(bytes));
    } else if (event->type == NX_EVENT_KEY_DOWN) {
        uint32_t modifiers = event->modifiers;
        if (modifiers & (NX_MOD_ALT | NX_MOD_SUPER)) return NX_OK;
        int control = (modifiers & NX_MOD_CONTROL) != 0;
        int extend = (modifiers & NX_MOD_SHIFT) != 0;
        switch (event->key) {
        case NX_KEY_LEFT:
        case NX_KEY_RIGHT:
            result = nx_text_buffer_move_cursor(buffer,
                event->key == NX_KEY_LEFT ?
                    (control ? NX_TEXT_MOVE_WORD_BACKWARD :
                               NX_TEXT_MOVE_GRAPHEME_BACKWARD) :
                    (control ? NX_TEXT_MOVE_WORD_FORWARD :
                               NX_TEXT_MOVE_GRAPHEME_FORWARD), extend);
            break;
        case NX_KEY_HOME:
        case NX_KEY_END:
            result = nx_text_buffer_move_cursor(buffer,
                event->key == NX_KEY_HOME ?
                    (control ? NX_TEXT_MOVE_DOCUMENT_START :
                               NX_TEXT_MOVE_LINE_START) :
                    (control ? NX_TEXT_MOVE_DOCUMENT_END :
                               NX_TEXT_MOVE_LINE_END), extend);
            break;
        case NX_KEY_BACKSPACE:
        case NX_KEY_DELETE:
            if (control && buffer->anchor == buffer->cursor) {
                size_t cursor = buffer->cursor;
                size_t start = event->key == NX_KEY_BACKSPACE ?
                               nx_tb_word_prior(buffer, cursor) : cursor;
                size_t end = event->key == NX_KEY_DELETE ?
                             nx_tb_word_next(buffer, cursor) : cursor;
                result = nx_tb_edit(buffer, start, end - start, NULL, 0);
            } else result = event->key == NX_KEY_BACKSPACE ?
                            nx_text_buffer_delete_backward(buffer) :
                            nx_text_buffer_delete_forward(buffer);
            break;
        case NX_KEY_ENTER:
            if (control) return NX_OK;
            result = nx_text_buffer_replace(buffer, "\n", 1u);
            break;
        case NX_KEY_A:
            if (!control) return NX_OK;
            result = nx_text_buffer_set_selection(buffer, 0u,
                                                   buffer->length);
            break;
        case NX_KEY_Z:
            if (!control) return NX_OK;
            result = extend ? nx_text_buffer_redo(buffer) :
                              nx_text_buffer_undo(buffer);
            if (result == NX_ERROR_STATE) result = NX_OK;
            break;
        case NX_KEY_Y:
            if (!control) return NX_OK;
            result = nx_text_buffer_redo(buffer);
            if (result == NX_ERROR_STATE) result = NX_OK;
            break;
        default: return NX_OK;
        }
    } else return NX_OK;
    if (result == NX_OK) *out_handled = 1;
    return result;
}

nx_result nx_numeric_parse(const char *bytes, size_t length,
                           double minimum, double maximum,
                           double *out_value, int *out_valid) {
    if (!out_value || !out_valid) return NX_ERROR_ARGUMENT;
    *out_value = 0.0;
    *out_valid = 0;
    if ((!bytes && length) || !isfinite(minimum) || !isfinite(maximum) ||
        maximum < minimum) return NX_ERROR_ARGUMENT;
    if (!length || length > 1024u) return NX_OK;
    size_t pos = 0;
    int negative = 0;
    if (bytes[pos] == '+' || bytes[pos] == '-') {
        negative = bytes[pos] == '-';
        if (++pos == length) return NX_OK;
    }
    uint64_t significand = 0;
    unsigned significant_digits = 0;
    int decimal_exponent = 0;
    size_t digit_count = 0;
    while (pos < length && bytes[pos] >= '0' && bytes[pos] <= '9') {
        unsigned digit = (unsigned)(bytes[pos++] - '0');
        ++digit_count;
        if (!significant_digits && !digit) continue;
        if (significant_digits < 18u) {
            significand = significand * 10u + digit;
            ++significant_digits;
        } else ++decimal_exponent;
    }
    if (pos < length && bytes[pos] == '.') {
        ++pos;
        while (pos < length && bytes[pos] >= '0' && bytes[pos] <= '9') {
            unsigned digit = (unsigned)(bytes[pos++] - '0');
            ++digit_count;
            if (!significant_digits && !digit) {
                --decimal_exponent;
                continue;
            }
            if (significant_digits < 18u) {
                significand = significand * 10u + digit;
                ++significant_digits;
                --decimal_exponent;
            }
        }
    }
    if (!digit_count) return NX_OK;
    int explicit_exponent = 0, exponent_negative = 0;
    if (pos < length && (bytes[pos] == 'e' || bytes[pos] == 'E')) {
        ++pos;
        if (pos < length && (bytes[pos] == '+' || bytes[pos] == '-'))
            exponent_negative = bytes[pos++] == '-';
        if (pos == length || bytes[pos] < '0' || bytes[pos] > '9')
            return NX_OK;
        while (pos < length && bytes[pos] >= '0' && bytes[pos] <= '9') {
            unsigned digit = (unsigned)(bytes[pos++] - '0');
            if (explicit_exponent < 10000)
                explicit_exponent = explicit_exponent * 10 + (int)digit;
            if (explicit_exponent > 10000) explicit_exponent = 10000;
        }
    }
    if (pos != length) return NX_OK;
    int exponent = decimal_exponent +
                   (exponent_negative ? -explicit_exponent : explicit_exponent);
    double value = (double)significand;
    if (significand && exponent) value *= pow(10.0, (double)exponent);
    if (negative) value = -value;
    if (!isfinite(value) || (significand && value == 0.0) ||
        value < minimum || value > maximum) return NX_OK;
    *out_value = value;
    *out_valid = 1;
    return NX_OK;
}

nx_result nx_numeric_input_apply_event(nx_text_buffer *buffer,
                                        const nx_event *event,
                                        double minimum, double maximum,
                                        double *committed_value,
                                        int *out_handled, int *out_valid) {
    if (!out_handled || !out_valid) return NX_ERROR_ARGUMENT;
    *out_handled = 0;
    *out_valid = 0;
    if (!buffer || !event || !committed_value ||
        !isfinite(*committed_value) || !isfinite(minimum) ||
        !isfinite(maximum) || maximum < minimum)
        return NX_ERROR_ARGUMENT;
    if (event->type == NX_EVENT_TEXT &&
        (event->scalar > 0x10ffffu ||
         (event->scalar >= 0xd800u && event->scalar <= 0xdfffu)))
        return NX_ERROR_ARGUMENT;
    if (event->type == NX_EVENT_KEY_DOWN && event->key == NX_KEY_ENTER) {
        *out_handled = 1;
    } else if (event->type == NX_EVENT_TEXT &&
               !((event->scalar >= '0' && event->scalar <= '9') ||
                 event->scalar == '+' || event->scalar == '-' ||
                 event->scalar == '.' || event->scalar == 'e' ||
                 event->scalar == 'E')) {
        /* Other text is left to the caller; the draft stays untouched. */
    } else {
        nx_result result = nx_text_buffer_apply_event(buffer, event,
                                                       out_handled);
        if (result != NX_OK) return result;
    }
    const char *bytes = NULL;
    size_t length = 0;
    nx_result result = nx_text_buffer_text(buffer, &bytes, &length);
    if (result != NX_OK) return result;
    double parsed = 0.0;
    result = nx_numeric_parse(bytes, length, minimum, maximum,
                              &parsed, out_valid);
    if (result != NX_OK) return result;
    if (event->type == NX_EVENT_KEY_DOWN && event->key == NX_KEY_ENTER &&
        *out_valid)
        *committed_value = parsed;
    return NX_OK;
}
