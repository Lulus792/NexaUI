#ifndef EXAMPLE_CONTENT_GUEST_FRAME_H
#define EXAMPLE_CONTENT_GUEST_FRAME_H

#include "surface.h"

#define CONTENT_GUEST_KEY_LIMIT 64u
typedef struct content_guest_keys {
    unsigned count;
    uint8_t bytes[CONTENT_GUEST_KEY_LIMIT*2u];
} content_guest_keys;

/* The owner sends a complete guest PNG. The worker decodes and fits it to
   the physical UI request while preserving the whole guest display. */
nx_result content_guest_frame(const char *path,content_request request,uint64_t sequence,
    unsigned kind,const content_guest_keys *keys,void **out,size_t *bytes);

#endif
