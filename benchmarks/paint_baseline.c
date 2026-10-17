/* Compile a user-selected, recorded NexaUI paint source beside the current
   implementation for paired measurements. It is never part of the library. */
#ifdef NEXAUI_SHARED
#undef NEXAUI_SHARED
#endif
#define nx_surface_image_create nx_before_surface_create
#define nx_shadow_image_create nx_before_shadow_create
#define nx_paint_image_view nx_before_paint_view
#define nx_draw_paint_image nx_before_draw_paint
#define nx_paint_image_destroy nx_before_paint_destroy
#include NEXAUI_PAINT_BASELINE_PATH
