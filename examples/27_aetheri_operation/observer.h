#ifndef OPERATION_OBSERVER_H
#define OPERATION_OBSERVER_H
#include <nexaui/nexaui.h>
#include "catalogue/store.h"

/* Example adapter only, not an installed SDK API. The application's original
   catalogue and immutable manifest/fonts outlive the view. The exact request is
   copied at creation. Neither construction, paint nor close accepts a request. */
typedef struct operation_observer operation_observer;
typedef enum observer_action { OBSERVER_NONE, OBSERVER_REFRESH, OBSERVER_CLOSE } observer_action;
nx_result observer_create(struct ae_catalogue *catalogue,const struct ae_fixture_request *request,
    const uint8_t *manifest,size_t length,nx_font_collection *fonts,operation_observer **out);
void observer_destroy(operation_observer *view);
/* Owner-thread original read API; the caller schedules this explicitly. On an
   error, paint reports unavailable rather than presenting old progress as fresh. */
int observer_refresh(operation_observer *view);
nx_context *observer_context(operation_observer *view);
nx_result observer_frame(operation_observer *view,float width,float height,observer_action *action);
#endif
