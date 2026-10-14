#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>

#define MAX_FONT_BYTES (64u * 1024u * 1024u)
#define MAX_FONTS 8u

typedef struct font_file {
    uint8_t *bytes;
    nx_font *font;
} font_file;

static int open_font(const char *path, font_file *out) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    int okay = 0;
    if (fseek(file, 0, SEEK_END) != 0) goto cleanup;
    long length = ftell(file);
    if (length <= 0 || (unsigned long)length > MAX_FONT_BYTES ||
        fseek(file, 0, SEEK_SET) != 0) goto cleanup;
    out->bytes = (uint8_t *)malloc((size_t)length);
    if (!out->bytes || fread(out->bytes, 1, (size_t)length, file) !=
                       (size_t)length) goto cleanup;
    if (nx_font_open(out->bytes, (size_t)length, NULL, &out->font) != NX_OK)
        goto cleanup;
    okay = 1;
cleanup:
    fclose(file);
    return okay;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > (int)MAX_FONTS + 1) {
        fprintf(stderr, "usage: nexaui_font_fallback PRIMARY_FONT [FALLBACK_FONT ...]\n");
        return 2;
    }
    int status = 1;
    font_file files[MAX_FONTS] = {{0}};
    nx_font_collection *collection = NULL;
    if (nx_font_collection_create(NULL, &collection) != NX_OK) goto cleanup;
    for (int i = 1; i < argc; ++i) {
        font_file *file = &files[i - 1];
        if (!open_font(argv[i], file) ||
            nx_font_collection_add(collection, file->font) != NX_OK) {
            fprintf(stderr, "cannot add font: %s\n", argv[i]);
            goto cleanup;
        }
    }
    const uint32_t scalars[] = {'A', 0x00e9u, 0x65e5u, 0x1f600u};
    for (size_t i = 0; i < sizeof(scalars) / sizeof(scalars[0]); ++i) {
        nx_font_match match;
        if (nx_font_collection_resolve(collection, scalars[i], &match) != NX_OK)
            goto cleanup;
        if (match.font)
            printf("U+%04X -> font %u (%s), glyph %u\n", scalars[i],
                   match.font_index, argv[match.font_index + 1u],
                   match.glyph_index);
        else
            printf("U+%04X -> missing\n", scalars[i]);
    }
    status = 0;
cleanup:
    nx_font_collection_destroy(collection);
    for (int i = 0; i < argc - 1; ++i) {
        nx_font_close(files[i].font);
        free(files[i].bytes);
    }
    return status;
}
