#include "nexaui/nexaui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define WIDTH 160u
#define HEIGHT 72u

static int save_frame(nx_context *ui, const char *prefix, unsigned frame) {
    char path[512];
    int length = snprintf(path, sizeof(path), "%s_%u.ppm", prefix, frame);
    if (length < 0 || (size_t)length >= sizeof(path)) return 0;
    const nx_command *commands;
    size_t count;
    if (nx_frame_commands(ui, &commands, &count) != NX_OK) return 0;
    uint8_t *pixels = (uint8_t *)malloc((size_t)WIDTH * HEIGHT * 4u);
    if (!pixels) return 0;
    int okay = nx_headless_render(commands, count, pixels, WIDTH, HEIGHT,
                                 WIDTH * 4u, (nx_color){0.07f, 0.09f, 0.13f, 1}) == NX_OK;
    FILE *file = okay ? fopen(path, "wb") : NULL;
    if (!file) okay = 0;
    if (okay && fprintf(file, "P6\n%u %u\n255\n", WIDTH, HEIGHT) < 0) okay = 0;
    for (size_t i = 0; okay && i < (size_t)WIDTH * HEIGHT; ++i)
        if (fwrite(pixels + i * 4u, 1, 3, file) != 3) okay = 0;
    if (file && fclose(file) != 0) okay = 0;
    free(pixels);
    return okay;
}

static int draw_frame(nx_context *ui, const char *prefix, unsigned frame) {
    if (nx_begin_frame(ui, WIDTH, HEIGHT, 1.0 / 60.0) != NX_OK) return 0;
    uint64_t id = nx_id(ui, "primary-action", sizeof("primary-action") - 1u);
    uint64_t root_id = nx_id(ui, "window", sizeof("window") - 1u);
    nx_rect rect = {20, 16, 120, 40};
    nx_interaction_state state;
    if (!id || !root_id ||
        nx_interact_region(ui, id, rect, NX_REGION_FOCUSABLE, &state) != NX_OK)
        return 0;
    nx_semantic_desc root = {0};
    root.id = root_id;
    root.role = NX_ROLE_WINDOW;
    root.bounds = (nx_rect){0, 0, WIDTH, HEIGHT};
    root.name = (nx_utf8_span){"Interactive region demo", 23};
    nx_semantic_desc button = {0};
    button.id = id;
    button.parent_id = root_id;
    button.role = NX_ROLE_BUTTON;
    button.bounds = rect;
    button.name = (nx_utf8_span){"Primary action", 14};
    button.actions = NX_ACTION_PRESS | NX_ACTION_FOCUS;
    if (nx_semantics_add(ui, &root, NULL) != NX_OK ||
        nx_semantics_add(ui, &button, NULL) != NX_OK) return 0;
    nx_color color = state.clicked ? (nx_color){0.20f, 0.75f, 0.43f, 1}
                    : state.held ? (nx_color){0.13f, 0.42f, 0.65f, 1}
                    : state.hovered ? (nx_color){0.30f, 0.65f, 0.90f, 1}
                                    : (nx_color){0.23f, 0.54f, 0.78f, 1};
    if (nx_fill_rect(ui, rect, color) != NX_OK || nx_end_frame(ui) != NX_OK)
        return 0;
    printf("frame %u: hovered=%d held=%d focused=%d clicked=%u\n",
           frame, state.hovered, state.held, state.focused, state.clicked);
    nx_semantic_view view;
    if (nx_semantics_get(ui, 1, &view) != NX_OK ||
        view.id != id || view.role != NX_ROLE_BUTTON) return 0;
    printf("  semantic: %.*s focused=%d actions=%u\n",
           (int)view.name.length, view.name.data,
           !!(view.state & NX_SEM_FOCUSED), view.actions);
    return save_frame(ui, prefix, frame);
}

int main(int argc, char **argv) {
    const char *prefix = argc > 1 ? argv[1] : "interaction";
    nx_context *ui = NULL;
    if (nx_context_create(NULL, &ui) != NX_OK) return 1;
    int okay = draw_frame(ui, prefix, 0);
    nx_event event = {0};
    event.type = NX_EVENT_POINTER_DOWN;
    event.x = 80;
    event.y = 36;
    event.button = NX_BUTTON_LEFT;
    if (okay) okay = nx_context_queue_event(ui, &event) == NX_OK;
    if (okay) okay = draw_frame(ui, prefix, 1);
    event.type = NX_EVENT_POINTER_UP;
    if (okay) okay = nx_context_queue_event(ui, &event) == NX_OK;
    if (okay) okay = draw_frame(ui, prefix, 2);
    nx_context_destroy(ui);
    if (!okay) fprintf(stderr, "Interactive example failed\n");
    return okay ? 0 : 1;
}
