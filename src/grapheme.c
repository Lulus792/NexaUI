#include "nexaui/nexaui.h"
#include "unicode_data.h"

static int nx_grapheme_control(uint8_t value) {
    return value == NX_UCD_GCB_CONTROL || value == NX_UCD_GCB_CR ||
           value == NX_UCD_GCB_LF;
}

static int nx_grapheme_break(uint8_t left, uint8_t right,
                             uint8_t right_incb, uint8_t right_ep,
                             int linker_extend, int ep_before_zwj,
                             size_t ri_count) {
    if (left == NX_UCD_GCB_CR && right == NX_UCD_GCB_LF) return 0; /* GB3 */
    if (nx_grapheme_control(left) || nx_grapheme_control(right)) return 1; /* GB4-5 */
    if (left == NX_UCD_GCB_L &&
        (right == NX_UCD_GCB_L || right == NX_UCD_GCB_V ||
         right == NX_UCD_GCB_LV || right == NX_UCD_GCB_LVT)) return 0; /* GB6 */
    if ((left == NX_UCD_GCB_LV || left == NX_UCD_GCB_V) &&
        (right == NX_UCD_GCB_V || right == NX_UCD_GCB_T)) return 0; /* GB7 */
    if ((left == NX_UCD_GCB_LVT || left == NX_UCD_GCB_T) &&
        right == NX_UCD_GCB_T) return 0; /* GB8 */
    if (right == NX_UCD_GCB_EXTEND || right == NX_UCD_GCB_ZWJ ||
        right == NX_UCD_GCB_SPACING_MARK || left == NX_UCD_GCB_PREPEND)
        return 0; /* GB9-9b */
    if (linker_extend && right_incb == NX_UCD_INCB_CONSONANT) return 0; /* GB9c */
    if (left == NX_UCD_GCB_ZWJ && ep_before_zwj && right_ep) return 0; /* GB11 */
    if (left == NX_UCD_GCB_REGIONAL_INDICATOR &&
        right == NX_UCD_GCB_REGIONAL_INDICATOR && (ri_count & 1u)) return 0; /* GB12-13 */
    return 1; /* GB999 */
}

nx_result nx_grapheme_next(const char *bytes, size_t length,
                           size_t start, size_t *out_next) {
    if (!bytes || !out_next || start >= length) return NX_ERROR_ARGUMENT;
    size_t offset = start;
    uint32_t scalar = 0;
    if (nx_utf8_decode(bytes, length, &offset, &scalar) != NX_OK)
        return NX_ERROR_ARGUMENT;
    uint8_t previous = nx_ucd_gcb(scalar);
    int ep_extend = nx_ucd_ep(scalar) == NX_UCD_EP_YES;
    int ep_before_zwj = 0;
    int linker_extend = nx_ucd_incb(scalar) == NX_UCD_INCB_LINKER;
    size_t ri_count = previous == NX_UCD_GCB_REGIONAL_INDICATOR ? 1u : 0u;
    while (offset < length) {
        size_t next = offset;
        if (nx_utf8_decode(bytes, length, &next, &scalar) != NX_OK)
            return NX_ERROR_ARGUMENT;
        uint8_t current = nx_ucd_gcb(scalar);
        uint8_t incb = nx_ucd_incb(scalar);
        uint8_t ep = nx_ucd_ep(scalar);
        if (nx_grapheme_break(previous, current, incb, ep,
                              linker_extend, ep_before_zwj, ri_count)) {
            *out_next = offset;
            return NX_OK;
        }
        ep_before_zwj = current == NX_UCD_GCB_ZWJ && ep_extend;
        ep_extend = ep == NX_UCD_EP_YES ||
                    (current == NX_UCD_GCB_EXTEND && ep_extend);
        linker_extend = incb == NX_UCD_INCB_LINKER ||
                        (incb == NX_UCD_INCB_EXTEND && linker_extend);
        ri_count = current == NX_UCD_GCB_REGIONAL_INDICATOR ? ri_count + 1u : 0u;
        previous = current;
        offset = next;
    }
    *out_next = length;
    return NX_OK;
}
