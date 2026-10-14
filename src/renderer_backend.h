#ifndef NEXAUI_RENDERER_BACKEND_H
#define NEXAUI_RENDERER_BACKEND_H

#include "nexaui/nexaui.h"

#if defined(NEXAUI_RENDER_INTERNAL_IMPL)
#define NX_RENDER_INTERNAL_API
#elif defined(_WIN32) && defined(NEXAUI_BUILDING_SHARED)
#define NX_RENDER_INTERNAL_API __declspec(dllexport)
#elif defined(_WIN32) && defined(NEXAUI_SHARED)
#define NX_RENDER_INTERNAL_API __declspec(dllimport)
#else
#define NX_RENDER_INTERNAL_API
#endif

/* Private driver boundary shared by headless and future native renderers.
   The caller zero-initializes nx_renderer_driver before open. All calls are
   serialized on one renderer thread. A successful submit must finish reading
   borrowed bitmap pixels (or copy/upload them into backend-owned storage)
   before returning. GPU execution may continue afterward. */
typedef enum nx_render_outcome {
    NX_RENDER_READY = 1,
    NX_RENDER_SUSPENDED = 2,
    NX_RENDER_DEVICE_LOST = 3
} nx_render_outcome;

typedef struct nx_render_target {
    uint32_t pixel_width, pixel_height;
    double scale;
} nx_render_target;

typedef struct nx_render_backend {
    void *user;
    nx_result (*resize)(void *user, nx_render_target target,
                        nx_render_outcome *out_outcome);
    nx_result (*submit)(void *user, nx_render_target target,
                        const nx_command *commands, size_t count,
                        nx_color clear_color,
                        nx_render_outcome *out_outcome);
    nx_result (*recover)(void *user);
    /* Failure retains backend ownership for another close attempt. */
    nx_result (*destroy)(void *user);
} nx_render_backend;

typedef struct nx_renderer_driver {
    nx_render_backend backend;
    nx_render_target target;
    int open, has_target, device_lost, closing;
} nx_renderer_driver;

/* No callback is called for a malformed stream. Clip depth and every command
   are validated before a backend sees any work. */
NX_RENDER_INTERNAL_API nx_result nx_render_validate_commands(
    const nx_command *commands, size_t count);
NX_RENDER_INTERNAL_API nx_result nx_renderer_open(nx_renderer_driver *driver,
                           const nx_render_backend *backend);
NX_RENDER_INTERNAL_API nx_result nx_renderer_run(nx_renderer_driver *driver,
                          nx_render_target target,
                          const nx_command *commands, size_t count,
                          nx_color clear_color,
                          nx_render_outcome *out_outcome);
NX_RENDER_INTERNAL_API nx_result nx_renderer_recover(nx_renderer_driver *driver);
/* Failure preserves the backend and prevents new submissions/recovery.
   Retry close when pending work can complete. NULL/already closed is NX_OK. */
NX_RENDER_INTERNAL_API nx_result nx_renderer_close(nx_renderer_driver *driver);

#endif
