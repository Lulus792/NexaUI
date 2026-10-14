#include "nexaui/nexaui.h"
#include "unicode_data.h"
#include "font_internal.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct nx_shape_char {
    uint32_t scalar, script;
    size_t start, end;
    uint8_t level, gc, invisible;
} nx_shape_char;

static int nx_shape_weak_script(uint32_t script) {
    return script == NX_SFNT_TAG('Z','y','y','y') ||
           script == NX_SFNT_TAG('Z','i','n','h') ||
           script == NX_SFNT_TAG('Z','z','z','z');
}

static int nx_shape_mark(uint8_t gc) {
    return gc == NX_UCD_GC_MN || gc == NX_UCD_GC_ME;
}

static void nx_shape_resolve_scripts(nx_shape_char *chars, size_t count) {
    uint32_t prior = 0;
    for (size_t i = 0; i < count; ++i) {
        if (nx_shape_weak_script(chars[i].script)) {
            if (prior) chars[i].script = prior;
        } else prior = chars[i].script;
    }
    uint32_t next = NX_SFNT_TAG('Z','y','y','y');
    for (size_t i = count; i > 0; --i) {
        if (nx_shape_weak_script(chars[i - 1u].script))
            chars[i - 1u].script = next;
        else next = chars[i - 1u].script;
    }
}

static nx_result nx_shape_font_metrics(const nx_font **fonts,
                                        nx_font_metrics *metrics,
                                        uint8_t *loaded, size_t index) {
    if (!loaded[index]) {
        nx_result result = nx_font_get_metrics(fonts[index], &metrics[index]);
        if (result != NX_OK) return result;
        loaded[index] = 1;
    }
    return NX_OK;
}

nx_result nx_shape_text(const nx_font_collection *collection,
                        const char *bytes, size_t length,
                        nx_bidi_direction direction, double pixels_per_em,
                        nx_shaped_glyph *glyphs, size_t glyph_capacity,
                        nx_shape_run *runs, size_t run_capacity,
                        size_t *out_glyph_count, size_t *out_run_count,
                        uint8_t *out_paragraph_level) {
    if (!collection || (!bytes && length) || !out_glyph_count ||
        !out_run_count || !out_paragraph_level ||
        direction < NX_BIDI_AUTO || direction > NX_BIDI_RTL ||
        !isfinite(pixels_per_em) || pixels_per_em <= 0.0 ||
        pixels_per_em > 16384.0)
        return NX_ERROR_ARGUMENT;
    size_t font_count = 0;
    nx_result result = nx_font_collection_count(collection, &font_count);
    if (result != NX_OK || !font_count || font_count > NX_FONT_COLLECTION_MAX_FONTS)
        return NX_ERROR_ARGUMENT;
    const nx_font *fonts[NX_FONT_COLLECTION_MAX_FONTS];
    nx_font_metrics metrics[NX_FONT_COLLECTION_MAX_FONTS];
    uint8_t loaded[NX_FONT_COLLECTION_MAX_FONTS] = {0};
    for (size_t i = 0; i < font_count; ++i) {
        result = nx_font_collection_font_at(collection, (uint32_t)i, &fonts[i]);
        if (result != NX_OK) return result;
    }
    size_t count = 0, offset = 0;
    uint32_t scalar = 0;
    while (offset < length) {
        if (nx_utf8_decode(bytes, length, &offset, &scalar) != NX_OK)
            return NX_ERROR_ARGUMENT;
        ++count;
    }
    if (count > SIZE_MAX / sizeof(nx_shape_char) ||
        count > SIZE_MAX / sizeof(nx_shaped_glyph) ||
        count > SIZE_MAX / sizeof(nx_shape_run) ||
        count > SIZE_MAX / sizeof(size_t)) return NX_ERROR_LIMIT;
    nx_shape_char *chars = count ? malloc(count * sizeof(*chars)) : NULL;
    uint8_t *levels = count ? malloc(count) : NULL;
    size_t *bidi_order = count ? malloc(count * sizeof(*bidi_order)) : NULL;
    nx_shaped_glyph *shaped = count ? malloc(count * sizeof(*shaped)) : NULL;
    nx_shape_run *built_runs = count ? malloc(count * sizeof(*built_runs)) : NULL;
    uint8_t *pair_mask = count ? calloc(count, 1u) : NULL;
    if (count && (!chars || !levels || !bidi_order || !shaped ||
                  !built_runs || !pair_mask)) {
        free(chars); free(levels); free(bidi_order); free(shaped);
        free(built_runs); free(pair_mask);
        return NX_ERROR_MEMORY;
    }
    size_t bidi_count = 0, bidi_visual = 0;
    uint8_t paragraph = 0;
    result = nx_bidi_analyze(bytes, length, direction, levels, count,
                             bidi_order, count, &bidi_count, &bidi_visual,
                             &paragraph);
    if (result != NX_OK) goto done;
    (void)bidi_visual;
    offset = 0;
    for (size_t i = 0; i < count; ++i) {
        size_t start = offset;
        (void)nx_utf8_decode(bytes, length, &offset, &scalar);
        chars[i] = (nx_shape_char){0};
        chars[i].scalar = scalar;
        chars[i].start = start;
        chars[i].end = offset;
        chars[i].level = levels[i];
        chars[i].script = nx_ucd_script_tag(scalar);
        chars[i].gc = nx_ucd_gc(scalar);
        chars[i].invisible = levels[i] == UINT8_MAX ||
                             chars[i].gc == NX_UCD_GC_CC ||
                             chars[i].gc == NX_UCD_GC_CF ||
                             nx_ucd_di(scalar) == NX_UCD_DI_YES;
    }
    nx_shape_resolve_scripts(chars, count);
    size_t glyph_count = 0, run_count = 0;
    size_t cluster_byte = 0, first = 0;
    while (cluster_byte < length) {
        size_t cluster_end = 0;
        result = nx_grapheme_next(bytes, length, cluster_byte, &cluster_end);
        if (result != NX_OK) goto done;
        size_t end = first;
        while (end < count && chars[end].start < cluster_end) ++end;
        size_t chosen = SIZE_MAX;
        for (size_t f = 0; f < font_count; ++f) {
            int complete = 1, any = 0;
            for (size_t i = first; i < end; ++i) {
                if (chars[i].invisible) continue;
                any = 1;
                uint16_t glyph = 0;
                result = nx_font_glyph_index(fonts[f], chars[i].scalar, &glyph);
                if (result != NX_OK) goto done;
                if (!glyph) { complete = 0; break; }
            }
            if (any && complete) { chosen = f; break; }
        }
        for (size_t i = first; i < end; ++i) {
            if (chars[i].invisible) continue;
            size_t font_index = chosen;
            uint16_t glyph_index = 0;
            if (font_index != SIZE_MAX) {
                result = nx_font_glyph_index(fonts[font_index],
                                             chars[i].scalar, &glyph_index);
                if (result != NX_OK) goto done;
            } else {
                nx_font_match match;
                result = nx_font_collection_resolve(collection,
                                                    chars[i].scalar, &match);
                if (result != NX_OK) goto done;
                font_index = match.font ? match.font_index : 0u;
                glyph_index = match.glyph_index;
            }
            result = nx_shape_font_metrics(fonts, metrics, loaded, font_index);
            if (result != NX_OK) goto done;
            nx_shaped_glyph item = {0};
            item.font = fonts[font_index];
            item.font_index = (uint32_t)font_index;
            item.script_tag = chars[i].script;
            item.scalar = chars[i].scalar;
            item.scalar_index = i;
            item.byte_start = chars[i].start;
            item.byte_end = chars[i].end;
            item.cluster_byte_start = cluster_byte;
            item.glyph_index = glyph_index;
            item.bidi_level = chars[i].level;
            shaped[glyph_count++] = item;
        }
        first = end;
        cluster_byte = cluster_end;
    }
    /* GSUB works on logical-order spans with one font, script and bidi level.
       The supported lookups can only shrink the glyph array. */
    for (size_t start = 0; start < glyph_count;) {
        size_t end = start + 1u;
        while (end < glyph_count && shaped[end].font == shaped[start].font &&
               shaped[end].script_tag == shaped[start].script_tag &&
               shaped[end].bidi_level == shaped[start].bidi_level) ++end;
        size_t span_count = end - start;
        result = nx_gsub_apply(shaped[start].font, shaped[start].script_tag,
                               shaped + start, &span_count);
        if (result != NX_OK) goto done;
        if (span_count < end - start) {
            memmove(shaped + start + span_count, shaped + end,
                    (glyph_count - end) * sizeof(*shaped));
            glyph_count -= end - start - span_count;
        }
        start += span_count;
    }
    for (size_t i = 0; i < glyph_count; ++i) {
        nx_shaped_glyph *item = &shaped[i];
        size_t font_index = item->font_index;
        uint16_t natural_advance = 0;
        int16_t lsb = 0;
        result = nx_font_glyph_hmetrics(item->font, item->glyph_index,
                                        &natural_advance, &lsb);
        if (result != NX_OK) goto done;
        (void)lsb;
        double scale = pixels_per_em / metrics[font_index].units_per_em;
        double advance = natural_advance * scale;
        int mark = nx_shape_mark(nx_ucd_gc(item->scalar));
        item->advance_x = mark ? 0.0 : advance;
        if (mark) {
            for (size_t prior = i; prior > 0; --prior) {
                const nx_shaped_glyph *base = &shaped[prior - 1u];
                if (base->cluster_byte_start != item->cluster_byte_start)
                    break;
                if (base->advance_x > 0.0) {
                    item->offset_x = -base->advance_x +
                                     (base->advance_x - advance) * 0.5;
                    break;
                }
            }
        }
        if (!run_count || built_runs[run_count - 1u].font != item->font ||
            built_runs[run_count - 1u].script_tag != item->script_tag ||
            built_runs[run_count - 1u].bidi_level != item->bidi_level) {
            built_runs[run_count++] = (nx_shape_run){
                item->font, item->font_index, item->script_tag,
                i, 1u, item->bidi_level};
        } else ++built_runs[run_count - 1u].glyph_count;
    }
    for (size_t r = 0; r < run_count; ++r) {
        nx_shape_run *run = &built_runs[r];
        result = nx_gpos_apply(run->font, run->script_tag,
                               shaped + run->glyph_start, run->glyph_count,
                               pixels_per_em, pair_mask + run->glyph_start);
        if (result != NX_OK) goto done;
    }
    for (size_t i = 1; i < glyph_count; ++i) {
        nx_shaped_glyph *prior = &shaped[i - 1u];
        nx_shaped_glyph *item = &shaped[i];
        if (pair_mask[i - 1u] || nx_shape_mark(nx_ucd_gc(item->scalar)) ||
            prior->advance_x <= 0.0 || prior->font != item->font ||
            prior->script_tag != item->script_tag ||
            prior->bidi_level != item->bidi_level ||
            (item->bidi_level & 1u)) continue;
        double scale = pixels_per_em / metrics[prior->font_index].units_per_em;
        int32_t kern = 0;
        result = nx_font_kerning(item->font, prior->glyph_index,
                                 item->glyph_index, &kern);
        if (result != NX_OK) goto done;
        prior->advance_x += kern * scale;
    }
    if (glyph_capacity < glyph_count || run_capacity < run_count ||
        (glyph_count && !glyphs) || (run_count && !runs)) {
        *out_glyph_count = glyph_count;
        *out_run_count = run_count;
        *out_paragraph_level = paragraph;
        result = NX_ERROR_LIMIT;
        goto done;
    }
    if (glyph_count) memcpy(glyphs, shaped, glyph_count * sizeof(*glyphs));
    if (run_count) memcpy(runs, built_runs, run_count * sizeof(*runs));
    *out_glyph_count = glyph_count;
    *out_run_count = run_count;
    *out_paragraph_level = paragraph;
done:
    free(chars); free(levels); free(bidi_order); free(shaped);
    free(built_runs); free(pair_mask);
    return result;
}
