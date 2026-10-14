#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_LIMIT (64u * 1024u * 1024u)
#define IMAGE_WIDTH 640u
#define IMAGE_HEIGHT 160u

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "usage: nexaui_text_headless FONT_FILE OUTPUT.ppm [TEXT]\n");
        return 2;
    }
    const char *message = argc == 4 ? argv[3] : "NexaUI Text\nAV AV";
    int status = 1;
    FILE *input = fopen(argv[1], "rb"), *output = NULL;
    uint8_t *font_bytes = NULL, *pixels = NULL;
    nx_font *font = NULL;
    nx_font_collection *collection = NULL;
    nx_text_layout *layout = NULL;
    nx_context *context = NULL;
    if (!input || fseek(input, 0, SEEK_END) != 0) goto cleanup;
    long length = ftell(input);
    if (length <= 0 || (unsigned long)length > FONT_LIMIT ||
        fseek(input, 0, SEEK_SET) != 0) goto cleanup;
    font_bytes = malloc((size_t)length);
    pixels = malloc((size_t)IMAGE_WIDTH * IMAGE_HEIGHT * 4u);
    if (!font_bytes || !pixels ||
        fread(font_bytes, 1, (size_t)length, input) != (size_t)length ||
        nx_font_open(font_bytes, (size_t)length, NULL, &font) != NX_OK ||
        nx_font_collection_create(NULL, &collection) != NX_OK ||
        nx_font_collection_add(collection, font) != NX_OK ||
        nx_text_layout_create(collection, message, strlen(message),
                              NX_BIDI_AUTO, 32.0, 600.0, &layout) != NX_OK ||
        nx_context_create(NULL, &context) != NX_OK ||
        nx_begin_frame(context, IMAGE_WIDTH, IMAGE_HEIGHT, 0.0) != NX_OK ||
        nx_draw_text_layout(context, layout, (nx_point){20.0f, 20.0f},
                            (nx_color){0.94f, 0.95f, 1.0f, 1.0f},
                            8u * 1024u * 1024u) != NX_OK ||
        nx_end_frame(context) != NX_OK) goto cleanup;
    const nx_command *commands = NULL;
    size_t command_count = 0;
    if (nx_frame_commands(context, &commands, &command_count) != NX_OK ||
        nx_headless_render(commands, command_count, pixels,
                           IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_WIDTH * 4u,
                           (nx_color){0.09f, 0.12f, 0.18f, 1.0f}) != NX_OK)
        goto cleanup;
    output = fopen(argv[2], "wb");
    if (!output || fprintf(output, "P6\n%u %u\n255\n",
                           IMAGE_WIDTH, IMAGE_HEIGHT) < 0)
        goto cleanup;
    for (size_t i = 0; i < (size_t)IMAGE_WIDTH * IMAGE_HEIGHT; ++i)
        if (fwrite(pixels + i * 4u, 1, 3u, output) != 3u) goto cleanup;
    status = 0;
    printf("wrote %s (%zu lines, %zu glyphs)\n", argv[2],
           nx_text_layout_line_count(layout),
           nx_text_layout_glyph_count(layout));
cleanup:
    if (output && fclose(output) != 0) status = 1;
    if (status) fprintf(stderr, "could not render text or write the image\n");
    nx_context_destroy(context);
    nx_text_layout_destroy(layout);
    nx_font_collection_destroy(collection);
    nx_font_close(font);
    free(pixels);
    free(font_bytes);
    if (input) fclose(input);
    return status;
}
