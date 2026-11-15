#ifndef AETHERI_NEXAUI_LOGIN_VIEW_H
#define AETHERI_NEXAUI_LOGIN_VIEW_H
#include <nexaui/nexaui.h>
#include "login/protocol.h"

typedef struct aetheri_login_view aetheri_login_view;

/* The host owns the admitted conversation, fonts and optional native window.
   Use one view per conversation on the UI owner thread. NULL conversation
   displays the unavailable state. Destroy the view before the window/fonts. */
nx_result aetheri_login_view_create(struct ae_login *conversation,
    const nx_font_collection *fonts, nx_window *ime_window,
    aetheri_login_view **out_view);
#ifdef AETHERI_LOGIN_CONNECTION_VIEW
struct ae_login_connection;
/* Borrow an original connection already admitted by its host. All commands
   use that connection's original checks. The host owns watch/pump/deadlines,
   interrupt and close, and must outlive the view. NULL displays unavailable.
   This constructor neither admits a service nor opens or pumps a socket. */
nx_result aetheri_login_view_create_connection(struct ae_login_connection *connection,
    const nx_font_collection *fonts, nx_window *ime_window,
    aetheri_login_view **out_view);
#endif
void aetheri_login_view_destroy(aetheri_login_view *view);
nx_context *aetheri_login_view_context(aetheri_login_view *view);

enum { AETHERI_LOGIN_REPAINT = 1, AETHERI_LOGIN_REQUEST_QUEUED = 2 };

/* Compose one frame. A successful explicit action queues at most one request
   in the original model after the frame completes. While REPAINT is returned,
   build another frame before publishing semantics or rendering. */
nx_result aetheri_login_view_frame(aetheri_login_view *view, float width,
    float height, unsigned *out_changes);
#endif
