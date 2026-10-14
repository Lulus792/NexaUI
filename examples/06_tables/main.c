#include "nexaui/nexaui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 560u
#define HEIGHT 390u
#define ROW_COUNT 6u
#define COLUMN_COUNT 3u
#define GLYPH_CACHE (4u * 1024u * 1024u)
#define FONT_LIMIT (64u * 1024u * 1024u)

typedef struct record {
    const char *name;
    int score;
    const char *score_text;
    const char *status;
    nx_text_layout *labels[COLUMN_COUNT];
} record;

typedef struct table_app {
    nx_context *ui;
    nx_font_collection *fonts;
    nx_text_layout *title;
    nx_text_layout *headers[COLUMN_COUNT];
    nx_table_column_desc columns[COLUMN_COUNT];
    nx_table_sort sort;
    record rows[ROW_COUNT];
    size_t order[ROW_COUNT];
} table_app;

static nx_utf8_span span(const char *text) {
    return (nx_utf8_span){text, strlen(text)};
}

static int compare_score(size_t left, size_t right, void *user) {
    const table_app *app = (const table_app *)user;
    int a = app->rows[left].score, b = app->rows[right].score;
    return (a > b) - (a < b);
}

static nx_result draw_table(table_app *app) {
    nx_table_desc table = {0};
    table.id = 10;
    table.bounds = (nx_rect){24, 62, 510, 272};
    table.name = span("Build results");
    table.header_height = 32;
    table.columns = app->columns;
    table.column_count = COLUMN_COUNT;
    nx_result result = nx_table_headers(app->ui, &table, &app->sort);
    if (result != NX_OK) return result;
    if (app->sort.column_id == app->columns[1].id) {
        result = nx_table_sort_indices(ROW_COUNT, compare_score, app,
            app->sort.descending, NULL, app->order, ROW_COUNT);
        if (result != NX_OK) return result;
    } else {
        for (size_t i = 0; i < ROW_COUNT; ++i) app->order[i] = i;
    }

    if (!app->fonts) {
        float x = table.bounds.x;
        for (size_t column = 0; column < COLUMN_COUNT; ++column) {
            result = nx_fill_round_rect(app->ui,
                (nx_rect){x + 14, table.bounds.y + 13,
                          42 + 12 * (float)column, 6}, 3,
                (nx_color){0.71f, 0.81f, 0.91f, 1});
            if (result != NX_OK) return result;
            x += app->columns[column].width;
        }
    }

    for (size_t position = 0; position < ROW_COUNT; ++position) {
        size_t index = app->order[position];
        record *item = &app->rows[index];
        float y = 94 + 40 * (float)position;
        nx_container_desc row = {0};
        row.id = 100 + index;
        row.parent_id = table.id;
        row.bounds = (nx_rect){24, y, 510, 38};
        row.name = span(item->name);
        result = nx_table_row(app->ui, &row);
        if (result != NX_OK) return result;
        const char *values[COLUMN_COUNT] = {
            item->name, item->score_text, item->status
        };
        float x = row.bounds.x;
        for (size_t column = 0; column < COLUMN_COUNT; ++column) {
            nx_container_desc cell = {0};
            cell.id = 1000 + index * COLUMN_COUNT + column;
            cell.parent_id = row.id;
            cell.bounds = (nx_rect){x, y, app->columns[column].width, 38};
            cell.name = span(values[column]);
            cell.label_layout = item->labels[column];
            cell.label_origin = (nx_point){x + 14, y + 9};
            cell.glyph_cache_bytes = cell.label_layout ? GLYPH_CACHE : 0;
            result = nx_table_cell(app->ui, &cell);
            if (result != NX_OK) return result;
            if (column == 1) {
                result = nx_fill_round_rect(app->ui,
                    (nx_rect){x + 14, y + 29,
                              (float)item->score * 0.85f, 4}, 2,
                    (nx_color){0.26f, 0.74f, 0.65f, 1});
            } else if (!app->fonts) {
                nx_color color = column == 0 ?
                    (nx_color){0.60f, 0.75f, 0.90f, 1} :
                    (nx_color){0.88f, 0.64f, 0.36f, 1};
                result = nx_fill_round_rect(app->ui,
                    (nx_rect){x + 14, y + 15,
                              column == 0 ? 38 + 9 * (float)index : 45,
                              7}, 3, color);
            }
            if (result != NX_OK) return result;
            x += app->columns[column].width;
        }
    }
    return NX_OK;
}

static nx_result draw_frame(table_app *app) {
    nx_result result = nx_begin_frame(app->ui, WIDTH, HEIGHT, 1.0 / 60.0);
    if (result != NX_OK) { fprintf(stderr, "begin frame: %d\n", result); return result; }
    result = nx_fill_rect(app->ui, (nx_rect){24, 22, 510, 19},
                          (nx_color){0.23f, 0.39f, 0.54f, 1});
    if (result != NX_OK) { fprintf(stderr, "title: %d\n", result); return result; }
    if (app->title) {
        result = nx_draw_text_layout(app->ui, app->title,
            (nx_point){26, 21}, (nx_color){0.92f, 0.97f, 1.0f, 1},
            GLYPH_CACHE);
        if (result != NX_OK) return result;
    }
    result = draw_table(app);
    if (result != NX_OK) { fprintf(stderr, "table: %d\n", result); return result; }
    result = nx_end_frame(app->ui);
    if (result != NX_OK) fprintf(stderr, "end frame: %d\n", result);
    return result;
}

static int click_score(table_app *app) {
    nx_event event = {0};
    event.type = NX_EVENT_POINTER_DOWN;
    event.button = NX_BUTTON_LEFT;
    event.x = 280;
    event.y = 76;
    if (nx_context_queue_event(app->ui, &event) != NX_OK) return 0;
    event.type = NX_EVENT_POINTER_UP;
    return nx_context_queue_event(app->ui, &event) == NX_OK;
}

static int save_frame(nx_context *ui, const char *path) {
    const nx_command *commands = NULL;
    size_t count = 0;
    if (nx_frame_commands(ui, &commands, &count) != NX_OK) return 0;
    uint8_t *pixels = malloc((size_t)WIDTH * HEIGHT * 4u);
    if (!pixels) return 0;
    int okay = nx_headless_render(commands, count, pixels, WIDTH, HEIGHT,
        WIDTH * 4u, (nx_color){0.07f, 0.10f, 0.14f, 1}) == NX_OK;
    FILE *file = okay ? fopen(path, "wb") : NULL;
    if (!file) okay = 0;
    if (okay && fprintf(file, "P6\n%u %u\n255\n", WIDTH, HEIGHT) < 0)
        okay = 0;
    for (size_t i = 0; okay && i < (size_t)WIDTH * HEIGHT; ++i)
        if (fwrite(pixels + 4u * i, 1, 3, file) != 3) okay = 0;
    if (file && fclose(file) != 0) okay = 0;
    free(pixels);
    return okay;
}

static int load_font(table_app *app, const char *path,
                     nx_font **out_font, uint8_t **out_bytes) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    int okay = 0;
    if (fseek(file, 0, SEEK_END) != 0) goto done;
    long length = ftell(file);
    if (length <= 0 || (unsigned long)length > FONT_LIMIT ||
        fseek(file, 0, SEEK_SET) != 0) goto done;
    *out_bytes = malloc((size_t)length);
    if (!*out_bytes || fread(*out_bytes, 1, (size_t)length, file) !=
                       (size_t)length ||
        nx_font_open(*out_bytes, (size_t)length, NULL, out_font) != NX_OK ||
        nx_font_collection_create(NULL, &app->fonts) != NX_OK ||
        nx_font_collection_add(app->fonts, *out_font) != NX_OK) goto done;
    if (nx_text_layout_create(app->fonts, "Build results", 13,
            NX_BIDI_AUTO, 17, 250, &app->title) != NX_OK) goto done;
    static const char *names[COLUMN_COUNT] = {"Name", "Score", "Status"};
    for (size_t column = 0; column < COLUMN_COUNT; ++column)
        if (nx_text_layout_create(app->fonts, names[column],
                strlen(names[column]), NX_BIDI_AUTO, 16, 160,
                &app->headers[column]) != NX_OK) goto done;
    for (size_t row = 0; row < ROW_COUNT; ++row) {
        const char *values[COLUMN_COUNT] = {
            app->rows[row].name, app->rows[row].score_text,
            app->rows[row].status
        };
        for (size_t column = 0; column < COLUMN_COUNT; ++column)
            if (nx_text_layout_create(app->fonts, values[column],
                    strlen(values[column]), NX_BIDI_AUTO, 15, 170,
                    &app->rows[row].labels[column]) != NX_OK) goto done;
    }
    okay = 1;
done:
    fclose(file);
    return okay;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: nexaui_tables_example OUTPUT.ppm [FONT_FILE]\n");
        return 2;
    }
    table_app app = {0};
    nx_font *font = NULL;
    uint8_t *font_bytes = NULL;
    int status = 1;
    const record data[ROW_COUNT] = {
        {"Ada", 75, "75", "Ready", {0}},
        {"Bela", 92, "92", "Ready", {0}},
        {"Cora", 75, "75", "Busy", {0}},
        {"Deni", 61, "61", "Busy", {0}},
        {"Eli", 88, "88", "Ready", {0}},
        {"Faye", 92, "92", "Busy", {0}}
    };
    memcpy(app.rows, data, sizeof(data));
    if (nx_context_create(NULL, &app.ui) != NX_OK) goto cleanup;
    nx_theme theme;
    if (nx_theme_preset(NX_THEME_DARK, &theme) != NX_OK ||
        nx_context_set_theme(app.ui, &theme) != NX_OK) goto cleanup;
    if (argc == 3 && !load_font(&app, argv[2], &font, &font_bytes))
        goto cleanup;
    static const char *headers[COLUMN_COUNT] = {"Name", "Score", "Status"};
    const float widths[COLUMN_COUNT] = {220, 130, 160};
    for (size_t column = 0; column < COLUMN_COUNT; ++column) {
        nx_table_column_desc *entry = &app.columns[column];
        entry->id = 20 + column;
        entry->resize_id = 0;
        entry->name = span(headers[column]);
        entry->width = widths[column];
        entry->minimum_width = widths[column];
        entry->maximum_width = widths[column];
        entry->sortable = column == 1;
        entry->label_layout = app.headers[column];
        entry->label_offset = (nx_point){14, 7};
        entry->glyph_cache_bytes = entry->label_layout ? GLYPH_CACHE : 0;
    }
    if (draw_frame(&app) != NX_OK || !click_score(&app) ||
        draw_frame(&app) != NX_OK ||
        app.sort.column_id != app.columns[1].id ||
        app.sort.descending || app.order[0] != 3 ||
        app.order[1] != 0 || app.order[2] != 2 ||
        app.order[3] != 4 || app.order[4] != 1 ||
        app.order[5] != 5 || !click_score(&app) ||
        draw_frame(&app) != NX_OK ||
        app.sort.column_id != app.columns[1].id ||
        !app.sort.descending || app.order[0] != 1 ||
        app.order[1] != 5 || app.order[2] != 4 ||
        app.order[3] != 0 || app.order[4] != 2 ||
        app.order[5] != 3) goto cleanup;
    uint32_t semantic_index;
    nx_semantic_view view;
    if (nx_semantics_find(app.ui, 101, &semantic_index) != NX_OK ||
        nx_semantics_get(app.ui, semantic_index, &view) != NX_OK ||
        view.role != NX_ROLE_ROW || view.bounds.y != 94 ||
        !save_frame(app.ui, argv[1])) goto cleanup;
    printf("Score descending: ");
    for (size_t i = 0; i < ROW_COUNT; ++i)
        printf("%s%s(%d)", i ? ", " : "", app.rows[app.order[i]].name,
               app.rows[app.order[i]].score);
    printf(" -> %s\n", argv[1]);
    status = 0;
cleanup:
    if (status) fprintf(stderr, "tables example failed\n");
    nx_text_layout_destroy(app.title);
    for (size_t column = 0; column < COLUMN_COUNT; ++column)
        nx_text_layout_destroy(app.headers[column]);
    for (size_t row = 0; row < ROW_COUNT; ++row)
        for (size_t column = 0; column < COLUMN_COUNT; ++column)
            nx_text_layout_destroy(app.rows[row].labels[column]);
    nx_context_destroy(app.ui);
    nx_font_collection_destroy(app.fonts);
    nx_font_close(font);
    free(font_bytes);
    return status;
}
