#include "nexaui/nexaui.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* Build with NEXAUI_BUILD_FUZZERS=ON under Clang/libFuzzer. */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    nx_font *font = NULL;
    if (nx_font_open(data, size, NULL, &font) != NX_OK) return 0;
    size_t count = 0;
    if (nx_font_table_count(font, &count) == NX_OK) {
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t tag;
            nx_bytes_view view;
            if (nx_font_table_at(font, i, &tag, &view) != NX_OK) break;
            nx_bytes_view found;
            (void)nx_font_find_table(font, tag, &found);
        }
    }
    nx_font_metrics metrics;
    if (nx_font_get_metrics(font, &metrics) == NX_OK) {
        uint16_t advance;
        int16_t lsb;
        (void)nx_font_glyph_hmetrics(font, 0, &advance, &lsb);
        (void)nx_font_glyph_hmetrics(font, (uint16_t)(metrics.glyph_count - 1u),
                                     &advance, &lsb);
    }
    for (size_t i = 0; i + 4u <= size && i < 64u; i += 4u) {
        uint32_t scalar = ((uint32_t)data[i] << 24u) |
                          ((uint32_t)data[i + 1u] << 16u) |
                          ((uint32_t)data[i + 2u] << 8u) | data[i + 3u];
        uint16_t glyph;
        (void)nx_font_glyph_index(font, scalar, &glyph);
    }
    if (nx_font_get_metrics(font, &metrics) == NX_OK) {
        nx_glyph_outline_buffer outline = {0};
        (void)nx_font_get_glyph_outline(font, 0, &outline);
        (void)nx_font_get_glyph_outline(font,
                                        (uint16_t)(metrics.glyph_count - 1u),
                                        &outline);
        if (size >= 2u) {
            uint16_t glyph_index = (uint16_t)((((uint16_t)data[0] << 8u) |
                                                data[1]) % metrics.glyph_count);
            nx_glyph_cache *cache = NULL;
            if (nx_glyph_cache_create(font, 4096u, NULL, &cache) == NX_OK) {
                nx_glyph_mask_view mask;
                (void)nx_glyph_cache_get(cache, glyph_index, 16u, &mask);
                (void)nx_glyph_cache_get(cache, glyph_index, 16u, &mask);
                nx_glyph_cache_destroy(cache);
            }
            if (nx_font_get_glyph_outline(font, glyph_index, &outline) == NX_OK &&
                outline.point_count && outline.point_count <= 4096u &&
                outline.contour_count <= 1024u) {
                nx_glyph_point *points = (nx_glyph_point *)malloc(
                    outline.point_count * sizeof(nx_glyph_point));
                uint16_t *ends = (uint16_t *)malloc(
                    outline.contour_count * sizeof(uint16_t));
                if (points && ends) {
                    outline.points = points;
                    outline.point_capacity = outline.point_count;
                    outline.contour_ends = ends;
                    outline.contour_capacity = outline.contour_count;
                    if (nx_font_get_glyph_outline(font, glyph_index,
                                                  &outline) == NX_OK) {
                        uint8_t mask[32u * 32u];
                        (void)nx_rasterize_glyph_a8(&outline, 0.02, 16.0, 24.0,
                                                     mask, 32, 32, 32,
                                                     sizeof(mask), NULL);
                    }
                }
                free(points);
                free(ends);
            }
        }
    }
    nx_font_close(font);
    return 0;
}
