#ifndef NEXAUI_SOFTWARE_H
#define NEXAUI_SOFTWARE_H

#include "nexaui/nexaui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Linux Wayland software presentation, provided by NexaUI::NexaUI. */
typedef struct nx_software_view nx_software_view;
typedef enum nx_software_view_outcome {
    NX_SOFTWARE_VIEW_READY = 1,
    NX_SOFTWARE_VIEW_SUSPENDED = 2,
    NX_SOFTWARE_VIEW_BUSY = 3
} nx_software_view_outcome;

/* Leases a Wayland window on its owner thread. Other Linux backends return
   UNSUPPORTED. The window can be destroyed first; the view must still close.
   *out_view is NULL on failure. No application frame is submitted here. */
NX_API nx_result nx_software_view_open(nx_window *window, nx_software_view **out_view);
/* Rasterizes logical commands at the window's current scale and submits a
   premultiplied ARGB8888 shared-memory buffer. Commands and bitmap bytes are
   borrowed only until return. READY means submitted, not displayed. BUSY means
   all three bounded buffers await compositor release; poll the window and
   retry. SUSPENDED means the window is hidden or not configured. At most
   128 MiB per outstanding buffer is allocated. Validation and allocation
   errors submit no new buffer; a connection failure after commit has an
   unknown presentation outcome. */
NX_API nx_result nx_software_view_render(nx_software_view *view,
    const nx_command *commands, size_t count, nx_color clear,
    nx_software_view_outcome *out_outcome);
/* Same owner thread. NULL view is a no-op. A reentrant close returns STATE.
   On normal close the view is destroyed and *view cleared even if restoring
   the window background reports an error. Outstanding submitted buffers stay
   with the window until compositor release or window destruction. */
NX_API nx_result nx_software_view_close(nx_software_view **view);

#ifdef __cplusplus
}
#endif
#endif
