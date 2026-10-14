#ifndef NEXACLOUD_SCENE_H
#define NEXACLOUD_SCENE_H
#include <nexaui/nexaui.h>

typedef struct cloud_scene cloud_scene;
cloud_scene *cloud_create(const char *font_path);
void cloud_destroy(cloud_scene *scene);
nx_context *cloud_context(cloud_scene *scene);
void cloud_set_window(cloud_scene *scene, nx_window *window);
nx_cursor_shape cloud_cursor(const cloud_scene *scene);
int cloud_frame(cloud_scene *scene, float width, float height, double dt);
int cloud_export(cloud_scene *scene, const char *path, float width, float height);
/* The scripted workflow uses the same normalized input as native windows. */
int cloud_smoke(cloud_scene *scene, const char *output_prefix);
#endif
