#include "nexaui/nexaui.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", \
    __FILE__, __LINE__, #x); return 1; } } while (0)

static void put16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8u);
    p[1] = (uint8_t)value;
}

static void put32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24u);
    p[1] = (uint8_t)(value >> 16u);
    p[2] = (uint8_t)(value >> 8u);
    p[3] = (uint8_t)value;
}

static void fixture(uint8_t bytes[376]) {
    memset(bytes, 0, 376);
    put32(bytes, UINT32_C(0x00010000));
    put16(bytes + 4u, 6);
    const uint32_t tags[6] = {
        NX_SFNT_TAG('c','m','a','p'), NX_SFNT_TAG('h','e','a','d'),
        NX_SFNT_TAG('h','h','e','a'), NX_SFNT_TAG('h','m','t','x'),
        NX_SFNT_TAG('k','e','r','n'), NX_SFNT_TAG('m','a','x','p')
    };
    const uint32_t offsets[6] = {264,108,196,232,352,164};
    const uint32_t lengths[6] = {88,54,36,32,24,32};
    for (size_t i = 0; i < 6; ++i) {
        put32(bytes + 12u + 16u * i, tags[i]);
        put32(bytes + 20u + 16u * i, offsets[i]);
        put32(bytes + 24u + 16u * i, lengths[i]);
    }
    put32(bytes + 108u, UINT32_C(0x00010000));
    put32(bytes + 120u, UINT32_C(0x5f0f3cf5));
    put16(bytes + 126u, 1000);
    put32(bytes + 164u, UINT32_C(0x00010000));
    put16(bytes + 168u, 8);
    put32(bytes + 196u, UINT32_C(0x00010000));
    put16(bytes + 200u, 800);
    put16(bytes + 202u, (uint16_t)-200);
    put16(bytes + 230u, 8);
    for (size_t i = 0; i < 8; ++i) put16(bytes + 232u + 4u * i, 600);
    put16(bytes + 236u, 300);
    uint8_t *cmap = bytes + 264u;
    put16(cmap + 2u, 1);
    put16(cmap + 4u, 3);
    put16(cmap + 6u, 10);
    put32(cmap + 8u, 12);
    uint8_t *sub = cmap + 12u;
    put16(sub, 12);
    put32(sub + 4u, 64);
    put32(sub + 12u, 4);
    const uint32_t mappings[4][2] = {
        {' ',1},{'A',2},{'V',3},{0x05d0,5}
    };
    for (size_t i = 0; i < 4; ++i) {
        put32(sub + 16u + 12u * i, mappings[i][0]);
        put32(sub + 20u + 12u * i, mappings[i][0]);
        put32(sub + 24u + 12u * i, mappings[i][1]);
    }
    put16(bytes + 354u, 1);
    put16(bytes + 358u, 20);
    put16(bytes + 360u, 1);
    put16(bytes + 362u, 1);
    put16(bytes + 370u, 2);
    put16(bytes + 372u, 3);
    put16(bytes + 374u, (uint16_t)-100);
}

static void ligature_fixture(uint8_t bytes[512]) {
    memset(bytes, 0, 512);
    fixture(bytes);
    put32(bytes + 76u, NX_SFNT_TAG('G','S','U','B'));
    put32(bytes + 88u, 86);
    uint8_t *gsub = bytes + 352u;
    memset(gsub, 0, 86u);
    put32(gsub, UINT32_C(0x00010000));
    put16(gsub + 4u, 10);
    put16(gsub + 6u, 30);
    put16(gsub + 8u, 44);
    put16(gsub + 10u, 1);
    put32(gsub + 12u, NX_SFNT_TAG('l','a','t','n'));
    put16(gsub + 16u, 8);
    put16(gsub + 18u, 4);
    put16(gsub + 24u, UINT16_MAX);
    put16(gsub + 26u, 1);
    put16(gsub + 30u, 1);
    put32(gsub + 32u, NX_SFNT_TAG('l','i','g','a'));
    put16(gsub + 36u, 8);
    put16(gsub + 40u, 1);
    put16(gsub + 44u, 1);
    put16(gsub + 46u, 4);
    put16(gsub + 48u, 4);
    put16(gsub + 52u, 1);
    put16(gsub + 54u, 8);
    put16(gsub + 56u, 1);
    put16(gsub + 58u, 14);
    put16(gsub + 60u, 1);
    put16(gsub + 62u, 20);
    put16(gsub + 70u, 1);
    put16(gsub + 72u, 1);
    put16(gsub + 74u, 3);
    put16(gsub + 76u, 1);
    put16(gsub + 78u, 4);
    put16(gsub + 80u, 7);
    put16(gsub + 82u, 2);
    put16(gsub + 84u, 2);
}

static void raster_fixture(uint8_t bytes[512]) {
    uint8_t source[376];
    fixture(source);
    memset(bytes, 0, 512);
    memcpy(bytes, source, 12u);
    put16(bytes + 4u, 8);
    memcpy(bytes + 140u, source + 108u, 268u);
    for (size_t i = 0; i < 6u; ++i) {
        const uint8_t *old_entry = source + 12u + 16u * i;
        uint8_t *entry = bytes + 12u + 16u * i;
        memcpy(entry, old_entry, 16u);
        uint32_t offset = ((uint32_t)old_entry[8] << 24u) |
                          ((uint32_t)old_entry[9] << 16u) |
                          ((uint32_t)old_entry[10] << 8u) | old_entry[11];
        put32(entry + 8u, offset + 32u);
    }
    uint8_t *loca_entry = bytes + 12u + 16u * 6u;
    uint8_t *glyf_entry = bytes + 12u + 16u * 7u;
    put32(loca_entry, NX_SFNT_TAG('l','o','c','a'));
    put32(loca_entry + 8u, 408u);
    put32(loca_entry + 12u, 18u);
    put32(glyf_entry, NX_SFNT_TAG('g','l','y','f'));
    put32(glyf_entry + 8u, 428u);
    put32(glyf_entry + 12u, 20u);
    for (size_t i = 3u; i < 9u; ++i)
        put16(bytes + 408u + i * 2u, 10u);
    uint8_t *glyph = bytes + 428u;
    put16(glyph, 1u);
    put16(glyph + 6u, 100u);
    put16(glyph + 8u, 100u);
    put16(glyph + 10u, 2u);
    glyph[14] = 0x31;
    glyph[15] = 0x33;
    glyph[16] = 0x27;
    glyph[17] = glyph[18] = glyph[19] = 100u;
}

static int caret_exists(const nx_text_layout *layout, size_t line,
                         size_t byte, double x) {
    for (size_t i = 0; i < nx_text_layout_caret_count(layout); ++i) {
        nx_text_caret caret;
        if (nx_text_layout_caret_at(layout, i, &caret) == NX_OK &&
            caret.line_index == line && caret.byte_offset == byte &&
            caret.x == x) return 1;
    }
    return 0;
}

static int test_rich_text(void) {
    uint8_t raster_bytes[512];
    raster_fixture(raster_bytes);
    nx_font *font = NULL;
    nx_font_collection *collection = NULL;
    nx_text_layout *layout = NULL;
    nx_context *ctx = NULL;
    CHECK(nx_font_open(raster_bytes, sizeof(raster_bytes), NULL,
                       &font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &collection) == NX_OK);
    CHECK(nx_font_collection_add(collection, font) == NX_OK);
    CHECK(nx_text_layout_create(collection, "A A", 3, NX_BIDI_LTR,
                                20.0, 100.0, &layout) == NX_OK);
    CHECK(nx_text_layout_is_grapheme_boundary(layout, 0));
    CHECK(nx_text_layout_is_grapheme_boundary(layout, 3));
    CHECK(!nx_text_layout_is_grapheme_boundary(layout, 4));
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 64, 40, 0) == NX_OK);
    nx_text_style_span style = {0};
    style.byte_start = 2;
    style.byte_end = 3;
    style.flags = NX_TEXT_STYLE_FOREGROUND | NX_TEXT_STYLE_BACKGROUND |
                  NX_TEXT_STYLE_UNDERLINE | NX_TEXT_STYLE_STRIKE;
    style.foreground = (nx_color){1, 0, 0, 1};
    style.background = (nx_color){0, 0, 1, 1};
    nx_color white = {1, 1, 1, 1};
    CHECK(nx_draw_rich_text_layout(ctx, layout, (nx_point){2, 1}, white,
                                   &style, 1, 1u) == NX_ERROR_LIMIT);
    style.byte_end = 4;
    CHECK(nx_draw_rich_text_layout(ctx, layout, (nx_point){2, 1}, white,
                                   &style, 1, 65536u) == NX_ERROR_ARGUMENT);
    style.byte_end = 3;
    style.foreground.r = -1;
    CHECK(nx_draw_rich_text_layout(ctx, layout, (nx_point){2, 1}, white,
                                   &style, 1, 65536u) == NX_ERROR_ARGUMENT);
    style.foreground.r = 1;
    nx_text_style_span overlapping[2] = {style, style};
    CHECK(nx_draw_rich_text_layout(ctx, layout, (nx_point){2, 1}, white,
                                   overlapping, 2, 65536u) == NX_ERROR_ARGUMENT);
    style.flags |= 0x80000000u;
    CHECK(nx_draw_rich_text_layout(ctx, layout, (nx_point){2, 1}, white,
                                   &style, 1, 65536u) == NX_ERROR_ARGUMENT);
    style.flags &= ~0x80000000u;
    CHECK(nx_draw_rich_text_layout(ctx, layout, (nx_point){2, 1}, white,
                                   &style, 1, 65536u) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    const nx_command *commands = NULL;
    size_t count = 0;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count == 5);
    CHECK(commands[0].type == NX_COMMAND_FILL_RECT &&
          commands[0].color.b == 1 && commands[0].color.r == 0);
    CHECK(commands[1].type == NX_COMMAND_BITMAP &&
          commands[1].color.r == 1 && commands[1].color.g == 1);
    CHECK(commands[2].type == NX_COMMAND_BITMAP &&
          commands[2].color.r == 1 && commands[2].color.g == 0);
    CHECK(commands[3].type == NX_COMMAND_FILL_RECT &&
          commands[4].type == NX_COMMAND_FILL_RECT &&
          commands[3].color.r == 1 && commands[4].color.r == 1);
    uint8_t pixels[64u * 40u * 4u];
    CHECK(nx_headless_render(commands, count, pixels, 64, 40, 64u * 4u,
                             (nx_color){0, 0, 0, 0}) == NX_OK);
    unsigned red_pixels = 0, blue_pixels = 0, white_pixels = 0;
    for (size_t i = 0; i < sizeof(pixels); i += 4u) {
        if (pixels[i] > 100 && pixels[i + 1u] < 50 &&
            pixels[i + 2u] < 50) ++red_pixels;
        if (pixels[i] < 50 && pixels[i + 1u] < 50 &&
            pixels[i + 2u] > 100) ++blue_pixels;
        if (pixels[i] > 100 && pixels[i + 1u] > 100 &&
            pixels[i + 2u] > 100) ++white_pixels;
    }
    CHECK(red_pixels > 0 && blue_pixels > 0 && white_pixels > 0);
    CHECK(nx_begin_frame(ctx, 64, 40, 0) == NX_OK);
    CHECK(nx_rich_label(ctx, 1, 0, (nx_rect){0, 0, 40, 20}, layout,
                        (nx_point){2, 1}, white, &style, 1, 1u) == NX_ERROR_LIMIT);
    CHECK(nx_rich_label(ctx, 1, 0, (nx_rect){0, 0, 40, 20}, layout,
                        (nx_point){2, 1}, white, &style, 1, 65536u) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t semantic_count = 0;
    nx_semantic_view view;
    CHECK(nx_semantics_count(ctx, &semantic_count) == NX_OK &&
          semantic_count == 1);
    CHECK(nx_semantics_get(ctx, 0, &view) == NX_OK &&
          view.role == NX_ROLE_TEXT && view.name.length == 3 &&
          memcmp(view.name.data, "A A", 3) == 0);
    nx_text_layout_destroy(layout);

    CHECK(nx_text_layout_create(collection, "A A", 3, NX_BIDI_LTR,
                                20.0, 12.0, &layout) == NX_OK);
    CHECK(nx_text_layout_line_count(layout) == 2);
    style.byte_start = 0;
    style.byte_end = 3;
    style.flags = NX_TEXT_STYLE_BACKGROUND | NX_TEXT_STYLE_UNDERLINE;
    CHECK(nx_begin_frame(ctx, 64, 50, 0) == NX_OK);
    CHECK(nx_draw_rich_text_layout(ctx, layout, (nx_point){0, 0}, white,
                                   &style, 1, 65536u) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count >= 6);
    size_t background_count = 0, underline_count = 0;
    for (size_t i = 0; i < count; ++i) {
        if (commands[i].type != NX_COMMAND_FILL_RECT) continue;
        if (commands[i].color.r == 0 && commands[i].color.b == 1)
            ++background_count;
        else ++underline_count;
    }
    CHECK(background_count == 2 && underline_count == 2);
    nx_text_layout_destroy(layout);

    CHECK(nx_text_layout_create(collection, "A\xcc\x81", 3, NX_BIDI_LTR,
                                20.0, 100.0, &layout) == NX_OK);
    CHECK(!nx_text_layout_is_grapheme_boundary(layout, 1));
    style.byte_start = 0;
    style.byte_end = 1;
    CHECK(nx_begin_frame(ctx, 64, 40, 0) == NX_OK);
    CHECK(nx_draw_rich_text_layout(ctx, layout, (nx_point){0, 0}, white,
                                   &style, 1, 65536u) == NX_ERROR_ARGUMENT);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_text_layout_destroy(layout);
    nx_context_destroy(ctx);
    nx_font_collection_destroy(collection);
    nx_font_close(font);

    uint8_t ligature_bytes[512];
    ligature_fixture(ligature_bytes);
    CHECK(nx_font_open(ligature_bytes, sizeof(ligature_bytes), NULL,
                       &font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &collection) == NX_OK);
    CHECK(nx_font_collection_add(collection, font) == NX_OK);
    CHECK(nx_text_layout_create(collection, "VA", 2, NX_BIDI_LTR,
                                20.0, 100.0, &layout) == NX_OK);
    CHECK(nx_text_layout_is_grapheme_boundary(layout, 1));
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 20, 20, 0) == NX_OK);
    CHECK(nx_draw_rich_text_layout(ctx, layout, (nx_point){0, 0}, white,
                                   &style, 1, 65536u) == NX_ERROR_ARGUMENT);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_context_destroy(ctx);
    nx_text_layout_destroy(layout);
    nx_font_collection_destroy(collection);
    nx_font_close(font);
    return 0;
}

int main(void) {
    if (test_rich_text()) return 1;
    uint8_t bytes[376];
    fixture(bytes);
    nx_font *font = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    nx_font_collection *collection = NULL;
    CHECK(nx_font_collection_create(NULL, &collection) == NX_OK);
    CHECK(nx_font_collection_add(collection, font) == NX_OK);
    nx_text_layout *layout = NULL;
    CHECK(nx_text_layout_create(collection, "AV AV", 5, NX_BIDI_LTR,
                                20.0, 24.0, &layout) == NX_OK);
    CHECK(nx_text_layout_line_count(layout) == 2);
    CHECK(nx_text_layout_glyph_count(layout) == 5);
    CHECK(nx_text_layout_width(layout) == 22.0);
    CHECK(nx_text_layout_height(layout) == 40.0);
    nx_text_line line;
    CHECK(nx_text_layout_line_at(layout, 0, &line) == NX_OK);
    CHECK(line.byte_start == 0 && line.byte_end == 3 &&
          line.glyph_count == 3 && line.width == 22.0 &&
          line.baseline_y == 16.0 && !line.hard_break);
    CHECK(nx_text_layout_line_at(layout, 1, &line) == NX_OK);
    CHECK(line.byte_start == 3 && line.byte_end == 5 &&
          line.glyph_count == 2 && line.top == 20.0);
    nx_text_glyph glyph;
    CHECK(nx_text_layout_glyph_at(layout, line.glyph_start, &glyph) == NX_OK);
    CHECK(glyph.shape.byte_start == 3 && glyph.shape.scalar_index == 3 &&
          glyph.line_index == 1);
    CHECK(nx_text_layout_glyph_at(layout, line.glyph_start + 1u,
                                  &glyph) == NX_OK);
    CHECK(glyph.shape.byte_start == 4 && glyph.shape.scalar_index == 4);
    CHECK(caret_exists(layout, 0, 3, 22.0));
    CHECK(caret_exists(layout, 1, 3, 0.0));
    nx_text_caret caret;
    CHECK(nx_text_layout_hit_test(layout, 11.0, 1.0, &caret) == NX_OK);
    CHECK(caret.line_index == 0 && caret.byte_offset == 1);
    CHECK(nx_text_layout_hit_test(layout, 0.0, 21.0, &caret) == NX_OK);
    CHECK(caret.line_index == 1 && caret.byte_offset == 3);
    nx_text_selection_rect rects[3] = {{99,99,99,99,99}};
    size_t rect_count = 99;
    CHECK(nx_text_layout_selection(layout, 0, 5, rects, 0,
                                    &rect_count) == NX_ERROR_LIMIT);
    CHECK(rect_count == 2 && rects[0].line_index == 99);
    CHECK(nx_text_layout_selection(layout, 0, 5, rects, 3,
                                    &rect_count) == NX_OK);
    CHECK(rect_count == 2 && rects[0].width == 22.0 &&
          rects[1].line_index == 1 && rects[1].width == 22.0);
    nx_text_layout_destroy(layout);
    layout = NULL;
    CHECK(nx_text_layout_create(collection, "A\xe2\x80\x8bV", 5,
                                NX_BIDI_LTR, 20.0, 12.0, &layout) == NX_OK);
    CHECK(nx_text_layout_line_count(layout) == 2);
    CHECK(nx_text_layout_line_at(layout, 0, &line) == NX_OK);
    CHECK(line.byte_end == 4 && line.width == 12.0);
    CHECK(caret_exists(layout, 0, 4, 12.0));
    CHECK(caret_exists(layout, 1, 4, 0.0));
    nx_text_layout_destroy(layout);
    layout = NULL;
    CHECK(nx_text_layout_create(collection, "A\r\nV", 4,
                                NX_BIDI_AUTO, 20.0, 100.0, &layout) == NX_OK);
    CHECK(nx_text_layout_line_count(layout) == 2);
    CHECK(nx_text_layout_line_at(layout, 0, &line) == NX_OK);
    CHECK(line.byte_end == 1 && line.break_byte_end == 3 &&
          line.hard_break && line.width == 12.0);
    CHECK(nx_text_layout_line_at(layout, 1, &line) == NX_OK);
    CHECK(line.byte_start == 3 && line.byte_end == 4);
    nx_text_layout_destroy(layout);
    layout = NULL;
    for (unsigned char separator = 0x1cu; separator <= 0x1eu; ++separator) {
        char paragraph[] = {'A', (char)separator, 'V'};
        CHECK(nx_text_layout_create(collection, paragraph, sizeof(paragraph),
                                    NX_BIDI_AUTO, 20.0, 100.0, &layout) == NX_OK);
        CHECK(nx_text_layout_line_count(layout) == 2);
        CHECK(nx_text_layout_line_at(layout, 0, &line) == NX_OK);
        CHECK(line.byte_start == 0 && line.byte_end == 1 &&
              line.break_byte_end == 2 && line.hard_break);
        CHECK(nx_text_layout_line_at(layout, 1, &line) == NX_OK);
        CHECK(line.byte_start == 2 && line.byte_end == 3);
        nx_text_layout_destroy(layout);
        layout = NULL;
    }
    CHECK(nx_text_layout_create(collection, "A\n\nV", 4,
                                NX_BIDI_AUTO, 20.0, 100.0, &layout) == NX_OK);
    CHECK(nx_text_layout_line_count(layout) == 3);
    CHECK(nx_text_layout_line_at(layout, 1, &line) == NX_OK);
    CHECK(line.byte_start == 2 && line.byte_end == 2 &&
          line.break_byte_end == 3 && line.glyph_count == 0);
    CHECK(caret_exists(layout, 1, 2, 0.0));
    nx_text_layout_destroy(layout);
    layout = NULL;
    CHECK(nx_text_layout_create(collection, "A\xe2\x80\xa8V", 5,
                                NX_BIDI_AUTO, 20.0, 100.0, &layout) == NX_OK);
    CHECK(nx_text_layout_line_count(layout) == 2);
    CHECK(nx_text_layout_line_at(layout, 0, &line) == NX_OK);
    CHECK(line.byte_end == 1 && line.break_byte_end == 4 &&
          line.hard_break);
    nx_text_layout_destroy(layout);
    layout = NULL;
    CHECK(nx_text_layout_create(collection, "A\n", 2,
                                NX_BIDI_AUTO, 20.0, 100.0, &layout) == NX_OK);
    CHECK(nx_text_layout_line_count(layout) == 2);
    CHECK(nx_text_layout_line_at(layout, 1, &line) == NX_OK);
    CHECK(line.byte_start == 2 && line.byte_end == 2 &&
          line.glyph_count == 0);
    CHECK(caret_exists(layout, 1, 2, 0.0));
    nx_text_layout_destroy(layout);
    layout = NULL;
    CHECK(nx_text_layout_create(collection, "", 0,
                                NX_BIDI_AUTO, 20.0, 100.0, &layout) == NX_OK);
    CHECK(nx_text_layout_line_count(layout) == 1 &&
          nx_text_layout_caret_count(layout) == 1);
    nx_text_layout_destroy(layout);
    layout = NULL;
    CHECK(nx_text_layout_create(collection, "A\xcc\x81", 3,
                                NX_BIDI_LTR, 20.0, 100.0, &layout) == NX_OK);
    CHECK(caret_exists(layout, 0, 0, 0.0));
    CHECK(caret_exists(layout, 0, 3, 12.0));
    for (size_t i = 0; i < nx_text_layout_caret_count(layout); ++i) {
        CHECK(nx_text_layout_caret_at(layout, i, &caret) == NX_OK);
        CHECK(caret.byte_offset != 1 && caret.byte_offset != 2);
    }
    CHECK(nx_text_layout_selection(layout, 1, 3, rects, 3,
                                    &rect_count) == NX_ERROR_ARGUMENT);
    nx_text_layout_destroy(layout);
    layout = NULL;
    CHECK(nx_text_layout_create(collection, "A\xd7\x90\xd7\x90V", 6,
                                NX_BIDI_LTR, 20.0, 100.0, &layout) == NX_OK);
    CHECK(nx_text_layout_glyph_at(layout, 1, &glyph) == NX_OK);
    CHECK(glyph.shape.scalar == 0x05d0 && glyph.x == 24.0);
    CHECK(nx_text_layout_glyph_at(layout, 2, &glyph) == NX_OK);
    CHECK(glyph.shape.scalar == 0x05d0 && glyph.x == 12.0);
    CHECK(caret_exists(layout, 0, 1, 12.0));
    CHECK(caret_exists(layout, 0, 1, 36.0));
    CHECK(nx_text_layout_selection(layout, 1, 3, rects, 3,
                                    &rect_count) == NX_OK);
    CHECK(rect_count == 1 && rects[0].x == 24.0 &&
          rects[0].width == 12.0);
    CHECK(nx_text_layout_selection(layout, 0, 3, rects, 3,
                                    &rect_count) == NX_OK);
    CHECK(rect_count == 2 && rects[0].x == 0.0 &&
          rects[0].width == 12.0 && rects[1].x == 24.0 &&
          rects[1].width == 12.0);
    nx_text_layout_destroy(layout);
    layout = (nx_text_layout *)1;
    CHECK(nx_text_layout_create(collection, "\xc0\xaf", 2,
                                NX_BIDI_AUTO, 20.0, 100.0,
                                &layout) == NX_ERROR_ARGUMENT);
    CHECK(layout == NULL);
    char repeated[2000];
    for (size_t i = 0; i < sizeof(repeated); i += 2u) {
        repeated[i] = 'A';
        repeated[i + 1u] = ' ';
    }
    CHECK(nx_text_layout_create(collection, repeated, sizeof(repeated),
                                NX_BIDI_LTR, 20.0, 12.0,
                                &layout) == NX_OK);
    CHECK(nx_text_layout_line_count(layout) == 1000);
    CHECK(nx_text_layout_height(layout) == 20000.0);
    nx_text_layout_destroy(layout);
    uint8_t ligature_bytes[512];
    ligature_fixture(ligature_bytes);
    nx_font *ligature_font = NULL;
    CHECK(nx_font_open(ligature_bytes, sizeof(ligature_bytes), NULL,
                       &ligature_font) == NX_OK);
    nx_font_collection *ligature_collection = NULL;
    CHECK(nx_font_collection_create(NULL, &ligature_collection) == NX_OK);
    CHECK(nx_font_collection_add(ligature_collection, ligature_font) == NX_OK);
    layout = NULL;
    CHECK(nx_text_layout_create(ligature_collection, "VA", 2,
                                NX_BIDI_LTR, 20.0, 100.0,
                                &layout) == NX_OK);
    CHECK(nx_text_layout_glyph_count(layout) == 1);
    CHECK(caret_exists(layout, 0, 0, 0.0));
    CHECK(caret_exists(layout, 0, 1, 6.0));
    CHECK(caret_exists(layout, 0, 2, 12.0));
    CHECK(nx_text_layout_selection(layout, 1, 2, rects, 3,
                                    &rect_count) == NX_OK);
    CHECK(rect_count == 1 && rects[0].x == 6.0 &&
          rects[0].width == 6.0);
    nx_text_layout_destroy(layout);
    nx_font_collection_destroy(ligature_collection);
    nx_font_close(ligature_font);
    uint8_t raster_bytes[512];
    raster_fixture(raster_bytes);
    nx_font *raster_font = NULL;
    CHECK(nx_font_open(raster_bytes, sizeof(raster_bytes), NULL,
                       &raster_font) == NX_OK);
    nx_font_collection *raster_collection = NULL;
    CHECK(nx_font_collection_create(NULL, &raster_collection) == NX_OK);
    CHECK(nx_font_collection_add(raster_collection, raster_font) == NX_OK);
    layout = NULL;
    CHECK(nx_text_layout_create(raster_collection, "A A", 3,
                                NX_BIDI_LTR, 20.0, 100.0,
                                &layout) == NX_OK);
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 64.0f, 40.0f, 0.0) == NX_OK);
    CHECK(nx_draw_text_layout(ctx, layout, (nx_point){2.0f, 1.0f},
                              (nx_color){1, 1, 1, 1}, 1u) == NX_ERROR_LIMIT);
    CHECK(nx_draw_text_layout(ctx, layout, (nx_point){2.0f, 1.0f},
                              (nx_color){1, 1, 1, 1}, 65536u) == NX_OK);
    nx_utf8_span source = {0};
    CHECK(nx_text_layout_text(layout, &source) == NX_OK &&
          source.length == 3 && memcmp(source.data, "A A", 3) == 0);
    CHECK(nx_label(ctx, 1, 0, (nx_rect){2, 1, 36, 20}, layout,
                   (nx_point){2, 1}, (nx_color){1, 1, 1, 1},
                   1u) == NX_ERROR_LIMIT);
    CHECK(nx_label(ctx, 1, 0, (nx_rect){2, 1, 36, 20}, layout,
                   (nx_point){2, 1}, (nx_color){1, 1, 1, 1},
                   65536u) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t semantic_count = 0;
    nx_semantic_view label_view;
    CHECK(nx_semantics_count(ctx, &semantic_count) == NX_OK && semantic_count == 1);
    CHECK(nx_semantics_get(ctx, 0, &label_view) == NX_OK &&
          label_view.role == NX_ROLE_TEXT && label_view.id == 1 &&
          label_view.name.length == 3 &&
          memcmp(label_view.name.data, "A A", 3) == 0);
    const nx_command *commands = NULL;
    size_t command_count = 0;
    CHECK(nx_frame_commands(ctx, &commands, &command_count) == NX_OK);
    CHECK(command_count == 4 && commands[0].type == NX_COMMAND_BITMAP &&
          commands[1].type == NX_COMMAND_BITMAP &&
          commands[2].type == NX_COMMAND_BITMAP &&
          commands[3].type == NX_COMMAND_BITMAP &&
          commands[0].bitmap.pixels != commands[1].bitmap.pixels);
    nx_text_layout_destroy(layout);
    nx_font_collection_destroy(raster_collection);
    nx_font_close(raster_font);
    uint8_t pixels[64u * 40u * 4u];
    CHECK(nx_headless_render(commands, command_count, pixels, 64u, 40u,
                             64u * 4u, (nx_color){0, 0, 0, 0}) == NX_OK);
    unsigned covered = 0;
    for (size_t i = 3u; i < sizeof(pixels); i += 4u)
        covered += pixels[i];
    CHECK(covered > 0u);
    CHECK(nx_begin_frame(ctx, 64.0f, 40.0f, 0.0) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_context_destroy(ctx);
    nx_font_collection_destroy(collection);
    nx_font_close(font);
    puts("text layout tests passed");
    return 0;
}
