#ifndef EXAMPLE_CONTENT_GUEST_FRAME_H
#define EXAMPLE_CONTENT_GUEST_FRAME_H

#include "surface.h"

#define CONTENT_GUEST_INPUT_LIMIT 128u
#define CONTENT_GUEST_RECORD_SIZE 5u
enum content_guest_input_kind { GUEST_KEY=1, GUEST_BUTTON, GUEST_MOTION };
/* Edge records are kind, code, pressed, zero, zero. Motion records encode
   signed 16-bit X/Y increments in little-endian order after the kind. */
typedef struct content_guest_batch {
    unsigned count;
    uint8_t records[CONTENT_GUEST_INPUT_LIMIT][CONTENT_GUEST_RECORD_SIZE];
} content_guest_batch;

/* The owner sends a complete guest PNG. The worker decodes and fits it to
   the physical UI request while preserving the whole guest display. */
nx_result content_guest_frame(const char *path,content_request request,uint64_t sequence,
    unsigned kind,const content_guest_batch *input,void **out,size_t *bytes);

#endif
