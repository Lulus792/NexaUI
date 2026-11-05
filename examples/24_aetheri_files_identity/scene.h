#ifndef NEXAUI_AETHERI_FILES_SCENE_H
#define NEXAUI_AETHERI_FILES_SCENE_H
#include "intent.h"
#include <nexaui/nexaui.h>

typedef struct files_scene files_scene;
/* model and fonts are borrowed until destruction; owner-thread only. */
nx_result files_scene_create(struct ae_files *model,nx_font_collection *fonts,files_scene **out);
void files_scene_destroy(files_scene *scene);
/* Optional search field; window is borrowed through the last scene frame. */
nx_result files_scene_enable_filter(files_scene *scene,nx_window *window);
nx_result files_scene_focus_filter(files_scene *scene,int clear);
int files_scene_filter_has_focus(const files_scene *scene);
/* Call sync after an external model refresh. Directory changes retire all row IDs. */
nx_result files_scene_sync(files_scene *scene);
/* Rejected whole queries keep the old model filter but invalidate activation. */
nx_result files_scene_filter(files_scene *scene,const char *query,size_t length);
nx_result files_scene_key(files_scene *scene,nx_key key,int owns_focus);
/* Positive logical pixels move down; selection and pending intents are unchanged. */
nx_result files_scene_scroll(files_scene *scene,double pixels);
nx_result files_scene_frame(files_scene *scene,float width,float height);
nx_context *files_scene_context(files_scene *scene);
uint64_t files_scene_selected(const files_scene *scene);
int files_scene_query_accepted(const files_scene *scene);
/* Refuse an invalid query or a selection hidden by the current filter. */
enum ae_files_result files_scene_stage(files_scene *scene,files_intent *intent);
#endif
