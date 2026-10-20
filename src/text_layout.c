#include "nexaui/nexaui.h"
#include "unicode_data.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct nx_text_layout {
    char *bytes;
    size_t length;
    uint8_t *grapheme, *breaks;
    nx_text_line *lines;
    nx_text_glyph *glyphs;
    nx_text_caret *carets;
    size_t line_count, glyph_count, caret_count;
    size_t line_capacity, glyph_capacity, caret_capacity;
    double width, height, line_height, ascent, max_width, pixels_per_em;
};

static int nx_tl_hard(uint32_t scalar) {
    uint8_t lb = nx_ucd_lb(scalar);
    return lb == NX_UCD_LB_BK || lb == NX_UCD_LB_CR ||
           lb == NX_UCD_LB_LF || lb == NX_UCD_LB_NL ||
           nx_ucd_bidi(scalar) == NX_UCD_BIDI_B;
}

static int nx_tl_space(uint32_t scalar) {
    return nx_ucd_lb(scalar) == NX_UCD_LB_SP;
}

static double nx_tl_visible_width(const nx_shaped_glyph *glyphs,
                                  size_t start, size_t end) {
    double width = 0.0;
    for (size_t i = start; i < end; ++i) width += glyphs[i].advance_x;
    for (size_t i = end; i > start &&
         nx_tl_space(glyphs[i - 1u].scalar); --i)
        width -= glyphs[i - 1u].advance_x;
    return width > 0.0 ? width : 0.0;
}

static nx_result nx_tl_add_line(nx_text_layout *layout,
                                const nx_shaped_glyph *glyphs,
                                size_t start, size_t end,
                                size_t byte_start, size_t byte_end,
                                size_t break_end, uint8_t paragraph_level) {
    if (layout->line_count >= layout->line_capacity ||
        end < start || end - start > layout->glyph_capacity - layout->glyph_count)
        return NX_ERROR_LIMIT;
    size_t line_index = layout->line_count;
    nx_text_line *line = &layout->lines[line_index];
    *line = (nx_text_line){0};
    line->byte_start = byte_start;
    line->byte_end = byte_end;
    line->break_byte_end = break_end;
    line->glyph_start = layout->glyph_count;
    line->glyph_count = end - start;
    line->top = line_index * layout->line_height;
    line->baseline_y = line->top + layout->ascent;
    line->height = layout->line_height;
    line->paragraph_level = paragraph_level;
    line->hard_break = break_end > byte_end;
    line->width = nx_tl_visible_width(glyphs, start, end);
    for (size_t i = start; i < end; ++i) {
        nx_text_glyph *dst = &layout->glyphs[layout->glyph_count++];
        dst->shape = glyphs[i];
        dst->line_index = line_index;
        dst->x = 0.0;
        dst->baseline_y = line->baseline_y;
    }
    for (size_t i = layout->glyph_count; i > line->glyph_start; --i) {
        if (!nx_tl_space(layout->glyphs[i - 1u].shape.scalar)) break;
        layout->glyphs[i - 1u].shape.advance_x = 0.0;
    }
    if (line->width > layout->width) layout->width = line->width;
    ++layout->line_count;
    layout->height = layout->line_count * layout->line_height;
    return NX_OK;
}

static int nx_tl_break_before(const nx_text_layout *layout,
                              const nx_shaped_glyph *left,
                              const nx_shaped_glyph *right) {
    return right->byte_start <= layout->length &&
           layout->breaks[right->byte_start] != NX_LINE_BREAK_NONE &&
           right->cluster_byte_start == right->byte_start &&
           left->byte_end <= right->byte_start;
}

static nx_result nx_tl_paragraph(nx_text_layout *layout,
                                 const nx_font_collection *collection,
                                 nx_bidi_direction direction,
                                 double pixels_per_em,
                                 size_t start_byte, size_t end_byte,
                                 size_t break_end, size_t scalar_base,
                                 nx_shaped_glyph *work, nx_shape_run *runs,
                                 size_t work_capacity) {
    size_t count = 0, run_count = 0;
    uint8_t paragraph_level = 0;
    nx_result result = nx_shape_text(collection,
                                     layout->bytes + start_byte,
                                     end_byte - start_byte,
                                     direction, pixels_per_em,
                                     work, work_capacity,
                                     runs, work_capacity,
                                     &count, &run_count, &paragraph_level);
    if (result != NX_OK) return result;
    for (size_t i = 0; i < count; ++i) {
        work[i].byte_start += start_byte;
        work[i].byte_end += start_byte;
        work[i].cluster_byte_start += start_byte;
        work[i].scalar_index += scalar_base;
    }
    if (!count)
        return nx_tl_add_line(layout, work, 0, 0, start_byte,
                              end_byte, break_end, paragraph_level);
    for (size_t first = 0; first < count;) {
        size_t selected = first;
        double total = 0.0, trailing_space = 0.0;
        for (size_t i = first; i < count; ++i) {
            total += work[i].advance_x;
            if (nx_tl_space(work[i].scalar))
                trailing_space += work[i].advance_x;
            else trailing_space = 0.0;
            size_t next = i + 1u;
            if (next < count &&
                !nx_tl_break_before(layout, &work[i], &work[next]))
                continue;
            double width = total - trailing_space;
            if (width < 0.0) width = 0.0;
            if (width <= layout->max_width || selected == first)
                selected = next;
            else break;
        }
        if (selected == first) selected = count;
        size_t line_start = first == 0 ? start_byte : work[first].byte_start;
        size_t line_end = selected == count ? end_byte :
                          work[selected].byte_start;
        size_t line_break_end = selected == count ? break_end : line_end;
        result = nx_tl_add_line(layout, work, first, selected,
                                line_start, line_end, line_break_end,
                                paragraph_level);
        if (result != NX_OK) return result;
        first = selected;
    }
    return NX_OK;
}

static nx_result nx_tl_add_caret(nx_text_layout *layout, size_t byte,
                                 size_t line_index, double x,
                                 nx_caret_affinity affinity) {
    if (layout->caret_count >= layout->caret_capacity)
        return NX_ERROR_LIMIT;
    const nx_text_line *line = &layout->lines[line_index];
    layout->carets[layout->caret_count++] = (nx_text_caret){
        byte, line_index, x, line->top, line->height, affinity};
    return NX_OK;
}

static int nx_tl_caret_compare(const void *a, const void *b) {
    const nx_text_caret *left = a, *right = b;
    if (left->line_index != right->line_index)
        return left->line_index < right->line_index ? -1 : 1;
    if (left->byte_offset != right->byte_offset)
        return left->byte_offset < right->byte_offset ? -1 : 1;
    if (left->x != right->x) return left->x < right->x ? -1 : 1;
    return (int)right->affinity - (int)left->affinity;
}

static int nx_tl_has_caret(const nx_text_caret *carets, size_t count,
                           size_t line_index, size_t byte) {
    size_t low = 0, high = count;
    while (low < high) {
        size_t mid = low + (high - low) / 2u;
        const nx_text_caret *item = &carets[mid];
        if (item->line_index < line_index ||
            (item->line_index == line_index && item->byte_offset < byte))
            low = mid + 1u;
        else high = mid;
    }
    return low < count && carets[low].line_index == line_index &&
           carets[low].byte_offset == byte;
}

static double nx_tl_glyph_boundary_x(const nx_text_glyph *glyph,
                                     int at_end) {
    int rtl = (glyph->shape.bidi_level & 1u) != 0;
    return glyph->x + ((at_end != rtl) ? glyph->shape.advance_x : 0.0);
}

static double nx_tl_fallback_x(const nx_text_line *line,
                               const nx_text_glyph *prior,
                               const nx_text_glyph *next) {
    if (next) return nx_tl_glyph_boundary_x(next, 0);
    if (prior) return nx_tl_glyph_boundary_x(prior, 1);
    return (line->paragraph_level & 1u) ? line->width : 0.0;
}

static nx_result nx_tl_reshape_lines(nx_text_layout *layout,
                                      const nx_font_collection *collection,
                                      nx_bidi_direction direction,
                                      double pixels_per_em,
                                      nx_shaped_glyph *work,
                                      nx_shape_run *runs,
                                      size_t work_capacity,
                                      uint8_t *levels) {
    layout->width = 0.0;
    /* One line covering the complete input is the original shaped paragraph:
       same bytes, direction, fonts and ppem, with no new line boundary. Keep
       its glyphs and resolved attachments rather than shaping it a second
       time. add_line already zeroed trailing-space advances. Sum those in the
       same order as the general reshape path, preserving exact line widths. */
    if (layout->line_count == 1u && layout->lines[0].byte_start == 0u &&
        layout->lines[0].byte_end == layout->length) {
        nx_text_line *line = &layout->lines[0];
        line->width = 0.0;
        for (size_t g = 0; g < line->glyph_count; ++g)
            line->width += layout->glyphs[line->glyph_start + g].shape.advance_x;
        if (line->width < 0.0) line->width = 0.0;
        layout->width = line->width;
        return NX_OK;
    }
    size_t scalar_index = 0, byte_cursor = 0;
    for (size_t i = 0; i < layout->line_count; ++i) {
        nx_text_line *line = &layout->lines[i];
        while (byte_cursor < line->byte_start) {
            uint32_t scalar = 0;
            (void)nx_utf8_decode(layout->bytes, layout->length,
                                 &byte_cursor, &scalar);
            ++scalar_index;
        }
        if (line->byte_end > line->byte_start) {
            size_t count = 0, run_count = 0;
            uint8_t paragraph_level = 0;
            nx_result result = nx_shape_text(collection,
                layout->bytes + line->byte_start,
                line->byte_end - line->byte_start,
                direction, pixels_per_em, work, work_capacity,
                runs, work_capacity, &count, &run_count,
                &paragraph_level);
            if (result != NX_OK) return result;
            size_t old_count = line->glyph_count;
            size_t old_cursor = 0;
            for (size_t g = 0; g < count; ++g) {
                size_t start = work[g].byte_start + line->byte_start;
                while (old_cursor + 1u < old_count &&
                       layout->glyphs[line->glyph_start + old_cursor]
                           .shape.byte_end <= start)
                    ++old_cursor;
                levels[g] = old_count ?
                    layout->glyphs[line->glyph_start + old_cursor]
                        .shape.bidi_level : work[g].bidi_level;
            }
            size_t tail_start = line->glyph_start + old_count;
            size_t tail_count = layout->glyph_count - tail_start;
            if (count > old_count &&
                count - old_count > layout->glyph_capacity - layout->glyph_count)
                return NX_ERROR_LIMIT;
            if (count != old_count) {
                memmove(layout->glyphs + line->glyph_start + count,
                        layout->glyphs + tail_start,
                        tail_count * sizeof(*layout->glyphs));
                if (count > old_count) {
                    size_t extra = count - old_count;
                    layout->glyph_count += extra;
                    for (size_t next = i + 1u; next < layout->line_count; ++next)
                        layout->lines[next].glyph_start += extra;
                } else {
                    size_t removed = old_count - count;
                    layout->glyph_count -= removed;
                    for (size_t next = i + 1u; next < layout->line_count; ++next)
                        layout->lines[next].glyph_start -= removed;
                }
                line->glyph_count = count;
            }
            for (size_t g = 0; g < count; ++g) {
                nx_text_glyph *dst = &layout->glyphs[line->glyph_start + g];
                dst->shape = work[g];
                dst->shape.byte_start += line->byte_start;
                dst->shape.byte_end += line->byte_start;
                dst->shape.cluster_byte_start += line->byte_start;
                dst->shape.scalar_index += scalar_index;
                dst->shape.bidi_level = levels[g];
                dst->line_index = i;
                dst->x = 0.0;
                dst->baseline_y = line->baseline_y;
            }
            for (size_t g = count; g > 0; --g) {
                nx_shaped_glyph *glyph =
                    &layout->glyphs[line->glyph_start + g - 1u].shape;
                if (!nx_tl_space(glyph->scalar)) break;
                glyph->advance_x = 0.0;
            }
            line->width = 0.0;
            for (size_t g = 0; g < line->glyph_count; ++g)
                line->width += layout->glyphs[line->glyph_start + g]
                               .shape.advance_x;
            if (line->width < 0.0) line->width = 0.0;
        }
        if (line->width > layout->width) layout->width = line->width;
    }
    return NX_OK;
}

static nx_result nx_tl_position(nx_text_layout *layout, size_t *order,
                                uint8_t *levels) {
    for (size_t line_index = 0; line_index < layout->line_count; ++line_index) {
        const nx_text_line *line = &layout->lines[line_index];
        size_t count = line->glyph_count;
        for (size_t i = 0; i < count; ++i) {
            order[i] = line->glyph_start + i;
            levels[i] = layout->glyphs[order[i]].shape.bidi_level;
        }
        for (size_t i = count; i > 0; --i) {
            uint8_t bidi = nx_ucd_bidi(layout->glyphs[order[i - 1u]]
                                        .shape.scalar);
            if (bidi != NX_UCD_BIDI_WS && bidi != NX_UCD_BIDI_LRI &&
                bidi != NX_UCD_BIDI_RLI && bidi != NX_UCD_BIDI_FSI &&
                bidi != NX_UCD_BIDI_PDI) break;
            levels[i - 1u] = line->paragraph_level;
            layout->glyphs[order[i - 1u]].shape.bidi_level =
                line->paragraph_level;
        }
        uint8_t highest = 0, lowest_odd = UINT8_MAX;
        for (size_t i = 0; i < count; ++i) {
            if (levels[i] > highest) highest = levels[i];
            if ((levels[i] & 1u) && levels[i] < lowest_odd)
                lowest_odd = levels[i];
        }
        if (lowest_odd != UINT8_MAX)
            for (unsigned level = highest; level >= lowest_odd; --level)
                for (size_t i = 0; i < count;) {
                    if (levels[i] < level) { ++i; continue; }
                    size_t first = i;
                    while (i < count && levels[i] >= level) ++i;
                    for (size_t left = first, right = i; left < --right; ++left) {
                        size_t temp_index = order[left];
                        order[left] = order[right];
                        order[right] = temp_index;
                        uint8_t temp_level = levels[left];
                        levels[left] = levels[right];
                        levels[right] = temp_level;
                    }
                }
        double pen = 0.0;
        for (size_t i = 0; i < count; ++i) {
            nx_text_glyph *glyph = &layout->glyphs[order[i]];
            glyph->x = pen;
            pen += glyph->shape.advance_x;
        }
        for (size_t i = line->glyph_start;
             i < line->glyph_start + count; ++i) {
            const nx_text_glyph *glyph = &layout->glyphs[i];
            size_t first = glyph->shape.byte_start;
            size_t last = glyph->shape.byte_end;
            if (first > last || last > layout->length)
                return NX_ERROR_STATE;
            size_t pieces = 0;
            for (size_t byte = first + 1u; byte <= last; ++byte)
                pieces += layout->grapheme[byte] != 0;
            if (!pieces) pieces = 1;
            size_t step = 0;
            for (size_t byte = first; byte <= last; ++byte) {
                if (!layout->grapheme[byte]) continue;
                if (byte > first) ++step;
                double fraction = (double)step / (double)pieces;
                double x = glyph->x +
                    ((glyph->shape.bidi_level & 1u) ? 1.0 - fraction :
                     fraction) * glyph->shape.advance_x;
                nx_caret_affinity affinity = byte == last ?
                    NX_CARET_UPSTREAM : NX_CARET_DOWNSTREAM;
                nx_result result = nx_tl_add_caret(layout, byte,
                                                    line_index, x, affinity);
                if (result != NX_OK) return result;
            }
        }
    }
    qsort(layout->carets, layout->caret_count,
          sizeof(*layout->carets), nx_tl_caret_compare);
    size_t existing = layout->caret_count;
    for (size_t line_index = 0; line_index < layout->line_count; ++line_index) {
        const nx_text_line *line = &layout->lines[line_index];
        size_t glyph_end = line->glyph_start + line->glyph_count;
        size_t prior_cursor = line->glyph_start;
        size_t next_cursor = line->glyph_start;
        for (size_t byte = line->byte_start; byte <= line->byte_end; ++byte) {
            while (prior_cursor < glyph_end &&
                   layout->glyphs[prior_cursor].shape.byte_end <= byte)
                ++prior_cursor;
            while (next_cursor < glyph_end &&
                   layout->glyphs[next_cursor].shape.byte_start < byte)
                ++next_cursor;
            if (!layout->grapheme[byte] ||
                nx_tl_has_caret(layout->carets, existing, line_index, byte))
                continue;
            const nx_text_glyph *prior = prior_cursor > line->glyph_start ?
                &layout->glyphs[prior_cursor - 1u] : NULL;
            const nx_text_glyph *next = next_cursor < glyph_end ?
                &layout->glyphs[next_cursor] : NULL;
            nx_result result = nx_tl_add_caret(layout, byte, line_index,
                nx_tl_fallback_x(line, prior, next), NX_CARET_DOWNSTREAM);
            if (result != NX_OK) return result;
        }
    }
    qsort(layout->carets, layout->caret_count,
          sizeof(*layout->carets), nx_tl_caret_compare);
    size_t kept = 0;
    for (size_t i = 0; i < layout->caret_count; ++i) {
        if (kept && layout->carets[kept - 1u].line_index ==
                    layout->carets[i].line_index &&
            layout->carets[kept - 1u].byte_offset ==
                    layout->carets[i].byte_offset &&
            layout->carets[kept - 1u].x == layout->carets[i].x)
            continue;
        layout->carets[kept++] = layout->carets[i];
    }
    layout->caret_count = kept;
    return NX_OK;
}

nx_result nx_text_layout_create(const nx_font_collection *collection,
                                 const char *bytes, size_t length,
                                 nx_bidi_direction direction,
                                 double pixels_per_em, double max_width,
                                 nx_text_layout **out_layout) {
    if (!out_layout) return NX_ERROR_ARGUMENT;
    *out_layout = NULL;
    if (!collection || (!bytes && length) ||
        direction < NX_BIDI_AUTO || direction > NX_BIDI_RTL ||
        !isfinite(pixels_per_em) || pixels_per_em <= 0.0 ||
        pixels_per_em > 16384.0 || !isfinite(max_width) ||
        max_width <= 0.0 || length == SIZE_MAX ||
        nx_utf8_validate(bytes, length, NULL) != NX_OK)
        return NX_ERROR_ARGUMENT;
    size_t font_count = 0;
    if (nx_font_collection_count(collection, &font_count) != NX_OK ||
        !font_count) return NX_ERROR_ARGUMENT;
    double ascent = 0.0, descent = 0.0, gap = 0.0;
    for (size_t i = 0; i < font_count; ++i) {
        const nx_font *font = NULL;
        nx_font_metrics metrics;
        if (nx_font_collection_font_at(collection, (uint32_t)i, &font) !=
            NX_OK) return NX_ERROR_ARGUMENT;
        nx_result result = nx_font_get_metrics(font, &metrics);
        if (result != NX_OK) return result;
        double scale = pixels_per_em / metrics.units_per_em;
        if (metrics.ascender * scale > ascent)
            ascent = metrics.ascender * scale;
        if (-metrics.descender * scale > descent)
            descent = -metrics.descender * scale;
        if (metrics.line_gap * scale > gap)
            gap = metrics.line_gap * scale;
    }
    if (ascent + descent <= 0.0) ascent = pixels_per_em;
    size_t scalar_count = 0, offset = 0;
    uint32_t scalar = 0;
    while (offset < length) {
        (void)nx_utf8_decode(bytes, length, &offset, &scalar);
        ++scalar_count;
    }
    if (scalar_count > SIZE_MAX / 6u - 1u ||
        scalar_count + 1u > SIZE_MAX / sizeof(nx_text_line) ||
        scalar_count + 1u > SIZE_MAX / sizeof(nx_text_glyph) ||
        6u * (scalar_count + 1u) > SIZE_MAX / sizeof(nx_text_caret) ||
        scalar_count > SIZE_MAX / sizeof(nx_shaped_glyph) ||
        scalar_count > SIZE_MAX / sizeof(nx_shape_run) ||
        scalar_count > SIZE_MAX / sizeof(size_t)) return NX_ERROR_LIMIT;
    nx_text_layout *layout = calloc(1u, sizeof(*layout));
    if (!layout) return NX_ERROR_MEMORY;
    layout->length = length;
    layout->max_width = max_width;
    layout->pixels_per_em = pixels_per_em;
    layout->ascent = ascent;
    layout->line_height = ascent + descent + gap;
    layout->line_capacity = scalar_count + 1u;
    layout->glyph_capacity = scalar_count + 1u;
    layout->caret_capacity = 6u * (scalar_count + 1u);
    layout->bytes = malloc(length + 1u);
    layout->grapheme = calloc(length + 1u, 1u);
    layout->breaks = malloc(length + 1u);
    layout->lines = malloc(layout->line_capacity * sizeof(*layout->lines));
    layout->glyphs = malloc(layout->glyph_capacity * sizeof(*layout->glyphs));
    layout->carets = malloc(layout->caret_capacity * sizeof(*layout->carets));
    nx_shaped_glyph *work = scalar_count ?
        malloc(scalar_count * sizeof(*work)) : NULL;
    nx_shape_run *runs = scalar_count ?
        malloc(scalar_count * sizeof(*runs)) : NULL;
    size_t *order = scalar_count ? malloc(scalar_count * sizeof(*order)) : NULL;
    uint8_t *levels = scalar_count ? malloc(scalar_count) : NULL;
    if (!layout->bytes || !layout->grapheme || !layout->breaks ||
        !layout->lines || !layout->glyphs || !layout->carets ||
        (scalar_count && (!work || !runs || !order || !levels))) {
        free(work); free(runs); free(order); free(levels);
        nx_text_layout_destroy(layout);
        return NX_ERROR_MEMORY;
    }
    if (length) memcpy(layout->bytes, bytes, length);
    layout->bytes[length] = 0;
    layout->grapheme[0] = 1u;
    for (size_t start = 0; start < length;) {
        size_t next = 0;
        nx_result result = nx_grapheme_next(bytes, length, start, &next);
        if (result != NX_OK) {
            free(work); free(runs); free(order); free(levels);
            nx_text_layout_destroy(layout);
            return result;
        }
        layout->grapheme[next] = 1u;
        start = next;
    }
    nx_result result = nx_line_breaks(bytes, length, layout->breaks,
                                      length + 1u);
    size_t paragraph_start = 0, paragraph_scalars = 0, scalar_base = 0;
    for (size_t pos = 0; result == NX_OK && pos < length;) {
        size_t scalar_start = pos;
        (void)nx_utf8_decode(bytes, length, &pos, &scalar);
        if (!nx_tl_hard(scalar)) {
            ++paragraph_scalars;
            continue;
        }
        size_t hard_scalars = 1u;
        if (scalar == '\r' && pos < length && bytes[pos] == '\n') {
            ++pos;
            ++hard_scalars;
        }
        result = nx_tl_paragraph(layout, collection, direction,
                                  pixels_per_em, paragraph_start,
                                  scalar_start, pos, scalar_base,
                                  work, runs, scalar_count);
        scalar_base += paragraph_scalars + hard_scalars;
        paragraph_scalars = 0;
        paragraph_start = pos;
    }
    if (result == NX_OK)
        result = nx_tl_paragraph(layout, collection, direction,
                                  pixels_per_em, paragraph_start,
                                  length, length, scalar_base,
                                  work, runs, scalar_count);
    if (result == NX_OK)
        result = nx_tl_reshape_lines(layout, collection, direction,
                                      pixels_per_em, work, runs,
                                      scalar_count, levels);
    if (result == NX_OK) result = nx_tl_position(layout, order, levels);
    free(work); free(runs); free(order); free(levels);
    if (result != NX_OK) {
        nx_text_layout_destroy(layout);
        return result;
    }
    *out_layout = layout;
    return NX_OK;
}

void nx_text_layout_destroy(nx_text_layout *layout) {
    if (!layout) return;
    free(layout->bytes);
    free(layout->grapheme);
    free(layout->breaks);
    free(layout->lines);
    free(layout->glyphs);
    free(layout->carets);
    free(layout);
}

size_t nx_text_layout_line_count(const nx_text_layout *layout) {
    return layout ? layout->line_count : 0u;
}

size_t nx_text_layout_glyph_count(const nx_text_layout *layout) {
    return layout ? layout->glyph_count : 0u;
}

size_t nx_text_layout_caret_count(const nx_text_layout *layout) {
    return layout ? layout->caret_count : 0u;
}

int nx_text_layout_is_grapheme_boundary(const nx_text_layout *layout,
                                        size_t byte_offset) {
    return layout && byte_offset <= layout->length &&
           layout->grapheme[byte_offset] != 0;
}

double nx_text_layout_width(const nx_text_layout *layout) {
    return layout ? layout->width : 0.0;
}

double nx_text_layout_height(const nx_text_layout *layout) {
    return layout ? layout->height : 0.0;
}

double nx_text_layout_pixels_per_em(const nx_text_layout *layout) {
    return layout ? layout->pixels_per_em : 0.0;
}

nx_result nx_text_layout_text(const nx_text_layout *layout,
                              nx_utf8_span *out_text) {
    if (!out_text) return NX_ERROR_ARGUMENT;
    *out_text = (nx_utf8_span){0};
    if (!layout) return NX_ERROR_ARGUMENT;
    *out_text = (nx_utf8_span){layout->bytes, layout->length};
    return NX_OK;
}

nx_result nx_text_layout_line_at(const nx_text_layout *layout,
                                  size_t index, nx_text_line *out_line) {
    if (!layout || !out_line || index >= layout->line_count)
        return NX_ERROR_ARGUMENT;
    *out_line = layout->lines[index];
    return NX_OK;
}

nx_result nx_text_layout_glyph_at(const nx_text_layout *layout,
                                   size_t index, nx_text_glyph *out_glyph) {
    if (!layout || !out_glyph || index >= layout->glyph_count)
        return NX_ERROR_ARGUMENT;
    *out_glyph = layout->glyphs[index];
    return NX_OK;
}

nx_result nx_text_layout_caret_at(const nx_text_layout *layout,
                                   size_t index, nx_text_caret *out_caret) {
    if (!layout || !out_caret || index >= layout->caret_count)
        return NX_ERROR_ARGUMENT;
    *out_caret = layout->carets[index];
    return NX_OK;
}

nx_result nx_text_layout_hit_test(const nx_text_layout *layout,
                                   double x, double y,
                                   nx_text_caret *out_caret) {
    if (!layout || !out_caret || !isfinite(x) || !isfinite(y) ||
        !layout->line_count || !layout->caret_count)
        return NX_ERROR_ARGUMENT;
    size_t line = y < 0.0 ? 0u :
                  y >= layout->height ? layout->line_count - 1u :
                  (size_t)(y / layout->line_height);
    double distance = INFINITY;
    const nx_text_caret *nearest = NULL;
    for (size_t i = 0; i < layout->caret_count; ++i) {
        const nx_text_caret *item = &layout->carets[i];
        if (item->line_index != line) continue;
        double current = fabs(x - item->x);
        if (current < distance ||
            (current == distance && nearest &&
             item->affinity > nearest->affinity)) {
            distance = current;
            nearest = item;
        }
    }
    if (!nearest) return NX_ERROR_STATE;
    *out_caret = *nearest;
    return NX_OK;
}

static double nx_tl_fraction(const nx_text_layout *layout,
                             const nx_text_glyph *glyph, size_t byte) {
    size_t first = glyph->shape.byte_start;
    size_t last = glyph->shape.byte_end;
    if (byte <= first) return 0.0;
    if (byte >= last) return 1.0;
    size_t parts = 0, before = 0;
    for (size_t i = first + 1u; i <= last; ++i) {
        if (!layout->grapheme[i]) continue;
        ++parts;
        if (i <= byte) ++before;
    }
    return parts ? (double)before / (double)parts : 0.0;
}

static int nx_tl_rect_compare(const void *a, const void *b) {
    const nx_text_selection_rect *left = a, *right = b;
    if (left->line_index != right->line_index)
        return left->line_index < right->line_index ? -1 : 1;
    return left->x < right->x ? -1 : left->x > right->x ? 1 : 0;
}

nx_result nx_text_layout_selection(const nx_text_layout *layout,
                                    size_t byte_start, size_t byte_end,
                                    nx_text_selection_rect *rects,
                                    size_t capacity, size_t *out_count) {
    if (!layout || !out_count || byte_start > byte_end ||
        byte_end > layout->length || !layout->grapheme[byte_start] ||
        !layout->grapheme[byte_end] || (!rects && capacity))
        return NX_ERROR_ARGUMENT;
    nx_text_selection_rect *found = layout->glyph_count ?
        malloc(layout->glyph_count * sizeof(*found)) : NULL;
    if (layout->glyph_count && !found) return NX_ERROR_MEMORY;
    size_t count = 0;
    for (size_t i = 0; i < layout->glyph_count; ++i) {
        const nx_text_glyph *glyph = &layout->glyphs[i];
        size_t first = glyph->shape.byte_start > byte_start ?
                       glyph->shape.byte_start : byte_start;
        size_t last = glyph->shape.byte_end < byte_end ?
                      glyph->shape.byte_end : byte_end;
        if (first >= last) continue;
        double a = nx_tl_fraction(layout, glyph, first);
        double b = nx_tl_fraction(layout, glyph, last);
        if (glyph->shape.bidi_level & 1u) {
            a = 1.0 - a;
            b = 1.0 - b;
        }
        double xa = glyph->x + a * glyph->shape.advance_x;
        double xb = glyph->x + b * glyph->shape.advance_x;
        double width = fabs(xb - xa);
        if (width <= 0.0) continue;
        const nx_text_line *line = &layout->lines[glyph->line_index];
        found[count++] = (nx_text_selection_rect){
            glyph->line_index, xa < xb ? xa : xb,
            line->top, width, line->height};
    }
    if (count) qsort(found, count, sizeof(*found), nx_tl_rect_compare);
    size_t merged = 0;
    for (size_t i = 0; i < count; ++i) {
        if (merged && found[merged - 1u].line_index == found[i].line_index &&
            found[i].x <= found[merged - 1u].x + found[merged - 1u].width) {
            double end = found[i].x + found[i].width;
            if (end > found[merged - 1u].x + found[merged - 1u].width)
                found[merged - 1u].width = end - found[merged - 1u].x;
        } else found[merged++] = found[i];
    }
    if (capacity < merged) {
        *out_count = merged;
        free(found);
        return NX_ERROR_LIMIT;
    }
    if (merged && !rects) {
        *out_count = merged;
        free(found);
        return NX_ERROR_LIMIT;
    }
    if (merged) memcpy(rects, found, merged * sizeof(*rects));
    *out_count = merged;
    free(found);
    return NX_OK;
}
