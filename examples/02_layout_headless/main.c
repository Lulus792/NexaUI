#include "nexaui/nexaui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define WIDTH 320u
#define HEIGHT 180u

static nx_layout_desc box(float width, float height, float grow) {
    nx_layout_desc d = nx_layout_default_desc(NX_LAYOUT_LEAF);
    d.width.preferred = width;
    d.width.has_preferred = 1;
    d.height.preferred = height;
    d.height.has_preferred = 1;
    d.width.grow = grow;
    return d;
}

static int paint(nx_context *ui, nx_layout *layout, uint32_t node, nx_color color) {
    nx_rect rect;
    return nx_layout_get_rect(layout, node, &rect) == NX_OK &&
           nx_fill_rect(ui, rect, color) == NX_OK;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "layout.ppm";
    nx_layout *layout = NULL;
    nx_context *ui = NULL;
    uint8_t *pixels = NULL;
    FILE *file = NULL;
    int status = 1;
    uint32_t root, header, content, side, main_area, footer;
    nx_layout_desc column = nx_layout_default_desc(NX_LAYOUT_COLUMN);
    column.padding = (nx_insets){12, 12, 12, 12};
    column.gap = 8;
    nx_layout_desc row = nx_layout_default_desc(NX_LAYOUT_ROW);
    row.gap = 8;
    row.height.preferred = 100;
    row.height.has_preferred = 1;
    nx_layout_desc header_desc = box(100, 24, 0);
    header_desc.align_x = NX_ALIGN_STRETCH;
    nx_layout_desc side_desc = box(56, 100, 0);
    nx_layout_desc main_desc = box(120, 100, 1);
    nx_layout_desc footer_desc = box(100, 16, 0);
    footer_desc.align_x = NX_ALIGN_STRETCH;
    if (nx_layout_create(NULL, 0, &layout) != NX_OK) goto cleanup;
    if (nx_context_create(NULL, &ui) != NX_OK) goto cleanup;
    if (nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &column, &root) != NX_OK ||
        nx_layout_add(layout, root, &header_desc, &header) != NX_OK ||
        nx_layout_add(layout, root, &row, &content) != NX_OK ||
        nx_layout_add(layout, content, &side_desc, &side) != NX_OK ||
        nx_layout_add(layout, content, &main_desc, &main_area) != NX_OK ||
        nx_layout_add(layout, root, &footer_desc, &footer) != NX_OK) goto cleanup;
    nx_size desired;
    if (nx_layout_measure(layout, root, WIDTH, HEIGHT, &desired) != NX_OK ||
        nx_layout_arrange(layout, root, (nx_rect){0, 0, WIDTH, HEIGHT}) != NX_OK ||
        nx_begin_frame(ui, WIDTH, HEIGHT, 1.0 / 60.0) != NX_OK) goto cleanup;
    if (!paint(ui, layout, header, (nx_color){0.21f, 0.36f, 0.50f, 1}) ||
        !paint(ui, layout, side, (nx_color){0.16f, 0.21f, 0.29f, 1}) ||
        !paint(ui, layout, main_area, (nx_color){0.24f, 0.46f, 0.66f, 1}) ||
        !paint(ui, layout, footer, (nx_color){0.17f, 0.29f, 0.41f, 1}) ||
        nx_end_frame(ui) != NX_OK) goto cleanup;
    const nx_command *commands;
    size_t count;
    if (nx_frame_commands(ui, &commands, &count) != NX_OK) goto cleanup;
    pixels = (uint8_t *)malloc((size_t)WIDTH * HEIGHT * 4u);
    if (!pixels || nx_headless_render(commands, count, pixels, WIDTH, HEIGHT,
                                      WIDTH * 4u, (nx_color){0.07f, 0.09f, 0.13f, 1}) != NX_OK)
        goto cleanup;
    file = fopen(path, "wb");
    if (!file || fprintf(file, "P6\n%u %u\n255\n", WIDTH, HEIGHT) < 0) goto cleanup;
    for (size_t i = 0; i < (size_t)WIDTH * HEIGHT; ++i)
        if (fwrite(pixels + i * 4u, 1, 3, file) != 3) goto cleanup;
    status = 0;
cleanup:
    if (file && fclose(file) != 0) status = 1;
    free(pixels);
    nx_context_destroy(ui);
    nx_layout_destroy(layout);
    if (status) fprintf(stderr, "Failed to write %s\n", path);
    return status;
}
