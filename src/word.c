#include "nexaui/nexaui.h"
#include "unicode_data.h"

static int nx_word_newline(uint8_t value) {
    return value == NX_UCD_WB_NEWLINE || value == NX_UCD_WB_CR ||
           value == NX_UCD_WB_LF;
}

static int nx_word_ignore(uint8_t value) {
    return value == NX_UCD_WB_EXTEND || value == NX_UCD_WB_FORMAT ||
           value == NX_UCD_WB_ZWJ;
}

static int nx_word_ah(uint8_t value) {
    return value == NX_UCD_WB_ALETTER || value == NX_UCD_WB_HEBREW_LETTER;
}

static int nx_word_mid_let(uint8_t value) {
    return value == NX_UCD_WB_MID_LETTER ||
           value == NX_UCD_WB_MID_NUM_LET ||
           value == NX_UCD_WB_SINGLE_QUOTE;
}

static int nx_word_mid_num(uint8_t value) {
    return value == NX_UCD_WB_MID_NUM ||
           value == NX_UCD_WB_MID_NUM_LET ||
           value == NX_UCD_WB_SINGLE_QUOTE;
}

static int nx_word_extensible(uint8_t value) {
    return nx_word_ah(value) || value == NX_UCD_WB_NUMERIC ||
           value == NX_UCD_WB_KATAKANA || value == NX_UCD_WB_EXTEND_NUM_LET;
}

static nx_result nx_word_peek_significant(const char *bytes, size_t length,
                                          size_t offset, uint8_t *out_value) {
    *out_value = NX_UCD_WB_OTHER;
    while (offset < length) {
        uint32_t scalar;
        if (nx_utf8_decode(bytes, length, &offset, &scalar) != NX_OK)
            return NX_ERROR_ARGUMENT;
        uint8_t value = nx_ucd_wb(scalar);
        if (!nx_word_ignore(value)) {
            *out_value = value;
            break;
        }
    }
    return NX_OK;
}

static nx_result nx_word_break(const char *bytes, size_t length,
                               size_t next_offset, uint8_t raw_left,
                               uint8_t left, uint8_t before_left,
                               uint8_t right, uint8_t right_ep,
                               size_t ri_count, int *out_break) {
    if (raw_left == NX_UCD_WB_CR && right == NX_UCD_WB_LF) {
        *out_break = 0; return NX_OK; /* WB3 */
    }
    if (nx_word_newline(raw_left) || nx_word_newline(right)) {
        *out_break = 1; return NX_OK; /* WB3a-b */
    }
    if (raw_left == NX_UCD_WB_ZWJ && right_ep) {
        *out_break = 0; return NX_OK; /* WB3c */
    }
    if (raw_left == NX_UCD_WB_WSEG_SPACE && right == NX_UCD_WB_WSEG_SPACE) {
        *out_break = 0; return NX_OK; /* WB3d */
    }
    if (nx_word_ignore(right)) {
        *out_break = 0; return NX_OK; /* WB4 */
    }
    if (nx_word_ah(left) && nx_word_ah(right)) {
        *out_break = 0; return NX_OK; /* WB5 */
    }
    if (nx_word_ah(left) && nx_word_mid_let(right)) {
        uint8_t lookahead;
        nx_result result = nx_word_peek_significant(bytes, length,
                                                    next_offset, &lookahead);
        if (result != NX_OK) return result;
        if (nx_word_ah(lookahead)) {
            *out_break = 0; return NX_OK; /* WB6 */
        }
    }
    if (nx_word_ah(before_left) && nx_word_mid_let(left) && nx_word_ah(right)) {
        *out_break = 0; return NX_OK; /* WB7 */
    }
    if (left == NX_UCD_WB_HEBREW_LETTER &&
        right == NX_UCD_WB_SINGLE_QUOTE) {
        *out_break = 0; return NX_OK; /* WB7a */
    }
    if (left == NX_UCD_WB_HEBREW_LETTER &&
        right == NX_UCD_WB_DOUBLE_QUOTE) {
        uint8_t lookahead;
        nx_result result = nx_word_peek_significant(bytes, length,
                                                    next_offset, &lookahead);
        if (result != NX_OK) return result;
        if (lookahead == NX_UCD_WB_HEBREW_LETTER) {
            *out_break = 0; return NX_OK; /* WB7b */
        }
    }
    if (before_left == NX_UCD_WB_HEBREW_LETTER &&
        left == NX_UCD_WB_DOUBLE_QUOTE &&
        right == NX_UCD_WB_HEBREW_LETTER) {
        *out_break = 0; return NX_OK; /* WB7c */
    }
    if ((left == NX_UCD_WB_NUMERIC && right == NX_UCD_WB_NUMERIC) ||
        (nx_word_ah(left) && right == NX_UCD_WB_NUMERIC) ||
        (left == NX_UCD_WB_NUMERIC && nx_word_ah(right))) {
        *out_break = 0; return NX_OK; /* WB8-10 */
    }
    if (before_left == NX_UCD_WB_NUMERIC && nx_word_mid_num(left) &&
        right == NX_UCD_WB_NUMERIC) {
        *out_break = 0; return NX_OK; /* WB11 */
    }
    if (left == NX_UCD_WB_NUMERIC && nx_word_mid_num(right)) {
        uint8_t lookahead;
        nx_result result = nx_word_peek_significant(bytes, length,
                                                    next_offset, &lookahead);
        if (result != NX_OK) return result;
        if (lookahead == NX_UCD_WB_NUMERIC) {
            *out_break = 0; return NX_OK; /* WB12 */
        }
    }
    if (left == NX_UCD_WB_KATAKANA && right == NX_UCD_WB_KATAKANA) {
        *out_break = 0; return NX_OK; /* WB13 */
    }
    if ((nx_word_extensible(left) && right == NX_UCD_WB_EXTEND_NUM_LET) ||
        (left == NX_UCD_WB_EXTEND_NUM_LET && nx_word_extensible(right))) {
        *out_break = 0; return NX_OK; /* WB13a-b */
    }
    if (left == NX_UCD_WB_REGIONAL_INDICATOR &&
        right == NX_UCD_WB_REGIONAL_INDICATOR && (ri_count & 1u)) {
        *out_break = 0; return NX_OK; /* WB15-16 */
    }
    *out_break = 1; /* WB999 */
    return NX_OK;
}

nx_result nx_word_next(const char *bytes, size_t length,
                       size_t start, size_t *out_next) {
    if (!bytes || !out_next || start >= length) return NX_ERROR_ARGUMENT;
    size_t offset = start;
    uint32_t scalar;
    if (nx_utf8_decode(bytes, length, &offset, &scalar) != NX_OK)
        return NX_ERROR_ARGUMENT;
    uint8_t raw_left = nx_ucd_wb(scalar);
    uint8_t left = nx_word_ignore(raw_left) ? NX_UCD_WB_OTHER : raw_left;
    uint8_t before_left = NX_UCD_WB_OTHER;
    size_t ri_count = left == NX_UCD_WB_REGIONAL_INDICATOR ? 1u : 0u;
    while (offset < length) {
        size_t next = offset;
        if (nx_utf8_decode(bytes, length, &next, &scalar) != NX_OK)
            return NX_ERROR_ARGUMENT;
        uint8_t right = nx_ucd_wb(scalar);
        int should_break;
        nx_result result = nx_word_break(bytes, length, next, raw_left,
                                         left, before_left, right,
                                         nx_ucd_ep(scalar), ri_count,
                                         &should_break);
        if (result != NX_OK) return result;
        if (should_break) {
            *out_next = offset;
            return NX_OK;
        }
        raw_left = right;
        if (!nx_word_ignore(right)) {
            before_left = left;
            left = right;
            ri_count = right == NX_UCD_WB_REGIONAL_INDICATOR ?
                       ri_count + 1u : 0u;
        }
        offset = next;
    }
    *out_next = length;
    return NX_OK;
}
