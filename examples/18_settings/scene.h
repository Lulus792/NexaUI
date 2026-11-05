#ifndef NEXAUI_SETTINGS_SCENE_H
#define NEXAUI_SETTINGS_SCENE_H
#include "nexaui/nexaui.h"

typedef struct settings_scene settings_scene;
typedef struct settings_session settings_session;
#define SETTINGS_NAME_CAPACITY 128u
typedef struct settings_apply_request {
    uint64_t token;
    char name[SETTINGS_NAME_CAPACITY]; /* UTF-8 bytes, not NUL-terminated. */
    size_t name_length;
    int reduced_motion;
} settings_apply_request;
/* The font bytes are borrowed until settings_destroy; window may be NULL for
   headless tests and otherwise must outlive the scene. */
int settings_create(const unsigned char *font_bytes, size_t font_size,
                    nx_window *window, settings_scene **out);
void settings_destroy(settings_scene *scene);
nx_context *settings_context(settings_scene *scene);
int settings_frame(settings_scene *scene, float width, float height);
int settings_applied(settings_scene *scene, nx_utf8_span *name, int *reduced_motion);
int settings_set_draft_name(settings_scene *scene, const char *name, size_t length);
/* Application-owned example catalog; unsupported language codes are rejected. */
int settings_set_language(settings_scene *scene, const char *language);
/* The host owns completion. Tokens identify one immutable submission; stale or
   duplicate completions are rejected. Ordinary scene destruction retains no work. */
uint64_t settings_pending_apply(const settings_scene *scene);
/* Claim the newly submitted request once. A pending or unknown result is not
   permission to start it again. The external owner retains the token. */
uint64_t settings_take_apply_request(settings_scene *scene);
/* Atomically claim and copy a request for an application-owned backend. The
   copy has no borrowed UI pointers and can outlive the scene. */
int settings_claim_apply_request(settings_scene *scene, settings_apply_request *out_request);
/* Mark the outcome uncertain without claiming success or failure. The same
   token remains pending until a later authoritative completion is observed. */
int settings_observe_apply_unknown(settings_scene *scene, uint64_t token);
int settings_complete_apply(settings_scene *scene, uint64_t token, int success);
/* Owner-thread, in-memory transfer of an example model into application-owned
   state. On successful detach *scene becomes NULL; on failure it is unchanged.
   Drafts above 65536 UTF-8 bytes are rejected without destroying the view.
   A claimed unresolved operation becomes unknown; it is never redispatched.
   The owner must query its original backend using the token and report the
   result through the session or restored scene. No worker, native or NexaUI
   pointer is stored; tokens are local to this example session, not global IDs.
   Retire published semantics with nx_window_publish_semantics(window, NULL)
   before detaching and publish the new scene after its first frame. The example
   restores its control focus and scroll offset when possible; a disabled Apply
   falls back to the checkbox. New input takes precedence. The focused control
   is revealed on a follow-up frame. Undo and text selection are not transferred.
   This is not disk persistence, cross-process restoration or authorization. */
int settings_detach(settings_scene **scene, settings_session **out_session);
void settings_session_destroy(settings_session *session);
uint64_t settings_session_pending_apply(const settings_session *session);
int settings_session_observe_unknown(settings_session *session, uint64_t token);
int settings_session_complete_apply(settings_session *session, uint64_t token, int success);
/* On success consumes *session and sets it to NULL. Failure leaves it intact. */
int settings_resume(const unsigned char *font_bytes, size_t font_size, nx_window *window,
                    settings_session **session, settings_scene **out_scene);
/* Test-only fixture controls for native accessibility capability checks. */
int settings_enable_accessibility_probe(settings_scene *scene);
int settings_take_accessibility_probe_redraw(settings_scene *scene);
#endif
