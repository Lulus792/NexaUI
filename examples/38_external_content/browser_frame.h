#ifndef EXAMPLE_CONTENT_BROWSER_FRAME_H
#define EXAMPLE_CONTENT_BROWSER_FRAME_H

#include "surface.h"

#define CONTENT_BROWSER_TEXT_LIMIT 256u
enum content_browser_edit_kind {
    CONTENT_BROWSER_FOCUS=5, CONTENT_BROWSER_PREEDIT, CONTENT_BROWSER_COMMIT, CONTENT_BROWSER_BLUR
};
typedef struct content_browser_edit {
    unsigned kind;
    int32_t begin, end; /* UTF-8 byte offsets, or -1 for hidden composition caret. */
    size_t length;
    char text[CONTENT_BROWSER_TEXT_LIMIT+1u];
} content_browser_edit;

/* Request one complete CPU frame from the app's private browser service.
   The worker calls this with an owned absolute socket path. On success the
   caller owns a free-compatible snapshot for this request and sequence. */
nx_result content_browser_frame(const char *socket_path,content_request request,
    uint64_t sequence,unsigned kind,void **out,size_t *bytes);
nx_result content_browser_edit_frame(const char *socket_path,content_request request,
    uint64_t sequence,const content_browser_edit *edit,void **out,size_t *bytes,nx_rect *caret);

#endif
