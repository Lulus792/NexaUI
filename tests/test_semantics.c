#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static nx_semantic_desc node(uint64_t id, uint64_t parent, nx_semantic_role role,
                             const char *name) {
    nx_semantic_desc d = {0};
    d.id = id;
    d.parent_id = parent;
    d.role = role;
    d.bounds = (nx_rect){0, 0, 100, 40};
    if (name) d.name = (nx_utf8_span){name, strlen(name)};
    return d;
}

static int test_tree_and_focus(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 200, 100, 0) == NX_OK);
    nx_semantic_desc root = node(1, 0, NX_ROLE_WINDOW, "Demo");
    nx_semantic_desc button = node(2, 1, NX_ROLE_BUTTON, "\xc3\x96" "ffnen");
    button.description = (nx_utf8_span){"Opens a file", 12};
    button.value = (nx_utf8_span){"Ready", 5};
    button.actions = NX_ACTION_PRESS | NX_ACTION_FOCUS;
    uint32_t root_index = UINT32_MAX, button_index = UINT32_MAX;
    CHECK(nx_semantics_add(ctx, &root, &root_index) == NX_OK && root_index == 0);
    CHECK(nx_semantics_add(ctx, &button, &button_index) == NX_OK && button_index == 1);
    nx_interaction_state interaction;
    CHECK(nx_interact_region(ctx, 2, button.bounds, NX_REGION_FOCUSABLE,
                              &interaction) == NX_OK);
    CHECK(nx_focus_set(ctx, 2) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count = 0;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 2);
    nx_semantic_view view;
    CHECK(nx_semantics_get(ctx, button_index, &view) == NX_OK);
    CHECK(view.id == 2 && view.parent_id == 1 && view.depth == 1);
    CHECK(view.role == NX_ROLE_BUTTON && (view.state & NX_SEM_FOCUSED));
    CHECK(view.actions == (NX_ACTION_PRESS | NX_ACTION_FOCUS));
    CHECK(view.name.length == button.name.length &&
          memcmp(view.name.data, button.name.data, button.name.length) == 0);
    CHECK(view.name.data[view.name.length] == 0);
    CHECK(view.description.length == 12 && view.value.length == 5);
    uint32_t found = UINT32_MAX;
    CHECK(nx_semantics_find(ctx, 2, &found) == NX_OK && found == button_index);
    CHECK(nx_begin_frame(ctx, 200, 100, 0) == NX_OK);
    CHECK(nx_semantics_count(ctx, &count) == NX_ERROR_STATE);
    CHECK(nx_semantics_add(ctx, &root, NULL) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 1);
    CHECK(nx_semantics_find(ctx, 2, &found) == NX_ERROR_ARGUMENT &&
          found == UINT32_MAX);
    nx_context_destroy(ctx);
    return 0;
}

static int test_validation_and_depth(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    nx_semantic_desc root = node(1, 0, NX_ROLE_PANEL, NULL);
    CHECK(nx_semantics_add(ctx, &root, NULL) == NX_OK);
    CHECK(nx_semantics_add(ctx, &root, NULL) == NX_ERROR_STATE);
    nx_semantic_desc bad = node(2, 1, NX_ROLE_BUTTON, NULL);
    CHECK(nx_semantics_add(ctx, &bad, NULL) == NX_ERROR_ARGUMENT);
    bad = node(2, 99, NX_ROLE_TEXT, "orphan");
    CHECK(nx_semantics_add(ctx, &bad, NULL) == NX_ERROR_ARGUMENT);
    const char invalid_utf8[] = "\xc0\x80";
    bad = node(2, 1, NX_ROLE_TEXT, NULL);
    bad.name = (nx_utf8_span){invalid_utf8, 2};
    CHECK(nx_semantics_add(ctx, &bad, NULL) == NX_ERROR_ARGUMENT);
    const char nul_name[] = {'A', 0, 'B'};
    bad.name = (nx_utf8_span){nul_name, 3};
    CHECK(nx_semantics_add(ctx, &bad, NULL) == NX_ERROR_ARGUMENT);
    bad = node(2, 1, NX_ROLE_TEXT, "valid");
    bad.state = NX_SEM_FOCUSED;
    CHECK(nx_semantics_add(ctx, &bad, NULL) == NX_ERROR_ARGUMENT);
    bad.state = NX_SEM_CHECKED | NX_SEM_MIXED;
    CHECK(nx_semantics_add(ctx, &bad, NULL) == NX_ERROR_ARGUMENT);
    uint64_t parent = 1;
    for (uint64_t depth = 1; depth < NX_SEMANTIC_MAX_DEPTH; ++depth) {
        nx_semantic_desc child = node(depth + 1, parent, NX_ROLE_PANEL, NULL);
        CHECK(nx_semantics_add(ctx, &child, NULL) == NX_OK);
        parent = child.id;
    }
    nx_semantic_desc too_deep = node(1000, parent, NX_ROLE_PANEL, NULL);
    CHECK(nx_semantics_add(ctx, &too_deep, NULL) == NX_ERROR_LIMIT);
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == NX_SEMANTIC_MAX_DEPTH);
    nx_context_destroy(ctx);
    return 0;
}

typedef struct tracking { unsigned calls, frees, fail_after; } tracking;
static void *tracked_allocate(void *user, size_t size) {
    tracking *t = (tracking *)user;
    ++t->calls;
    if (t->fail_after && t->calls > t->fail_after) return NULL;
    return malloc(size);
}
static void tracked_deallocate(void *user, void *pointer, size_t size) {
    tracking *t = (tracking *)user;
    (void)size;
    ++t->frees;
    free(pointer);
}

static int test_allocation_and_reuse(void) {
    for (unsigned limit = 2; limit <= 4; ++limit) {
        tracking t = {0, 0, limit};
        nx_context_desc desc = {0};
        desc.allocator = (nx_allocator){&t, tracked_allocate, tracked_deallocate};
        nx_context *ctx = NULL;
        CHECK(nx_context_create(&desc, &ctx) == NX_OK);
        CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
        nx_semantic_desc d = node(1, 0, NX_ROLE_WINDOW, "Test");
        CHECK(nx_semantics_add(ctx, &d, NULL) == NX_ERROR_MEMORY);
        CHECK(nx_end_frame(ctx) == NX_OK);
        size_t count;
        CHECK(nx_semantics_count(ctx, &count) == NX_OK && !count);
        nx_context_destroy(ctx);
        CHECK(t.frees == limit);
    }
    tracking t = {0};
    nx_context_desc desc = {0};
    desc.allocator = (nx_allocator){&t, tracked_allocate, tracked_deallocate};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&desc, &ctx) == NX_OK);
    nx_semantic_desc d = node(1, 0, NX_ROLE_WINDOW, "Stable");
    for (unsigned frame = 0; frame < 100; ++frame) {
        CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
        CHECK(nx_semantics_add(ctx, &d, NULL) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        if (frame > 0) CHECK(t.calls == 5);
    }
    nx_context_destroy(ctx);
    CHECK(t.frees == 5);
    return 0;
}

static int test_many_nodes_and_string_ownership(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 100, 0) == NX_OK);
    char title[] = "Original";
    nx_semantic_desc root = node(1, 0, NX_ROLE_WINDOW, title);
    CHECK(nx_semantics_add(ctx, &root, NULL) == NX_OK);
    title[0] = 'X';
    for (uint64_t id = 2; id <= 1001; ++id) {
        nx_semantic_desc child = node(id, 1, NX_ROLE_CELL, NULL);
        CHECK(nx_semantics_add(ctx, &child, NULL) == NX_OK);
    }
    CHECK(nx_end_frame(ctx) == NX_OK);
    size_t count;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 1001);
    nx_semantic_view view;
    CHECK(nx_semantics_get(ctx, 0, &view) == NX_OK);
    CHECK(view.name.length == 8 && memcmp(view.name.data, "Original", 8) == 0);
    uint32_t index;
    CHECK(nx_semantics_find(ctx, 1001, &index) == NX_OK && index == 1000);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK);
    CHECK(view.parent_id == 1 && view.depth == 1 && view.role == NX_ROLE_CELL);
    nx_context_destroy(ctx);
    return 0;
}

static int action_frame(nx_context *ctx, unsigned expected_clicks) {
    CHECK(nx_begin_frame(ctx, 400, 400, 0) == NX_OK);
    CHECK(nx_semantics_request_action(ctx, 1, NX_ACTION_PRESS) == NX_ERROR_STATE);
    for (uint64_t id=1; id<=70; ++id) {
        nx_widget_desc d = {0};
        d.semantic = node(id, 0, NX_ROLE_BUTTON, "Action");
        d.semantic.actions = NX_ACTION_PRESS | NX_ACTION_FOCUS | NX_ACTION_SELECT;
        if (id==69) d.semantic.state = NX_SEM_DISABLED;
        if (id==70) d.semantic.actions = NX_ACTION_FOCUS;
        d.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
        nx_interaction_state state;
        CHECK(nx_widget_register(ctx, &d, &state) == NX_OK);
        CHECK(state.clicked == (id<=64 ? expected_clicks : 0));
        if (expected_clicks && id==2) CHECK(state.focused);
    }
    nx_semantic_desc label = node(71, 0, NX_ROLE_TEXT, "Read only");
    CHECK(nx_semantics_add(ctx, &label, NULL) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    return 0;
}

static int test_action_dispatch_and_backpressure(void) {
    nx_context *ctx=NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    CHECK(nx_semantics_request_action(NULL, 1, NX_ACTION_PRESS) == NX_ERROR_ARGUMENT);
    CHECK(nx_semantics_request_action(ctx, 1, NX_ACTION_PRESS) == NX_ERROR_STATE);
    CHECK(action_frame(ctx, 0)==0);
    CHECK(nx_semantics_request_action(ctx, 0, NX_ACTION_PRESS) == NX_ERROR_ARGUMENT);
    CHECK(nx_semantics_request_action(ctx, 1, NX_ACTION_PRESS | NX_ACTION_FOCUS) == NX_ERROR_UNSUPPORTED);
    CHECK(nx_semantics_request_action(ctx, 1, NX_ACTION_INCREMENT) == NX_ERROR_UNSUPPORTED);
    CHECK(nx_semantics_request_action(ctx, 99, NX_ACTION_PRESS) == NX_ERROR_STATE);
    CHECK(nx_semantics_request_action(ctx, 69, NX_ACTION_PRESS) == NX_ERROR_STATE);
    CHECK(nx_semantics_request_action(ctx, 70, NX_ACTION_PRESS) == NX_ERROR_UNSUPPORTED);
    CHECK(nx_semantics_request_action(ctx, 71, NX_ACTION_PRESS) == NX_ERROR_STATE);
    CHECK(nx_semantics_request_action(ctx, 2, NX_ACTION_FOCUS) == NX_OK);
    CHECK(nx_semantics_request_action(ctx, 2, NX_ACTION_FOCUS) == NX_ERROR_LIMIT);
    CHECK(nx_semantics_request_action(ctx, 1, NX_ACTION_SELECT) == NX_OK);
    for (uint64_t id=2; id<=63; ++id)
        CHECK(nx_semantics_request_action(ctx, id, NX_ACTION_PRESS) == NX_OK);
    CHECK(nx_semantics_request_action(ctx, 64, NX_ACTION_PRESS) == NX_ERROR_LIMIT);
    CHECK(nx_begin_frame(ctx,400,400,0)==NX_OK);
    for (uint64_t id=1; id<=70; ++id) {
        nx_widget_desc d = {0}; d.semantic=node(id,0,NX_ROLE_BUTTON,"Action");
        d.semantic.actions=NX_ACTION_PRESS | NX_ACTION_SELECT | NX_ACTION_FOCUS;
        d.flags=NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
        nx_interaction_state state;
        CHECK(nx_widget_register(ctx,&d,&state)==NX_OK);
        CHECK(state.clicked==(id<=63 ? 1u : 0u));
        if (id==2) CHECK(state.focused);
    }
    CHECK(nx_end_frame(ctx)==NX_OK);
    CHECK(nx_semantics_request_action(ctx,64,NX_ACTION_PRESS)==NX_OK);
    CHECK(nx_begin_frame(ctx,400,400,0)==NX_OK);
    /* Removing a target consumes no click on a different ID. */
    nx_widget_desc replacement={0}; replacement.semantic=node(80,0,NX_ROLE_BUTTON,"New");
    replacement.semantic.actions=NX_ACTION_PRESS; replacement.flags=NX_WIDGET_INTERACTIVE;
    nx_interaction_state state;
    CHECK(nx_widget_register(ctx,&replacement,&state)==NX_OK && !state.clicked);
    CHECK(nx_end_frame(ctx)==NX_OK);
    CHECK(nx_semantics_request_action(ctx,64,NX_ACTION_PRESS)==NX_ERROR_STATE);
    nx_context_destroy(ctx);
    return 0;
}

static int test_modal_action_rejection(void) {
    nx_context *ctx=NULL; CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    CHECK(nx_begin_frame(ctx,200,120,0)==NX_OK);
    nx_control_desc underlay={0}; underlay.id=1; underlay.bounds=(nx_rect){0,0,200,120};
    underlay.name=(nx_utf8_span){"Underlay",8}; nx_interaction_state state;
    CHECK(nx_button(ctx,&underlay,&state)==NX_OK);
    nx_overlay_desc overlay={0}; overlay.panel.id=10; overlay.backdrop_id=11;
    overlay.panel.name=(nx_utf8_span){"Dialog",6}; overlay.panel.bounds=(nx_rect){30,20,140,80};
    int dismissed=0; CHECK(nx_modal_begin(ctx,&overlay,&dismissed)==NX_OK);
    nx_control_desc inner=underlay; inner.id=12; inner.parent_id=10; inner.bounds=(nx_rect){50,30,60,20};
    CHECK(nx_button(ctx,&inner,&state)==NX_OK);
    CHECK(nx_overlay_end(ctx)==NX_OK && nx_end_frame(ctx)==NX_OK);
    uint32_t index; nx_semantic_view view;
    CHECK(nx_semantics_find(ctx,1,&index)==NX_OK && nx_semantics_get(ctx,index,&view)==NX_OK);
    CHECK(view.state & NX_SEM_DISABLED);
    CHECK(nx_semantics_request_action(ctx,1,NX_ACTION_PRESS)==NX_ERROR_STATE);
    CHECK(nx_semantics_request_action(ctx,1,NX_ACTION_FOCUS)==NX_ERROR_STATE);
    CHECK(nx_semantics_request_action(ctx,12,NX_ACTION_PRESS)==NX_OK);
    nx_context_destroy(ctx); return 0;
}

static int selection_frame(nx_context *ctx,uint64_t *choice,int required) {
    CHECK(nx_begin_frame(ctx,200,100,0)==NX_OK);
    nx_semantic_desc group=node(90,0,NX_ROLE_LIST,"Choices");
    CHECK(nx_selection_group(ctx,&group,required,choice)==NX_OK);
    for (uint64_t id=91;id<=93;++id) {
        nx_widget_desc d={0}; d.semantic=node(id,90,NX_ROLE_LIST_ITEM,"Item");
        d.semantic.actions=NX_ACTION_SELECT|NX_ACTION_FOCUS;
        d.semantic.state=*choice==id ? NX_SEM_SELECTED : 0;
        d.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE;
        nx_interaction_state state;
        CHECK(nx_widget_register(ctx,&d,&state)==NX_OK && !state.clicked);
    }
    CHECK(nx_end_frame(ctx)==NX_OK); return 0;
}
static int test_selection_models(void) {
    nx_context *ctx=NULL; CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    uint64_t choice=0; CHECK(selection_frame(ctx,&choice,0)==0);
    nx_semantic_view view; CHECK(nx_semantics_get(ctx,0,&view)==NX_OK && (view.state&NX_SEM_SELECTION_SINGLE));
    CHECK(nx_semantics_get(ctx,1,&view)==NX_OK && (view.actions&NX_ACTION_DESELECT));
    CHECK(nx_semantics_request_action(ctx,91,NX_ACTION_SELECT)==NX_OK);
    CHECK(nx_semantics_request_action(ctx,92,NX_ACTION_SELECT)==NX_OK);
    CHECK(selection_frame(ctx,&choice,0)==0 && choice==92);
    CHECK(nx_semantics_request_action(ctx,91,NX_ACTION_DESELECT)==NX_OK);
    CHECK(selection_frame(ctx,&choice,0)==0 && choice==92);
    CHECK(nx_semantics_request_action(ctx,92,NX_ACTION_DESELECT)==NX_OK);
    CHECK(selection_frame(ctx,&choice,0)==0 && choice==0);
    CHECK(nx_semantics_request_action(ctx,91,NX_ACTION_ADD_SELECTION)==NX_OK);
    CHECK(nx_semantics_request_action(ctx,92,NX_ACTION_ADD_SELECTION)==NX_OK);
    CHECK(selection_frame(ctx,&choice,0)==0 && choice==91);
    CHECK(selection_frame(ctx,&choice,1)==0);
    CHECK(nx_semantics_request_action(ctx,91,NX_ACTION_DESELECT)==NX_ERROR_UNSUPPORTED);
    CHECK(nx_semantics_get(ctx,1,&view)==NX_OK && !(view.actions&NX_ACTION_DESELECT));
    CHECK(nx_semantics_request_action(ctx,93,NX_ACTION_SELECT)==NX_OK);
    CHECK(nx_begin_frame(ctx,200,100,0)==NX_OK);
    CHECK(nx_selection_apply(ctx,90,1,&choice)==NX_OK && choice==93);
    CHECK(nx_selection_apply(ctx,90,1,&choice)==NX_OK && choice==93);
    nx_semantic_desc bad=node(200,0,NX_ROLE_LIST,"Raw"); bad.state=NX_SEM_SELECTION_SINGLE;
    CHECK(nx_semantics_add(ctx,&bad,NULL)==NX_ERROR_ARGUMENT);
    nx_semantic_desc group=node(90,0,NX_ROLE_LIST,"Choices");
    CHECK(nx_selection_group(ctx,&group,0,&choice)==NX_OK && choice==93);
    CHECK(nx_end_frame(ctx)==NX_OK);
    nx_context_destroy(ctx);
    /* An invalid completed projection must not become queryable/publishable. */
    CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    CHECK(nx_begin_frame(ctx,200,100,0)==NX_OK);
    choice=0; CHECK(nx_selection_group(ctx,&group,1,&choice)==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_ERROR_STATE);
    nx_context_destroy(ctx);
    CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    CHECK(nx_begin_frame(ctx,200,100,0)==NX_OK);
    CHECK(nx_selection_group(ctx,&group,0,&choice)==NX_OK);
    for (uint64_t id=91;id<=92;++id) {
        nx_widget_desc d={0};d.semantic=node(id,90,NX_ROLE_LIST_ITEM,"Item");
        d.semantic.actions=NX_ACTION_SELECT;d.semantic.state=NX_SEM_SELECTED;d.flags=NX_WIDGET_INTERACTIVE;
        nx_interaction_state state;CHECK(nx_widget_register(ctx,&d,&state)==NX_OK);
    }
    CHECK(nx_end_frame(ctx)==NX_ERROR_STATE);nx_context_destroy(ctx);return 0;
}

static int test_nested_tabs_and_selection_rollback(void) {
    nx_context *ctx=NULL; CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    uint64_t choice=91;
    for (unsigned frame=0;frame<2;++frame) {
        if (frame) {
            nx_event click={0};click.type=NX_EVENT_POINTER_DOWN;click.button=NX_BUTTON_LEFT;click.x=110;click.y=15;
            CHECK(nx_context_queue_event(ctx,&click)==NX_OK);click.type=NX_EVENT_POINTER_UP;CHECK(nx_context_queue_event(ctx,&click)==NX_OK);
        }
        CHECK(nx_begin_frame(ctx,200,100,0)==NX_OK);
        nx_semantic_desc group=node(90,0,NX_ROLE_TAB_LIST,"Tabs");
        CHECK(nx_selection_group(ctx,&group,0,&choice)==NX_ERROR_ARGUMENT);
        CHECK(nx_selection_group(ctx,&group,1,&choice)==NX_OK);
        for (unsigned i=0;i<2;++i) {
            nx_semantic_desc panel=node(94+i,90,NX_ROLE_PANEL,"Panel");CHECK(nx_semantics_add(ctx,&panel,NULL)==NX_OK);
            nx_control_desc tab={0};tab.id=91+i;tab.parent_id=94+i;tab.name=(nx_utf8_span){"Tab",3};tab.bounds=(nx_rect){90.f*i,0,80,30};
            nx_interaction_state state;CHECK(nx_tab(ctx,&tab,&choice,&state)==NX_OK);
        }
        CHECK(nx_end_frame(ctx)==NX_OK);
        CHECK(choice==(frame ? 92u : 91u));
    }
    nx_context_destroy(ctx);
    tracking t={0};nx_context_desc config={0};config.allocator=(nx_allocator){&t,tracked_allocate,tracked_deallocate};
    CHECK(nx_context_create(&config,&ctx)==NX_OK);choice=91;
    CHECK(selection_frame(ctx,&choice,1)==0);
    CHECK(nx_semantics_request_action(ctx,92,NX_ACTION_SELECT)==NX_OK);
    CHECK(nx_begin_frame(ctx,200,100,0)==NX_OK);
    char long_name[2048];memset(long_name,'A',sizeof(long_name));
    nx_semantic_desc group=node(90,0,NX_ROLE_LIST,NULL);group.name=(nx_utf8_span){long_name,sizeof(long_name)};
    t.fail_after=t.calls;
    CHECK(nx_selection_group(ctx,&group,1,&choice)==NX_ERROR_MEMORY && choice==91);
    t.fail_after=0;group.name=(nx_utf8_span){"Choices",7};
    CHECK(nx_selection_group(ctx,&group,1,&choice)==NX_OK && choice==92);
    uint64_t untouched=0;CHECK(nx_selection_group(ctx,&group,1,&untouched)==NX_ERROR_STATE && !untouched);
    nx_widget_desc item={0};item.semantic=node(92,90,NX_ROLE_LIST_ITEM,"Item");item.semantic.state=NX_SEM_SELECTED;item.semantic.actions=NX_ACTION_SELECT;item.flags=NX_WIDGET_INTERACTIVE;
    nx_interaction_state state;CHECK(nx_widget_register(ctx,&item,&state)==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK);nx_context_destroy(ctx);CHECK(t.frees+1u==t.calls);return 0;
}

int main(void) {
    if (test_tree_and_focus()) return 1;
    if (test_validation_and_depth()) return 1;
    if (test_allocation_and_reuse()) return 1;
    if (test_many_nodes_and_string_ownership()) return 1;
    if (test_action_dispatch_and_backpressure()) return 1;
    if (test_modal_action_rejection()) return 1;
    if (test_selection_models()) return 1;
    if (test_nested_tabs_and_selection_rollback()) return 1;
    puts("semantics tests passed");
    return 0;
}
