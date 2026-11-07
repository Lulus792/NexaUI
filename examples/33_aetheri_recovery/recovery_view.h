#ifndef EXAMPLE_RECOVERY_VIEW_H
#define EXAMPLE_RECOVERY_VIEW_H
#include "view_model.h"
/* Optional example presentation, not a public SDK/authority contract. Owner,
   fonts and any bound window outlive this single-threaded view. */
typedef struct rvg_view rvg_view;
typedef struct rvg_style { int german; float text_size,raster_scale; nx_theme_kind theme; } rvg_style;
typedef enum rvg_mode { RVG_LIST, RVG_FILTER, RVG_DETAIL } rvg_mode;
typedef struct rvg_intent { int refresh,close; } rvg_intent;
enum { RVG_ROOT=1,RVG_SCROLL=10,RVG_THUMB,RVG_STATUS=20,RVG_EMPTY,
       RVG_FILTER_BUTTON=40,RVG_REFRESH,RVG_CLOSE,RVG_BACK,RVG_APPLY,RVG_QUERY,RVG_DETAIL_TEXT,
       RVG_ROWS=1000 };
nx_result rvg_create(rv_owner *,nx_font_collection *,rvg_style,rvg_view **out);
/* Publish NULL before releasing the context. On retirement failure preserve
   the complete view; caller may retry closing. Does not close the owner/window. */
nx_result rvg_close(rvg_view **);
void rvg_bind_window(rvg_view *,nx_window *);
nx_context *rvg_context(rvg_view *);
/* Between frames. A new owner snapshot gets a fresh context/provider binding;
   allocation failure preserves the old presentation. No query is dispatched. */
nx_result rvg_sync(rvg_view *);
nx_result rvg_filter(rvg_view *,nx_utf8_span); /* applied local metadata filter */
nx_result rvg_queue_event(rvg_view *,const nx_event *);
/* Owns begin/end_frame and settles internal focus/layout transitions. Consume
   intents only on success. Refresh is a request to the app, not recovery work. */
nx_result rvg_frame(rvg_view *,float width,float height,rvg_intent *);
size_t rvg_matches(const rvg_view *);
rvg_mode rvg_current_mode(const rvg_view *);
/* Diagnostics count only measured visible/overscan row layouts, not metadata. */
size_t rvg_last_row_layouts(const rvg_view *);
#endif
