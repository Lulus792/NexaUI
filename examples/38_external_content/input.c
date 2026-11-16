#include "input.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_MODS (NX_MOD_SHIFT | NX_MOD_CONTROL | NX_MOD_ALT | NX_MOD_SUPER)
#define INPUT_BUTTONS (NX_BUTTON_LEFT | NX_BUTTON_RIGHT | NX_BUTTON_MIDDLE | NX_BUTTON_X1 | NX_BUTTON_X2)
#define INPUT_PIXELS UINT32_C(16777216)
struct content_input {
    uint64_t generation;
    nx_key escape_key;
    uint32_t escape_modifiers;
    content_input_sink sink;
    void *user;
    content_input_target target;
    unsigned char physical[NX_KEY_COUNT], held[NX_KEY_COUNT], draining[NX_KEY_COUNT];
    uint32_t physical_buttons, held_buttons, draining_buttons;
    float pointer_x, pointer_y;
    int active, window_focused, fenced, busy;
    nx_result last_error;
};
static int key_valid(nx_key key) { return key > NX_KEY_UNKNOWN && key < NX_KEY_COUNT; }
static uint32_t modifier(nx_key key) {
    switch (key) {
    case NX_KEY_SHIFT: return NX_MOD_SHIFT;
    case NX_KEY_CONTROL: return NX_MOD_CONTROL;
    case NX_KEY_ALT: return NX_MOD_ALT;
    case NX_KEY_SUPER: return NX_MOD_SUPER;
    default: return 0;
    }
}
static size_t key_count(const unsigned char *keys) {
    size_t count = 0;
    for (int k = 1; k < NX_KEY_COUNT; ++k) if (keys[k]) ++count;
    return count;
}
static uint32_t host_modifiers(const content_input *input, uint32_t mods) {
    for (int k = 1; k < NX_KEY_COUNT; ++k)
        if (input->draining[k]) mods &= ~modifier((nx_key)k);
    return mods;
}
static nx_result admit(content_input *input, const nx_event *events, size_t count) {
    input->busy = 1;
    nx_result result = input->sink(input->user, input->generation, events, count);
    input->busy = 0;
    if (result != NX_OK) {
        input->active = 0;
        input->fenced = 1;
        input->last_error = result;
        for (int k = 1; k < NX_KEY_COUNT; ++k)
            input->draining[k] |= input->physical[k];
        input->draining_buttons |= input->physical_buttons;
    }
    return result;
}
nx_result content_input_create(uint64_t generation, nx_key escape_key,
    uint32_t escape_modifiers, content_input_sink sink, void *user, content_input **out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out = NULL;
    if (!generation || !sink || !key_valid(escape_key) || modifier(escape_key) ||
        (escape_modifiers & ~INPUT_MODS)) return NX_ERROR_ARGUMENT;
    content_input *input = calloc(1, sizeof(*input));
    if (!input) return NX_ERROR_MEMORY;
    input->generation = generation; input->escape_key = escape_key;
    input->escape_modifiers = escape_modifiers; input->sink = sink; input->user = user;
    *out = input;
    return NX_OK;
}
nx_result content_input_release(content_input *input) {
    if (!input) return NX_ERROR_ARGUMENT;
    if (input->busy) return NX_ERROR_STATE;
    if (!input->active) return input->fenced ? input->last_error : NX_OK;
    nx_event batch[NX_KEY_COUNT + 6]; size_t count = 0;
    for (int k = 1; k < NX_KEY_COUNT; ++k) {
        if (input->held[k]) batch[count++] = (nx_event){.type=NX_EVENT_KEY_UP, .key=(nx_key)k};
        input->draining[k] |= input->physical[k];
    }
    for (uint32_t b = 1; b <= NX_BUTTON_X2; b <<= 1)
        if (input->held_buttons & b) batch[count++] = (nx_event){.type=NX_EVENT_POINTER_UP,
            .button=b, .x=input->pointer_x, .y=input->pointer_y};
    input->draining_buttons |= input->physical_buttons;
    batch[count++] = (nx_event){.type=NX_EVENT_FOCUS, .focused=0};
    input->active = 0;
    nx_result result = admit(input, batch, count);
    if (result == NX_OK) { memset(input->held, 0, sizeof(input->held)); input->held_buttons = 0; }
    return result;
}
nx_result content_input_set_target(content_input *input, const content_input_target *target) {
    if (!input || !target) return NX_ERROR_ARGUMENT;
    if (input->busy) return NX_ERROR_STATE;
    nx_rect r = target->bounds;
    if (!isfinite(r.x) || !isfinite(r.y) || !isfinite(r.width) || !isfinite(r.height) ||
        r.width < 0 || r.height < 0 || !isfinite(r.x+r.width) || !isfinite(r.y+r.height) ||
        target->width > INPUT_PIXELS || target->height > INPUT_PIXELS ||
        (target->available && (r.width <= 0 || r.height <= 0 || !target->width || !target->height)))
        return NX_ERROR_ARGUMENT;
    nx_rect old = input->target.bounds;
    int changed = old.x != r.x || old.y != r.y || old.width != r.width || old.height != r.height ||
        input->target.width != target->width || input->target.height != target->height;
    nx_result result = NX_OK;
    if (input->active && (changed || !target->available || !target->focused))
        result = content_input_release(input);
    input->target = *target;
    return result;
}
nx_result content_input_arm(content_input *input) {
    if (!input) return NX_ERROR_ARGUMENT;
    if (input->busy || input->active || input->fenced || !input->window_focused ||
        !input->target.available || !input->target.focused || input->physical_buttons ||
        input->draining_buttons || key_count(input->physical) || key_count(input->draining))
        return NX_ERROR_STATE;
    nx_event event = {.type=NX_EVENT_FOCUS, .focused=1,
        .width=(float)input->target.width, .height=(float)input->target.height};
    nx_result result = admit(input, &event, 1);
    if (result == NX_OK) input->active = 1;
    return result;
}
static int pointer_event(nx_event_type type) {
    return type == NX_EVENT_POINTER_MOVE || type == NX_EVENT_POINTER_DOWN ||
        type == NX_EVENT_POINTER_UP || type == NX_EVENT_WHEEL;
}
static int text_event(nx_event_type type) {
    return type == NX_EVENT_TEXT || type == NX_EVENT_TEXT_COMMAND ||
        type == NX_EVENT_IME_PREEDIT_CHANGED || type == NX_EVENT_IME_DELETE_SURROUNDING ||
        type == NX_EVENT_IME_DELETE_RANGE || type == NX_EVENT_IME_TRANSACTION_BEGIN ||
        type == NX_EVENT_IME_TRANSACTION_END;
}
static float pixel(float value, float start, float size, uint32_t extent) {
    double p = ((double)value - (double)start) * (double)extent / (double)size;
    if (p < 0) p = 0;
    if (p >= (double)extent) p = (double)(extent - 1u);
    return (float)(uint32_t)p;
}
nx_result content_input_event(content_input *input, const nx_event *event,
    nx_event *host_event, int *route_host) {
    if (route_host) *route_host = 0;
    if (!input || !event || !host_event || !route_host) return NX_ERROR_ARGUMENT;
    if (input->busy) return NX_ERROR_STATE;
    nx_event e = *event;
    int key = e.type == NX_EVENT_KEY_DOWN || e.type == NX_EVENT_KEY_DOWN_CONSUMED || e.type == NX_EVENT_KEY_UP;
    int button = e.type == NX_EVENT_POINTER_DOWN || e.type == NX_EVENT_POINTER_UP;
    if ((pointer_event(e.type) && (!isfinite(e.x) || !isfinite(e.y))) ||
        (e.type == NX_EVENT_WHEEL && (!isfinite(e.delta_x) || !isfinite(e.delta_y))) ||
        (button && (!e.button || (e.button & (e.button-1u)) || (e.button & ~INPUT_BUTTONS))) ||
        (key && (e.key < NX_KEY_UNKNOWN || e.key >= NX_KEY_COUNT))) return NX_ERROR_ARGUMENT;
    *host_event = e;
    if (e.type == NX_EVENT_FOCUS) {
        input->window_focused = !!e.focused;
        nx_result result = NX_OK;
        if (!e.focused) {
            result = content_input_release(input);
            /* Native ups may never arrive outside the window. A fresh down is
               required after regain; autorepeat never creates a guest hold. */
            memset(input->physical, 0, sizeof(input->physical));
            memset(input->draining, 0, sizeof(input->draining));
            input->physical_buttons = input->draining_buttons = 0;
        }
        if (result != NX_OK) return result;
        *route_host = 1; return NX_OK;
    }
    if (input->active && ((key && !key_valid(e.key)) ||
        (e.type >= NX_EVENT_TOUCH_DOWN && e.type <= NX_EVENT_TOUCH_CANCEL))) {
        nx_result result = content_input_release(input);
        return result == NX_OK ? NX_ERROR_UNSUPPORTED : result;
    }
    if (e.type == NX_EVENT_POINTER_CANCEL) {
        nx_result result = content_input_release(input);
        if (result != NX_OK) return result;
        input->physical_buttons = input->draining_buttons = 0;
        host_event->modifiers = host_modifiers(input, e.modifiers);
        *route_host = 1; return NX_OK;
    }
    if (input->active && pointer_event(e.type)) {
        nx_rect r = input->target.bounds;
        if (e.x < r.x || e.y < r.y || e.x >= r.x+r.width || e.y >= r.y+r.height) {
            nx_result result = content_input_release(input);
            if (result != NX_OK) return result;
        }
    }
    int suppress = 0, forward = 0;
    nx_event guest = {0}; guest.type = e.type; guest.modifiers = e.modifiers & INPUT_MODS;
    if (key && key_valid(e.key)) {
        int was_down = input->physical[e.key];
        suppress = input->draining[e.key];
        if (e.type == NX_EVENT_KEY_UP) {
            input->physical[e.key] = input->draining[e.key] = 0;
            forward = input->held[e.key] != 0;
        } else {
            /* A repeat without a fresh down after focus gain is not a press. */
            if (!e.repeat) input->physical[e.key] = 1;
            forward = !was_down && !e.repeat && e.type == NX_EVENT_KEY_DOWN;
            if (input->active && e.key == input->escape_key && !e.repeat &&
                (e.modifiers & INPUT_MODS) == input->escape_modifiers) {
                input->draining[e.key] = 1;
                return content_input_release(input);
            }
        }
        guest.key = e.key;
    } else if (button) {
        int was_down = (input->physical_buttons & e.button) != 0;
        suppress = (input->draining_buttons & e.button) != 0;
        if (e.type == NX_EVENT_POINTER_UP) {
            input->physical_buttons &= ~e.button; input->draining_buttons &= ~e.button;
            forward = (input->held_buttons & e.button) != 0;
        } else { input->physical_buttons |= e.button; forward = !was_down; }
        guest.button = e.button;
    } else forward = e.type == NX_EVENT_POINTER_MOVE || e.type == NX_EVENT_WHEEL;
    if (input->active && (key || pointer_event(e.type) || text_event(e.type))) {
        if (!forward || suppress) return NX_OK;
        if (pointer_event(e.type)) {
            nx_rect r = input->target.bounds;
            guest.x = pixel(e.x, r.x, r.width, input->target.width);
            guest.y = pixel(e.y, r.y, r.height, input->target.height);
            guest.delta_x = e.delta_x; guest.delta_y = e.delta_y;
        }
        nx_result result = admit(input, &guest, 1);
        if (result == NX_OK) {
            if (key) input->held[e.key] = (unsigned char)(e.type == NX_EVENT_KEY_DOWN);
            if (button) {
                if (e.type == NX_EVENT_POINTER_DOWN) input->held_buttons |= e.button;
                else input->held_buttons &= ~e.button;
            }
            if (pointer_event(e.type)) { input->pointer_x = guest.x; input->pointer_y = guest.y; }
        }
        return result;
    }
    if (suppress) return NX_OK;
    host_event->modifiers = host_modifiers(input, e.modifiers);
    *route_host = 1;
    return NX_OK;
}
nx_result content_input_rebind(content_input *input, uint64_t generation) {
    if (!input || !generation) return NX_ERROR_ARGUMENT;
    if (input->busy || input->active || generation <= input->generation) return NX_ERROR_STATE;
    input->generation = generation; input->fenced = 0; input->last_error = NX_OK;
    memset(input->held, 0, sizeof(input->held)); input->held_buttons = 0;
    input->target.available = input->target.focused = 0;
    return NX_OK;
}
nx_result content_input_get_status(const content_input *input, content_input_status *out) {
    if (!input || !out) return NX_ERROR_ARGUMENT;
    *out = (content_input_status){input->generation, key_count(input->held), key_count(input->physical),
        key_count(input->draining), input->held_buttons, input->physical_buttons, input->draining_buttons,
        input->active, input->window_focused, input->fenced, input->last_error};
    return NX_OK;
}
nx_result content_input_close(content_input **input) {
    if (!input) return NX_ERROR_ARGUMENT;
    if (!*input) return NX_OK;
    if ((*input)->busy) return NX_ERROR_STATE;
    nx_result result = content_input_release(*input);
    free(*input); *input = NULL;
    return result;
}
