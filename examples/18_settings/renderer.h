#ifndef NEXAUI_SETTINGS_RENDERER_H
#define NEXAUI_SETTINGS_RENDERER_H

#include "nexaui/nexaui.h"
typedef struct settings_renderer settings_renderer;
typedef enum settings_renderer_outcome {
    SETTINGS_RENDER_READY = 1,
    SETTINGS_RENDER_SUSPENDED = 2,
    SETTINGS_RENDER_DEVICE_LOST = 3,
    SETTINGS_RENDER_BUSY = 4
} settings_renderer_outcome;

nx_result settings_renderer_open(nx_window *window, settings_renderer **out);
nx_result settings_renderer_render(settings_renderer *renderer, nx_pixel_size pixels,
    double scale, const nx_command *commands, size_t count, nx_color clear,
    settings_renderer_outcome *outcome);
nx_result settings_renderer_recover(settings_renderer *renderer);
nx_result settings_renderer_close(settings_renderer **renderer);
#endif
