#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>

#define MAX_FONT_BYTES (64u * 1024u * 1024u)

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "usage: nexaui_glyph_raster FONT_FILE OUTPUT.pgm [HEX_SCALAR]\n");
        return 2;
    }
    uint32_t scalar = 'A';
    if (argc == 4) {
        char *end = NULL;
        unsigned long parsed = strtoul(argv[3], &end, 16);
        if (!argv[3][0] || !end || *end || parsed > 0x10fffful ||
            (parsed >= 0xd800ul && parsed <= 0xdffful)) {
            fprintf(stderr, "invalid Unicode scalar: %s\n", argv[3]);
            return 2;
        }
        scalar = (uint32_t)parsed;
    }
    int status = 1;
    FILE *file = fopen(argv[1], "rb");
    FILE *output = NULL;
    uint8_t *bytes = NULL;
    nx_font *font = NULL;
    nx_glyph_cache *cache = NULL;
    if (!file) goto cleanup;
    if (fseek(file, 0, SEEK_END) != 0) goto cleanup;
    long file_length = ftell(file);
    if (file_length <= 0 || (unsigned long)file_length > MAX_FONT_BYTES ||
        fseek(file, 0, SEEK_SET) != 0) goto cleanup;
    bytes = (uint8_t *)malloc((size_t)file_length);
    if (!bytes || fread(bytes, 1, (size_t)file_length, file) != (size_t)file_length)
        goto cleanup;
    if (nx_font_open(bytes, (size_t)file_length, NULL, &font) != NX_OK)
        goto cleanup;
    uint16_t glyph = 0;
    if (nx_font_glyph_index(font, scalar, &glyph) != NX_OK || !glyph ||
        nx_glyph_cache_create(font, 8u * 1024u * 1024u, NULL, &cache) != NX_OK)
        goto cleanup;
    nx_glyph_mask_view mask = {0};
    if (nx_glyph_cache_get(cache, glyph, 64u, &mask) != NX_OK ||
        !mask.width || !mask.height)
        goto cleanup;
    size_t mask_size = (size_t)mask.width * mask.height;
    output = fopen(argv[2], "wb");
    if (!output || fprintf(output, "P5\n%u %u\n255\n",
                           mask.width, mask.height) < 0 ||
        fwrite(mask.pixels, 1, mask_size, output) != mask_size)
        goto cleanup;
    status = 0;
    printf("wrote %ux%u A8 glyph mask for U+%04X (glyph %u, advance %.2f px)\n",
           mask.width, mask.height, scalar, glyph, mask.advance_x);
cleanup:
    if (output && fclose(output) != 0) status = 1;
    if (status) fprintf(stderr, "could not rasterize the font or write the mask\n");
    nx_glyph_cache_destroy(cache);
    nx_font_close(font);
    free(bytes);
    if (file) fclose(file);
    return status;
}
