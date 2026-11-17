#ifndef EXAMPLE_CONTENT_SURFACE_H
#define EXAMPLE_CONTENT_SURFACE_H
#include <nexaui/nexaui.h>

typedef struct content_surface content_surface;
typedef struct content_request {
    uint64_t generation, resize;
    uint32_t width, height;
} content_request;
/* In-process owned snapshot layout. The header is followed by height*stride
   bytes of straight-alpha RGBA8, then optional UTF-8 text for the same frame.
   The producer finishes writing before publishing and supplies the disposer. */
typedef struct content_frame_header {
    content_request request;
    uint64_t sequence;
    uint32_t format, stride;
    /* Optional changed rectangle in physical source pixels. Outside it, the
       full image must match the stated base in this same request. Set all
       five fields to zero when the producer has no damage information. */
    uint64_t damage_base;
    uint32_t damage_x, damage_y, damage_width, damage_height;
    uint32_t text_bytes; /* Unpadded length, without a NUL terminator. */
} content_frame_header;
typedef struct content_surface_status {
    content_request request;
    uint64_t sequence, copied_frames;
    size_t staging_bytes;
    size_t last_copy_bytes; /* Bytes copied for sequence, excluding row padding. */
} content_surface_status;

#define CONTENT_FRAME_LIMIT (64u * 1024u * 1024u)
#define CONTENT_FRAME_TEXT_LIMIT (32u * 1024u)

/* Reference consumer of existing public APIs; no installed SDK API/engine.
   Borrow exchange until destroy; one UI owner thread. Generation is nonzero
   and must change when the app replaces/closes a content session, even when
   dimensions are unchanged. The app owns that identity and worker lifetime.
   max_bytes bounds both staging and an accepted frame payload, <=64MiB.
   Allocator/disposer callbacks must not reenter/mutate this surface. */
nx_result content_surface_create(nx_snapshot_exchange *exchange, uint64_t generation,
    size_t max_bytes, const nx_allocator *allocator, content_surface **out);
/* Preserves object/pixels and returns STATE while a viewport lease is live. */
nx_result content_surface_destroy(content_surface **surface);
/* Caller begins/ends the UI frame and renders its command stream. Resize
   records a new request; a missing/stale frame returns STATE and rolls back
   this viewport. Host controls can still be painted. A successful nonempty
   draw requires complete AFTER rendering, before another draw or destroy. */
nx_result content_surface_draw(content_surface *surface, nx_context *context,
    const nx_viewport_desc *desc, nx_interaction_state *out);
nx_result content_surface_complete(content_surface *surface);
nx_result content_surface_get_status(const content_surface *surface,
    content_surface_status *out);
/* Borrow the accepted frame's text until the next draw/resize or destroy. */
nx_result content_surface_get_text(const content_surface *surface, nx_utf8_span *out);
#endif
