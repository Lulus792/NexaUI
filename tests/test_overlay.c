#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static nx_control_desc button(uint64_t id, nx_rect bounds) {
    nx_control_desc desc = {0};
    desc.id = id;
    desc.bounds = bounds;
    desc.name = (nx_utf8_span){"Action", 6};
    return desc;
}

static nx_overlay_desc overlay(uint64_t id, uint64_t backdrop_id) {
    nx_overlay_desc desc = {0};
    desc.panel.id = id;
    desc.panel.bounds = (nx_rect){40, 20, 120, 80};
    desc.panel.name = (nx_utf8_span){"Actions", 7};
    desc.backdrop_id = backdrop_id;
    desc.dismiss_on_outside = 1;
    desc.dismiss_on_escape = 1;
    return desc;
}

static int click(nx_context *ctx, float x, float y) {
    nx_event event = {0};
    event.type = NX_EVENT_POINTER_DOWN;
    event.button = NX_BUTTON_LEFT;
    event.x = x;
    event.y = y;
    if (nx_context_queue_event(ctx, &event) != NX_OK) return 0;
    event.type = NX_EVENT_POINTER_UP;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int key(nx_context *ctx, nx_key code, uint32_t modifiers) {
    nx_event event = {0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = code;
    event.modifiers = modifiers;
    if (nx_context_queue_event(ctx, &event) != NX_OK) return 0;
    event.type = NX_EVENT_KEY_UP;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int test_modal_popup_tooltip(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_control_desc underlay = button(1, (nx_rect){0, 0, 200, 120});
    nx_control_desc first = button(12, (nx_rect){50, 30, 60, 20});
    first.parent_id = 10;
    nx_control_desc second = button(13, (nx_rect){50, 60, 60, 20});
    second.parent_id = 10;
    nx_overlay_desc modal = overlay(10, 11);
    nx_interaction_state state;
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_focus_set(ctx, 1) == NX_OK);

    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK);
    int dismissed = 1;
    CHECK(nx_modal_begin(ctx, &modal, &dismissed) == NX_OK && !dismissed);
    CHECK(nx_end_frame(ctx) == NX_ERROR_STATE);
    CHECK(nx_button(ctx, &first, &state) == NX_OK);
    CHECK(nx_button(ctx, &second, &state) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_focus_id(ctx) == 12);
    uint32_t index;
    nx_semantic_view view;
    CHECK(nx_semantics_find(ctx, 10, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
          view.role == NX_ROLE_DIALOG);
    const nx_command *commands;
    size_t command_count;
    uint8_t pixels[200u * 120u * 4u];
    CHECK(nx_frame_commands(ctx, &commands, &command_count) == NX_OK);
    CHECK(nx_headless_render(commands, command_count, pixels, 200, 120,
        200u * 4u, (nx_color){0, 0, 0, 0}) == NX_OK);
    CHECK(pixels[(10u * 200u + 10u) * 4u + 3u] != 0);
    CHECK(memcmp(&pixels[(10u * 200u + 10u) * 4u],
                 &pixels[(25u * 200u + 45u) * 4u], 4u) != 0);

    CHECK(key(ctx, NX_KEY_TAB, NX_MOD_SHIFT));
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK && !state.clicked);
    CHECK(nx_modal_begin(ctx, &modal, &dismissed) == NX_OK && !dismissed);
    CHECK(nx_button(ctx, &first, &state) == NX_OK);
    CHECK(nx_button(ctx, &second, &state) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_focus_id(ctx) == 13);
    CHECK(nx_focus_set(ctx, 1) == NX_ERROR_ARGUMENT);
    CHECK(key(ctx, NX_KEY_TAB, 0));
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK);
    CHECK(nx_modal_begin(ctx, &modal, &dismissed) == NX_OK && !dismissed);
    CHECK(nx_button(ctx, &first, &state) == NX_OK);
    CHECK(nx_button(ctx, &second, &state) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 12);

    CHECK(click(ctx, 10, 10));
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK && !state.clicked);
    CHECK(nx_modal_begin(ctx, &modal, &dismissed) == NX_OK && dismissed);
    CHECK(nx_button(ctx, &first, &state) == NX_OK);
    CHECK(nx_button(ctx, &second, &state) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 1);

    CHECK(click(ctx, 10, 10));
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK && state.clicked == 1);
    CHECK(nx_end_frame(ctx) == NX_OK);

    nx_container_desc tooltip = {0};
    tooltip.id = 30;
    tooltip.bounds = (nx_rect){5, 5, 50, 20};
    tooltip.name = (nx_utf8_span){"Hint", 4};
    nx_overlay_desc popup = overlay(20, 21);
    nx_control_desc popup_child = button(22, (nx_rect){50, 30, 60, 20});
    popup_child.parent_id = 20;
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK);
    CHECK(nx_tooltip(ctx, &tooltip) == NX_OK);
    CHECK(nx_popup_begin(ctx, &popup, &dismissed) == NX_OK && !dismissed);
    CHECK(nx_button(ctx, &popup_child, &state) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 22);
    CHECK(nx_semantics_find(ctx, 30, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
          view.role == NX_ROLE_TOOLTIP);
    CHECK(nx_semantics_find(ctx, 20, &index) == NX_OK);
    CHECK(nx_semantics_get(ctx, index, &view) == NX_OK &&
          view.role == NX_ROLE_POPUP);
    CHECK(key(ctx, NX_KEY_ESCAPE, 0));
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK);
    CHECK(nx_popup_begin(ctx, &popup, &dismissed) == NX_OK && dismissed);
    CHECK(nx_button(ctx, &popup_child, &state) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 1);
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK);
    CHECK(nx_popup_begin(ctx, &popup, &dismissed) == NX_OK && !dismissed);
    CHECK(nx_button(ctx, &popup_child, &state) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 22);
    CHECK(nx_overlay_close(ctx) == NX_OK && nx_focus_id(ctx) == 1);
    CHECK(click(ctx, 10, 10));
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    CHECK(nx_button(ctx, &underlay, &state) == NX_OK && state.clicked == 1);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_context_destroy(ctx);
    return 0;
}

static void *fail_allocate(void *user, size_t size) {
    size_t *fail_size = (size_t *)user;
    if (*fail_size == size) {
        *fail_size = 0;
        return NULL;
    }
    return malloc(size);
}

static void fail_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static int test_failure_retry(void) {
    size_t fail_size = 4u * sizeof(nx_command);
    nx_context_desc config = {0};
    config.initial_command_capacity = 2;
    config.allocator = (nx_allocator){&fail_size, fail_allocate,
                                       fail_deallocate};
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&config, &ctx) == NX_OK);
    nx_overlay_desc modal = overlay(10, 11);
    CHECK(nx_begin_frame(ctx, 200, 120, 0) == NX_OK);
    int dismissed = 1;
    CHECK(nx_modal_begin(ctx, &modal, &dismissed) == NX_ERROR_MEMORY);
    CHECK(!dismissed);
    CHECK(nx_modal_begin(ctx, &modal, &dismissed) == NX_OK);
    CHECK(nx_overlay_end(ctx) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK && nx_focus_id(ctx) == 10);
    size_t count;
    CHECK(nx_semantics_count(ctx, &count) == NX_OK && count == 1);
    nx_context_destroy(ctx);
    return 0;
}

static int nested_item_clicked;
static int nested_frame(nx_context *ctx,int with_child,int *outer_dismissed,int *inner_dismissed) {
    nx_control_desc background=button(1,(nx_rect){0,0,200,120});
    nx_control_desc trigger=button(12,(nx_rect){50,30,60,20});trigger.parent_id=10;
    nx_control_desc item=button(22,(nx_rect){80,50,50,20});item.parent_id=20;
    nx_overlay_desc outer=overlay(10,11),inner=overlay(20,21);inner.panel.parent_id=12;
    inner.panel.bounds=(nx_rect){70,40,70,40};
    nx_interaction_state state;
    if(nx_begin_frame(ctx,200,120,0)!=NX_OK || nx_button(ctx,&background,&state)!=NX_OK ||
       nx_modal_begin(ctx,&outer,outer_dismissed)!=NX_OK || nx_button(ctx,&trigger,&state)!=NX_OK) return 0;
    if(with_child && (nx_popup_begin(ctx,&inner,inner_dismissed)!=NX_OK ||
       nx_button(ctx,&item,&state)!=NX_OK || nx_overlay_end(ctx)!=NX_OK)) return 0;
    nested_item_clicked=with_child?state.clicked:0;
    return nx_overlay_end(ctx)==NX_OK && nx_end_frame(ctx)==NX_OK;
}

static int test_nested_overlay(void) {
    nx_context *ctx=NULL;CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    nx_control_desc background=button(1,(nx_rect){0,0,200,120}); nx_interaction_state state;
    CHECK(nx_begin_frame(ctx,200,120,0)==NX_OK && nx_button(ctx,&background,&state)==NX_OK && nx_end_frame(ctx)==NX_OK);
    CHECK(nx_focus_set(ctx,1)==NX_OK);
    int outer=0,inner=0;
    CHECK(nested_frame(ctx,0,&outer,&inner) && !outer && nx_focus_id(ctx)==12);
    CHECK(nested_frame(ctx,1,&outer,&inner) && !outer && !inner && nx_focus_id(ctx)==22);
    uint32_t index;nx_semantic_view view;
    CHECK(nx_semantics_find(ctx,12,&index)==NX_OK && nx_semantics_get(ctx,index,&view)==NX_OK && (view.state&NX_SEM_DISABLED));
    CHECK(nx_semantics_request_action(ctx,12,NX_ACTION_PRESS)==NX_ERROR_STATE);
    CHECK(nx_semantics_find(ctx,22,&index)==NX_OK && nx_semantics_get(ctx,index,&view)==NX_OK && !(view.state&NX_SEM_DISABLED));
    CHECK(nx_semantics_request_action(ctx,22,NX_ACTION_PRESS)==NX_OK);
    CHECK(nested_frame(ctx,1,&outer,&inner) && nested_item_clicked==1 && nx_focus_id(ctx)==22);
    CHECK(key(ctx,NX_KEY_TAB,NX_MOD_SHIFT));
    CHECK(nested_frame(ctx,1,&outer,&inner) && !outer && !inner && nx_focus_id(ctx)==22);
    CHECK(key(ctx,NX_KEY_ESCAPE,0));
    CHECK(nested_frame(ctx,1,&outer,&inner) && !outer && inner && nx_focus_id(ctx)==12);
    CHECK(nested_frame(ctx,0,&outer,&inner) && nx_focus_id(ctx)==12);
    CHECK(nested_frame(ctx,1,&outer,&inner) && !outer && !inner && nx_focus_id(ctx)==22);
    CHECK(nx_overlay_close(ctx)==NX_OK && nx_focus_id(ctx)==12);
    CHECK(nx_semantics_request_action(ctx,22,NX_ACTION_PRESS)==NX_ERROR_STATE);
    CHECK(nested_frame(ctx,0,&outer,&inner) && nx_focus_id(ctx)==12);
    CHECK(nested_frame(ctx,1,&outer,&inner) && !inner && nx_focus_id(ctx)==22);
    CHECK(click(ctx,55,35));
    CHECK(nested_frame(ctx,1,&outer,&inner) && !outer && inner && nx_focus_id(ctx)==12);
    CHECK(nested_frame(ctx,0,&outer,&inner) && nx_focus_id(ctx)==12);
    CHECK(nested_frame(ctx,1,&outer,&inner) && !inner && nx_focus_id(ctx)==22);
    CHECK(nested_frame(ctx,0,&outer,&inner) && nx_focus_id(ctx)==12);
    CHECK(key(ctx,NX_KEY_ESCAPE,0));
    CHECK(nested_frame(ctx,0,&outer,&inner) && outer && nx_focus_id(ctx)==1);
    /* Both layers may be opened in the same frame before parent focus was
       published; closing the child still returns to the parent's first control. */
    CHECK(nested_frame(ctx,1,&outer,&inner) && !outer && !inner && nx_focus_id(ctx)==22);
    CHECK(nx_overlay_close(ctx)==NX_OK && nx_focus_id(ctx)==12);
    nx_context_destroy(ctx);return 0;
}

/* Two independently owned top-level popups remain visible. Only the last
   popup receives input; dismissing it restores the earlier popup's focus. */
static int sibling_frame(nx_context *ctx,int show_second,int *first_dismissed,int *second_dismissed) {
    nx_control_desc background=button(1,(nx_rect){0,0,200,120});
    nx_control_desc first_item=button(12,(nx_rect){50,30,40,20});first_item.parent_id=10;
    nx_control_desc second_item=button(22,(nx_rect){100,50,40,20});second_item.parent_id=20;
    nx_overlay_desc first=overlay(10,11),second=overlay(20,21);
    nx_interaction_state state;
    if(nx_begin_frame(ctx,200,120,0)!=NX_OK || nx_button(ctx,&background,&state)!=NX_OK ||
       nx_popup_begin(ctx,&first,first_dismissed)!=NX_OK || nx_button(ctx,&first_item,&state)!=NX_OK ||
       nx_overlay_end(ctx)!=NX_OK) return 0;
    if(show_second && (nx_popup_begin(ctx,&second,second_dismissed)!=NX_OK ||
       nx_button(ctx,&second_item,&state)!=NX_OK || nx_overlay_end(ctx)!=NX_OK)) return 0;
    return nx_end_frame(ctx)==NX_OK;
}
static int test_sibling_overlays(void) {
    nx_context *ctx=NULL;CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    nx_control_desc background=button(1,(nx_rect){0,0,200,120});nx_interaction_state state;
    CHECK(nx_begin_frame(ctx,200,120,0)==NX_OK && nx_button(ctx,&background,&state)==NX_OK && nx_end_frame(ctx)==NX_OK);
    CHECK(nx_focus_set(ctx,1)==NX_OK);
    int first=0,second=0;
    CHECK(sibling_frame(ctx,0,&first,&second) && nx_focus_id(ctx)==12);
    CHECK(sibling_frame(ctx,1,&first,&second) && nx_focus_id(ctx)==22);
    CHECK(nx_semantics_request_action(ctx,12,NX_ACTION_PRESS)==NX_ERROR_STATE);
    CHECK(nx_semantics_request_action(ctx,22,NX_ACTION_PRESS)==NX_OK);
    CHECK(sibling_frame(ctx,1,&first,&second) && nx_focus_id(ctx)==22);
    CHECK(nx_overlay_close(ctx)==NX_OK && nx_focus_id(ctx)==12);
    CHECK(sibling_frame(ctx,0,&first,&second) && nx_focus_id(ctx)==12);
    CHECK(nx_overlay_close(ctx)==NX_OK && nx_focus_id(ctx)==1);
    nx_context_destroy(ctx);return 0;
}

static void *fail_next_allocate(void *user,size_t size) {
    int *fail=user;if (*fail) { *fail=0;return NULL; } return malloc(size);
}
static int test_nested_failure_retry(void) {
    int fail=0; nx_context_desc config={0};
    config.allocator=(nx_allocator){&fail,fail_next_allocate,fail_deallocate};
    nx_context *ctx=NULL;CHECK(nx_context_create(&config,&ctx)==NX_OK);
    nx_overlay_desc outer=overlay(10,11),inner=overlay(20,21);inner.panel.parent_id=10;
    static char long_name[8192];memset(long_name,'a',sizeof(long_name));
    inner.panel.name=(nx_utf8_span){long_name,sizeof(long_name)};
    int dismissed=0;CHECK(nx_begin_frame(ctx,200,120,0)==NX_OK);
    CHECK(nx_modal_begin(ctx,&outer,&dismissed)==NX_OK && !dismissed);
    nx_overlay_desc wrong=inner;wrong.panel.parent_id=0;
    CHECK(nx_popup_begin(ctx,&wrong,&dismissed)==NX_ERROR_STATE && !dismissed);
    fail=1;CHECK(nx_popup_begin(ctx,&inner,&dismissed)==NX_ERROR_MEMORY && !dismissed);
    CHECK(nx_popup_begin(ctx,&inner,&dismissed)==NX_OK && !dismissed);
    CHECK(nx_overlay_end(ctx)==NX_OK && nx_overlay_end(ctx)==NX_OK && nx_end_frame(ctx)==NX_OK);
    CHECK(nx_focus_id(ctx)==20);
    CHECK(nx_overlay_close(ctx)==NX_OK && nx_focus_id(ctx)==10);
    nx_context_destroy(ctx);return 0;
}

static int test_nested_depth_limit(void) {
    nx_context *ctx=NULL;CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    CHECK(nx_begin_frame(ctx,200,120,0)==NX_OK);
    int dismissed=0;
    for(unsigned i=0;i<16;++i) {
        nx_overlay_desc d=overlay(100u+i*2u,101u+i*2u);
        if(i) d.panel.parent_id=100u+(i-1u)*2u;
        CHECK(nx_popup_begin(ctx,&d,&dismissed)==NX_OK && !dismissed);
    }
    nx_overlay_desc overflow=overlay(200,201);overflow.panel.parent_id=130;
    CHECK(nx_popup_begin(ctx,&overflow,&dismissed)==NX_ERROR_LIMIT);
    for(unsigned i=0;i<16;++i) CHECK(nx_overlay_end(ctx)==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK && nx_focus_id(ctx)==130);
    CHECK(nx_overlay_close(ctx)==NX_OK && nx_focus_id(ctx)==128);
    nx_context_destroy(ctx);return 0;
}

int main(void) {
    if (test_modal_popup_tooltip() || test_failure_retry() || test_nested_overlay() || test_nested_failure_retry() || test_nested_depth_limit() || test_sibling_overlays()) return 1;
    puts("overlay tests passed");
    return 0;
}
