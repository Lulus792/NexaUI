#include "nexaui/nexaui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

/* Adding semantic commands must not change the existing event C ABI. */
typedef struct prior_event_layout {
    nx_event_type type; uint32_t modifiers; float x,y,delta_x,delta_y,width,height,scale;
    uint32_t button,contact_id; nx_key key; uint32_t scalar; int repeat,focused;
    uint64_t ime_target,ime_revision; uint32_t ime_before,ime_after; int32_t ime_index; uint32_t ime_length;
} prior_event_layout;
_Static_assert(sizeof(nx_event)==sizeof(prior_event_layout),"event ABI size");
_Static_assert(offsetof(nx_event,text_command)==offsetof(prior_event_layout,scalar),"event payload ABI");
_Static_assert(offsetof(nx_event,ime_target)==offsetof(prior_event_layout,ime_target),"event target ABI");

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#endif

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static int test_frame_transitions(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_event focus = {0};
    focus.type = NX_EVENT_FOCUS;
    focus.focused = 1;
    nx_event move = {0};
    move.type = NX_EVENT_POINTER_MOVE;
    move.x = 10;
    move.y = 20;
    nx_event down = move;
    down.type = NX_EVENT_POINTER_DOWN;
    down.button = NX_BUTTON_LEFT;
    nx_event key = {0};
    key.type = NX_EVENT_KEY_DOWN;
    key.key = NX_KEY_A;
    key.modifiers = NX_MOD_SHIFT;
    nx_event text_event = {0};
    text_event.type = NX_EVENT_TEXT;
    text_event.scalar = 0x1f30du;
    nx_event wheel = move;
    wheel.type = NX_EVENT_WHEEL;
    wheel.delta_x = 1;
    wheel.delta_y = -2;
    CHECK(nx_context_queue_event(ctx, &focus) == NX_OK);
    CHECK(nx_context_queue_event(ctx, &move) == NX_OK);
    CHECK(nx_context_queue_event(ctx, &down) == NX_OK);
    CHECK(nx_context_queue_event(ctx, &key) == NX_OK);
    CHECK(nx_context_queue_event(ctx, &text_event) == NX_OK);
    CHECK(nx_context_queue_event(ctx, &wheel) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 100, 0.016) == NX_OK);
    const nx_event *events;
    size_t count;
    CHECK(nx_frame_events(ctx, &events, &count) == NX_OK && count == 6);
    CHECK(events[0].type == NX_EVENT_FOCUS && events[4].scalar == 0x1f30du);
    nx_input_snapshot input;
    CHECK(nx_frame_input(ctx, &input) == NX_OK);
    CHECK(input.focused && input.pointer_x == 10 && input.pointer_y == 20);
    CHECK(input.buttons_down == NX_BUTTON_LEFT && input.buttons_pressed == NX_BUTTON_LEFT);
    CHECK(input.wheel_x == 1 && input.wheel_y == -2);
    CHECK(nx_key_down(ctx, NX_KEY_A) && nx_key_pressed(ctx, NX_KEY_A));
    CHECK(!nx_key_released(ctx, NX_KEY_A));
    nx_event up = down;
    up.type = NX_EVENT_POINTER_UP;
    key.type = NX_EVENT_KEY_UP;
    key.modifiers = 0;
    CHECK(nx_context_queue_event(ctx, &up) == NX_OK);
    CHECK(nx_context_queue_event(ctx, &key) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 100, 0.016) == NX_OK);
    CHECK(nx_frame_input(ctx, &input) == NX_OK);
    CHECK(!input.buttons_down && !input.buttons_pressed &&
          input.buttons_released == NX_BUTTON_LEFT);
    CHECK(!input.wheel_x && !input.wheel_y);
    CHECK(!nx_key_down(ctx, NX_KEY_A) && !nx_key_pressed(ctx, NX_KEY_A));
    CHECK(nx_key_released(ctx, NX_KEY_A));
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 100, 0.016) == NX_OK);
    CHECK(nx_frame_input(ctx, &input) == NX_OK);
    CHECK(!input.buttons_released && !nx_key_released(ctx, NX_KEY_A));
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_context_destroy(ctx);
    return 0;
}

static int test_focus_and_validation(void) {
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_event event = {0};
    event.type = NX_EVENT_TEXT;
    event.scalar = 0xd800u;
    CHECK(nx_context_queue_event(ctx, &event) == NX_ERROR_ARGUMENT);
    event.type = NX_EVENT_POINTER_MOVE;
    event.x = NAN;
    CHECK(nx_context_queue_event(ctx, &event) == NX_ERROR_ARGUMENT);
    event = (nx_event){0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = NX_KEY_ESCAPE;
    CHECK(nx_context_queue_event(ctx, &event) == NX_OK);
    event = (nx_event){0};
    event.type = NX_EVENT_FOCUS;
    event.focused = 0;
    CHECK(nx_context_queue_event(ctx, &event) == NX_OK);
    CHECK(nx_begin_frame(ctx, 10, 10, 0) == NX_OK);
    CHECK(!nx_key_down(ctx, NX_KEY_ESCAPE));
    CHECK(nx_key_pressed(ctx, NX_KEY_ESCAPE));
    CHECK(nx_key_released(ctx, NX_KEY_ESCAPE));
    nx_input_snapshot input;
    CHECK(nx_frame_input(ctx, &input) == NX_OK && !input.focused);
    nx_context_destroy(ctx);
    return 0;
}

typedef struct fail_state { unsigned calls, frees; } fail_state;
static void *fail_allocate(void *user, size_t size) {
    fail_state *state = (fail_state *)user;
    ++state->calls;
    return state->calls > 2 ? NULL : malloc(size);
}
static void fail_deallocate(void *user, void *pointer, size_t size) {
    fail_state *state = (fail_state *)user;
    (void)size;
    ++state->frees;
    free(pointer);
}

static int test_event_allocation_failure(void) {
    fail_state state = {0};
    nx_context_desc desc = {0};
    desc.allocator = (nx_allocator){&state, fail_allocate, fail_deallocate};
    desc.initial_event_capacity = 1;
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&desc, &ctx) == NX_OK);
    nx_event event = {0};
    event.type = NX_EVENT_FOCUS;
    event.focused = 1;
    CHECK(nx_context_queue_event(ctx, &event) == NX_ERROR_MEMORY);
    CHECK(nx_begin_frame(ctx, 10, 10, 0) == NX_OK);
    const nx_event *events;
    size_t count;
    CHECK(nx_frame_events(ctx, &events, &count) == NX_OK && count == 0);
    nx_context_destroy(ctx);
    CHECK(state.frees == 2);
    return 0;
}

typedef struct count_state { unsigned calls, frees; } count_state;
static void *count_allocate(void *user, size_t size) {
    count_state *state = (count_state *)user;
    void *pointer = malloc(size);
    if (pointer) ++state->calls;
    return pointer;
}
static void count_deallocate(void *user, void *pointer, size_t size) {
    count_state *state = (count_state *)user;
    (void)size;
    ++state->frees;
    free(pointer);
}

static int test_steady_state_events(void) {
    count_state state = {0};
    nx_context_desc desc = {0};
    desc.allocator = (nx_allocator){&state, count_allocate, count_deallocate};
    desc.initial_event_capacity = 1;
    nx_context *ctx = NULL;
    CHECK(nx_context_create(&desc, &ctx) == NX_OK);
    nx_event event = {0};
    event.type = NX_EVENT_POINTER_MOVE;
    for (unsigned frame = 0; frame < 100; ++frame) {
        event.x = (float)frame;
        CHECK(nx_context_queue_event(ctx, &event) == NX_OK);
        CHECK(nx_begin_frame(ctx, 100, 100, 0.01) == NX_OK);
        CHECK(nx_end_frame(ctx) == NX_OK);
        if (frame == 1) CHECK(state.calls == 4);
        if (frame > 1) CHECK(state.calls == 4);
    }
    nx_context_destroy(ctx);
    CHECK(state.frees == 4);
    return 0;
}

#ifdef _WIN32
static int test_win32_event_path(void) {
    fail_state failed = {0};
    nx_window_desc failing_desc = {0};
    failing_desc.width = 100;
    failing_desc.height = 100;
    failing_desc.flags = NX_WINDOW_HIDDEN;
    failing_desc.allocator = (nx_allocator){&failed, fail_allocate, fail_deallocate};
    nx_window *failed_window = NULL;
    CHECK(nx_window_create(&failing_desc, &failed_window) == NX_ERROR_MEMORY &&
          !failed_window && failed.frees == 2);
    nx_window_desc desc = {0};
    desc.title_utf8 = "NexaUI input test";
    desc.width = 320;
    desc.height = 200;
    desc.flags = NX_WINDOW_HIDDEN;
    nx_window *window = NULL;
    CHECK(nx_window_create(&desc, &window) == NX_OK);
    size_t clipboard_length = 0;
    CHECK(nx_window_clipboard_set_text(window, "\xc0", 1u) ==
          NX_ERROR_ARGUMENT);
    CHECK(nx_window_clipboard_get_text(window, NULL, 1u,
                                        &clipboard_length) ==
          NX_ERROR_ARGUMENT);
    HWND hwnd = FindWindowW(L"NexaUIWindowClass", L"NexaUI input test");
    CHECK(hwnd != NULL);
    nx_event event;
    int available;
    do {
        CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    } while (available);
    SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(10, 20));
    SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(11, 21));
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK && available);
    CHECK(event.type == NX_EVENT_POINTER_MOVE && event.x > 0 && event.y > 0);
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK && !available);
    SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(10, 20));
    SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(10, 20));
    SendMessageW(hwnd, WM_KEYDOWN, 'A', 0);
    SendMessageW(hwnd, WM_CHAR, 0xd83c, 0);
    SendMessageW(hwnd, WM_CHAR, 0xdf0d, 0);
    SendMessageW(hwnd, WM_KEYUP, 'A', 0);
    nx_context *ctx = NULL;
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    unsigned seen = 0, text_seen = 0;
    do {
        CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
        if (available) {
            ++seen;
            if (event.type == NX_EVENT_TEXT && event.scalar == 0x1f30du) ++text_seen;
            CHECK(nx_context_queue_event(ctx, &event) == NX_OK);
        }
    } while (available);
    CHECK(seen >= 5 && text_seen == 1);
    CHECK(nx_begin_frame(ctx, 320, 200, 0) == NX_OK);
    nx_input_snapshot input;
    CHECK(nx_frame_input(ctx, &input) == NX_OK);
    CHECK(input.buttons_pressed == NX_BUTTON_LEFT &&
          input.buttons_released == NX_BUTTON_LEFT && !input.buttons_down);
    CHECK(nx_key_pressed(ctx, NX_KEY_A) && nx_key_released(ctx, NX_KEY_A));
    nx_context_destroy(ctx);
    nx_window_destroy(window);
    return 0;
}
#endif

static int test_consumed_key_and_text_command(void) {
    nx_context *ctx=NULL; nx_interaction_state state;
    CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    CHECK(nx_begin_frame(ctx,100,100,0)==NX_OK);
    CHECK(nx_interact_region(ctx,1,(nx_rect){0,0,20,20},NX_REGION_FOCUSABLE,&state)==NX_OK);
    CHECK(nx_interact_region(ctx,2,(nx_rect){30,0,20,20},NX_REGION_FOCUSABLE,&state)==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK);
    nx_event event={0}; event.type=NX_EVENT_KEY_DOWN_CONSUMED; event.key=NX_KEY_TAB;
    CHECK(nx_context_queue_event(ctx,&event)==NX_OK);
    CHECK(nx_begin_frame(ctx,100,100,0)==NX_OK);
    CHECK(nx_key_down(ctx,NX_KEY_TAB) && nx_key_pressed(ctx,NX_KEY_TAB) && nx_focus_id(ctx)==0);
    CHECK(nx_interact_region(ctx,1,(nx_rect){0,0,20,20},NX_REGION_FOCUSABLE,&state)==NX_OK);
    CHECK(nx_interact_region(ctx,2,(nx_rect){30,0,20,20},NX_REGION_FOCUSABLE,&state)==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK);
    event.type=NX_EVENT_TEXT_COMMAND; event.text_command=NX_TEXT_COMMAND_TAB; event.modifiers=NX_MOD_SUPER;
    CHECK(nx_context_queue_event(ctx,&event)==NX_OK);
    event.text_command=NX_TEXT_COMMAND_DELETE_BACKWARD|NX_TEXT_COMMAND_EXTEND_SELECTION;
    CHECK(nx_context_queue_event(ctx,&event)==NX_ERROR_ARGUMENT);
    event.text_command=NX_TEXT_COMMAND_NONE;
    CHECK(nx_context_queue_event(ctx,&event)==NX_ERROR_ARGUMENT);
    event.text_command=NX_TEXT_COMMAND_COPY; event.ime_target=1;
    CHECK(nx_context_queue_event(ctx,&event)==NX_ERROR_ARGUMENT);
    CHECK(nx_begin_frame(ctx,100,100,0)==NX_OK && nx_focus_id(ctx)==1);
    CHECK(nx_interact_region(ctx,1,(nx_rect){0,0,20,20},NX_REGION_FOCUSABLE,&state)==NX_OK);
    CHECK(nx_interact_region(ctx,2,(nx_rect){30,0,20,20},NX_REGION_FOCUSABLE,&state)==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK);
    nx_context_destroy(ctx); return 0;
}

int main(void) {
    if (test_consumed_key_and_text_command()) return 1;
    if (test_frame_transitions()) return 1;
    if (test_focus_and_validation()) return 1;
    if (test_event_allocation_failure()) return 1;
    if (test_steady_state_events()) return 1;
#ifdef _WIN32
    if (test_win32_event_path()) return 1;
#endif
    puts("input tests passed");
    return 0;
}
