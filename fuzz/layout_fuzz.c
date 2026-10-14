#include "nexaui/nexaui.h"
#include "../tests/fixture_font.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define require(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    abort(); \
} } while (0)

static void check_layout(const nx_text_layout *layout,
                         const char *bytes, size_t length) {
    nx_utf8_span text = {0};
    require(nx_text_layout_text(layout, &text) == NX_OK);
    require(text.length == length);
    require(!length || memcmp(text.data, bytes, length) == 0);
    require(nx_text_layout_is_grapheme_boundary(layout, 0));
    require(nx_text_layout_is_grapheme_boundary(layout, length));
    require(isfinite(nx_text_layout_width(layout)) &&
            nx_text_layout_width(layout) >= 0.0);
    require(isfinite(nx_text_layout_height(layout)) &&
            nx_text_layout_height(layout) > 0.0);

    size_t lines = nx_text_layout_line_count(layout);
    size_t glyphs = nx_text_layout_glyph_count(layout);
    size_t carets = nx_text_layout_caret_count(layout);
    require(lines > 0 && lines <= length + 1u);
    require(glyphs <= length && carets > 0);
    for (size_t i = 0; i < lines; ++i) {
        nx_text_line line;
        require(nx_text_layout_line_at(layout, i, &line) == NX_OK);
        require(line.byte_start <= line.byte_end &&
                line.byte_end <= line.break_byte_end &&
                line.break_byte_end <= length);
        require(line.glyph_start <= glyphs &&
                line.glyph_count <= glyphs - line.glyph_start);
        require(isfinite(line.top) && isfinite(line.width) &&
                isfinite(line.height) && line.height > 0.0);
    }
    for (size_t i = 0; i < glyphs; ++i) {
        nx_text_glyph glyph;
        require(nx_text_layout_glyph_at(layout, i, &glyph) == NX_OK);
        require(glyph.line_index < lines &&
                glyph.shape.byte_start <= glyph.shape.byte_end &&
                glyph.shape.byte_end <= length);
        require(isfinite(glyph.x) && isfinite(glyph.baseline_y) &&
                isfinite(glyph.shape.advance_x));
    }
    for (size_t i = 0; i < carets; ++i) {
        nx_text_caret caret;
        require(nx_text_layout_caret_at(layout, i, &caret) == NX_OK);
        require(caret.byte_offset <= length && caret.line_index < lines);
        require(nx_text_layout_is_grapheme_boundary(layout,
                                                    caret.byte_offset));
        require(isfinite(caret.x) && isfinite(caret.top) &&
                isfinite(caret.height) && caret.height > 0.0);
    }
    const double x[] = {-10.0, 0.0, nx_text_layout_width(layout) / 2.0,
                        nx_text_layout_width(layout) + 10.0};
    const double y[] = {-10.0, 0.0, nx_text_layout_height(layout) / 2.0,
                        nx_text_layout_height(layout) + 10.0};
    for (size_t i = 0; i < 4u; ++i) {
        nx_text_caret caret;
        require(nx_text_layout_hit_test(layout, x[i], y[i], &caret) == NX_OK);
        require(caret.line_index < lines && caret.byte_offset <= length);
        require(nx_text_layout_is_grapheme_boundary(layout,
                                                    caret.byte_offset));
    }
    size_t rect_count = 0;
    nx_result query = nx_text_layout_selection(layout, 0, length,
                                               NULL, 0, &rect_count);
    require(query == (rect_count ? NX_ERROR_LIMIT : NX_OK));
    require(rect_count <= glyphs);
    if (rect_count) {
        nx_text_selection_rect *rects = malloc(rect_count * sizeof(*rects));
        require(rects != NULL);
        size_t written = 0;
        require(nx_text_layout_selection(layout, 0, length, rects,
                                         rect_count, &written) == NX_OK);
        require(written == rect_count);
        for (size_t i = 0; i < written; ++i) {
            require(rects[i].line_index < lines);
            require(isfinite(rects[i].x) && isfinite(rects[i].y) &&
                    isfinite(rects[i].width) && isfinite(rects[i].height));
            require(rects[i].width > 0.0 && rects[i].height > 0.0);
        }
        free(rects);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 256u) return 0;
    const char *text = (const char *)data;
    uint8_t font_bytes[512];
    font_fixture(font_bytes);
    nx_font *font = NULL;
    nx_font_collection *collection = NULL;
    require(nx_font_open(font_bytes, sizeof(font_bytes), NULL, &font) == NX_OK);
    require(nx_font_collection_create(NULL, &collection) == NX_OK);
    require(nx_font_collection_add(collection, font) == NX_OK);
    const nx_bidi_direction directions[] = {NX_BIDI_AUTO, NX_BIDI_LTR,
                                            NX_BIDI_RTL};
    const double widths[] = {24.0, 96.0, 240.0};
    for (size_t i = 0; i < 3u; ++i) {
        nx_text_layout *layout = NULL;
        nx_result result = nx_text_layout_create(collection, text, size,
                                                  directions[i], 20.0,
                                                  widths[i], &layout);
        if (nx_utf8_validate(text, size, NULL) == NX_OK) {
            if (result != NX_OK)
                fprintf(stderr, "layout result=%d direction=%d width=%.1f size=%zu\n",
                        (int)result, (int)directions[i], widths[i], size);
            require(result == NX_OK && layout != NULL);
            check_layout(layout, text, size);
        } else {
            require(result == NX_ERROR_ARGUMENT && layout == NULL);
        }
        nx_text_layout_destroy(layout);
    }
    nx_font_collection_destroy(collection);
    nx_font_close(font);
    return 0;
}
