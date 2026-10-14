#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>

#define MAX_FONT_BYTES (64u * 1024u * 1024u)

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: nexaui_font_inspect FONT_FILE\n");
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    int status = 1;
    uint8_t *bytes = NULL;
    nx_font *font = NULL;
    if (fseek(file, 0, SEEK_END) != 0) goto cleanup;
    long file_length = ftell(file);
    if (file_length <= 0 || (unsigned long)file_length > MAX_FONT_BYTES ||
        fseek(file, 0, SEEK_SET) != 0) goto cleanup;
    bytes = (uint8_t *)malloc((size_t)file_length);
    if (!bytes || fread(bytes, 1, (size_t)file_length, file) != (size_t)file_length)
        goto cleanup;
    if (nx_font_open(bytes, (size_t)file_length, NULL, &font) != NX_OK)
        goto cleanup;
    size_t count = 0;
    if (nx_font_table_count(font, &count) != NX_OK) goto cleanup;
    printf("%zu SFNT tables\n", count);
    nx_font_metrics metrics;
    if (nx_font_get_metrics(font, &metrics) == NX_OK)
        printf("units/em=%u glyphs=%u ascender=%d descender=%d hmetrics=%u\n",
               metrics.units_per_em, metrics.glyph_count,
               metrics.ascender, metrics.descender, metrics.number_of_hmetrics);
    const uint32_t sample_scalars[] = {0x41u, 0x20acu, 0x65e5u, 0x1f600u};
    for (size_t i = 0; i < sizeof(sample_scalars) / sizeof(sample_scalars[0]); ++i) {
        uint16_t glyph = 0;
        if (nx_font_glyph_index(font, sample_scalars[i], &glyph) == NX_OK)
            printf("U+%04X -> glyph %u\n", sample_scalars[i], glyph);
    }
    const uint32_t outline_scalars[] = {'A', 0x00e9u};
    for (size_t sample = 0;
         sample < sizeof(outline_scalars) / sizeof(outline_scalars[0]); ++sample) {
        uint16_t sample_glyph = 0;
        if (nx_font_glyph_index(font, outline_scalars[sample], &sample_glyph) != NX_OK ||
            !sample_glyph) continue;
        nx_glyph_outline_buffer outline = {0};
        nx_result result = nx_font_get_glyph_outline(font, sample_glyph, &outline);
        if (result == NX_OK)
            printf("U+%04X glyph %u: %zu contours, %zu points\n",
                   outline_scalars[sample], sample_glyph,
                   outline.contour_count, outline.point_count);
        else if (result == NX_ERROR_UNSUPPORTED)
            printf("U+%04X glyph %u: non-glyf outline unsupported\n",
                   outline_scalars[sample], sample_glyph);
        if (result == NX_OK && outline.point_count) {
            nx_glyph_point *points = (nx_glyph_point *)malloc(
                outline.point_count * sizeof(nx_glyph_point));
            uint16_t *ends = (uint16_t *)malloc(
                outline.contour_count * sizeof(uint16_t));
            if (!points || !ends) {
                free(points);
                free(ends);
                goto cleanup;
            }
            outline.points = points;
            outline.point_capacity = outline.point_count;
            outline.contour_ends = ends;
            outline.contour_capacity = outline.contour_count;
            result = nx_font_get_glyph_outline(font, sample_glyph, &outline);
            if (result == NX_OK) {
                int64_t sum_x = 0, sum_y = 0;
                size_t on_curve_count = 0;
                for (size_t i = 0; i < outline.point_count; ++i) {
                    sum_x += points[i].x;
                    sum_y += points[i].y;
                    on_curve_count += points[i].on_curve != 0;
                }
                printf("  first point=(%d,%d), on_curve=%u; sums=(%lld,%lld), on-curve=%zu\n",
                       points[0].x, points[0].y, points[0].on_curve,
                       (long long)sum_x, (long long)sum_y, on_curve_count);
            }
            free(points);
            free(ends);
            if (result != NX_OK) goto cleanup;
        }
    }
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t tag;
        nx_bytes_view view;
        if (nx_font_table_at(font, i, &tag, &view) != NX_OK) goto cleanup;
        char label[5];
        for (unsigned j = 0; j < 4; ++j) {
            unsigned char ch = (unsigned char)(tag >> (24u - j * 8u));
            label[j] = ch >= 32u && ch <= 126u ? (char)ch : '?';
        }
        label[4] = 0;
        printf("%s %zu bytes\n", label, view.length);
    }
    status = 0;
cleanup:
    if (status) fprintf(stderr, "invalid or unreadable SFNT file\n");
    nx_font_close(font);
    free(bytes);
    fclose(file);
    return status;
}
