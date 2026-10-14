#ifndef NEXAUI_PLATFORM_URI_LIST_H
#define NEXAUI_PLATFORM_URI_LIST_H

#include "drop_queue.h"

/* Appends local file paths from text/uri-list as one atomic drop batch.
   Unsupported or malformed URI lines are ignored. */
nx_result nx_platform_uri_list_append(nx_platform_drop_queue *queue,
                                      const char *text, size_t length,
                                      float x, float y,
                                      const char *local_host);

#endif
