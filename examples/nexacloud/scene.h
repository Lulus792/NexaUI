#ifndef NEXACLOUD_SCENE_H
#define NEXACLOUD_SCENE_H
#include <nexaui/nexaui.h>

typedef struct cloud_scene cloud_scene;
cloud_scene *cloud_create(const char *font_path);
void cloud_destroy(cloud_scene *scene);
nx_context *cloud_context(cloud_scene *scene);
void cloud_set_window(cloud_scene *scene, nx_window *window);
nx_cursor_shape cloud_cursor(const cloud_scene *scene);
/* Text preference, independent of window/backbuffer density. Applied next frame;
   finite values in [1,2] are accepted without changing an in-flight command stream. */
int cloud_set_text_scale(cloud_scene *scene, float scale);
float cloud_text_scale(const cloud_scene *scene);
/* Physical pixels per logical unit. Applied at the next begin-frame so a
   density change never invalidates an already recorded stamp. */
int cloud_set_pixel_scale(cloud_scene *scene,float scale);
int cloud_frame(cloud_scene *scene, float width, float height, double dt);
int cloud_export(cloud_scene *scene, const char *path, float width, float height);
/* The scripted workflow uses the same normalized input as native windows. */
int cloud_smoke(cloud_scene *scene, const char *output_prefix);
#endif
