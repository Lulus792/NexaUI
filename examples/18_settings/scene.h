#ifndef NEXAUI_SETTINGS_SCENE_H
#define NEXAUI_SETTINGS_SCENE_H
#include "nexaui/nexaui.h"

typedef struct settings_scene settings_scene;
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
   duplicate completions are rejected. No work survives scene destruction. */
uint64_t settings_pending_apply(const settings_scene *scene);
/* Claim the newly submitted request once. A pending or unknown result is not
   permission to start it again. The external owner retains the token. */
uint64_t settings_take_apply_request(settings_scene *scene);
/* Mark the outcome uncertain without claiming success or failure. The same
   token remains pending until a later authoritative completion is observed. */
int settings_observe_apply_unknown(settings_scene *scene, uint64_t token);
int settings_complete_apply(settings_scene *scene, uint64_t token, int success);
/* Test-only fixture controls for native accessibility capability checks. */
int settings_enable_accessibility_probe(settings_scene *scene);
int settings_take_accessibility_probe_redraw(settings_scene *scene);
#endif
