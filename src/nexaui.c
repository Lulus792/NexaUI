#include "internal.h"
#include "edit_shortcuts.h"
#include "secure_zero.h"
#include "context_identity.h"
#include "text_command.h"

#include <stdint.h>
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NX_TOUCH_DRAG_THRESHOLD 8.0

typedef struct nx_event_route {
    uint64_t pointer_id, focus_id;
} nx_event_route;
/* Routing metadata shares the event allocation, but never changes public
   events. sizeof(nx_event) preserves the uint64_t alignment of the tail. */
#define NX_EVENT_SLOT_BYTES (sizeof(nx_event) + sizeof(nx_event_route))
typedef struct nx_event_buffer {
    nx_event *data;
    nx_event_route *routes;
    size_t count, capacity;
} nx_event_buffer;

typedef struct nx_region {
    uint64_t id;
    uint64_t overlay_id;
    uint64_t nav_parent_id;
    unsigned nav_axis; /* 1 vertical menu, 2 horizontal menu bar. */
    nx_rect rect;
    double clip_x0, clip_y0, clip_x1, clip_y1;
    int has_clip;
    uint32_t flags;
    unsigned clicks;
    int pointer_pressed, pointer_released;
    float pointer_x, pointer_y;
    double drag_delta_x, drag_delta_y;
    double wheel_delta_x, wheel_delta_y;
    double touch_delta_x, touch_delta_y;
} nx_region;

#define NX_REGION_INACTIVE 8u

typedef struct nx_clip_bounds {
    double x0, y0, x1, y1;
} nx_clip_bounds;

typedef struct nx_region_buffer {
    nx_region *data;
    size_t count, capacity;
    uint32_t *slots; /* Index plus one; zero is empty. */
    size_t slot_capacity;
} nx_region_buffer;

typedef struct nx_semantic_record {
    uint64_t id, parent_id;
    unsigned depth;
    uint32_t selection_owner, selection_count;
    nx_semantic_role role;
    nx_rect bounds;
    size_t name_offset, name_length;
    size_t description_offset, description_length;
    size_t value_offset, value_length;
    uint32_t state, actions;
    size_t radio_outer_command, radio_inner_command;
    nx_color radio_unchecked_color;
    int has_radio_visual;
    size_t tab_background_command;
    nx_color tab_unselected_color;
    int has_tab_visual;
    size_t tree_background_command;
    nx_color tree_unselected_color;
    int has_tree_visual;
} nx_semantic_record;

static int nx_same_selection_group(const nx_semantic_record *a, const nx_semantic_record *b) {
    return a->selection_owner != UINT32_MAX ? a->selection_owner == b->selection_owner :
        b->selection_owner == UINT32_MAX && a->parent_id == b->parent_id;
}

typedef struct nx_semantic_buffer {
    nx_semantic_record *nodes;
    size_t count, capacity;
    uint32_t *slots;
    size_t slot_capacity;
    char *text;
    size_t text_count, text_capacity;
} nx_semantic_buffer;

typedef struct nx_owned_bitmap {
    struct nx_owned_bitmap *next;
    size_t allocation_size;
    uint8_t pixels[1];
} nx_owned_bitmap;

typedef struct nx_scroll_scope {
    unsigned clip_depth;
    uint64_t parent_id, thumb_x_id, thumb_y_id;
    nx_rect thumb_x_rect, thumb_y_rect;
    nx_color thumb_color;
    double x_fraction, y_fraction;
    int show_x, show_y;
    int disabled;
} nx_scroll_scope;

struct nx_context {
    nx_allocator allocator;
    nx_command *commands;
    size_t command_count;
    size_t command_capacity;
    nx_owned_bitmap *owned_bitmaps;
    uint64_t id_stack[NX_MAX_ID_DEPTH + 1u];
    unsigned id_depth;
    unsigned clip_depth;
    nx_clip_bounds clip_stack[NX_MAX_CLIP_DEPTH];
    unsigned scroll_scope_count;
    nx_scroll_scope scroll_scopes[NX_MAX_CLIP_DEPTH];
    nx_theme base_theme;
    nx_theme theme_stack[NX_MAX_THEME_DEPTH + 1u];
    unsigned theme_depth;
    unsigned state; /* 0 idle, 1 building, 2 finished */
    uint64_t frame_serial;
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
    uint64_t native_identity;
#endif
    float width, height;
    double delta_seconds;
    int reduced_motion;
    float text_raster_scale;
    nx_event_buffer pending_events, frame_events;
    size_t event_initial_capacity;
    size_t region_initial_capacity;
    nx_input_snapshot input;
    unsigned char keys_down[NX_KEY_COUNT];
    unsigned char keys_pressed[NX_KEY_COUNT];
    unsigned char action_keys_pressed[NX_KEY_COUNT];
    unsigned char keys_released[NX_KEY_COUNT];
    nx_region_buffer previous_regions, current_regions;
    uint64_t focused_id, hovered_id, pointer_active_id, key_active_id;
    nx_window *ime_active_window, *ime_candidate_window;
    uint64_t ime_active_id, ime_candidate_id, ime_candidate_revision;
    int ime_active_password, ime_candidate_password;
    uint64_t touch_scroll_id, touch_target_id;
    uint32_t touch_id;
    int touch_active, touch_dragging;
    uint64_t previous_overlay_id, current_overlay_id, building_overlay_id;
    uint64_t overlay_return_focus_id;
    int overlay_built_this_frame;
    float pointer_active_last_x, pointer_active_last_y;
    float touch_start_x, touch_start_y;
    float touch_last_x, touch_last_y;
    nx_key key_active_key;
    int pointer_valid;
    nx_semantic_buffer semantics;
    struct { uint64_t id, owner_id; uint32_t action; } accessible_requests[64];
    struct { uint64_t id, owner_id; uint32_t action; } selection_requests[64];
    size_t selection_request_count;
    size_t accessible_request_count;
    size_t semantic_initial_capacity, semantic_text_initial_capacity;
};

typedef struct nx_field_pointer {
    size_t anchor;
    int active;
    uint64_t tick_serial, id;
} nx_field_pointer;

struct nx_numeric_field {
    nx_text_buffer *buffer;
    double committed;
    size_t history_bytes;
    const nx_context *last_context;
    uint64_t last_frame_serial;
    size_t next_event_index;
    double scroll_x, scroll_y;
    nx_field_pointer pointer;
    int suppress_caret_reveal;
};

struct nx_text_field {
    nx_text_buffer *buffer;
    size_t history_bytes;
    int sensitive;
    const nx_context *last_context;
    uint64_t last_frame_serial;
    size_t next_event_index;
    int submitted;
    double vertical_x;
    int vertical_x_valid;
    double scroll_x, scroll_y;
    nx_text_caret command_caret;
    int command_caret_valid;
    int suppress_caret_reveal;
    nx_field_pointer pointer;
};

static nx_region *nx_region_find(const nx_region_buffer *buffer, uint64_t id);
static nx_region *nx_hit_test(nx_region_buffer *buffer, float x, float y,
                              uint64_t overlay_id);
static nx_region *nx_hit_test_scroll(nx_region_buffer *buffer, float x, float y,
                                     uint64_t overlay_id);
static void nx_process_interaction(nx_context *ctx, const nx_event *event, nx_event_route *route);
static void nx_region_buffer_clear(nx_region_buffer *buffer);
static uint32_t nx_semantic_find_index(const nx_semantic_buffer *buffer, uint64_t id);

static void *nx_default_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_default_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static nx_result nx_reserve_command(nx_context *ctx) {
    if (ctx->command_count < ctx->command_capacity) return NX_OK;
    size_t next = ctx->command_capacity ? ctx->command_capacity * 2u : 64u;
    if (next < ctx->command_capacity || next > SIZE_MAX / sizeof(nx_command))
        return NX_ERROR_LIMIT;
    size_t bytes = next * sizeof(nx_command);
    nx_command *new_commands = (nx_command *)ctx->allocator.allocate(ctx->allocator.user, bytes);
    if (!new_commands) return NX_ERROR_MEMORY;
    if (ctx->commands) {
        memcpy(new_commands, ctx->commands, ctx->command_count * sizeof(nx_command));
        ctx->allocator.deallocate(ctx->allocator.user, ctx->commands,
                                  ctx->command_capacity * sizeof(nx_command));
    }
    ctx->commands = new_commands;
    ctx->command_capacity = next;
    return NX_OK;
}

static void nx_free_owned_bitmaps(nx_context *ctx,
                                  nx_owned_bitmap *stop) {
    while (ctx->owned_bitmaps != stop) {
        nx_owned_bitmap *item = ctx->owned_bitmaps;
        ctx->owned_bitmaps = item->next;
        ctx->allocator.deallocate(ctx->allocator.user, item,
                                  item->allocation_size);
    }
}

nx_result nx_context_create(const nx_context_desc *desc, nx_context **out_context) {
    if (!out_context) return NX_ERROR_ARGUMENT;
    *out_context = NULL;
    nx_allocator a = desc ? desc->allocator : (nx_allocator){0};
    if ((a.allocate == NULL) != (a.deallocate == NULL)) return NX_ERROR_ARGUMENT;
    if (!a.allocate) {
        a.allocate = nx_default_allocate;
        a.deallocate = nx_default_deallocate;
        a.user = NULL;
    }
    size_t capacity = desc && desc->initial_command_capacity
                        ? desc->initial_command_capacity : 64u;
    if (capacity > SIZE_MAX / sizeof(nx_command)) return NX_ERROR_LIMIT;
    size_t event_capacity = desc && desc->initial_event_capacity
                            ? desc->initial_event_capacity : 64u;
    if (event_capacity > SIZE_MAX / NX_EVENT_SLOT_BYTES) return NX_ERROR_LIMIT;
    size_t region_capacity = desc && desc->initial_region_capacity
                             ? desc->initial_region_capacity : 64u;
    if (region_capacity > UINT32_MAX - 1u ||
        region_capacity > SIZE_MAX / sizeof(nx_region)) return NX_ERROR_LIMIT;
    size_t semantic_capacity = desc && desc->initial_semantic_capacity
                               ? desc->initial_semantic_capacity : 64u;
    size_t semantic_text_capacity = desc && desc->initial_semantic_text_capacity
                                    ? desc->initial_semantic_text_capacity : 256u;
    if (semantic_capacity > UINT32_MAX - 1u ||
        semantic_capacity > SIZE_MAX / sizeof(nx_semantic_record)) return NX_ERROR_LIMIT;
    nx_context *ctx = (nx_context *)a.allocate(a.user, sizeof(nx_context));
    if (!ctx) return NX_ERROR_MEMORY;
    memset(ctx, 0, sizeof(*ctx));
    ctx->allocator = a;
    ctx->text_raster_scale = 1;
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
    ctx->native_identity = nx_native_next_identity();
    if (!ctx->native_identity) { a.deallocate(a.user, ctx, sizeof(*ctx)); return NX_ERROR_LIMIT; }
#endif
    (void)nx_theme_preset(NX_THEME_DARK, &ctx->base_theme);
    ctx->theme_stack[0] = ctx->base_theme;
    ctx->commands = (nx_command *)a.allocate(a.user, capacity * sizeof(nx_command));
    if (!ctx->commands) {
        a.deallocate(a.user, ctx, sizeof(nx_context));
        return NX_ERROR_MEMORY;
    }
    ctx->command_capacity = capacity;
    ctx->event_initial_capacity = event_capacity;
    ctx->region_initial_capacity = region_capacity;
    ctx->semantic_initial_capacity = semantic_capacity;
    ctx->semantic_text_initial_capacity = semantic_text_capacity;
    *out_context = ctx;
    return NX_OK;
}

#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
uint64_t nx_context_identity(const nx_context *ctx) { return ctx ? ctx->native_identity : 0; }
uint64_t nx_context_frame_serial(const nx_context *ctx) { return ctx ? ctx->frame_serial : 0; }
#endif

void nx_context_destroy(nx_context *ctx) {
    if (!ctx) return;
    if (ctx->ime_active_window)
        (void)nx_window_set_ime_enabled(ctx->ime_active_window, 0);
    nx_allocator a = ctx->allocator;
    nx_free_owned_bitmaps(ctx, NULL);
    if (ctx->pending_events.data)
        a.deallocate(a.user, ctx->pending_events.data,
                     ctx->pending_events.capacity * NX_EVENT_SLOT_BYTES);
    if (ctx->frame_events.data)
        a.deallocate(a.user, ctx->frame_events.data,
                     ctx->frame_events.capacity * NX_EVENT_SLOT_BYTES);
    nx_region_buffer *buffers[2] = {&ctx->previous_regions, &ctx->current_regions};
    for (unsigned i = 0; i < 2; ++i) {
        nx_region_buffer *buffer = buffers[i];
        if (buffer->data)
            a.deallocate(a.user, buffer->data, buffer->capacity * sizeof(nx_region));
        if (buffer->slots)
            a.deallocate(a.user, buffer->slots,
                         buffer->slot_capacity * sizeof(uint32_t));
    }
    if (ctx->semantics.nodes)
        a.deallocate(a.user, ctx->semantics.nodes,
                     ctx->semantics.capacity * sizeof(nx_semantic_record));
    if (ctx->semantics.slots)
        a.deallocate(a.user, ctx->semantics.slots,
                     ctx->semantics.slot_capacity * sizeof(uint32_t));
    if (ctx->semantics.text)
        a.deallocate(a.user, ctx->semantics.text, ctx->semantics.text_capacity);
    a.deallocate(a.user, ctx->commands, ctx->command_capacity * sizeof(nx_command));
    a.deallocate(a.user, ctx, sizeof(nx_context));
}

nx_result nx_context_set_text_raster_scale(nx_context *ctx, float scale) {
    if (!ctx || !isfinite(scale) || scale < .125f || scale > 8)
        return NX_ERROR_ARGUMENT;
    if (ctx->state == 1u) return NX_ERROR_STATE;
    ctx->text_raster_scale = scale;
    return NX_OK;
}

nx_result nx_context_get_text_raster_scale(const nx_context *ctx, float *out_scale) {
    if (!out_scale) return NX_ERROR_ARGUMENT;
    *out_scale = 0;
    if (!ctx) return NX_ERROR_ARGUMENT;
    *out_scale = ctx->text_raster_scale;
    return NX_OK;
}

nx_result nx_context_set_theme(nx_context *ctx, const nx_theme *theme) {
    if (!ctx) return NX_ERROR_ARGUMENT;
    if (ctx->state == 1u) return NX_ERROR_STATE;
    nx_theme base;
    (void)nx_theme_preset(NX_THEME_DARK, &base);
    if (theme) {
        nx_theme resolved;
        nx_result result = nx_theme_resolve(&base, theme, &resolved);
        if (result != NX_OK) return result;
        base = resolved;
    }
    ctx->base_theme = base;
    ctx->theme_stack[0] = base;
    return NX_OK;
}

nx_result nx_push_theme(nx_context *ctx, const nx_theme *overrides) {
    if (!ctx || !overrides) return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    if (ctx->theme_depth == NX_MAX_THEME_DEPTH) return NX_ERROR_LIMIT;
    nx_theme resolved;
    nx_result result = nx_theme_resolve(&ctx->theme_stack[ctx->theme_depth],
                                        overrides, &resolved);
    if (result != NX_OK) return result;
    ctx->theme_stack[++ctx->theme_depth] = resolved;
    return NX_OK;
}

nx_result nx_pop_theme(nx_context *ctx) {
    if (!ctx || ctx->state != 1u || !ctx->theme_depth)
        return NX_ERROR_STATE;
    --ctx->theme_depth;
    return NX_OK;
}

nx_result nx_theme_color(const nx_context *ctx, nx_color_token token,
                         nx_color *out_color) {
    if (!out_color) return NX_ERROR_ARGUMENT;
    *out_color = (nx_color){0};
    if (!ctx || token < 0 || token >= NX_COLOR_TOKEN_COUNT)
        return NX_ERROR_ARGUMENT;
    const nx_theme *theme = ctx->state == 1u
        ? &ctx->theme_stack[ctx->theme_depth] : &ctx->base_theme;
    *out_color = theme->colors[token];
    return NX_OK;
}

nx_result nx_theme_metric(const nx_context *ctx, nx_metric_token token,
                          float *out_value) {
    if (!out_value) return NX_ERROR_ARGUMENT;
    *out_value = 0;
    if (!ctx || token < 0 || token >= NX_METRIC_TOKEN_COUNT)
        return NX_ERROR_ARGUMENT;
    const nx_theme *theme = ctx->state == 1u
        ? &ctx->theme_stack[ctx->theme_depth] : &ctx->base_theme;
    *out_value = theme->metrics[token];
    return NX_OK;
}

nx_result nx_context_set_reduced_motion(nx_context *ctx, int enabled) {
    if (!ctx || (enabled != 0 && enabled != 1)) return NX_ERROR_ARGUMENT;
    if (ctx->state == 1u) return NX_ERROR_STATE;
    ctx->reduced_motion = enabled;
    return NX_OK;
}

nx_result nx_context_get_reduced_motion(const nx_context *ctx, int *out_enabled) {
    if (!out_enabled) return NX_ERROR_ARGUMENT;
    *out_enabled = 0;
    if (!ctx) return NX_ERROR_ARGUMENT;
    *out_enabled = ctx->reduced_motion;
    return NX_OK;
}

nx_result nx_transition_reset(nx_transition *transition, double value) {
    if (!transition || !isfinite(value)) return NX_ERROR_ARGUMENT;
    *transition = (nx_transition){0};
    transition->value = transition->start = transition->target = value;
    transition->initialized = 1;
    return NX_OK;
}

nx_result nx_transition_update(nx_context *ctx, nx_transition *transition,
                               double target, double duration_seconds,
                               nx_easing easing, double *out_value,
                               int *out_active) {
    if (!ctx || !transition || !out_value || !out_active || !isfinite(target) ||
        !isfinite(duration_seconds) || duration_seconds < 0.0 ||
        (easing != NX_EASING_LINEAR && easing != NX_EASING_SMOOTHSTEP))
        return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;

    nx_transition next = *transition;
    if (!next.initialized) {
        (void)nx_transition_reset(&next, target);
    } else if (next.last_frame_serial != ctx->frame_serial && next.active &&
               !ctx->reduced_motion) {
        double remaining = next.duration - next.elapsed;
        if (ctx->delta_seconds >= remaining) {
            next.elapsed = next.duration;
            next.value = next.target;
            next.active = 0;
        } else {
            next.elapsed += ctx->delta_seconds;
            double t = next.elapsed / next.duration;
            if (next.easing == NX_EASING_SMOOTHSTEP) t = t * t * (3.0 - 2.0 * t);
            next.value = (1.0 - t) * next.start + t * next.target;
        }
    }
    next.last_frame_serial = ctx->frame_serial;
    if (ctx->reduced_motion || duration_seconds == 0.0) {
        next.value = next.start = next.target = target;
        next.elapsed = next.duration = 0.0;
        next.easing = easing;
        next.active = 0;
    } else if (next.target != target ||
               (next.active && (next.duration != duration_seconds ||
                                next.easing != easing))) {
        next.start = next.value;
        next.target = target;
        next.elapsed = 0.0;
        next.duration = duration_seconds;
        next.easing = easing;
        next.active = next.value != target;
    }
    *transition = next;
    *out_value = next.value;
    *out_active = next.active;
    return NX_OK;
}

nx_result nx_begin_frame(nx_context *ctx, float width, float height, double delta_seconds) {
    if (!ctx || !isfinite(width) || !isfinite(height) || !isfinite(delta_seconds) ||
        width <= 0.0f || height <= 0.0f || delta_seconds < 0.0)
        return NX_ERROR_ARGUMENT;
    if (ctx->state == 1u) return NX_ERROR_STATE;
    nx_free_owned_bitmaps(ctx, NULL);
    if (++ctx->frame_serial == 0) ++ctx->frame_serial;
    ctx->state = 1u;
    ctx->ime_candidate_window = NULL;
    ctx->ime_candidate_id = 0;
    ctx->ime_candidate_revision = 0;
    ctx->ime_candidate_password = 0;
    ctx->command_count = 0;
    ctx->semantics.count = 0;
    ctx->semantics.text_count = 0;
    if (ctx->semantics.slots)
        memset(ctx->semantics.slots, 0,
               ctx->semantics.slot_capacity * sizeof(uint32_t));
    ctx->id_depth = 0;
    ctx->clip_depth = 0;
    ctx->scroll_scope_count = 0;
    ctx->current_overlay_id = 0;
    ctx->building_overlay_id = 0;
    ctx->overlay_built_this_frame = 0;
    ctx->theme_depth = 0;
    ctx->theme_stack[0] = ctx->base_theme;
    ctx->id_stack[0] = UINT64_C(14695981039346656037);
    ctx->width = width;
    ctx->height = height;
    ctx->delta_seconds = delta_seconds;
    ctx->input.buttons_pressed = 0;
    ctx->input.buttons_released = 0;
    ctx->input.wheel_x = 0;
    ctx->input.wheel_y = 0;
    memset(ctx->keys_pressed, 0, sizeof(ctx->keys_pressed));
    memset(ctx->action_keys_pressed,0,sizeof(ctx->action_keys_pressed));
    memset(ctx->keys_released, 0, sizeof(ctx->keys_released));
    nx_event_buffer previous = ctx->frame_events;
    ctx->frame_events = ctx->pending_events;
    ctx->pending_events = previous;
    ctx->pending_events.count = 0;
    nx_region_buffer old_regions = ctx->previous_regions;
    ctx->previous_regions = ctx->current_regions;
    ctx->current_regions = old_regions;
    nx_region_buffer_clear(&ctx->current_regions);
    for (size_t i = 0; i < ctx->previous_regions.count; ++i) {
        nx_region *region = &ctx->previous_regions.data[i];
        region->clicks = 0;
        region->pointer_pressed = region->pointer_released = 0;
        region->drag_delta_x = region->drag_delta_y = 0.0;
        region->wheel_delta_x = region->wheel_delta_y = 0.0;
        region->touch_delta_x = region->touch_delta_y = 0.0;
    }
    if (ctx->focused_id && !nx_region_find(&ctx->previous_regions, ctx->focused_id))
        ctx->focused_id = 0;
    if (ctx->pointer_active_id &&
        !nx_region_find(&ctx->previous_regions, ctx->pointer_active_id))
        ctx->pointer_active_id = 0;
    if (ctx->key_active_id && !nx_region_find(&ctx->previous_regions, ctx->key_active_id))
        ctx->key_active_id = 0;
    if (ctx->touch_scroll_id &&
        !nx_region_find(&ctx->previous_regions, ctx->touch_scroll_id))
        ctx->touch_scroll_id = 0;
    if (ctx->touch_target_id &&
        !nx_region_find(&ctx->previous_regions, ctx->touch_target_id))
        ctx->touch_target_id = 0;
    if (ctx->touch_active && !ctx->touch_scroll_id && !ctx->touch_target_id) {
        ctx->touch_active = 0;
        ctx->touch_dragging = 0;
        ctx->touch_id = 0;
    }
    nx_region *hover = ctx->pointer_valid
                       ? nx_hit_test(&ctx->previous_regions, ctx->input.pointer_x,
                                     ctx->input.pointer_y,
                                     ctx->previous_overlay_id) : NULL;
    ctx->hovered_id = hover && !(hover->flags & NX_REGION_DISABLED) ? hover->id : 0;
    for (size_t i = 0; i < ctx->frame_events.count; ++i) {
        const nx_event *event = &ctx->frame_events.data[i];
        switch (event->type) {
        case NX_EVENT_POINTER_MOVE:
        case NX_EVENT_POINTER_DOWN:
        case NX_EVENT_POINTER_UP:
        case NX_EVENT_WHEEL:
            ctx->input.pointer_x = event->x;
            ctx->input.pointer_y = event->y;
            ctx->input.modifiers = event->modifiers;
            if (event->type == NX_EVENT_POINTER_DOWN) {
                if (!(ctx->input.buttons_down & event->button))
                    ctx->input.buttons_pressed |= event->button;
                ctx->input.buttons_down |= event->button;
            } else if (event->type == NX_EVENT_POINTER_UP) {
                if (ctx->input.buttons_down & event->button)
                    ctx->input.buttons_released |= event->button;
                ctx->input.buttons_down &= ~event->button;
            } else if (event->type == NX_EVENT_WHEEL) {
                double x = (double)ctx->input.wheel_x + event->delta_x;
                double y = (double)ctx->input.wheel_y + event->delta_y;
                ctx->input.wheel_x = (float)fmax(-FLT_MAX, fmin(FLT_MAX, x));
                ctx->input.wheel_y = (float)fmax(-FLT_MAX, fmin(FLT_MAX, y));
            }
            break;
        case NX_EVENT_POINTER_CANCEL:
            ctx->input.buttons_released |= ctx->input.buttons_down;
            ctx->input.buttons_down = 0;
            break;
        case NX_EVENT_KEY_DOWN:
        case NX_EVENT_KEY_DOWN_CONSUMED:
            if (!ctx->keys_down[event->key]) {
                ctx->keys_pressed[event->key] = 1;
                if (event->type==NX_EVENT_KEY_DOWN) ctx->action_keys_pressed[event->key]=1;
            }
            ctx->keys_down[event->key] = 1;
            ctx->input.modifiers = event->modifiers;
            break;
        case NX_EVENT_KEY_UP:
            if (ctx->keys_down[event->key]) ctx->keys_released[event->key] = 1;
            ctx->keys_down[event->key] = 0;
            ctx->input.modifiers = event->modifiers;
            break;
        case NX_EVENT_FOCUS:
            ctx->input.focused = event->focused;
            if (!event->focused) {
                ctx->input.buttons_released |= ctx->input.buttons_down;
                ctx->input.buttons_down = 0;
                for (int key = 0; key < NX_KEY_COUNT; ++key) {
                    if (ctx->keys_down[key]) ctx->keys_released[key] = 1;
                    ctx->keys_down[key] = 0;
                }
                ctx->input.modifiers = 0;
            }
            break;
        case NX_EVENT_TEXT:
        case NX_EVENT_TEXT_COMMAND:
        case NX_EVENT_IME_PREEDIT_CHANGED:
        case NX_EVENT_IME_DELETE_SURROUNDING:
        case NX_EVENT_IME_DELETE_RANGE:
        case NX_EVENT_RESIZE:
        case NX_EVENT_WINDOW_STATE:
        case NX_EVENT_TOUCH_DOWN:
        case NX_EVENT_TOUCH_MOVE:
        case NX_EVENT_TOUCH_UP:
        case NX_EVENT_TOUCH_CANCEL:
            break;
        }
        nx_event_route *route = &ctx->frame_events.routes[i];
        *route = (nx_event_route){0};
        if (event->type != NX_EVENT_KEY_DOWN_CONSUMED)
            nx_process_interaction(ctx, event, route);
        route->focus_id = ctx->focused_id;
    }
    ctx->selection_request_count = 0;
    for (size_t i = 0; i < ctx->accessible_request_count; ++i) {
        uint64_t id = ctx->accessible_requests[i].id;
        uint32_t action = ctx->accessible_requests[i].action;
        nx_region *region = nx_region_find(&ctx->previous_regions, id);
        if (!region || (region->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE)) ||
            (ctx->previous_overlay_id && region->overlay_id != ctx->previous_overlay_id)) continue;
        if (ctx->accessible_requests[i].owner_id && action != NX_ACTION_FOCUS && action != NX_ACTION_PRESS) {
            size_t slot = ctx->selection_request_count++;
            ctx->selection_requests[slot].id = id;
            ctx->selection_requests[slot].owner_id = ctx->accessible_requests[i].owner_id;
            ctx->selection_requests[slot].action = action;
        } else if (action == NX_ACTION_FOCUS) {
            if (region->flags & NX_REGION_FOCUSABLE) { ctx->focused_id = id; ctx->key_active_id = 0; }
        } else if (region->clicks != UINT_MAX) ++region->clicks;
    }
    ctx->accessible_request_count = 0;
    return NX_OK;
}

static int nx_valid_event(const nx_event *event) {
    if (!event || (event->modifiers & ~(NX_MOD_SHIFT | NX_MOD_CONTROL |
                                        NX_MOD_ALT | NX_MOD_SUPER))) return 0;
    switch (event->type) {
    case NX_EVENT_POINTER_MOVE:
    case NX_EVENT_POINTER_DOWN:
    case NX_EVENT_POINTER_UP:
    case NX_EVENT_WHEEL:
        if (!isfinite(event->x) || !isfinite(event->y)) return 0;
        if (event->type == NX_EVENT_POINTER_DOWN || event->type == NX_EVENT_POINTER_UP)
            return event->button && !(event->button & (event->button - 1u)) &&
                   (event->button & (NX_BUTTON_LEFT | NX_BUTTON_RIGHT | NX_BUTTON_MIDDLE |
                                     NX_BUTTON_X1 | NX_BUTTON_X2));
        if (event->type == NX_EVENT_WHEEL)
            return isfinite(event->delta_x) && isfinite(event->delta_y);
        return 1;
    case NX_EVENT_POINTER_CANCEL:
        return 1;
    case NX_EVENT_TOUCH_DOWN:
    case NX_EVENT_TOUCH_MOVE:
    case NX_EVENT_TOUCH_UP:
        return isfinite(event->x) && isfinite(event->y);
    case NX_EVENT_TOUCH_CANCEL:
        return 1;
    case NX_EVENT_KEY_DOWN:
    case NX_EVENT_KEY_DOWN_CONSUMED:
    case NX_EVENT_KEY_UP:
        return event->key > NX_KEY_UNKNOWN && event->key < NX_KEY_COUNT &&
               (event->repeat == 0 || event->repeat == 1);
    case NX_EVENT_TEXT:
        return event->scalar <= 0x10ffffu &&
               !(event->scalar >= 0xd800u && event->scalar <= 0xdfffu);
    case NX_EVENT_TEXT_COMMAND:
        return nx_text_command_valid(event->text_command) &&
               ((event->ime_target == 0) == (event->ime_revision == 0)) &&
               (event->repeat == 0 || event->repeat == 1);
    case NX_EVENT_IME_PREEDIT_CHANGED:
        return 1;
    case NX_EVENT_IME_DELETE_SURROUNDING:
        return event->ime_target != 0 && event->ime_revision != 0 &&
               (event->ime_before != 0 || event->ime_after != 0);
    case NX_EVENT_IME_DELETE_RANGE:
        return event->ime_target != 0 && event->ime_revision != 0 &&
               event->ime_length != 0;
    case NX_EVENT_FOCUS:
        return event->focused == 0 || event->focused == 1;
    case NX_EVENT_RESIZE:
        return isfinite(event->width) && isfinite(event->height) &&
               isfinite(event->scale) && event->width >= 0 &&
               event->height >= 0 && event->scale > 0;
    case NX_EVENT_WINDOW_STATE:
        return !(event->window_state & ~(NX_WINDOW_STATE_MINIMIZED |
            NX_WINDOW_STATE_MAXIMIZED | NX_WINDOW_STATE_FULLSCREEN |
            NX_WINDOW_STATE_ACTIVE));
    default:
        return 0;
    }
}

nx_result nx_context_queue_event(nx_context *ctx, const nx_event *event) {
    if (!ctx || !nx_valid_event(event)) return NX_ERROR_ARGUMENT;
    nx_event_buffer *buffer = &ctx->pending_events;
    if (buffer->count == buffer->capacity) {
        size_t next = buffer->capacity ? buffer->capacity * 2u
                                      : ctx->event_initial_capacity;
        if (next < buffer->capacity || next > SIZE_MAX / NX_EVENT_SLOT_BYTES)
            return NX_ERROR_LIMIT;
        nx_event *data = (nx_event *)ctx->allocator.allocate(
            ctx->allocator.user, next * NX_EVENT_SLOT_BYTES);
        if (!data) return NX_ERROR_MEMORY;
        if (buffer->count) memcpy(data, buffer->data, buffer->count * sizeof(nx_event));
        if (buffer->data)
            ctx->allocator.deallocate(ctx->allocator.user, buffer->data,
                                      buffer->capacity * NX_EVENT_SLOT_BYTES);
        buffer->data = data;
        buffer->routes = (nx_event_route *)(data + next);
        buffer->capacity = next;
    }
    buffer->data[buffer->count++] = *event;
    return NX_OK;
}

nx_result nx_frame_events(const nx_context *ctx,
                          const nx_event **out_events, size_t *out_count) {
    if (!out_events || !out_count) return NX_ERROR_ARGUMENT;
    *out_events = NULL;
    *out_count = 0;
    if (!ctx || !ctx->state) return NX_ERROR_STATE;
    *out_events = ctx->frame_events.count ? ctx->frame_events.data : NULL;
    *out_count = ctx->frame_events.count;
    return NX_OK;
}

nx_result nx_frame_input(const nx_context *ctx, nx_input_snapshot *out_input) {
    if (!out_input) return NX_ERROR_ARGUMENT;
    *out_input = (nx_input_snapshot){0};
    if (!ctx || !ctx->state) return NX_ERROR_STATE;
    *out_input = ctx->input;
    return NX_OK;
}

static int nx_valid_key_query(const nx_context *ctx, nx_key key) {
    return ctx && ctx->state && key > NX_KEY_UNKNOWN && key < NX_KEY_COUNT;
}

int nx_key_down(const nx_context *ctx, nx_key key) {
    return nx_valid_key_query(ctx, key) ? ctx->keys_down[key] : 0;
}

int nx_key_pressed(const nx_context *ctx, nx_key key) {
    return nx_valid_key_query(ctx, key) ? ctx->keys_pressed[key] : 0;
}

int nx_key_released(const nx_context *ctx, nx_key key) {
    return nx_valid_key_query(ctx, key) ? ctx->keys_released[key] : 0;
}

static size_t nx_region_slot(uint64_t id, size_t capacity) {
    id ^= id >> 33u;
    id *= UINT64_C(0xff51afd7ed558ccd);
    id ^= id >> 33u;
    return (size_t)id & (capacity - 1u);
}

static nx_region *nx_region_find(const nx_region_buffer *buffer, uint64_t id) {
    if (!buffer->slot_capacity || !id) return NULL;
    size_t slot = nx_region_slot(id, buffer->slot_capacity);
    for (;;) {
        uint32_t entry = buffer->slots[slot];
        if (!entry) return NULL;
        nx_region *region = &buffer->data[entry - 1u];
        if (region->id == id) return region;
        slot = (slot + 1u) & (buffer->slot_capacity - 1u);
    }
}

static void nx_region_buffer_clear(nx_region_buffer *buffer) {
    buffer->count = 0;
    if (buffer->slots)
        memset(buffer->slots, 0, buffer->slot_capacity * sizeof(uint32_t));
}

static nx_result nx_region_reserve(nx_context *ctx, nx_region_buffer *buffer) {
    if (buffer->count == buffer->capacity) {
        size_t next = buffer->capacity ? buffer->capacity * 2u
                                       : ctx->region_initial_capacity;
        if (next < buffer->capacity || next > UINT32_MAX - 1u ||
            next > SIZE_MAX / sizeof(nx_region)) return NX_ERROR_LIMIT;
        nx_region *data = (nx_region *)ctx->allocator.allocate(
            ctx->allocator.user, next * sizeof(nx_region));
        if (!data) return NX_ERROR_MEMORY;
        if (buffer->count)
            memcpy(data, buffer->data, buffer->count * sizeof(nx_region));
        if (buffer->data)
            ctx->allocator.deallocate(ctx->allocator.user, buffer->data,
                                      buffer->capacity * sizeof(nx_region));
        buffer->data = data;
        buffer->capacity = next;
    }
    if (buffer->count + 1u > SIZE_MAX / 2u) return NX_ERROR_LIMIT;
    size_t required = (buffer->count + 1u) * 2u;
    if (buffer->slot_capacity >= required) return NX_OK;
    size_t next = buffer->slot_capacity ? buffer->slot_capacity : 128u;
    while (next < required) {
        if (next > SIZE_MAX / 2u) return NX_ERROR_LIMIT;
        next *= 2u;
    }
    if (next > SIZE_MAX / sizeof(uint32_t)) return NX_ERROR_LIMIT;
    uint32_t *slots = (uint32_t *)ctx->allocator.allocate(
        ctx->allocator.user, next * sizeof(uint32_t));
    if (!slots) return NX_ERROR_MEMORY;
    memset(slots, 0, next * sizeof(uint32_t));
    for (size_t i = 0; i < buffer->count; ++i) {
        size_t slot = nx_region_slot(buffer->data[i].id, next);
        while (slots[slot]) slot = (slot + 1u) & (next - 1u);
        slots[slot] = (uint32_t)i + 1u;
    }
    if (buffer->slots)
        ctx->allocator.deallocate(ctx->allocator.user, buffer->slots,
                                  buffer->slot_capacity * sizeof(uint32_t));
    buffer->slots = slots;
    buffer->slot_capacity = next;
    return NX_OK;
}

static int nx_contains(nx_rect rect, float x, float y) {
    return (double)x >= rect.x && (double)y >= rect.y &&
           (double)x < (double)rect.x + rect.width &&
           (double)y < (double)rect.y + rect.height;
}

static nx_region *nx_hit_test(nx_region_buffer *buffer, float x, float y,
                              uint64_t overlay_id) {
    for (size_t i = buffer->count; i > 0; --i) {
        nx_region *region = &buffer->data[i - 1u];
        if (region->flags & NX_REGION_INACTIVE) continue;
        if (overlay_id && region->overlay_id != overlay_id) continue;
        if (nx_contains(region->rect, x, y) &&
            (!region->has_clip ||
             ((double)x >= region->clip_x0 && (double)x < region->clip_x1 &&
              (double)y >= region->clip_y0 && (double)y < region->clip_y1)))
            return region;
    }
    return NULL;
}

static nx_region *nx_hit_test_scroll(nx_region_buffer *buffer, float x,
                                      float y, uint64_t overlay_id) {
    for (size_t i = buffer->count; i > 0; --i) {
        nx_region *region = &buffer->data[i - 1u];
        if (region->flags & NX_REGION_INACTIVE) continue;
        if (overlay_id && region->overlay_id != overlay_id) continue;
        if (!(region->flags & NX_REGION_SCROLLABLE) ||
            (region->flags & NX_REGION_DISABLED)) continue;
        if (nx_contains(region->rect, x, y) &&
            (!region->has_clip ||
             ((double)x >= region->clip_x0 && (double)x < region->clip_x1 &&
              (double)y >= region->clip_y0 && (double)y < region->clip_y1)))
            return region;
    }
    return NULL;
}

static void nx_focus_advance(nx_context *ctx, int backwards) {
    nx_region_buffer *regions = &ctx->previous_regions;
    size_t count = regions->count;
    if (!count) {
        ctx->focused_id = 0;
        return;
    }
    size_t start = count;
    int has_overlay_child_focus = 0;
    if (ctx->previous_overlay_id)
        for (size_t i = 0; i < count; ++i) {
            const nx_region *region = &regions->data[i];
            if (region->overlay_id == ctx->previous_overlay_id &&
                region->id != ctx->previous_overlay_id &&
                (region->flags & NX_REGION_FOCUSABLE) &&
                !(region->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE))) {
                has_overlay_child_focus = 1;
                break;
            }
        }
    for (size_t i = 0; i < count; ++i)
        if (regions->data[i].id == ctx->focused_id) {
            start = i;
            break;
        }
    for (size_t step = 0; step < count; ++step) {
        size_t index = backwards
            ? (start == count ? count - 1u - step : (start + count - 1u - step) % count)
            : (start == count ? step : (start + 1u + step) % count);
        nx_region *region = &regions->data[index];
        if ((region->flags & NX_REGION_FOCUSABLE) &&
            !(region->flags & NX_REGION_INACTIVE) &&
            !(has_overlay_child_focus &&
              region->id == ctx->previous_overlay_id) &&
            (!ctx->previous_overlay_id ||
             region->overlay_id == ctx->previous_overlay_id) &&
            !(region->flags & NX_REGION_DISABLED)) {
            ctx->focused_id = region->id;
            ctx->key_active_id = 0;
            return;
        }
    }
    ctx->focused_id = 0;
    ctx->key_active_id = 0;
}

static void nx_menu_focus_advance(nx_context *ctx, nx_key key) {
    nx_region_buffer *regions = &ctx->previous_regions;
    nx_region *focused = nx_region_find(regions, ctx->focused_id);
    if (!focused || !focused->nav_axis) return;
    int direction = 0;
    if (key == NX_KEY_HOME) direction = 2;
    else if (key == NX_KEY_END) direction = -2;
    else if ((focused->nav_axis == 1u && key == NX_KEY_DOWN) ||
             (focused->nav_axis == 2u && key == NX_KEY_RIGHT))
        direction = 1;
    else if ((focused->nav_axis == 1u && key == NX_KEY_UP) ||
             (focused->nav_axis == 2u && key == NX_KEY_LEFT))
        direction = -1;
    if (!direction) return;
    size_t count = regions->count;
    size_t start = 0;
    for (; start < count; ++start)
        if (regions->data[start].id == focused->id) break;
    if (start == count) return;
    for (size_t step = 0; step < count; ++step) {
        size_t index;
        if (direction == 2) index = step;
        else if (direction == -2) index = count - 1u - step;
        else if (direction > 0) index = (start + 1u + step) % count;
        else index = (start + count - 1u - step) % count;
        nx_region *candidate = &regions->data[index];
        if (candidate->nav_axis != focused->nav_axis ||
            candidate->nav_parent_id != focused->nav_parent_id ||
            !(candidate->flags & NX_REGION_FOCUSABLE) ||
            (candidate->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE)) ||
            (ctx->previous_overlay_id &&
             candidate->overlay_id != ctx->previous_overlay_id))
            continue;
        ctx->focused_id = candidate->id;
        ctx->key_active_id = 0;
        return;
    }
}

static void nx_mark_click(nx_region *region) {
    if (region && region->clicks < UINT_MAX) ++region->clicks;
}

static void nx_record_pointer_motion(nx_context *ctx, const nx_event *event) {
    nx_region *region = nx_region_find(&ctx->previous_regions,
                                       ctx->pointer_active_id);
    if (region && !(region->flags & NX_REGION_DISABLED)) {
        region->drag_delta_x += (double)event->x - ctx->pointer_active_last_x;
        region->drag_delta_y += (double)event->y - ctx->pointer_active_last_y;
        region->pointer_x = event->x;
        region->pointer_y = event->y;
    }
    ctx->pointer_active_last_x = event->x;
    ctx->pointer_active_last_y = event->y;
}

static void nx_process_interaction(nx_context *ctx, const nx_event *event, nx_event_route *route) {
    nx_region_buffer *regions = &ctx->previous_regions;
    if (event->type == NX_EVENT_TOUCH_DOWN) {
        if (!ctx->touch_active) {
            nx_region *hit = nx_hit_test(regions, event->x, event->y,
                                         ctx->previous_overlay_id);
            nx_region *scroll = nx_hit_test_scroll(regions, event->x,
                                                    event->y,
                                                    ctx->previous_overlay_id);
            if (hit && (hit->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE)))
                hit = NULL;
            if (hit || scroll) {
                ctx->touch_active = 1;
                ctx->touch_dragging = 0;
                ctx->touch_id = event->contact_id;
                ctx->touch_target_id = hit ? hit->id : 0;
                ctx->touch_scroll_id = scroll ? scroll->id : 0;
                ctx->touch_start_x = event->x;
                ctx->touch_start_y = event->y;
                ctx->touch_last_x = event->x;
                ctx->touch_last_y = event->y;
            }
        }
    } else if (event->type == NX_EVENT_TOUCH_MOVE ||
               event->type == NX_EVENT_TOUCH_UP) {
        if (ctx->touch_active && ctx->touch_id == event->contact_id) {
            double dx = (double)event->x - ctx->touch_start_x;
            double dy = (double)event->y - ctx->touch_start_y;
            int was_dragging = ctx->touch_dragging;
            if (dx * dx + dy * dy >=
                NX_TOUCH_DRAG_THRESHOLD * NX_TOUCH_DRAG_THRESHOLD)
                ctx->touch_dragging = 1;
            nx_region *scroll = nx_region_find(regions, ctx->touch_scroll_id);
            if (ctx->touch_dragging && scroll &&
                !(scroll->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE))) {
                scroll->touch_delta_x += (double)event->x -
                    (was_dragging ? ctx->touch_last_x : ctx->touch_start_x);
                scroll->touch_delta_y += (double)event->y -
                    (was_dragging ? ctx->touch_last_y : ctx->touch_start_y);
            }
            ctx->touch_last_x = event->x;
            ctx->touch_last_y = event->y;
            if (event->type == NX_EVENT_TOUCH_UP) {
                nx_region *target = nx_region_find(regions, ctx->touch_target_id);
                nx_region *hit = nx_hit_test(regions, event->x, event->y,
                                             ctx->previous_overlay_id);
                if (!ctx->touch_dragging && target == hit && target &&
                    !(target->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE))) {
                    route->pointer_id = target->id;
                    target->pointer_pressed = 1;
                    target->pointer_released = 1;
                    target->pointer_x = event->x;
                    target->pointer_y = event->y;
                    nx_mark_click(target);
                    ctx->focused_id = (target->flags & NX_REGION_FOCUSABLE) ?
                                      target->id : 0;
                    ctx->key_active_id = 0;
                }
                ctx->touch_active = 0;
                ctx->touch_dragging = 0;
                ctx->touch_id = 0;
                ctx->touch_scroll_id = 0;
                ctx->touch_target_id = 0;
            }
        }
    } else if (event->type == NX_EVENT_TOUCH_CANCEL) {
        if (ctx->touch_active && ctx->touch_id == event->contact_id) {
            ctx->touch_active = 0;
            ctx->touch_dragging = 0;
            ctx->touch_id = 0;
            ctx->touch_scroll_id = 0;
            ctx->touch_target_id = 0;
        }
    } else if (event->type == NX_EVENT_POINTER_MOVE ||
        event->type == NX_EVENT_POINTER_DOWN ||
        event->type == NX_EVENT_POINTER_UP ||
        event->type == NX_EVENT_WHEEL) {
        ctx->pointer_valid = 1;
        nx_region *hit = nx_hit_test(regions, event->x, event->y,
                                    ctx->previous_overlay_id);
        ctx->hovered_id = hit && !(hit->flags & NX_REGION_DISABLED) ? hit->id : 0;
        if (event->type == NX_EVENT_WHEEL) {
            nx_region *scroll = nx_hit_test_scroll(regions, event->x,
                                                    event->y,
                                                    ctx->previous_overlay_id);
            if (scroll) {
                scroll->wheel_delta_x += event->delta_x;
                scroll->wheel_delta_y += event->delta_y;
            }
        } else if (event->type == NX_EVENT_POINTER_DOWN &&
                   event->button == NX_BUTTON_LEFT) {
            ctx->pointer_active_id = hit && !(hit->flags & NX_REGION_DISABLED)
                                     ? hit->id : 0;
            ctx->pointer_active_last_x = event->x;
            ctx->pointer_active_last_y = event->y;
            nx_region *captured = nx_region_find(regions, ctx->pointer_active_id);
            if (captured) {
                route->pointer_id = captured->id;
                captured->pointer_pressed = 1;
                captured->pointer_x = event->x;
                captured->pointer_y = event->y;
            }
            ctx->focused_id = hit && (hit->flags & NX_REGION_FOCUSABLE) &&
                              !(hit->flags & NX_REGION_DISABLED) ? hit->id : 0;
            ctx->key_active_id = 0;
        } else if (event->type == NX_EVENT_POINTER_UP &&
                   event->button == NX_BUTTON_LEFT) {
            nx_region *captured = nx_region_find(regions, ctx->pointer_active_id);
            if (captured) {
                route->pointer_id = captured->id;
                nx_record_pointer_motion(ctx, event);
                captured->pointer_released = 1;
            }
            if (hit && !(hit->flags & NX_REGION_DISABLED) &&
                hit->id == ctx->pointer_active_id) nx_mark_click(hit);
            ctx->pointer_active_id = 0;
        } else if (event->type == NX_EVENT_POINTER_MOVE &&
                   ctx->pointer_active_id) {
            route->pointer_id = ctx->pointer_active_id;
            nx_record_pointer_motion(ctx, event);
        }
    } else if (event->type == NX_EVENT_POINTER_CANCEL) {
        route->pointer_id = ctx->pointer_active_id;
        ctx->pointer_active_id = 0;
    } else if (event->type == NX_EVENT_FOCUS && !event->focused) {
        ctx->pointer_active_id = 0;
        ctx->touch_active = 0;
        ctx->touch_dragging = 0;
        ctx->touch_id = 0;
        ctx->touch_scroll_id = 0;
        ctx->touch_target_id = 0;
        ctx->key_active_id = 0;
    } else if (event->type == NX_EVENT_TEXT_COMMAND &&
               (!event->ime_target || event->ime_target == ctx->focused_id)) {
        uint32_t command=nx_text_command_base(event->text_command);
        if (!event->repeat && (command==NX_TEXT_COMMAND_TAB || command==NX_TEXT_COMMAND_BACKTAB))
            nx_focus_advance(ctx,command==NX_TEXT_COMMAND_BACKTAB);
    } else if (event->type == NX_EVENT_KEY_DOWN) {
        if (event->key == NX_KEY_TAB && !event->repeat) {
            nx_focus_advance(ctx, (event->modifiers & NX_MOD_SHIFT) != 0);
        } else if (!event->repeat &&
                   (event->key == NX_KEY_ENTER || event->key == NX_KEY_SPACE)) {
            nx_region *focused = nx_region_find(regions, ctx->focused_id);
            if (focused && (focused->flags & NX_REGION_FOCUSABLE) &&
                !(focused->flags & NX_REGION_DISABLED)) {
                ctx->key_active_id = focused->id;
                ctx->key_active_key = event->key;
            }
        } else nx_menu_focus_advance(ctx, event->key);
    } else if (event->type == NX_EVENT_KEY_UP &&
               event->key == ctx->key_active_key) {
        if (ctx->focused_id == ctx->key_active_id)
            nx_mark_click(nx_region_find(regions, ctx->focused_id));
        ctx->key_active_id = 0;
        ctx->key_active_key = NX_KEY_UNKNOWN;
    }
}

nx_result nx_interact_region(nx_context *ctx, uint64_t id, nx_rect hit_rect,
                             uint32_t flags, nx_interaction_state *out_state) {
    if (!out_state) return NX_ERROR_ARGUMENT;
    *out_state = (nx_interaction_state){0};
    if (!ctx || !id || !nx_valid_rect(hit_rect) ||
        (flags & ~(NX_REGION_FOCUSABLE | NX_REGION_DISABLED |
                   NX_REGION_SCROLLABLE)))
        return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    nx_region_buffer *current = &ctx->current_regions;
    if (nx_region_find(current, id)) return NX_ERROR_STATE;
    nx_result result = nx_region_reserve(ctx, current);
    if (result != NX_OK) return result;
    size_t index = current->count++;
    nx_region region = {0};
    region.id = id;
    region.overlay_id = ctx->building_overlay_id;
    region.rect = hit_rect;
    region.flags = flags;
    if (ctx->clip_depth) {
        nx_clip_bounds clip = ctx->clip_stack[ctx->clip_depth - 1u];
        region.clip_x0 = clip.x0;
        region.clip_y0 = clip.y0;
        region.clip_x1 = clip.x1;
        region.clip_y1 = clip.y1;
        region.has_clip = 1;
    }
    current->data[index] = region;
    size_t slot = nx_region_slot(id, current->slot_capacity);
    while (current->slots[slot]) slot = (slot + 1u) & (current->slot_capacity - 1u);
    current->slots[slot] = (uint32_t)index + 1u;
    if (!(flags & NX_REGION_DISABLED)) {
        nx_region *previous = nx_region_find(&ctx->previous_regions, id);
        out_state->clicked = previous ? previous->clicks : 0;
        out_state->hovered = ctx->hovered_id == id && ctx->pointer_valid &&
                             nx_contains(hit_rect, ctx->input.pointer_x,
                                         ctx->input.pointer_y) &&
                             (!region.has_clip ||
                              ((double)ctx->input.pointer_x >= region.clip_x0 &&
                               (double)ctx->input.pointer_x < region.clip_x1 &&
                               (double)ctx->input.pointer_y >= region.clip_y0 &&
                               (double)ctx->input.pointer_y < region.clip_y1));
        out_state->held = ctx->pointer_active_id == id || ctx->key_active_id == id;
        out_state->focused = ctx->focused_id == id;
        if (previous) {
            out_state->pointer_pressed = previous->pointer_pressed;
            out_state->pointer_released = previous->pointer_released;
            out_state->pointer_x = previous->pointer_x;
            out_state->pointer_y = previous->pointer_y;
            out_state->drag_delta_x = (float)fmax(-FLT_MAX,
                fmin(FLT_MAX, previous->drag_delta_x));
            out_state->drag_delta_y = (float)fmax(-FLT_MAX,
                fmin(FLT_MAX, previous->drag_delta_y));
            if (flags & NX_REGION_SCROLLABLE) {
                out_state->wheel_x = (float)fmax(-FLT_MAX,
                    fmin(FLT_MAX, previous->wheel_delta_x));
                out_state->wheel_y = (float)fmax(-FLT_MAX,
                    fmin(FLT_MAX, previous->wheel_delta_y));
            }
        }
    }
    return NX_OK;
}

nx_result nx_focus_set(nx_context *ctx, uint64_t id) {
    if (!ctx) return NX_ERROR_ARGUMENT;
    if (!id) {
        ctx->focused_id = 0;
        ctx->key_active_id = 0;
        return NX_OK;
    }
    nx_region *region = nx_region_find(&ctx->current_regions, id);
    if (!region) region = nx_region_find(&ctx->previous_regions, id);
    if (!region || !(region->flags & NX_REGION_FOCUSABLE) ||
        (region->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE)))
        return NX_ERROR_ARGUMENT;
    uint64_t active_overlay = ctx->building_overlay_id ?
        ctx->building_overlay_id : ctx->previous_overlay_id;
    if (active_overlay && region->overlay_id != active_overlay)
        return NX_ERROR_ARGUMENT;
    ctx->focused_id = id;
    ctx->key_active_id = 0;
    return NX_OK;
}

uint64_t nx_focus_id(const nx_context *ctx) {
    return ctx ? ctx->focused_id : 0;
}

static uint32_t nx_semantic_find_index(const nx_semantic_buffer *buffer, uint64_t id) {
    if (!buffer->slot_capacity || !id) return UINT32_MAX;
    size_t slot = nx_region_slot(id, buffer->slot_capacity);
    for (;;) {
        uint32_t entry = buffer->slots[slot];
        if (!entry) return UINT32_MAX;
        if (buffer->nodes[entry - 1u].id == id) return entry - 1u;
        slot = (slot + 1u) & (buffer->slot_capacity - 1u);
    }
}

static int nx_valid_span(nx_utf8_span span) {
    if (!span.data && span.length) return 0;
    if (!span.length) return 1;
    return !memchr(span.data, 0, span.length) &&
           nx_utf8_validate(span.data, span.length, NULL) == NX_OK;
}

static int nx_semantic_requires_name(nx_semantic_role role) {
    switch (role) {
    case NX_ROLE_BUTTON:
    case NX_ROLE_CHECKBOX:
    case NX_ROLE_RADIO:
    case NX_ROLE_SWITCH:
    case NX_ROLE_SLIDER:
    case NX_ROLE_TEXT_FIELD:
    case NX_ROLE_SCROLL_AREA:
    case NX_ROLE_COLUMN_HEADER:
    case NX_ROLE_POPUP:
    case NX_ROLE_DIALOG:
    case NX_ROLE_TOOLTIP:
    case NX_ROLE_MENU_BAR:
    case NX_ROLE_TAB:
    case NX_ROLE_MENU_ITEM:
    case NX_ROLE_TREE_ITEM:
        return 1;
    default:
        return 0;
    }
}

static nx_result nx_semantic_reserve_node(nx_context *ctx) {
    nx_semantic_buffer *buffer = &ctx->semantics;
    if (buffer->count == buffer->capacity) {
        size_t next = buffer->capacity ? buffer->capacity * 2u
                                       : ctx->semantic_initial_capacity;
        if (next < buffer->capacity || next > UINT32_MAX - 1u ||
            next > SIZE_MAX / sizeof(nx_semantic_record)) return NX_ERROR_LIMIT;
        nx_semantic_record *nodes = (nx_semantic_record *)ctx->allocator.allocate(
            ctx->allocator.user, next * sizeof(nx_semantic_record));
        if (!nodes) return NX_ERROR_MEMORY;
        if (buffer->count)
            memcpy(nodes, buffer->nodes, buffer->count * sizeof(nx_semantic_record));
        if (buffer->nodes)
            ctx->allocator.deallocate(ctx->allocator.user, buffer->nodes,
                                      buffer->capacity * sizeof(nx_semantic_record));
        buffer->nodes = nodes;
        buffer->capacity = next;
    }
    if (buffer->count + 1u > SIZE_MAX / 2u) return NX_ERROR_LIMIT;
    size_t required = (buffer->count + 1u) * 2u;
    if (buffer->slot_capacity >= required) return NX_OK;
    size_t next = buffer->slot_capacity ? buffer->slot_capacity : 128u;
    while (next < required) {
        if (next > SIZE_MAX / 2u) return NX_ERROR_LIMIT;
        next *= 2u;
    }
    if (next > SIZE_MAX / sizeof(uint32_t)) return NX_ERROR_LIMIT;
    uint32_t *slots = (uint32_t *)ctx->allocator.allocate(
        ctx->allocator.user, next * sizeof(uint32_t));
    if (!slots) return NX_ERROR_MEMORY;
    memset(slots, 0, next * sizeof(uint32_t));
    for (size_t i = 0; i < buffer->count; ++i) {
        size_t slot = nx_region_slot(buffer->nodes[i].id, next);
        while (slots[slot]) slot = (slot + 1u) & (next - 1u);
        slots[slot] = (uint32_t)i + 1u;
    }
    if (buffer->slots)
        ctx->allocator.deallocate(ctx->allocator.user, buffer->slots,
                                  buffer->slot_capacity * sizeof(uint32_t));
    buffer->slots = slots;
    buffer->slot_capacity = next;
    return NX_OK;
}

static nx_result nx_semantic_reserve_text(nx_context *ctx, size_t extra) {
    nx_semantic_buffer *buffer = &ctx->semantics;
    if (extra > SIZE_MAX - buffer->text_count) return NX_ERROR_LIMIT;
    size_t required = buffer->text_count + extra;
    if (required <= buffer->text_capacity) return NX_OK;
    size_t next = buffer->text_capacity ? buffer->text_capacity
                                        : ctx->semantic_text_initial_capacity;
    while (next < required) {
        if (next > SIZE_MAX / 2u) return NX_ERROR_LIMIT;
        next *= 2u;
    }
    char *text = (char *)ctx->allocator.allocate(ctx->allocator.user, next);
    if (!text) return NX_ERROR_MEMORY;
    if (buffer->text_count)
        memcpy(text, buffer->text, buffer->text_count);
    if (buffer->text)
        ctx->allocator.deallocate(ctx->allocator.user, buffer->text,
                                  buffer->text_capacity);
    buffer->text = text;
    buffer->text_capacity = next;
    return NX_OK;
}

static size_t nx_semantic_copy_span(nx_semantic_buffer *buffer, nx_utf8_span span) {
    if (!span.length) return 0;
    size_t offset = buffer->text_count;
    memcpy(buffer->text + offset, span.data, span.length);
    buffer->text[offset + span.length] = 0;
    buffer->text_count += span.length + 1u;
    return offset;
}

static nx_result nx_semantics_add_impl(nx_context *ctx, const nx_semantic_desc *desc,
                                        uint32_t *out_index, int selection) {
    if (out_index) *out_index = UINT32_MAX;
    if (!ctx || !desc || !desc->id ||
        desc->role < NX_ROLE_WINDOW || desc->role > NX_ROLE_TAB_LIST ||
        !nx_valid_rect(desc->bounds) || !nx_valid_span(desc->name) ||
        !nx_valid_span(desc->description) || !nx_valid_span(desc->value) ||
        (nx_semantic_requires_name(desc->role) && !desc->name.length) ||
        (desc->state & ~(NX_SEM_DISABLED | NX_SEM_CHECKED | NX_SEM_MIXED |
                         NX_SEM_EXPANDED | NX_SEM_SELECTED | NX_SEM_READONLY |
                         NX_SEM_REQUIRED | NX_SEM_INVALID | NX_SEM_BUSY |
                         NX_SEM_PROTECTED | (selection ? NX_SEM_SELECTION_SINGLE | NX_SEM_SELECTION_REQUIRED : 0u))) ||
        ((desc->state & (NX_SEM_CHECKED | NX_SEM_MIXED)) ==
         (NX_SEM_CHECKED | NX_SEM_MIXED)) ||
        (desc->actions & ~(NX_ACTION_PRESS | NX_ACTION_FOCUS |
                           NX_ACTION_INCREMENT | NX_ACTION_DECREMENT |
                           NX_ACTION_SET_VALUE | NX_ACTION_EXPAND |
                           NX_ACTION_COLLAPSE | NX_ACTION_SELECT)))
        return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    nx_semantic_buffer *buffer = &ctx->semantics;
    if (nx_semantic_find_index(buffer, desc->id) != UINT32_MAX) return NX_ERROR_STATE;
    unsigned depth = 0;
    uint32_t owner = UINT32_MAX;
    if (desc->parent_id) {
        uint32_t parent = nx_semantic_find_index(buffer, desc->parent_id);
        if (parent == UINT32_MAX) return NX_ERROR_ARGUMENT;
        depth = buffer->nodes[parent].depth + 1u;
        owner = (buffer->nodes[parent].state & NX_SEM_SELECTION_SINGLE) ? parent : buffer->nodes[parent].selection_owner;
        if (depth >= NX_SEMANTIC_MAX_DEPTH) return NX_ERROR_LIMIT;
    }
    nx_utf8_span spans[3] = {desc->name, desc->description, desc->value};
    size_t extra = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (!spans[i].length) continue;
        if (spans[i].length >= SIZE_MAX - extra) return NX_ERROR_LIMIT;
        extra += spans[i].length + 1u;
    }
    nx_result result = nx_semantic_reserve_node(ctx);
    if (result != NX_OK) return result;
    result = nx_semantic_reserve_text(ctx, extra);
    if (result != NX_OK) return result;
    uint32_t index = (uint32_t)buffer->count++;
    nx_semantic_record *node = &buffer->nodes[index];
    *node = (nx_semantic_record){0};
    node->id = desc->id;
    node->parent_id = desc->parent_id;
    node->depth = depth;
    node->selection_owner = owner;
    node->role = desc->role;
    node->bounds = desc->bounds;
    node->state = desc->state;
    node->actions = desc->actions;
    node->name_offset = nx_semantic_copy_span(buffer, desc->name);
    node->name_length = desc->name.length;
    node->description_offset = nx_semantic_copy_span(buffer, desc->description);
    node->description_length = desc->description.length;
    node->value_offset = nx_semantic_copy_span(buffer, desc->value);
    node->value_length = desc->value.length;
    size_t slot = nx_region_slot(node->id, buffer->slot_capacity);
    while (buffer->slots[slot]) slot = (slot + 1u) & (buffer->slot_capacity - 1u);
    buffer->slots[slot] = index + 1u;
    if (out_index) *out_index = index;
    return NX_OK;
}

nx_result nx_semantics_add(nx_context *ctx, const nx_semantic_desc *desc, uint32_t *out_index) {
    return nx_semantics_add_impl(ctx, desc, out_index, 0);
}

nx_result nx_selection_apply(nx_context *ctx, uint64_t group_id, int required, uint64_t *selected_id) {
    if (!ctx || !group_id || !selected_id || (required != 0 && required != 1)) return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    uint64_t next = *selected_id;
    for (size_t i = 0; i < ctx->selection_request_count; ++i) {
        if (ctx->selection_requests[i].owner_id != group_id) continue;
        uint64_t id = ctx->selection_requests[i].id;
        uint32_t action = ctx->selection_requests[i].action;
        if (action == NX_ACTION_SELECT || (action == NX_ACTION_ADD_SELECTION && (!next || next == id))) next = id;
        else if (action == NX_ACTION_DESELECT && !required && next == id) next = 0;
        ctx->selection_requests[i].owner_id = 0;
    }
    *selected_id = next;
    return NX_OK;
}

nx_result nx_selection_group(nx_context *ctx, const nx_semantic_desc *desc, int required, uint64_t *selected_id) {
    if (!ctx || !desc || !selected_id || (required != 0 && required != 1) || desc->actions ||
        (desc->state & (NX_SEM_SELECTION_SINGLE | NX_SEM_SELECTION_REQUIRED)) ||
        (desc->role == NX_ROLE_TAB_LIST && !required) ||
        (desc->role != NX_ROLE_GROUP && desc->role != NX_ROLE_LIST && desc->role != NX_ROLE_TREE && desc->role != NX_ROLE_TAB_LIST)) return NX_ERROR_ARGUMENT;
    nx_semantic_desc group = *desc;
    group.state |= NX_SEM_SELECTION_SINGLE | (required ? NX_SEM_SELECTION_REQUIRED : 0u);
    nx_result result = nx_semantics_add_impl(ctx, &group, NULL, 1);
    if (result != NX_OK) return result;
    return nx_selection_apply(ctx, desc->id, required, selected_id);
}

static void nx_semantics_rollback_last(nx_context *ctx, uint64_t id,
                                        size_t old_text_count) {
    nx_semantic_buffer *buffer = &ctx->semantics;
    size_t slot = nx_region_slot(id, buffer->slot_capacity);
    while (buffer->slots[slot]) {
        uint32_t entry = buffer->slots[slot] - 1u;
        if (buffer->nodes[entry].id == id) {
            buffer->slots[slot] = 0;
            break;
        }
        slot = (slot + 1u) & (buffer->slot_capacity - 1u);
    }
    --buffer->count;
    buffer->text_count = old_text_count;
}

static void nx_region_rollback_last(nx_context *ctx, uint64_t id) {
    nx_region_buffer *buffer = &ctx->current_regions;
    size_t slot = nx_region_slot(id, buffer->slot_capacity);
    while (buffer->slots[slot]) {
        uint32_t entry = buffer->slots[slot] - 1u;
        if (buffer->data[entry].id == id) {
            buffer->slots[slot] = 0;
            break;
        }
        slot = (slot + 1u) & (buffer->slot_capacity - 1u);
    }
    --buffer->count;
}

nx_result nx_widget_register(nx_context *ctx, const nx_widget_desc *desc,
                             nx_interaction_state *out_state) {
    if (!out_state) return NX_ERROR_ARGUMENT;
    *out_state = (nx_interaction_state){0};
    if (!ctx || !desc ||
        (desc->flags & ~(NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE |
                         NX_WIDGET_SCROLLABLE)) ||
        ((desc->flags & NX_WIDGET_FOCUSABLE) &&
         !(desc->flags & NX_WIDGET_INTERACTIVE)) ||
        ((desc->flags & NX_WIDGET_SCROLLABLE) &&
         !(desc->flags & NX_WIDGET_INTERACTIVE)) ||
        ((desc->semantic.actions & NX_ACTION_FOCUS) != 0) !=
            ((desc->flags & NX_WIDGET_FOCUSABLE) != 0) ||
        (desc->semantic.actions && !(desc->flags & NX_WIDGET_INTERACTIVE)))
        return NX_ERROR_ARGUMENT;
    size_t old_text_count = ctx->semantics.text_count;
    nx_result result = nx_semantics_add(ctx, &desc->semantic, NULL);
    if (result != NX_OK) return result;
    if (!(desc->flags & NX_WIDGET_INTERACTIVE)) return NX_OK;
    uint32_t region_flags = 0;
    if (desc->flags & NX_WIDGET_FOCUSABLE) region_flags |= NX_REGION_FOCUSABLE;
    if (desc->flags & NX_WIDGET_SCROLLABLE) region_flags |= NX_REGION_SCROLLABLE;
    if (desc->semantic.state & NX_SEM_DISABLED) region_flags |= NX_REGION_DISABLED;
    result = nx_interact_region(ctx, desc->semantic.id, desc->semantic.bounds,
                                region_flags, out_state);
    if (result != NX_OK)
        nx_semantics_rollback_last(ctx, desc->semantic.id, old_text_count);
    return result;
}

nx_result nx_semantics_count(const nx_context *ctx, size_t *out_count) {
    if (!out_count) return NX_ERROR_ARGUMENT;
    *out_count = 0;
    if (!ctx || ctx->state != 2u) return NX_ERROR_STATE;
    *out_count = ctx->semantics.count;
    return NX_OK;
}

nx_result nx_semantics_get(const nx_context *ctx, uint32_t index,
                           nx_semantic_view *out_view) {
    if (!out_view) return NX_ERROR_ARGUMENT;
    *out_view = (nx_semantic_view){0};
    if (!ctx || ctx->state != 2u) return NX_ERROR_STATE;
    if (index >= ctx->semantics.count) return NX_ERROR_ARGUMENT;
    const nx_semantic_record *node = &ctx->semantics.nodes[index];
    nx_semantic_view *view = out_view;
    view->id = node->id;
    view->parent_id = node->parent_id;
    view->depth = node->depth;
    view->role = node->role;
    view->bounds = node->bounds;
    view->state = node->state | (ctx->focused_id == node->id ? NX_SEM_FOCUSED : 0u);
    view->actions = node->actions;
    if ((node->actions & NX_ACTION_SELECT) && node->selection_owner != UINT32_MAX) {
        view->actions |= NX_ACTION_ADD_SELECTION;
        if (!(ctx->semantics.nodes[node->selection_owner].state & NX_SEM_SELECTION_REQUIRED)) view->actions |= NX_ACTION_DESELECT;
    }
    const nx_region *region = nx_region_find(&ctx->current_regions, node->id);
    if (region && (region->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE) ||
        (ctx->current_overlay_id && region->overlay_id != ctx->current_overlay_id))) view->state |= NX_SEM_DISABLED;
    view->name = (nx_utf8_span){node->name_length
                 ? ctx->semantics.text + node->name_offset : NULL, node->name_length};
    view->description = (nx_utf8_span){node->description_length
                        ? ctx->semantics.text + node->description_offset : NULL,
                        node->description_length};
    view->value = (nx_utf8_span){node->value_length
                  ? ctx->semantics.text + node->value_offset : NULL, node->value_length};
    return NX_OK;
}

nx_result nx_semantics_request_action(nx_context *ctx, uint64_t id, uint32_t action) {
    if (!ctx || !id) return NX_ERROR_ARGUMENT;
    if (ctx->state != 2u) return NX_ERROR_STATE;
    if (action != NX_ACTION_PRESS && action != NX_ACTION_FOCUS && action != NX_ACTION_SELECT &&
        action != NX_ACTION_ADD_SELECTION && action != NX_ACTION_DESELECT)
        return NX_ERROR_UNSUPPORTED;
    uint32_t index = nx_semantic_find_index(&ctx->semantics, id);
    if (index == UINT32_MAX) return NX_ERROR_STATE;
    const nx_semantic_record *node = &ctx->semantics.nodes[index];
    const nx_region *region = nx_region_find(&ctx->current_regions, id);
    if (!region || (node->state & NX_SEM_DISABLED) ||
        (region->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE)) ||
        (ctx->current_overlay_id && region->overlay_id != ctx->current_overlay_id)) return NX_ERROR_STATE;
    const nx_semantic_record *owner = node->selection_owner == UINT32_MAX ? NULL : &ctx->semantics.nodes[node->selection_owner];
    uint32_t supported = node->actions;
    if (owner && (supported & NX_ACTION_SELECT)) {
        supported |= NX_ACTION_ADD_SELECTION;
        if (!(owner->state & NX_SEM_SELECTION_REQUIRED)) supported |= NX_ACTION_DESELECT;
        if (owner->state & NX_SEM_DISABLED) return NX_ERROR_STATE;
    }
    if (!(supported & action) || (action == NX_ACTION_FOCUS && !(region->flags & NX_REGION_FOCUSABLE)))
        return NX_ERROR_UNSUPPORTED;
    for (size_t i = 0; i < ctx->accessible_request_count; ++i)
        if (ctx->accessible_requests[i].id == id && ctx->accessible_requests[i].action == action) return NX_ERROR_LIMIT;
    if (ctx->accessible_request_count == 64u) return NX_ERROR_LIMIT;
    ctx->accessible_requests[ctx->accessible_request_count].owner_id = owner ? owner->id : 0;
    ctx->accessible_requests[ctx->accessible_request_count].id = id;
    ctx->accessible_requests[ctx->accessible_request_count++].action = action;
    return NX_OK;
}

nx_result nx_semantics_find(const nx_context *ctx, uint64_t id,
                            uint32_t *out_index) {
    if (!out_index) return NX_ERROR_ARGUMENT;
    *out_index = UINT32_MAX;
    if (!ctx || ctx->state != 2u) return NX_ERROR_STATE;
    if (!id) return NX_ERROR_ARGUMENT;
    uint32_t index = nx_semantic_find_index(&ctx->semantics, id);
    if (index == UINT32_MAX) return NX_ERROR_ARGUMENT;
    *out_index = index;
    return NX_OK;
}

nx_result nx_end_frame(nx_context *ctx) {
    if (!ctx || ctx->state != 1u) return NX_ERROR_STATE;
    if (ctx->id_depth || ctx->clip_depth || ctx->theme_depth ||
        ctx->scroll_scope_count || ctx->building_overlay_id)
        return NX_ERROR_STATE;
    for (size_t i = 0; i < ctx->semantics.count; ++i) ctx->semantics.nodes[i].selection_count = 0;
    for (size_t i = 0; i < ctx->semantics.count; ++i) {
        nx_semantic_record *n = &ctx->semantics.nodes[i];
        if (n->selection_owner != UINT32_MAX && (n->actions & NX_ACTION_SELECT) &&
            (n->state & (n->role == NX_ROLE_RADIO ? NX_SEM_CHECKED : NX_SEM_SELECTED))) {
            if (++ctx->semantics.nodes[n->selection_owner].selection_count > 1u) return NX_ERROR_STATE;
        }
    }
    for (size_t i = 0; i < ctx->semantics.count; ++i)
        if ((ctx->semantics.nodes[i].state & NX_SEM_SELECTION_REQUIRED) && !ctx->semantics.nodes[i].selection_count) return NX_ERROR_STATE;
    if (ctx->current_overlay_id) {
        nx_region *focused_overlay = nx_region_find(&ctx->current_regions,
                                                     ctx->focused_id);
        if (!focused_overlay ||
            focused_overlay->overlay_id != ctx->current_overlay_id ||
            !(focused_overlay->flags & NX_REGION_FOCUSABLE) ||
            (focused_overlay->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE))) {
            ctx->focused_id = ctx->current_overlay_id;
            for (size_t i = 0; i < ctx->current_regions.count; ++i) {
                nx_region *candidate = &ctx->current_regions.data[i];
                if (candidate->overlay_id == ctx->current_overlay_id &&
                    candidate->id != ctx->current_overlay_id &&
                    (candidate->flags & NX_REGION_FOCUSABLE) &&
                    !(candidate->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE))) {
                    ctx->focused_id = candidate->id;
                    break;
                }
            }
        }
    } else if (ctx->previous_overlay_id || ctx->overlay_built_this_frame) {
        ctx->focused_id = ctx->overlay_return_focus_id;
        ctx->overlay_return_focus_id = 0;
        if (ctx->overlay_built_this_frame)
            for (size_t i = 0; i < ctx->current_regions.count; ++i)
                if (ctx->current_regions.data[i].overlay_id)
                    ctx->current_regions.data[i].flags |= NX_REGION_INACTIVE;
    }
    nx_region *focused = nx_region_find(&ctx->current_regions, ctx->focused_id);
    if (!focused || !(focused->flags & NX_REGION_FOCUSABLE) ||
        (focused->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE)))
        ctx->focused_id = 0;
    nx_region *pointer = nx_region_find(&ctx->current_regions, ctx->pointer_active_id);
    if (!pointer || (pointer->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE)) ||
        (ctx->current_overlay_id &&
         pointer->overlay_id != ctx->current_overlay_id))
        ctx->pointer_active_id = 0;
    nx_region *touch = nx_region_find(&ctx->current_regions, ctx->touch_scroll_id);
    if (ctx->touch_scroll_id &&
        (!touch || (touch->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE)) ||
         (ctx->current_overlay_id &&
          touch->overlay_id != ctx->current_overlay_id)))
        ctx->touch_scroll_id = 0;
    touch = nx_region_find(&ctx->current_regions, ctx->touch_target_id);
    if (ctx->touch_target_id &&
        (!touch || (touch->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE)) ||
         (ctx->current_overlay_id &&
          touch->overlay_id != ctx->current_overlay_id)))
        ctx->touch_target_id = 0;
    if (ctx->touch_active && !ctx->touch_scroll_id && !ctx->touch_target_id) {
        ctx->touch_active = 0;
        ctx->touch_dragging = 0;
        ctx->touch_id = 0;
    }
    nx_region *key = nx_region_find(&ctx->current_regions, ctx->key_active_id);
    if (!key || (key->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE)) ||
        (ctx->current_overlay_id && key->overlay_id != ctx->current_overlay_id))
        ctx->key_active_id = 0;
    ctx->previous_overlay_id = ctx->current_overlay_id;
    nx_window *desired_ime_window =
        ctx->focused_id == ctx->ime_candidate_id ?
        ctx->ime_candidate_window : NULL;
    if (ctx->ime_active_window != desired_ime_window ||
        (desired_ime_window && ctx->ime_active_password !=
                               ctx->ime_candidate_password)) {
        if (ctx->ime_active_window)
            (void)nx_window_set_ime_enabled(ctx->ime_active_window, 0);
        ctx->ime_active_window = NULL;
        ctx->ime_active_password = 0;
        if (desired_ime_window &&
            nx_window_set_ime_purpose(desired_ime_window,
                ctx->ime_candidate_password ? NX_IME_PURPOSE_PASSWORD :
                                              NX_IME_PURPOSE_NORMAL) == NX_OK) {
            if (ctx->ime_candidate_password)
                (void)nx_window_set_ime_surrounding_text(desired_ime_window,
                    ctx->ime_candidate_id, ctx->ime_candidate_revision,
                    "", 0, 0, 0);
            if (nx_window_set_ime_enabled(desired_ime_window, 1) == NX_OK)
                ctx->ime_active_window = desired_ime_window;
        }
        if (ctx->ime_active_window)
            ctx->ime_active_password = ctx->ime_candidate_password;
    }
    ctx->ime_active_id = ctx->ime_active_window ? ctx->ime_candidate_id : 0;
    ctx->state = 2u;
    return NX_OK;
}

uint64_t nx_id(nx_context *ctx, const char *label, size_t length) {
    if (!ctx || ctx->state != 1u || (!label && length)) return 0;
    uint64_t hash = ctx->id_stack[ctx->id_depth];
    /* The terminator and length distinguish a scope path from concatenation. */
    hash = (hash ^ UINT64_C(255)) * UINT64_C(1099511628211);
    for (size_t i = 0; i < length; ++i)
        hash = (hash ^ (unsigned char)label[i]) * UINT64_C(1099511628211);
    for (unsigned i = 0; i < sizeof(size_t); ++i) {
        hash = (hash ^ (unsigned char)(length & 255u)) * UINT64_C(1099511628211);
        length >>= 8u;
    }
    return hash ? hash : 1u;
}

nx_result nx_push_id(nx_context *ctx, const char *label, size_t length) {
    if (!ctx || ctx->state != 1u || (!label && length)) return NX_ERROR_ARGUMENT;
    if (ctx->id_depth == NX_MAX_ID_DEPTH) return NX_ERROR_LIMIT;
    uint64_t id = nx_id(ctx, label, length);
    ctx->id_stack[++ctx->id_depth] = id;
    return NX_OK;
}

nx_result nx_pop_id(nx_context *ctx) {
    if (!ctx || ctx->state != 1u || !ctx->id_depth) return NX_ERROR_STATE;
    --ctx->id_depth;
    return NX_OK;
}

nx_result nx_fill_rect(nx_context *ctx, nx_rect rect, nx_color color) {
    if (!ctx || !nx_valid_rect(rect) || !nx_valid_color(color)) return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    nx_result result = nx_reserve_command(ctx);
    if (result != NX_OK) return result;
    ctx->commands[ctx->command_count++] = (nx_command){NX_COMMAND_FILL_RECT, rect, color};
    return NX_OK;
}

static int nx_valid_point(nx_point point) {
    return isfinite(point.x) && isfinite(point.y);
}

nx_result nx_stroke_rect(nx_context *ctx, nx_rect rect,
                         float thickness, nx_color color) {
    if (!ctx || !nx_valid_rect(rect) || !isfinite(thickness) ||
        thickness <= 0.0f || !nx_valid_color(color)) return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    nx_result result = nx_reserve_command(ctx);
    if (result != NX_OK) return result;
    nx_command command = {0};
    command.type = NX_COMMAND_STROKE_RECT;
    command.rect = rect;
    command.color = color;
    command.thickness = thickness;
    ctx->commands[ctx->command_count++] = command;
    return NX_OK;
}

nx_result nx_fill_round_rect(nx_context *ctx, nx_rect rect,
                             float radius, nx_color color) {
    if (!ctx || !nx_valid_rect(rect) || !isfinite(radius) ||
        radius < 0.0f || !nx_valid_color(color)) return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    nx_result result = nx_reserve_command(ctx);
    if (result != NX_OK) return result;
    nx_command command = {0};
    command.type = NX_COMMAND_FILL_ROUND_RECT;
    command.rect = rect;
    command.color = color;
    command.radius = radius;
    ctx->commands[ctx->command_count++] = command;
    return NX_OK;
}

nx_result nx_draw_line(nx_context *ctx, nx_point a, nx_point b,
                       float thickness, nx_color color) {
    if (!ctx || !nx_valid_point(a) || !nx_valid_point(b) ||
        !isfinite(thickness) || thickness <= 0.0f ||
        !nx_valid_color(color)) return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    nx_result result = nx_reserve_command(ctx);
    if (result != NX_OK) return result;
    nx_command command = {0};
    command.type = NX_COMMAND_LINE;
    command.color = color;
    command.thickness = thickness;
    command.points[0] = a;
    command.points[1] = b;
    ctx->commands[ctx->command_count++] = command;
    return NX_OK;
}

nx_result nx_fill_triangle(nx_context *ctx, nx_point a, nx_point b,
                           nx_point c, nx_color color) {
    if (!ctx || !nx_valid_point(a) || !nx_valid_point(b) ||
        !nx_valid_point(c) || !nx_valid_color(color)) return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    nx_result result = nx_reserve_command(ctx);
    if (result != NX_OK) return result;
    nx_command command = {0};
    command.type = NX_COMMAND_FILL_TRIANGLE;
    command.color = color;
    command.points[0] = a;
    command.points[1] = b;
    command.points[2] = c;
    ctx->commands[ctx->command_count++] = command;
    return NX_OK;
}

nx_result nx_draw_bitmap(nx_context *ctx, nx_rect dest,
                         nx_bitmap_view bitmap, nx_color tint) {
    if (!ctx || !nx_valid_rect(dest) || !nx_valid_bitmap_view(bitmap) ||
        !nx_valid_color(tint)) return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    nx_result result = nx_reserve_command(ctx);
    if (result != NX_OK) return result;
    nx_command command = {0};
    command.type = NX_COMMAND_BITMAP;
    command.rect = dest;
    command.color = tint;
    command.bitmap = bitmap;
    ctx->commands[ctx->command_count++] = command;
    return NX_OK;
}

static size_t nx_text_style_index(const nx_text_style_span *spans,
                                  size_t count, size_t byte_start) {
    size_t first = 0, last = count;
    while (first < last) {
        size_t middle = first + (last - first) / 2u;
        if (spans[middle].byte_end <= byte_start) first = middle + 1u;
        else last = middle;
    }
    return first;
}

static nx_result nx_text_bitmap_piece(nx_context *ctx,nx_rect dest,
    nx_bitmap_view bitmap,nx_color tint,double x0,double y0,double x1,double y1) {
    if (x1<=x0 || y1<=y0) return NX_OK;
    nx_result result=nx_push_clip(ctx,(nx_rect){(float)x0,(float)y0,(float)(x1-x0),(float)(y1-y0)});
    if (result!=NX_OK) return result;
    result=nx_draw_bitmap(ctx,dest,bitmap,tint);
    nx_result popped=nx_pop_clip(ctx);
    return result==NX_OK?popped:result;
}

/* Selection rectangles are sorted by line then x and merged by the layout.
   Split only boundary masks into disjoint clips: no double antialias blending,
   no rerasterization and no complete-text redraw for every selected line. */
static nx_result nx_text_selected_bitmap(nx_context *ctx,nx_rect dest,
    nx_bitmap_view bitmap,nx_color normal,nx_color selected,nx_point origin,
    size_t line,const nx_text_selection_rect *rects,size_t count) {
    double x0=dest.x,y0=dest.y,x1=x0+dest.width,y1=y0+dest.height;
    size_t lo=0,hi=count;
    while (lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        const nx_text_selection_rect *r=&rects[mid];
        if (r->line_index<line || (r->line_index==line && origin.x+r->x+r->width<=x0)) lo=mid+1;
        else hi=mid;
    }
    if (lo==count || rects[lo].line_index!=line || origin.x+rects[lo].x>=x1)
        return nx_draw_bitmap(ctx,dest,bitmap,normal);
    double top=fmax(y0,origin.y+rects[lo].y),bottom=fmin(y1,origin.y+rects[lo].y+rects[lo].height);
    if (bottom<=top) return nx_draw_bitmap(ctx,dest,bitmap,normal);
    if (origin.x+rects[lo].x<=x0 && origin.x+rects[lo].x+rects[lo].width>=x1 && top==y0 && bottom==y1)
        return nx_draw_bitmap(ctx,dest,bitmap,selected);
    nx_result result=nx_text_bitmap_piece(ctx,dest,bitmap,normal,x0,y0,x1,top);
    if (result==NX_OK) result=nx_text_bitmap_piece(ctx,dest,bitmap,normal,x0,bottom,x1,y1);
    double edge=x0;
    for (size_t i=lo;result==NX_OK && i<count && rects[i].line_index==line;++i) {
        double start=fmax(x0,origin.x+rects[i].x),end=fmin(x1,origin.x+rects[i].x+rects[i].width);
        if (start>=x1) break;
        if (end<=edge) continue;
        if (start>edge) result=nx_text_bitmap_piece(ctx,dest,bitmap,normal,edge,top,start,bottom);
        if (result==NX_OK) result=nx_text_bitmap_piece(ctx,dest,bitmap,selected,fmax(edge,start),top,end,bottom);
        edge=end;
    }
    if (result==NX_OK) result=nx_text_bitmap_piece(ctx,dest,bitmap,normal,edge,top,x1,bottom);
    return result;
}

static nx_result nx_draw_text_layout_impl(nx_context *ctx,
                                          const nx_text_layout *layout,
                                          nx_point origin, nx_color tint,
                                          const nx_text_style_span *spans,
                                          size_t span_count,
                                          size_t cache_bytes,
                                          const nx_text_selection_rect *selection,
                                          size_t selection_count,nx_color selection_tint,
                                          double raster_scale,double coordinate_scale) {
    if (!ctx || !layout || !isfinite(origin.x) || !isfinite(origin.y) ||
        !nx_valid_color(tint) || !cache_bytes || !isfinite(raster_scale) ||
        raster_scale < .125 || raster_scale > 8)
        return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    double ppem = nx_text_layout_pixels_per_em(layout);
    double raster_ppem=ppem*raster_scale;
    if (ppem < .125 || ppem > 16384.0 || !isfinite(raster_ppem) ||
        raster_ppem < .125 || raster_ppem > 16384.0)
        return NX_ERROR_ARGUMENT;
    nx_glyph_cache *caches[NX_FONT_COLLECTION_MAX_FONTS] = {0};
    size_t first_command = ctx->command_count;
    unsigned first_clip_depth=ctx->clip_depth;
    nx_owned_bitmap *first_bitmap = ctx->owned_bitmaps;
    nx_result result = NX_OK;
    for (size_t i = 0; i < nx_text_layout_glyph_count(layout); ++i) {
        nx_text_glyph glyph;
        result = nx_text_layout_glyph_at(layout, i, &glyph);
        if (result != NX_OK) break;
        uint32_t font_index = glyph.shape.font_index;
        if (!glyph.shape.font || font_index >= NX_FONT_COLLECTION_MAX_FONTS) {
            result = NX_ERROR_STATE;
            break;
        }
        if (!caches[font_index]) {
            result = nx_glyph_cache_create(glyph.shape.font, cache_bytes,
                                           &ctx->allocator,
                                           &caches[font_index]);
            if (result != NX_OK) break;
        }
        nx_glyph_mask_view mask;
        result = nx_glyph_cache_get_fractional(caches[font_index],
                                    glyph.shape.glyph_index,
                                    raster_ppem, &mask);
        if (result != NX_OK) break;
        if (!mask.width || !mask.height) continue;
        if (mask.width > SIZE_MAX / mask.height ||
            (size_t)mask.width * mask.height >
                SIZE_MAX - sizeof(nx_owned_bitmap) + 1u) {
            result = NX_ERROR_LIMIT;
            break;
        }
        size_t pixels_size = (size_t)mask.width * mask.height;
        size_t allocation_size = sizeof(nx_owned_bitmap) + pixels_size - 1u;
        nx_owned_bitmap *owned = ctx->allocator.allocate(
            ctx->allocator.user, allocation_size);
        if (!owned) { result = NX_ERROR_MEMORY; break; }
        owned->next = ctx->owned_bitmaps;
        owned->allocation_size = allocation_size;
        ctx->owned_bitmaps = owned;
        for (uint32_t y = 0; y < mask.height; ++y)
            memcpy(owned->pixels + (size_t)y * mask.width,
                   mask.pixels + (size_t)y * mask.stride_bytes,
                   mask.width);
        double units=coordinate_scale/raster_scale;
        double x = (double)origin.x + (glyph.x + glyph.shape.offset_x)*coordinate_scale +
                   mask.x_offset*units;
        double y = (double)origin.y + (glyph.baseline_y -
                   glyph.shape.offset_y)*coordinate_scale + mask.y_offset*units;
        if(raster_scale==1&&coordinate_scale==1){
            x=(double)origin.x+glyph.x+glyph.shape.offset_x+mask.x_offset;
            y=(double)origin.y+glyph.baseline_y-glyph.shape.offset_y+mask.y_offset;
        }
        if (!isfinite(x) || !isfinite(y) || x < -FLT_MAX ||
            x > FLT_MAX || y < -FLT_MAX || y > FLT_MAX) {
            result = NX_ERROR_LIMIT;
            break;
        }
        nx_rect dest = {(float)x, (float)y,
                        (float)(mask.width*units), (float)(mask.height*units)};
        nx_bitmap_view bitmap = {owned->pixels, mask.width, mask.height,
                                 mask.width, NX_BITMAP_A8};
        nx_color glyph_tint = tint;
        if (span_count) {
            size_t style = nx_text_style_index(spans, span_count,
                                               glyph.shape.byte_start);
            if (style < span_count &&
                spans[style].byte_start <= glyph.shape.byte_start &&
                glyph.shape.byte_end <= spans[style].byte_end &&
                (spans[style].flags & NX_TEXT_STYLE_FOREGROUND))
                glyph_tint = spans[style].foreground;
        }
        result = selection_count?nx_text_selected_bitmap(ctx,dest,bitmap,glyph_tint,selection_tint,
            origin,glyph.line_index,selection,selection_count):nx_draw_bitmap(ctx, dest, bitmap, glyph_tint);
        if (result != NX_OK) break;
    }
    for (size_t i = 0; i < NX_FONT_COLLECTION_MAX_FONTS; ++i)
        nx_glyph_cache_destroy(caches[i]);
    if (result != NX_OK) {
        ctx->command_count = first_command;
        ctx->clip_depth=first_clip_depth;
        nx_free_owned_bitmaps(ctx, first_bitmap);
    }
    return result;
}

nx_result nx_draw_text_layout(nx_context *ctx,
                              const nx_text_layout *layout,
                              nx_point origin, nx_color tint,
                              size_t cache_bytes) {
    return nx_draw_text_layout_impl(ctx, layout, origin, tint,
                                    NULL, 0, cache_bytes,NULL,0,tint,ctx?ctx->text_raster_scale:1,1);
}

nx_result nx_draw_text_layout_scaled(nx_context *ctx,const nx_text_layout *layout,
    nx_point origin,nx_color tint,float raster_scale,size_t cache_bytes) {
    return nx_draw_text_layout_impl(ctx,layout,origin,tint,
        NULL,0,cache_bytes,NULL,0,tint,raster_scale,1);
}

nx_result nx_draw_text_layout_pixels(nx_context *ctx,const nx_text_layout *layout,
    float raster_scale,size_t cache_bytes) {
    nx_color white={1,1,1,1};
    return nx_draw_text_layout_impl(ctx,layout,(nx_point){0},white,
        NULL,0,cache_bytes,NULL,0,white,raster_scale,raster_scale);
}

static nx_result nx_rich_fill(nx_context *ctx, double x, double y,
                              double width, double height, nx_color color) {
    if (!isfinite(x) || !isfinite(y) || !isfinite(width) ||
        !isfinite(height) || fabs(x) > FLT_MAX || fabs(y) > FLT_MAX ||
        width > FLT_MAX || height > FLT_MAX)
        return NX_ERROR_LIMIT;
    nx_rect rect = {(float)x, (float)y, (float)width, (float)height};
    return nx_fill_rect(ctx, rect, color);
}

static nx_result nx_rich_geometry(nx_context *ctx,
                                  const nx_text_layout *layout,
                                  nx_point origin, nx_color base_tint,
                                  const nx_text_style_span *span,
                                  int background_pass) {
    uint32_t relevant = background_pass ? NX_TEXT_STYLE_BACKGROUND :
                        (NX_TEXT_STYLE_UNDERLINE | NX_TEXT_STYLE_STRIKE);
    if (!(span->flags & relevant)) return NX_OK;
    size_t count = 0;
    nx_result result = nx_text_layout_selection(layout, span->byte_start,
                                                  span->byte_end,
                                                  NULL, 0, &count);
    if (result == NX_OK && !count) return NX_OK;
    if (result != NX_ERROR_LIMIT) return result;
    if (count > SIZE_MAX / sizeof(nx_text_selection_rect)) return NX_ERROR_LIMIT;
    size_t allocation_size = count * sizeof(nx_text_selection_rect);
    nx_text_selection_rect *rects = ctx->allocator.allocate(ctx->allocator.user,
                                                             allocation_size);
    if (!rects) return NX_ERROR_MEMORY;
    result = nx_text_layout_selection(layout, span->byte_start,
                                      span->byte_end, rects, count, &count);
    if (result == NX_OK) {
        double ppem = nx_text_layout_pixels_per_em(layout);
        double thickness = fmax(1.0, ppem / 16.0);
        nx_color decoration = (span->flags & NX_TEXT_STYLE_FOREGROUND) ?
                               span->foreground : base_tint;
        for (size_t i = 0; i < count && result == NX_OK; ++i) {
            nx_text_selection_rect *rect = &rects[i];
            double x = (double)origin.x + rect->x;
            if (background_pass) {
                result = nx_rich_fill(ctx, x, (double)origin.y + rect->y,
                                      rect->width, rect->height,
                                      span->background);
                continue;
            }
            nx_text_line line;
            result = nx_text_layout_line_at(layout, rect->line_index, &line);
            if (result != NX_OK) break;
            if (span->flags & NX_TEXT_STYLE_UNDERLINE)
                result = nx_rich_fill(ctx, x,
                    (double)origin.y + line.baseline_y + ppem * 0.08,
                    rect->width, thickness, decoration);
            if (result == NX_OK && (span->flags & NX_TEXT_STYLE_STRIKE))
                result = nx_rich_fill(ctx, x,
                    (double)origin.y + line.baseline_y - ppem * 0.3,
                    rect->width, thickness, decoration);
        }
    }
    ctx->allocator.deallocate(ctx->allocator.user, rects, allocation_size);
    return result;
}

nx_result nx_draw_rich_text_layout(nx_context *ctx,
                                   const nx_text_layout *layout,
                                   nx_point origin, nx_color base_tint,
                                   const nx_text_style_span *spans,
                                   size_t span_count, size_t cache_bytes) {
    if (!ctx || !layout || !isfinite(origin.x) || !isfinite(origin.y) ||
        !nx_valid_color(base_tint) || (!spans && span_count) || !cache_bytes)
        return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    nx_utf8_span source;
    nx_result result = nx_text_layout_text(layout, &source);
    if (result != NX_OK) return result;
    size_t prior_end = 0;
    for (size_t i = 0; i < span_count; ++i) {
        const nx_text_style_span *span = &spans[i];
        if (span->byte_start >= span->byte_end ||
            span->byte_end > source.length ||
            (i && span->byte_start < prior_end) || !span->flags ||
            (span->flags & ~(NX_TEXT_STYLE_FOREGROUND |
                             NX_TEXT_STYLE_BACKGROUND |
                             NX_TEXT_STYLE_UNDERLINE | NX_TEXT_STYLE_STRIKE)) ||
            !nx_text_layout_is_grapheme_boundary(layout, span->byte_start) ||
            !nx_text_layout_is_grapheme_boundary(layout, span->byte_end) ||
            ((span->flags & NX_TEXT_STYLE_FOREGROUND) &&
             !nx_valid_color(span->foreground)) ||
            ((span->flags & NX_TEXT_STYLE_BACKGROUND) &&
             !nx_valid_color(span->background)))
            return NX_ERROR_ARGUMENT;
        prior_end = span->byte_end;
    }
    for (size_t i = 0; i < nx_text_layout_glyph_count(layout); ++i) {
        nx_text_glyph glyph;
        result = nx_text_layout_glyph_at(layout, i, &glyph);
        if (result != NX_OK) return result;
        size_t style = nx_text_style_index(spans, span_count,
                                           glyph.shape.byte_start);
        if (style < span_count) {
            const nx_text_style_span *span = &spans[style];
            if ((span->byte_start > glyph.shape.byte_start &&
                 span->byte_start < glyph.shape.byte_end) ||
                (span->byte_end > glyph.shape.byte_start &&
                 span->byte_end < glyph.shape.byte_end))
                return NX_ERROR_ARGUMENT;
        }
    }
    size_t first_command = ctx->command_count;
    nx_owned_bitmap *first_bitmap = ctx->owned_bitmaps;
    for (size_t i = 0; i < span_count && result == NX_OK; ++i)
        result = nx_rich_geometry(ctx, layout, origin, base_tint,
                                  &spans[i], 1);
    if (result == NX_OK)
        result = nx_draw_text_layout_impl(ctx, layout, origin, base_tint,
                                          spans, span_count, cache_bytes,NULL,0,base_tint,ctx->text_raster_scale,1);
    for (size_t i = 0; i < span_count && result == NX_OK; ++i)
        result = nx_rich_geometry(ctx, layout, origin, base_tint,
                                  &spans[i], 0);
    if (result != NX_OK) {
        ctx->command_count = first_command;
        nx_free_owned_bitmaps(ctx, first_bitmap);
    }
    return result;
}

static nx_result nx_display_register(nx_context *ctx, uint64_t id,
                                     uint64_t parent_id, nx_semantic_role role,
                                     nx_rect bounds, nx_utf8_span name) {
    nx_widget_desc widget = {0};
    widget.semantic.id = id;
    widget.semantic.parent_id = parent_id;
    widget.semantic.role = role;
    widget.semantic.bounds = bounds;
    widget.semantic.name = name;
    nx_interaction_state state;
    return nx_widget_register(ctx, &widget, &state);
}

nx_result nx_label(nx_context *ctx, uint64_t id, uint64_t parent_id,
                   nx_rect bounds, const nx_text_layout *layout,
                   nx_point origin, nx_color tint, size_t cache_bytes) {
    nx_utf8_span text;
    nx_result result = nx_text_layout_text(layout, &text);
    if (result != NX_OK) return result;
    if (!ctx) return NX_ERROR_ARGUMENT;
    size_t old_text_count = ctx->semantics.text_count;
    result = nx_display_register(ctx, id, parent_id, NX_ROLE_TEXT, bounds, text);
    if (result != NX_OK) return result;
    result = nx_draw_text_layout(ctx, layout, origin, tint, cache_bytes);
    if (result != NX_OK) nx_semantics_rollback_last(ctx, id, old_text_count);
    return result;
}

nx_result nx_rich_label(nx_context *ctx, uint64_t id, uint64_t parent_id,
                        nx_rect bounds, const nx_text_layout *layout,
                        nx_point origin, nx_color base_tint,
                        const nx_text_style_span *spans, size_t span_count,
                        size_t cache_bytes) {
    nx_utf8_span text;
    nx_result result = nx_text_layout_text(layout, &text);
    if (result != NX_OK) return result;
    if (!ctx) return NX_ERROR_ARGUMENT;
    size_t old_text_count = ctx->semantics.text_count;
    result = nx_display_register(ctx, id, parent_id, NX_ROLE_TEXT, bounds, text);
    if (result != NX_OK) return result;
    result = nx_draw_rich_text_layout(ctx, layout, origin, base_tint,
                                      spans, span_count, cache_bytes);
    if (result != NX_OK) nx_semantics_rollback_last(ctx, id, old_text_count);
    return result;
}

nx_result nx_separator(nx_context *ctx, uint64_t id, uint64_t parent_id,
                       nx_rect bounds, nx_color color) {
    if (!ctx) return NX_ERROR_ARGUMENT;
    size_t old_text_count = ctx->semantics.text_count;
    nx_result result = nx_display_register(ctx, id, parent_id,
                                           NX_ROLE_SEPARATOR, bounds,
                                           (nx_utf8_span){0});
    if (result != NX_OK) return result;
    result = nx_fill_rect(ctx, bounds, color);
    if (result != NX_OK) nx_semantics_rollback_last(ctx, id, old_text_count);
    return result;
}

nx_result nx_image(nx_context *ctx, uint64_t id, uint64_t parent_id,
                   nx_rect bounds, nx_utf8_span name,
                   nx_bitmap_view bitmap, nx_color tint) {
    if (!ctx || !name.length) return NX_ERROR_ARGUMENT;
    size_t old_text_count = ctx->semantics.text_count;
    nx_result result = nx_display_register(ctx, id, parent_id,
                                           NX_ROLE_IMAGE, bounds, name);
    if (result != NX_OK) return result;
    result = nx_draw_bitmap(ctx, bounds, bitmap, tint);
    if (result != NX_OK) nx_semantics_rollback_last(ctx, id, old_text_count);
    return result;
}

static nx_result nx_control_paint(nx_context *ctx,
                                  const nx_control_desc *desc,
                                  nx_semantic_role role, int checked,
                                  nx_interaction_state state) {
    nx_color accent, surface, border, hover, pressed, disabled, focus, text_color;
    nx_color_token label_token = desc->disabled ? NX_COLOR_DISABLED_TEXT :
        role == NX_ROLE_BUTTON && !state.held && !state.hovered ?
        NX_COLOR_ACCENT_TEXT : NX_COLOR_TEXT;
    float radius, focus_width;
    nx_result result;
    if (nx_theme_color(ctx, NX_COLOR_ACCENT, &accent) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_SURFACE_RAISED, &surface) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_BORDER, &border) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_HOVER, &hover) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_PRESSED, &pressed) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_DISABLED_BACKGROUND, &disabled) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_FOCUS, &focus) != NX_OK ||
        nx_theme_color(ctx, label_token, &text_color) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_RADIUS_MD, &radius) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_FOCUS_WIDTH, &focus_width) != NX_OK)
        return NX_ERROR_STATE;
    nx_rect bounds = desc->bounds;
    nx_color base = desc->disabled ? disabled :
                    state.held ? pressed : state.hovered ? hover : accent;
    if (role == NX_ROLE_BUTTON) {
        result = nx_fill_round_rect(ctx, bounds, radius, base);
    } else {
        float side = fminf(bounds.width, bounds.height);
        nx_rect mark = {bounds.x, bounds.y, side, side};
        if (role == NX_ROLE_SWITCH) {
            mark.width = fminf(bounds.width, side * 1.8f);
            result = nx_fill_round_rect(ctx, mark, side * 0.5f,
                                        desc->disabled ? disabled :
                                        checked ? accent : surface);
            if (result != NX_OK) return result;
            float thumb = side * 0.7f;
            double position = checked ? 1.0 : 0.0;
            if (desc->switch_transition) {
                int active;
                nx_easing easing = desc->switch_easing ? desc->switch_easing :
                                   NX_EASING_SMOOTHSTEP;
                result = nx_transition_update(ctx, desc->switch_transition,
                                              position, desc->switch_duration_seconds,
                                              easing, &position, &active);
                if (result != NX_OK) return result;
            }
            position = fmax(0.0, fmin(1.0, position));
            nx_rect knob = {mark.x + (float)(position * (mark.width - thumb)),
                            mark.y + (side - thumb) * 0.5f, thumb, thumb};
            result = nx_fill_round_rect(ctx, knob, thumb * 0.5f, text_color);
        } else {
            nx_color box = desc->disabled ? disabled :
                           checked ? accent : surface;
            if (role == NX_ROLE_CHECKBOX) {
                float inset = fminf(2.0f, side * 0.1f);
                result = nx_fill_round_rect(ctx, mark,
                                            fminf(radius, side * 0.25f), border);
                if (result != NX_OK) return result;
                mark.x += inset;
                mark.y += inset;
                mark.width -= 2.0f * inset;
                mark.height -= 2.0f * inset;
            }
            result = nx_fill_round_rect(ctx, mark,
                                        role == NX_ROLE_RADIO ? side * 0.5f :
                                        fminf(radius, side * 0.25f), box);
            if (result != NX_OK) return result;
            if (checked || role == NX_ROLE_RADIO) {
                float inset = side * 0.3f;
                nx_rect inner = {mark.x + inset, mark.y + inset,
                                 side - 2.0f * inset, side - 2.0f * inset};
                result = nx_fill_round_rect(ctx, inner,
                                            role == NX_ROLE_RADIO ?
                                            inner.width * 0.5f : 0.0f,
                                            checked ? text_color :
                                            (nx_color){0});
            }
        }
    }
    if (result != NX_OK) return result;
    if (state.focused) {
        result = nx_stroke_rect(ctx, bounds, focus_width, focus);
        if (result != NX_OK) return result;
    }
    if (desc->label_layout)
        return nx_draw_text_layout(ctx, desc->label_layout,
                                   desc->label_origin, text_color,
                                   desc->glyph_cache_bytes);
    return NX_OK;
}

static nx_result nx_control_run(nx_context *ctx,
                                const nx_control_desc *desc,
                                nx_semantic_role role, int checked,
                                int *out_checked,
                                nx_interaction_state *out_state) {
    if (!out_state) return NX_ERROR_ARGUMENT;
    *out_state = (nx_interaction_state){0};
    if (!ctx || !desc || !out_checked || !desc->name.length ||
        (desc->label_layout && !desc->glyph_cache_bytes))
        return NX_ERROR_ARGUMENT;
    if (role == NX_ROLE_SWITCH && desc->switch_transition &&
        (!isfinite(desc->switch_duration_seconds) ||
         desc->switch_duration_seconds < 0.0 ||
         (desc->switch_easing && desc->switch_easing != NX_EASING_LINEAR &&
          desc->switch_easing != NX_EASING_SMOOTHSTEP)))
        return NX_ERROR_ARGUMENT;
    nx_color radio_unchecked_color = {0};
    if (role == NX_ROLE_RADIO) {
        nx_result color_result = nx_theme_color(ctx, desc->disabled ?
                                                NX_COLOR_DISABLED_BACKGROUND :
                                                NX_COLOR_SURFACE_RAISED,
                                                &radio_unchecked_color);
        if (color_result != NX_OK) return color_result;
    }
    nx_widget_desc widget = {0};
    widget.semantic.id = desc->id;
    widget.semantic.parent_id = desc->parent_id;
    widget.semantic.role = role;
    widget.semantic.bounds = desc->bounds;
    widget.semantic.name = desc->name;
    widget.semantic.state = (desc->disabled ? NX_SEM_DISABLED : 0u) |
                            (checked ? NX_SEM_CHECKED : 0u);
    widget.semantic.actions = NX_ACTION_FOCUS |
        (role == NX_ROLE_RADIO ? NX_ACTION_SELECT : NX_ACTION_PRESS);
    widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    size_t old_text_count = ctx->semantics.text_count;
    size_t old_command_count = ctx->command_count;
    nx_interaction_state state;
    nx_result result = nx_widget_register(ctx, &widget, &state);
    if (result != NX_OK) return result;
    int next_checked = checked;
    if (role == NX_ROLE_CHECKBOX || role == NX_ROLE_SWITCH)
        next_checked ^= (int)(state.clicked & 1u);
    else if (role == NX_ROLE_RADIO && state.clicked)
        next_checked = 1;
    nx_transition prior_transition = {0};
    if (role == NX_ROLE_SWITCH && desc->switch_transition)
        prior_transition = *desc->switch_transition;
    result = nx_control_paint(ctx, desc, role, next_checked, state);
    if (result != NX_OK) {
        if (role == NX_ROLE_SWITCH && desc->switch_transition)
            *desc->switch_transition = prior_transition;
        ctx->command_count = old_command_count;
        nx_region_rollback_last(ctx, desc->id);
        nx_semantics_rollback_last(ctx, desc->id, old_text_count);
        return result;
    }
    nx_semantic_record *node = &ctx->semantics.nodes[ctx->semantics.count - 1u];
    if (next_checked) node->state |= NX_SEM_CHECKED;
    else node->state &= ~NX_SEM_CHECKED;
    if (role == NX_ROLE_RADIO) {
        node->has_radio_visual = 1;
        node->radio_outer_command = old_command_count;
        node->radio_inner_command = old_command_count + 1u;
        node->radio_unchecked_color = radio_unchecked_color;
    }
    *out_checked = next_checked;
    *out_state = state;
    return NX_OK;
}

nx_result nx_button(nx_context *ctx, const nx_control_desc *desc,
                    nx_interaction_state *out_state) {
    int unused = 0;
    return nx_control_run(ctx, desc, NX_ROLE_BUTTON, 0, &unused, out_state);
}

nx_result nx_checkbox(nx_context *ctx, const nx_control_desc *desc,
                      int *checked, nx_interaction_state *out_state) {
    if (!checked) {
        if (out_state) *out_state = (nx_interaction_state){0};
        return NX_ERROR_ARGUMENT;
    }
    int next = 0;
    nx_result result = nx_control_run(ctx, desc, NX_ROLE_CHECKBOX,
                                      *checked != 0, &next, out_state);
    if (result == NX_OK) *checked = next;
    return result;
}

nx_result nx_radio(nx_context *ctx, const nx_control_desc *desc,
                   uint64_t option, uint64_t *selected,
                   nx_interaction_state *out_state) {
    if (!option || !selected) {
        if (out_state) *out_state = (nx_interaction_state){0};
        return NX_ERROR_ARGUMENT;
    }
    int next = 0;
    nx_result result = nx_control_run(ctx, desc, NX_ROLE_RADIO,
                                      *selected == option, &next, out_state);
    if (result != NX_OK) return result;
    if (out_state->clicked) {
        *selected = option;
        for (size_t i = 0; i + 1u < ctx->semantics.count; ++i) {
            nx_semantic_record *node = &ctx->semantics.nodes[i];
            if (node->role == NX_ROLE_RADIO && node->has_radio_visual &&
                nx_same_selection_group(node, &ctx->semantics.nodes[ctx->semantics.count - 1u])) {
                node->state &= ~NX_SEM_CHECKED;
                ctx->commands[node->radio_outer_command].color =
                    node->radio_unchecked_color;
                ctx->commands[node->radio_inner_command].color =
                    (nx_color){0};
            }
        }
    }
    return NX_OK;
}

nx_result nx_toggle(nx_context *ctx, const nx_control_desc *desc,
                    int *on, nx_interaction_state *out_state) {
    if (!on) {
        if (out_state) *out_state = (nx_interaction_state){0};
        return NX_ERROR_ARGUMENT;
    }
    int next = 0;
    nx_result result = nx_control_run(ctx, desc, NX_ROLE_SWITCH,
                                      *on != 0, &next, out_state);
    if (result == NX_OK) *on = next;
    return result;
}

static int nx_valid_container_desc(const nx_container_desc *desc) {
    return desc && desc->id && desc->name.length &&
           nx_valid_rect(desc->bounds) && desc->bounds.width > 0.0f &&
           desc->bounds.height > 0.0f &&
           (!desc->label_layout || desc->glyph_cache_bytes);
}

static nx_result nx_container_paint(nx_context *ctx,
                                    const nx_container_desc *desc,
                                    nx_semantic_role role) {
    nx_color background, border, text_color;
    float radius, border_width;
    if (nx_theme_color(ctx, role == NX_ROLE_GROUP ? NX_COLOR_SURFACE :
                       NX_COLOR_SURFACE_RAISED, &background) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_BORDER, &border) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_TEXT, &text_color) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_RADIUS_MD, &radius) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_BORDER_WIDTH, &border_width) != NX_OK)
        return NX_ERROR_STATE;
    nx_result result = nx_fill_round_rect(ctx, desc->bounds, radius,
                                           background);
    if (result != NX_OK) return result;
    result = nx_stroke_rect(ctx, desc->bounds, border_width, border);
    if (result != NX_OK) return result;
    if (desc->label_layout)
        result = nx_draw_text_layout(ctx, desc->label_layout,
                                     desc->label_origin, text_color,
                                     desc->glyph_cache_bytes);
    return result;
}

static nx_result nx_container_run(nx_context *ctx,
                                  const nx_container_desc *desc,
                                  nx_semantic_role role) {
    if (!ctx || !nx_valid_container_desc(desc)) return NX_ERROR_ARGUMENT;
    size_t old_text_count = ctx->semantics.text_count;
    size_t old_command_count = ctx->command_count;
    nx_owned_bitmap *old_bitmap = ctx->owned_bitmaps;
    nx_result result = nx_display_register(ctx, desc->id, desc->parent_id,
                                            role, desc->bounds, desc->name);
    if (result != NX_OK) return result;
    result = nx_container_paint(ctx, desc, role);
    if (result != NX_OK) {
        ctx->command_count = old_command_count;
        nx_free_owned_bitmaps(ctx, old_bitmap);
        nx_semantics_rollback_last(ctx, desc->id, old_text_count);
    }
    return result;
}

nx_result nx_panel(nx_context *ctx, const nx_container_desc *desc) {
    return nx_container_run(ctx, desc, NX_ROLE_PANEL);
}

nx_result nx_group(nx_context *ctx, const nx_container_desc *desc) {
    return nx_container_run(ctx, desc, NX_ROLE_GROUP);
}

/* The icon paths use a 24-unit square. Every operation goes through ordinary
   validated commands, so native and software renderers share their geometry. */
typedef struct nx_icon_pen {
    nx_context *ctx;
    nx_rect box;
    float stroke;
    nx_color color;
    nx_result result;
} nx_icon_pen;

static void nx_icon_line(nx_icon_pen *p, float x0, float y0, float x1, float y1) {
    if (p->result != NX_OK) return;
    nx_point a = {p->box.x + x0 * p->box.width / 24.0f,
                  p->box.y + y0 * p->box.height / 24.0f};
    nx_point b = {p->box.x + x1 * p->box.width / 24.0f,
                  p->box.y + y1 * p->box.height / 24.0f};
    p->result = nx_draw_line(p->ctx, a, b, p->stroke, p->color);
}

static void nx_icon_arc(nx_icon_pen *p, float x, float y, float rx, float ry,
                         float start, float end) {
    const unsigned steps = 16;
    for (unsigned i = 0; i < steps; ++i) {
        float a = (start + (end - start) * i / steps) * 0.01745329252f;
        float b = (start + (end - start) * (i + 1u) / steps) * 0.01745329252f;
        nx_icon_line(p, x + rx * cosf(a), y + ry * sinf(a),
                       x + rx * cosf(b), y + ry * sinf(b));
    }
}

static void nx_icon_box(nx_icon_pen *p, float x, float y, float w, float h) {
    nx_icon_line(p, x, y, x + w, y);
    nx_icon_line(p, x + w, y, x + w, y + h);
    nx_icon_line(p, x + w, y + h, x, y + h);
    nx_icon_line(p, x, y + h, x, y);
}

nx_result nx_draw_icon(nx_context *ctx, nx_icon icon, nx_rect bounds,
                        float stroke_width, nx_color color) {
    if (!ctx || icon < NX_ICON_NONE || icon >= NX_ICON_COUNT ||
        !nx_valid_rect(bounds) || !isfinite(stroke_width) || stroke_width <= 0 ||
        !nx_valid_color(color)) return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    size_t old_count = ctx->command_count;
    nx_icon_pen p = {ctx, bounds, stroke_width, color, NX_OK};
    switch (icon) {
    case NX_ICON_NONE: break;
    case NX_ICON_CLOUD:
        nx_icon_arc(&p, 6, 14, 4, 4, 90, 265);
        nx_icon_arc(&p, 11, 9, 6, 6, 180, 355);
        nx_icon_arc(&p, 18, 14, 4, 4, 265, 450);
        nx_icon_line(&p, 6, 18, 9, 18); nx_icon_line(&p, 15, 18, 18, 18);
        nx_icon_line(&p, 12, 21, 12, 12);
        nx_icon_line(&p, 9, 15, 12, 12); nx_icon_line(&p, 12, 12, 15, 15); break;
    case NX_ICON_HOME:
        nx_icon_line(&p, 2, 11, 12, 3); nx_icon_line(&p, 12, 3, 22, 11);
        nx_icon_line(&p, 5, 9, 5, 21); nx_icon_line(&p, 19, 9, 19, 21);
        nx_icon_line(&p, 5, 21, 10, 21); nx_icon_line(&p, 14, 21, 19, 21);
        nx_icon_box(&p, 10, 14, 4, 7); break;
    case NX_ICON_FOLDER:
        nx_icon_line(&p, 3, 7, 3, 21); nx_icon_line(&p, 3, 21, 21, 21);
        nx_icon_line(&p, 21, 21, 21, 7); nx_icon_line(&p, 21, 7, 11, 7);
        nx_icon_line(&p, 11, 7, 9, 4); nx_icon_line(&p, 9, 4, 3, 4);
        nx_icon_line(&p, 3, 4, 3, 7); nx_icon_line(&p, 3, 10, 21, 10); break;
    case NX_ICON_UPLOAD: case NX_ICON_DOWNLOAD: {
        float y0 = icon == NX_ICON_DOWNLOAD ? 4.0f : 17.0f;
        float y1 = icon == NX_ICON_DOWNLOAD ? 17.0f : 4.0f;
        float wing = icon == NX_ICON_DOWNLOAD ? 12.0f : 9.0f;
        nx_icon_line(&p, 12, y0, 12, y1);
        nx_icon_line(&p, 7, wing, 12, y1); nx_icon_line(&p, 12, y1, 17, wing);
        nx_icon_line(&p, 3, 17, 3, 21); nx_icon_line(&p, 3, 21, 21, 21);
        nx_icon_line(&p, 21, 21, 21, 17); break;
    }
    case NX_ICON_TRANSFER:
        nx_icon_line(&p, 8, 17, 8, 4);
        nx_icon_line(&p, 4, 8, 8, 4); nx_icon_line(&p, 8, 4, 12, 8);
        nx_icon_line(&p, 16, 4, 16, 17);
        nx_icon_line(&p, 12, 13, 16, 17); nx_icon_line(&p, 16, 17, 20, 13);
        nx_icon_line(&p, 3, 17, 3, 21); nx_icon_line(&p, 3, 21, 21, 21);
        nx_icon_line(&p, 21, 21, 21, 17); break;
    case NX_ICON_STAR: {
        float px = 12, py = 2;
        for (unsigned i = 1; i <= 10; ++i) {
            float angle = (-90.0f + i * 36.0f) * 0.01745329252f;
            float radius = i & 1u ? 4.3f : 10.0f;
            float x = 12 + radius * cosf(angle), y = 12 + radius * sinf(angle);
            nx_icon_line(&p, px, py, x, y); px = x; py = y;
        } break;
    }
    case NX_ICON_TRASH:
        nx_icon_line(&p, 3, 6, 21, 6); nx_icon_box(&p, 9, 3, 6, 3);
        nx_icon_line(&p, 6, 6, 7, 21); nx_icon_line(&p, 7, 21, 17, 21);
        nx_icon_line(&p, 17, 21, 18, 6);
        nx_icon_line(&p, 10, 10, 10, 17); nx_icon_line(&p, 14, 10, 14, 17); break;
    case NX_ICON_SETTINGS:
        nx_icon_arc(&p, 12, 12, 4, 4, 0, 360);
        for (unsigned i = 0; i < 8; ++i) {
            float a = (float)i * 0.7853981634f;
            nx_icon_line(&p, 12 + 8 * cosf(a), 12 + 8 * sinf(a),
                             12 + 10 * cosf(a), 12 + 10 * sinf(a));
            nx_icon_arc(&p, 12, 12, 8, 8, i * 45.0f + 10, i * 45.0f + 35);
        } break;
    case NX_ICON_SEARCH:
        nx_icon_arc(&p, 10, 10, 6.5f, 6.5f, 0, 360);
        nx_icon_line(&p, 15, 15, 21, 21); break;
    case NX_ICON_GRID:
        for (unsigned i = 0; i < 4; ++i)
            nx_icon_box(&p, 4 + (i % 2) * 10.0f, 4 + (i / 2) * 10.0f, 6, 6);
        break;
    case NX_ICON_LIST:
        for (unsigned i = 0; i < 3; ++i) {
            float y = 5 + i * 7.0f;
            nx_icon_line(&p, 3, y, 4, y); nx_icon_line(&p, 8, y, 21, y);
        } break;
    case NX_ICON_MORE:
        for (unsigned i = 0; i < 3; ++i)
            nx_icon_arc(&p, 5 + i * 7.0f, 12, 0.65f, 0.65f, 0, 360);
        break;
    case NX_ICON_CHEVRON_RIGHT: case NX_ICON_CHEVRON_LEFT: {
        float x = icon == NX_ICON_CHEVRON_RIGHT ? 9.0f : 15.0f;
        nx_icon_line(&p, x, 5, 24 - x, 12);
        nx_icon_line(&p, 24 - x, 12, x, 19); break;
    }
    case NX_ICON_CLOSE:
        nx_icon_line(&p, 5, 5, 19, 19); nx_icon_line(&p, 5, 19, 19, 5); break;
    case NX_ICON_PLUS:
        nx_icon_line(&p, 12, 4, 12, 20); nx_icon_line(&p, 4, 12, 20, 12); break;
    case NX_ICON_LINK:
        nx_icon_arc(&p, 8, 16, 5, 5, 45, 280);
        nx_icon_arc(&p, 16, 8, 5, 5, 225, 460);
        nx_icon_line(&p, 8, 16, 16, 8); break;
    case NX_ICON_SHARE:
        nx_icon_line(&p, 7, 12, 17, 5); nx_icon_line(&p, 7, 12, 17, 19);
        nx_icon_arc(&p, 5, 12, 3, 3, 0, 360);
        nx_icon_arc(&p, 19, 4, 3, 3, 0, 360);
        nx_icon_arc(&p, 19, 20, 3, 3, 0, 360); break;
    case NX_ICON_CHECK:
        nx_icon_line(&p, 4, 12, 9, 17); nx_icon_line(&p, 9, 17, 20, 6); break;
    case NX_ICON_STORAGE:
        nx_icon_arc(&p, 12, 5, 8, 3, 0, 360);
        nx_icon_arc(&p, 12, 12, 8, 3, 0, 180);
        nx_icon_arc(&p, 12, 19, 8, 3, 0, 180);
        nx_icon_line(&p, 4, 5, 4, 19); nx_icon_line(&p, 20, 5, 20, 19); break;
    case NX_ICON_THERMOMETER:
        nx_icon_arc(&p, 12, 17, 5, 5, -55, 235);
        nx_icon_line(&p, 9, 13, 9, 5); nx_icon_line(&p, 15, 13, 15, 5);
        nx_icon_arc(&p, 12, 5, 3, 3, 180, 360);
        nx_icon_line(&p, 12, 9, 12, 18); break;
    case NX_ICON_WIFI: case NX_ICON_WIFI_SHIELD:
        if (icon == NX_ICON_WIFI_SHIELD) {
            nx_icon_pen wifi = p;
            wifi.box.width *= .70f; wifi.box.height *= .70f;
            nx_icon_arc(&wifi, 12, 22, 20, 18, 240, 300);
            nx_icon_arc(&wifi, 12, 22, 13, 12, 240, 300);
            nx_icon_arc(&wifi, 12, 22, 6, 6, 240, 300);
            nx_icon_arc(&wifi, 12, 21, 1, 1, 0, 360);
            p.result = wifi.result;
            p.box.x += bounds.width * .58f;
            p.box.y += bounds.height * .46f;
            p.box.width *= .42f; p.box.height *= .54f;
            /* All child lines retain the caller's logical stroke width. */
            nx_icon_line(&p, 3, 5, 12, 2); nx_icon_line(&p, 12, 2, 21, 5);
            nx_icon_line(&p, 21, 5, 20, 14); nx_icon_line(&p, 20, 14, 17, 19);
            nx_icon_line(&p, 17, 19, 12, 22); nx_icon_line(&p, 12, 22, 7, 19);
            nx_icon_line(&p, 7, 19, 4, 14); nx_icon_line(&p, 4, 14, 3, 5);
            nx_icon_line(&p, 7, 12, 11, 16); nx_icon_line(&p, 11, 16, 17, 9);
        } else {
            nx_icon_arc(&p, 12, 22, 20, 18, 240, 300);
            nx_icon_arc(&p, 12, 22, 13, 12, 240, 300);
            nx_icon_arc(&p, 12, 22, 6, 6, 240, 300);
            nx_icon_arc(&p, 12, 21, 1, 1, 0, 360);
        } break;
    case NX_ICON_SHIELD: case NX_ICON_SHIELD_CHECK:
        nx_icon_line(&p, 3, 5, 12, 2); nx_icon_line(&p, 12, 2, 21, 5);
        nx_icon_line(&p, 21, 5, 20, 14); nx_icon_line(&p, 20, 14, 17, 19);
        nx_icon_line(&p, 17, 19, 12, 22); nx_icon_line(&p, 12, 22, 7, 19);
        nx_icon_line(&p, 7, 19, 4, 14); nx_icon_line(&p, 4, 14, 3, 5);
        if (icon == NX_ICON_SHIELD_CHECK) {
            nx_icon_line(&p, 7, 12, 11, 16); nx_icon_line(&p, 11, 16, 17, 9);
        } break;
    case NX_ICON_ACTIVITY:
        nx_icon_line(&p, 1, 13, 6, 13); nx_icon_line(&p, 6, 13, 10, 3);
        nx_icon_line(&p, 10, 3, 14, 22); nx_icon_line(&p, 14, 22, 18, 10);
        nx_icon_line(&p, 18, 10, 20, 13); nx_icon_line(&p, 20, 13, 23, 13); break;
    case NX_ICON_IMAGE:
        nx_icon_box(&p, 3, 3, 18, 18); nx_icon_arc(&p, 16, 8, 2, 2, 0, 360);
        nx_icon_line(&p, 3, 17, 9, 10); nx_icon_line(&p, 9, 10, 15, 17);
        nx_icon_line(&p, 15, 17, 18, 14); nx_icon_line(&p, 18, 14, 21, 18); break;
    case NX_ICON_EDIT:
        nx_icon_line(&p, 4, 20, 6, 14); nx_icon_line(&p, 6, 14, 17, 3);
        nx_icon_line(&p, 17, 3, 21, 7); nx_icon_line(&p, 21, 7, 10, 18);
        nx_icon_line(&p, 10, 18, 4, 20); nx_icon_line(&p, 14, 6, 18, 10); break;
    case NX_ICON_INFO:
        nx_icon_arc(&p, 12, 12, 10, 10, 0, 360);
        nx_icon_line(&p, 12, 7, 12, 7.3f); nx_icon_line(&p, 12, 11, 12, 17); break;
    case NX_ICON_SUN:
        nx_icon_arc(&p, 12, 12, 5, 5, 0, 360);
        for (unsigned i = 0; i < 8; ++i) {
            float a = i * 0.7853981634f;
            nx_icon_line(&p, 12 + 8 * cosf(a), 12 + 8 * sinf(a),
                             12 + 10 * cosf(a), 12 + 10 * sinf(a));
        } break;
    default: return NX_ERROR_ARGUMENT;
    }
    if (p.result != NX_OK) ctx->command_count = old_count;
    return p.result;
}

nx_result nx_draw_surface(nx_context *ctx, nx_rect bounds,
                           const nx_surface_style *style) {
    if (!ctx || !style || !nx_valid_rect(bounds) ||
        !nx_valid_color(style->background) || !nx_valid_color(style->border) ||
        !isfinite(style->radius) || style->radius < 0 ||
        !isfinite(style->border_width) || style->border_width < 0)
        return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    size_t old_count = ctx->command_count;
    float border = fminf(style->border_width,
                         0.5f * fminf(bounds.width, bounds.height));
    nx_result result = nx_fill_round_rect(ctx, bounds, style->radius,
                                          border > 0 ? style->border : style->background);
    if (result == NX_OK && border > 0) {
        nx_rect inner = {bounds.x + border, bounds.y + border,
                          bounds.width - 2 * border, bounds.height - 2 * border};
        result = nx_fill_round_rect(ctx, inner, fmaxf(0, style->radius - border),
                                    style->background);
    }
    if (result != NX_OK) ctx->command_count = old_count;
    return result;
}

nx_result nx_card(nx_context *ctx, const nx_container_desc *desc) {
    if (!ctx || !nx_valid_container_desc(desc)) return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    nx_theme *t = &ctx->theme_stack[ctx->theme_depth];
    nx_surface_style style = {t->colors[NX_COLOR_SURFACE], t->colors[NX_COLOR_BORDER],
        t->metrics[NX_METRIC_RADIUS_MD], t->metrics[NX_METRIC_BORDER_WIDTH]};
    size_t old_commands = ctx->command_count, old_text = ctx->semantics.text_count;
    nx_owned_bitmap *old_bitmap = ctx->owned_bitmaps;
    nx_result result = nx_display_register(ctx, desc->id, desc->parent_id,
                                           NX_ROLE_PANEL, desc->bounds, desc->name);
    if (result != NX_OK) return result;
    result = nx_draw_surface(ctx, desc->bounds, &style);
    if (result == NX_OK && desc->label_layout)
        result = nx_draw_text_layout(ctx, desc->label_layout, desc->label_origin,
                                      t->colors[NX_COLOR_TEXT], desc->glyph_cache_bytes);
    if (result != NX_OK) {
        ctx->command_count = old_commands;
        nx_free_owned_bitmaps(ctx, old_bitmap);
        nx_semantics_rollback_last(ctx, desc->id, old_text);
    }
    return result;
}

nx_result nx_draw_text_stamp(nx_context *ctx, const nx_text_stamp *stamp,
                              nx_point origin, nx_color tint) {
    if (!ctx || !stamp || !nx_valid_point(origin) || !nx_valid_color(tint)) return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    nx_bitmap_view view; nx_point offset;
    nx_result result = nx_text_stamp_view(stamp, &view, &offset);
    if (result != NX_OK || !view.width) return result;
    nx_rect bounds;
    result=nx_text_stamp_bounds(stamp,&bounds);
    if(result!=NX_OK)return result;
    bounds.x+=origin.x;bounds.y+=origin.y;
    return nx_draw_bitmap(ctx,bounds,view,tint);
}

nx_result nx_label_stamp(nx_context *ctx, uint64_t id, uint64_t parent_id,
                          nx_rect bounds, const nx_text_stamp *stamp,
                          nx_point origin, nx_color tint) {
    if (!ctx || !stamp) return NX_ERROR_ARGUMENT;
    nx_utf8_span name;
    nx_result result = nx_text_stamp_text(stamp, &name);
    if (result != NX_OK) return result;
    size_t old_text = ctx->semantics.text_count;
    result = nx_display_register(ctx, id, parent_id, NX_ROLE_TEXT, bounds, name);
    if (result != NX_OK) return result;
    result = nx_draw_text_stamp(ctx, stamp, origin, tint);
    if (result != NX_OK) nx_semantics_rollback_last(ctx, id, old_text);
    return result;
}

nx_result nx_action(nx_context *ctx, const nx_action_desc *desc,
                     nx_interaction_state *out_state) {
    if (!out_state) return NX_ERROR_ARGUMENT;
    *out_state = (nx_interaction_state){0};
    if (!ctx || !desc || !desc->control.name.length ||
        desc->variant < NX_ACTION_PRIMARY || desc->variant > NX_ACTION_TILE ||
        desc->icon < NX_ICON_NONE || desc->icon >= NX_ICON_COUNT ||
        (desc->control.label_layout && (!desc->control.glyph_cache_bytes || desc->label_stamp)))
        return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    const nx_control_desc *c = &desc->control;
    if (desc->icon != NX_ICON_NONE && (!nx_valid_rect(desc->icon_bounds) ||
        desc->icon_bounds.x < c->bounds.x || desc->icon_bounds.y < c->bounds.y ||
        (double)desc->icon_bounds.x + desc->icon_bounds.width > (double)c->bounds.x + c->bounds.width ||
        (double)desc->icon_bounds.y + desc->icon_bounds.height > (double)c->bounds.y + c->bounds.height))
        return NX_ERROR_ARGUMENT;
    int selectable = desc->variant == NX_ACTION_NAVIGATION || desc->variant == NX_ACTION_TILE;
    nx_widget_desc widget = {0};
    widget.semantic = (nx_semantic_desc){c->id, c->parent_id,
        desc->variant == NX_ACTION_NAVIGATION ? NX_ROLE_TAB :
        desc->variant == NX_ACTION_TILE ? NX_ROLE_LIST_ITEM : NX_ROLE_BUTTON,
        c->bounds, c->name, {0}, {0},
        (c->disabled ? NX_SEM_DISABLED : 0u) | (desc->selected ? NX_SEM_SELECTED : 0u),
        NX_ACTION_FOCUS | (selectable ? NX_ACTION_SELECT : NX_ACTION_PRESS)};
    widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    size_t old_text = ctx->semantics.text_count, old_commands = ctx->command_count;
    nx_owned_bitmap *old_bitmap = ctx->owned_bitmaps;
    nx_interaction_state state;
    nx_result result = nx_widget_register(ctx, &widget, &state);
    if (result != NX_OK) return result;
    nx_theme *t = &ctx->theme_stack[ctx->theme_depth];
    nx_color foreground = t->colors[c->disabled ? NX_COLOR_DISABLED_TEXT :
        state.held || state.hovered ? NX_COLOR_TEXT :
        desc->selected ? NX_COLOR_SELECTION_TEXT :
        desc->variant == NX_ACTION_PRIMARY ? NX_COLOR_ACCENT_TEXT : NX_COLOR_TEXT];
    nx_color background = t->colors[c->disabled ? NX_COLOR_DISABLED_BACKGROUND :
        state.held ? NX_COLOR_PRESSED : state.hovered ? NX_COLOR_HOVER :
        desc->selected ? NX_COLOR_SELECTION : desc->variant == NX_ACTION_PRIMARY ?
        NX_COLOR_ACCENT : NX_COLOR_SURFACE_RAISED];
    nx_surface_style surface = {background, t->colors[desc->selected ? NX_COLOR_ACCENT : NX_COLOR_BORDER],
        t->metrics[NX_METRIC_RADIUS_MD], t->metrics[NX_METRIC_BORDER_WIDTH]};
    if (state.focused) {
        surface.border = t->colors[NX_COLOR_FOCUS];
        surface.border_width = t->metrics[NX_METRIC_FOCUS_WIDTH];
    }
    int quiet = desc->variant == NX_ACTION_QUIET || desc->variant == NX_ACTION_NAVIGATION;
    if (!quiet || state.hovered || state.held || state.focused || desc->selected)
        result = nx_draw_surface(ctx, c->bounds, &surface);
    if (result == NX_OK && desc->icon != NX_ICON_NONE)
        result = nx_draw_icon(ctx, desc->icon, desc->icon_bounds,
            fmaxf(1, fminf(desc->icon_bounds.width, desc->icon_bounds.height) / 14), foreground);
    if (result == NX_OK && (c->label_layout || desc->label_stamp)) {
        result = nx_push_clip(ctx, c->bounds);
        if (result == NX_OK) {
            result = desc->label_stamp ? nx_draw_text_stamp(ctx, desc->label_stamp, c->label_origin, foreground) :
                nx_draw_text_layout(ctx, c->label_layout, c->label_origin, foreground, c->glyph_cache_bytes);
            nx_result popped = nx_pop_clip(ctx);
            if (result == NX_OK) result = popped;
        }
    }
    if (result != NX_OK) {
        ctx->command_count = old_commands;
        nx_free_owned_bitmaps(ctx, old_bitmap);
        nx_region_rollback_last(ctx, c->id);
        nx_semantics_rollback_last(ctx, c->id, old_text);
        return result;
    }
    *out_state = state;
    return NX_OK;
}

nx_result nx_progress(nx_context *ctx, const nx_progress_desc *desc) {
    if (!ctx || !desc || !desc->name.length || !nx_valid_rect(desc->bounds) ||
        !isfinite(desc->value) || desc->value < 0 || desc->value > 1)
        return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    char value[16];
    int length = snprintf(value, sizeof(value), "%.0f%%", desc->value * 100);
    if (length <= 0 || (size_t)length >= sizeof(value)) return NX_ERROR_LIMIT;
    nx_semantic_desc node = {desc->id, desc->parent_id, NX_ROLE_PROGRESS,
        desc->bounds, desc->name, {0}, {value, (size_t)length}, NX_SEM_READONLY, 0};
    size_t old_text = ctx->semantics.text_count, old_commands = ctx->command_count;
    nx_result result = nx_semantics_add(ctx, &node, NULL);
    if (result != NX_OK) return result;
    nx_theme *t = &ctx->theme_stack[ctx->theme_depth];
    float radius = desc->bounds.height * 0.5f;
    result = nx_fill_round_rect(ctx, desc->bounds, radius, t->colors[NX_COLOR_SELECTION]);
    if (result == NX_OK && desc->value > 0) {
        nx_rect fill = desc->bounds;
        fill.width *= (float)desc->value;
        result = nx_fill_round_rect(ctx, fill, radius, t->colors[NX_COLOR_ACCENT]);
    }
    if (result != NX_OK) {
        ctx->command_count = old_commands;
        nx_semantics_rollback_last(ctx, desc->id, old_text);
    }
    return result;
}

nx_result nx_scroll_area_begin(nx_context *ctx,
                                const nx_scroll_area_desc *desc,
                                float *offset_x, float *offset_y,
                                nx_scroll_info *out_info) {
    if (!out_info) return NX_ERROR_ARGUMENT;
    *out_info = (nx_scroll_info){0};
    if (!ctx || !desc || !offset_x || !offset_y || !desc->id ||
        !desc->name.length || !nx_valid_rect(desc->viewport) ||
        desc->viewport.width <= 0.0f || desc->viewport.height <= 0.0f ||
        !isfinite(desc->content_size.width) ||
        !isfinite(desc->content_size.height) ||
        desc->content_size.width < 0.0f ||
        desc->content_size.height < 0.0f ||
        !(desc->axes & (NX_SCROLL_X | NX_SCROLL_Y)) ||
        (desc->axes & ~(NX_SCROLL_X | NX_SCROLL_Y)) ||
        !isfinite(desc->wheel_step) || desc->wheel_step < 0.0f ||
        !isfinite(*offset_x) || !isfinite(*offset_y) ||
        *offset_x < 0.0f || *offset_y < 0.0f ||
        desc->thumb_x_id == desc->id || desc->thumb_y_id == desc->id ||
        (desc->parent_id && (desc->thumb_x_id == desc->parent_id ||
                             desc->thumb_y_id == desc->parent_id)) ||
        (desc->thumb_x_id && desc->thumb_x_id == desc->thumb_y_id) ||
        (desc->thumb_x_id && !(desc->axes & NX_SCROLL_X)) ||
        (desc->thumb_y_id && !(desc->axes & NX_SCROLL_Y)))
        return NX_ERROR_ARGUMENT;
    if (ctx->scroll_scope_count == NX_MAX_CLIP_DEPTH)
        return NX_ERROR_LIMIT;
    nx_widget_desc widget = {0};
    widget.semantic.id = desc->id;
    widget.semantic.parent_id = desc->parent_id;
    widget.semantic.role = NX_ROLE_SCROLL_AREA;
    widget.semantic.bounds = desc->viewport;
    widget.semantic.name = desc->name;
    widget.semantic.state = desc->disabled ? NX_SEM_DISABLED : 0u;
    widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_SCROLLABLE;
    size_t old_text_count = ctx->semantics.text_count;
    size_t old_command_count = ctx->command_count;
    nx_interaction_state state;
    nx_result result = nx_widget_register(ctx, &widget, &state);
    if (result != NX_OK) return result;
    double max_x = (desc->axes & NX_SCROLL_X) ?
        fmax(0.0, (double)desc->content_size.width - desc->viewport.width) : 0.0;
    double max_y = (desc->axes & NX_SCROLL_Y) ?
        fmax(0.0, (double)desc->content_size.height - desc->viewport.height) : 0.0;
    double step = desc->wheel_step > 0.0f ? desc->wheel_step : 40.0;
    double x = fmax(0.0, fmin(*offset_x - (double)state.wheel_x * step, max_x));
    double y = fmax(0.0, fmin(*offset_y - (double)state.wheel_y * step, max_y));
    double thumb_x_width = max_x > 0.0 ?
        fmin((double)desc->viewport.width,
             fmax(8.0, (double)desc->viewport.width *
                  desc->viewport.width / desc->content_size.width)) : 0.0;
    double thumb_y_height = max_y > 0.0 ?
        fmin((double)desc->viewport.height,
             fmax(8.0, (double)desc->viewport.height *
                  desc->viewport.height / desc->content_size.height)) : 0.0;
    double travel_x = desc->viewport.width - thumb_x_width;
    double travel_y = desc->viewport.height - thumb_y_height;
    if (!desc->disabled) {
        nx_region *scroll = nx_region_find(&ctx->previous_regions, desc->id);
        if (scroll) {
            x -= scroll->touch_delta_x;
            y -= scroll->touch_delta_y;
        }
        nx_region *thumb_x = nx_region_find(&ctx->previous_regions,
                                           desc->thumb_x_id);
        nx_region *thumb_y = nx_region_find(&ctx->previous_regions,
                                           desc->thumb_y_id);
        if (thumb_x && travel_x > 0.0)
            x += thumb_x->drag_delta_x * max_x / travel_x;
        if (thumb_y && travel_y > 0.0)
            y += thumb_y->drag_delta_y * max_y / travel_y;
        for (size_t i = 0; i < ctx->frame_events.count; ++i) {
            const nx_event *event = &ctx->frame_events.data[i];
            if (event->type != NX_EVENT_KEY_DOWN ||
                (event->modifiers & (NX_MOD_ALT | NX_MOD_CONTROL | NX_MOD_SUPER)))
                continue;
            if (max_x > 0.0 && desc->thumb_x_id &&
                ctx->focused_id == desc->thumb_x_id) {
                if (event->key == NX_KEY_LEFT) x -= step;
                else if (event->key == NX_KEY_RIGHT) x += step;
                else if (event->key == NX_KEY_PAGE_UP) x -= desc->viewport.width;
                else if (event->key == NX_KEY_PAGE_DOWN) x += desc->viewport.width;
                else if (event->key == NX_KEY_HOME) x = 0.0;
                else if (event->key == NX_KEY_END) x = max_x;
            }
            if (max_y > 0.0 && desc->thumb_y_id &&
                ctx->focused_id == desc->thumb_y_id) {
                if (event->key == NX_KEY_UP) y -= step;
                else if (event->key == NX_KEY_DOWN) y += step;
                else if (event->key == NX_KEY_PAGE_UP) y -= desc->viewport.height;
                else if (event->key == NX_KEY_PAGE_DOWN) y += desc->viewport.height;
                else if (event->key == NX_KEY_HOME) y = 0.0;
                else if (event->key == NX_KEY_END) y = max_y;
            }
        }
    }
    x = fmax(0.0, fmin(x, max_x));
    y = fmax(0.0, fmin(y, max_y));
    nx_color background, border, thumb;
    float radius, border_width;
    if (nx_theme_color(ctx, NX_COLOR_SURFACE_RAISED, &background) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_BORDER, &border) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_ACCENT, &thumb) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_RADIUS_SM, &radius) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_BORDER_WIDTH, &border_width) != NX_OK) {
        result = NX_ERROR_STATE;
        goto scroll_rollback;
    }
    result = nx_fill_round_rect(ctx, desc->viewport, radius, background);
    if (result != NX_OK) goto scroll_rollback;
    result = nx_stroke_rect(ctx, desc->viewport, border_width, border);
    if (result != NX_OK) goto scroll_rollback;
    nx_scroll_scope scope = {0};
    scope.parent_id = desc->id;
    scope.thumb_x_id = max_x > 0.0 ? desc->thumb_x_id : 0;
    scope.thumb_y_id = max_y > 0.0 ? desc->thumb_y_id : 0;
    scope.thumb_color = thumb;
    scope.x_fraction = max_x > 0.0 ? x / max_x : 0.0;
    scope.y_fraction = max_y > 0.0 ? y / max_y : 0.0;
    scope.show_x = max_x > 0.0;
    scope.show_y = max_y > 0.0;
    scope.disabled = desc->disabled;
    if (scope.show_y) {
        double width = fmin(6.0, (double)desc->viewport.width);
        double height = thumb_y_height;
        double tx = (double)desc->viewport.x + desc->viewport.width - width;
        double ty = (double)desc->viewport.y +
            y / max_y * (desc->viewport.height - height);
        if (!isfinite(tx) || !isfinite(ty) ||
            fabs(tx) > FLT_MAX || fabs(ty) > FLT_MAX) {
            result = NX_ERROR_LIMIT;
            goto scroll_rollback;
        }
        scope.thumb_y_rect = (nx_rect){(float)tx, (float)ty,
                                       (float)width, (float)height};
    }
    if (scope.show_x) {
        double height = fmin(6.0, (double)desc->viewport.height);
        double width = thumb_x_width;
        double tx = (double)desc->viewport.x +
            x / max_x * (desc->viewport.width - width);
        double ty = (double)desc->viewport.y + desc->viewport.height - height;
        if (!isfinite(tx) || !isfinite(ty) ||
            fabs(tx) > FLT_MAX || fabs(ty) > FLT_MAX) {
            result = NX_ERROR_LIMIT;
            goto scroll_rollback;
        }
        scope.thumb_x_rect = (nx_rect){(float)tx, (float)ty,
                                       (float)width, (float)height};
    }
    result = nx_push_clip(ctx, desc->viewport);
    if (result != NX_OK) goto scroll_rollback;
    scope.clip_depth = ctx->clip_depth;
    ctx->scroll_scopes[ctx->scroll_scope_count++] = scope;
    *offset_x = (float)x;
    *offset_y = (float)y;
    *out_info = (nx_scroll_info){desc->viewport, desc->content_size,
                                 (float)x, (float)y,
                                 (float)max_x, (float)max_y};
    return NX_OK;
scroll_rollback:
    ctx->command_count = old_command_count;
    nx_region_rollback_last(ctx, desc->id);
    nx_semantics_rollback_last(ctx, desc->id, old_text_count);
    return result;
}

static nx_result nx_scroll_thumb(nx_context *ctx, uint64_t id,
                                  uint64_t parent_id, nx_rect rect,
                                  nx_color color, double fraction,
                                  int vertical, int disabled,
                                  int *out_registered) {
    *out_registered = 0;
    if (id) {
        char value[8];
        int length = snprintf(value, sizeof(value), "%d%%",
                              (int)lround(fraction * 100.0));
        if (length < 0 || (size_t)length >= sizeof(value))
            return NX_ERROR_LIMIT;
        const char *name = vertical ? "Vertical scrollbar" :
                                      "Horizontal scrollbar";
        nx_widget_desc widget = {0};
        widget.semantic.id = id;
        widget.semantic.parent_id = parent_id;
        widget.semantic.role = NX_ROLE_SLIDER;
        widget.semantic.bounds = rect;
        widget.semantic.name = (nx_utf8_span){name, strlen(name)};
        widget.semantic.value = (nx_utf8_span){value, (size_t)length};
        widget.semantic.state = disabled ? NX_SEM_DISABLED : 0u;
        widget.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_INCREMENT |
                                  NX_ACTION_DECREMENT;
        widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
        nx_interaction_state interaction;
        nx_result result = nx_widget_register(ctx, &widget, &interaction);
        if (result != NX_OK) return result;
        *out_registered = 1;
    }
    return nx_fill_round_rect(ctx, rect,
                              0.5f * fminf(rect.width, rect.height), color);
}

nx_result nx_scroll_area_end(nx_context *ctx) {
    if (!ctx || ctx->state != 1u || !ctx->scroll_scope_count ||
        ctx->clip_depth !=
            ctx->scroll_scopes[ctx->scroll_scope_count - 1u].clip_depth)
        return NX_ERROR_STATE;
    nx_scroll_scope *scope = &ctx->scroll_scopes[ctx->scroll_scope_count - 1u];
    size_t old_commands = ctx->command_count;
    size_t x_text = ctx->semantics.text_count;
    size_t y_text = x_text;
    int x_registered = 0, y_registered = 0;
    nx_result result = NX_OK;
    if (scope->show_x) {
        result = nx_scroll_thumb(ctx, scope->thumb_x_id, scope->parent_id,
                                  scope->thumb_x_rect, scope->thumb_color,
                                  scope->x_fraction, 0, scope->disabled,
                                  &x_registered);
        if (result != NX_OK) goto rollback;
    }
    y_text = ctx->semantics.text_count;
    if (scope->show_y) {
        result = nx_scroll_thumb(ctx, scope->thumb_y_id, scope->parent_id,
                                  scope->thumb_y_rect, scope->thumb_color,
                                  scope->y_fraction, 1, scope->disabled,
                                  &y_registered);
        if (result != NX_OK) goto rollback;
    }
    result = nx_pop_clip(ctx);
    if (result != NX_OK) goto rollback;
    --ctx->scroll_scope_count;
    return NX_OK;
rollback:
    ctx->command_count = old_commands;
    if (y_registered) {
        nx_region_rollback_last(ctx, scope->thumb_y_id);
        nx_semantics_rollback_last(ctx, scope->thumb_y_id, y_text);
    }
    if (x_registered) {
        nx_region_rollback_last(ctx, scope->thumb_x_id);
        nx_semantics_rollback_last(ctx, scope->thumb_x_id, x_text);
    }
    return result;
}

nx_result nx_tab_panel(nx_context *ctx, const nx_container_desc *desc) {
    return nx_container_run(ctx, desc, NX_ROLE_TAB_PANEL);
}

typedef struct nx_table_header_stage {
    float width;
    nx_interaction_state header, handle;
} nx_table_header_stage;

nx_result nx_table_headers(nx_context *ctx, const nx_table_desc *desc,
                           nx_table_sort *sort) {
    if (!ctx || !desc || !sort || !desc->id || !desc->name.length ||
        !nx_valid_rect(desc->bounds) || desc->bounds.width <= 0.0f ||
        desc->bounds.height <= 0.0f || !isfinite(desc->header_height) ||
        desc->header_height <= 0.0f ||
        desc->header_height > desc->bounds.height || !desc->columns ||
        !desc->column_count ||
        desc->column_count > SIZE_MAX / sizeof(nx_table_header_stage))
        return NX_ERROR_ARGUMENT;
    double extent = desc->bounds.x;
    for (size_t i = 0; i < desc->column_count; ++i) {
        const nx_table_column_desc *column = &desc->columns[i];
        if (!column->id || column->id == desc->id ||
            column->resize_id == desc->id ||
            column->resize_id == column->id || !column->name.length ||
            !nx_valid_span(column->name) ||
            !isfinite(column->width) || !isfinite(column->minimum_width) ||
            !isfinite(column->maximum_width) ||
            column->minimum_width <= 0.0f ||
            column->maximum_width < column->minimum_width ||
            column->width < column->minimum_width ||
            column->width > column->maximum_width ||
            !isfinite(column->label_offset.x) ||
            !isfinite(column->label_offset.y) ||
            (column->label_layout && !column->glyph_cache_bytes))
            return NX_ERROR_ARGUMENT;
        extent += column->width;
        if (extent > FLT_MAX) return NX_ERROR_LIMIT;
        for (size_t j = 0; j < i; ++j) {
            const nx_table_column_desc *prior = &desc->columns[j];
            if (column->id == prior->id ||
                (column->resize_id && column->resize_id == prior->resize_id) ||
                (column->resize_id && column->resize_id == prior->id) ||
                (prior->resize_id && prior->resize_id == column->id))
                return NX_ERROR_ARGUMENT;
        }
    }
    if (ctx->state != 1u) return NX_ERROR_STATE;
    nx_table_header_stage *stage = (nx_table_header_stage *)
        ctx->allocator.allocate(ctx->allocator.user,
                                desc->column_count * sizeof(*stage));
    if (!stage) return NX_ERROR_MEMORY;
    size_t old_semantics = ctx->semantics.count;
    size_t old_text = ctx->semantics.text_count;
    size_t old_regions = ctx->current_regions.count;
    size_t old_commands = ctx->command_count;
    nx_owned_bitmap *old_bitmaps = ctx->owned_bitmaps;
    unsigned old_clip = ctx->clip_depth;
    nx_table_sort next_sort = *sort;
    nx_semantic_desc table = {0};
    table.id = desc->id;
    table.parent_id = desc->parent_id;
    table.role = NX_ROLE_TABLE;
    table.bounds = desc->bounds;
    table.name = desc->name;
    table.state = desc->disabled ? NX_SEM_DISABLED : 0u;
    nx_result result = nx_semantics_add(ctx, &table, NULL);
    if (result != NX_OK) goto table_finish;
    nx_rect header_clip = {desc->bounds.x, desc->bounds.y,
                           desc->bounds.width, desc->header_height};
    result = nx_push_clip(ctx, header_clip);
    if (result != NX_OK) goto table_rollback;
    double x = desc->bounds.x;
    for (size_t i = 0; i < desc->column_count; ++i) {
        const nx_table_column_desc *column = &desc->columns[i];
        stage[i] = (nx_table_header_stage){0};
        stage[i].width = column->width;
        nx_rect bounds = {(float)x, desc->bounds.y,
                          column->width, desc->header_height};
        nx_widget_desc header = {0};
        header.semantic.id = column->id;
        header.semantic.parent_id = desc->id;
        header.semantic.role = NX_ROLE_COLUMN_HEADER;
        header.semantic.bounds = bounds;
        header.semantic.name = column->name;
        header.semantic.state = desc->disabled ? NX_SEM_DISABLED : 0u;
        if (column->sortable) {
            header.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_SELECT;
            header.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
        }
        result = nx_widget_register(ctx, &header, &stage[i].header);
        if (result != NX_OK) goto table_rollback;
        if (!desc->disabled && column->sortable && stage[i].header.clicked) {
            next_sort.descending = next_sort.column_id == column->id ?
                (!!next_sort.descending ^ (int)(stage[i].header.clicked & 1u)) :
                !(stage[i].header.clicked & 1u);
            next_sort.column_id = column->id;
        }
        if (column->resize_id) {
            float handle_width = fminf(10.0f, column->width);
            nx_widget_desc handle = {0};
            handle.semantic.id = column->resize_id;
            handle.semantic.parent_id = column->id;
            handle.semantic.role = NX_ROLE_SEPARATOR;
            handle.semantic.bounds = (nx_rect){(float)(x + column->width -
                handle_width), desc->bounds.y, handle_width,
                desc->header_height};
            handle.semantic.name = column->name;
            handle.semantic.state = desc->disabled ? NX_SEM_DISABLED : 0u;
            handle.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_INCREMENT |
                                      NX_ACTION_DECREMENT;
            handle.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
            result = nx_widget_register(ctx, &handle, &stage[i].handle);
            if (result != NX_OK) goto table_rollback;
            double delta = desc->disabled ? 0.0 : stage[i].handle.drag_delta_x;
            if (!desc->disabled && stage[i].handle.focused) {
                for (size_t e = 0; e < ctx->frame_events.count; ++e) {
                    const nx_event *event = &ctx->frame_events.data[e];
                    if (event->type != NX_EVENT_KEY_DOWN) continue;
                    if (event->key == NX_KEY_RIGHT) delta += 10.0;
                    if (event->key == NX_KEY_LEFT) delta -= 10.0;
                }
            }
            double width = (double)column->width + delta;
            if (width < column->minimum_width) width = column->minimum_width;
            if (width > column->maximum_width) width = column->maximum_width;
            stage[i].width = (float)width;
        }
        x += column->width;
    }
    nx_color surface, selected, hover, pressed, border, text_color, focus;
    float border_width, focus_width;
    if (nx_theme_color(ctx, NX_COLOR_SURFACE_RAISED, &surface) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_ACCENT, &selected) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_HOVER, &hover) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_PRESSED, &pressed) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_BORDER, &border) != NX_OK ||
        nx_theme_color(ctx, desc->disabled ? NX_COLOR_DISABLED_TEXT :
                       NX_COLOR_TEXT, &text_color) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_FOCUS, &focus) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_BORDER_WIDTH, &border_width) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_FOCUS_WIDTH, &focus_width) != NX_OK) {
        result = NX_ERROR_STATE;
        goto table_rollback;
    }
    x = desc->bounds.x;
    for (size_t i = 0; i < desc->column_count; ++i) {
        const nx_table_column_desc *column = &desc->columns[i];
        nx_rect bounds = {(float)x, desc->bounds.y,
                          stage[i].width, desc->header_height};
        int active = next_sort.column_id == column->id;
        nx_color background = active ? selected :
            stage[i].header.held ? pressed :
            stage[i].header.hovered ? hover : surface;
        result = nx_fill_rect(ctx, bounds, background);
        if (result != NX_OK) goto table_rollback;
        result = nx_stroke_rect(ctx, bounds, border_width, border);
        if (result != NX_OK) goto table_rollback;
        if (column->label_layout) {
            nx_point origin = {bounds.x + column->label_offset.x,
                               bounds.y + column->label_offset.y};
            result = nx_draw_text_layout(ctx, column->label_layout, origin,
                                         text_color, column->glyph_cache_bytes);
            if (result != NX_OK) goto table_rollback;
        }
        if (active && column->sortable) {
            float side = fminf(8.0f, fminf(bounds.width, bounds.height));
            float cx = bounds.x + bounds.width - 15.0f;
            float cy = bounds.y + bounds.height * 0.5f;
            nx_point a = {cx - side * 0.5f, cy};
            nx_point b = {cx + side * 0.5f, cy};
            nx_point c = {cx, cy + (next_sort.descending ? side * 0.5f :
                                                   -side * 0.5f)};
            result = nx_fill_triangle(ctx, a, b, c, text_color);
            if (result != NX_OK) goto table_rollback;
        }
        if (column->resize_id) {
            float edge = bounds.x + bounds.width;
            result = nx_draw_line(ctx,
                (nx_point){edge - 1.0f, bounds.y + 4.0f},
                (nx_point){edge - 1.0f, bounds.y + bounds.height - 4.0f},
                border_width, stage[i].handle.focused ? focus : border);
            if (result != NX_OK) goto table_rollback;
        }
        if (stage[i].header.focused) {
            result = nx_stroke_rect(ctx, bounds, focus_width, focus);
            if (result != NX_OK) goto table_rollback;
        }
        nx_semantic_record *node = &ctx->semantics.nodes[
            nx_semantic_find_index(&ctx->semantics, column->id)];
        node->bounds = bounds;
        if (active) {
            static const char ascending[] = "ascending";
            static const char descending[] = "descending";
            nx_utf8_span value = next_sort.descending ?
                (nx_utf8_span){descending, sizeof(descending) - 1u} :
                (nx_utf8_span){ascending, sizeof(ascending) - 1u};
            result = nx_semantic_reserve_text(ctx, value.length + 1u);
            if (result != NX_OK) goto table_rollback;
            node->value_offset = nx_semantic_copy_span(&ctx->semantics, value);
            node->value_length = value.length;
            node->state |= NX_SEM_SELECTED;
        }
        nx_region *region = nx_region_find(&ctx->current_regions, column->id);
        if (region) region->rect = bounds;
        if (column->resize_id) {
            float handle_width = fminf(10.0f, bounds.width);
            nx_rect handle_bounds = {bounds.x + bounds.width - handle_width,
                                     bounds.y, handle_width, bounds.height};
            nx_semantic_record *handle = &ctx->semantics.nodes[
                nx_semantic_find_index(&ctx->semantics, column->resize_id)];
            handle->bounds = handle_bounds;
            region = nx_region_find(&ctx->current_regions, column->resize_id);
            if (region) region->rect = handle_bounds;
        }
        x += stage[i].width;
        if (x > FLT_MAX) { result = NX_ERROR_LIMIT; goto table_rollback; }
    }
    result = nx_pop_clip(ctx);
    if (result != NX_OK) goto table_rollback;
    for (size_t i = 0; i < desc->column_count; ++i)
        desc->columns[i].width = stage[i].width;
    *sort = next_sort;
    goto table_finish;
table_rollback:
    ctx->clip_depth = old_clip;
    ctx->command_count = old_commands;
    nx_free_owned_bitmaps(ctx, old_bitmaps);
    while (ctx->current_regions.count > old_regions)
        nx_region_rollback_last(ctx,
            ctx->current_regions.data[ctx->current_regions.count - 1u].id);
    while (ctx->semantics.count > old_semantics)
        nx_semantics_rollback_last(ctx,
            ctx->semantics.nodes[ctx->semantics.count - 1u].id, old_text);
table_finish:
    ctx->allocator.deallocate(ctx->allocator.user, stage,
                              desc->column_count * sizeof(*stage));
    return result;
}

nx_result nx_table_row(nx_context *ctx, const nx_container_desc *desc) {
    return nx_container_run(ctx, desc, NX_ROLE_ROW);
}

nx_result nx_table_cell(nx_context *ctx, const nx_container_desc *desc) {
    return nx_container_run(ctx, desc, NX_ROLE_CELL);
}

nx_result nx_tree(nx_context *ctx, const nx_container_desc *desc) {
    return nx_container_run(ctx, desc, NX_ROLE_TREE);
}

static nx_result nx_overlay_begin_common(nx_context *ctx,
                                          const nx_overlay_desc *desc,
                                          int modal, int *out_dismissed) {
    if (!out_dismissed) return NX_ERROR_ARGUMENT;
    *out_dismissed = 0;
    if (!ctx || !desc || !nx_valid_container_desc(&desc->panel) ||
        !desc->backdrop_id || desc->backdrop_id == desc->panel.id)
        return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u || ctx->building_overlay_id ||
        ctx->overlay_built_this_frame || ctx->clip_depth ||
        ctx->scroll_scope_count) return NX_ERROR_STATE;
    size_t old_regions = ctx->current_regions.count;
    size_t old_semantics = ctx->semantics.count;
    size_t old_text = ctx->semantics.text_count;
    size_t old_commands = ctx->command_count;
    nx_owned_bitmap *old_bitmaps = ctx->owned_bitmaps;
    ctx->building_overlay_id = desc->panel.id;
    nx_rect backdrop = {0, 0, ctx->width, ctx->height};
    nx_interaction_state backdrop_state;
    nx_result result = nx_interact_region(ctx, desc->backdrop_id, backdrop,
                                           0, &backdrop_state);
    if (result != NX_OK) goto overlay_rollback;
    nx_widget_desc panel = {0};
    panel.semantic.id = desc->panel.id;
    panel.semantic.parent_id = desc->panel.parent_id;
    panel.semantic.role = modal ? NX_ROLE_DIALOG : NX_ROLE_POPUP;
    panel.semantic.bounds = desc->panel.bounds;
    panel.semantic.name = desc->panel.name;
    panel.semantic.actions = NX_ACTION_FOCUS;
    panel.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    nx_interaction_state panel_state;
    result = nx_widget_register(ctx, &panel, &panel_state);
    if (result != NX_OK) goto overlay_rollback;
    int dismissed = desc->dismiss_on_outside && backdrop_state.clicked;
    if (desc->dismiss_on_escape) {
        for (size_t i = 0; i < ctx->frame_events.count; ++i) {
            const nx_event *event = &ctx->frame_events.data[i];
            if (!event->repeat &&
                ((event->type==NX_EVENT_KEY_DOWN && event->key==NX_KEY_ESCAPE) ||
                 (event->type==NX_EVENT_TEXT_COMMAND && event->text_command==NX_TEXT_COMMAND_CANCEL &&
                  (!event->ime_target || event->ime_target==ctx->focused_id))))
                dismissed = 1;
        }
    }
    if (modal) {
        nx_color shade;
        if (nx_theme_color(ctx, NX_COLOR_SHADOW, &shade) != NX_OK) {
            result = NX_ERROR_STATE;
            goto overlay_rollback;
        }
        result = nx_fill_rect(ctx, backdrop, shade);
        if (result != NX_OK) goto overlay_rollback;
    }
    result = nx_container_paint(ctx, &desc->panel, panel.semantic.role);
    if (result != NX_OK) goto overlay_rollback;
    if (!ctx->previous_overlay_id)
        ctx->overlay_return_focus_id = ctx->focused_id;
    ctx->current_overlay_id = dismissed ? 0 : desc->panel.id;
    ctx->overlay_built_this_frame = 1;
    *out_dismissed = dismissed;
    return NX_OK;
overlay_rollback:
    ctx->building_overlay_id = 0;
    ctx->command_count = old_commands;
    nx_free_owned_bitmaps(ctx, old_bitmaps);
    while (ctx->current_regions.count > old_regions)
        nx_region_rollback_last(ctx,
            ctx->current_regions.data[ctx->current_regions.count - 1u].id);
    while (ctx->semantics.count > old_semantics)
        nx_semantics_rollback_last(ctx,
            ctx->semantics.nodes[ctx->semantics.count - 1u].id, old_text);
    return result;
}

nx_result nx_popup_begin(nx_context *ctx, const nx_overlay_desc *desc,
                          int *out_dismissed) {
    return nx_overlay_begin_common(ctx, desc, 0, out_dismissed);
}

nx_result nx_modal_begin(nx_context *ctx, const nx_overlay_desc *desc,
                          int *out_dismissed) {
    return nx_overlay_begin_common(ctx, desc, 1, out_dismissed);
}

nx_result nx_overlay_end(nx_context *ctx) {
    if (!ctx || ctx->state != 1u || !ctx->building_overlay_id)
        return NX_ERROR_STATE;
    ctx->building_overlay_id = 0;
    return NX_OK;
}

nx_result nx_overlay_close(nx_context *ctx) {
    if (!ctx || ctx->building_overlay_id || !ctx->state)
        return NX_ERROR_STATE;
    if (ctx->state == 1u) {
        if (!ctx->current_overlay_id && !ctx->previous_overlay_id)
            return NX_ERROR_STATE;
        ctx->current_overlay_id = 0;
        return NX_OK;
    }
    if (!ctx->previous_overlay_id) return NX_ERROR_STATE;
    uint64_t closing_id = ctx->previous_overlay_id;
    for (size_t i = 0; i < ctx->current_regions.count; ++i)
        if (ctx->current_regions.data[i].overlay_id == closing_id)
            ctx->current_regions.data[i].flags |= NX_REGION_INACTIVE;
    nx_region *pointer = nx_region_find(&ctx->current_regions,
                                        ctx->pointer_active_id);
    if (pointer && pointer->overlay_id == closing_id)
        ctx->pointer_active_id = 0;
    nx_region *key = nx_region_find(&ctx->current_regions, ctx->key_active_id);
    if (key && key->overlay_id == closing_id)
        ctx->key_active_id = 0;
    ctx->focused_id = ctx->overlay_return_focus_id;
    nx_region *focused = nx_region_find(&ctx->current_regions, ctx->focused_id);
    if (!focused || !(focused->flags & NX_REGION_FOCUSABLE) ||
        (focused->flags & (NX_REGION_DISABLED | NX_REGION_INACTIVE)))
        ctx->focused_id = 0;
    ctx->overlay_return_focus_id = 0;
    ctx->previous_overlay_id = 0;
    return NX_OK;
}

nx_result nx_tooltip(nx_context *ctx, const nx_container_desc *desc) {
    return nx_container_run(ctx, desc, NX_ROLE_TOOLTIP);
}

nx_result nx_menu_bar(nx_context *ctx, const nx_container_desc *desc) {
    return nx_container_run(ctx, desc, NX_ROLE_MENU_BAR);
}

nx_result nx_menu(nx_context *ctx, const nx_container_desc *desc) {
    return nx_container_run(ctx, desc, NX_ROLE_MENU);
}

nx_result nx_toolbar(nx_context *ctx, const nx_container_desc *desc) {
    return nx_container_run(ctx, desc, NX_ROLE_TOOLBAR);
}

nx_result nx_status_bar(nx_context *ctx, const nx_container_desc *desc) {
    return nx_container_run(ctx, desc, NX_ROLE_STATUS);
}

nx_result nx_menu_item(nx_context *ctx, const nx_menu_item_desc *desc,
                        int *checked, nx_interaction_state *out_state) {
    if (!out_state) return NX_ERROR_ARGUMENT;
    *out_state = (nx_interaction_state){0};
    if (!ctx || !desc || !desc->control.id ||
        !desc->control.parent_id || !desc->control.name.length ||
        !nx_valid_span(desc->control.name) ||
        !nx_valid_rect(desc->control.bounds) ||
        desc->control.bounds.width <= 0.0f ||
        desc->control.bounds.height <= 0.0f ||
        (desc->control.label_layout && !desc->control.glyph_cache_bytes) ||
        (desc->checkable && !checked)) return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    uint32_t parent_index = nx_semantic_find_index(&ctx->semantics,
                                                   desc->control.parent_id);
    if (parent_index == UINT32_MAX) return NX_ERROR_ARGUMENT;
    nx_semantic_role parent_role = ctx->semantics.nodes[parent_index].role;
    if (parent_role != NX_ROLE_MENU && parent_role != NX_ROLE_MENU_BAR)
        return NX_ERROR_ARGUMENT;
    size_t old_text = ctx->semantics.text_count;
    size_t old_commands = ctx->command_count;
    nx_owned_bitmap *old_bitmaps = ctx->owned_bitmaps;
    nx_widget_desc widget = {0};
    widget.semantic.id = desc->control.id;
    widget.semantic.parent_id = desc->control.parent_id;
    widget.semantic.role = NX_ROLE_MENU_ITEM;
    widget.semantic.bounds = desc->control.bounds;
    widget.semantic.name = desc->control.name;
    widget.semantic.state = (desc->control.disabled ? NX_SEM_DISABLED : 0u) |
        (desc->checkable && *checked ? NX_SEM_CHECKED : 0u);
    widget.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_PRESS;
    widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    nx_interaction_state state;
    nx_result result = nx_widget_register(ctx, &widget, &state);
    if (result != NX_OK) return result;
    nx_region *region = nx_region_find(&ctx->current_regions,
                                       desc->control.id);
    region->nav_parent_id = desc->control.parent_id;
    region->nav_axis = parent_role == NX_ROLE_MENU_BAR ? 2u : 1u;
    int next_checked = desc->checkable ?
        (!!*checked ^ (int)(state.clicked & 1u)) : 0;
    nx_color surface, selection, selection_text, hover, pressed, disabled_bg;
    nx_color foreground;
    nx_color focus;
    float focus_width;
    if (nx_theme_color(ctx, NX_COLOR_SURFACE_RAISED, &surface) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_SELECTION, &selection) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_SELECTION_TEXT, &selection_text) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_HOVER, &hover) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_PRESSED, &pressed) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_DISABLED_BACKGROUND, &disabled_bg) != NX_OK ||
        nx_theme_color(ctx, desc->control.disabled ? NX_COLOR_DISABLED_TEXT :
                       NX_COLOR_TEXT, &foreground) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_FOCUS, &focus) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_FOCUS_WIDTH, &focus_width) != NX_OK) {
        result = NX_ERROR_STATE;
        goto menu_item_rollback;
    }
    nx_color background = desc->control.disabled ? disabled_bg :
        next_checked ? selection :
        state.held ? pressed : state.hovered ? hover : surface;
    nx_color text_color = !desc->control.disabled && next_checked ?
        selection_text : foreground;
    result = nx_fill_rect(ctx, desc->control.bounds, background);
    if (result != NX_OK) goto menu_item_rollback;
    if (next_checked) {
        float side = fminf(desc->control.bounds.width,
                           desc->control.bounds.height) * 0.30f;
        float x = desc->control.bounds.x + side;
        float y = desc->control.bounds.y + desc->control.bounds.height * 0.5f;
        result = nx_draw_line(ctx, (nx_point){x, y},
            (nx_point){x + side * 0.45f, y + side * 0.5f},
            1.5f, text_color);
        if (result != NX_OK) goto menu_item_rollback;
        result = nx_draw_line(ctx,
            (nx_point){x + side * 0.45f, y + side * 0.5f},
            (nx_point){x + side * 1.3f, y - side * 0.6f},
            1.5f, text_color);
        if (result != NX_OK) goto menu_item_rollback;
    }
    if (desc->control.label_layout) {
        result = nx_draw_text_layout(ctx, desc->control.label_layout,
            desc->control.label_origin, text_color,
            desc->control.glyph_cache_bytes);
        if (result != NX_OK) goto menu_item_rollback;
    }
    if (state.focused) {
        result = nx_stroke_rect(ctx, desc->control.bounds,
                                focus_width, focus);
        if (result != NX_OK) goto menu_item_rollback;
    }
    if (next_checked)
        ctx->semantics.nodes[ctx->semantics.count - 1u].state |= NX_SEM_CHECKED;
    if (desc->checkable) *checked = next_checked;
    *out_state = state;
    return NX_OK;
menu_item_rollback:
    ctx->command_count = old_commands;
    nx_free_owned_bitmaps(ctx, old_bitmaps);
    nx_region_rollback_last(ctx, desc->control.id);
    nx_semantics_rollback_last(ctx, desc->control.id, old_text);
    return result;
}

nx_result nx_tree_item(nx_context *ctx, const nx_tree_item_desc *desc,
                       int *expanded, uint64_t *selected_id,
                       nx_interaction_state *out_state) {
    if (!out_state) return NX_ERROR_ARGUMENT;
    *out_state = (nx_interaction_state){0};
    if (!ctx || !desc || !selected_id || !desc->control.id ||
        !desc->control.name.length || !nx_valid_span(desc->control.name) ||
        !nx_valid_rect(desc->control.bounds) ||
        desc->control.bounds.width <= 0.0f ||
        desc->control.bounds.height <= 0.0f ||
        (desc->control.label_layout && !desc->control.glyph_cache_bytes) ||
        (desc->has_children && (!expanded || !desc->expander_id ||
            desc->expander_id == desc->control.id ||
            !nx_valid_rect(desc->expander_bounds) ||
            desc->expander_bounds.width <= 0.0f ||
            desc->expander_bounds.height <= 0.0f ||
            desc->expander_bounds.x < desc->control.bounds.x ||
            desc->expander_bounds.y < desc->control.bounds.y ||
            (double)desc->expander_bounds.x + desc->expander_bounds.width >
                (double)desc->control.bounds.x + desc->control.bounds.width ||
            (double)desc->expander_bounds.y + desc->expander_bounds.height >
                (double)desc->control.bounds.y + desc->control.bounds.height)) ||
        (!desc->has_children && desc->expander_id))
        return NX_ERROR_ARGUMENT;
    size_t old_text = ctx->semantics.text_count;
    size_t old_semantics = ctx->semantics.count;
    size_t old_regions = ctx->current_regions.count;
    size_t old_commands = ctx->command_count;
    nx_owned_bitmap *old_bitmaps = ctx->owned_bitmaps;
    nx_widget_desc item = {0};
    item.semantic.id = desc->control.id;
    item.semantic.parent_id = desc->control.parent_id;
    item.semantic.role = NX_ROLE_TREE_ITEM;
    item.semantic.bounds = desc->control.bounds;
    item.semantic.name = desc->control.name;
    item.semantic.state = (desc->control.disabled ? NX_SEM_DISABLED : 0u) |
        (*selected_id == desc->control.id ? NX_SEM_SELECTED : 0u) |
        (desc->has_children && *expanded ? NX_SEM_EXPANDED : 0u);
    item.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_SELECT |
        (desc->has_children ? (*expanded ? NX_ACTION_COLLAPSE :
                                           NX_ACTION_EXPAND) : 0u);
    item.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    nx_interaction_state state;
    nx_result result = nx_widget_register(ctx, &item, &state);
    if (result != NX_OK) return result;
    nx_interaction_state handle_state = {0};
    if (desc->has_children) {
        nx_widget_desc handle = {0};
        handle.semantic.id = desc->expander_id;
        handle.semantic.parent_id = desc->control.id;
        handle.semantic.role = NX_ROLE_BUTTON;
        handle.semantic.bounds = desc->expander_bounds;
        handle.semantic.name = desc->control.name;
        handle.semantic.state = (desc->control.disabled ? NX_SEM_DISABLED : 0u) |
            (*expanded ? NX_SEM_EXPANDED : 0u);
        handle.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_PRESS |
            (*expanded ? NX_ACTION_COLLAPSE : NX_ACTION_EXPAND);
        handle.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
        result = nx_widget_register(ctx, &handle, &handle_state);
        if (result != NX_OK) goto tree_item_rollback;
    }
    int next_expanded = desc->has_children ? !!*expanded : 0;
    if (!desc->control.disabled && desc->has_children) {
        next_expanded ^= (int)(handle_state.clicked & 1u);
        if (state.focused || handle_state.focused) {
            for (size_t i = 0; i < ctx->frame_events.count; ++i) {
                const nx_event *event = &ctx->frame_events.data[i];
                if (event->type != NX_EVENT_KEY_DOWN) continue;
                if (event->key == NX_KEY_RIGHT) next_expanded = 1;
                if (event->key == NX_KEY_LEFT) next_expanded = 0;
            }
        }
    }
    int next_selected = *selected_id == desc->control.id ||
        (!desc->control.disabled && state.clicked);
    nx_color surface, selection, selection_text, hover, pressed, border;
    nx_color foreground, focus;
    float border_width, focus_width;
    if (nx_theme_color(ctx, NX_COLOR_SURFACE_RAISED, &surface) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_SELECTION, &selection) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_SELECTION_TEXT, &selection_text) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_HOVER, &hover) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_PRESSED, &pressed) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_BORDER, &border) != NX_OK ||
        nx_theme_color(ctx, desc->control.disabled ? NX_COLOR_DISABLED_TEXT :
                       NX_COLOR_TEXT, &foreground) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_FOCUS, &focus) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_BORDER_WIDTH, &border_width) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_FOCUS_WIDTH, &focus_width) != NX_OK) {
        result = NX_ERROR_STATE;
        goto tree_item_rollback;
    }
    nx_color normal = state.held ? pressed : state.hovered ? hover : surface;
    nx_color item_text = next_selected ? selection_text : foreground;
    result = nx_fill_rect(ctx, desc->control.bounds,
                          next_selected ? selection : normal);
    if (result != NX_OK) goto tree_item_rollback;
    result = nx_stroke_rect(ctx, desc->control.bounds, border_width, border);
    if (result != NX_OK) goto tree_item_rollback;
    if (desc->has_children) {
        nx_rect bounds = desc->expander_bounds;
        float side = fminf(bounds.width, bounds.height) * 0.35f;
        float cx = bounds.x + bounds.width * 0.5f;
        float cy = bounds.y + bounds.height * 0.5f;
        nx_point a, b, c;
        if (next_expanded) {
            a = (nx_point){cx - side, cy - side * 0.5f};
            b = (nx_point){cx + side, cy - side * 0.5f};
            c = (nx_point){cx, cy + side};
        } else {
            a = (nx_point){cx - side * 0.5f, cy - side};
            b = (nx_point){cx - side * 0.5f, cy + side};
            c = (nx_point){cx + side, cy};
        }
        result = nx_fill_triangle(ctx, a, b, c, item_text);
        if (result != NX_OK) goto tree_item_rollback;
        if (handle_state.focused) {
            result = nx_stroke_rect(ctx, bounds, focus_width, focus);
            if (result != NX_OK) goto tree_item_rollback;
        }
    }
    if (desc->control.label_layout) {
        result = nx_draw_text_layout(ctx, desc->control.label_layout,
            desc->control.label_origin, item_text,
            desc->control.glyph_cache_bytes);
        if (result != NX_OK) goto tree_item_rollback;
    }
    if (state.focused) {
        result = nx_stroke_rect(ctx, desc->control.bounds,
                                focus_width, focus);
        if (result != NX_OK) goto tree_item_rollback;
    }
    nx_semantic_record *node = &ctx->semantics.nodes[
        nx_semantic_find_index(&ctx->semantics, desc->control.id)];
    node->has_tree_visual = 1;
    node->tree_background_command = old_commands;
    node->tree_unselected_color = normal;
    if (next_selected) node->state |= NX_SEM_SELECTED;
    else node->state &= ~NX_SEM_SELECTED;
    if (desc->has_children) {
        if (next_expanded) node->state |= NX_SEM_EXPANDED;
        else node->state &= ~NX_SEM_EXPANDED;
        node->actions = NX_ACTION_FOCUS | NX_ACTION_SELECT |
            (next_expanded ? NX_ACTION_COLLAPSE : NX_ACTION_EXPAND);
        nx_semantic_record *handle = &ctx->semantics.nodes[
            nx_semantic_find_index(&ctx->semantics, desc->expander_id)];
        if (next_expanded) handle->state |= NX_SEM_EXPANDED;
        else handle->state &= ~NX_SEM_EXPANDED;
        handle->actions = NX_ACTION_FOCUS | NX_ACTION_PRESS |
            (next_expanded ? NX_ACTION_COLLAPSE : NX_ACTION_EXPAND);
    }
    if (!desc->control.disabled && state.clicked) {
        uint64_t prior_id = *selected_id;
        if (prior_id && prior_id != desc->control.id) {
            uint32_t prior_index = nx_semantic_find_index(&ctx->semantics,
                                                           prior_id);
            if (prior_index != UINT32_MAX) {
                nx_semantic_record *prior = &ctx->semantics.nodes[prior_index];
                if (prior->role == NX_ROLE_TREE_ITEM &&
                    ((prior->selection_owner == UINT32_MAX && node->selection_owner == UINT32_MAX) ||
                     nx_same_selection_group(prior, node))) {
                    prior->state &= ~NX_SEM_SELECTED;
                    if (prior->has_tree_visual)
                        ctx->commands[prior->tree_background_command].color =
                            prior->tree_unselected_color;
                }
            }
        }
        *selected_id = desc->control.id;
    }
    if (desc->has_children) *expanded = next_expanded;
    *out_state = state;
    return NX_OK;
tree_item_rollback:
    ctx->command_count = old_commands;
    nx_free_owned_bitmaps(ctx, old_bitmaps);
    while (ctx->current_regions.count > old_regions)
        nx_region_rollback_last(ctx,
            ctx->current_regions.data[ctx->current_regions.count - 1u].id);
    while (ctx->semantics.count > old_semantics)
        nx_semantics_rollback_last(ctx,
            ctx->semantics.nodes[ctx->semantics.count - 1u].id, old_text);
    return result;
}

nx_result nx_collapsible_section(nx_context *ctx,
                                  const nx_collapsible_desc *desc,
                                  int *expanded,
                                  nx_interaction_state *out_state) {
    if (!out_state) return NX_ERROR_ARGUMENT;
    *out_state = (nx_interaction_state){0};
    if (!ctx || !desc || !expanded ||
        !nx_valid_container_desc(&desc->container) ||
        !desc->header_id || desc->header_id == desc->container.id ||
        !nx_valid_rect(desc->header_bounds) ||
        desc->header_bounds.width <= 0.0f ||
        desc->header_bounds.height <= 0.0f ||
        desc->header_bounds.x < desc->container.bounds.x ||
        desc->header_bounds.y < desc->container.bounds.y ||
        (double)desc->header_bounds.x + desc->header_bounds.width >
            (double)desc->container.bounds.x + desc->container.bounds.width ||
        (double)desc->header_bounds.y + desc->header_bounds.height >
            (double)desc->container.bounds.y + desc->container.bounds.height)
        return NX_ERROR_ARGUMENT;
    size_t group_text_count = ctx->semantics.text_count;
    size_t old_command_count = ctx->command_count;
    nx_owned_bitmap *old_bitmap = ctx->owned_bitmaps;
    nx_semantic_desc group = {0};
    group.id = desc->container.id;
    group.parent_id = desc->container.parent_id;
    group.role = NX_ROLE_GROUP;
    group.bounds = desc->container.bounds;
    group.name = desc->container.name;
    group.state = *expanded ? NX_SEM_EXPANDED : 0u;
    nx_result result = nx_semantics_add(ctx, &group, NULL);
    if (result != NX_OK) return result;
    size_t header_text_count = ctx->semantics.text_count;
    nx_widget_desc header = {0};
    header.semantic.id = desc->header_id;
    header.semantic.parent_id = desc->container.id;
    header.semantic.role = NX_ROLE_BUTTON;
    header.semantic.bounds = desc->header_bounds;
    header.semantic.name = desc->container.name;
    header.semantic.state = (desc->disabled ? NX_SEM_DISABLED : 0u) |
                            (*expanded ? NX_SEM_EXPANDED : 0u);
    header.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_PRESS |
        (*expanded ? NX_ACTION_COLLAPSE : NX_ACTION_EXPAND);
    header.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    nx_interaction_state state;
    result = nx_widget_register(ctx, &header, &state);
    if (result != NX_OK) {
        nx_semantics_rollback_last(ctx, group.id, group_text_count);
        return result;
    }
    int next = (*expanded != 0) ^ (int)(state.clicked & 1u);
    nx_color surface, header_color, border, foreground;
    float radius, border_width;
    if (nx_theme_color(ctx, NX_COLOR_SURFACE, &surface) != NX_OK ||
        nx_theme_color(ctx, desc->disabled ? NX_COLOR_DISABLED_BACKGROUND :
                       state.held ? NX_COLOR_PRESSED :
                       state.hovered ? NX_COLOR_HOVER :
                       NX_COLOR_SURFACE_RAISED, &header_color) != NX_OK ||
        nx_theme_color(ctx, state.focused ? NX_COLOR_FOCUS : NX_COLOR_BORDER,
                       &border) != NX_OK ||
        nx_theme_color(ctx, desc->disabled ? NX_COLOR_DISABLED_TEXT :
                       NX_COLOR_TEXT, &foreground) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_RADIUS_MD, &radius) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_BORDER_WIDTH, &border_width) != NX_OK) {
        result = NX_ERROR_STATE;
        goto rollback;
    }
    result = nx_fill_round_rect(ctx, desc->container.bounds, radius, surface);
    if (result != NX_OK) goto rollback;
    result = nx_fill_round_rect(ctx, desc->header_bounds, radius, header_color);
    if (result != NX_OK) goto rollback;
    result = nx_stroke_rect(ctx, desc->container.bounds, border_width, border);
    if (result != NX_OK) goto rollback;
    float side = fminf(desc->header_bounds.height, desc->header_bounds.width);
    float cx = desc->header_bounds.x + side * 0.5f;
    float cy = desc->header_bounds.y + desc->header_bounds.height * 0.5f;
    nx_point a, b, c;
    if (next) {
        a = (nx_point){cx - side * 0.16f, cy - side * 0.08f};
        b = (nx_point){cx + side * 0.16f, cy - side * 0.08f};
        c = (nx_point){cx, cy + side * 0.12f};
    } else {
        a = (nx_point){cx - side * 0.08f, cy - side * 0.16f};
        b = (nx_point){cx + side * 0.12f, cy};
        c = (nx_point){cx - side * 0.08f, cy + side * 0.16f};
    }
    result = nx_fill_triangle(ctx, a, b, c, foreground);
    if (result != NX_OK) goto rollback;
    if (desc->container.label_layout) {
        result = nx_draw_text_layout(ctx, desc->container.label_layout,
            desc->container.label_origin, foreground,
            desc->container.glyph_cache_bytes);
        if (result != NX_OK) goto rollback;
    }
    nx_semantic_record *group_node =
        &ctx->semantics.nodes[ctx->semantics.count - 2u];
    nx_semantic_record *header_node =
        &ctx->semantics.nodes[ctx->semantics.count - 1u];
    if (next) {
        group_node->state |= NX_SEM_EXPANDED;
        header_node->state |= NX_SEM_EXPANDED;
    } else {
        group_node->state &= ~NX_SEM_EXPANDED;
        header_node->state &= ~NX_SEM_EXPANDED;
    }
    header_node->actions = NX_ACTION_FOCUS | NX_ACTION_PRESS |
                           (next ? NX_ACTION_COLLAPSE : NX_ACTION_EXPAND);
    *expanded = next;
    *out_state = state;
    return NX_OK;
rollback:
    ctx->command_count = old_command_count;
    nx_free_owned_bitmaps(ctx, old_bitmap);
    nx_region_rollback_last(ctx, desc->header_id);
    nx_semantics_rollback_last(ctx, desc->header_id, header_text_count);
    nx_semantics_rollback_last(ctx, desc->container.id, group_text_count);
    return result;
}

nx_result nx_tab(nx_context *ctx, const nx_control_desc *desc,
                 uint64_t *selected_id, nx_interaction_state *out_state) {
    if (!out_state) return NX_ERROR_ARGUMENT;
    *out_state = (nx_interaction_state){0};
    if (!ctx || !desc || !selected_id || !desc->id || !desc->name.length ||
        !nx_valid_rect(desc->bounds) || desc->bounds.width <= 0.0f ||
        desc->bounds.height <= 0.0f ||
        (desc->label_layout && !desc->glyph_cache_bytes))
        return NX_ERROR_ARGUMENT;
    nx_widget_desc widget = {0};
    widget.semantic.id = desc->id;
    widget.semantic.parent_id = desc->parent_id;
    widget.semantic.role = NX_ROLE_TAB;
    widget.semantic.bounds = desc->bounds;
    widget.semantic.name = desc->name;
    widget.semantic.state = (desc->disabled ? NX_SEM_DISABLED : 0u) |
                            (*selected_id == desc->id ? NX_SEM_SELECTED : 0u);
    widget.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_SELECT;
    widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    size_t old_text_count = ctx->semantics.text_count;
    size_t old_command_count = ctx->command_count;
    nx_owned_bitmap *old_bitmap = ctx->owned_bitmaps;
    nx_interaction_state state;
    nx_result result = nx_widget_register(ctx, &widget, &state);
    if (result != NX_OK) return result;
    int selected = state.clicked || *selected_id == desc->id;
    nx_color chosen, unselected, foreground, focus;
    float radius, focus_width;
    if (nx_theme_color(ctx, desc->disabled ? NX_COLOR_DISABLED_BACKGROUND :
                       NX_COLOR_ACCENT, &chosen) != NX_OK ||
        nx_theme_color(ctx, desc->disabled ? NX_COLOR_DISABLED_BACKGROUND :
                       state.held ? NX_COLOR_PRESSED :
                       state.hovered ? NX_COLOR_HOVER :
                       NX_COLOR_SURFACE_RAISED, &unselected) != NX_OK ||
        nx_theme_color(ctx, desc->disabled ? NX_COLOR_DISABLED_TEXT :
                       NX_COLOR_TEXT, &foreground) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_FOCUS, &focus) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_RADIUS_SM, &radius) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_FOCUS_WIDTH, &focus_width) != NX_OK) {
        result = NX_ERROR_STATE;
        goto tab_rollback;
    }
    result = nx_fill_round_rect(ctx, desc->bounds, radius,
                                 selected ? chosen : unselected);
    if (result != NX_OK) goto tab_rollback;
    if (state.focused) {
        result = nx_stroke_rect(ctx, desc->bounds, focus_width, focus);
        if (result != NX_OK) goto tab_rollback;
    }
    if (desc->label_layout) {
        result = nx_draw_text_layout(ctx, desc->label_layout,
            desc->label_origin, foreground, desc->glyph_cache_bytes);
        if (result != NX_OK) goto tab_rollback;
    }
    nx_semantic_record *node = &ctx->semantics.nodes[ctx->semantics.count - 1u];
    node->has_tab_visual = 1;
    node->tab_background_command = old_command_count;
    node->tab_unselected_color = unselected;
    if (selected) node->state |= NX_SEM_SELECTED;
    else node->state &= ~NX_SEM_SELECTED;
    if (state.clicked) {
        for (size_t i = 0; i + 1u < ctx->semantics.count; ++i) {
            nx_semantic_record *prior = &ctx->semantics.nodes[i];
            if (prior->role != NX_ROLE_TAB ||
                !nx_same_selection_group(prior, node)) continue;
            prior->state &= ~NX_SEM_SELECTED;
            if (prior->has_tab_visual)
                ctx->commands[prior->tab_background_command].color =
                    prior->tab_unselected_color;
        }
        *selected_id = desc->id;
    }
    *out_state = state;
    return NX_OK;
tab_rollback:
    ctx->command_count = old_command_count;
    nx_free_owned_bitmaps(ctx, old_bitmap);
    nx_region_rollback_last(ctx, desc->id);
    nx_semantics_rollback_last(ctx, desc->id, old_text_count);
    return result;
}

static double nx_value_clamp(double value, double minimum, double maximum) {
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

static double nx_value_snap(double value, double minimum, double maximum,
                            double step) {
    value = nx_value_clamp(value, minimum, maximum);
    if (value == minimum || value == maximum) return value;
    double steps = (value - minimum) / step;
    if (!isfinite(steps)) return value;
    return nx_value_clamp(minimum + round(steps) * step, minimum, maximum);
}

static nx_result nx_value_format(double value, char text[64], size_t *length) {
    int count = snprintf(text, 64, "%.17g", value);
    if (count < 0 || count >= 64) return NX_ERROR_LIMIT;
    *length = (size_t)count;
    return NX_OK;
}

static nx_result nx_value_paint(nx_context *ctx,
                                const nx_value_control_desc *desc,
                                double value, int drag,
                                nx_interaction_state state) {
    nx_color surface, accent, focus, text_color;
    float radius, focus_width;
    const nx_control_desc *control = &desc->control;
    if (nx_theme_color(ctx, control->disabled ? NX_COLOR_DISABLED_BACKGROUND :
                       NX_COLOR_SURFACE_RAISED, &surface) != NX_OK ||
        nx_theme_color(ctx, control->disabled ? NX_COLOR_DISABLED_TEXT :
                       NX_COLOR_ACCENT, &accent) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_FOCUS, &focus) != NX_OK ||
        nx_theme_color(ctx, control->disabled ? NX_COLOR_DISABLED_TEXT :
                       NX_COLOR_TEXT, &text_color) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_RADIUS_MD, &radius) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_FOCUS_WIDTH, &focus_width) != NX_OK)
        return NX_ERROR_STATE;
    nx_rect bounds = control->bounds;
    nx_result result = nx_fill_round_rect(ctx, bounds, radius, surface);
    if (result != NX_OK) return result;
    double fraction = (value - desc->minimum) /
                      (desc->maximum - desc->minimum);
    fraction = nx_value_clamp(fraction, 0.0, 1.0);
    if (drag) {
        float marker_width = fminf(4.0f, bounds.width);
        nx_rect marker = {bounds.x + (float)(fraction *
                          (bounds.width - marker_width)), bounds.y,
                          marker_width, bounds.height};
        result = nx_fill_round_rect(ctx, marker, radius, accent);
    } else {
        nx_rect filled = bounds;
        filled.width = (float)(fraction * bounds.width);
        result = nx_fill_round_rect(ctx, filled, radius, accent);
    }
    if (result != NX_OK) return result;
    if (state.focused) {
        result = nx_stroke_rect(ctx, bounds, focus_width, focus);
        if (result != NX_OK) return result;
    }
    if (control->label_layout)
        return nx_draw_text_layout(ctx, control->label_layout,
                                   control->label_origin, text_color,
                                   control->glyph_cache_bytes);
    return NX_OK;
}

static nx_result nx_value_control_run(nx_context *ctx,
                                      const nx_value_control_desc *desc,
                                      double *value, int drag,
                                      nx_interaction_state *out_state) {
    if (!out_state) return NX_ERROR_ARGUMENT;
    *out_state = (nx_interaction_state){0};
    if (!ctx || !desc || !value || !isfinite(*value) ||
        !isfinite(desc->minimum) || !isfinite(desc->maximum) ||
        !isfinite(desc->step) || desc->step <= 0.0 ||
        desc->maximum <= desc->minimum ||
        !isfinite(desc->maximum - desc->minimum) ||
        (drag && (!isfinite(desc->units_per_pixel) ||
                  desc->units_per_pixel <= 0.0)) ||
        !nx_valid_rect(desc->control.bounds) ||
        desc->control.bounds.width <= 0.0f ||
        desc->control.bounds.height <= 0.0f ||
        !desc->control.name.length ||
        (desc->control.label_layout && !desc->control.glyph_cache_bytes))
        return NX_ERROR_ARGUMENT;
    double next = nx_value_clamp(*value, desc->minimum, desc->maximum);
    char initial_text[64];
    size_t initial_length = 0;
    nx_result result = nx_value_format(next, initial_text, &initial_length);
    if (result != NX_OK) return result;
    nx_widget_desc widget = {0};
    widget.semantic.id = desc->control.id;
    widget.semantic.parent_id = desc->control.parent_id;
    widget.semantic.role = NX_ROLE_SLIDER;
    widget.semantic.bounds = desc->control.bounds;
    widget.semantic.name = desc->control.name;
    widget.semantic.value = (nx_utf8_span){initial_text, initial_length};
    widget.semantic.state = desc->control.disabled ? NX_SEM_DISABLED : 0u;
    widget.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_INCREMENT |
                              NX_ACTION_DECREMENT | NX_ACTION_SET_VALUE;
    widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    size_t old_text_count = ctx->semantics.text_count;
    size_t old_command_count = ctx->command_count;
    nx_interaction_state state;
    result = nx_widget_register(ctx, &widget, &state);
    if (result != NX_OK) return result;
    if (!desc->control.disabled) {
        if (drag) {
            double delta = (double)state.drag_delta_x *
                           desc->units_per_pixel;
            next = nx_value_clamp(next + delta,
                                  desc->minimum, desc->maximum);
        } else if (state.pointer_pressed || state.pointer_released ||
                   state.drag_delta_x != 0.0f ||
                   state.drag_delta_y != 0.0f) {
            double fraction = ((double)state.pointer_x -
                               desc->control.bounds.x) /
                              desc->control.bounds.width;
            fraction = nx_value_clamp(fraction, 0.0, 1.0);
            next = desc->minimum +
                   fraction * (desc->maximum - desc->minimum);
        }
        if (state.focused) {
            for (size_t i = 0; i < ctx->frame_events.count; ++i) {
                const nx_event *event = &ctx->frame_events.data[i];
                if (event->type != NX_EVENT_KEY_DOWN) continue;
                double increment = desc->step;
                if (event->key == NX_KEY_PAGE_UP ||
                    event->key == NX_KEY_PAGE_DOWN)
                    increment = fmin(desc->maximum - desc->minimum,
                                     10.0 * desc->step);
                if (event->key == NX_KEY_RIGHT ||
                    event->key == NX_KEY_UP ||
                    event->key == NX_KEY_PAGE_UP)
                    next = nx_value_clamp(next + increment,
                                          desc->minimum, desc->maximum);
                else if (event->key == NX_KEY_LEFT ||
                         event->key == NX_KEY_DOWN ||
                         event->key == NX_KEY_PAGE_DOWN)
                    next = nx_value_clamp(next - increment,
                                          desc->minimum, desc->maximum);
                else if (event->key == NX_KEY_HOME) next = desc->minimum;
                else if (event->key == NX_KEY_END) next = desc->maximum;
            }
        }
    }
    next = nx_value_snap(next, desc->minimum, desc->maximum, desc->step);
    if (next != nx_value_clamp(*value, desc->minimum, desc->maximum)) {
        char updated_text[64];
        size_t updated_length = 0;
        result = nx_value_format(next, updated_text, &updated_length);
        if (result != NX_OK) goto rollback;
        result = nx_semantic_reserve_text(ctx, updated_length + 1u);
        if (result != NX_OK) goto rollback;
        nx_semantic_record *node = &ctx->semantics.nodes[ctx->semantics.count - 1u];
        node->value_offset = nx_semantic_copy_span(&ctx->semantics,
            (nx_utf8_span){updated_text, updated_length});
        node->value_length = updated_length;
    }
    result = nx_value_paint(ctx, desc, next, drag, state);
    if (result != NX_OK) goto rollback;
    *value = next;
    *out_state = state;
    return NX_OK;
rollback:
    ctx->command_count = old_command_count;
    nx_region_rollback_last(ctx, desc->control.id);
    nx_semantics_rollback_last(ctx, desc->control.id, old_text_count);
    return result;
}

nx_result nx_slider(nx_context *ctx, const nx_value_control_desc *desc,
                    double *value, nx_interaction_state *out_state) {
    return nx_value_control_run(ctx, desc, value, 0, out_state);
}

nx_result nx_drag_value(nx_context *ctx, const nx_value_control_desc *desc,
                        double *value, nx_interaction_state *out_state) {
    return nx_value_control_run(ctx, desc, value, 1, out_state);
}

nx_result nx_splitter(nx_context *ctx, const nx_splitter_desc *desc,
                      float *ratio, nx_interaction_state *out_state) {
    if (!out_state) return NX_ERROR_ARGUMENT;
    *out_state = (nx_interaction_state){0};
    if (!ctx || !desc || !ratio || !desc->id || !desc->name.length ||
        !nx_valid_rect(desc->split.divider) ||
        desc->split.divider.width <= 0 || desc->split.divider.height <= 0 ||
        (desc->axis != NX_SPLIT_X && desc->axis != NX_SPLIT_Y) ||
        !isfinite(*ratio) || *ratio < 0 || *ratio > 1 ||
        !isfinite(desc->split.first_extent) ||
        !isfinite(desc->split.second_extent) ||
        desc->split.first_extent < 0 || desc->split.second_extent < 0 ||
        !isfinite(desc->split.effective_ratio) ||
        desc->split.effective_ratio < 0 || desc->split.effective_ratio > 1 ||
        !isfinite(desc->minimum_ratio) || !isfinite(desc->maximum_ratio) ||
        !isfinite(desc->keyboard_step) || desc->keyboard_step < 0)
        return NX_ERROR_ARGUMENT;
    double extent = (double)desc->split.first_extent +
                    (double)desc->split.second_extent;
    if (!(extent > 0) || !isfinite(extent)) return NX_ERROR_ARGUMENT;
    double minimum = desc->minimum_ratio;
    double maximum = desc->maximum_ratio;
    if (minimum == 0 && maximum == 0) maximum = 1;
    if (minimum < 0 || maximum > 1 || minimum > maximum)
        return NX_ERROR_ARGUMENT;
    double step = desc->keyboard_step > 0 ? desc->keyboard_step : 0.05;
    double next = *ratio;
    char text[64];
    size_t length = 0;
    nx_result result = nx_value_format(next, text, &length);
    if (result != NX_OK) return result;
    nx_widget_desc widget = {0};
    widget.semantic.id = desc->id;
    widget.semantic.parent_id = desc->parent_id;
    widget.semantic.role = NX_ROLE_SEPARATOR;
    widget.semantic.bounds = desc->split.divider;
    widget.semantic.name = desc->name;
    widget.semantic.value = (nx_utf8_span){text, length};
    widget.semantic.state = desc->disabled ? NX_SEM_DISABLED : 0u;
    widget.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_INCREMENT |
                              NX_ACTION_DECREMENT | NX_ACTION_SET_VALUE;
    widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    size_t old_text = ctx->semantics.text_count;
    size_t old_commands = ctx->command_count;
    nx_interaction_state state;
    result = nx_widget_register(ctx, &widget, &state);
    if (result != NX_OK) return result;
    if (!desc->disabled) {
        double delta = desc->axis == NX_SPLIT_X ? state.drag_delta_x :
                       state.drag_delta_y;
        int adjusting = delta != 0;
        if (adjusting)
            next = (double)desc->split.effective_ratio + delta / extent;
        if (state.focused) {
            for (size_t i = 0; i < ctx->frame_events.count; ++i) {
                const nx_event *event = &ctx->frame_events.data[i];
                if (event->type != NX_EVENT_KEY_DOWN) continue;
                if ((desc->axis == NX_SPLIT_X && event->key == NX_KEY_RIGHT) ||
                    (desc->axis == NX_SPLIT_Y && event->key == NX_KEY_DOWN)) {
                    if (!adjusting) next = desc->split.effective_ratio;
                    next += step;
                    adjusting = 1;
                } else if ((desc->axis == NX_SPLIT_X && event->key == NX_KEY_LEFT) ||
                           (desc->axis == NX_SPLIT_Y && event->key == NX_KEY_UP)) {
                    if (!adjusting) next = desc->split.effective_ratio;
                    next -= step;
                    adjusting = 1;
                } else if (event->key == NX_KEY_HOME) {
                    next = minimum;
                    adjusting = 1;
                } else if (event->key == NX_KEY_END) {
                    next = maximum;
                    adjusting = 1;
                }
            }
        }
        if (next < minimum) next = minimum;
        if (next > maximum) next = maximum;
    }
    if (next != *ratio) {
        char updated[64];
        size_t updated_length = 0;
        result = nx_value_format(next, updated, &updated_length);
        if (result != NX_OK) goto splitter_rollback;
        result = nx_semantic_reserve_text(ctx, updated_length + 1u);
        if (result != NX_OK) goto splitter_rollback;
        nx_semantic_record *node = &ctx->semantics.nodes[ctx->semantics.count - 1u];
        node->value_offset = nx_semantic_copy_span(&ctx->semantics,
            (nx_utf8_span){updated, updated_length});
        node->value_length = updated_length;
    }
    nx_color surface, detail, focus;
    if (nx_theme_color(ctx, desc->disabled ? NX_COLOR_DISABLED_BACKGROUND :
                       state.held ? NX_COLOR_PRESSED : state.hovered ?
                       NX_COLOR_HOVER : NX_COLOR_SURFACE_RAISED, &surface) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_BORDER, &detail) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_FOCUS, &focus) != NX_OK) {
        result = NX_ERROR_STATE;
        goto splitter_rollback;
    }
    result = nx_fill_rect(ctx, desc->split.divider, surface);
    if (result != NX_OK) goto splitter_rollback;
    nx_point a, b;
    if (desc->axis == NX_SPLIT_X) {
        float x = desc->split.divider.x + desc->split.divider.width * 0.5f;
        a = (nx_point){x, desc->split.divider.y + desc->split.divider.height * 0.25f};
        b = (nx_point){x, desc->split.divider.y + desc->split.divider.height * 0.75f};
    } else {
        float y = desc->split.divider.y + desc->split.divider.height * 0.5f;
        a = (nx_point){desc->split.divider.x + desc->split.divider.width * 0.25f, y};
        b = (nx_point){desc->split.divider.x + desc->split.divider.width * 0.75f, y};
    }
    result = nx_draw_line(ctx, a, b, 1.0f, detail);
    if (result != NX_OK) goto splitter_rollback;
    if (state.focused) {
        result = nx_stroke_rect(ctx, desc->split.divider, 1.0f, focus);
        if (result != NX_OK) goto splitter_rollback;
    }
    *ratio = (float)next;
    *out_state = state;
    return NX_OK;
splitter_rollback:
    ctx->command_count = old_commands;
    nx_region_rollback_last(ctx, desc->id);
    nx_semantics_rollback_last(ctx, desc->id, old_text);
    return result;
}

static nx_result nx_set_semantic_value(nx_context *ctx, uint64_t id,
                                        const char *bytes, size_t length) {
    uint32_t index = nx_semantic_find_index(&ctx->semantics, id);
    if (index == UINT32_MAX) return NX_ERROR_STATE;
    nx_result result = nx_semantic_reserve_text(ctx, length + 1u);
    if (result != NX_OK) return result;
    nx_semantic_record *node = &ctx->semantics.nodes[index];
    node->value_offset = nx_semantic_copy_span(&ctx->semantics,
        (nx_utf8_span){bytes, length});
    node->value_length = length;
    return NX_OK;
}

static nx_result nx_color_picker_part(nx_context *ctx, uint64_t id,
                                       uint64_t parent_id, nx_rect bounds,
                                       nx_utf8_span name,
                                       nx_semantic_role role, int disabled,
                                       nx_interaction_state *out_state) {
    nx_widget_desc widget = {0};
    widget.semantic.id = id;
    widget.semantic.parent_id = parent_id;
    widget.semantic.role = role;
    widget.semantic.bounds = bounds;
    widget.semantic.name = name;
    widget.semantic.state = disabled ? NX_SEM_DISABLED : 0u;
    widget.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_INCREMENT |
                              NX_ACTION_DECREMENT | NX_ACTION_SET_VALUE;
    widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    return nx_widget_register(ctx, &widget, out_state);
}

static double nx_color_unit(double value) {
    return value < 0.0 ? 0.0 : value > 1.0 ? 1.0 : value;
}

nx_result nx_color_picker(nx_context *ctx, const nx_color_picker_desc *desc,
                          nx_color_picker_state *memory, nx_color *color,
                          int *out_changed) {
    if (!out_changed) return NX_ERROR_ARGUMENT;
    *out_changed = 0;
    if (!ctx || !desc || !memory || !color || !nx_valid_color(*color) ||
        (memory->initialized && (!isfinite(memory->hue) ||
                                 memory->hue < 0 || memory->hue > 1)) ||
        !desc->id || !desc->square_id || !desc->hue_id || !desc->preview_id ||
        (desc->show_alpha && !desc->alpha_id) ||
        desc->id == desc->square_id || desc->id == desc->hue_id ||
        desc->id == desc->preview_id ||
        desc->square_id == desc->hue_id ||
        desc->square_id == desc->preview_id ||
        desc->hue_id == desc->preview_id ||
        (desc->show_alpha &&
         (desc->alpha_id == desc->id ||
          desc->alpha_id == desc->square_id ||
          desc->alpha_id == desc->hue_id ||
          desc->alpha_id == desc->preview_id)) ||
        !desc->name.length || !desc->square_name.length ||
        !desc->hue_name.length || !desc->preview_name.length ||
        (desc->show_alpha && !desc->alpha_name.length) ||
        !nx_valid_span(desc->name) || !nx_valid_span(desc->square_name) ||
        !nx_valid_span(desc->hue_name) ||
        !nx_valid_span(desc->preview_name) ||
        (desc->show_alpha && !nx_valid_span(desc->alpha_name)) ||
        !nx_valid_rect(desc->bounds) || !nx_valid_rect(desc->square_bounds) ||
        !nx_valid_rect(desc->hue_bounds) ||
        !nx_valid_rect(desc->preview_bounds) ||
        (desc->show_alpha && !nx_valid_rect(desc->alpha_bounds)) ||
        desc->bounds.width <= 0 || desc->bounds.height <= 0 ||
        desc->square_bounds.width <= 0 ||
        desc->square_bounds.height <= 0 ||
        desc->hue_bounds.width <= 0 || desc->hue_bounds.height <= 0 ||
        desc->preview_bounds.width <= 0 ||
        desc->preview_bounds.height <= 0 ||
        (desc->show_alpha && (desc->alpha_bounds.width <= 0 ||
                              desc->alpha_bounds.height <= 0)))
        return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    nx_hsv hsv;
    nx_result result = nx_color_to_hsv(*color, &hsv);
    if (result != NX_OK) return result;
    if (hsv.s == 0.0f && memory->initialized) hsv.h = memory->hue;
    size_t old_regions = ctx->current_regions.count;
    size_t old_semantics = ctx->semantics.count;
    size_t old_text = ctx->semantics.text_count;
    size_t old_commands = ctx->command_count;
    nx_owned_bitmap *old_bitmaps = ctx->owned_bitmaps;
    nx_semantic_desc group = {0};
    group.id = desc->id;
    group.parent_id = desc->parent_id;
    group.role = NX_ROLE_GROUP;
    group.bounds = desc->bounds;
    group.name = desc->name;
    group.state = desc->disabled ? NX_SEM_DISABLED : 0u;
    result = nx_semantics_add(ctx, &group, NULL);
    if (result != NX_OK) return result;
    nx_interaction_state square, hue_state, alpha_state = {0};
    result = nx_color_picker_part(ctx, desc->square_id, desc->id,
        desc->square_bounds, desc->square_name, NX_ROLE_CUSTOM,
        desc->disabled, &square);
    if (result != NX_OK) goto color_rollback;
    result = nx_color_picker_part(ctx, desc->hue_id, desc->id,
        desc->hue_bounds, desc->hue_name, NX_ROLE_SLIDER,
        desc->disabled, &hue_state);
    if (result != NX_OK) goto color_rollback;
    if (desc->show_alpha) {
        result = nx_color_picker_part(ctx, desc->alpha_id, desc->id,
            desc->alpha_bounds, desc->alpha_name, NX_ROLE_SLIDER,
            desc->disabled, &alpha_state);
        if (result != NX_OK) goto color_rollback;
    }
    nx_semantic_desc preview = {0};
    preview.id = desc->preview_id;
    preview.parent_id = desc->id;
    preview.role = NX_ROLE_IMAGE;
    preview.bounds = desc->preview_bounds;
    preview.name = desc->preview_name;
    result = nx_semantics_add(ctx, &preview, NULL);
    if (result != NX_OK) goto color_rollback;
    int touched = 0;
    if (!desc->disabled) {
        if (square.pointer_pressed || square.pointer_released ||
            square.drag_delta_x != 0 || square.drag_delta_y != 0) {
            hsv.s = (float)nx_color_unit(((double)square.pointer_x -
                       desc->square_bounds.x) / desc->square_bounds.width);
            hsv.v = (float)nx_color_unit(1.0 - ((double)square.pointer_y -
                       desc->square_bounds.y) / desc->square_bounds.height);
            touched = 1;
        }
        if (hue_state.pointer_pressed || hue_state.pointer_released ||
            hue_state.drag_delta_x != 0) {
            hsv.h = (float)nx_color_unit(((double)hue_state.pointer_x -
                       desc->hue_bounds.x) / desc->hue_bounds.width);
            touched = 1;
        }
        if (desc->show_alpha &&
            (alpha_state.pointer_pressed || alpha_state.pointer_released ||
             alpha_state.drag_delta_x != 0)) {
            hsv.a = (float)nx_color_unit(((double)alpha_state.pointer_x -
                       desc->alpha_bounds.x) / desc->alpha_bounds.width);
            touched = 1;
        }
        for (size_t i = 0; i < ctx->frame_events.count; ++i) {
            const nx_event *event = &ctx->frame_events.data[i];
            if (event->type != NX_EVENT_KEY_DOWN ||
                (event->modifiers & (NX_MOD_CONTROL | NX_MOD_ALT | NX_MOD_SUPER)))
                continue;
            double step = event->modifiers & NX_MOD_SHIFT ? 0.1 : 0.01;
            if (square.focused) {
                if (event->key == NX_KEY_LEFT) hsv.s = (float)nx_color_unit(hsv.s - step);
                else if (event->key == NX_KEY_RIGHT) hsv.s = (float)nx_color_unit(hsv.s + step);
                else if (event->key == NX_KEY_UP) hsv.v = (float)nx_color_unit(hsv.v + step);
                else if (event->key == NX_KEY_DOWN) hsv.v = (float)nx_color_unit(hsv.v - step);
                else continue;
                touched = 1;
            } else if (hue_state.focused) {
                if (event->key == NX_KEY_LEFT || event->key == NX_KEY_RIGHT) {
                    double direction = event->key == NX_KEY_RIGHT ? 1.0 : -1.0;
                    double next_hue = fmod((double)hsv.h +
                        direction * (event->modifiers & NX_MOD_SHIFT ? 10.0 : 1.0) /
                        360.0, 1.0);
                    if (next_hue < 0) next_hue += 1.0;
                    hsv.h = (float)next_hue;
                } else if (event->key == NX_KEY_HOME) hsv.h = 0;
                else if (event->key == NX_KEY_END) hsv.h = 359.0f / 360.0f;
                else continue;
                touched = 1;
            } else if (desc->show_alpha && alpha_state.focused) {
                if (event->key == NX_KEY_LEFT) hsv.a = (float)nx_color_unit(hsv.a - step);
                else if (event->key == NX_KEY_RIGHT) hsv.a = (float)nx_color_unit(hsv.a + step);
                else if (event->key == NX_KEY_HOME) hsv.a = 0;
                else if (event->key == NX_KEY_END) hsv.a = 1;
                else continue;
                touched = 1;
            }
        }
    }
    nx_color next = *color;
    if (touched) {
        result = nx_color_from_hsv(hsv, &next);
        if (result != NX_OK) goto color_rollback;
    }
    char hex[10], numeric[64];
    size_t length = 0;
    result = nx_color_format_hex(next, 1, hex, sizeof(hex), &length);
    if (result != NX_OK) goto color_rollback;
    result = nx_set_semantic_value(ctx, desc->id, hex, length);
    if (result != NX_OK) goto color_rollback;
    result = nx_set_semantic_value(ctx, desc->preview_id, hex, length);
    if (result != NX_OK) goto color_rollback;
    int count = snprintf(numeric, sizeof(numeric), "S %.3f V %.3f", hsv.s, hsv.v);
    if (count < 0 || (size_t)count >= sizeof(numeric)) {
        result = NX_ERROR_LIMIT; goto color_rollback;
    }
    result = nx_set_semantic_value(ctx, desc->square_id, numeric, (size_t)count);
    if (result != NX_OK) goto color_rollback;
    result = nx_value_format((double)hsv.h * 360.0, numeric, &length);
    if (result != NX_OK) goto color_rollback;
    result = nx_set_semantic_value(ctx, desc->hue_id, numeric, length);
    if (result != NX_OK) goto color_rollback;
    if (desc->show_alpha) {
        result = nx_value_format(hsv.a, numeric, &length);
        if (result != NX_OK) goto color_rollback;
        result = nx_set_semantic_value(ctx, desc->alpha_id, numeric, length);
        if (result != NX_OK) goto color_rollback;
    }
    nx_color surface, raised, border, focus;
    if (nx_theme_color(ctx, NX_COLOR_SURFACE, &surface) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_SURFACE_RAISED, &raised) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_BORDER, &border) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_FOCUS, &focus) != NX_OK) {
        result = NX_ERROR_STATE; goto color_rollback;
    }
    result = nx_fill_rect(ctx, desc->bounds, surface);
    if (result != NX_OK) goto color_rollback;
    for (unsigned y = 0; y < 12; ++y) {
        for (unsigned x = 0; x < 16; ++x) {
            nx_hsv sample = {hsv.h, (x + 0.5f) / 16.0f,
                             1.0f - (y + 0.5f) / 12.0f, 1.0f};
            nx_color fill;
            result = nx_color_from_hsv(sample, &fill);
            if (result != NX_OK) goto color_rollback;
            nx_rect cell = {desc->square_bounds.x +
                desc->square_bounds.width * x / 16.0f,
                desc->square_bounds.y +
                desc->square_bounds.height * y / 12.0f,
                desc->square_bounds.width / 16.0f + 0.01f,
                desc->square_bounds.height / 12.0f + 0.01f};
            result = nx_fill_rect(ctx, cell, fill);
            if (result != NX_OK) goto color_rollback;
        }
    }
    for (unsigned i = 0; i < 24; ++i) {
        nx_color fill;
        result = nx_color_from_hsv((nx_hsv){(i + 0.5f) / 24.0f, 1, 1, 1}, &fill);
        if (result != NX_OK) goto color_rollback;
        nx_rect cell = {desc->hue_bounds.x + desc->hue_bounds.width * i / 24.0f,
            desc->hue_bounds.y, desc->hue_bounds.width / 24.0f + 0.01f,
            desc->hue_bounds.height};
        result = nx_fill_rect(ctx, cell, fill);
        if (result != NX_OK) goto color_rollback;
    }
    if (desc->show_alpha) {
        result = nx_fill_rect(ctx, desc->alpha_bounds, raised);
        if (result != NX_OK) goto color_rollback;
        for (unsigned i = 0; i < 16; ++i) {
            nx_rect cell = {desc->alpha_bounds.x +
                desc->alpha_bounds.width * i / 16.0f,
                desc->alpha_bounds.y, desc->alpha_bounds.width / 16.0f + 0.01f,
                desc->alpha_bounds.height};
            nx_color fill = next;
            fill.a = (i + 0.5f) / 16.0f;
            result = nx_fill_rect(ctx, cell, fill);
            if (result != NX_OK) goto color_rollback;
        }
    }
    result = nx_fill_rect(ctx, desc->preview_bounds, raised);
    if (result != NX_OK) goto color_rollback;
    result = nx_fill_rect(ctx, desc->preview_bounds, next);
    if (result != NX_OK) goto color_rollback;
    result = nx_stroke_rect(ctx, desc->preview_bounds, 1.0f, border);
    if (result != NX_OK) goto color_rollback;
    nx_rect marker = {desc->square_bounds.x + hsv.s * desc->square_bounds.width - 3,
        desc->square_bounds.y + (1.0f - hsv.v) * desc->square_bounds.height - 3,
        6, 6};
    result = nx_stroke_rect(ctx, marker, 2.0f, focus);
    if (result != NX_OK) goto color_rollback;
    nx_rect hue_marker = {desc->hue_bounds.x + hsv.h * desc->hue_bounds.width - 1,
                           desc->hue_bounds.y, 2, desc->hue_bounds.height};
    result = nx_fill_rect(ctx, hue_marker, focus);
    if (result != NX_OK) goto color_rollback;
    if (desc->show_alpha) {
        nx_rect alpha_marker = {desc->alpha_bounds.x + hsv.a *
            desc->alpha_bounds.width - 1, desc->alpha_bounds.y,
            2, desc->alpha_bounds.height};
        result = nx_fill_rect(ctx, alpha_marker, focus);
        if (result != NX_OK) goto color_rollback;
    }
    if (square.focused || hue_state.focused || alpha_state.focused) {
        nx_rect focus_rect = square.focused ? desc->square_bounds :
            hue_state.focused ? desc->hue_bounds : desc->alpha_bounds;
        result = nx_stroke_rect(ctx, focus_rect, 2.0f, focus);
        if (result != NX_OK) goto color_rollback;
    }
    *out_changed = touched && (next.r != color->r || next.g != color->g ||
        next.b != color->b || next.a != color->a);
    if (*out_changed) *color = next;
    memory->hue = hsv.h;
    memory->initialized = 1;
    return NX_OK;
color_rollback:
    ctx->command_count = old_commands;
    nx_free_owned_bitmaps(ctx, old_bitmaps);
    while (ctx->current_regions.count > old_regions)
        nx_region_rollback_last(ctx,
            ctx->current_regions.data[ctx->current_regions.count - 1u].id);
    while (ctx->semantics.count > old_semantics)
        nx_semantics_rollback_last(ctx,
            ctx->semantics.nodes[ctx->semantics.count - 1u].id, old_text);
    return result;
}

nx_result nx_theme_selector(nx_context *ctx,
                            const nx_theme_selector_desc *desc,
                            nx_theme_kind *selected, int *out_changed) {
    if (!out_changed) return NX_ERROR_ARGUMENT;
    *out_changed = 0;
    if (!ctx || !desc || !selected ||
        *selected < NX_THEME_DARK || *selected > NX_THEME_HIGH_CONTRAST ||
        !desc->id || !desc->name.length || !nx_valid_span(desc->name) ||
        !nx_valid_rect(desc->bounds) ||
        desc->bounds.width <= 0 || desc->bounds.height <= 0 ||
        (desc->fonts && (!isfinite(desc->pixels_per_em) ||
                         desc->pixels_per_em < 1 ||
                         desc->pixels_per_em > 16384 ||
                         !desc->glyph_cache_bytes)))
        return NX_ERROR_ARGUMENT;
    for (unsigned i = 0; i < 3; ++i) {
        if (!desc->option_ids[i] || desc->option_ids[i] == desc->id ||
            !desc->option_names[i].length ||
            !nx_valid_span(desc->option_names[i])) return NX_ERROR_ARGUMENT;
        for (unsigned j = 0; j < i; ++j)
            if (desc->option_ids[i] == desc->option_ids[j])
                return NX_ERROR_ARGUMENT;
    }
    if (ctx->state != 1u) return NX_ERROR_STATE;
    size_t old_regions = ctx->current_regions.count;
    size_t old_semantics = ctx->semantics.count;
    size_t old_text = ctx->semantics.text_count;
    size_t old_commands = ctx->command_count;
    nx_owned_bitmap *old_bitmaps = ctx->owned_bitmaps;
    uint64_t old_focus = ctx->focused_id;
    nx_semantic_desc group = {0};
    group.id = desc->id;
    group.parent_id = desc->parent_id;
    group.role = NX_ROLE_GROUP;
    group.bounds = desc->bounds;
    group.name = desc->name;
    group.state = desc->disabled ? NX_SEM_DISABLED : 0u;
    nx_result result = nx_semantics_add(ctx, &group, NULL);
    if (result != NX_OK) return result;
    nx_rect option_bounds[3];
    nx_interaction_state states[3];
    unsigned next = (unsigned)*selected - 1u;
    int focus_changed = 0;
    for (unsigned i = 0; i < 3; ++i) {
        option_bounds[i] = (nx_rect){desc->bounds.x +
            desc->bounds.width * i / 3.0f, desc->bounds.y,
            desc->bounds.width / 3.0f, desc->bounds.height};
        nx_widget_desc widget = {0};
        widget.semantic.id = desc->option_ids[i];
        widget.semantic.parent_id = desc->id;
        widget.semantic.role = NX_ROLE_RADIO;
        widget.semantic.bounds = option_bounds[i];
        widget.semantic.name = desc->option_names[i];
        widget.semantic.state = (desc->disabled ? NX_SEM_DISABLED : 0u) |
            (i == next ? NX_SEM_CHECKED : 0u);
        widget.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_SELECT |
                                  NX_ACTION_PRESS;
        widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
        result = nx_widget_register(ctx, &widget, &states[i]);
        if (result != NX_OK) goto theme_selector_rollback;
        if (!desc->disabled && states[i].clicked) next = i;
    }
    if (!desc->disabled) {
        int focused = -1;
        for (unsigned i = 0; i < 3; ++i)
            if (states[i].focused) focused = (int)i;
        if (focused >= 0) {
            for (size_t e = 0; e < ctx->frame_events.count; ++e) {
                const nx_event *event = &ctx->frame_events.data[e];
                if (event->type != NX_EVENT_KEY_DOWN ||
                    (event->modifiers & (NX_MOD_CONTROL | NX_MOD_ALT | NX_MOD_SUPER)))
                    continue;
                if (event->key == NX_KEY_RIGHT) next = (next + 1u) % 3u;
                else if (event->key == NX_KEY_LEFT) next = (next + 2u) % 3u;
                else if (event->key == NX_KEY_HOME) next = 0;
                else if (event->key == NX_KEY_END) next = 2;
                else continue;
                focus_changed = 1;
            }
        }
    }
    if (focus_changed) {
        result = nx_focus_set(ctx, desc->option_ids[next]);
        if (result != NX_OK) goto theme_selector_rollback;
    }
    for (unsigned i = 0; i < 3; ++i) {
        uint32_t index = nx_semantic_find_index(&ctx->semantics,
                                                desc->option_ids[i]);
        if (index == UINT32_MAX) { result = NX_ERROR_STATE; goto theme_selector_rollback; }
        nx_semantic_record *node = &ctx->semantics.nodes[index];
        if (i == next) node->state |= NX_SEM_CHECKED;
        else node->state &= ~NX_SEM_CHECKED;
    }
    result = nx_set_semantic_value(ctx, desc->id,
        desc->option_names[next].data, desc->option_names[next].length);
    if (result != NX_OK) goto theme_selector_rollback;
    nx_color focus;
    if (nx_theme_color(ctx, NX_COLOR_FOCUS, &focus) != NX_OK) {
        result = NX_ERROR_STATE; goto theme_selector_rollback;
    }
    for (unsigned i = 0; i < 3; ++i) {
        nx_theme preset;
        result = nx_theme_preset((nx_theme_kind)(i + 1u), &preset);
        if (result != NX_OK) goto theme_selector_rollback;
        nx_rect bounds = option_bounds[i];
        result = nx_fill_rect(ctx, bounds,
                              preset.colors[NX_COLOR_BACKGROUND]);
        if (result != NX_OK) goto theme_selector_rollback;
        nx_rect accent = {bounds.x + 6, bounds.y + bounds.height - 7,
                          fmaxf(0, bounds.width - 12), 3};
        result = nx_fill_rect(ctx, accent, preset.colors[NX_COLOR_ACCENT]);
        if (result != NX_OK) goto theme_selector_rollback;
        if (desc->fonts) {
            nx_text_layout *label = NULL;
            result = nx_text_layout_create(desc->fonts,
                desc->option_names[i].data, desc->option_names[i].length,
                NX_BIDI_AUTO, desc->pixels_per_em,
                fmax(1.0, (double)bounds.width - 12.0), &label);
            if (result != NX_OK) goto theme_selector_rollback;
            nx_point origin = {bounds.x + 6,
                bounds.y + (float)(((double)bounds.height -
                    nx_text_layout_height(label)) * 0.5)};
            result = nx_draw_text_layout(ctx, label, origin,
                preset.colors[NX_COLOR_TEXT], desc->glyph_cache_bytes);
            nx_text_layout_destroy(label);
            if (result != NX_OK) goto theme_selector_rollback;
        }
        if (i == next || (!desc->disabled &&
                          ctx->focused_id == desc->option_ids[i])) {
            result = nx_stroke_rect(ctx, bounds,
                !desc->disabled && ctx->focused_id == desc->option_ids[i]
                    ? 3.0f : 2.0f,
                focus);
            if (result != NX_OK) goto theme_selector_rollback;
        }
    }
    *out_changed = next + 1u != (unsigned)*selected;
    if (*out_changed) *selected = (nx_theme_kind)(next + 1u);
    return NX_OK;
theme_selector_rollback:
    ctx->focused_id = old_focus;
    ctx->command_count = old_commands;
    nx_free_owned_bitmaps(ctx, old_bitmaps);
    while (ctx->current_regions.count > old_regions)
        nx_region_rollback_last(ctx,
            ctx->current_regions.data[ctx->current_regions.count - 1u].id);
    while (ctx->semantics.count > old_semantics)
        nx_semantics_rollback_last(ctx,
            ctx->semantics.nodes[ctx->semantics.count - 1u].id, old_text);
    return result;
}

typedef struct nx_password_mask {
    char *text;
    size_t *source_offsets; /* Mask byte index -> source grapheme boundary. */
    size_t count;
} nx_password_mask;

static void nx_password_mask_destroy(nx_password_mask *mask) {
    free(mask->text);
    free(mask->source_offsets);
    *mask = (nx_password_mask){0};
}

static nx_result nx_password_mask_create(nx_utf8_span source,
                                          nx_password_mask *out_mask) {
    *out_mask = (nx_password_mask){0};
    size_t count = 0;
    for (size_t at = 0; at < source.length; ++count) {
        size_t next = 0;
        nx_result result = nx_grapheme_next(source.data, source.length,
                                            at, &next);
        if (result != NX_OK) return result;
        at = next;
    }
    if (count == SIZE_MAX || count > SIZE_MAX / sizeof(size_t) - 1u)
        return NX_ERROR_LIMIT;
    char *text = malloc(count + 1u);
    if (!text) return NX_ERROR_MEMORY;
    size_t *offsets = malloc((count + 1u) * sizeof(*offsets));
    if (!offsets) { free(text); return NX_ERROR_MEMORY; }
    offsets[0] = 0;
    size_t at = 0;
    for (size_t i = 0; i < count; ++i) {
        size_t next = 0;
        nx_result result = nx_grapheme_next(source.data, source.length,
                                            at, &next);
        if (result != NX_OK) { free(text); free(offsets); return result; }
        text[i] = '*';
        offsets[i + 1u] = next;
        at = next;
    }
    text[count] = '\0';
    *out_mask = (nx_password_mask){text, offsets, count};
    return NX_OK;
}

static nx_result nx_password_mask_offset(const nx_password_mask *mask,
                                          size_t source_offset,
                                          size_t *out_offset) {
    size_t lo = 0, hi = mask->count + 1u;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2u;
        if (mask->source_offsets[mid] < source_offset) lo = mid + 1u;
        else hi = mid;
    }
    if (lo > mask->count || mask->source_offsets[lo] != source_offset)
        return NX_ERROR_STATE;
    *out_offset = lo;
    return NX_OK;
}

static size_t nx_password_mask_nearest(const nx_password_mask *mask,
                                       size_t source_offset, int round_up) {
    size_t lo = 0, hi = mask->count + 1u;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2u;
        if (mask->source_offsets[mid] < source_offset) lo = mid + 1u;
        else hi = mid;
    }
    if (!round_up && lo && (lo > mask->count ||
        mask->source_offsets[lo] != source_offset)) --lo;
    return lo > mask->count ? mask->count : lo;
}

static nx_result nx_field_paint(nx_context *ctx, nx_rect bounds,
                                const nx_font_collection *fonts, double ppem,
                                size_t glyph_cache_bytes,
                                nx_text_buffer *buffer, int multiline,
                                int disabled, int invalid, int focused,
                                const nx_password_mask *mask,
                                nx_utf8_span preedit, int32_t preedit_begin,
                                int32_t preedit_end,
                                const nx_text_caret *preferred_caret,
                                int reveal_caret,
                                double *scroll_x, double *scroll_y,
                                nx_rect *out_caret_rect,
                                int *out_caret_visible) {
    if (out_caret_rect) *out_caret_rect = (nx_rect){0};
    if (out_caret_visible) *out_caret_visible = 0;
    nx_rect drawn_caret = {0};
    nx_text_selection_rect *selection_rects=NULL;
    size_t selection_count=0;
    int caret_drawn = 0;
    const char *bytes = NULL;
    size_t length = 0;
    nx_result result = nx_text_buffer_text(buffer, &bytes, &length);
    if (result != NX_OK) return result;
    size_t anchor = 0, cursor = 0;
    result = nx_text_buffer_selection(buffer, &anchor, &cursor);
    if (result != NX_OK) return result;
    char *composed = NULL;
    nx_password_mask composed_mask = {0};
    const nx_password_mask *layout_mask = mask;
    size_t preedit_start = 0, preedit_finish = 0;
    int composing = focused && preedit.length;
    int show_caret = focused;
    if (composing) {
        size_t start = anchor < cursor ? anchor : cursor;
        size_t end = anchor > cursor ? anchor : cursor;
        if (!preedit.data ||
            preedit.length > SIZE_MAX - (length - (end - start)) - 1u)
            return NX_ERROR_LIMIT;
        size_t composed_length = length - (end - start) + preedit.length;
        composed = malloc(composed_length + 1u);
        if (!composed) return NX_ERROR_MEMORY;
        if (start) memcpy(composed, bytes, start);
        memcpy(composed + start, preedit.data, preedit.length);
        if (end < length)
            memcpy(composed + start + preedit.length, bytes + end,
                   length - end);
        composed[composed_length] = '\0';
        bytes = composed;
        length = composed_length;
        preedit_start = start;
        preedit_finish = start + preedit.length;
        if (preedit_begin < 0 && preedit_end < 0) {
            anchor = cursor = start;
            show_caret = 0;
        } else {
            anchor = start + (size_t)preedit_begin;
            cursor = start + (size_t)preedit_end;
        }
    }
    if (composing && mask) {
        result = nx_password_mask_create((nx_utf8_span){bytes, length},
                                          &composed_mask);
        if (result != NX_OK) {
            nx_private_secure_zero(composed, length + 1u);
            free(composed);
            return result;
        }
        layout_mask = &composed_mask;
    }
    nx_text_layout *layout = NULL;
    double max_width = multiline ? fmax(1.0, (double)bounds.width - 8.0) :
                                  1000000000.0;
    result = nx_text_layout_create(fonts,
                                   layout_mask ? layout_mask->text : bytes,
                                   layout_mask ? layout_mask->count : length,
                                   NX_BIDI_LTR,
                                   ppem, max_width, &layout);
    if (result != NX_OK) {
        nx_password_mask_destroy(&composed_mask);
        if (mask && composed) nx_private_secure_zero(composed, length + 1u);
        free(composed);
        return result;
    }
    if (composing) {
        if (mask) {
            preedit_start = nx_password_mask_nearest(layout_mask,
                preedit_start, 0);
            preedit_finish = nx_password_mask_nearest(layout_mask,
                preedit_finish, 1);
            anchor = nx_password_mask_nearest(layout_mask, anchor, 1);
            cursor = nx_password_mask_nearest(layout_mask, cursor, 1);
        } else {
            while (preedit_start &&
                   !nx_text_layout_is_grapheme_boundary(layout, preedit_start))
                --preedit_start;
            while (preedit_finish < length &&
                   !nx_text_layout_is_grapheme_boundary(layout,
                                                        preedit_finish))
                ++preedit_finish;
            size_t lo = anchor < cursor ? anchor : cursor;
            size_t hi = anchor > cursor ? anchor : cursor;
            while (lo && !nx_text_layout_is_grapheme_boundary(layout, lo))
                --lo;
            while (hi < length &&
                   !nx_text_layout_is_grapheme_boundary(layout, hi)) ++hi;
            if (anchor <= cursor) { anchor = lo; cursor = hi; }
            else { anchor = hi; cursor = lo; }
        }
    }
    double inner_width = fmax(1.0, (double)bounds.width - 8.0);
    double inner_height = fmax(1.0, (double)bounds.height - 8.0);
    double max_scroll_x = multiline ? 0.0 :
                          fmax(0.0, nx_text_layout_width(layout) - inner_width);
    double max_scroll_y = multiline ?
                          fmax(0.0, nx_text_layout_height(layout) - inner_height) :
                          0.0;
    *scroll_x = fmax(0.0, fmin(*scroll_x, max_scroll_x));
    *scroll_y = fmax(0.0, fmin(*scroll_y, max_scroll_y));
    if (mask && !composing) {
        result = nx_password_mask_offset(mask, anchor, &anchor);
        if (result != NX_OK) goto done;
        result = nx_password_mask_offset(mask, cursor, &cursor);
        if (result != NX_OK) goto done;
    }
    nx_text_caret cursor_caret = {0};
    int has_cursor_caret = 0;
    if (show_caret) {
        for (size_t i = 0; i < nx_text_layout_caret_count(layout); ++i) {
            result = nx_text_layout_caret_at(layout, i, &cursor_caret);
            if (result != NX_OK) goto done;
            if (cursor_caret.byte_offset == cursor ||
                (composing && cursor_caret.byte_offset > cursor)) {
                has_cursor_caret = 1;
                break;
            }
        }
        if (!composing && preferred_caret && preferred_caret->byte_offset==cursor) {
            for (size_t i=0;i<nx_text_layout_caret_count(layout);++i) {
                nx_text_caret candidate;
                result=nx_text_layout_caret_at(layout,i,&candidate);
                if (result!=NX_OK) goto done;
                if (candidate.byte_offset==cursor && candidate.line_index==preferred_caret->line_index &&
                    candidate.affinity==preferred_caret->affinity) { cursor_caret=candidate; has_cursor_caret=1; break; }
            }
        }
        if (has_cursor_caret && (reveal_caret || composing)) {
            if (!multiline) {
                if (cursor_caret.x < *scroll_x) *scroll_x = cursor_caret.x;
                else if (cursor_caret.x > *scroll_x + inner_width)
                    *scroll_x = cursor_caret.x - inner_width;
                *scroll_x = fmax(0.0, fmin(*scroll_x, max_scroll_x));
            } else {
                if (cursor_caret.top < *scroll_y)
                    *scroll_y = cursor_caret.top;
                else if (cursor_caret.top + cursor_caret.height >
                         *scroll_y + inner_height)
                    *scroll_y = cursor_caret.top + cursor_caret.height -
                                inner_height;
                *scroll_y = fmax(0.0, fmin(*scroll_y, max_scroll_y));
            }
        }
    }
    double ox = (double)bounds.x + 4.0 - *scroll_x;
    double oy = multiline ? (double)bounds.y + 4.0 - *scroll_y :
                (double)bounds.y +
                ((double)bounds.height - nx_text_layout_height(layout)) * 0.5;
    if (!isfinite(ox) || !isfinite(oy) || fabs(ox) > FLT_MAX ||
        fabs(oy) > FLT_MAX) { result = NX_ERROR_LIMIT; goto done; }
    nx_point origin = {(float)ox, (float)oy};
    nx_color background, foreground, border, selection, selection_text;
    float radius, border_width;
    if (nx_theme_color(ctx, disabled ? NX_COLOR_DISABLED_BACKGROUND :
                       NX_COLOR_SURFACE_RAISED, &background) != NX_OK ||
        nx_theme_color(ctx, disabled ? NX_COLOR_DISABLED_TEXT :
                       NX_COLOR_TEXT, &foreground) != NX_OK ||
        nx_theme_color(ctx, invalid ? NX_COLOR_ERROR :
                       focused ? NX_COLOR_FOCUS : NX_COLOR_BORDER,
                       &border) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_SELECTION, &selection) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_SELECTION_TEXT, &selection_text) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_RADIUS_SM, &radius) != NX_OK ||
        nx_theme_metric(ctx, NX_METRIC_BORDER_WIDTH, &border_width) != NX_OK) {
        result = NX_ERROR_STATE;
        goto done;
    }
    result = nx_fill_round_rect(ctx, bounds, radius, background);
    if (result != NX_OK) goto done;
    result = nx_push_clip(ctx, bounds);
    if (result != NX_OK) goto done;
    if (anchor != cursor) {
        size_t start = anchor < cursor ? anchor : cursor;
        size_t end = anchor > cursor ? anchor : cursor;
        size_t count = 0;
        result = nx_text_layout_selection(layout, start, end, NULL, 0, &count);
        if (result != NX_OK && result != NX_ERROR_LIMIT) goto done;
        nx_text_selection_rect *rects = NULL;
        if (count) {
            if (count > SIZE_MAX / sizeof(*rects)) {
                result = NX_ERROR_LIMIT;
                goto done;
            }
            rects = malloc(count * sizeof(*rects));
            if (!rects) { result = NX_ERROR_MEMORY; goto done; }
            selection_rects=rects;
            result = nx_text_layout_selection(layout, start, end,
                                              rects, count, &count);
            if (result == NX_OK) {
                for (size_t i = 0; i < count; ++i) {
                    nx_rect rect = {origin.x + (float)rects[i].x,
                                    origin.y + (float)rects[i].y,
                                    (float)rects[i].width,
                                    (float)rects[i].height};
                    result = nx_fill_rect(ctx, rect, selection);
                    if (result != NX_OK) break;
                }
            }
            if (result != NX_OK) goto done;
            selection_count=count;
        }
    }
    result = nx_draw_text_layout_impl(ctx, layout, origin, foreground,NULL,0,
                                  glyph_cache_bytes,selection_rects,selection_count,selection_text,ctx->text_raster_scale,1);
    if (result != NX_OK) goto done;
    if (composing) {
        size_t count = 0;
        result = nx_text_layout_selection(layout, preedit_start,
            preedit_finish, NULL, 0, &count);
        if (result != NX_OK && result != NX_ERROR_LIMIT) goto done;
        if (count > SIZE_MAX / sizeof(nx_text_selection_rect)) {
            result = NX_ERROR_LIMIT;
            goto done;
        }
        nx_text_selection_rect *rects = count ?
            malloc(count * sizeof(*rects)) : NULL;
        if (count && !rects) { result = NX_ERROR_MEMORY; goto done; }
        if (count)
            result = nx_text_layout_selection(layout, preedit_start,
                preedit_finish, rects, count, &count);
        if (result == NX_OK) {
            for (size_t i = 0; i < count; ++i) {
                float y = origin.y + (float)(rects[i].y +
                                             rects[i].height) - 1.0f;
                nx_point a = {origin.x + (float)rects[i].x, y};
                nx_point b = {a.x + (float)rects[i].width, y};
                result = nx_draw_line(ctx, a, b, 1.0f, foreground);
                if (result != NX_OK) break;
            }
        }
        free(rects);
        if (result != NX_OK) goto done;
    }
    if (show_caret && has_cursor_caret) {
        nx_point a = {origin.x + (float)cursor_caret.x,
                      origin.y + (float)cursor_caret.top};
        nx_point b = {a.x, a.y + (float)cursor_caret.height};
        result = nx_draw_line(ctx, a, b, 1.0f, foreground);
        if (result != NX_OK) goto done;
        drawn_caret = (nx_rect){a.x, a.y, 1.0f,
                                (float)cursor_caret.height};
        caret_drawn = a.x>=bounds.x && a.x<=bounds.x+bounds.width &&
            b.y>bounds.y && a.y<bounds.y+bounds.height;
    }
    result = nx_pop_clip(ctx);
    if (result != NX_OK) goto done;
    result = nx_stroke_rect(ctx, bounds, border_width, border);
done:
    if (result == NX_OK) {
        if (out_caret_rect) *out_caret_rect = drawn_caret;
        if (out_caret_visible) *out_caret_visible = caret_drawn;
    }
    nx_text_layout_destroy(layout);
    free(selection_rects);
    nx_password_mask_destroy(&composed_mask);
    if (mask && composed) nx_private_secure_zero(composed, length + 1u);
    free(composed);
    return result;
}


nx_result nx_numeric_field_create(double initial_value,
                                  size_t history_bytes,
                                  nx_numeric_field **out_field) {
    if (!out_field) return NX_ERROR_ARGUMENT;
    *out_field = NULL;
    if (!isfinite(initial_value)) return NX_ERROR_ARGUMENT;
    char text[64];
    size_t length = 0;
    nx_result result = nx_value_format(initial_value, text, &length);
    if (result != NX_OK) return result;
    nx_numeric_field *field = (nx_numeric_field *)calloc(1u, sizeof(*field));
    if (!field) return NX_ERROR_MEMORY;
    result = nx_text_buffer_create(text, length, history_bytes,
                                   &field->buffer);
    if (result != NX_OK) {
        free(field);
        return result;
    }
    field->committed = initial_value;
    field->history_bytes = history_bytes;
    *out_field = field;
    return NX_OK;
}

void nx_numeric_field_destroy(nx_numeric_field *field) {
    if (!field) return;
    nx_text_buffer_destroy(field->buffer);
    free(field);
}

nx_result nx_numeric_field_set_value(nx_numeric_field *field, double value) {
    if (!field || !isfinite(value)) return NX_ERROR_ARGUMENT;
    char text[64];
    size_t length = 0;
    nx_result result = nx_value_format(value, text, &length);
    if (result != NX_OK) return result;
    nx_text_buffer *replacement = NULL;
    result = nx_text_buffer_create(text, length, field->history_bytes,
                                   &replacement);
    if (result != NX_OK) return result;
    nx_text_buffer_destroy(field->buffer);
    field->buffer = replacement;
    field->committed = value;
    field->suppress_caret_reveal=0;
    field->pointer=(nx_field_pointer){0};
    field->last_context = NULL;
    field->last_frame_serial = 0;
    field->next_event_index = 0;
    field->scroll_x = field->scroll_y = 0.0;
    return NX_OK;
}

nx_result nx_numeric_field_value(const nx_numeric_field *field,
                                 double *out_value) {
    if (!out_value) return NX_ERROR_ARGUMENT;
    *out_value = 0;
    if (!field) return NX_ERROR_ARGUMENT;
    *out_value = field->committed;
    return NX_OK;
}

nx_result nx_numeric_field_text(const nx_numeric_field *field,
                                nx_utf8_span *out_text) {
    if (!out_text) return NX_ERROR_ARGUMENT;
    *out_text = (nx_utf8_span){0};
    if (!field) return NX_ERROR_ARGUMENT;
    return nx_text_buffer_text(field->buffer, &out_text->data,
                               &out_text->length);
}

/* The pointer's document anchor stays fixed while layout, masking and scroll
   offsets determine the moving end. All changes commit after a valid hit. */
static nx_result nx_field_pointer_hit(nx_context *ctx,
    const nx_text_field_desc *desc, nx_text_buffer *buffer,
    nx_field_pointer *pointer, double *scroll_x, double *scroll_y,
    double x, double y, int press, int extend, int tick,
    nx_text_caret *out_caret, int *out_applied) {
    *out_applied=0;
    if (tick && pointer->tick_serial==ctx->frame_serial) return NX_OK;
    double x0=(double)desc->bounds.x+4, y0=(double)desc->bounds.y+4;
    double width=fmax(1.0,(double)desc->bounds.width-8);
    double height=fmax(1.0,(double)desc->bounds.height-8);
    double x1=x0+width, y1=y0+height;
    nx_region *region=nx_region_find(&ctx->current_regions,desc->id);
    if (region && region->has_clip) {
        x0=fmax(x0,region->clip_x0); y0=fmax(y0,region->clip_y0);
        x1=fmin(x1,region->clip_x1); y1=fmin(y1,region->clip_y1);
    }
    if (x1<=x0 || y1<=y0) { pointer->active=0; return NX_OK; }
    double dx=x<x0?x-x0:x>x1?x-x1:0;
    double dy=y<y0?y-y0:y>y1?y-y1:0;
    if (tick && (desc->multiline?dy==0:dx==0)) {
        pointer->tick_serial=ctx->frame_serial; return NX_OK;
    }
    const char *bytes=NULL; size_t length=0,anchor=0,cursor=0;
    nx_result result=nx_text_buffer_text(buffer,&bytes,&length);
    if (result!=NX_OK) return result;
    result=nx_text_buffer_selection(buffer,&anchor,&cursor);
    if (result!=NX_OK) return result;
    nx_password_mask mask={0}; nx_text_layout *layout=NULL;
    if (desc->password) {
        result=nx_password_mask_create((nx_utf8_span){bytes,length},&mask);
        if (result!=NX_OK) return result;
    }
    result=nx_text_layout_create(desc->fonts,desc->password?mask.text:bytes,
        desc->password?mask.count:length,NX_BIDI_LTR,desc->pixels_per_em,
        desc->multiline?width:1000000000.0,&layout);
    if (result!=NX_OK) goto done;
    double next_x=*scroll_x, next_y=*scroll_y;
    double limit_x=desc->multiline?0:fmax(0.0,nx_text_layout_width(layout)-width);
    double limit_y=desc->multiline?fmax(0.0,nx_text_layout_height(layout)-height):0;
    if (tick) {
        /* Bound catch-up after a paused frame and speed in logical units.
           Event frequency does not multiply the per-frame scroll step. */
        double dt=fmin(ctx->delta_seconds,0.1), speed=desc->pixels_per_em*40;
        next_x+=fmax(-speed,fmin(speed,dx*12))*dt;
        next_y+=fmax(-speed,fmin(speed,dy*12))*dt;
    }
    next_x=fmax(0.0,fmin(limit_x,next_x)); next_y=fmax(0.0,fmin(limit_y,next_y));
    if (tick && next_x==*scroll_x && next_y==*scroll_y) {
        pointer->tick_serial=ctx->frame_serial; goto done;
    }
    double ox=(double)desc->bounds.x+4-next_x;
    double oy=desc->multiline?(double)desc->bounds.y+4-next_y:
        (double)desc->bounds.y+((double)desc->bounds.height-nx_text_layout_height(layout))*.5;
    nx_text_caret hit;
    result=nx_text_layout_hit_test(layout,fmax(x0,fmin(x1,x))-ox,
        fmax(y0,fmin(y1,y))-oy,&hit);
    if (result!=NX_OK) goto done;
    size_t byte=hit.byte_offset;
    if (desc->password) {
        if (byte>mask.count) { result=NX_ERROR_STATE; goto done; }
        byte=mask.source_offsets[byte];
    }
    size_t fixed=press?(extend?anchor:byte):pointer->anchor;
    result=nx_text_buffer_set_selection(buffer,fixed,byte);
    if (result==NX_OK) {
        pointer->anchor=fixed; pointer->active=1; pointer->id=desc->id;
        if (tick) pointer->tick_serial=ctx->frame_serial;
        *scroll_x=next_x; *scroll_y=next_y;
        *out_caret=hit; *out_applied=1;
    }
done:
    nx_text_layout_destroy(layout); nx_password_mask_destroy(&mask); return result;
}

static int nx_field_pointer_event(const nx_event *event) {
    return event->type==NX_EVENT_POINTER_DOWN || event->type==NX_EVENT_POINTER_MOVE ||
        event->type==NX_EVENT_POINTER_UP || event->type==NX_EVENT_TOUCH_UP;
}

nx_result nx_numeric_field_widget(nx_context *ctx,
                                  const nx_numeric_field_desc *desc,
                                  nx_numeric_field *field,
                                  nx_interaction_state *out_state,
                                  int *out_valid) {
    if (!out_state || !out_valid) return NX_ERROR_ARGUMENT;
    *out_state = (nx_interaction_state){0};
    *out_valid = 0;
    if (!ctx || !desc || !field || !desc->fonts || !desc->name.length ||
        !nx_valid_rect(desc->bounds) || desc->bounds.width <= 0.0f ||
        desc->bounds.height <= 0.0f ||
        !isfinite(desc->pixels_per_em) || desc->pixels_per_em < 1.0 ||
        desc->pixels_per_em > 16384.0 ||
        !desc->glyph_cache_bytes || !isfinite(desc->minimum) ||
        !isfinite(desc->maximum) || desc->maximum < desc->minimum)
        return NX_ERROR_ARGUMENT;
    nx_utf8_span draft;
    nx_result result = nx_numeric_field_text(field, &draft);
    if (result != NX_OK) return result;
    double parsed = 0;
    int valid = 0;
    result = nx_numeric_parse(draft.data, draft.length, desc->minimum,
                              desc->maximum, &parsed, &valid);
    if (result != NX_OK) return result;
    nx_widget_desc widget = {0};
    widget.semantic.id = desc->id;
    widget.semantic.parent_id = desc->parent_id;
    widget.semantic.role = NX_ROLE_TEXT_FIELD;
    widget.semantic.bounds = desc->bounds;
    widget.semantic.name = desc->name;
    widget.semantic.value = draft;
    widget.semantic.state = (valid ? 0u : NX_SEM_INVALID) |
                            (desc->disabled ? NX_SEM_DISABLED : 0u);
    widget.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_SET_VALUE;
    widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    size_t old_text_count = ctx->semantics.text_count;
    size_t old_command_count = ctx->command_count;
    nx_owned_bitmap *old_bitmap = ctx->owned_bitmaps;
    unsigned old_clip_depth = ctx->clip_depth;
    nx_interaction_state state;
    result = nx_widget_register(ctx, &widget, &state);
    if (result != NX_OK) return result;
    nx_text_layout *layout = NULL;
    if (field->last_context != ctx ||
        field->last_frame_serial != ctx->frame_serial) {
        if (field->last_context!=ctx || field->last_frame_serial+1u!=ctx->frame_serial)
            field->pointer.active=0;
        field->last_context = ctx;
        field->last_frame_serial = ctx->frame_serial;
        field->next_event_index = 0;
    }
    nx_text_field_desc pointer_desc={0};
    pointer_desc.id=desc->id; pointer_desc.bounds=desc->bounds;
    pointer_desc.fonts=desc->fonts; pointer_desc.pixels_per_em=desc->pixels_per_em;
    if (field->pointer.id!=desc->id || desc->disabled) field->pointer.active=0;
    if (!desc->disabled) {
        while (field->next_event_index < ctx->frame_events.count) {
            const nx_event *event = &ctx->frame_events.data[field->next_event_index];
            const nx_event_route *route=&ctx->frame_events.routes[field->next_event_index];
            if (event->type==NX_EVENT_POINTER_CANCEL ||
                (event->type==NX_EVENT_FOCUS && !event->focused)) field->pointer.active=0;
            if (route->pointer_id==desc->id && nx_field_pointer_event(event)) {
                int press=event->type==NX_EVENT_POINTER_DOWN || event->type==NX_EVENT_TOUCH_UP;
                if (press || (field->pointer.active && field->pointer.id==desc->id)) {
                    nx_text_caret hit; int applied=0;
                    result=nx_field_pointer_hit(ctx,&pointer_desc,field->buffer,&field->pointer,
                        &field->scroll_x,&field->scroll_y,event->x,event->y,press,
                        (event->modifiers&NX_MOD_SHIFT)!=0,0,&hit,&applied);
                    if (result!=NX_OK) goto rollback;
                    if (applied) field->suppress_caret_reveal=1;
                }
                if (event->type==NX_EVENT_POINTER_UP || event->type==NX_EVENT_TOUCH_UP)
                    field->pointer.active=0;
                ++field->next_event_index; continue;
            }
            if (route->focus_id!=desc->id) { ++field->next_event_index; continue; }
            if (event->type == NX_EVENT_KEY_DOWN ||
                event->type == NX_EVENT_TEXT) {
                field->pointer.active=0; field->suppress_caret_reveal=0;
                int handled = 0;
                result = nx_numeric_input_apply_event(field->buffer, event,
                    desc->minimum, desc->maximum, &field->committed,
                    &handled, &valid);
                if (result != NX_OK) goto rollback;
            }
            ++field->next_event_index;
        }
    } else field->next_event_index = ctx->frame_events.count;
    if (field->pointer.active) {
        if (!state.focused || desc->disabled || ctx->pointer_active_id!=desc->id)
            field->pointer.active=0;
        else {
            nx_text_caret hit; int applied=0;
            result=nx_field_pointer_hit(ctx,&pointer_desc,field->buffer,&field->pointer,
                &field->scroll_x,&field->scroll_y,ctx->input.pointer_x,ctx->input.pointer_y,
                0,0,1,&hit,&applied);
            if (result!=NX_OK) goto rollback;
            if (applied) field->suppress_caret_reveal=1;
        }
    }
    result = nx_numeric_field_text(field, &draft);
    if (result != NX_OK) goto rollback;
    result = nx_numeric_parse(draft.data, draft.length, desc->minimum,
                              desc->maximum, &parsed, &valid);
    if (result != NX_OK) goto rollback;
    nx_semantic_record *node = &ctx->semantics.nodes[ctx->semantics.count - 1u];
    int changed = node->value_length != draft.length ||
                  (draft.length &&
                   memcmp(ctx->semantics.text + node->value_offset,
                          draft.data, draft.length) != 0);
    if (changed && draft.length) {
        result = nx_semantic_reserve_text(ctx, draft.length + 1u);
        if (result != NX_OK) goto rollback;
        node->value_offset = nx_semantic_copy_span(&ctx->semantics, draft);
    }
    node->value_length = draft.length;
    if (valid) node->state &= ~NX_SEM_INVALID;
    else node->state |= NX_SEM_INVALID;
    result = nx_field_paint(ctx, desc->bounds, desc->fonts,
                            desc->pixels_per_em, desc->glyph_cache_bytes,
                            field->buffer, 0, desc->disabled, !valid,
                            state.focused, NULL, (nx_utf8_span){0}, 0, 0, NULL, !field->suppress_caret_reveal,
                            &field->scroll_x,
                            &field->scroll_y, NULL, NULL);
    if (result != NX_OK) goto rollback;
    *out_state = state;
    *out_valid = valid;
    return NX_OK;
rollback:
    ctx->command_count = old_command_count;
    ctx->clip_depth = old_clip_depth;
    nx_free_owned_bitmaps(ctx, old_bitmap);
    nx_text_layout_destroy(layout);
    nx_region_rollback_last(ctx, desc->id);
    nx_semantics_rollback_last(ctx, desc->id, old_text_count);
    return result;
}

static nx_result nx_text_field_create_impl(const char *initial,
                                            size_t length,
                                            size_t history_bytes,
                                            int sensitive,
                                            nx_text_field **out_field) {
    if (!out_field) return NX_ERROR_ARGUMENT;
    *out_field = NULL;
    nx_text_field *field = calloc(1u, sizeof(*field));
    if (!field) return NX_ERROR_MEMORY;
    nx_result result = sensitive ?
        nx_text_buffer_create_sensitive(initial, length, &field->buffer) :
        nx_text_buffer_create(initial, length, history_bytes, &field->buffer);
    if (result != NX_OK) { free(field); return result; }
    field->history_bytes = sensitive ? 0u : history_bytes;
    field->sensitive = sensitive;
    *out_field = field;
    return NX_OK;
}

nx_result nx_text_field_create(const char *initial, size_t length,
                               size_t history_bytes,
                               nx_text_field **out_field) {
    return nx_text_field_create_impl(initial, length, history_bytes, 0,
                                     out_field);
}

nx_result nx_text_field_create_password(const char *initial, size_t length,
                                        nx_text_field **out_field) {
    if (initial && length &&
        (memchr(initial, '\n', length) || memchr(initial, '\r', length))) {
        if (out_field) *out_field = NULL;
        return NX_ERROR_ARGUMENT;
    }
    return nx_text_field_create_impl(initial, length, 0u, 1, out_field);
}

void nx_text_field_destroy(nx_text_field *field) {
    if (!field) return;
    nx_text_buffer_destroy(field->buffer);
    if (field->sensitive) nx_private_secure_zero(field, sizeof(*field));
    free(field);
}

nx_result nx_text_field_set_text(nx_text_field *field,
                                 const char *text, size_t length) {
    if (!field) return NX_ERROR_ARGUMENT;
    if (field->sensitive && text && length &&
        (memchr(text, '\n', length) || memchr(text, '\r', length)))
        return NX_ERROR_ARGUMENT;
    nx_text_buffer *replacement = NULL;
    nx_result result = field->sensitive ?
        nx_text_buffer_create_sensitive(text, length, &replacement) :
        nx_text_buffer_create(text, length, field->history_bytes,
                              &replacement);
    if (result != NX_OK) return result;
    nx_text_buffer_destroy(field->buffer);
    field->buffer = replacement;
    field->last_context = NULL;
    field->last_frame_serial = 0;
    field->next_event_index = 0;
    field->submitted = 0;
    field->pointer=(nx_field_pointer){0};
    field->vertical_x_valid = 0;
    field->command_caret_valid = 0;
    field->suppress_caret_reveal = 0;
    field->scroll_x = field->scroll_y = 0.0;
    return NX_OK;
}

nx_result nx_text_field_text(const nx_text_field *field,
                             nx_utf8_span *out_text) {
    if (!out_text) return NX_ERROR_ARGUMENT;
    *out_text = (nx_utf8_span){0};
    if (!field) return NX_ERROR_ARGUMENT;
    return nx_text_buffer_text(field->buffer, &out_text->data,
                               &out_text->length);
}

nx_result nx_text_field_selection(const nx_text_field *field,
                                  size_t *out_anchor, size_t *out_cursor) {
    if (!field) return NX_ERROR_ARGUMENT;
    return nx_text_buffer_selection(field->buffer, out_anchor, out_cursor);
}

static nx_result nx_window_clipboard_read_adapter(void *user, char *bytes,
                                                   size_t capacity,
                                                   size_t *out_length) {
    return nx_window_clipboard_get_text((nx_window *)user, bytes, capacity,
                                        out_length);
}

static nx_result nx_window_clipboard_write_adapter(void *user,
                                                    const char *bytes,
                                                    size_t length) {
    return nx_window_clipboard_set_text((nx_window *)user, bytes, length);
}

nx_result nx_text_clipboard_for_window(nx_window *window,
                                       nx_text_clipboard *out_clipboard) {
    if (!out_clipboard) return NX_ERROR_ARGUMENT;
    *out_clipboard = (nx_text_clipboard){0};
    if (!window) return NX_ERROR_ARGUMENT;
    out_clipboard->user = window;
    out_clipboard->read = nx_window_clipboard_read_adapter;
    out_clipboard->write = nx_window_clipboard_write_adapter;
    return NX_OK;
}

static nx_result nx_field_clipboard_event(nx_text_buffer *buffer,
                                          const nx_text_clipboard *clipboard,
                                          nx_key key, int multiline,
                                          int sensitive) {
    if (key == NX_KEY_C || key == NX_KEY_X) {
        if (!clipboard->write) return NX_OK;
        nx_utf8_span selected;
        nx_result result = nx_text_buffer_selected_span(buffer, &selected);
        if (result != NX_OK || !selected.length) return result;
        if (clipboard->write(clipboard->user, selected.data,
                             selected.length) != NX_OK) return NX_OK;
        return key == NX_KEY_X ? nx_text_buffer_replace(buffer, NULL, 0u) :
                                 NX_OK;
    }
    if (key != NX_KEY_V || !clipboard->read) return NX_OK;
    size_t length = 0;
    if (clipboard->read(clipboard->user, NULL, 0u, &length) != NX_OK ||
        length == SIZE_MAX) return NX_OK;
    char *bytes = malloc(length + 1u);
    if (!bytes) return NX_ERROR_MEMORY;
    size_t actual = 0;
    nx_result result = clipboard->read(clipboard->user, bytes, length + 1u,
                                       &actual);
    if (result == NX_OK && actual <= length) {
        if (!multiline) {
            size_t written = 0;
            for (size_t i = 0; i < actual; ++i) {
                if (bytes[i] == '\r') {
                    if (i + 1u < actual && bytes[i + 1u] == '\n') ++i;
                    bytes[written++] = ' ';
                } else if (bytes[i] == '\n') bytes[written++] = ' ';
                else bytes[written++] = bytes[i];
            }
            actual = written;
        }
        result = nx_text_buffer_replace(buffer, bytes, actual);
    } else result = NX_OK; /* Unavailable clipboard leaves the draft alone. */
    if (sensitive) nx_private_secure_zero(bytes, length + 1u);
    free(bytes);
    return result;
}

static int nx_field_caret_order(nx_text_caret a,nx_text_caret b) {
    if (a.line_index!=b.line_index) return a.line_index<b.line_index?-1:1;
    if (a.x!=b.x) return a.x<b.x?-1:1;
    return a.byte_offset<b.byte_offset?-1:a.byte_offset>b.byte_offset?1:0;
}

static uint32_t nx_field_key_command(const nx_event *event) {
    uint32_t modifiers=event->modifiers,command=0;
    int control=(modifiers&NX_MOD_CONTROL)!=0;
#if defined(__APPLE__)
    if (!(modifiers&NX_MOD_CONTROL) && (modifiers&NX_MOD_SUPER) && !(modifiers&NX_MOD_ALT)) {
        if (event->key==NX_KEY_LEFT) command=NX_TEXT_COMMAND_LINE_LEFT;
        if (event->key==NX_KEY_RIGHT) command=NX_TEXT_COMMAND_LINE_RIGHT;
        if (event->key==NX_KEY_UP) command=NX_TEXT_COMMAND_DOCUMENT_START;
        if (event->key==NX_KEY_DOWN) command=NX_TEXT_COMMAND_DOCUMENT_END;
    } else if (!(modifiers&(NX_MOD_CONTROL|NX_MOD_SUPER)) && (modifiers&NX_MOD_ALT)) {
        if (event->key==NX_KEY_LEFT) command=NX_TEXT_COMMAND_MOVE_WORD_LEFT;
        if (event->key==NX_KEY_RIGHT) command=NX_TEXT_COMMAND_MOVE_WORD_RIGHT;
    } else
#endif
    if (!(modifiers&(NX_MOD_ALT|NX_MOD_SUPER))) switch (event->key) {
    case NX_KEY_LEFT: command=control?NX_TEXT_COMMAND_MOVE_WORD_LEFT:NX_TEXT_COMMAND_MOVE_LEFT; break;
    case NX_KEY_RIGHT: command=control?NX_TEXT_COMMAND_MOVE_WORD_RIGHT:NX_TEXT_COMMAND_MOVE_RIGHT; break;
    case NX_KEY_HOME: command=control?NX_TEXT_COMMAND_DOCUMENT_START:NX_TEXT_COMMAND_LINE_START; break;
    case NX_KEY_END: command=control?NX_TEXT_COMMAND_DOCUMENT_END:NX_TEXT_COMMAND_LINE_END; break;
    case NX_KEY_UP: if (!control) command=NX_TEXT_COMMAND_MOVE_UP; break;
    case NX_KEY_DOWN: if (!control) command=NX_TEXT_COMMAND_MOVE_DOWN; break;
    case NX_KEY_PAGE_UP: if (!control) command=NX_TEXT_COMMAND_MOVE_PAGE_UP; break;
    case NX_KEY_PAGE_DOWN: if (!control) command=NX_TEXT_COMMAND_MOVE_PAGE_DOWN; break;
    default: break;
    }
#if defined(__APPLE__)
    if (!control && !(modifiers&(NX_MOD_ALT|NX_MOD_SUPER))) {
        if (event->key==NX_KEY_HOME) command=(modifiers&NX_MOD_SHIFT)?NX_TEXT_COMMAND_DOCUMENT_START:NX_TEXT_COMMAND_SCROLL_DOCUMENT_START;
        if (event->key==NX_KEY_END) command=(modifiers&NX_MOD_SHIFT)?NX_TEXT_COMMAND_DOCUMENT_END:NX_TEXT_COMMAND_SCROLL_DOCUMENT_END;
        if (event->key==NX_KEY_PAGE_UP) command=NX_TEXT_COMMAND_PAGE_UP;
        if (event->key==NX_KEY_PAGE_DOWN) command=NX_TEXT_COMMAND_PAGE_DOWN;
    }
#endif
    if ((modifiers&NX_MOD_SHIFT) && command && command!=NX_TEXT_COMMAND_SCROLL_DOCUMENT_START &&
        command!=NX_TEXT_COMMAND_SCROLL_DOCUMENT_END) command|=NX_TEXT_COMMAND_EXTEND_SELECTION;
    return command;
}

/* Visual commands use the very same layout and mask as drawing. Keep the
   chosen affinity through soft-wrap/bidi boundaries instead of reducing a
   caret to a byte offset before painting it. */
static nx_result nx_field_visual_command(const nx_text_field_desc *desc,nx_text_field *field,uint32_t value) {
    uint32_t command=nx_text_command_base(value);
    int extend=(value&NX_TEXT_COMMAND_EXTEND_SELECTION)!=0;
    int left=command==NX_TEXT_COMMAND_MOVE_LEFT || command==NX_TEXT_COMMAND_MOVE_WORD_LEFT;
    int right=command==NX_TEXT_COMMAND_MOVE_RIGHT || command==NX_TEXT_COMMAND_MOVE_WORD_RIGHT;
    int word=command==NX_TEXT_COMMAND_MOVE_WORD_LEFT || command==NX_TEXT_COMMAND_MOVE_WORD_RIGHT;
    int vertical=command==NX_TEXT_COMMAND_MOVE_UP || command==NX_TEXT_COMMAND_MOVE_DOWN ||
        command==NX_TEXT_COMMAND_PAGE_UP || command==NX_TEXT_COMMAND_PAGE_DOWN ||
        command==NX_TEXT_COMMAND_MOVE_PAGE_UP || command==NX_TEXT_COMMAND_MOVE_PAGE_DOWN;
    int erase=command==NX_TEXT_COMMAND_DELETE_LINE_START || command==NX_TEXT_COMMAND_DELETE_LINE_END;
    nx_utf8_span source; size_t anchor=0,cursor=0;
    nx_result result=nx_text_field_text(field,&source);
    if (result!=NX_OK) return result;
    result=nx_text_buffer_selection(field->buffer,&anchor,&cursor);
    if (result!=NX_OK) return result;
    if (erase && anchor!=cursor) {
        result=nx_text_buffer_erase_internal(field->buffer,anchor<cursor?anchor:cursor,anchor>cursor?anchor:cursor);
        if (result==NX_OK) field->command_caret_valid=field->vertical_x_valid=0;
        return result;
    }
    nx_password_mask mask={0}; nx_text_layout *layout=NULL;
    size_t display_cursor=cursor,display_anchor=anchor;
    if (desc->password) {
        result=nx_password_mask_create(source,&mask); if (result!=NX_OK) return result;
        result=nx_password_mask_offset(&mask,cursor,&display_cursor);
        if (result==NX_OK) result=nx_password_mask_offset(&mask,anchor,&display_anchor);
        if (result!=NX_OK) goto done;
    }
    result=nx_text_layout_create(desc->fonts,desc->password?mask.text:source.data,
        desc->password?mask.count:source.length,NX_BIDI_LTR,desc->pixels_per_em,
        desc->multiline?fmax(1.0,(double)desc->bounds.width-8.0):1000000000.0,&layout);
    if (result!=NX_OK) goto done;
    nx_text_caret current={0},target={0}; int found=0;
    for (size_t i=0;i<nx_text_layout_caret_count(layout);++i) {
        nx_text_caret c; result=nx_text_layout_caret_at(layout,i,&c); if (result!=NX_OK) goto done;
        if (c.byte_offset!=display_cursor) continue;
        if (!found) { current=c; found=1; }
        if (field->command_caret_valid && field->command_caret.byte_offset==display_cursor &&
            c.line_index==field->command_caret.line_index && c.affinity==field->command_caret.affinity) { current=c; break; }
    }
    if (!found) { result=NX_ERROR_STATE; goto done; }
    target=current;
    double page_scroll=field->scroll_y;
    int page_command=command==NX_TEXT_COMMAND_PAGE_UP || command==NX_TEXT_COMMAND_PAGE_DOWN ||
        command==NX_TEXT_COMMAND_MOVE_PAGE_UP || command==NX_TEXT_COMMAND_MOVE_PAGE_DOWN;
    int scrolling=command>=NX_TEXT_COMMAND_SCROLL_LINE_UP && command<=NX_TEXT_COMMAND_SCROLL_DOCUMENT_END;
    if (page_command || scrolling) {
        double page=fmax(1.0,(double)desc->bounds.height-8.0);
        double limit=desc->multiline?fmax(0.0,nx_text_layout_height(layout)-page):0;
        double amount=scrolling?current.height:page;
        int backward=command==NX_TEXT_COMMAND_PAGE_UP || command==NX_TEXT_COMMAND_MOVE_PAGE_UP || command==NX_TEXT_COMMAND_SCROLL_LINE_UP;
        page_scroll=fmax(0.0,fmin(limit,field->scroll_y+(backward?-amount:amount)));
        if (command==NX_TEXT_COMMAND_SCROLL_DOCUMENT_START) page_scroll=0;
        if (command==NX_TEXT_COMMAND_SCROLL_DOCUMENT_END) page_scroll=limit;
        if (command==NX_TEXT_COMMAND_SCROLL_DOCUMENT_START || command==NX_TEXT_COMMAND_SCROLL_DOCUMENT_END) {
            double horizontal=desc->multiline?0:fmax(0.0,nx_text_layout_width(layout)-fmax(1.0,(double)desc->bounds.width-8.0));
            field->scroll_x=command==NX_TEXT_COMMAND_SCROLL_DOCUMENT_START?0:horizontal;
        }
    }
    if (scrolling || (!extend && (command==NX_TEXT_COMMAND_PAGE_UP || command==NX_TEXT_COMMAND_PAGE_DOWN))) {
        field->scroll_y=page_scroll;
        field->suppress_caret_reveal=1;
        result=NX_OK; goto done;
    }
    if (left || right) {
        int collapse=!extend && anchor!=cursor,best=0;
        for (size_t i=0;i<nx_text_layout_caret_count(layout);++i) {
            nx_text_caret c; result=nx_text_layout_caret_at(layout,i,&c); if (result!=NX_OK) goto done;
            if (collapse) {
                if (c.byte_offset!=display_cursor && c.byte_offset!=display_anchor) continue;
                if (!best || (left?nx_field_caret_order(c,target)<0:nx_field_caret_order(c,target)>0)) { target=c; best=1; }
            } else {
                int order=nx_field_caret_order(c,current);
                size_t source_byte=desc->password?mask.source_offsets[c.byte_offset]:c.byte_offset;
                if ((left?order>=0:order<=0) || (word && !nx_text_buffer_word_boundary_internal(field->buffer,source_byte))) continue;
                if (!best || (left?nx_field_caret_order(c,target)>0:nx_field_caret_order(c,target)<0)) { target=c; best=1; }
            }
        }
    } else if (vertical) {
        int backward=command==NX_TEXT_COMMAND_MOVE_UP || command==NX_TEXT_COMMAND_PAGE_UP || command==NX_TEXT_COMMAND_MOVE_PAGE_UP;
        double x=field->vertical_x_valid?field->vertical_x:current.x;
        double y=current.top+current.height*.5+(backward?-current.height:current.height);
        if (page_command) {
            double page=fmax(1.0,(double)desc->bounds.height-8.0),margin=fmin(page*.5,current.height*.5);
            double relative=fmax(margin,fmin(page-margin,current.top+current.height*.5-field->scroll_y));
            y=page_scroll+relative;
        }
        result=nx_text_layout_hit_test(layout,x,y,&target);
        if (result!=NX_OK) goto done;
    } else {
        nx_text_line line; result=nx_text_layout_line_at(layout,current.line_index,&line); if (result!=NX_OK) goto done;
        int end=command==NX_TEXT_COMMAND_LINE_END || command==NX_TEXT_COMMAND_DELETE_LINE_END;
        int visual_edge=command==NX_TEXT_COMMAND_LINE_LEFT || command==NX_TEXT_COMMAND_LINE_RIGHT;
        int edge_found=0;
        for (size_t i=0;i<nx_text_layout_caret_count(layout);++i) {
            nx_text_caret c; result=nx_text_layout_caret_at(layout,i,&c); if (result!=NX_OK) goto done;
            if (c.line_index!=current.line_index) continue;
            if (visual_edge) {
                if (!edge_found || (command==NX_TEXT_COMMAND_LINE_LEFT?nx_field_caret_order(c,target)<0:nx_field_caret_order(c,target)>0)) { target=c; edge_found=1; }
            } else if (c.byte_offset==(end?line.byte_end:line.byte_start)) {
                if (!edge_found || c.affinity==(end?NX_CARET_UPSTREAM:NX_CARET_DOWNSTREAM)) { target=c; edge_found=1; }
            }
        }
    }
    {
        size_t byte=desc->password?mask.source_offsets[target.byte_offset]:target.byte_offset;
        if (erase) result=nx_text_buffer_erase_internal(field->buffer,byte<cursor?byte:cursor,byte>cursor?byte:cursor);
        else result=nx_text_buffer_set_selection(field->buffer,extend?anchor:byte,byte);
        if (result==NX_OK) {
            field->command_caret=target; field->command_caret_valid=!erase;
            if (page_command) field->scroll_y=page_scroll;
            if (vertical) {
                if (!field->vertical_x_valid) field->vertical_x=current.x;
                field->vertical_x_valid=1;
            } else field->vertical_x_valid=0;
        }
    }
done:
    nx_text_layout_destroy(layout); nx_password_mask_destroy(&mask); return result;
}

static nx_result nx_field_text_command(const nx_text_field_desc *desc,nx_text_field *field,const nx_event *event) {
    uint32_t command=nx_text_command_base(event->text_command);
    int scrolling=command>=NX_TEXT_COMMAND_SCROLL_LINE_UP && command<=NX_TEXT_COMMAND_SCROLL_DOCUMENT_END;
    if (command!=NX_TEXT_COMMAND_COPY && command!=NX_TEXT_COMMAND_PAGE_UP && command!=NX_TEXT_COMMAND_PAGE_DOWN && !scrolling)
        field->suppress_caret_reveal=0;
    if ((event->text_command&NX_TEXT_COMMAND_EXTEND_SELECTION) &&
        (command==NX_TEXT_COMMAND_PAGE_UP || command==NX_TEXT_COMMAND_PAGE_DOWN)) field->suppress_caret_reveal=0;
    if (command==NX_TEXT_COMMAND_NEWLINE && !desc->multiline) { field->submitted=1; return NX_OK; }
    if (command>=NX_TEXT_COMMAND_COPY && command<=NX_TEXT_COMMAND_PASTE) {
        nx_key key=command==NX_TEXT_COMMAND_COPY?NX_KEY_C:command==NX_TEXT_COMMAND_CUT?NX_KEY_X:NX_KEY_V;
        if (desc->password && key!=NX_KEY_V) return NX_OK;
        nx_result result=nx_field_clipboard_event(field->buffer,&desc->clipboard,key,desc->multiline,field->sensitive);
        if (result==NX_OK && key!=NX_KEY_C) field->command_caret_valid=field->vertical_x_valid=0;
        return result;
    }
    if (command==NX_TEXT_COMMAND_MOVE_LEFT || command==NX_TEXT_COMMAND_MOVE_RIGHT ||
        command==NX_TEXT_COMMAND_MOVE_WORD_LEFT || command==NX_TEXT_COMMAND_MOVE_WORD_RIGHT ||
        (command>=NX_TEXT_COMMAND_MOVE_UP && command<=NX_TEXT_COMMAND_LINE_RIGHT) ||
        command==NX_TEXT_COMMAND_DELETE_LINE_START || command==NX_TEXT_COMMAND_DELETE_LINE_END || scrolling ||
        command==NX_TEXT_COMMAND_MOVE_PAGE_UP || command==NX_TEXT_COMMAND_MOVE_PAGE_DOWN)
        return nx_field_visual_command(desc,field,event->text_command);
    int handled=0; nx_result result=nx_text_buffer_apply_event(field->buffer,event,&handled);
    if (result==NX_OK && handled) field->command_caret_valid=field->vertical_x_valid=0;
    return result;
}

static uint64_t nx_field_ime_revision(const nx_text_field *field) {
    const char *bytes = NULL;
    size_t length = 0, anchor = 0, cursor = 0;
    (void)nx_text_buffer_text(field->buffer, &bytes, &length);
    (void)nx_text_buffer_selection(field->buffer, &anchor, &cursor);
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < length; ++i)
        hash = (hash ^ (unsigned char)bytes[i]) * UINT64_C(1099511628211);
    uint64_t values[] = {(uint64_t)length, (uint64_t)anchor,
                         (uint64_t)cursor, (uint64_t)(uintptr_t)field};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
        for (unsigned byte = 0; byte < 8; ++byte)
            hash = (hash ^ (uint8_t)(values[i] >> (byte * 8u))) *
                   UINT64_C(1099511628211);
    return hash ? hash : 1u;
}

static int nx_field_ime_surrounding(nx_utf8_span text,
    size_t anchor, size_t cursor, size_t *out_start, size_t *out_length) {
    if (memchr(text.data, '\0', text.length)) return 0;
    size_t lo = anchor < cursor ? anchor : cursor;
    size_t hi = anchor > cursor ? anchor : cursor;
    if (hi - lo > 4000u) return 0;
    size_t budget = 4000u - (hi - lo);
    size_t before = lo < budget / 2u ? lo : budget / 2u;
    size_t after = text.length - hi < budget - before ?
                   text.length - hi : budget - before;
    size_t extra = budget - before - after;
    if (extra > lo - before) extra = lo - before;
    before += extra;
    size_t start = lo - before;
    size_t end = hi + after;
    while (start < lo &&
           ((unsigned char)text.data[start] & 0xc0u) == 0x80u)
        ++start;
    while (end > hi && end < text.length &&
           ((unsigned char)text.data[end] & 0xc0u) == 0x80u)
        --end;
    *out_start = start;
    *out_length = end - start;
    return 1;
}

nx_result nx_text_field_widget(nx_context *ctx,
                               const nx_text_field_desc *desc,
                               nx_text_field *field,
                               nx_interaction_state *out_state,
                               int *out_valid, int *out_submitted) {
    if (!out_state || !out_valid || !out_submitted) return NX_ERROR_ARGUMENT;
    *out_state = (nx_interaction_state){0};
    *out_valid = 0;
    *out_submitted = 0;
    if (!ctx || !desc || !field || !desc->fonts || !desc->name.length ||
        !nx_valid_rect(desc->bounds) || desc->bounds.width <= 0.0f ||
        desc->bounds.height <= 0.0f ||
        !isfinite(desc->pixels_per_em) || desc->pixels_per_em < 1.0 ||
        desc->pixels_per_em > 16384.0 ||
        !desc->glyph_cache_bytes || (desc->password && desc->multiline) ||
        (!!desc->password != !!field->sensitive))
        return NX_ERROR_ARGUMENT;
    nx_utf8_span draft;
    nx_result result = nx_text_field_text(field, &draft);
    if (result != NX_OK) return result;
    if (!desc->multiline &&
        (memchr(draft.data, '\n', draft.length) ||
         memchr(draft.data, '\r', draft.length))) return NX_ERROR_ARGUMENT;
    nx_password_mask mask = {0};
    int valid = !desc->validate ||
                !!desc->validate(draft, desc->validate_user);
    nx_widget_desc widget = {0};
    widget.semantic.id = desc->id;
    widget.semantic.parent_id = desc->parent_id;
    widget.semantic.role = NX_ROLE_TEXT_FIELD;
    widget.semantic.bounds = desc->bounds;
    widget.semantic.name = desc->name;
    widget.semantic.value = desc->password ? (nx_utf8_span){0} : draft;
    widget.semantic.state = (valid ? 0u : NX_SEM_INVALID) |
                            (desc->disabled ? NX_SEM_DISABLED : 0u) |
                            (desc->password ? NX_SEM_PROTECTED : 0u);
    widget.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_SET_VALUE;
    widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    size_t old_text_count = ctx->semantics.text_count;
    size_t old_command_count = ctx->command_count;
    nx_owned_bitmap *old_bitmap = ctx->owned_bitmaps;
    unsigned old_clip_depth = ctx->clip_depth;
    nx_interaction_state state;
    result = nx_widget_register(ctx, &widget, &state);
    if (result != NX_OK) {
        nx_password_mask_destroy(&mask);
        return result;
    }
    nx_text_layout *layout = NULL;
    char *ime_preedit_copy = NULL;
    size_t ime_preedit_capacity = 0;
    if (field->last_context != ctx ||
        field->last_frame_serial != ctx->frame_serial) {
        if (field->last_context!=ctx || field->last_frame_serial+1u!=ctx->frame_serial)
            field->pointer.active=0;
        field->last_context = ctx;
        field->last_frame_serial = ctx->frame_serial;
        field->next_event_index = 0;
        field->submitted = 0;
    }
    if (field->pointer.id!=desc->id || desc->disabled) field->pointer.active=0;
    if (!desc->disabled) {
        uint64_t ime_batch_target = 0, ime_batch_revision = 0;
        int ime_batch_accepted = 0;
        while (field->next_event_index < ctx->frame_events.count) {
            const nx_event *event = &ctx->frame_events.data[field->next_event_index];
            const nx_event_route *route=&ctx->frame_events.routes[field->next_event_index];
            if (event->type==NX_EVENT_POINTER_CANCEL ||
                (event->type==NX_EVENT_FOCUS && !event->focused)) field->pointer.active=0;
            if (route->pointer_id==desc->id && nx_field_pointer_event(event)) {
                int press=event->type==NX_EVENT_POINTER_DOWN || event->type==NX_EVENT_TOUCH_UP;
                if (press || (field->pointer.active && field->pointer.id==desc->id)) {
                    nx_text_caret hit; int applied=0;
                    result=nx_field_pointer_hit(ctx,desc,field->buffer,&field->pointer,
                        &field->scroll_x,&field->scroll_y,event->x,event->y,press,
                        (event->modifiers&NX_MOD_SHIFT)!=0,0,&hit,&applied);
                    if (result!=NX_OK) goto rollback;
                    if (applied) {
                        field->command_caret=hit; field->command_caret_valid=1;
                        field->vertical_x_valid=0; field->suppress_caret_reveal=1;
                    }
                }
                ime_batch_target=ime_batch_revision=0; ime_batch_accepted=0;
                if (event->type==NX_EVENT_POINTER_UP || event->type==NX_EVENT_TOUCH_UP)
                    field->pointer.active=0;
                ++field->next_event_index; continue;
            }
            if (route->focus_id!=desc->id) {
                ime_batch_target=ime_batch_revision=0; ime_batch_accepted=0;
                ++field->next_event_index; continue;
            }
            if ((event->type==NX_EVENT_KEY_DOWN && event->key!=NX_KEY_C) ||
                event->type==NX_EVENT_TEXT || event->type==NX_EVENT_IME_DELETE_RANGE ||
                event->type==NX_EVENT_IME_DELETE_SURROUNDING ||
                (event->type==NX_EVENT_TEXT_COMMAND &&
                 nx_text_command_base(event->text_command)!=NX_TEXT_COMMAND_COPY)) field->pointer.active=0;
            if ((event->type==NX_EVENT_TEXT_COMMAND && !event->ime_target) ||
                (event->type != NX_EVENT_TEXT &&
                event->type != NX_EVENT_TEXT_COMMAND &&
                event->type != NX_EVENT_IME_DELETE_SURROUNDING &&
                event->type != NX_EVENT_IME_DELETE_RANGE))
                ime_batch_target = ime_batch_revision = 0;
            if (event->type==NX_EVENT_TEXT_COMMAND) {
                if (event->ime_target) {
                    int same=ime_batch_target==event->ime_target && ime_batch_revision==event->ime_revision;
                    if (!same) ime_batch_accepted=desc->ime_window && event->ime_target==desc->id &&
                        event->ime_revision==nx_field_ime_revision(field);
                    ime_batch_target=event->ime_target; ime_batch_revision=event->ime_revision;
                    if (!ime_batch_accepted) { ++field->next_event_index; continue; }
                }
                result=nx_field_text_command(desc,field,event);
                if (result!=NX_OK) goto rollback;
            } else if (!desc->multiline && event->type == NX_EVENT_KEY_DOWN &&
                event->key == NX_KEY_ENTER &&
                !(event->modifiers & (NX_MOD_CONTROL | NX_MOD_ALT |
                                      NX_MOD_SUPER))) {
                field->submitted = 1;
                field->vertical_x_valid = 0;
            } else if (event->type == NX_EVENT_KEY_DOWN &&
                       nx_edit_primary(event->modifiers) &&
                       (event->key == NX_KEY_C || event->key == NX_KEY_X ||
                        event->key == NX_KEY_V)) {
                result = desc->password && event->key != NX_KEY_V ? NX_OK :
                    nx_field_clipboard_event(field->buffer,
                        &desc->clipboard, event->key, desc->multiline,
                        field->sensitive);
                if (result != NX_OK) goto rollback;
                if (event->key!=NX_KEY_C) {
                    field->vertical_x_valid = 0;
                    field->command_caret_valid = 0;
                    field->suppress_caret_reveal=0;
                }
            } else if (event->type==NX_EVENT_KEY_DOWN && nx_field_key_command(event)) {
                nx_event action=*event; action.type=NX_EVENT_TEXT_COMMAND;
                action.text_command=nx_field_key_command(event);
                result=nx_field_text_command(desc,field,&action);
                if (result!=NX_OK) goto rollback;
            } else if (event->type == NX_EVENT_IME_DELETE_SURROUNDING ||
                       event->type == NX_EVENT_IME_DELETE_RANGE) {
                ime_batch_target = event->ime_target;
                ime_batch_revision = event->ime_revision;
                ime_batch_accepted = !desc->password && desc->ime_window &&
                    event->ime_target == desc->id &&
                    event->ime_revision == nx_field_ime_revision(field);
                if (ime_batch_accepted) {
                    result = event->type == NX_EVENT_IME_DELETE_RANGE ?
                        nx_text_buffer_delete_relative(field->buffer,
                            event->ime_index, event->ime_length) :
                        nx_text_buffer_delete_surrounding(field->buffer,
                            event->ime_before, event->ime_after);
                    if (result == NX_ERROR_ARGUMENT) {
                        ime_batch_accepted = 0;
                        result = NX_OK;
                    }
                    if (result != NX_OK) goto rollback;
                    if (ime_batch_accepted) field->suppress_caret_reveal=field->command_caret_valid=field->vertical_x_valid=0;
                }
            } else if (event->type == NX_EVENT_TEXT &&
                       event->ime_target) {
                if (desc->ime_window &&
                    event->ime_target == desc->id &&
                    ((ime_batch_target == event->ime_target &&
                      ime_batch_revision == event->ime_revision &&
                      ime_batch_accepted) ||
                     ((ime_batch_target != event->ime_target ||
                       ime_batch_revision != event->ime_revision) &&
                      event->ime_revision == nx_field_ime_revision(field)))) {
                    ime_batch_target = event->ime_target;
                    ime_batch_revision = event->ime_revision;
                    ime_batch_accepted = 1;
                    int handled = 0;
                    result = nx_text_buffer_apply_event(field->buffer, event,
                                                         &handled);
                    if (result != NX_OK) goto rollback;
                    if (handled) field->suppress_caret_reveal = field->command_caret_valid = field->vertical_x_valid = 0;
                }
            } else if (event->type == NX_EVENT_KEY_DOWN ||
                       event->type == NX_EVENT_TEXT) {
                int handled = 0;
                result = nx_text_buffer_apply_event(field->buffer, event,
                                                     &handled);
                if (result != NX_OK) goto rollback;
                if (handled) field->suppress_caret_reveal = field->command_caret_valid = field->vertical_x_valid = 0;
            }
            ++field->next_event_index;
        }
    } else field->next_event_index = ctx->frame_events.count;
    if (field->pointer.active) {
        if (!state.focused || desc->disabled || ctx->pointer_active_id!=desc->id)
            field->pointer.active=0;
        else {
            nx_text_caret hit; int applied=0;
            result=nx_field_pointer_hit(ctx,desc,field->buffer,&field->pointer,
                &field->scroll_x,&field->scroll_y,ctx->input.pointer_x,ctx->input.pointer_y,
                0,0,1,&hit,&applied);
            if (result!=NX_OK) goto rollback;
            if (applied) {
                field->command_caret=hit; field->command_caret_valid=1;
                field->vertical_x_valid=0; field->suppress_caret_reveal=1;
            }
        }
    }
    result = nx_text_field_text(field, &draft);
    if (result != NX_OK) goto rollback;
    if (desc->password) {
        nx_password_mask_destroy(&mask);
        result = nx_password_mask_create(draft, &mask);
        if (result != NX_OK) goto rollback;
    }
    valid = !desc->validate || !!desc->validate(draft, desc->validate_user);
    nx_semantic_record *node = &ctx->semantics.nodes[ctx->semantics.count - 1u];
    int changed = !desc->password &&
                  (node->value_length != draft.length ||
                  (draft.length &&
                   memcmp(ctx->semantics.text + node->value_offset,
                          draft.data, draft.length) != 0));
    if (changed && draft.length) {
        result = nx_semantic_reserve_text(ctx, draft.length + 1u);
        if (result != NX_OK) goto rollback;
        node->value_offset = nx_semantic_copy_span(&ctx->semantics, draft);
    }
    node->value_length = desc->password ? 0u : draft.length;
    if (valid) node->state &= ~NX_SEM_INVALID;
    else node->state |= NX_SEM_INVALID;
    nx_utf8_span ime_preedit = {0};
    int32_t ime_begin = 0, ime_end = 0;
    if (state.focused && !desc->disabled && desc->ime_window &&
        ((!desc->password && !ctx->ime_active_password &&
          (!ctx->ime_active_window ||
           (ctx->ime_active_window == desc->ime_window &&
            ctx->ime_active_id == desc->id))) ||
         (ctx->ime_active_window == desc->ime_window &&
          ctx->ime_active_id == desc->id &&
          ctx->ime_active_password == !!desc->password))) {
        result = nx_window_get_ime_preedit(desc->ime_window, NULL, 0,
            &ime_preedit.length, &ime_begin, &ime_end);
        if (result == NX_ERROR_UNSUPPORTED) {
            result = NX_OK;
            ime_preedit.length = 0;
        } else if (result != NX_OK) goto rollback;
        if (ime_preedit.length) {
            if (ime_preedit.length == SIZE_MAX) {
                result = NX_ERROR_LIMIT;
                goto rollback;
            }
            ime_preedit_capacity = ime_preedit.length + 1u;
            ime_preedit_copy = malloc(ime_preedit_capacity);
            if (!ime_preedit_copy) { result = NX_ERROR_MEMORY; goto rollback; }
            result = nx_window_get_ime_preedit(desc->ime_window,
                ime_preedit_copy, ime_preedit.length + 1u,
                &ime_preedit.length, &ime_begin, &ime_end);
            if (result != NX_OK) goto rollback;
            ime_preedit.data = ime_preedit_copy;
        }
    }
    nx_rect caret_rect = {0};
    int caret_visible = 0;
    result = nx_field_paint(ctx, desc->bounds, desc->fonts,
                            desc->pixels_per_em, desc->glyph_cache_bytes,
                            field->buffer, desc->multiline, desc->disabled,
                            !valid, state.focused,
                            desc->password ? &mask : NULL,
                            ime_preedit, ime_begin, ime_end,
                            field->command_caret_valid?&field->command_caret:NULL, !field->suppress_caret_reveal, &field->scroll_x,
                            &field->scroll_y, &caret_rect,
                            &caret_visible);
    if (result != NX_OK) goto rollback;
    if (caret_visible && desc->ime_window)
        (void)nx_window_set_ime_rect(desc->ime_window, caret_rect);
    if (state.focused && !desc->disabled && desc->ime_window) {
        if (desc->password) {
            (void)nx_window_set_ime_surrounding_text(desc->ime_window,
                desc->id, nx_field_ime_revision(field), "", 0, 0, 0);
        } else {
            size_t anchor = 0, cursor = 0, start = 0;
            size_t surrounding_length = 0;
            (void)nx_text_buffer_selection(field->buffer, &anchor, &cursor);
            if (nx_field_ime_surrounding(draft, anchor, cursor,
                                         &start, &surrounding_length))
                (void)nx_window_set_ime_surrounding_text(desc->ime_window,
                    desc->id, nx_field_ime_revision(field),
                    draft.data + start, surrounding_length,
                    cursor - start, anchor - start);
            else
                (void)nx_window_set_ime_surrounding_text(desc->ime_window,
                    desc->id, nx_field_ime_revision(field), "", 0, 0, 0);
        }
        ctx->ime_candidate_window = desc->ime_window;
        ctx->ime_candidate_id = desc->id;
        ctx->ime_candidate_revision = nx_field_ime_revision(field);
        ctx->ime_candidate_password = !!desc->password;
    }
    *out_state = state;
    *out_valid = valid;
    *out_submitted = field->submitted;
    if (desc->password && ime_preedit_copy)
        nx_private_secure_zero(ime_preedit_copy, ime_preedit_capacity);
    free(ime_preedit_copy);
    nx_password_mask_destroy(&mask);
    return NX_OK;
rollback:
    if (desc->password && ime_preedit_copy)
        nx_private_secure_zero(ime_preedit_copy, ime_preedit_capacity);
    free(ime_preedit_copy);
    nx_password_mask_destroy(&mask);
    ctx->command_count = old_command_count;
    ctx->clip_depth = old_clip_depth;
    nx_free_owned_bitmaps(ctx, old_bitmap);
    nx_text_layout_destroy(layout);
    nx_region_rollback_last(ctx, desc->id);
    nx_semantics_rollback_last(ctx, desc->id, old_text_count);
    return result;
}

static int nx_compare_u64(const void *left, const void *right) {
    uint64_t a = *(const uint64_t *)left;
    uint64_t b = *(const uint64_t *)right;
    return (a > b) - (a < b);
}

static nx_result nx_search_popup_run(nx_context *ctx,
                                     const nx_search_popup_desc *desc,
                                     nx_search_state *search,
                                     int selectable, int *out_dismissed,
                                     uint64_t *out_activated_id) {
    if (!out_dismissed || !out_activated_id) return NX_ERROR_ARGUMENT;
    *out_dismissed = 0;
    *out_activated_id = 0;
    if (!ctx || !desc || !search || !search->query ||
        !desc->list_id || !desc->empty_id ||
        desc->list_id == desc->empty_id ||
        !desc->list_name.length || !desc->empty_name.length ||
        !nx_valid_span(desc->list_name) ||
        !nx_valid_span(desc->empty_name) ||
        !nx_valid_rect(desc->list_bounds) ||
        desc->list_bounds.width <= 0 || desc->list_bounds.height <= 0 ||
        !isfinite(desc->row_height) || desc->row_height <= 0 ||
        (double)desc->list_bounds.height < desc->row_height ||
        desc->query.multiline || desc->query.disabled ||
        desc->query.parent_id != desc->overlay.panel.id ||
        !desc->query.id || desc->query.id == desc->list_id ||
        desc->query.id == desc->empty_id ||
        desc->overlay.panel.id == desc->list_id ||
        desc->overlay.panel.id == desc->empty_id ||
        desc->overlay.backdrop_id == desc->list_id ||
        desc->overlay.backdrop_id == desc->empty_id ||
        desc->overlay.backdrop_id == desc->query.id ||
        (desc->empty_layout && !desc->empty_glyph_cache_bytes) ||
        (!desc->options && desc->option_count) ||
        desc->option_count > SIZE_MAX / sizeof(size_t) ||
        desc->option_count > SIZE_MAX / sizeof(uint64_t))
        return NX_ERROR_ARGUMENT;
    for (size_t i = 0; i < desc->option_count; ++i) {
        const nx_search_option *option = &desc->options[i];
        if (!option->id || !option->name.length ||
            !nx_valid_span(option->name) ||
            (option->label_layout && !option->glyph_cache_bytes) ||
            option->id == desc->list_id || option->id == desc->empty_id ||
            option->id == desc->query.id ||
            option->id == desc->overlay.panel.id ||
            option->id == desc->overlay.backdrop_id)
            return NX_ERROR_ARGUMENT;
    }
    if (ctx->state != 1u) return NX_ERROR_STATE;
    size_t old_regions = ctx->current_regions.count;
    size_t old_semantics = ctx->semantics.count;
    size_t old_text = ctx->semantics.text_count;
    size_t old_commands = ctx->command_count;
    nx_owned_bitmap *old_bitmaps = ctx->owned_bitmaps;
    unsigned old_clip = ctx->clip_depth;
    uint64_t old_building_overlay = ctx->building_overlay_id;
    uint64_t old_current_overlay = ctx->current_overlay_id;
    uint64_t old_return_focus = ctx->overlay_return_focus_id;
    uint64_t old_focus = ctx->focused_id;
    int old_overlay_built = ctx->overlay_built_this_frame;
    nx_result result = NX_OK;
    size_t *matches = NULL;
    uint64_t *ids = NULL;
    if (desc->option_count) {
        matches = (size_t *)ctx->allocator.allocate(ctx->allocator.user,
                        desc->option_count * sizeof(size_t));
        ids = (uint64_t *)ctx->allocator.allocate(ctx->allocator.user,
                        desc->option_count * sizeof(uint64_t));
        if (!matches || !ids) { result = NX_ERROR_MEMORY; goto search_finish; }
        for (size_t i = 0; i < desc->option_count; ++i)
            ids[i] = desc->options[i].id;
        qsort(ids, desc->option_count, sizeof(uint64_t), nx_compare_u64);
        for (size_t i = 1; i < desc->option_count; ++i)
            if (ids[i] == ids[i - 1u]) {
                result = NX_ERROR_ARGUMENT;
                goto search_finish;
            }
    }
    int dismissed = 0;
    result = nx_popup_begin(ctx, &desc->overlay, &dismissed);
    if (result != NX_OK) goto search_rollback;
    nx_interaction_state query_state;
    int query_valid = 0, submitted = 0;
    result = nx_text_field_widget(ctx, &desc->query, search->query,
                                  &query_state, &query_valid, &submitted);
    if (result != NX_OK) goto search_rollback;
    nx_utf8_span query;
    result = nx_text_field_text(search->query, &query);
    if (result != NX_OK) goto search_rollback;
    size_t match_count = 0;
    result = nx_search_filter(desc->options, desc->option_count, query,
                              matches, desc->option_count, &match_count);
    if (result != NX_OK) goto search_rollback;
    double rows_double = floor((double)desc->list_bounds.height / desc->row_height);
    size_t visible_count = rows_double >= (double)match_count ?
        match_count : (size_t)rows_double;
    if (visible_count == 0 && match_count) visible_count = 1;
    uint64_t active = search->active_id;
    uint64_t prior_active = active;
    size_t active_pos = SIZE_MAX;
    for (size_t i = 0; i < match_count; ++i)
        if (desc->options[matches[i]].id == active &&
            !desc->options[matches[i]].disabled) {
            active_pos = i;
            break;
        }
    if (active_pos == SIZE_MAX && selectable && search->selected_id)
        for (size_t i = 0; i < match_count; ++i)
            if (desc->options[matches[i]].id == search->selected_id &&
                !desc->options[matches[i]].disabled) {
                active_pos = i;
                break;
            }
    if (active_pos == SIZE_MAX)
        for (size_t i = 0; i < match_count; ++i)
            if (!desc->options[matches[i]].disabled) {
                active_pos = i;
                break;
            }
    if (active_pos != SIZE_MAX)
        active = desc->options[matches[active_pos]].id;
    else active = 0;
    int ensure_visible = active != prior_active;
    if (query_state.focused && !dismissed) {
        for (size_t e = 0; e < ctx->frame_events.count; ++e) {
            const nx_event *event = &ctx->frame_events.data[e];
            int direction=0;
            if (event->type==NX_EVENT_KEY_DOWN &&
                !(event->modifiers&(NX_MOD_CONTROL|NX_MOD_ALT|NX_MOD_SUPER)))
                direction=event->key==NX_KEY_DOWN?1:event->key==NX_KEY_UP?-1:0;
            else if (event->type==NX_EVENT_TEXT_COMMAND &&
                (!event->ime_target || event->ime_target==desc->query.id))
                direction=event->text_command==NX_TEXT_COMMAND_MOVE_DOWN?1:
                    event->text_command==NX_TEXT_COMMAND_MOVE_UP?-1:0;
            if (!direction || !match_count) continue;
            size_t next = active_pos;
            for (size_t attempts = 0; attempts < match_count; ++attempts) {
                next = direction > 0 ?
                    (next == SIZE_MAX || next + 1u == match_count ? 0 : next + 1u) :
                    (next == SIZE_MAX || next == 0 ? match_count - 1u : next - 1u);
                if (!desc->options[matches[next]].disabled) {
                    active_pos = next;
                    active = desc->options[matches[next]].id;
                    ensure_visible = 1;
                    break;
                }
            }
        }
    }
    size_t first_visible = search->first_visible;
    if (match_count <= visible_count) first_visible = 0;
    else if (first_visible > match_count - visible_count)
        first_visible = match_count - visible_count;
    if (ensure_visible && active_pos != SIZE_MAX && visible_count) {
        if (active_pos < first_visible) first_visible = active_pos;
        else if (active_pos >= first_visible + visible_count)
            first_visible = active_pos - visible_count + 1u;
    }
    nx_widget_desc list = {0};
    list.semantic.id = desc->list_id;
    list.semantic.parent_id = desc->overlay.panel.id;
    list.semantic.role = NX_ROLE_LIST;
    list.semantic.bounds = desc->list_bounds;
    list.semantic.name = desc->list_name;
    list.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_SCROLLABLE;
    nx_interaction_state list_state;
    result = nx_widget_register(ctx, &list, &list_state);
    if (result != NX_OK) goto search_rollback;
    if (match_count > visible_count && list_state.wheel_y != 0) {
        double steps = fmin((double)match_count,
                            ceil(fabs((double)list_state.wheel_y) * 3.0));
        size_t move = (size_t)steps;
        if (list_state.wheel_y > 0)
            first_visible = move > first_visible ? 0 : first_visible - move;
        else {
            size_t last = match_count - visible_count;
            first_visible = move > last - first_visible ? last : first_visible + move;
        }
    }
    nx_color surface, selected, hover, foreground, selected_text, disabled_text;
    if (nx_theme_color(ctx, NX_COLOR_SURFACE_RAISED, &surface) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_SELECTION, &selected) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_HOVER, &hover) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_TEXT, &foreground) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_SELECTION_TEXT, &selected_text) != NX_OK ||
        nx_theme_color(ctx, NX_COLOR_DISABLED_TEXT, &disabled_text) != NX_OK) {
        result = NX_ERROR_STATE;
        goto search_rollback;
    }
    result = nx_fill_rect(ctx, desc->list_bounds, surface);
    if (result != NX_OK) goto search_rollback;
    result = nx_push_clip(ctx, desc->list_bounds);
    if (result != NX_OK) goto search_rollback;
    uint64_t activated = 0;
    if (!match_count) {
        nx_semantic_desc empty = {0};
        empty.id = desc->empty_id;
        empty.parent_id = desc->list_id;
        empty.role = NX_ROLE_TEXT;
        empty.bounds = desc->list_bounds;
        empty.name = desc->empty_name;
        result = nx_semantics_add(ctx, &empty, NULL);
        if (result != NX_OK) goto search_rollback;
        {
            nx_text_layout *automatic = NULL;
            const nx_text_layout *label = desc->empty_layout;
            size_t cache_bytes = desc->empty_glyph_cache_bytes;
            nx_point origin = {desc->list_bounds.x + desc->empty_offset.x,
                               desc->list_bounds.y + desc->empty_offset.y};
            if (!label) {
                result = nx_text_layout_create(desc->query.fonts,
                    desc->empty_name.data, desc->empty_name.length,
                    NX_BIDI_AUTO, desc->query.pixels_per_em,
                    fmax(1.0, (double)desc->list_bounds.width - 16.0),
                    &automatic);
                if (result != NX_OK) goto search_rollback;
                label = automatic;
                cache_bytes = desc->query.glyph_cache_bytes;
                origin = (nx_point){desc->list_bounds.x + 8.0f,
                    desc->list_bounds.y +
                    (float)(((double)desc->list_bounds.height -
                             nx_text_layout_height(label)) * 0.5)};
            }
            result = nx_draw_text_layout(ctx, label, origin, disabled_text,
                                          cache_bytes);
            nx_text_layout_destroy(automatic);
            if (result != NX_OK) goto search_rollback;
        }
    }
    for (size_t row = 0; row < visible_count; ++row) {
        const nx_search_option *option = &desc->options[matches[first_visible + row]];
        double y = (double)desc->list_bounds.y + row * (double)desc->row_height;
        nx_rect bounds = {desc->list_bounds.x, (float)y,
                          desc->list_bounds.width, desc->row_height};
        nx_widget_desc widget = {0};
        widget.semantic.id = option->id;
        widget.semantic.parent_id = desc->list_id;
        widget.semantic.role = NX_ROLE_LIST_ITEM;
        widget.semantic.bounds = bounds;
        widget.semantic.name = option->name;
        widget.semantic.state = (option->disabled ? NX_SEM_DISABLED : 0u) |
            ((selectable ? search->selected_id : active) == option->id ?
             NX_SEM_SELECTED : 0u);
        widget.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_PRESS |
                                  NX_ACTION_SELECT;
        widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
        nx_interaction_state row_state;
        result = nx_widget_register(ctx, &widget, &row_state);
        if (result != NX_OK) goto search_rollback;
        if (!option->disabled && row_state.clicked && !dismissed) {
            activated = option->id;
            active = option->id;
        }
        nx_color background = option->id == active ? selected :
                              row_state.hovered ? hover : surface;
        result = nx_fill_rect(ctx, bounds, background);
        if (result != NX_OK) goto search_rollback;
        nx_text_layout *automatic = NULL;
        const nx_text_layout *label = option->label_layout;
        size_t cache_bytes = option->glyph_cache_bytes;
        nx_point origin = {bounds.x + option->label_offset.x,
                           bounds.y + option->label_offset.y};
        if (!label) {
            result = nx_text_layout_create(desc->query.fonts,
                option->name.data, option->name.length, NX_BIDI_AUTO,
                desc->query.pixels_per_em,
                fmax(1.0, (double)bounds.width - 16.0), &automatic);
            if (result != NX_OK) goto search_rollback;
            label = automatic;
            cache_bytes = desc->query.glyph_cache_bytes;
            origin = (nx_point){bounds.x + 8.0f,
                bounds.y + (float)(((double)bounds.height -
                                    nx_text_layout_height(label)) * 0.5)};
        }
        result = nx_draw_text_layout(ctx, label, origin,
            option->disabled ? disabled_text :
            option->id == active ? selected_text : foreground, cache_bytes);
        nx_text_layout_destroy(automatic);
        if (result != NX_OK) goto search_rollback;
    }
    if (!dismissed && submitted && query_valid && active) activated = active;
    result = nx_pop_clip(ctx);
    if (result != NX_OK) goto search_rollback;
    result = nx_overlay_end(ctx);
    if (result != NX_OK) goto search_rollback;
    if (activated) {
        result = nx_overlay_close(ctx);
        if (result != NX_OK) goto search_rollback;
        dismissed = 1;
    }
    search->active_id = active;
    search->first_visible = first_visible;
    if (activated && selectable) search->selected_id = activated;
    *out_dismissed = dismissed;
    *out_activated_id = activated;
    goto search_finish;
search_rollback:
    ctx->clip_depth = old_clip;
    ctx->command_count = old_commands;
    nx_free_owned_bitmaps(ctx, old_bitmaps);
    while (ctx->current_regions.count > old_regions)
        nx_region_rollback_last(ctx,
            ctx->current_regions.data[ctx->current_regions.count - 1u].id);
    while (ctx->semantics.count > old_semantics)
        nx_semantics_rollback_last(ctx,
            ctx->semantics.nodes[ctx->semantics.count - 1u].id, old_text);
    ctx->building_overlay_id = old_building_overlay;
    ctx->current_overlay_id = old_current_overlay;
    ctx->overlay_return_focus_id = old_return_focus;
    ctx->focused_id = old_focus;
    ctx->overlay_built_this_frame = old_overlay_built;
search_finish:
    if (ids) ctx->allocator.deallocate(ctx->allocator.user, ids,
                        desc->option_count * sizeof(uint64_t));
    if (matches) ctx->allocator.deallocate(ctx->allocator.user, matches,
                        desc->option_count * sizeof(size_t));
    return result;
}

nx_result nx_command_palette(nx_context *ctx,
                             const nx_search_popup_desc *desc,
                             nx_search_state *search, int *out_dismissed,
                             uint64_t *out_activated_id) {
    return nx_search_popup_run(ctx, desc, search, 0,
                               out_dismissed, out_activated_id);
}

nx_result nx_searchable_select(nx_context *ctx,
                               const nx_search_popup_desc *desc,
                               nx_search_state *search, int *out_dismissed,
                               uint64_t *out_activated_id) {
    return nx_search_popup_run(ctx, desc, search, 1,
                               out_dismissed, out_activated_id);
}

nx_result nx_push_clip(nx_context *ctx, nx_rect rect) {
    if (!ctx || !nx_valid_rect(rect)) return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    if (ctx->clip_depth == NX_MAX_CLIP_DEPTH) return NX_ERROR_LIMIT;
    nx_result result = nx_reserve_command(ctx);
    if (result != NX_OK) return result;
    nx_clip_bounds clip = {rect.x, rect.y,
                           (double)rect.x + rect.width,
                           (double)rect.y + rect.height};
    if (ctx->clip_depth) {
        nx_clip_bounds parent = ctx->clip_stack[ctx->clip_depth - 1u];
        clip.x0 = fmax(clip.x0, parent.x0);
        clip.y0 = fmax(clip.y0, parent.y0);
        clip.x1 = fmax(clip.x0, fmin(clip.x1, parent.x1));
        clip.y1 = fmax(clip.y0, fmin(clip.y1, parent.y1));
    }
    ctx->commands[ctx->command_count++] = (nx_command){NX_COMMAND_PUSH_CLIP, rect, {0}};
    ctx->clip_stack[ctx->clip_depth] = clip;
    ++ctx->clip_depth;
    return NX_OK;
}

nx_result nx_pop_clip(nx_context *ctx) {
    if (!ctx || ctx->state != 1u || !ctx->clip_depth) return NX_ERROR_STATE;
    nx_result result = nx_reserve_command(ctx);
    if (result != NX_OK) return result;
    ctx->commands[ctx->command_count++] = (nx_command){NX_COMMAND_POP_CLIP, {0}, {0}};
    --ctx->clip_depth;
    return NX_OK;
}

nx_result nx_frame_commands(const nx_context *ctx,
                            const nx_command **out_commands, size_t *out_count) {
    if (!out_commands || !out_count) return NX_ERROR_ARGUMENT;
    *out_commands = NULL;
    *out_count = 0;
    if (!ctx || ctx->state != 2u) return NX_ERROR_STATE;
    *out_commands = ctx->command_count ? ctx->commands : NULL;
    *out_count = ctx->command_count;
    return NX_OK;
}

static nx_rect nx_plot_area(nx_rect bounds, int labels) {
    float left = fminf(labels ? 52.0f : 24.0f, bounds.width * 0.26f);
    float right = fminf(12.0f, bounds.width * 0.08f);
    float top = fminf(12.0f, bounds.height * 0.10f);
    float bottom = fminf(labels ? 30.0f : 16.0f, bounds.height * 0.26f);
    return (nx_rect){bounds.x + left, bounds.y + top,
                     bounds.width - left - right,
                     bounds.height - top - bottom};
}

static nx_point nx_plot_map(nx_rect area, const nx_plot_desc *desc,
                            nx_series_point point) {
    double x = (point.x - desc->x_min) / (desc->x_max - desc->x_min);
    double y = (fmax(desc->y_min, fmin(desc->y_max, point.y)) - desc->y_min) /
               (desc->y_max - desc->y_min);
    return (nx_point){area.x + (float)(x * area.width),
                      area.y + (float)((1.0 - y) * area.height)};
}

static nx_result nx_plot_tick_label(nx_context *ctx,
                                     const nx_plot_desc *desc, nx_rect area,
                                     double value, float anchor_x,
                                     float anchor_y, int x_axis,
                                     nx_color color) {
    char label[48];
    int length = snprintf(label, sizeof(label), "%.4g", value);
    if (length <= 0 || (size_t)length >= sizeof(label)) return NX_ERROR_LIMIT;
    nx_text_layout *layout = NULL;
    nx_result result = nx_text_layout_create(desc->fonts, label, (size_t)length,
        NX_BIDI_LTR, desc->pixels_per_em, 256.0, &layout);
    if (result != NX_OK) return result;
    float width = (float)nx_text_layout_width(layout);
    float height = (float)nx_text_layout_height(layout);
    nx_point origin = x_axis ?
        (nx_point){anchor_x - width * 0.5f, anchor_y + 5.0f} :
        (nx_point){area.x - width - 6.0f, anchor_y - height * 0.5f};
    result = nx_draw_text_layout(ctx, layout, origin, color,
                                 desc->glyph_cache_bytes);
    nx_text_layout_destroy(layout);
    return result;
}


typedef struct nx_plot_segment_plan {
    const nx_plot_layer *layer;
    nx_series_envelope *buckets;
    size_t count;
} nx_plot_segment_plan;
static nx_color nx_plot_gradient(const nx_plot_layer *l,double t) {
    nx_color a=l->fill_top,b=l->fill_bottom,c={0};
    double alpha=(1-t)*a.a+t*b.a;c.a=(float)alpha;
    if(alpha>0){c.r=(float)(((1-t)*a.r*a.a+t*b.r*b.a)/alpha);
        c.g=(float)(((1-t)*a.g*a.a+t*b.g*b.a)/alpha);
        c.b=(float)(((1-t)*a.b*a.a+t*b.b*b.a)/alpha);}
    return c;
}
static void nx_plot_marks(const nx_series_envelope *b,nx_series_mark marks[4]) {
    marks[0]=b->first;marks[1]=b->minimum;marks[2]=b->maximum;marks[3]=b->last;
    for(unsigned a=1;a<4;++a){nx_series_mark m=marks[a];unsigned j=a;
        while(j&&marks[j-1].index>m.index){marks[j]=marks[j-1];--j;}marks[j]=m;}
}
static nx_result nx_plot_multi_paint(nx_context *ctx,const nx_plot_desc *d,nx_rect area,
    const nx_plot_multi_desc *multi,const nx_plot_segment_plan *plans,size_t plan_count) {
    uint32_t w=(uint32_t)fmin(4096,ceil((double)area.width*multi->raster_scale));
    uint32_t h=(uint32_t)fmin(4096,ceil((double)area.height*multi->raster_scale));
    for(size_t layer=0;layer<multi->layer_count;++layer) {
        const nx_plot_layer *l=&multi->layers[layer];nx_owned_bitmap *image=NULL;
        if(l->filled&&!multi->hide_fills&&!d->disabled) {
            size_t bytes=(size_t)w*h*4;
            size_t allocation=sizeof(nx_owned_bitmap)+bytes-1;
            image=ctx->allocator.allocate(ctx->allocator.user,allocation);
            if(!image)return NX_ERROR_MEMORY;
            image->allocation_size=allocation;image->next=ctx->owned_bitmaps;ctx->owned_bitmaps=image;
            /* Ordered disjoint segments assign each raster column at most
               once. Store its vertical span, then write contiguous rows. */
            uint16_t lower[4096]={0},upper[4096]={0};
            int first_x=(int)w,last_x=0,first_y=(int)h,last_y=0;
            memset(image->pixels,0,bytes);
            nx_point base=nx_plot_map(area,d,(nx_series_point){d->x_min,l->baseline});
            for(size_t segment=0;segment<plan_count;++segment) {
                const nx_plot_segment_plan *p=&plans[segment];if(p->layer!=l)continue;
                nx_point previous={0};int has_previous=0;
                for(size_t i=0;i<p->count;++i) {
                    nx_series_mark marks[4];nx_plot_marks(&p->buckets[i],marks);
                    for(unsigned j=0;j<4;++j){
                        if(j&&marks[j].index==marks[j-1].index)continue;
                        nx_point point=nx_plot_map(area,d,marks[j].point);
                        if(has_previous&&point.x>previous.x){
                            int x0=(int)ceil(((double)previous.x-area.x)*w/area.width-.5);
                            int x1=(int)ceil(((double)point.x-area.x)*w/area.width-.5);
                            x0=x0<0?0:x0;x1=x1>(int)w?(int)w:x1;
                            for(int x=x0;x<x1;++x){
                                double logical=area.x+(x+.5)*area.width/w;
                                double fraction=(logical-previous.x)/(point.x-previous.x);
                                double y=previous.y+(point.y-previous.y)*fraction;
                                double low=fmin(y,base.y),high=fmax(y,base.y);
                                int y0=(int)ceil((low-area.y)*h/area.height-.5);
                                int y1=(int)ceil((high-area.y)*h/area.height-.5);
                                y0=y0<0?0:y0;y1=y1>(int)h?(int)h:y1;
                                lower[x]=(uint16_t)y0;upper[x]=(uint16_t)y1;
                                if(y1>y0){
                                    if(x<first_x)first_x=x;if(x+1>last_x)last_x=x+1;
                                    if(y0<first_y)first_y=y0;if(y1>last_y)last_y=y1;
                                }
                            }
                        }
                        previous=point;has_previous=1;
                    }
                }
            }
            /* Empty, baseline-only and sparse histories keep the memset fast
               path; shade only the occupied rectangle, including its gaps. */
            for(int row=first_y;row<last_y;++row){
                nx_color c=nx_plot_gradient(l,(row+.5)/h);
                uint8_t color[4]={(uint8_t)floorf(c.r*255+.5f),(uint8_t)floorf(c.g*255+.5f),
                    (uint8_t)floorf(c.b*255+.5f),(uint8_t)floorf(c.a*255+.5f)};
                uint8_t *dest=image->pixels+(size_t)row*w*4;
                for(int x=first_x;x<last_x;++x){
                    int covered=row>=lower[x]&&row<upper[x];
                    dest[4*x]=covered?color[0]:0;dest[4*x+1]=covered?color[1]:0;
                    dest[4*x+2]=covered?color[2]:0;dest[4*x+3]=covered?color[3]:0;
                }
            }
            nx_result r=nx_draw_bitmap(ctx,area,(nx_bitmap_view){image->pixels,w,h,(size_t)w*4,NX_BITMAP_RGBA8},(nx_color){1,1,1,1});
            if(r!=NX_OK)return r;
        }
        nx_color line=l->line;
        if(d->disabled) {nx_result r=nx_theme_color(ctx,NX_COLOR_DISABLED_TEXT,&line);if(r!=NX_OK)return r;}
        for(size_t segment=0;segment<plan_count;++segment) {
            const nx_plot_segment_plan *p=&plans[segment];if(p->layer!=l)continue;
            nx_point previous={0};int has_previous=0;
            for(size_t i=0;i<p->count;++i) {
                nx_series_mark marks[4];nx_plot_marks(&p->buckets[i],marks);
                for(unsigned j=0;j<4;++j){if(j&&marks[j].index==marks[j-1].index)continue;
                    nx_point point=nx_plot_map(area,d,marks[j].point);
                    nx_result r=nx_draw_line(ctx,has_previous?previous:point,point,1.5f,line);
                    if(r!=NX_OK)return r;previous=point;has_previous=1;}
            }
        }
    }
    return NX_OK;
}
static nx_result nx_plot_paint(nx_context *ctx, const nx_plot_desc *desc,
                               nx_rect area, const nx_series_envelope *buckets,
                               size_t bucket_count, const nx_plot_state *state,
    const nx_plot_multi_desc *multi,const nx_plot_segment_plan *plans,size_t plan_count) {
    nx_color surface, border, grid, line, text, focus;
    nx_result result = nx_theme_color(ctx, desc->disabled ?
        NX_COLOR_DISABLED_BACKGROUND : NX_COLOR_SURFACE, &surface);
    if (result != NX_OK) return result;
    if ((result = nx_theme_color(ctx, NX_COLOR_BORDER, &border)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_TEXT_MUTED, &text)) != NX_OK ||
        (result = nx_theme_color(ctx, desc->disabled ? NX_COLOR_DISABLED_TEXT :
                                  NX_COLOR_ACCENT, &line)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_FOCUS, &focus)) != NX_OK)
        return result;
    grid = border;
    grid.a *= 0.65f;
    if ((result = nx_fill_rect(ctx, desc->bounds, surface)) != NX_OK) return result;
    if ((result = nx_stroke_rect(ctx, desc->bounds, 1.0f, border)) != NX_OK)
        return result;
    for (unsigned tick = 0; tick <= 4u; ++tick) {
        float x = area.x + area.width * (float)tick / 4.0f;
        float y = area.y + area.height * (float)tick / 4.0f;
        if ((result = nx_draw_line(ctx, (nx_point){x, area.y},
                                   (nx_point){x, area.y + area.height},
                                   1.0f, grid)) != NX_OK ||
            (result = nx_draw_line(ctx, (nx_point){area.x, y},
                                   (nx_point){area.x + area.width, y},
                                   1.0f, grid)) != NX_OK) return result;
    }
    if ((result = nx_draw_line(ctx, (nx_point){area.x, area.y},
                               (nx_point){area.x, area.y + area.height},
                               1.5f, text)) != NX_OK ||
        (result = nx_draw_line(ctx, (nx_point){area.x, area.y + area.height},
                               (nx_point){area.x + area.width,
                                          area.y + area.height},
                               1.5f, text)) != NX_OK) return result;
    if (desc->fonts) {
        for (unsigned tick = 0; tick <= 4u; ++tick) {
            double fraction = (double)tick / 4.0;
            result = nx_plot_tick_label(ctx, desc, area,
                desc->x_min + fraction * (desc->x_max - desc->x_min),
                area.x + area.width * (float)fraction,
                area.y + area.height, 1, text);
            if (result != NX_OK) return result;
            result = nx_plot_tick_label(ctx, desc, area,
                desc->y_max - fraction * (desc->y_max - desc->y_min),
                area.x, area.y + area.height * (float)fraction, 0, text);
            if (result != NX_OK) return result;
        }
    }
    result = nx_push_clip(ctx, area);
    if (result != NX_OK) return result;
    if(multi) result=nx_plot_multi_paint(ctx,desc,area,multi,plans,plan_count);
    nx_point previous = {0};
    int has_previous = 0;
    for (size_t i = 0; !multi && i < bucket_count && result == NX_OK; ++i) {
        nx_series_mark marks[4] = {buckets[i].first, buckets[i].minimum,
                                    buckets[i].maximum, buckets[i].last};
        for (unsigned a = 1; a < 4u; ++a) {
            nx_series_mark chosen = marks[a];
            unsigned b = a;
            while (b && marks[b - 1u].index > chosen.index) {
                marks[b] = marks[b - 1u];
                --b;
            }
            marks[b] = chosen;
        }
        for (unsigned a = 0; a < 4u && result == NX_OK; ++a) {
            if (a && marks[a].index == marks[a - 1u].index) continue;
            nx_point point = nx_plot_map(area, desc, marks[a].point);
            result = nx_draw_line(ctx, has_previous ? previous : point,
                                  point, 1.5f, line);
            previous = point;
            has_previous = 1;
        }
    }
    if (result == NX_OK && state->cursor_valid && !desc->disabled) {
        float x = area.x + (float)((state->cursor_x - desc->x_min) /
                                  (desc->x_max - desc->x_min) * area.width);
        result = nx_draw_line(ctx, (nx_point){x, area.y},
                              (nx_point){x, area.y + area.height},
                              1.0f, focus);
    }
    nx_result pop_result = nx_pop_clip(ctx);
    if (result == NX_OK) result = pop_result;
    if (state->interaction.focused && result == NX_OK)
        result = nx_stroke_rect(ctx, desc->bounds, 2.0f, focus);
    return result;
}

static nx_result nx_plot_core(nx_context *ctx, const nx_plot_desc *desc,
                  nx_plot_state *state,const nx_plot_multi_desc *multi) {
    if (!ctx || !desc || !state || !desc->id || !desc->name.length ||
        !nx_valid_span(desc->name) ||
        !nx_valid_rect(desc->bounds) || desc->bounds.width <= 0.0f ||
        desc->bounds.height <= 0.0f || !isfinite(desc->x_min) ||
        !isfinite(desc->x_max) || !isfinite(desc->y_min) ||
        !isfinite(desc->y_max) || !isfinite(desc->x_max - desc->x_min) ||
        !isfinite(desc->y_max - desc->y_min) ||
        desc->x_max <= desc->x_min || desc->y_max <= desc->y_min ||
        (desc->fonts && (!isfinite(desc->pixels_per_em) ||
                         desc->pixels_per_em < 1.0 ||
                         desc->pixels_per_em > 16384.0 ||
                         !desc->glyph_cache_bytes))) return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    nx_rect area = nx_plot_area(desc->bounds, desc->fonts != NULL);
    if (!nx_valid_rect(area) || area.width <= 0.0f || area.height <= 0.0f)
        return NX_ERROR_ARGUMENT;
    uint32_t columns = area.width >= 16384.0f ? 16384u :
                       (uint32_t)ceilf(area.width);
    if (!columns) columns = 1u;
    if (!desc->scratch || desc->scratch_capacity < columns)
        return NX_ERROR_LIMIT;
    size_t bucket_count=0,plan_count=0;nx_plot_segment_plan plans[32];
    nx_result result=NX_OK;
    char description[640]="Time series plot";size_t description_length=16;
    if(multi) {
        if(!multi->layers||!multi->layer_count||multi->layer_count>8||
            !nx_valid_span(multi->x_unit)||!nx_valid_span(multi->y_unit)||
            multi->x_unit.length>24||multi->y_unit.length>24||
            !isfinite(multi->raster_scale)||multi->raster_scale<.125f||multi->raster_scale>8)
            return NX_ERROR_ARGUMENT;
        size_t samples=0,total_segments=0,filled=0;
        for(size_t k=0;k<multi->layer_count;++k) {
            const nx_plot_layer *l=&multi->layers[k];
            if(!l->name.length||l->name.length>64||!nx_valid_span(l->name)||!l->segments||!l->segment_count||
                l->segment_count>32||!nx_valid_color(l->line)||!nx_valid_color(l->fill_top)||
                !nx_valid_color(l->fill_bottom)||!isfinite(l->baseline))return NX_ERROR_ARGUMENT;
            total_segments+=l->segment_count;filled+=l->filled&&!multi->hide_fills&&!desc->disabled;
            if(total_segments>32)return NX_ERROR_LIMIT;
            for(size_t j=0;j<l->segment_count;++j){
                if(l->segments[j].count>1000000-samples)return NX_ERROR_LIMIT;
                samples+=l->segments[j].count;
            }
            int n=snprintf(description+description_length,sizeof(description)-description_length,
                "; %.*s",(int)l->name.length,l->name.data);
            if(n<0||(size_t)n>=sizeof(description)-description_length)return NX_ERROR_LIMIT;
            description_length+=(size_t)n;
        }
        if(desc->scratch_capacity<(size_t)columns*total_segments)return NX_ERROR_LIMIT;
        double w=ceil((double)area.width*multi->raster_scale),h=ceil((double)area.height*multi->raster_scale);
        if(filled&&(w>4096||h>4096||w*h*filled>4194304))return NX_ERROR_LIMIT;
        int n=snprintf(description+description_length,sizeof(description)-description_length,
            "; x [%.*s]; y [%.*s]",(int)multi->x_unit.length,multi->x_unit.data?multi->x_unit.data:"",
            (int)multi->y_unit.length,multi->y_unit.data?multi->y_unit.data:"");
        if(n<0||(size_t)n>=sizeof(description)-description_length)return NX_ERROR_LIMIT;
        description_length+=(size_t)n;
        for(size_t k=0;k<multi->layer_count;++k) {
            const nx_plot_layer *l=&multi->layers[k];double last=desc->x_min;int has_last=0;
            for(size_t j=0;j<l->segment_count;++j){
                nx_plot_segment_plan *p=&plans[plan_count];p->layer=l;p->buckets=desc->scratch+plan_count*columns;
                result=nx_series_decimate(&l->segments[j],desc->x_min,desc->x_max,columns,p->buckets,columns,&p->count);
                if(result!=NX_OK)return result;
                if(p->count){if(has_last&&p->buckets[0].first.point.x<last)return NX_ERROR_ARGUMENT;
                    last=p->buckets[p->count-1].last.point.x;has_last=1;}
                ++plan_count;
            }
        }
    } else {
        result=nx_series_decimate(&desc->series,desc->x_min,desc->x_max,columns,desc->scratch,desc->scratch_capacity,&bucket_count);
        if(result!=NX_OK)return result;
    }

    nx_plot_state next = *state;
    if (!next.cursor_valid || !isfinite(next.cursor_x)) {
        next.cursor_valid = 0;
        next.cursor_x = desc->x_min;
    }
    next.cursor_x = fmax(desc->x_min, fmin(desc->x_max, next.cursor_x));
    nx_widget_desc widget = {0};
    widget.semantic.id = desc->id;
    widget.semantic.parent_id = desc->parent_id;
    widget.semantic.role = NX_ROLE_CUSTOM;
    widget.semantic.bounds = desc->bounds;
    widget.semantic.name = desc->name;
    widget.semantic.description = (nx_utf8_span){description,description_length};
    widget.semantic.state = desc->disabled ? NX_SEM_DISABLED : 0;
    widget.semantic.actions = NX_ACTION_FOCUS;
    widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    nx_owned_bitmap *old_bitmap=ctx->owned_bitmaps;
    size_t old_text_count = ctx->semantics.text_count;
    size_t old_command_count = ctx->command_count;
    unsigned old_clip_depth = ctx->clip_depth;
    result = nx_widget_register(ctx, &widget, &next.interaction);
    if (result != NX_OK) return result;
    if (!desc->disabled) {
        if (next.interaction.hovered || next.interaction.held) {
            float pointer_x = ctx->input.pointer_x;
            if (pointer_x >= area.x && pointer_x <= area.x + area.width) {
                next.cursor_x = desc->x_min +
                    ((double)(pointer_x - area.x) / (double)area.width) *
                    (desc->x_max - desc->x_min);
                next.cursor_valid = 1;
            }
        }
        if (next.interaction.focused) {
            double step = (desc->x_max - desc->x_min) / (double)columns;
            if (ctx->action_keys_pressed[NX_KEY_LEFT]) {
                next.cursor_x = fmax(desc->x_min, next.cursor_x - step);
                next.cursor_valid = 1;
            }
            if (ctx->action_keys_pressed[NX_KEY_RIGHT]) {
                next.cursor_x = fmin(desc->x_max, next.cursor_x + step);
                next.cursor_valid = 1;
            }
        }
    }
    if (next.cursor_valid) {
        char value[48];
        int length = snprintf(value, sizeof(value), "x=%.6g", next.cursor_x);
        if (length <= 0 || (size_t)length >= sizeof(value)) {
            result = NX_ERROR_LIMIT;
            goto plot_rollback;
        }
        result = nx_semantic_reserve_text(ctx, (size_t)length + 1u);
        if (result != NX_OK) goto plot_rollback;
        nx_semantic_record *node = &ctx->semantics.nodes[ctx->semantics.count - 1u];
        node->value_offset = nx_semantic_copy_span(&ctx->semantics,
            (nx_utf8_span){value, (size_t)length});
        node->value_length = (size_t)length;
    }
    result = nx_plot_paint(ctx, desc, area, desc->scratch, bucket_count, &next,multi,plans,plan_count);
    if (result != NX_OK) goto plot_rollback;
    *state = next;
    return NX_OK;
plot_rollback:
    nx_free_owned_bitmaps(ctx,old_bitmap);
    ctx->clip_depth = old_clip_depth;
    ctx->command_count = old_command_count;
    nx_region_rollback_last(ctx, desc->id);
    nx_semantics_rollback_last(ctx, desc->id, old_text_count);
    return result;
}

nx_result nx_plot(nx_context *ctx,const nx_plot_desc *desc,nx_plot_state *state) {
    return nx_plot_core(ctx,desc,state,NULL);
}
nx_result nx_plot_multi(nx_context *ctx,const nx_plot_multi_desc *desc,nx_plot_state *state) {
    if(!desc)return NX_ERROR_ARGUMENT;
    return nx_plot_core(ctx,&desc->plot,state,desc);
}

static int nx_dashboard_fonts_valid(const nx_font_collection *fonts,
                                    double ppem, size_t cache_bytes) {
    return !fonts || (isfinite(ppem) && ppem >= 1.0 && ppem <= 16384.0 &&
                      cache_bytes > 0);
}

static nx_result nx_dashboard_text(nx_context *ctx,
                                   const nx_font_collection *fonts,
                                   double ppem, size_t cache_bytes,
                                   nx_utf8_span text, nx_point origin,
                                   float max_width, nx_color color) {
    if (!fonts || !text.length) return NX_OK;
    nx_text_layout *layout = NULL;
    nx_result result = nx_text_layout_create(fonts, text.data, text.length,
        NX_BIDI_LTR, ppem, fmax(1.0, (double)max_width), &layout);
    if (result != NX_OK) return result;
    result = nx_draw_text_layout(ctx, layout, origin, color, cache_bytes);
    nx_text_layout_destroy(layout);
    return result;
}

static float nx_dashboard_header_height(nx_rect bounds,
                                         const nx_font_collection *fonts,
                                         double ppem) {
    float wanted = fonts ? fmaxf(28.0f, (float)ppem + 12.0f) : 28.0f;
    return fminf(wanted, bounds.height);
}

static void nx_dashboard_rollback(nx_context *ctx, size_t command_count,
                                   size_t semantic_count, size_t text_count,
                                   unsigned clip_depth) {
    ctx->command_count = command_count;
    ctx->clip_depth = clip_depth;
    while (ctx->semantics.count > semantic_count) {
        uint64_t id = ctx->semantics.nodes[ctx->semantics.count - 1u].id;
        nx_semantics_rollback_last(ctx, id, text_count);
    }
}

static nx_result nx_dashboard_begin(nx_context *ctx, uint64_t id,
                                     uint64_t parent_id, nx_rect bounds,
                                     nx_utf8_span name,
                                     nx_utf8_span description,
                                     nx_semantic_role role,
                                     const nx_font_collection *fonts,
                                     double ppem, size_t cache_bytes,
                                     nx_color surface, nx_color border,
                                     nx_color text) {
    nx_semantic_desc semantic = {0};
    semantic.id = id;
    semantic.parent_id = parent_id;
    semantic.role = role;
    semantic.bounds = bounds;
    semantic.name = name;
    semantic.description = description;
    nx_result result = nx_semantics_add(ctx, &semantic, NULL);
    if (result != NX_OK) return result;
    if ((result = nx_fill_rect(ctx, bounds, surface)) != NX_OK ||
        (result = nx_stroke_rect(ctx, bounds, 1.0f, border)) != NX_OK ||
        (result = nx_push_clip(ctx, bounds)) != NX_OK) return result;
    result = nx_dashboard_text(ctx, fonts, ppem, cache_bytes, name,
        (nx_point){bounds.x + 8.0f, bounds.y + 3.0f},
        fmaxf(1.0f, bounds.width - 16.0f), text);
    if (result != NX_OK) return result;
    float header = nx_dashboard_header_height(bounds, fonts, ppem);
    return nx_draw_line(ctx, (nx_point){bounds.x, bounds.y + header},
                        (nx_point){bounds.x + bounds.width,
                                   bounds.y + header}, 1.0f, border);
}

static nx_result nx_dashboard_row(nx_context *ctx, nx_rect row,
                                   nx_color background, nx_color border) {
    nx_result result = nx_fill_rect(ctx, row, background);
    if (result != NX_OK) return result;
    return nx_draw_line(ctx, (nx_point){row.x, row.y + row.height},
                        (nx_point){row.x + row.width, row.y + row.height},
                        1.0f, border);
}

static nx_result nx_dashboard_common(const nx_context *ctx, uint64_t id,
                                      nx_rect bounds, nx_utf8_span name,
                                      size_t count, size_t first_row,
                                      float row_height,
                                      const nx_font_collection *fonts,
                                      double ppem, size_t cache_bytes,
                                      int telemetry,
                                      float *out_row_height,
                                      size_t *out_visible) {
    if (!ctx || !id || !name.length || !nx_valid_span(name) ||
        !nx_valid_rect(bounds) || bounds.width <= 0.0f ||
        bounds.height <= 0.0f || first_row > count ||
        !isfinite(row_height) || row_height < 0.0f ||
        (row_height > 0.0f && row_height < 16.0f) ||
        !nx_dashboard_fonts_valid(fonts, ppem, cache_bytes))
        return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    float height = row_height;
    if (height == 0.0f) {
        nx_result result = nx_theme_metric(ctx, NX_METRIC_CONTROL_HEIGHT,
                                            &height);
        if (result != NX_OK) return result;
        height = fmaxf(24.0f, height);
    }
    if (fonts) {
        float minimum = telemetry ?
            (float)(ppem * (ppem > 16.0 ? 3.0 : 2.0) +
                    (ppem > 16.0 ? 20.0 : 16.0)) :
            (ppem > 16.0 ? (float)(ppem * 2.0 + 20.0) :
                           (float)(ppem + 16.0));
        height = fmaxf(height, minimum);
    }
    double room = fmax(0.0, (double)bounds.height -
                       (double)nx_dashboard_header_height(bounds, fonts, ppem));
    double fit = floor(room / (double)height);
    size_t remaining = count - first_row;
    size_t visible = fit >= (double)remaining ? remaining : (size_t)fit;
    if (visible > 4096u) visible = 4096u;
    *out_row_height = height;
    *out_visible = visible;
    return NX_OK;
}

nx_result nx_telemetry_panel(nx_context *ctx,
                              const nx_telemetry_panel_desc *desc) {
    if (!desc || (desc->metric_count && !desc->metrics)) return NX_ERROR_ARGUMENT;
    float row_height = 0.0f;
    size_t visible = 0;
    nx_result result = nx_dashboard_common(ctx, desc->id, desc->bounds,
        desc->name, desc->metric_count, desc->first_row, desc->row_height,
        desc->fonts, desc->pixels_per_em, desc->glyph_cache_bytes,
        1,
        &row_height, &visible);
    if (result != NX_OK) return result;
    nx_color surface, raised, border, text, muted, accent, warning, error;
    if ((result = nx_theme_color(ctx, NX_COLOR_SURFACE, &surface)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_SURFACE_RAISED, &raised)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_BORDER, &border)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_TEXT, &text)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_TEXT_MUTED, &muted)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_ACCENT, &accent)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_WARNING, &warning)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_ERROR, &error)) != NX_OK)
        return result;
    size_t old_commands = ctx->command_count;
    size_t old_semantics = ctx->semantics.count;
    size_t old_text = ctx->semantics.text_count;
    unsigned old_clip = ctx->clip_depth;
    result = nx_dashboard_begin(ctx, desc->id, desc->parent_id,
        desc->bounds, desc->name,
        desc->metric_count ? (nx_utf8_span){0} :
            (nx_utf8_span){"No telemetry", 12}, NX_ROLE_GROUP,
        desc->fonts, desc->pixels_per_em,
        desc->glyph_cache_bytes, surface, border, text);
    if (result != NX_OK) goto telemetry_rollback;
    float header = nx_dashboard_header_height(desc->bounds, desc->fonts,
                                               desc->pixels_per_em);
    if (!desc->metric_count) {
        result = nx_dashboard_text(ctx, desc->fonts, desc->pixels_per_em,
            desc->glyph_cache_bytes, (nx_utf8_span){"No telemetry", 12},
            (nx_point){desc->bounds.x + 8.0f,
                       desc->bounds.y + header + 8.0f},
            fmaxf(1.0f, desc->bounds.width - 16.0f), muted);
        if (result != NX_OK) goto telemetry_rollback;
    }
    for (size_t row_index = 0; row_index < visible; ++row_index) {
        const nx_telemetry_metric *metric =
            &desc->metrics[desc->first_row + row_index];
        double span = metric->maximum - metric->minimum;
        if (!metric->id || !metric->name.length ||
            !nx_valid_span(metric->name) || !nx_valid_span(metric->unit) ||
            !nx_valid_span(metric->status) || metric->unit.length > 64u ||
            !isfinite(metric->value) || !isfinite(metric->minimum) ||
            !isfinite(metric->maximum) || !isfinite(span) || span <= 0.0 ||
            metric->level < NX_TELEMETRY_NORMAL ||
            metric->level > NX_TELEMETRY_CRITICAL ||
            (metric->level != NX_TELEMETRY_NORMAL && !metric->status.length)) {
            result = NX_ERROR_ARGUMENT;
            goto telemetry_rollback;
        }
        char value[128];
        int length = snprintf(value, sizeof(value), "%.6g %.*s",
            metric->value, (int)metric->unit.length,
            metric->unit.length ? metric->unit.data : "");
        if (length <= 0 || (size_t)length >= sizeof(value)) {
            result = NX_ERROR_LIMIT;
            goto telemetry_rollback;
        }
        char semantic_value[256];
        int semantic_length = snprintf(semantic_value, sizeof(semantic_value),
            "%s (range %.6g to %.6g)", value,
            metric->minimum, metric->maximum);
        if (semantic_length <= 0 ||
            (size_t)semantic_length >= sizeof(semantic_value)) {
            result = NX_ERROR_LIMIT;
            goto telemetry_rollback;
        }
        nx_rect row = {desc->bounds.x,
                       desc->bounds.y + header + (float)row_index * row_height,
                       desc->bounds.width, row_height};
        nx_semantic_desc semantic = {0};
        semantic.id = metric->id;
        semantic.parent_id = desc->id;
        semantic.role = NX_ROLE_PROGRESS;
        semantic.bounds = row;
        semantic.name = metric->name;
        semantic.description = metric->status;
        semantic.value = (nx_utf8_span){semantic_value,
                                        (size_t)semantic_length};
        result = nx_semantics_add(ctx, &semantic, NULL);
        if (result != NX_OK) goto telemetry_rollback;
        result = nx_dashboard_row(ctx, row,
            row_index & 1u ? surface : raised, border);
        if (result != NX_OK) goto telemetry_rollback;
        result = nx_push_clip(ctx, row);
        if (result != NX_OK) goto telemetry_rollback;
        nx_color signal = metric->level == NX_TELEMETRY_CRITICAL ? error :
                          metric->level == NX_TELEMETRY_WARNING ? warning : accent;
        float gap = fminf(8.0f, row.width * 0.08f);
        float bar_width = fmaxf(0.0f, row.width - 2.0f * gap);
        double clamped = fmax(metric->minimum,
                              fmin(metric->maximum, metric->value));
        float fraction = (float)((clamped - metric->minimum) / span);
        nx_rect track = {row.x + gap, row.y + row.height - 5.0f,
                         bar_width, 3.0f};
        nx_rect bar = track;
        bar.width *= fraction;
        nx_color track_color = border;
        track_color.a *= 0.45f;
        result = nx_fill_rect(ctx, track, track_color);
        if (result == NX_OK) result = nx_fill_rect(ctx, bar, signal);
        int stacked = desc->fonts && desc->pixels_per_em > 16.0;
        if (result == NX_OK) result = nx_dashboard_text(ctx, desc->fonts,
            desc->pixels_per_em, desc->glyph_cache_bytes, metric->name,
            (nx_point){row.x + gap, row.y + 2.0f},
            stacked ? fmaxf(1.0f, row.width - 2.0f * gap) :
                      fmaxf(1.0f, row.width * 0.52f - gap), text);
        if (result == NX_OK) result = nx_dashboard_text(ctx, desc->fonts,
            desc->pixels_per_em, desc->glyph_cache_bytes,
            (nx_utf8_span){value, (size_t)length},
            stacked ? (nx_point){row.x + gap,
                                 row.y + (float)desc->pixels_per_em + 5.0f} :
                      (nx_point){row.x + row.width * 0.55f, row.y + 2.0f},
            stacked ? fmaxf(1.0f, row.width - 2.0f * gap) :
                      fmaxf(1.0f, row.width * 0.45f - gap),
            metric->level == NX_TELEMETRY_NORMAL ? muted : signal);
        if (result == NX_OK && metric->status.length)
            result = nx_dashboard_text(ctx, desc->fonts,
                desc->pixels_per_em, desc->glyph_cache_bytes,
                metric->status,
                (nx_point){row.x + gap,
                    row.y + (stacked ? (float)(desc->pixels_per_em * 2.0 + 8.0)
                                     : 18.0f)},
                fmaxf(1.0f, row.width - 2.0f * gap), signal);
        if (result != NX_OK) goto telemetry_rollback;
        result = nx_pop_clip(ctx);
        if (result != NX_OK) goto telemetry_rollback;
    }
    result = nx_pop_clip(ctx);
    if (result != NX_OK) goto telemetry_rollback;
    return NX_OK;
telemetry_rollback:
    nx_dashboard_rollback(ctx, old_commands, old_semantics, old_text, old_clip);
    return result;
}

nx_result nx_profiler_panel(nx_context *ctx,
                            const nx_profiler_panel_desc *desc) {
    if (!desc || (desc->entry_count && !desc->entries) ||
        !isfinite(desc->frame_ms) || desc->frame_ms <= 0.0)
        return NX_ERROR_ARGUMENT;
    float row_height = 0.0f;
    size_t visible = 0;
    nx_result result = nx_dashboard_common(ctx, desc->id, desc->bounds,
        desc->name, desc->entry_count, desc->first_row, desc->row_height,
        desc->fonts, desc->pixels_per_em, desc->glyph_cache_bytes,
        0,
        &row_height, &visible);
    if (result != NX_OK) return result;
    nx_color surface, raised, border, text, muted, accent;
    if ((result = nx_theme_color(ctx, NX_COLOR_SURFACE, &surface)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_SURFACE_RAISED, &raised)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_BORDER, &border)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_TEXT, &text)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_TEXT_MUTED, &muted)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_ACCENT, &accent)) != NX_OK)
        return result;
    size_t old_commands = ctx->command_count;
    size_t old_semantics = ctx->semantics.count;
    size_t old_text = ctx->semantics.text_count;
    unsigned old_clip = ctx->clip_depth;
    result = nx_dashboard_begin(ctx, desc->id, desc->parent_id,
        desc->bounds, desc->name,
        desc->entry_count ? (nx_utf8_span){0} :
            (nx_utf8_span){"No profiling samples", 20}, NX_ROLE_LIST,
        desc->fonts, desc->pixels_per_em,
        desc->glyph_cache_bytes, surface, border, text);
    if (result != NX_OK) goto profiler_rollback;
    float header = nx_dashboard_header_height(desc->bounds, desc->fonts,
                                               desc->pixels_per_em);
    if (!desc->entry_count) {
        result = nx_dashboard_text(ctx, desc->fonts, desc->pixels_per_em,
            desc->glyph_cache_bytes,
            (nx_utf8_span){"No profiling samples", 20},
            (nx_point){desc->bounds.x + 8.0f,
                       desc->bounds.y + header + 8.0f},
            fmaxf(1.0f, desc->bounds.width - 16.0f), muted);
        if (result != NX_OK) goto profiler_rollback;
    }
    for (size_t row_index = 0; row_index < visible; ++row_index) {
        const nx_profiler_entry *entry =
            &desc->entries[desc->first_row + row_index];
        if (!entry->id || !entry->name.length ||
            !nx_valid_span(entry->name) || !isfinite(entry->inclusive_ms) ||
            !isfinite(entry->self_ms) || entry->inclusive_ms < 0.0 ||
            entry->self_ms < 0.0 || entry->self_ms > entry->inclusive_ms) {
            result = NX_ERROR_ARGUMENT;
            goto profiler_rollback;
        }
        char value[128];
        int length = snprintf(value, sizeof(value),
            "%.4g ms total, %.4g ms self, %llu calls",
            entry->inclusive_ms, entry->self_ms,
            (unsigned long long)entry->calls);
        if (length <= 0 || (size_t)length >= sizeof(value)) {
            result = NX_ERROR_LIMIT;
            goto profiler_rollback;
        }
        char visual_value[48];
        int visual_length = snprintf(visual_value, sizeof(visual_value),
                                     "%.4g ms", entry->inclusive_ms);
        if (visual_length <= 0 ||
            (size_t)visual_length >= sizeof(visual_value)) {
            result = NX_ERROR_LIMIT;
            goto profiler_rollback;
        }
        char level[32];
        int level_length = snprintf(level, sizeof(level), "level %u",
                                    entry->depth);
        if (level_length <= 0 || (size_t)level_length >= sizeof(level)) {
            result = NX_ERROR_LIMIT;
            goto profiler_rollback;
        }
        nx_rect row = {desc->bounds.x,
                       desc->bounds.y + header + (float)row_index * row_height,
                       desc->bounds.width, row_height};
        nx_semantic_desc semantic = {0};
        semantic.id = entry->id;
        semantic.parent_id = desc->id;
        semantic.role = NX_ROLE_LIST_ITEM;
        semantic.bounds = row;
        semantic.name = entry->name;
        semantic.description = (nx_utf8_span){level, (size_t)level_length};
        semantic.value = (nx_utf8_span){value, (size_t)length};
        result = nx_semantics_add(ctx, &semantic, NULL);
        if (result != NX_OK) goto profiler_rollback;
        result = nx_dashboard_row(ctx, row,
            row_index & 1u ? surface : raised, border);
        if (result != NX_OK) goto profiler_rollback;
        result = nx_push_clip(ctx, row);
        if (result != NX_OK) goto profiler_rollback;
        float gap = fminf(8.0f, row.width * 0.08f);
        float bar_width = fmaxf(0.0f, row.width - 2.0f * gap);
        float inclusive = entry->inclusive_ms >= desc->frame_ms ? 1.0f :
                          (float)(entry->inclusive_ms / desc->frame_ms);
        float self = entry->self_ms >= desc->frame_ms ? 1.0f :
                     (float)(entry->self_ms / desc->frame_ms);
        nx_color pale = accent;
        pale.a *= 0.35f;
        result = nx_fill_rect(ctx, (nx_rect){row.x + gap,
            row.y + row.height - 6.0f, bar_width * inclusive, 4.0f}, pale);
        if (result == NX_OK) result = nx_fill_rect(ctx,
            (nx_rect){row.x + gap, row.y + row.height - 6.0f,
                      bar_width * self, 4.0f}, accent);
        float indent = fminf((float)entry->depth * 12.0f,
                             row.width * 0.25f);
        int stacked = desc->fonts && desc->pixels_per_em > 16.0;
        if (result == NX_OK) result = nx_dashboard_text(ctx, desc->fonts,
            desc->pixels_per_em, desc->glyph_cache_bytes, entry->name,
            (nx_point){row.x + gap + indent, row.y + 2.0f},
            stacked ? fmaxf(1.0f, row.width - 2.0f * gap - indent) :
                      fmaxf(1.0f, row.width * 0.52f - gap - indent), text);
        if (result == NX_OK) result = nx_dashboard_text(ctx, desc->fonts,
            desc->pixels_per_em, desc->glyph_cache_bytes,
            (nx_utf8_span){visual_value, (size_t)visual_length},
            stacked ? (nx_point){row.x + gap,
                                 row.y + (float)desc->pixels_per_em + 5.0f} :
                      (nx_point){row.x + row.width * 0.55f, row.y + 2.0f},
            stacked ? fmaxf(1.0f, row.width - 2.0f * gap) :
                      fmaxf(1.0f, row.width * 0.45f - gap), muted);
        if (result != NX_OK) goto profiler_rollback;
        result = nx_pop_clip(ctx);
        if (result != NX_OK) goto profiler_rollback;
    }
    result = nx_pop_clip(ctx);
    if (result != NX_OK) goto profiler_rollback;
    return NX_OK;
profiler_rollback:
    nx_dashboard_rollback(ctx, old_commands, old_semantics, old_text, old_clip);
    return result;
}

nx_result nx_property_row(nx_context *ctx,
                           const nx_property_row_desc *desc,
                           nx_property_action *out_action) {
    if (!out_action) return NX_ERROR_ARGUMENT;
    *out_action = (nx_property_action){0};
    if (!ctx || !desc || !desc->id || !desc->name.length ||
        !nx_valid_span(desc->name) || !nx_valid_span(desc->text_value) ||
        !nx_valid_span(desc->unit) || !nx_valid_rect(desc->bounds) ||
        desc->bounds.width <= 0.0f || desc->bounds.height <= 0.0f ||
        desc->bounds.height < 16.0f ||
        !nx_dashboard_fonts_valid(desc->fonts, desc->pixels_per_em,
                                   desc->glyph_cache_bytes) ||
        (desc->fonts && desc->bounds.height <
            (float)(desc->pixels_per_em > 16.0 ?
                    desc->pixels_per_em * 2.0 + 8.0 :
                    desc->pixels_per_em + 8.0)) ||
        desc->kind < NX_PROPERTY_BOOLEAN || desc->kind > NX_PROPERTY_TEXT ||
        (desc->kind == NX_PROPERTY_BOOLEAN &&
         desc->boolean_value != 0 && desc->boolean_value != 1) ||
        (desc->kind == NX_PROPERTY_NUMBER &&
         (!isfinite(desc->number_value) || !isfinite(desc->minimum) ||
          !isfinite(desc->maximum) || !isfinite(desc->step) ||
          !isfinite(desc->maximum - desc->minimum) ||
          desc->minimum >= desc->maximum ||
          desc->number_value < desc->minimum ||
          desc->number_value > desc->maximum || desc->step <= 0.0 ||
          desc->unit.length > 64u))) return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u) return NX_ERROR_STATE;
    size_t old_commands = ctx->command_count;
    size_t old_semantics = ctx->semantics.count;
    size_t old_text = ctx->semantics.text_count;
    unsigned old_clip = ctx->clip_depth;
    nx_owned_bitmap *old_bitmaps = ctx->owned_bitmaps;
    nx_widget_desc widget = {0};
    widget.semantic.id = desc->id;
    widget.semantic.parent_id = desc->parent_id;
    widget.semantic.role = NX_ROLE_CUSTOM;
    widget.semantic.bounds = desc->bounds;
    widget.semantic.name = desc->name;
    widget.semantic.state = desc->read_only ? NX_SEM_READONLY : 0u;
    widget.semantic.actions = NX_ACTION_FOCUS |
        (desc->read_only ? 0u :
         desc->kind == NX_PROPERTY_NUMBER ?
             NX_ACTION_PRESS | NX_ACTION_INCREMENT | NX_ACTION_DECREMENT :
             NX_ACTION_PRESS);
    widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    nx_property_action next = {0};
    next.boolean_value = desc->boolean_value;
    next.number_value = desc->number_value;
    nx_result result = nx_widget_register(ctx, &widget, &next.interaction);
    if (result != NX_OK) return result;
    if (!desc->read_only) {
        if (desc->kind == NX_PROPERTY_BOOLEAN &&
            (next.interaction.clicked & 1u)) {
            next.boolean_value = !desc->boolean_value;
            next.changed = 1;
        } else if (desc->kind == NX_PROPERTY_NUMBER) {
            if (next.interaction.focused && ctx->action_keys_pressed[NX_KEY_LEFT]) {
                double distance = desc->number_value - desc->minimum;
                next.number_value = distance <= desc->step ? desc->minimum :
                                    desc->number_value - desc->step;
                next.changed = next.number_value != desc->number_value;
            }
            if (next.interaction.focused && ctx->action_keys_pressed[NX_KEY_RIGHT]) {
                double distance = desc->maximum - next.number_value;
                next.number_value = distance <= desc->step ? desc->maximum :
                                    next.number_value + desc->step;
                next.changed = next.number_value != desc->number_value;
            }
            if (next.interaction.clicked) next.edit_requested = 1;
        } else if (next.interaction.clicked) next.edit_requested = 1;
    }
    char formatted[128];
    nx_utf8_span value = {0};
    if (desc->kind == NX_PROPERTY_BOOLEAN) {
        value = next.boolean_value ? (nx_utf8_span){"true", 4} :
                                     (nx_utf8_span){"false", 5};
    } else if (desc->kind == NX_PROPERTY_NUMBER) {
        int length = desc->unit.length ?
            snprintf(formatted, sizeof(formatted), "%.6g %.*s",
                next.number_value, (int)desc->unit.length, desc->unit.data) :
            snprintf(formatted, sizeof(formatted), "%.6g",
                next.number_value);
        if (length <= 0 || (size_t)length >= sizeof(formatted)) {
            result = NX_ERROR_LIMIT;
            goto property_rollback;
        }
        value = (nx_utf8_span){formatted, (size_t)length};
    } else value = desc->text_value;
    if (value.length) {
        if (value.length == SIZE_MAX) {
            result = NX_ERROR_LIMIT;
            goto property_rollback;
        }
        result = nx_semantic_reserve_text(ctx, value.length + 1u);
        if (result != NX_OK) goto property_rollback;
        nx_semantic_record *node =
            &ctx->semantics.nodes[ctx->semantics.count - 1u];
        node->value_offset = nx_semantic_copy_span(&ctx->semantics, value);
        node->value_length = value.length;
    }
    nx_color surface, border, text, muted, hover, pressed, focus, accent;
    if ((result = nx_theme_color(ctx, NX_COLOR_SURFACE_RAISED, &surface)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_BORDER, &border)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_TEXT, &text)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_TEXT_MUTED, &muted)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_HOVER, &hover)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_PRESSED, &pressed)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_FOCUS, &focus)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_ACCENT, &accent)) != NX_OK)
        goto property_rollback;
    nx_color background = next.interaction.held ? pressed :
                          next.interaction.hovered ? hover : surface;
    result = nx_fill_rect(ctx, desc->bounds, background);
    if (result != NX_OK) goto property_rollback;
    result = nx_stroke_rect(ctx, desc->bounds, 1.0f, border);
    if (result != NX_OK) goto property_rollback;
    result = nx_push_clip(ctx, desc->bounds);
    if (result != NX_OK) goto property_rollback;
    int stacked = desc->fonts && desc->pixels_per_em > 16.0;
    float gap = fminf(8.0f, desc->bounds.width * 0.08f);
    if (desc->kind == NX_PROPERTY_BOOLEAN) {
        float box = fminf(14.0f, desc->bounds.height - 8.0f);
        nx_rect mark = {desc->bounds.x + desc->bounds.width - gap - box,
                        desc->bounds.y + (desc->bounds.height - box) * 0.5f,
                        box, box};
        result = nx_stroke_rect(ctx, mark, 1.0f, border);
        if (result == NX_OK && next.boolean_value) {
            nx_rect inner = {mark.x + 3.0f, mark.y + 3.0f,
                             fmaxf(0.0f, box - 6.0f),
                             fmaxf(0.0f, box - 6.0f)};
            result = nx_fill_rect(ctx, inner, accent);
        }
        if (result != NX_OK) goto property_rollback;
    }
    result = nx_dashboard_text(ctx, desc->fonts, desc->pixels_per_em,
        desc->glyph_cache_bytes, desc->name,
        (nx_point){desc->bounds.x + gap, desc->bounds.y + 2.0f},
        stacked ? fmaxf(1.0f, desc->bounds.width - 2.0f * gap) :
                  fmaxf(1.0f, desc->bounds.width * 0.5f - gap), text);
    if (result != NX_OK) goto property_rollback;
    if (desc->kind != NX_PROPERTY_BOOLEAN) {
        result = nx_dashboard_text(ctx, desc->fonts, desc->pixels_per_em,
            desc->glyph_cache_bytes, value,
            stacked ? (nx_point){desc->bounds.x + gap,
                                 desc->bounds.y + (float)desc->pixels_per_em + 4.0f} :
                      (nx_point){desc->bounds.x + desc->bounds.width * 0.52f,
                                 desc->bounds.y + 2.0f},
            stacked ? fmaxf(1.0f, desc->bounds.width - 2.0f * gap) :
                      fmaxf(1.0f, desc->bounds.width * 0.48f - gap),
            next.changed ? accent : muted);
        if (result != NX_OK) goto property_rollback;
    }
    result = nx_pop_clip(ctx);
    if (result != NX_OK) goto property_rollback;
    if (next.interaction.focused) {
        result = nx_stroke_rect(ctx, desc->bounds, 2.0f, focus);
        if (result != NX_OK) goto property_rollback;
    }
    *out_action = next;
    return NX_OK;
property_rollback:
    nx_dashboard_rollback(ctx, old_commands, old_semantics, old_text, old_clip);
    nx_free_owned_bitmaps(ctx, old_bitmaps);
    if (ctx->current_regions.count &&
        ctx->current_regions.data[ctx->current_regions.count - 1u].id == desc->id)
        nx_region_rollback_last(ctx, desc->id);
    return result;
}

struct nx_viewport_surface {
    nx_allocator allocator;
    nx_viewport_source source;
    uint32_t pixel_width, pixel_height;
    int in_flight;
};

nx_result nx_viewport_surface_create(const nx_viewport_source *source,
                                     const nx_allocator *allocator,
                                     nx_viewport_surface **out_surface) {
    if (!out_surface) return NX_ERROR_ARGUMENT;
    *out_surface = NULL;
    if (!source || !source->resize || !source->acquire || !source->release)
        return NX_ERROR_ARGUMENT;
    nx_allocator a = allocator ? *allocator : (nx_allocator){0};
    if ((a.allocate == NULL) != (a.deallocate == NULL))
        return NX_ERROR_ARGUMENT;
    if (!a.allocate) {
        a = (nx_allocator){NULL, nx_default_allocate, nx_default_deallocate};
    }
    nx_viewport_surface *surface = a.allocate(a.user, sizeof(*surface));
    if (!surface) return NX_ERROR_MEMORY;
    *surface = (nx_viewport_surface){0};
    surface->allocator = a;
    surface->source = *source;
    *out_surface = surface;
    return NX_OK;
}

nx_result nx_viewport_surface_destroy(nx_viewport_surface *surface) {
    if (!surface) return NX_ERROR_ARGUMENT;
    if (surface->in_flight) return NX_ERROR_STATE;
    nx_allocator a = surface->allocator;
    a.deallocate(a.user, surface, sizeof(*surface));
    return NX_OK;
}

nx_result nx_viewport_surface_complete(nx_viewport_surface *surface) {
    if (!surface) return NX_ERROR_ARGUMENT;
    if (!surface->in_flight) return NX_ERROR_STATE;
    surface->source.release(surface->source.user);
    surface->in_flight = 0;
    return NX_OK;
}

nx_result nx_viewport(nx_context *ctx, nx_viewport_surface *surface,
                      const nx_viewport_desc *desc,
                      nx_interaction_state *out_state) {
    if (!out_state) return NX_ERROR_ARGUMENT;
    *out_state = (nx_interaction_state){0};
    if (!ctx || !surface || !desc || !desc->id || !desc->name.length ||
        !nx_valid_span(desc->name) || !nx_valid_rect(desc->bounds) ||
        !isfinite(desc->scale) || desc->scale <= 0.0)
        return NX_ERROR_ARGUMENT;
    if (ctx->state != 1u || surface->in_flight) return NX_ERROR_STATE;
    uint32_t width = 0, height = 0;
    if (desc->bounds.width > 0.0f && desc->bounds.height > 0.0f) {
        double w = ceil((double)desc->bounds.width * desc->scale);
        double h = ceil((double)desc->bounds.height * desc->scale);
        if (!isfinite(w) || !isfinite(h) ||
            w > NX_VIEWPORT_MAX_DIMENSION || h > NX_VIEWPORT_MAX_DIMENSION)
            return NX_ERROR_LIMIT;
        width = (uint32_t)w;
        height = (uint32_t)h;
    }
    if (!width || !height) {
        if (surface->pixel_width || surface->pixel_height) {
            nx_result result = surface->source.resize(surface->source.user, 0, 0);
            if (result != NX_OK) return result;
            surface->pixel_width = surface->pixel_height = 0;
        }
        return NX_OK;
    }
    size_t old_text = ctx->semantics.text_count;
    size_t old_commands = ctx->command_count;
    unsigned old_clip = ctx->clip_depth;
    nx_widget_desc widget = {0};
    widget.semantic.id = desc->id;
    widget.semantic.parent_id = desc->parent_id;
    widget.semantic.role = NX_ROLE_CUSTOM;
    widget.semantic.bounds = desc->bounds;
    widget.semantic.name = desc->name;
    widget.semantic.state = desc->disabled ? NX_SEM_DISABLED : 0u;
    widget.semantic.actions = NX_ACTION_FOCUS | NX_ACTION_PRESS;
    widget.flags = NX_WIDGET_INTERACTIVE | NX_WIDGET_FOCUSABLE;
    nx_interaction_state interaction;
    nx_result result = nx_widget_register(ctx, &widget, &interaction);
    if (result != NX_OK) return result;
    int acquired = 0;
    if (surface->pixel_width != width || surface->pixel_height != height) {
        result = surface->source.resize(surface->source.user, width, height);
        if (result != NX_OK) goto viewport_rollback;
        surface->pixel_width = width;
        surface->pixel_height = height;
    }
    nx_bitmap_view bitmap = {0};
    result = surface->source.acquire(surface->source.user, &bitmap);
    if (result != NX_OK) goto viewport_rollback;
    acquired = 1;
    if (bitmap.width != width || bitmap.height != height ||
        bitmap.format != NX_BITMAP_RGBA8 || !nx_valid_bitmap_view(bitmap)) {
        result = NX_ERROR_ARGUMENT;
        goto viewport_rollback;
    }
    nx_color background, focus;
    if ((result = nx_theme_color(ctx, NX_COLOR_SURFACE_RAISED,
                                 &background)) != NX_OK ||
        (result = nx_theme_color(ctx, NX_COLOR_FOCUS, &focus)) != NX_OK)
        goto viewport_rollback;
    result = nx_fill_rect(ctx, desc->bounds, background);
    if (result != NX_OK) goto viewport_rollback;
    result = nx_push_clip(ctx, desc->bounds);
    if (result != NX_OK) goto viewport_rollback;
    result = nx_draw_bitmap(ctx, desc->bounds, bitmap,
                            (nx_color){1, 1, 1, 1});
    if (result != NX_OK) goto viewport_rollback;
    result = nx_pop_clip(ctx);
    if (result != NX_OK) goto viewport_rollback;
    if (interaction.focused && !desc->disabled) {
        result = nx_stroke_rect(ctx, desc->bounds, 2.0f, focus);
        if (result != NX_OK) goto viewport_rollback;
    }
    surface->in_flight = 1;
    *out_state = interaction;
    return NX_OK;
viewport_rollback:
    ctx->command_count = old_commands;
    ctx->clip_depth = old_clip;
    if (acquired) surface->source.release(surface->source.user);
    nx_region_rollback_last(ctx, desc->id);
    nx_semantics_rollback_last(ctx, desc->id, old_text);
    return result;
}
