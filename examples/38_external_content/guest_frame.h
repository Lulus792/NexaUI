#ifndef EXAMPLE_CONTENT_GUEST_FRAME_H
#define EXAMPLE_CONTENT_GUEST_FRAME_H

#include "surface.h"

#define CONTENT_GUEST_INPUT_LIMIT 128u
#define CONTENT_GUEST_RECORD_SIZE 5u
#define GUEST_SHARE_PATH_LIMIT 96u
#define GUEST_CLIPBOARD_LIMIT 192u
enum content_guest_input_kind { GUEST_KEY=1, GUEST_BUTTON, GUEST_MOTION };
/* Edge records are kind, code, pressed, zero, zero. Motion records encode
   signed 16-bit X/Y increments in little-endian order after the kind. */
typedef struct content_guest_batch {
    unsigned count;
    uint8_t records[CONTENT_GUEST_INPUT_LIMIT][CONTENT_GUEST_RECORD_SIZE];
} content_guest_batch;

enum { CONTENT_GUEST_RESOURCES=7, CONTENT_GUEST_RESET_PREVIEW=8,
       CONTENT_GUEST_RESET_APPLY=9, CONTENT_GUEST_RESET_STATUS=10, CONTENT_GUEST_SHARING=11,
       CONTENT_GUEST_REVOKE_FILES=12, CONTENT_GUEST_GRANT_FILES=13,
       CONTENT_GUEST_CLIPBOARD=14, CONTENT_GUEST_CLIPBOARD_READ=15,
       GUEST_RESOURCE_RUNNING=1, GUEST_RESOURCE_PAUSED=2,
       GUEST_RESOURCE_DISK=1, GUEST_RESOURCE_WRITABLE=2,
       GUEST_RESET_APPLIED=1, GUEST_RESET_REJECTED, GUEST_RESET_UNKNOWN, GUEST_RESET_UNREQUESTED };
enum { GUEST_SHARE_FIXED=0, GUEST_SHARE_ACTIVE, GUEST_SHARE_PENDING, GUEST_SHARE_REVOKED, GUEST_SHARE_UNKNOWN,
       GUEST_SHARE_OFFERED, GUEST_SHARE_ATTACHING };
enum { GUEST_CLIPBOARD_SEND=1, GUEST_CLIPBOARD_READ=2 };
typedef struct content_guest_resources {
    unsigned state, flags, cpus;
    uint64_t ram, capacity, read, written;
} content_guest_resources;
typedef struct content_guest_reset_preview {
    uint8_t environment[16], checkpoint[32], confirmation[16];
    uint64_t capacity;
    char file_share[GUEST_SHARE_PATH_LIMIT+1];
} content_guest_reset_preview;
typedef struct content_guest_sharing {
    unsigned state, clipboard;
    uint8_t environment[16];
    char file_share[GUEST_SHARE_PATH_LIMIT+1];
} content_guest_sharing;
typedef struct content_guest_clipboard {
    size_t bytes;
    char text[GUEST_CLIPBOARD_LIMIT+1u];
} content_guest_clipboard;

/* The owner sends a complete guest PNG. The worker decodes and fits it to
   the physical UI request while preserving the whole guest display. */
nx_result content_guest_frame(const char *path,content_request request,uint64_t sequence,
    unsigned kind,const content_guest_batch *input,void **out,size_t *bytes);
/* Separately read the host VM owner's resource snapshot, not a guest frame. */
nx_result content_guest_read_resources(const char *path,content_request request,content_guest_resources *out);
nx_result content_guest_read_sharing(const char *path,content_request request,content_guest_sharing *out);
nx_result content_guest_file_action(const char *path,content_request request,unsigned kind,const uint8_t environment[16],
    content_guest_sharing *out);
int content_guest_clipboard_valid(const char *text,size_t bytes);
nx_result content_guest_send_clipboard(const char *path,content_request request,uint64_t ticket,
    const uint8_t environment[16],const content_guest_clipboard *text,unsigned *out);
nx_result content_guest_read_clipboard(const char *path,content_request request,uint64_t ticket,
    const uint8_t environment[16],content_guest_clipboard *out);
/* Reads the owner's disk and baseline identity. This request has no reset effect. */
nx_result content_guest_read_reset_preview(const char *path,content_request request,content_guest_reset_preview *out);
nx_result content_guest_reset_action(const char *path,content_request request,unsigned kind,
    const content_guest_reset_preview *scope,unsigned *out);

#endif
