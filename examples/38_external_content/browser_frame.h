#ifndef EXAMPLE_CONTENT_BROWSER_FRAME_H
#define EXAMPLE_CONTENT_BROWSER_FRAME_H

#include "surface.h"

/* Request one complete CPU frame from the app's private browser service.
   The worker calls this with an owned absolute socket path. On success the
   caller owns a free-compatible snapshot for this request and sequence. */
nx_result content_browser_frame(const char *socket_path,content_request request,
    uint64_t sequence,unsigned kind,void **out,size_t *bytes);

#endif
