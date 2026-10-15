#ifndef NEXAUI_WINDOWS_ACCESSIBILITY_H
#define NEXAUI_WINDOWS_ACCESSIBILITY_H
#include "window_internal.h"

typedef struct nx_uia_bridge nx_uia_bridge;
nx_result nx_uia_create(HWND hwnd, nx_allocator allocator, nx_uia_bridge **out);
nx_result nx_uia_publish(nx_uia_bridge *bridge, const nx_context *context);
int nx_uia_matches_context(nx_uia_bridge *bridge, const nx_context *context);
nx_result nx_uia_route(nx_uia_bridge *bridge, nx_context *context, size_t *count);
void nx_uia_geometry(nx_uia_bridge *bridge);
void nx_uia_destroy(nx_uia_bridge *bridge);
LRESULT nx_uia_get_object(nx_uia_bridge *bridge, WPARAM wparam, LPARAM lparam);
int nx_uia_object_id(LPARAM lparam);
#endif
