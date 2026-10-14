#include "nexaui/nexaui.h"
#include "unicode_data.h"

#include <string.h>

typedef struct nx_lb_char {
    size_t start, end, base_start;
    uint32_t scalar, base_scalar;
    uint8_t raw, cls, gc, eaw, ep;
    int inherited;
} nx_lb_char;

static uint8_t nx_lb_resolve(uint32_t scalar) {
    uint8_t value = nx_ucd_lb(scalar);
    if (value == NX_UCD_LB_AI || value == NX_UCD_LB_SG ||
        value == NX_UCD_LB_XX) return NX_UCD_LB_AL;
    if (value == NX_UCD_LB_CJ) return NX_UCD_LB_NS;
    if (value == NX_UCD_LB_SA) {
        uint8_t gc = nx_ucd_gc(scalar);
        return gc == NX_UCD_GC_MN || gc == NX_UCD_GC_MC ?
               NX_UCD_LB_CM : NX_UCD_LB_AL;
    }
    return value;
}

static int nx_lb_hard(uint8_t value) {
    return value == NX_UCD_LB_BK || value == NX_UCD_LB_CR ||
           value == NX_UCD_LB_LF || value == NX_UCD_LB_NL;
}

static int nx_lb_mark(uint8_t value) {
    return value == NX_UCD_LB_CM || value == NX_UCD_LB_ZWJ;
}

static int nx_lb_base_allowed(uint8_t value) {
    return !nx_lb_hard(value) && value != NX_UCD_LB_SP &&
           value != NX_UCD_LB_ZW;
}

static size_t nx_lb_previous_offset(const char *bytes, size_t offset) {
    --offset;
    while (offset && ((unsigned char)bytes[offset] & 0xc0u) == 0x80u)
        --offset;
    return offset;
}

static nx_lb_char nx_lb_read(const char *bytes, size_t length, size_t start,
                             const nx_lb_char *previous_char) {
    nx_lb_char result;
    result.start = start;
    result.end = start;
    result.base_start = start;
    result.scalar = 0;
    result.inherited = 0;
    (void)nx_utf8_decode(bytes, length, &result.end, &result.scalar);
    result.base_scalar = result.scalar;
    result.raw = nx_lb_resolve(result.scalar);
    result.cls = result.raw;
    result.gc = nx_ucd_gc(result.scalar);
    result.eaw = nx_ucd_eaw(result.scalar);
    result.ep = nx_ucd_ep(result.scalar);
    if (nx_lb_mark(result.raw)) {
        if (previous_char &&
            (previous_char->inherited ||
             (!nx_lb_mark(previous_char->raw) &&
              nx_lb_base_allowed(previous_char->raw)))) {
            result.cls = previous_char->cls;
            result.base_start = previous_char->base_start;
            result.base_scalar = previous_char->base_scalar;
            result.gc = previous_char->gc;
            result.eaw = previous_char->eaw;
            result.ep = previous_char->ep;
            result.inherited = 1;
            return result;
        }
        if (previous_char) {
            result.cls = NX_UCD_LB_AL;
            result.gc = NX_UCD_GC_LU;
            result.eaw = NX_UCD_EAW_NA;
            result.ep = NX_UCD_EP_NO;
            return result;
        }
        size_t cursor = start;
        while (cursor) {
            size_t previous = nx_lb_previous_offset(bytes, cursor);
            size_t end = previous;
            uint32_t scalar = 0;
            (void)nx_utf8_decode(bytes, length, &end, &scalar);
            uint8_t value = nx_lb_resolve(scalar);
            if (!nx_lb_mark(value)) {
                if (nx_lb_base_allowed(value)) {
                    result.cls = value;
                    result.base_start = previous;
                    result.base_scalar = scalar;
                    result.gc = nx_ucd_gc(scalar);
                    result.eaw = nx_ucd_eaw(scalar);
                    result.ep = nx_ucd_ep(scalar);
                    result.inherited = 1;
                }
                break;
            }
            cursor = previous;
        }
        if (!result.inherited) {
            result.cls = NX_UCD_LB_AL;
            result.gc = NX_UCD_GC_LU;
            result.eaw = NX_UCD_EAW_NA;
            result.ep = NX_UCD_EP_NO;
        }
    }
    return result;
}

static int nx_lb_before(const char *bytes, size_t length, size_t offset,
                        nx_lb_char *out) {
    if (!offset) return 0;
    *out = nx_lb_read(bytes, length, nx_lb_previous_offset(bytes, offset), NULL);
    return 1;
}

static int nx_lb_before_effective(const char *bytes, size_t length,
                                  nx_lb_char value, nx_lb_char *out) {
    return nx_lb_before(bytes, length, value.base_start, out);
}

static int nx_lb_after(const char *bytes, size_t length, size_t offset,
                       nx_lb_char *out) {
    if (offset == length) return 0;
    *out = nx_lb_read(bytes, length, offset, NULL);
    return 1;
}

static int nx_lb_east(uint8_t eaw) {
    return eaw == NX_UCD_EAW_F || eaw == NX_UCD_EAW_W ||
           eaw == NX_UCD_EAW_H;
}

static int nx_lb_alpha(uint8_t value) {
    return value == NX_UCD_LB_AL || value == NX_UCD_LB_HL;
}

static int nx_lb_hangul(uint8_t value) {
    return value == NX_UCD_LB_JL || value == NX_UCD_LB_JV ||
           value == NX_UCD_LB_JT || value == NX_UCD_LB_H2 ||
           value == NX_UCD_LB_H3;
}

static int nx_lb_aksara(const nx_lb_char *value) {
    return value->cls == NX_UCD_LB_AK || value->cls == NX_UCD_LB_AS ||
           value->base_scalar == 0x25CCu;
}

static int nx_lb_ak_or_dotted(const nx_lb_char *value) {
    return value->cls == NX_UCD_LB_AK || value->base_scalar == 0x25CCu;
}

static int nx_lb_initial_context(uint8_t value) {
    return nx_lb_hard(value) || value == NX_UCD_LB_OP ||
           value == NX_UCD_LB_QU || value == NX_UCD_LB_GL ||
           value == NX_UCD_LB_SP || value == NX_UCD_LB_ZW;
}

static int nx_lb_final_context(uint8_t value) {
    return nx_lb_hard(value) || value == NX_UCD_LB_SP ||
           value == NX_UCD_LB_GL || value == NX_UCD_LB_WJ ||
           value == NX_UCD_LB_CL || value == NX_UCD_LB_QU ||
           value == NX_UCD_LB_CP || value == NX_UCD_LB_EX ||
           value == NX_UCD_LB_IS || value == NX_UCD_LB_SY ||
           value == NX_UCD_LB_ZW;
}

static int nx_lb_hyphen_context(uint8_t value) {
    return nx_lb_hard(value) || value == NX_UCD_LB_SP ||
           value == NX_UCD_LB_ZW || value == NX_UCD_LB_CB ||
           value == NX_UCD_LB_GL;
}

static nx_lb_char nx_lb_nonspace_before(const char *bytes, size_t length,
                                        nx_lb_char current) {
    nx_lb_char before;
    while (current.cls == NX_UCD_LB_SP &&
           nx_lb_before(bytes, length, current.start, &before))
        current = before;
    return current;
}

static int nx_lb_numeric_stem(const char *bytes, size_t length,
                              nx_lb_char current) {
    nx_lb_char before;
    if (current.cls == NX_UCD_LB_CL || current.cls == NX_UCD_LB_CP) {
        if (!nx_lb_before_effective(bytes, length, current, &before)) return 0;
        current = before;
    }
    while (current.cls == NX_UCD_LB_SY || current.cls == NX_UCD_LB_IS) {
        if (!nx_lb_before_effective(bytes, length, current, &before)) return 0;
        current = before;
    }
    return current.cls == NX_UCD_LB_NU;
}

static int nx_lb_prefix_number(const char *bytes, size_t length,
                               nx_lb_char right) {
    nx_lb_char next;
    if (right.cls == NX_UCD_LB_NU) return 1;
    if (right.cls != NX_UCD_LB_OP ||
        !nx_lb_after(bytes, length, right.end, &next)) return 0;
    if (next.cls == NX_UCD_LB_IS) {
        if (!nx_lb_after(bytes, length, next.end, &next)) return 0;
    }
    return next.cls == NX_UCD_LB_NU;
}

static uint8_t nx_lb_boundary(const char *bytes, size_t length,
                              nx_lb_char left, nx_lb_char right,
                              size_t ri_count) {
    nx_lb_char before, after;

    if (left.raw == NX_UCD_LB_BK) return NX_LINE_BREAK_MANDATORY; /* LB4 */
    if (left.raw == NX_UCD_LB_CR && right.raw == NX_UCD_LB_LF)
        return NX_LINE_BREAK_NONE; /* LB5 */
    if (left.raw == NX_UCD_LB_CR || left.raw == NX_UCD_LB_LF ||
        left.raw == NX_UCD_LB_NL) return NX_LINE_BREAK_MANDATORY; /* LB5 */
    if (nx_lb_hard(right.raw)) return NX_LINE_BREAK_NONE; /* LB6 */
    if (right.raw == NX_UCD_LB_SP || right.raw == NX_UCD_LB_ZW)
        return NX_LINE_BREAK_NONE; /* LB7 */
    nx_lb_char base = nx_lb_nonspace_before(bytes, length, left);
    if (base.cls == NX_UCD_LB_ZW) return NX_LINE_BREAK_OPTIONAL; /* LB8 */
    if (left.raw == NX_UCD_LB_ZWJ) return NX_LINE_BREAK_NONE; /* LB8a */
    if (nx_lb_mark(right.raw) && right.inherited)
        return NX_LINE_BREAK_NONE; /* LB9 */
    if (nx_lb_mark(left.raw) && !left.inherited &&
        nx_lb_mark(right.raw) && !right.inherited)
        return NX_LINE_BREAK_NONE; /* LB10, LB28: remaining marks act as AL */
    if (left.cls == NX_UCD_LB_WJ || right.cls == NX_UCD_LB_WJ)
        return NX_LINE_BREAK_NONE; /* LB11 */
    if (left.cls == NX_UCD_LB_GL) return NX_LINE_BREAK_NONE; /* LB12 */
    if (right.cls == NX_UCD_LB_GL && left.cls != NX_UCD_LB_SP &&
        left.cls != NX_UCD_LB_HY && left.cls != NX_UCD_LB_HH)
        return NX_LINE_BREAK_NONE; /* LB12a */
    if (right.cls == NX_UCD_LB_CL || right.cls == NX_UCD_LB_CP ||
        right.cls == NX_UCD_LB_EX || right.cls == NX_UCD_LB_SY)
        return NX_LINE_BREAK_NONE; /* LB13 */
    int has_before = nx_lb_before_effective(bytes, length, left, &before);
    int has_after = nx_lb_after(bytes, length, right.end, &after);
    if (base.cls == NX_UCD_LB_OP) return NX_LINE_BREAK_NONE; /* LB14 */
    if (base.cls == NX_UCD_LB_QU && base.gc == NX_UCD_GC_PI &&
        (!nx_lb_before_effective(bytes, length, base, &before) ||
         nx_lb_initial_context(before.cls)))
        return NX_LINE_BREAK_NONE; /* LB15a */
    if (right.cls == NX_UCD_LB_QU && right.gc == NX_UCD_GC_PF &&
        (!has_after || nx_lb_final_context(after.cls)))
        return NX_LINE_BREAK_NONE; /* LB15b */
    if (left.cls == NX_UCD_LB_SP && right.cls == NX_UCD_LB_IS &&
        has_after && after.cls == NX_UCD_LB_NU)
        return NX_LINE_BREAK_OPTIONAL; /* LB15c */
    if (right.cls == NX_UCD_LB_IS) return NX_LINE_BREAK_NONE; /* LB15d */
    if ((base.cls == NX_UCD_LB_CL || base.cls == NX_UCD_LB_CP) &&
        right.cls == NX_UCD_LB_NS) return NX_LINE_BREAK_NONE; /* LB16 */
    if (base.cls == NX_UCD_LB_B2 && right.cls == NX_UCD_LB_B2)
        return NX_LINE_BREAK_NONE; /* LB17 */
    if (left.cls == NX_UCD_LB_SP) return NX_LINE_BREAK_OPTIONAL; /* LB18 */
    if ((right.cls == NX_UCD_LB_QU && right.gc != NX_UCD_GC_PI) ||
        (left.cls == NX_UCD_LB_QU && left.gc != NX_UCD_GC_PF))
        return NX_LINE_BREAK_NONE; /* LB19 */
    if (right.cls == NX_UCD_LB_QU &&
        (!nx_lb_east(left.eaw) || !has_after || !nx_lb_east(after.eaw)))
        return NX_LINE_BREAK_NONE; /* LB19a */
    if (left.cls == NX_UCD_LB_QU &&
        (!nx_lb_east(right.eaw) || !has_before || !nx_lb_east(before.eaw)))
        return NX_LINE_BREAK_NONE; /* LB19a */
    if (left.cls == NX_UCD_LB_CB || right.cls == NX_UCD_LB_CB)
        return NX_LINE_BREAK_OPTIONAL; /* LB20 */
    if ((left.cls == NX_UCD_LB_HY || left.cls == NX_UCD_LB_HH) &&
        nx_lb_alpha(right.cls) &&
        (!has_before || nx_lb_hyphen_context(before.cls)))
        return NX_LINE_BREAK_NONE; /* LB20a */
    if (right.cls == NX_UCD_LB_BA || right.cls == NX_UCD_LB_HH ||
        right.cls == NX_UCD_LB_HY || right.cls == NX_UCD_LB_NS ||
        left.cls == NX_UCD_LB_BB) return NX_LINE_BREAK_NONE; /* LB21 */
    if (has_before && before.cls == NX_UCD_LB_HL &&
        (left.cls == NX_UCD_LB_HY || left.cls == NX_UCD_LB_HH) &&
        right.cls != NX_UCD_LB_HL) return NX_LINE_BREAK_NONE; /* LB21a */
    if (left.cls == NX_UCD_LB_SY && right.cls == NX_UCD_LB_HL)
        return NX_LINE_BREAK_NONE; /* LB21b */
    if (right.cls == NX_UCD_LB_IN) return NX_LINE_BREAK_NONE; /* LB22 */
    if ((nx_lb_alpha(left.cls) && right.cls == NX_UCD_LB_NU) ||
        (left.cls == NX_UCD_LB_NU && nx_lb_alpha(right.cls)))
        return NX_LINE_BREAK_NONE; /* LB23 */
    if ((left.cls == NX_UCD_LB_PR &&
         (right.cls == NX_UCD_LB_ID || right.cls == NX_UCD_LB_EB ||
          right.cls == NX_UCD_LB_EM)) ||
        ((left.cls == NX_UCD_LB_ID || left.cls == NX_UCD_LB_EB ||
          left.cls == NX_UCD_LB_EM) && right.cls == NX_UCD_LB_PO))
        return NX_LINE_BREAK_NONE; /* LB23a */
    if (((left.cls == NX_UCD_LB_PR || left.cls == NX_UCD_LB_PO) &&
         nx_lb_alpha(right.cls)) ||
        (nx_lb_alpha(left.cls) &&
         (right.cls == NX_UCD_LB_PR || right.cls == NX_UCD_LB_PO)))
        return NX_LINE_BREAK_NONE; /* LB24 */
    if ((right.cls == NX_UCD_LB_PO || right.cls == NX_UCD_LB_PR) &&
        nx_lb_numeric_stem(bytes, length, left))
        return NX_LINE_BREAK_NONE; /* LB25 */
    if ((left.cls == NX_UCD_LB_PO || left.cls == NX_UCD_LB_PR) &&
        nx_lb_prefix_number(bytes, length, right))
        return NX_LINE_BREAK_NONE; /* LB25 */
    if ((left.cls == NX_UCD_LB_HY || left.cls == NX_UCD_LB_IS) &&
        right.cls == NX_UCD_LB_NU) return NX_LINE_BREAK_NONE; /* LB25 */
    if (right.cls == NX_UCD_LB_NU &&
        nx_lb_numeric_stem(bytes, length, left))
        return NX_LINE_BREAK_NONE; /* LB25 */
    if ((left.cls == NX_UCD_LB_JL &&
         (right.cls == NX_UCD_LB_JL || right.cls == NX_UCD_LB_JV ||
          right.cls == NX_UCD_LB_H2 || right.cls == NX_UCD_LB_H3)) ||
        ((left.cls == NX_UCD_LB_JV || left.cls == NX_UCD_LB_H2) &&
         (right.cls == NX_UCD_LB_JV || right.cls == NX_UCD_LB_JT)) ||
        ((left.cls == NX_UCD_LB_JT || left.cls == NX_UCD_LB_H3) &&
         right.cls == NX_UCD_LB_JT)) return NX_LINE_BREAK_NONE; /* LB26 */
    if ((nx_lb_hangul(left.cls) && right.cls == NX_UCD_LB_PO) ||
        (left.cls == NX_UCD_LB_PR && nx_lb_hangul(right.cls)))
        return NX_LINE_BREAK_NONE; /* LB27 */
    if (nx_lb_alpha(left.cls) && nx_lb_alpha(right.cls))
        return NX_LINE_BREAK_NONE; /* LB28 */
    if ((left.cls == NX_UCD_LB_AP && nx_lb_aksara(&right)) ||
        (nx_lb_aksara(&left) &&
         (right.cls == NX_UCD_LB_VF || right.cls == NX_UCD_LB_VI)) ||
        (left.cls == NX_UCD_LB_VI && has_before &&
         nx_lb_aksara(&before) && nx_lb_ak_or_dotted(&right)) ||
        (nx_lb_aksara(&left) && nx_lb_aksara(&right) &&
         has_after && after.cls == NX_UCD_LB_VF))
        return NX_LINE_BREAK_NONE; /* LB28a */
    if (left.cls == NX_UCD_LB_IS && nx_lb_alpha(right.cls))
        return NX_LINE_BREAK_NONE; /* LB29 */
    if (((nx_lb_alpha(left.cls) || left.cls == NX_UCD_LB_NU) &&
         right.cls == NX_UCD_LB_OP && !nx_lb_east(right.eaw)) ||
        (left.cls == NX_UCD_LB_CP && !nx_lb_east(left.eaw) &&
         (nx_lb_alpha(right.cls) || right.cls == NX_UCD_LB_NU)))
        return NX_LINE_BREAK_NONE; /* LB30 */
    if (left.cls == NX_UCD_LB_RI && right.cls == NX_UCD_LB_RI &&
        (ri_count & 1u))
        return NX_LINE_BREAK_NONE; /* LB30a */
    if ((left.cls == NX_UCD_LB_EB ||
         (left.ep == NX_UCD_EP_YES && left.gc == NX_UCD_GC_CN)) &&
        right.cls == NX_UCD_LB_EM) return NX_LINE_BREAK_NONE; /* LB30b */
    return NX_LINE_BREAK_OPTIONAL; /* LB31 */
}

nx_result nx_line_breaks(const char *bytes, size_t length,
                         uint8_t *out_breaks, size_t capacity) {
    if ((!bytes && length) || !out_breaks || capacity <= length)
        return NX_ERROR_ARGUMENT;
    if (length) {
        uintptr_t source = (uintptr_t)bytes;
        uintptr_t target = (uintptr_t)out_breaks;
        if ((target >= source && target - source < length) ||
            (source >= target && source - target <= length))
            return NX_ERROR_ARGUMENT;
    }
    if (nx_utf8_validate(bytes, length, NULL) != NX_OK)
        return NX_ERROR_ARGUMENT;
    memset(out_breaks, 0, length + 1u);
    if (length) {
        nx_lb_char left = nx_lb_read(bytes, length, 0, NULL);
        size_t ri_count = left.raw == NX_UCD_LB_RI ? 1u : 0u;
        while (left.end < length) {
            nx_lb_char right = nx_lb_read(bytes, length, left.end, &left);
            out_breaks[right.start] = nx_lb_boundary(bytes, length, left,
                                                      right, ri_count);
            if (right.raw == NX_UCD_LB_RI) ++ri_count;
            else if (!nx_lb_mark(right.raw)) ri_count = 0;
            left = right;
        }
    }
    out_breaks[length] = NX_LINE_BREAK_MANDATORY;
    return NX_OK;
}
