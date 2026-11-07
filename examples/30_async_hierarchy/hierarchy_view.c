#include "hierarchy_view.h"
#include "../common/text_wrap.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static nx_utf8_span span(const char *s) { return (nx_utf8_span){s, strlen(s)}; }
static void dispose(void *user, void *data, size_t bytes) {
    (void)user; (void)bytes; free(data);
}
static void clear_layouts(ah_view *v) {
    for (size_t i = 0; i < AH_CHILD_LIMIT + 4u; ++i) {
        nx_text_layout_destroy(v->layouts[i]); v->layouts[i] = NULL;
    }
    v->layout_revision = 0;
}
static void invalidate(ah_view *v) {
    v->ticket = 0;
    if (v->generation == UINT64_MAX) v->exhausted = 1;
    else ++v->generation;
    if (v->revision != UINT64_MAX) ++v->revision;
    else v->exhausted = 1;
}
nx_result ah_init(ah_view *v, nx_sim_channel *channel, nx_font_collection *fonts, uint64_t epoch) {
    if (!v || !channel || !epoch) return NX_ERROR_ARGUMENT;
    *v = (ah_view){0}; v->channel = channel; v->fonts = fonts; v->epoch = epoch;
    v->phase = AH_UNQUERIED; v->text_size = 16; v->revision = 1;
    return NX_OK;
}
void ah_destroy(ah_view *v) {
    if (!v) return;
    clear_layouts(v); *v = (ah_view){0};
}
void ah_collapse(ah_view *v) {
    if (!v) return;
    v->expanded = 0;
    if (v->phase == AH_LOADING) {
        invalidate(v); v->phase = AH_UNQUERIED; v->count = 0;
    }
}
nx_result ah_request(ah_view *v) {
    if (!v || !v->channel || !v->expanded) return NX_ERROR_STATE;
    invalidate(v); v->count = 0; v->restore_focus = 1; v->scroll_y = 0;
    nx_result r = v->exhausted ? NX_ERROR_LIMIT : NX_OK;
    ah_key *key = r == NX_OK ? malloc(sizeof(*key)) : NULL;
    if (r == NX_OK && !key) r = NX_ERROR_MEMORY;
    if (key) {
        *key = (ah_key){v->epoch, v->generation, AH_BRANCH};
        r = nx_sim_channel_post_command(v->channel, key, sizeof(*key), dispose, NULL, &v->ticket);
        if (r != NX_OK) free(key);
    }
    v->error = r; v->phase = r == NX_OK ? AH_LOADING : AH_FAILED;
    return r;
}
static int valid_children(const ah_response *r) {
    if (r->count > AH_CHILD_LIMIT) return 0;
    for (size_t i = 0; i < r->count; ++i) {
        const ah_child *c = &r->children[i];
        const char *end = memchr(c->name, 0, AH_NAME_BYTES);
        if (c->id <= AH_THUMB || !end || end == c->name ||
            nx_utf8_validate(c->name, (size_t)(end - c->name), NULL) != NX_OK) return 0;
        for (size_t j = 0; j < i; ++j) if (r->children[j].id == c->id) return 0;
    }
    return 1;
}
int ah_observe(ah_view *v, const nx_sim_message *m) {
    if (!v || !m || !m->ticket || m->ticket != v->ticket ||
        v->phase != AH_LOADING || !v->expanded) return 0;
    const ah_response *r = m->data;
    int bad_key = m->bytes == sizeof(*r) && r &&
        (r->key.view != v->epoch || r->key.generation != v->generation || r->key.branch != AH_BRANCH);
    v->ticket = 0; v->error = m->status;
    if (bad_key) v->error = NX_ERROR_ARGUMENT;
    if (m->status == NX_OK && (m->bytes != sizeof(*r) || !r || !valid_children(r))) v->error = NX_ERROR_ARGUMENT;
    if (v->error != NX_OK) { v->phase = AH_FAILED; v->count = 0; }
    else {
        v->count = r->count;
        memcpy(v->children, r->children, r->count * sizeof(*r->children));
        v->phase = r->count ? AH_READY : AH_EMPTY;
        int found = v->selected == 0 || v->selected == AH_BRANCH;
        for (size_t i = 0; i < r->count; ++i) if (v->children[i].id == v->selected) found = 1;
        if (!found) v->selected = AH_BRANCH;
    }
    if (v->revision == UINT64_MAX) { v->exhausted = 1; v->phase = AH_FAILED; v->error = NX_ERROR_LIMIT; }
    else ++v->revision;
    return 1;
}
const char *ah_status(const ah_view *v) {
    if (v->exhausted) return v->german ? "Ansicht erneut öffnen" : "Reopen this view to continue";
    static const char *const words[2][5] = {
        {"Children not requested", "Loading children", "Could not load children", "Children ready", "No children"},
        {"Unterelemente noch nicht angefordert", "Unterelemente werden geladen", "Unterelemente konnten nicht geladen werden", "Unterelemente bereit", "Keine Unterelemente"}
    };
    return words[!!v->german][v->phase];
}
static nx_result layouts(ah_view *v, double width) {
    if (!v->fonts) return NX_OK;
    if (v->layout_revision == v->revision && v->layout_width == width &&
        v->layout_text_size == v->text_size && v->layout_german == v->german &&
        v->layout_exhausted == v->exhausted && v->layout_phase == v->phase) return NX_OK;
    nx_text_layout *next[AH_CHILD_LIMIT + 4u] = {0};
    const char *labels[4] = {v->german ? "Zweig" : "Branch", ah_status(v),
        v->german ? "Erneut versuchen" : "Retry", v->german ? "Aktualisieren" : "Refresh"};
    nx_result result = NX_OK;
    for (size_t i = 0; i < 4u + v->count && result == NX_OK; ++i) {
        const char *s = i < 4 ? labels[i] : v->children[i - 4].name;
        if (!nx_example_small_text_layout(v->fonts, s, strlen(s), v->text_size, width, &next[i]))
            result = NX_ERROR_STATE;
    }
    if (result != NX_OK) {
        for (size_t i = 0; i < AH_CHILD_LIMIT + 4u; ++i) nx_text_layout_destroy(next[i]);
        return result;
    }
    clear_layouts(v);
    memcpy(v->layouts, next, sizeof(next)); v->layout_revision = v->revision;
    v->layout_width = width; v->layout_german = v->german; v->layout_phase = v->phase;
    v->layout_text_size = v->text_size;
    v->layout_exhausted = v->exhausted;
    return NX_OK;
}
static nx_result row(ah_view *v, nx_context *ctx, uint64_t id, uint64_t parent,
    nx_rect box, const char *name, nx_text_layout *label, int *expanded) {
    nx_tree_item_desc d = {0};
    d.control.id = id; d.control.parent_id = parent; d.control.bounds = box; d.control.name = span(name);
    d.control.label_layout = label; d.control.label_origin = (nx_point){box.x + 28, box.y + 4};
    d.control.glyph_cache_bytes = 1024u * 1024u;
    if (expanded) {
        d.has_children = 1; d.expander_id = AH_EXPANDER;
        d.expander_bounds = (nx_rect){box.x, box.y, 24, box.height};
    }
    nx_interaction_state state;
    return nx_tree_item(ctx, &d, expanded, &v->selected, &state);
}
nx_result ah_build(ah_view *v, nx_context *ctx, uint64_t parent, nx_rect box,
    int *intent, int *redraw) {
    if (!v || !ctx || !intent || !redraw || !isfinite(v->text_size) ||
        v->text_size < 12 || v->text_size > 32 || !isfinite(box.width) ||
        !isfinite(box.height) || box.width < 280 || box.height < 160) return NX_ERROR_ARGUMENT;
    *intent = AH_INTENT_NONE; *redraw = 0;
    int reflow = v->layout_width != box.width - 76 || v->layout_text_size != v->text_size;
    nx_result result = layouts(v, box.width - 76);
    if (result != NX_OK) return result;
    float row_height = v->text_size >= 16 ? v->text_size + 16 : 32;
    float heights[AH_CHILD_LIMIT], children_height = 0;
    for (size_t i = 0; i < v->count; ++i) {
        float label_height = v->layouts[4 + i] ? (float)nx_text_layout_height(v->layouts[4 + i]) + 8 : 0;
        heights[i] = label_height > row_height ? label_height : row_height;
        children_height += heights[i];
    }
    uint64_t focused = nx_focus_id(ctx);
    int reveal = focused != v->revealed_focus || reflow || v->reveal_height != box.height;
    v->revealed_focus = focused; v->reveal_height = box.height;
    int child_still_present = 0, child_was_present = 0;
    for (size_t i = 0; i < v->count; ++i) if (focused == v->children[i].id) child_still_present = 1;
    for (size_t i = 0; i < v->previous_count; ++i) if (focused == v->previous_ids[i]) child_was_present = 1;
    if ((child_was_present && !child_still_present) ||
        (v->restore_focus && (focused == AH_RETRY || focused == AH_REFRESH)))
        if (nx_focus_set(ctx, AH_BRANCH) != NX_OK) return NX_ERROR_STATE;
    v->restore_focus = 0;
    nx_container_desc tree = {0}; tree.id = AH_TREE; tree.parent_id = parent;
    tree.bounds = box; tree.name = span(v->german ? "Hierarchie" : "Hierarchy");
    result = nx_tree(ctx, &tree);
    if (result != NX_OK) return result;
    int was_open = v->expanded;
    nx_rect branch = {box.x + 8, box.y + 8, box.width - 16, row_height};
    result = row(v, ctx, AH_BRANCH, AH_TREE, branch, v->german ? "Zweig" : "Branch", v->layouts[0], &v->expanded);
    if (result != NX_OK) return result;
    if (was_open && !v->expanded) {
        ah_collapse(v);
        if (child_was_present || focused == AH_RETRY || focused == AH_REFRESH) {
            if (nx_focus_set(ctx, AH_BRANCH) != NX_OK) return NX_ERROR_STATE;
            *redraw = 1;
        }
    }
    v->previous_count = 0;
    if (!v->expanded) return NX_OK;
    if (v->phase == AH_UNQUERIED) *intent = AH_INTENT_LOAD;
    float viewport_y = branch.y + row_height + 8;
    float status_height = v->layouts[1] ? (float)nx_text_layout_height(v->layouts[1]) + 8 : row_height;
    int has_button = v->phase != AH_LOADING && v->phase != AH_UNQUERIED;
    float button_height = has_button ? row_height + 8 : 0;
    nx_scroll_area_desc scroll = {0}; scroll.id = AH_SCROLL; scroll.parent_id = AH_BRANCH;
    scroll.thumb_y_id = AH_THUMB; scroll.axes = NX_SCROLL_Y; scroll.name = span(v->german ? "Unterelemente" : "Children");
    scroll.viewport = (nx_rect){box.x + 24, viewport_y, box.width - 40, box.y + box.height - 8 - viewport_y};
    scroll.content_size = (nx_size){scroll.viewport.width, status_height + 8 + button_height + children_height};
    scroll.wheel_step = row_height;
    float target_y = -1, target_height = row_height, child_y = status_height + 8 + button_height;
    if (focused == AH_RETRY || focused == AH_REFRESH) target_y = status_height + 8;
    for (size_t i = 0; i < v->count; ++i) {
        if (focused == v->children[i].id) { target_y = child_y; target_height = heights[i]; }
        child_y += heights[i];
    }
    if (target_y >= 0 && (target_height <= scroll.viewport.height || reveal)) {
        if (target_height > scroll.viewport.height || target_y < v->scroll_y) v->scroll_y = target_y;
        else if (target_height <= scroll.viewport.height && target_y + target_height > v->scroll_y + scroll.viewport.height)
            v->scroll_y = target_y + target_height - scroll.viewport.height;
    }
    /* The scrollbar has its own keyboard focus. As in the Settings example,
       also let a focused over-height label be read without leaving that row. */
    if (target_y >= 0 && target_height > scroll.viewport.height && !reveal) {
        const nx_event *events = NULL; size_t count = 0;
        result = nx_frame_events(ctx, &events, &count);
        if (result != NX_OK) return result;
        for (size_t i = 0; i < count; ++i) if (events[i].type == NX_EVENT_KEY_DOWN && !events[i].modifiers) {
            if (events[i].key == NX_KEY_PAGE_DOWN) v->scroll_y += scroll.viewport.height * 0.5f;
            else if (events[i].key == NX_KEY_PAGE_UP) {
                v->scroll_y -= scroll.viewport.height * 0.5f;
                if (v->scroll_y < 0) v->scroll_y = 0;
            }
        }
    }
    float offset_x = 0; nx_scroll_info scroll_info;
    result = nx_scroll_area_begin(ctx, &scroll, &offset_x, &v->scroll_y, &scroll_info);
    if (result != NX_OK) return result;
    nx_rect content = scroll_info.viewport;
    float y = content.y - v->scroll_y;
    nx_semantic_desc status = {0}; status.id = AH_STATUS; status.parent_id = AH_SCROLL;
    status.role = NX_ROLE_STATUS; status.name = span(ah_status(v));
    status.bounds = (nx_rect){content.x, y, content.width, status_height};
    status.state = v->phase == AH_LOADING ? NX_SEM_BUSY : v->phase == AH_FAILED ? NX_SEM_INVALID : 0;
    result = nx_semantics_add(ctx, &status, NULL);
    if (result != NX_OK) return result;
    if (v->layouts[1]) {
        nx_color color;
        if (nx_theme_color(ctx, v->phase == AH_FAILED ? NX_COLOR_ERROR : NX_COLOR_TEXT, &color) != NX_OK) return NX_ERROR_STATE;
        result = nx_draw_text_layout(ctx, v->layouts[1], (nx_point){status.bounds.x, y}, color, 1024u * 1024u);
        if (result != NX_OK) return result;
    }
    y += status_height + 8;
    if (has_button) {
        nx_control_desc button = {0};
        button.id = v->phase == AH_FAILED ? AH_RETRY : AH_REFRESH; button.parent_id = AH_SCROLL;
        button.name = span(v->phase == AH_FAILED ? (v->german ? "Erneut versuchen" : "Retry") : (v->german ? "Aktualisieren" : "Refresh"));
        button.bounds = (nx_rect){content.x, y, content.width, row_height};
        button.label_layout = v->layouts[v->phase == AH_FAILED ? 2 : 3];
        button.disabled = v->exhausted;
        button.label_origin = (nx_point){button.bounds.x + 8, y + 4}; button.glyph_cache_bytes = 1024u * 1024u;
        nx_interaction_state clicked;
        result = nx_button(ctx, &button, &clicked);
        if (result != NX_OK) return result;
        if (clicked.clicked) *intent = v->phase == AH_FAILED ? AH_INTENT_RETRY : AH_INTENT_REFRESH;
        y += row_height + 8;
    }
    for (size_t i = 0; i < v->count; ++i) {
        result = row(v, ctx, v->children[i].id, AH_SCROLL,
            (nx_rect){content.x, y, content.width, heights[i]}, v->children[i].name, v->layouts[4 + i], NULL);
        if (result != NX_OK) return result;
        v->previous_ids[v->previous_count++] = v->children[i].id;
        y += heights[i];
    }
    return nx_scroll_area_end(ctx);
}
