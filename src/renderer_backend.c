#include "renderer_backend.h"
#include "internal.h"

#include <string.h>

static int nx_render_valid_point(nx_point p) {
    return isfinite(p.x) && isfinite(p.y);
}

nx_result nx_render_validate_commands(const nx_command *commands, size_t count) {
    if (!commands && count) return NX_ERROR_ARGUMENT;
    unsigned depth = 0;
    for (size_t i = 0; i < count; ++i) {
        const nx_command *cmd = &commands[i];
        switch (cmd->type) {
        case NX_COMMAND_FILL_RECT:
            if (!nx_valid_rect(cmd->rect) || !nx_valid_color(cmd->color))
                return NX_ERROR_COMMAND;
            break;
        case NX_COMMAND_STROKE_RECT:
            if (!nx_valid_rect(cmd->rect) || !nx_valid_color(cmd->color) ||
                !isfinite(cmd->thickness) || cmd->thickness <= 0.0f)
                return NX_ERROR_COMMAND;
            break;
        case NX_COMMAND_FILL_ROUND_RECT:
            if (!nx_valid_rect(cmd->rect) || !nx_valid_color(cmd->color) ||
                !isfinite(cmd->radius) || cmd->radius < 0.0f)
                return NX_ERROR_COMMAND;
            break;
        case NX_COMMAND_LINE:
            if (!nx_render_valid_point(cmd->points[0]) ||
                !nx_render_valid_point(cmd->points[1]) ||
                !nx_valid_color(cmd->color) || !isfinite(cmd->thickness) ||
                cmd->thickness <= 0.0f) return NX_ERROR_COMMAND;
            break;
        case NX_COMMAND_FILL_TRIANGLE:
            if (!nx_render_valid_point(cmd->points[0]) ||
                !nx_render_valid_point(cmd->points[1]) ||
                !nx_render_valid_point(cmd->points[2]) ||
                !nx_valid_color(cmd->color)) return NX_ERROR_COMMAND;
            break;
        case NX_COMMAND_BITMAP:
            if (!nx_valid_rect(cmd->rect) || !nx_valid_color(cmd->color) ||
                !nx_valid_bitmap_view(cmd->bitmap)) return NX_ERROR_COMMAND;
            break;
        case NX_COMMAND_PUSH_CLIP:
            if (!nx_valid_rect(cmd->rect) || depth == NX_MAX_CLIP_DEPTH)
                return NX_ERROR_COMMAND;
            ++depth;
            break;
        case NX_COMMAND_POP_CLIP:
            if (!depth) return NX_ERROR_COMMAND;
            --depth;
            break;
        default:
            return NX_ERROR_COMMAND;
        }
    }
    return depth ? NX_ERROR_COMMAND : NX_OK;
}

nx_result nx_renderer_open(nx_renderer_driver *driver,
                           const nx_render_backend *backend) {
    if (!driver || !backend || !backend->resize || !backend->submit ||
        !backend->recover || !backend->destroy) return NX_ERROR_ARGUMENT;
    if (driver->open) return NX_ERROR_STATE;
    *driver = (nx_renderer_driver){0};
    driver->backend = *backend;
    driver->open = 1;
    return NX_OK;
}

static int nx_render_valid_outcome(nx_render_outcome outcome) {
    return outcome >= NX_RENDER_READY && outcome <= NX_RENDER_DEVICE_LOST;
}

nx_result nx_renderer_run(nx_renderer_driver *driver,
                          nx_render_target target,
                          const nx_command *commands, size_t count,
                          nx_color clear_color,
                          nx_render_outcome *out_outcome) {
    if (!out_outcome) return NX_ERROR_ARGUMENT;
    *out_outcome = NX_RENDER_SUSPENDED;
    if (!driver || !driver->open || !isfinite(target.scale) ||
        target.scale <= 0.0 || !nx_valid_color(clear_color))
        return NX_ERROR_ARGUMENT;
    if (driver->closing) return NX_ERROR_STATE;
    nx_result result = nx_render_validate_commands(commands, count);
    if (result != NX_OK) return result;
    if (driver->device_lost) {
        *out_outcome = NX_RENDER_DEVICE_LOST;
        return NX_OK;
    }
    if (!target.pixel_width || !target.pixel_height) {
        target.pixel_width = target.pixel_height = 0;
    }
    if (!driver->has_target ||
        driver->target.pixel_width != target.pixel_width ||
        driver->target.pixel_height != target.pixel_height ||
        driver->target.scale != target.scale) {
        nx_render_outcome outcome = NX_RENDER_SUSPENDED;
        result = driver->backend.resize(driver->backend.user, target, &outcome);
        if (result != NX_OK) return result;
        if (!nx_render_valid_outcome(outcome)) return NX_ERROR_PLATFORM;
        if (outcome == NX_RENDER_DEVICE_LOST) {
            driver->device_lost = 1;
            *out_outcome = outcome;
            return NX_OK;
        }
        if (outcome == NX_RENDER_SUSPENDED) return NX_OK;
        driver->target = target;
        driver->has_target = 1;
    }
    if (!target.pixel_width || !target.pixel_height) return NX_OK;
    nx_render_outcome outcome = NX_RENDER_SUSPENDED;
    result = driver->backend.submit(driver->backend.user, target,
                                     commands, count, clear_color, &outcome);
    if (result != NX_OK) return result;
    if (!nx_render_valid_outcome(outcome)) return NX_ERROR_PLATFORM;
    if (outcome == NX_RENDER_SUSPENDED) driver->has_target = 0;
    if (outcome == NX_RENDER_DEVICE_LOST) driver->device_lost = 1;
    *out_outcome = outcome;
    return NX_OK;
}

nx_result nx_renderer_recover(nx_renderer_driver *driver) {
    if (!driver || !driver->open || !driver->device_lost || driver->closing)
        return NX_ERROR_STATE;
    nx_result result = driver->backend.recover(driver->backend.user);
    if (result != NX_OK) return result;
    driver->device_lost = 0;
    driver->has_target = 0;
    return NX_OK;
}

nx_result nx_renderer_close(nx_renderer_driver *driver) {
    if (!driver || !driver->open) return NX_OK;
    driver->closing = 1;
    nx_result result = driver->backend.destroy(driver->backend.user);
    if (result != NX_OK) return result;
    *driver = (nx_renderer_driver){0};
    return NX_OK;
}
