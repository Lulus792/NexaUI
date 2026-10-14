#include "nexaui/nexaui.h"
#include "unicode_data.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct nx_bc {
    uint32_t scalar;
    size_t match;
    uint8_t original, type, level;
} nx_bc;

typedef struct nx_bs {
    uint8_t level, override, isolate;
} nx_bs;

typedef struct nx_br {
    size_t first, last, next;
    uint8_t level;
} nx_br;

static int nx_is_isolate(uint8_t type) {
    return type == NX_UCD_BIDI_LRI || type == NX_UCD_BIDI_RLI ||
           type == NX_UCD_BIDI_FSI;
}

static int nx_is_removed(uint8_t type) {
    return type == NX_UCD_BIDI_LRE || type == NX_UCD_BIDI_RLE ||
           type == NX_UCD_BIDI_LRO || type == NX_UCD_BIDI_RLO ||
           type == NX_UCD_BIDI_PDF || type == NX_UCD_BIDI_BN;
}

static uint8_t nx_dir(uint8_t level) {
    return level & 1u ? NX_UCD_BIDI_R : NX_UCD_BIDI_L;
}

static uint8_t nx_first_strong(const nx_bc *chars, size_t first,
                               size_t end, int stop_at_pdi) {
    unsigned isolates = 0;
    for (size_t i = first; i < end; ++i) {
        uint8_t type = chars[i].original;
        if (nx_is_isolate(type)) { ++isolates; continue; }
        if (type == NX_UCD_BIDI_PDI) {
            if (isolates) --isolates;
            else if (stop_at_pdi) break;
            continue;
        }
        if (isolates) continue;
        if (type == NX_UCD_BIDI_L || type == NX_UCD_BIDI_R ||
            type == NX_UCD_BIDI_AL) return type;
    }
    return NX_UCD_BIDI_ON;
}

static void nx_match_isolates(nx_bc *chars, size_t count, size_t *stack) {
    size_t depth = 0;
    for (size_t i = 0; i < count; ++i) {
        chars[i].match = SIZE_MAX;
        if (nx_is_isolate(chars[i].original)) stack[depth++] = i;
        else if (chars[i].original == NX_UCD_BIDI_PDI && depth) {
            size_t open = stack[--depth];
            chars[open].match = i;
            chars[i].match = open;
        }
    }
}

static void nx_explicit(nx_bc *chars, size_t count, uint8_t paragraph) {
    nx_bs stack[127];
    size_t top = 0;
    size_t overflow_isolates = 0, overflow_embeddings = 0;
    size_t valid_isolates = 0;
    stack[0] = (nx_bs){paragraph, UINT8_MAX, 0};
    for (size_t i = 0; i < count; ++i) {
        uint8_t type = chars[i].original;
        uint8_t level = stack[top].level;
        chars[i].level = level;
        chars[i].type = type;
        if (type == NX_UCD_BIDI_B) {
            chars[i].level = paragraph;
            top = 0;
            overflow_isolates = overflow_embeddings = valid_isolates = 0;
        } else if (type == NX_UCD_BIDI_RLE || type == NX_UCD_BIDI_LRE ||
                   type == NX_UCD_BIDI_RLO || type == NX_UCD_BIDI_LRO) {
            unsigned next = (unsigned)level + 1u;
            int right = type == NX_UCD_BIDI_RLE || type == NX_UCD_BIDI_RLO;
            if ((next & 1u) != (unsigned)right) ++next;
            if (next <= 125u && !overflow_isolates && !overflow_embeddings) {
                stack[++top] = (nx_bs){(uint8_t)next,
                    type == NX_UCD_BIDI_RLO ? NX_UCD_BIDI_R :
                    type == NX_UCD_BIDI_LRO ? NX_UCD_BIDI_L : UINT8_MAX, 0};
            } else if (!overflow_isolates) ++overflow_embeddings;
        } else if (nx_is_isolate(type)) {
            uint8_t resolved = type;
            if (type == NX_UCD_BIDI_FSI) {
                size_t end = chars[i].match == SIZE_MAX ? count : chars[i].match;
                uint8_t strong = nx_first_strong(chars, i + 1u, end, 1);
                resolved = strong == NX_UCD_BIDI_R || strong == NX_UCD_BIDI_AL ?
                           NX_UCD_BIDI_RLI : NX_UCD_BIDI_LRI;
            }
            unsigned next = (unsigned)level + 1u;
            int right = resolved == NX_UCD_BIDI_RLI;
            if ((next & 1u) != (unsigned)right) ++next;
            if (stack[top].override != UINT8_MAX) chars[i].type = stack[top].override;
            if (next <= 125u && !overflow_isolates && !overflow_embeddings) {
                stack[++top] = (nx_bs){(uint8_t)next, UINT8_MAX, 1};
                ++valid_isolates;
            } else ++overflow_isolates;
        } else if (type == NX_UCD_BIDI_PDI) {
            if (overflow_isolates) --overflow_isolates;
            else if (valid_isolates) {
                overflow_embeddings = 0;
                while (top && !stack[top].isolate) --top;
                if (top) --top;
                --valid_isolates;
            }
            chars[i].level = stack[top].level;
            if (stack[top].override != UINT8_MAX) chars[i].type = stack[top].override;
        } else if (type == NX_UCD_BIDI_PDF) {
            if (!overflow_isolates) {
                if (overflow_embeddings) --overflow_embeddings;
                else if (top && !stack[top].isolate) --top;
            }
        } else if (type != NX_UCD_BIDI_BN && stack[top].override != UINT8_MAX)
            chars[i].type = stack[top].override;
        if (nx_is_removed(type)) chars[i].level = UINT8_MAX;
    }
}

static int nx_neutral(uint8_t type) {
    return type == NX_UCD_BIDI_ON || type == NX_UCD_BIDI_WS ||
           type == NX_UCD_BIDI_S || type == NX_UCD_BIDI_B ||
           type == NX_UCD_BIDI_LRI || type == NX_UCD_BIDI_RLI ||
           type == NX_UCD_BIDI_FSI || type == NX_UCD_BIDI_PDI;
}

static uint8_t nx_strong_for_neutral(uint8_t type) {
    return type == NX_UCD_BIDI_EN || type == NX_UCD_BIDI_AN ?
           NX_UCD_BIDI_R : type;
}

static int nx_pair_compare(const void *left, const void *right) {
    size_t a = *(const size_t *)left, b = *(const size_t *)right;
    return (a > b) - (a < b);
}

static void nx_resolve_sequence(nx_bc *chars, const size_t *seq, size_t count,
                                uint8_t sos, uint8_t eos,
                                size_t *bracket_stack, size_t *pairs) {
    /* W1 */
    for (size_t p = 0; p < count; ++p) {
        nx_bc *c = &chars[seq[p]];
        if (c->type == NX_UCD_BIDI_NSM) {
            uint8_t prior = p ? chars[seq[p - 1u]].type : sos;
            c->type = nx_is_isolate(prior) || prior == NX_UCD_BIDI_PDI ?
                      NX_UCD_BIDI_ON : prior;
        }
    }
    /* W2, W3 */
    uint8_t strong = sos;
    for (size_t p = 0; p < count; ++p) {
        nx_bc *c = &chars[seq[p]];
        if (c->type == NX_UCD_BIDI_EN && strong == NX_UCD_BIDI_AL)
            c->type = NX_UCD_BIDI_AN;
        if (c->type == NX_UCD_BIDI_L || c->type == NX_UCD_BIDI_R ||
            c->type == NX_UCD_BIDI_AL) strong = c->type;
    }
    for (size_t p = 0; p < count; ++p)
        if (chars[seq[p]].type == NX_UCD_BIDI_AL)
            chars[seq[p]].type = NX_UCD_BIDI_R;
    /* W4 */
    for (size_t p = 1; p + 1u < count; ++p) {
        uint8_t left = chars[seq[p - 1u]].type;
        uint8_t right = chars[seq[p + 1u]].type;
        uint8_t *middle = &chars[seq[p]].type;
        if ((*middle == NX_UCD_BIDI_ES || *middle == NX_UCD_BIDI_CS) &&
            left == NX_UCD_BIDI_EN && right == NX_UCD_BIDI_EN)
            *middle = NX_UCD_BIDI_EN;
        else if (*middle == NX_UCD_BIDI_CS && left == NX_UCD_BIDI_AN &&
                 right == NX_UCD_BIDI_AN) *middle = NX_UCD_BIDI_AN;
    }
    /* W5 */
    for (size_t p = 0; p < count;) {
        if (chars[seq[p]].type != NX_UCD_BIDI_ET) { ++p; continue; }
        size_t start = p;
        while (p < count && chars[seq[p]].type == NX_UCD_BIDI_ET) ++p;
        if ((start && chars[seq[start - 1u]].type == NX_UCD_BIDI_EN) ||
            (p < count && chars[seq[p]].type == NX_UCD_BIDI_EN))
            for (size_t j = start; j < p; ++j)
                chars[seq[j]].type = NX_UCD_BIDI_EN;
    }
    /* W6, W7 */
    for (size_t p = 0; p < count; ++p) {
        uint8_t *type = &chars[seq[p]].type;
        if (*type == NX_UCD_BIDI_ES || *type == NX_UCD_BIDI_ET ||
            *type == NX_UCD_BIDI_CS) *type = NX_UCD_BIDI_ON;
    }
    strong = sos;
    for (size_t p = 0; p < count; ++p) {
        uint8_t *type = &chars[seq[p]].type;
        if (*type == NX_UCD_BIDI_EN && strong == NX_UCD_BIDI_L)
            *type = NX_UCD_BIDI_L;
        if (*type == NX_UCD_BIDI_L || *type == NX_UCD_BIDI_R) strong = *type;
    }
    /* N0: find nested bracket pairs, with a limit of 63 openers. */
    size_t depth = 0, pair_count = 0;
    for (size_t p = 0; p < count; ++p) {
        nx_bc *c = &chars[seq[p]];
        if (c->type != NX_UCD_BIDI_ON) continue;
        uint32_t paired = 0;
        uint8_t kind = nx_ucd_bidi_bracket(c->scalar, &paired);
        if (kind == 1u) {
            if (depth < 63u) bracket_stack[depth++] = p;
            else { pair_count = 0; break; }
        } else if (kind == 2u) {
            for (size_t d = depth; d > 0; --d) {
                size_t opening = bracket_stack[d - 1u];
                uint32_t candidate = 0;
                (void)nx_ucd_bidi_bracket(chars[seq[opening]].scalar,
                                           &candidate);
                if (candidate == c->scalar ||
                    (candidate == 0x232au && c->scalar == 0x3009u) ||
                    (candidate == 0x3009u && c->scalar == 0x232au)) {
                    pairs[pair_count++] = opening;
                    pairs[pair_count++] = p;
                    depth = d - 1u;
                    break;
                }
            }
        }
    }
    /* Pair discovery is by closer; process by opener. */
    qsort(pairs, pair_count / 2u, 2u * sizeof(size_t), nx_pair_compare);
    for (size_t k = 0; k < pair_count; k += 2u) {
        size_t open = pairs[k], close = pairs[k + 1u];
        uint8_t embedding = nx_dir(chars[seq[open]].level);
        uint8_t opposite = embedding == NX_UCD_BIDI_L ?
                           NX_UCD_BIDI_R : NX_UCD_BIDI_L;
        int has_embedding = 0, has_opposite = 0;
        for (size_t p = open + 1u; p < close; ++p) {
            uint8_t type = nx_strong_for_neutral(chars[seq[p]].type);
            has_embedding |= type == embedding;
            has_opposite |= type == opposite;
        }
        int resolved = -1;
        if (has_embedding) resolved = embedding;
        else if (has_opposite) {
            uint8_t preceding = sos;
            for (size_t p = open; p > 0; --p) {
                uint8_t type = nx_strong_for_neutral(chars[seq[p - 1u]].type);
                if (type == NX_UCD_BIDI_L || type == NX_UCD_BIDI_R) {
                    preceding = type; break;
                }
            }
            resolved = preceding == opposite ? opposite : embedding;
        }
        if (resolved >= 0) {
            chars[seq[open]].type = chars[seq[close]].type = (uint8_t)resolved;
            for (size_t edge = 0; edge < 2; ++edge) {
                size_t p = edge ? close : open;
                while (p + 1u < count &&
                       chars[seq[p + 1u]].original == NX_UCD_BIDI_NSM)
                    chars[seq[++p]].type = (uint8_t)resolved;
            }
        }
    }
    /* N1, N2 */
    for (size_t p = 0; p < count;) {
        if (!nx_neutral(chars[seq[p]].type)) { ++p; continue; }
        size_t start = p;
        while (p < count && nx_neutral(chars[seq[p]].type)) ++p;
        uint8_t before = start ? nx_strong_for_neutral(chars[seq[start - 1u]].type) : sos;
        uint8_t after = p < count ? nx_strong_for_neutral(chars[seq[p]].type) : eos;
        uint8_t resolved = before == after ? before :
                           nx_dir(chars[seq[start]].level);
        for (size_t j = start; j < p; ++j) chars[seq[j]].type = resolved;
    }
    /* I1, I2 */
    for (size_t p = 0; p < count; ++p) {
        nx_bc *c = &chars[seq[p]];
        if (c->level & 1u) {
            if (c->type == NX_UCD_BIDI_L || c->type == NX_UCD_BIDI_EN ||
                c->type == NX_UCD_BIDI_AN) ++c->level;
        } else if (c->type == NX_UCD_BIDI_R) ++c->level;
        else if (c->type == NX_UCD_BIDI_EN || c->type == NX_UCD_BIDI_AN)
            c->level += 2u;
    }
}

nx_result nx_bidi_analyze(const char *bytes, size_t length,
                          nx_bidi_direction direction,
                          uint8_t *levels, size_t levels_capacity,
                          size_t *visual_order, size_t order_capacity,
                          size_t *out_scalar_count,
                          size_t *out_visual_count,
                          uint8_t *out_paragraph_level) {
    if ((!bytes && length) || !out_scalar_count || !out_visual_count ||
        !out_paragraph_level || direction > NX_BIDI_RTL || direction < NX_BIDI_AUTO)
        return NX_ERROR_ARGUMENT;
    size_t count = 0, offset = 0;
    uint32_t scalar = 0;
    while (offset < length) {
        if (nx_utf8_decode(bytes, length, &offset, &scalar) != NX_OK)
            return NX_ERROR_ARGUMENT;
        if (count == SIZE_MAX) return NX_ERROR_LIMIT;
        ++count;
    }
    if (levels_capacity < count || order_capacity < count ||
        (count && (!levels || !visual_order))) return NX_ERROR_ARGUMENT;
    if (count > SIZE_MAX / sizeof(nx_bc) ||
        count > SIZE_MAX / sizeof(nx_br) ||
        count > SIZE_MAX / sizeof(size_t)) return NX_ERROR_LIMIT;
    nx_bc *chars = count ? (nx_bc *)calloc(count, sizeof(*chars)) : NULL;
    size_t *scratch = count ? (size_t *)malloc(count * sizeof(*scratch)) : NULL;
    size_t *visible = count ? (size_t *)malloc(count * sizeof(*visible)) : NULL;
    size_t *seq = count ? (size_t *)malloc(count * sizeof(*seq)) : NULL;
    size_t *pairs = count ? (size_t *)malloc(count * sizeof(*pairs)) : NULL;
    nx_br *runs = count ? (nx_br *)malloc(count * sizeof(*runs)) : NULL;
    uint8_t *seen = count ? (uint8_t *)calloc(count, 1u) : NULL;
    if (count && (!chars || !scratch || !visible || !seq || !pairs || !runs || !seen)) {
        free(chars); free(scratch); free(visible); free(seq); free(pairs); free(runs); free(seen);
        return NX_ERROR_MEMORY;
    }
    offset = 0;
    for (size_t i = 0; i < count; ++i) {
        (void)nx_utf8_decode(bytes, length, &offset, &chars[i].scalar);
        chars[i].original = nx_ucd_bidi(chars[i].scalar);
        if (chars[i].original == NX_UCD_BIDI_B && i + 1u < count) {
            free(chars); free(scratch); free(visible); free(seq); free(pairs); free(runs); free(seen);
            return NX_ERROR_ARGUMENT;
        }
    }
    nx_match_isolates(chars, count, scratch);
    uint8_t first_strong = direction == NX_BIDI_AUTO ?
                           nx_first_strong(chars, 0, count, 0) : NX_UCD_BIDI_ON;
    uint8_t paragraph = direction == NX_BIDI_LTR ? 0u :
                        direction == NX_BIDI_RTL ? 1u :
                        (first_strong == NX_UCD_BIDI_R ||
                         first_strong == NX_UCD_BIDI_AL);
    nx_explicit(chars, count, paragraph);
    size_t visible_count = 0;
    for (size_t i = 0; i < count; ++i)
        if (chars[i].level != UINT8_MAX) visible[visible_count++] = i;
    size_t run_count = 0;
    for (size_t p = 0; p < visible_count;) {
        size_t first = p;
        uint8_t level = chars[visible[p]].level;
        while (p < visible_count && chars[visible[p]].level == level)
            scratch[visible[p++]] = run_count;
        runs[run_count++] = (nx_br){first, p - 1u, SIZE_MAX, level};
    }
    for (size_t r = 0; r < run_count; ++r) {
        size_t last = visible[runs[r].last];
        if (!nx_is_isolate(chars[last].original) || chars[last].match == SIZE_MAX)
            continue;
        runs[r].next = scratch[chars[last].match];
    }
    for (size_t r = 0; r < run_count; ++r) {
        if (seen[r]) continue;
        size_t seq_count = 0, last_run = r;
        for (size_t current = r; current != SIZE_MAX && !seen[current];
             current = runs[current].next) {
            seen[current] = 1;
            last_run = current;
            for (size_t p = runs[current].first; p <= runs[current].last; ++p)
                seq[seq_count++] = visible[p];
        }
        uint8_t first_level = runs[r].level;
        uint8_t last_level = runs[last_run].level;
        uint8_t prior_level = paragraph, following_level = paragraph;
        if (r) prior_level = runs[r - 1u].level;
        size_t last_scalar = visible[runs[last_run].last];
        if (last_run + 1u < run_count &&
            !(nx_is_isolate(chars[last_scalar].original) &&
              chars[last_scalar].match == SIZE_MAX))
            following_level = runs[last_run + 1u].level;
        uint8_t sos = nx_dir(first_level > prior_level ? first_level : prior_level);
        uint8_t eos = nx_dir(last_level > following_level ? last_level : following_level);
        nx_resolve_sequence(chars, seq, seq_count, sos, eos, scratch, pairs);
    }
    /* L1: this API treats the paragraph as a single display line. */
    for (size_t p = 0; p < visible_count; ++p) {
        uint8_t type = chars[visible[p]].original;
        if (type == NX_UCD_BIDI_B || type == NX_UCD_BIDI_S) {
            chars[visible[p]].level = paragraph;
            size_t j = p;
            while (j && (chars[visible[j - 1u]].original == NX_UCD_BIDI_WS ||
                         nx_is_isolate(chars[visible[j - 1u]].original) ||
                         chars[visible[j - 1u]].original == NX_UCD_BIDI_PDI))
                chars[visible[--j]].level = paragraph;
        }
    }
    for (size_t p = visible_count; p > 0; --p) {
        uint8_t type = chars[visible[p - 1u]].original;
        if (type != NX_UCD_BIDI_WS && !nx_is_isolate(type) &&
            type != NX_UCD_BIDI_PDI) break;
        chars[visible[p - 1u]].level = paragraph;
    }
    for (size_t p = 0; p < visible_count; ++p) visual_order[p] = visible[p];
    uint8_t highest = 0, lowest_odd = UINT8_MAX;
    for (size_t p = 0; p < visible_count; ++p) {
        uint8_t level = chars[visible[p]].level;
        if (level > highest) highest = level;
        if ((level & 1u) && level < lowest_odd) lowest_odd = level;
    }
    if (lowest_odd != UINT8_MAX)
        for (unsigned level = highest; level >= lowest_odd; --level)
            for (size_t p = 0; p < visible_count;) {
                if (chars[visual_order[p]].level < level) { ++p; continue; }
                size_t start = p;
                while (p < visible_count && chars[visual_order[p]].level >= level) ++p;
                for (size_t left = start, right = p; left < --right; ++left) {
                    size_t temp = visual_order[left];
                    visual_order[left] = visual_order[right];
                    visual_order[right] = temp;
                }
            }
    for (size_t i = 0; i < count; ++i) levels[i] = chars[i].level;
    *out_scalar_count = count;
    *out_visual_count = visible_count;
    *out_paragraph_level = paragraph;
    free(chars); free(scratch); free(visible); free(seq); free(pairs); free(runs); free(seen);
    return NX_OK;
}
