#include <nexaui/nexaui.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (0)
static nx_action_desc action(uint64_t id) {
    nx_action_desc d = {0}; d.control.id = id; d.control.bounds = (nx_rect){5,5,80,40};
    d.control.name = (nx_utf8_span){"Files",5}; d.variant = NX_ACTION_NAVIGATION;
    d.icon = NX_ICON_FOLDER; d.icon_bounds = (nx_rect){12,12,24,24}; return d;
}
static int key(nx_context *ui, nx_key code) {
    nx_event e = {0}; e.type = NX_EVENT_KEY_DOWN; e.key = code;
    CHECK(nx_context_queue_event(ui,&e) == NX_OK); e.type = NX_EVENT_KEY_UP;
    CHECK(nx_context_queue_event(ui,&e) == NX_OK); return 0;
}
static int controls(void) {
    nx_context *ui = NULL; CHECK(nx_context_create(NULL,&ui) == NX_OK);
    nx_action_desc d = action(1); nx_interaction_state state;
    CHECK(nx_begin_frame(ui,120,100,0) == NX_OK);
    CHECK(nx_action(ui,&d,&state) == NX_OK && !state.clicked);
    CHECK(nx_end_frame(ui) == NX_OK);
    CHECK(nx_focus_set(ui,1) == NX_OK); CHECK(key(ui,NX_KEY_SPACE) == 0);
    CHECK(nx_begin_frame(ui,120,100,0) == NX_OK);
    d.selected = 1; CHECK(nx_action(ui,&d,&state) == NX_OK && state.clicked && state.focused);
    uint32_t index; nx_semantic_view view;
    nx_progress_desc p = {2,0,{5,60,80,8},{"Upload",6},.625};
    CHECK(nx_progress(ui,&p) == NX_OK);
    p.id = 3; p.value = NAN; CHECK(nx_progress(ui,&p) == NX_ERROR_ARGUMENT);
    CHECK(nx_end_frame(ui) == NX_OK);
    CHECK(nx_semantics_find(ui,1,&index) == NX_OK);
    CHECK(nx_semantics_get(ui,index,&view) == NX_OK && view.role == NX_ROLE_TAB &&
        (view.state & NX_SEM_SELECTED) && (view.actions & NX_ACTION_SELECT));
    CHECK(nx_semantics_find(ui,2,&index) == NX_OK && nx_semantics_get(ui,index,&view) == NX_OK);
    CHECK(view.role == NX_ROLE_PROGRESS && (view.state & NX_SEM_READONLY) &&
        view.value.length == 3 && memcmp(view.value.data,"62%",3) == 0);
    CHECK(nx_semantics_find(ui,3,&index) != NX_OK);
    CHECK(key(ui,NX_KEY_ENTER) == 0); d.control.disabled = 1;
    CHECK(nx_begin_frame(ui,120,100,0) == NX_OK);
    CHECK(nx_action(ui,&d,&state) == NX_OK && !state.clicked && !state.focused);
    CHECK(nx_end_frame(ui) == NX_OK);
    nx_context_destroy(ui); return 0;
}

static int icons_and_surface(void) {
    nx_context *ui = NULL; CHECK(nx_context_create(NULL,&ui) == NX_OK);
    CHECK(nx_begin_frame(ui,800,120,0) == NX_OK);
    for (int i = NX_ICON_CLOUD; i < NX_ICON_COUNT; ++i)
        CHECK(nx_draw_icon(ui,(nx_icon)i,(nx_rect){(float)(i-1)*27,5,24,24},1.5f,
            (nx_color){1,1,1,1}) == NX_OK);
    CHECK(nx_draw_icon(ui,NX_ICON_FOLDER,(nx_rect){0,0,NAN,24},1,(nx_color){1,1,1,1}) == NX_ERROR_ARGUMENT);
    nx_surface_style style = {{.2f,.4f,.6f,1},{1,0,0,1},10,2};
    CHECK(nx_draw_surface(ui,(nx_rect){10,45,40,40},&style) == NX_OK);
    CHECK(nx_end_frame(ui) == NX_OK);
    const nx_command *commands; size_t count;
    CHECK(nx_frame_commands(ui,&commands,&count) == NX_OK);
    uint8_t *pixels = malloc(800*120*4); CHECK(pixels);
    CHECK(nx_headless_render(commands,count,pixels,800,120,800*4,(nx_color){0,0,0,1}) == NX_OK);
    CHECK(pixels[(45*800+10)*4] == 0); /* Rounded outside corner remains clear. */
    CHECK(pixels[(45*800+30)*4] == 255); /* Border at the top center. */
    CHECK(pixels[(50*800+30)*4] == 51 && pixels[(50*800+30)*4+1] == 102);
    free(pixels); nx_context_destroy(ui); return 0;
}

static double linear(double c) { return c <= .04045 ? c / 12.92 : pow((c + .055) / 1.055, 2.4); }
static double luminance(nx_color c) {
    return .2126 * linear(c.r) + .7152 * linear(c.g) + .0722 * linear(c.b);
}
static int primary_contrast(void) {
    for (int kind = NX_THEME_DARK; kind <= NX_THEME_MIDNIGHT; ++kind)
        for (unsigned phase = 0; phase < 4; ++phase) {
            nx_context *ui; nx_theme theme; nx_interaction_state state;
            CHECK(nx_context_create(NULL, &ui) == NX_OK);
            CHECK(nx_theme_preset((nx_theme_kind)kind, &theme) == NX_OK);
            CHECK(nx_context_set_theme(ui, &theme) == NX_OK);
            nx_action_desc d = action(1); d.variant = NX_ACTION_PRIMARY; d.selected = phase == 3;
            CHECK(nx_begin_frame(ui, 120, 100, 0) == NX_OK);
            CHECK(nx_action(ui, &d, &state) == NX_OK); CHECK(nx_end_frame(ui) == NX_OK);
            if (phase == 1 || phase == 2) {
                nx_event e = {0}; e.type = phase == 1 ? NX_EVENT_POINTER_MOVE : NX_EVENT_POINTER_DOWN;
                e.button = NX_BUTTON_LEFT; e.x = e.y = 20;
                CHECK(nx_context_queue_event(ui, &e) == NX_OK);
            }
            CHECK(nx_begin_frame(ui, 120, 100, 0) == NX_OK);
            CHECK(nx_action(ui, &d, &state) == NX_OK); CHECK(nx_end_frame(ui) == NX_OK);
            if (phase == 1) CHECK(state.hovered);
            if (phase == 2) CHECK(state.held);
            const nx_command *commands; size_t count;
            CHECK(nx_frame_commands(ui, &commands, &count) == NX_OK);
            uint8_t pixels[120 * 100 * 4];
            CHECK(nx_headless_render(commands, count, pixels, 120, 100, 120 * 4, (nx_color){0,0,0,1}) == NX_OK);
            const uint8_t *p = pixels + (25 * 120 + 65) * 4;
            nx_color background = {p[0] / 255.f, p[1] / 255.f, p[2] / 255.f, 1};
            unsigned strokes = 0;
            for (size_t i = 0; i < count; ++i) if (commands[i].type == NX_COMMAND_LINE) {
                double a = luminance(background), b = luminance(commands[i].color);
                double ratio = (fmax(a,b) + .05) / (fmin(a,b) + .05);
                if (ratio < 4.5) fprintf(stderr, "primary theme %d phase %u contrast %.3f\n", kind, phase, ratio);
                CHECK(ratio >= 4.5); ++strokes;
            }
            CHECK(strokes); nx_context_destroy(ui);
        }
    return 0;
}

typedef struct fail_allocator { size_t calls, fail; } fail_allocator;
static void *allocate(void *user, size_t size) {
    fail_allocator *a = user; if (++a->calls == a->fail) return NULL; return malloc(size);
}
static void deallocate(void *user, void *p, size_t size) { (void)user; (void)size; free(p); }
static int rollback(void) {
    unsigned failures = 0;
    for (size_t offset = 1; offset <= 20; ++offset) {
        fail_allocator a = {0}; nx_context_desc desc = {0};
        desc.allocator = (nx_allocator){&a,allocate,deallocate};
        desc.initial_command_capacity = desc.initial_region_capacity = desc.initial_semantic_capacity = 1;
        desc.initial_semantic_text_capacity = 1;
        nx_context *ui; CHECK(nx_context_create(&desc,&ui) == NX_OK);
        CHECK(nx_begin_frame(ui,100,60,0) == NX_OK);
        a.fail = a.calls + offset;
        nx_action_desc d = action(1); d.icon = NX_ICON_SETTINGS;
        nx_interaction_state state; memset(&state,0x7f,sizeof(state));
        nx_result result = nx_action(ui,&d,&state);
        if (result == NX_ERROR_MEMORY) {
            ++failures; uint32_t index;
            nx_interaction_state zero = {0}; CHECK(memcmp(&state,&zero,sizeof(state)) == 0);
            CHECK(nx_end_frame(ui) == NX_OK);
            CHECK(nx_semantics_find(ui,1,&index) != NX_OK);
            const nx_command *commands; size_t count;
            CHECK(nx_frame_commands(ui,&commands,&count) == NX_OK && count == 0);
            CHECK(nx_begin_frame(ui,100,60,0) == NX_OK);
            a.fail = 0; CHECK(nx_action(ui,&d,&state) == NX_OK);
            CHECK(nx_end_frame(ui) == NX_OK);
        } else { CHECK(result == NX_OK); CHECK(nx_end_frame(ui) == NX_OK); }
        nx_context_destroy(ui);
    }
    CHECK(failures >= 4); return 0;
}
int main(void) { return controls() || icons_and_surface() || primary_contrast() || rollback(); }
