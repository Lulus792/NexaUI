#ifndef EXAMPLE_SOFTWARE_VIEW_H
#define EXAMPLE_SOFTWARE_VIEW_H
#include "software_components.h"
/* Optional example presentation, not a public SDK/authority contract. Owner,
   fonts and any bound window outlive this single-threaded view. */
typedef struct swg_view swg_view;
typedef struct swg_style { int german; float text_size,raster_scale; nx_theme_kind theme; } swg_style;
typedef enum swg_mode { SWG_LIST, SWG_FILTER, SWG_DETAIL, SWG_CONTENTS, SWG_DECLARATION } swg_mode;
typedef struct swg_intent { int refresh,close,resize; } swg_intent;
enum { SWG_ROOT=1,SWG_SCROLL=10,SWG_THUMB,SWG_STATUS=20,SWG_EMPTY,
       SWG_FILTER_BUTTON=40,SWG_REFRESH,SWG_CLOSE,SWG_BACK,SWG_APPLY,SWG_QUERY,SWG_DETAIL_TEXT,SWG_RESIZE,
       SWG_CONTENTS_BUTTON,SWG_CONTENT_TREE=200,SWG_FILES,SWG_ACTIONS,SWG_FILES_EXPAND,SWG_ACTIONS_EXPAND,
       SWG_FILE_ROWS=300,SWG_ACTION_ROWS=400,
       SWG_ROWS=1000 };
nx_result swg_create(sw_owner *,nx_font_collection *,swg_style,swg_view **out);
/* Publish NULL before releasing the context. On retirement failure preserve
   the complete view; caller may retry closing. Does not close the owner/window. */
nx_result swg_close(swg_view **);
void swg_bind_window(swg_view *,nx_window *);
nx_context *swg_context(swg_view *);
/* Native backing density, between frames. Logical layout/drafts stay intact;
   replacement contexts inherit it. Failure preserves the configured density. */
nx_result swg_set_raster_scale(swg_view *,float);
/* Borrow immutable optional manifest batch until replacement/close. Retire the
   context before changing it; failure preserves the old binding. Lookup still
   requires exact current source/recipient/row/revision agreement. A non-OK batch
   status is unavailable metadata, distinct from no selected manifest. */
nx_result swg_set_components(swg_view *,const sw_components *,nx_result);
/* Between frames. A new owner snapshot gets a fresh context/provider binding;
   allocation failure preserves the old presentation. No query is dispatched. */
nx_result swg_sync(swg_view *);
nx_result swg_filter(swg_view *,nx_utf8_span); /* applied local metadata filter */
nx_result swg_queue_event(swg_view *,const nx_event *);
/* Owns begin/end_frame and settles internal focus/layout transitions. Consume
   intents only on success. Refresh is a request to the app, not software work. */
nx_result swg_frame(swg_view *,float width,float height,swg_intent *);
size_t swg_matches(const swg_view *);
swg_mode swg_current_mode(const swg_view *);
/* Diagnostics count only measured visible/overscan row layouts, not metadata. */
size_t swg_last_row_layouts(const swg_view *);
#endif
