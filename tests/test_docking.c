#include "nexaui/nexaui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
    return 1; \
} } while (0)

static int queue_pointer(nx_context *ctx, nx_event_type type, float x, float y) {
    nx_event e = {0};
    e.type = type;
    e.button = NX_BUTTON_LEFT;
    e.x = x;
    e.y = y;
    return nx_context_queue_event(ctx, &e) == NX_OK;
}

static int queue_key(nx_context *ctx, nx_key key) {
    nx_event e = {0};
    e.type = NX_EVENT_KEY_DOWN;
    e.key = key;
    return nx_context_queue_event(ctx, &e) == NX_OK;
}

static int test_splitter(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_splitter_desc d = {0};
    d.id = 21;
    d.name = (nx_utf8_span){"Resize panes", 12};
    d.axis = NX_SPLIT_X;
    d.split.divider = (nx_rect){50, 0, 6, 30};
    d.split.first_extent = 50;
    d.split.second_extent = 44;
    d.split.effective_ratio = 0.5f;
    float ratio = 0.5f;
    nx_interaction_state state;
    CHECK(nx_begin_frame(ctx, 100, 40, 0) == NX_OK);
    CHECK(nx_splitter(ctx, &d, &ratio, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);

    CHECK(queue_pointer(ctx, NX_EVENT_POINTER_DOWN, 53, 10));
    CHECK(queue_pointer(ctx, NX_EVENT_POINTER_MOVE, 72, 10));
    CHECK(nx_begin_frame(ctx, 100, 40, 0) == NX_OK);
    CHECK(nx_splitter(ctx, &d, &ratio, &state) == NX_OK);
    CHECK(state.held && state.drag_delta_x == 19);
    CHECK(fabsf(ratio - (0.5f + 19.0f / 94.0f)) < 0.0001f);
    CHECK(nx_end_frame(ctx) == NX_OK);
    uint32_t index = 0;
    nx_semantic_view view;
    CHECK(nx_semantics_find(ctx, 21, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK);
    CHECK(view.role == NX_ROLE_SEPARATOR &&
          (view.actions & NX_ACTION_INCREMENT) && view.value.length);

    d.split.effective_ratio = ratio;
    d.split.divider.x = 69;
    CHECK(queue_pointer(ctx, NX_EVENT_POINTER_MOVE, 220, 10));
    CHECK(queue_pointer(ctx, NX_EVENT_POINTER_UP, 220, 10));
    CHECK(nx_begin_frame(ctx, 100, 40, 0) == NX_OK);
    CHECK(nx_splitter(ctx, &d, &ratio, &state) == NX_OK);
    CHECK(state.pointer_released && ratio == 1.0f);
    CHECK(nx_end_frame(ctx) == NX_OK);

    CHECK(nx_focus_set(ctx, 21) == NX_OK);
    CHECK(queue_key(ctx, NX_KEY_HOME));
    CHECK(nx_begin_frame(ctx, 100, 40, 0) == NX_OK);
    CHECK(nx_splitter(ctx, &d, &ratio, &state) == NX_OK);
    CHECK(ratio == 0.0f && state.focused);
    CHECK(nx_end_frame(ctx) == NX_OK);

    d.split.effective_ratio = ratio;
    CHECK(queue_key(ctx, NX_KEY_RIGHT));
    CHECK(nx_begin_frame(ctx, 100, 40, 0) == NX_OK);
    CHECK(nx_splitter(ctx, &d, &ratio, &state) == NX_OK);
    CHECK(fabsf(ratio - 0.05f) < 0.0001f);
    CHECK(nx_end_frame(ctx) == NX_OK);

    d.disabled = 1;
    CHECK(queue_key(ctx, NX_KEY_END));
    CHECK(nx_begin_frame(ctx, 100, 40, 0) == NX_OK);
    CHECK(nx_splitter(ctx, &d, &ratio, &state) == NX_OK);
    CHECK(fabsf(ratio - 0.05f) < 0.0001f && !state.focused);
    CHECK(nx_end_frame(ctx) == NX_OK);

    d.split.first_extent = NAN;
    CHECK(nx_begin_frame(ctx, 100, 40, 0) == NX_OK);
    CHECK(nx_splitter(ctx, &d, &ratio, &state) == NX_ERROR_ARGUMENT);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_context_destroy(ctx);
    return 0;
}

static int test_layout_split_drag(void) {
    nx_layout *layout = NULL;
    nx_context *ctx = NULL;
    uint32_t root = 0, child = 0;
    nx_size desired;
    nx_split_info info;
    CHECK(nx_layout_create(NULL, 3, &layout) == NX_OK);
    nx_layout_desc split = nx_layout_default_desc(NX_LAYOUT_SPLIT);
    nx_layout_desc leaf = nx_layout_default_desc(NX_LAYOUT_LEAF);
    CHECK(nx_layout_add(layout, NX_LAYOUT_NO_PARENT, &split, &root) == NX_OK);
    CHECK(nx_layout_add(layout, root, &leaf, &child) == NX_OK);
    CHECK(nx_layout_add(layout, root, &leaf, &child) == NX_OK);
    CHECK(nx_layout_measure(layout, root, 100, 30, &desired) == NX_OK);
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 100, 30}) == NX_OK);
    CHECK(nx_layout_get_split_info(layout, root, &info) == NX_OK);
    float initial_x = info.divider.x;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_splitter_desc handle = {0};
    handle.id = 50;
    handle.name = (nx_utf8_span){"Split", 5};
    handle.axis = NX_SPLIT_X;
    handle.split = info;
    float ratio = 0.5f;
    nx_interaction_state state;
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_splitter(ctx, &handle, &ratio, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(queue_pointer(ctx, NX_EVENT_POINTER_DOWN, initial_x + 2, 15));
    CHECK(queue_pointer(ctx, NX_EVENT_POINTER_MOVE, initial_x + 12, 15));
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_splitter(ctx, &handle, &ratio, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(ratio > 0.5f && nx_layout_set_split(layout, root, ratio) == NX_OK);
    CHECK(nx_layout_arrange(layout, root, (nx_rect){0, 0, 100, 30}) == NX_OK);
    CHECK(nx_layout_get_split_info(layout, root, &info) == NX_OK);
    CHECK(info.divider.x > initial_x + 9 && info.divider.x < initial_x + 11);
    nx_context_destroy(ctx);
    nx_layout_destroy(layout);
    return 0;
}

static int test_dock_model(void) {
    nx_dock_node nodes[8], restored_nodes[8];
    nx_dock_tab tabs[8], restored_tabs[8];
    nx_dock_model m, restored = {0};
    CHECK(nx_dock_init(&m, nodes, 8, tabs, 8, 10, 100) == NX_OK);
    CHECK(nx_dock_split(&m, 10, 11, 12, 200, NX_SPLIT_X, 0.4f, 1) == NX_OK);
    CHECK(nx_dock_split(&m, 12, 13, 14, 300, NX_SPLIT_Y, 0.6f, 0) == NX_OK);
    CHECK(nx_dock_validate(&m) == NX_OK && m.root_id == 11);
    CHECK(nx_dock_move(&m, 100, 14) == NX_ERROR_STATE);
    CHECK(nx_dock_move(&m, 200, 10) == NX_ERROR_STATE);
    CHECK(nx_dock_add_tab(&m, 12, 201) == NX_OK);
    CHECK(nx_dock_select(&m, 200) == NX_OK);
    CHECK(nx_dock_move(&m, 200, 10) == NX_OK);
    CHECK(m.tabs[m.tab_count - 1].panel_id == 200 &&
          m.tabs[m.tab_count - 1].stack_id == 10);
    CHECK(nx_dock_validate(&m) == NX_OK);
    CHECK(nx_dock_split(&m, 10, 15, 16, 400, NX_SPLIT_X, 0.5f, 1) == NX_OK);
    CHECK(nx_dock_move(&m, 400, 10) == NX_ERROR_STATE);
    CHECK(nx_dock_select(&m, 300) == NX_OK);
    CHECK(nx_dock_set_ratio(&m, 11, 0.25f) == NX_OK);
    CHECK(nx_dock_set_ratio(&m, 11, NAN) == NX_ERROR_ARGUMENT);
    size_t required = 0;
    CHECK(nx_dock_encode(&m, NULL, 0, &required) == NX_OK);
    CHECK(required == 24 + m.node_count * 56 + m.tab_count * 16);
    unsigned char bytes[512] = {0};
    CHECK(nx_dock_encode(&m, bytes, required - 1, &required) == NX_ERROR_LIMIT);
    CHECK(nx_dock_encode(&m, bytes, sizeof(bytes), &required) == NX_OK);
    restored.nodes = restored_nodes;
    restored.node_capacity = 8;
    restored.tabs = restored_tabs;
    restored.tab_capacity = 8;
    CHECK(nx_dock_decode(&restored, bytes, required) == NX_OK);
    CHECK(nx_dock_validate(&restored) == NX_OK);
    CHECK(restored.node_count == m.node_count &&
          restored.tab_count == m.tab_count && restored.root_id == m.root_id);
    unsigned char roundtrip[512] = {0};
    size_t roundtrip_length = 0;
    CHECK(nx_dock_encode(&restored, roundtrip, sizeof(roundtrip),
                         &roundtrip_length) == NX_OK);
    CHECK(roundtrip_length == required &&
          memcmp(bytes, roundtrip, required) == 0);
    bytes[0] = 'X';
    CHECK(nx_dock_decode(&restored, bytes, required) == NX_ERROR_ARGUMENT);
    CHECK(restored.node_count == 0 && restored.tab_count == 0 &&
          restored.root_id == 0);
    bytes[0] = 'N';
    bytes[24 + 48] = 0xff; /* Invalid root stack ratio bits. */
    CHECK(nx_dock_decode(&restored, bytes, required) == NX_ERROR_ARGUMENT);
    CHECK(restored.node_count == 0 && restored.root_id == 0);
    CHECK(nx_dock_decode(&restored, roundtrip, required - 1) == NX_ERROR_ARGUMENT);
    CHECK(restored.node_count == 0 && restored.root_id == 0);
    return 0;
}

int main(void) {
    if (test_splitter()) return 1;
    if (test_layout_split_drag()) return 1;
    if (test_dock_model()) return 1;
    return 0;
}
