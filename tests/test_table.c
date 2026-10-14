#include "nexaui/nexaui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static int compare_rows(size_t left, size_t right, void *user) {
    const int *values = (const int *)user;
    return (values[left] > values[right]) -
           (values[left] < values[right]);
}

static void *fail_allocate(void *user, size_t size) {
    size_t *fail_size = (size_t *)user;
    if (*fail_size == size) {
        *fail_size = 0;
        return NULL;
    }
    return malloc(size);
}

static void fail_deallocate(void *user, void *ptr, size_t size) {
    (void)user;
    (void)size;
    free(ptr);
}

static int test_sort(void) {
    int values[] = {2, 1, 2, 1, 3};
    size_t indices[5] = {99, 99, 99, 99, 99};
    size_t ascending[] = {1, 3, 0, 2, 4};
    size_t descending[] = {4, 0, 2, 1, 3};
    CHECK(nx_table_sort_indices(5, compare_rows, values, 0, NULL,
                                indices, 5) == NX_OK);
    CHECK(memcmp(indices, ascending, sizeof(indices)) == 0);
    CHECK(nx_table_sort_indices(5, compare_rows, values, 1, NULL,
                                indices, 5) == NX_OK);
    CHECK(memcmp(indices, descending, sizeof(indices)) == 0);
    size_t fail_size = sizeof(indices);
    nx_allocator allocator = {&fail_size, fail_allocate, fail_deallocate};
    CHECK(nx_table_sort_indices(5, compare_rows, values, 0, &allocator,
                                indices, 5) == NX_ERROR_MEMORY);
    CHECK(memcmp(indices, descending, sizeof(indices)) == 0);
    CHECK(nx_table_sort_indices(5, compare_rows, values, 0, NULL,
                                indices, 4) == NX_ERROR_ARGUMENT);
    CHECK(nx_table_sort_indices(0, NULL, NULL, 0, NULL, NULL, 0) == NX_OK);
    return 0;
}

static nx_table_desc table_desc(nx_table_column_desc columns[2]) {
    columns[0] = (nx_table_column_desc){0};
    columns[0].id = 11;
    columns[0].resize_id = 12;
    columns[0].name = (nx_utf8_span){"Name", 4};
    columns[0].width = 100;
    columns[0].minimum_width = 60;
    columns[0].maximum_width = 140;
    columns[0].sortable = 1;
    columns[1] = (nx_table_column_desc){0};
    columns[1].id = 13;
    columns[1].name = (nx_utf8_span){"Age", 3};
    columns[1].width = 80;
    columns[1].minimum_width = 40;
    columns[1].maximum_width = 100;
    columns[1].sortable = 1;
    nx_table_desc desc = {0};
    desc.id = 10;
    desc.bounds = (nx_rect){0, 0, 180, 70};
    desc.name = (nx_utf8_span){"People", 6};
    desc.header_height = 24;
    desc.columns = columns;
    desc.column_count = 2;
    return desc;
}

static int queue_pointer(nx_context *ctx, nx_event_type type,
                         float x, float y) {
    nx_event event = {0};
    event.type = type;
    event.button = NX_BUTTON_LEFT;
    event.x = x;
    event.y = y;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int test_headers(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_table_column_desc columns[2];
    nx_table_desc desc = table_desc(columns);
    nx_table_sort sort = {0};
    for (unsigned frame = 0; frame < 8; ++frame) {
        if (frame == 1 || frame == 2) {
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_DOWN, 20, 10));
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_UP, 20, 10));
        }
        if (frame == 3) {
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_DOWN, 96, 10));
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_MOVE, 130, 10));
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_UP, 130, 10));
        }
        if (frame == 4) {
            nx_event event = {0};
            event.type = NX_EVENT_KEY_DOWN;
            event.key = NX_KEY_LEFT;
            CHECK(nx_context_queue_event(ctx, &event) == NX_OK);
        }
        if (frame == 5) {
            for (unsigned click = 0; click < 2; ++click) {
                CHECK(queue_pointer(ctx, NX_EVENT_POINTER_DOWN, 20, 10));
                CHECK(queue_pointer(ctx, NX_EVENT_POINTER_UP, 20, 10));
            }
        }
        if (frame == 6) {
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_DOWN, 120, 10));
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_MOVE, 300, 10));
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_UP, 300, 10));
        }
        if (frame == 7) {
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_DOWN, 150, 10));
            CHECK(queue_pointer(ctx, NX_EVENT_POINTER_UP, 150, 10));
        }
        CHECK(nx_begin_frame(ctx, 180, 70, 0) == NX_OK);
        CHECK(nx_table_headers(ctx, &desc, &sort) == NX_OK);
        nx_container_desc row = {0}, cell = {0};
        row.id = 20;
        row.parent_id = 10;
        row.bounds = (nx_rect){0, 24, 180, 22};
        row.name = (nx_utf8_span){"Row 1", 5};
        cell.id = 21;
        cell.parent_id = 20;
        cell.bounds = (nx_rect){0, 24, columns[0].width, 22};
        cell.name = (nx_utf8_span){"Alice", 5};
        CHECK(nx_table_row(ctx, &row) == NX_OK);
        CHECK(nx_table_cell(ctx, &cell) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        CHECK(sort.column_id == (frame == 7 ? 13u : frame ? 11u : 0u));
        CHECK(sort.descending == (frame >= 2 && frame < 7));
        CHECK(fabs(columns[0].width -
                   (frame < 3 ? 100.0 : frame == 3 ? 134.0 :
                    frame < 6 ? 124.0 : 140.0)) < 0.001);
        uint32_t index;
        nx_semantic_view view;
        CHECK(nx_semantics_find(ctx, 11, &index) == NX_OK);
        CHECK(nx_semantics_get(ctx, index, &view) == NX_OK);
        CHECK(view.role == NX_ROLE_COLUMN_HEADER && view.parent_id == 10);
        CHECK(!!(view.state & NX_SEM_SELECTED) ==
              !!(frame && frame < 7));
        if (frame && frame < 7)
            CHECK(view.value.length == (frame == 1 ? 9u : 10u));
        CHECK(nx_semantics_find(ctx, 12, &index) == NX_OK);
        CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
              view.role == NX_ROLE_SEPARATOR && view.parent_id == 11);
        if (frame == 7) {
            CHECK(nx_semantics_find(ctx, 13, &index) == NX_OK);
            CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
                  (view.state & NX_SEM_SELECTED) &&
                  view.value.length == 9u);
        }
        CHECK(nx_semantics_find(ctx, 21, &index) == NX_OK);
        CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
              view.role == NX_ROLE_CELL && view.parent_id == 20);
        if (frame == 3) {
            const nx_command *commands;
            size_t count;
            uint8_t pixels[180u * 70u * 4u];
            CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK);
            CHECK(nx_headless_render(commands, count, pixels, 180, 70,
                180u * 4u, (nx_color){0, 0, 0, 0}) == NX_OK);
            CHECK(pixels[(10u * 180u + 10u) * 4u + 3u] != 0);
        }
    }
    nx_context_destroy(ctx);
    return 0;
}

static int test_paint_retry(void) {
    size_t fail_size = 4u * sizeof(nx_command);
    nx_context_desc config = {0};
    config.initial_command_capacity = 2;
    config.allocator = (nx_allocator){&fail_size, fail_allocate,
                                       fail_deallocate};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&config, &ctx) == NX_OK);
    nx_table_column_desc columns[2];
    nx_table_desc desc = table_desc(columns);
    nx_table_sort sort = {0};
    CHECK(nx_begin_frame(ctx, 180, 70, 0) == NX_OK);
    CHECK(nx_table_headers(ctx, &desc, &sort) == NX_ERROR_MEMORY);
    CHECK(columns[0].width == 100 && sort.column_id == 0);
    CHECK(nx_table_headers(ctx, &desc, &sort) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 4);
    nx_context_destroy(ctx);
    return 0;
}

int main(void) {
    if (test_sort() || test_headers() || test_paint_retry()) return 1;
    puts("table tests passed");
    return 0;
}
