#include "nexaui/nexaui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define WIDTH 480u
#define HEIGHT 320u
#define ROW_COUNT 10000u
#define SCROLL_ID UINT64_C(70000)

static const nx_rect viewport = {24, 48, 432, 240};

static nx_result measure_row(void *user, size_t index,
                              uint64_t *out_key, double *out_height) {
    (void)user;
    *out_key = UINT64_C(100000) + (uint64_t)index;
    *out_height = index % 7u == 0u ? 36.0 : 28.0;
    return NX_OK;
}

typedef struct row_builder {
    nx_context *ui;
} row_builder;

static nx_result draw_row(void *user, const nx_virtual_row *row) {
    row_builder *builder = (row_builder *)user;
    char name[48];
    int length = snprintf(name, sizeof(name), "Row %zu", row->index);
    if (length < 0 || (size_t)length >= sizeof(name)) return NX_ERROR_LIMIT;
    nx_control_desc button = {0};
    button.id = row->key;
    button.parent_id = SCROLL_ID;
    button.bounds = (nx_rect){viewport.x,
                              viewport.y + (float)row->viewport_y,
                              viewport.width, (float)row->height};
    button.name = (nx_utf8_span){name, (size_t)length};
    nx_interaction_state state;
    nx_result result = nx_button(builder->ui, &button, &state);
    if (result != NX_OK) return result;
    nx_color accent = row->index % 2u ?
        (nx_color){0.17f, 0.58f, 0.83f, 1} :
        (nx_color){0.31f, 0.72f, 0.61f, 1};
    nx_rect bar = {button.bounds.x + 14.0f, button.bounds.y + 8.0f,
                   32.0f + (float)(row->index % 11u) * 26.0f,
                   button.bounds.height - 16.0f};
    return nx_fill_round_rect(builder->ui, bar, 3.0f, accent);
}

static nx_result build_frame(nx_context *ui, const nx_virtual_list *list,
                              float *scroll_y, size_t *out_rows) {
    *out_rows = 0;
    nx_result result = nx_begin_frame(ui, WIDTH, HEIGHT, 1.0 / 60.0);
    if (result != NX_OK) return result;
    result = nx_fill_rect(ui, (nx_rect){24, 16, 432, 18},
                          (nx_color){0.23f, 0.39f, 0.54f, 1});
    if (result != NX_OK) return result;
    nx_virtual_range extent;
    result = nx_virtual_list_range(list, 0, viewport.height, 0, &extent);
    if (result != NX_OK) return result;
    nx_scroll_area_desc scroll = {0};
    scroll.id = SCROLL_ID;
    scroll.viewport = viewport;
    scroll.name = (nx_utf8_span){"Measured rows", 13};
    scroll.content_size = (nx_size){viewport.width, (float)extent.total_extent};
    scroll.axes = NX_SCROLL_Y;
    scroll.thumb_y_id = SCROLL_ID + 1u;
    float scroll_x = 0;
    nx_scroll_info info;
    result = nx_scroll_area_begin(ui, &scroll, &scroll_x, scroll_y, &info);
    if (result != NX_OK) return result;
    row_builder builder = {ui};
    nx_virtual_range visible;
    result = nx_virtual_list_visit(list, *scroll_y, viewport.height,
                                    2, 32, draw_row, &builder,
                                    &visible, out_rows);
    if (result != NX_OK) return result;
    result = nx_fill_rect(ui,
        (nx_rect){viewport.x + viewport.width - 8.0f, viewport.y,
                  8.0f, viewport.height},
        (nx_color){0.13f, 0.20f, 0.28f, 1});
    if (result != NX_OK) return result;
    result = nx_scroll_area_end(ui);
    if (result != NX_OK) return result;
    result = nx_fill_rect(ui, (nx_rect){24, 301, 432, 3},
                          (nx_color){0.23f, 0.39f, 0.54f, 1});
    if (result != NX_OK) return result;
    return nx_end_frame(ui);
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "scroll.ppm";
    nx_virtual_list *list = NULL;
    nx_context *ui = NULL;
    uint8_t *pixels = NULL;
    FILE *file = NULL;
    int status = 1;
    double anchor = 0, start = 0;
    float scroll_y = 0;
    size_t visible = 0;
    if (nx_virtual_list_create(ROW_COUNT, 28.0, NULL, &list) != NX_OK ||
        nx_virtual_list_rebuild(list, ROW_COUNT, measure_row, NULL,
                                 viewport.height, &anchor) != NX_OK ||
        nx_virtual_list_offset_at(list, 2500u, &start) != NX_OK ||
        nx_context_create(NULL, &ui) != NX_OK) goto cleanup;
    scroll_y = (float)start;
    if (build_frame(ui, list, &scroll_y, &visible) != NX_OK) goto cleanup;
    nx_event wheel = {0};
    wheel.type = NX_EVENT_WHEEL;
    wheel.x = 100;
    wheel.y = 120;
    wheel.delta_y = -2;
    if (nx_context_queue_event(ui, &wheel) != NX_OK ||
        build_frame(ui, list, &scroll_y, &visible) != NX_OK) goto cleanup;
    const nx_command *commands;
    size_t command_count;
    if (nx_frame_commands(ui, &commands, &command_count) != NX_OK) goto cleanup;
    pixels = (uint8_t *)malloc((size_t)WIDTH * HEIGHT * 4u);
    if (!pixels || nx_headless_render(commands, command_count, pixels,
                                      WIDTH, HEIGHT, WIDTH * 4u,
                                      (nx_color){0.07f, 0.10f, 0.14f, 1}) != NX_OK)
        goto cleanup;
    file = fopen(path, "wb");
    if (!file || fprintf(file, "P6\n%u %u\n255\n", WIDTH, HEIGHT) < 0)
        goto cleanup;
    for (size_t i = 0; i < (size_t)WIDTH * HEIGHT; ++i)
        if (fwrite(pixels + 4u * i, 1, 3, file) != 3) goto cleanup;
    printf("Rendered %zu of %u rows at offset %.0f to %s\n",
           visible, ROW_COUNT, scroll_y, path);
    status = 0;
cleanup:
    if (file && fclose(file) != 0) status = 1;
    free(pixels);
    nx_context_destroy(ui);
    nx_virtual_list_destroy(list);
    if (status) fprintf(stderr, "Failed to render %s\n", path);
    return status;
}
