/* Count real shaper work without a wall-clock threshold or production hook.
   This TU supplies the production layout implementation; the normal library
   supplies the real shaper and fonts. Shared builds still call their shaper. */
#ifdef NEXAUI_SHARED
#undef NEXAUI_SHARED
#endif
#include "nexaui/nexaui.h"
#include <stdio.h>
#include <string.h>
#include "search_font_fixture.h"
static unsigned shape_calls;
static nx_result counted_shape(const nx_font_collection *fonts,
    const char *bytes, size_t length, nx_bidi_direction direction, double ppem,
    nx_shaped_glyph *glyphs, size_t glyph_capacity, nx_shape_run *runs,
    size_t run_capacity, size_t *glyph_count, size_t *run_count, uint8_t *level) {
    ++shape_calls;
    return nx_shape_text(fonts, bytes, length, direction, ppem, glyphs,
        glyph_capacity, runs, run_capacity, glyph_count, run_count, level);
}
#define nx_shape_text counted_shape
#ifdef NEXAUI_LAYOUT_WORK_SOURCE
#include NEXAUI_LAYOUT_WORK_SOURCE
#else
#include "../src/text_layout.c"
#endif
#undef nx_shape_text
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", \
    __FILE__, __LINE__, #x); return 1; } } while (0)

int main(void) {
    uint8_t bytes[512]; font_fixture(bytes);
    nx_font *font = NULL; nx_font_collection *fonts = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &fonts) == NX_OK);
    CHECK(nx_font_collection_add(fonts, font) == NX_OK);
    const char *whole[] = {"AB ABA", "AB ABA  ", "A\xcc\x81 B", "AB \xd7\x90\xd7\x91"};
    for (unsigned d = 0; d < 3; ++d) for (unsigned i = 0; i < 4; ++i) {
        nx_text_layout *layout = NULL; shape_calls = 0;
        CHECK(nx_text_layout_create(fonts, whole[i], strlen(whole[i]),
            (nx_bidi_direction)d, 14.5, 1000, &layout) == NX_OK);
        CHECK(nx_text_layout_line_count(layout) == 1 && shape_calls == 1);
        nx_text_layout_destroy(layout);
    }
    const char *split[] = {"AB AB AB", "AB\nAB", "AB\r\nAB", "AB\xe2\x80\xa8" "AB"};
    for (unsigned i = 0; i < 4; ++i) {
        nx_text_layout *layout = NULL; shape_calls = 0;
        CHECK(nx_text_layout_create(fonts, split[i], strlen(split[i]),
            NX_BIDI_AUTO, 14.5, i ? 1000 : 16, &layout) == NX_OK);
        CHECK(nx_text_layout_line_count(layout) > 1);
        /* New boundaries retain the original reshape path. */
        CHECK(shape_calls >= 1 + nx_text_layout_line_count(layout));
        nx_text_layout_destroy(layout);
    }
    nx_font_collection_destroy(fonts); nx_font_close(font);
    puts("Whole paragraphs shape once; soft/hard line boundaries still reshape.");
    return 0;
}
