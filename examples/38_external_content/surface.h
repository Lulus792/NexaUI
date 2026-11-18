#ifndef EXAMPLE_CONTENT_SURFACE_H
#define EXAMPLE_CONTENT_SURFACE_H
#include <nexaui/nexaui.h>
#include "pdf_links.h"
#include "browser_ax.h"

typedef struct content_surface content_surface;
typedef struct content_request {
    uint64_t generation, resize;
    uint32_t width, height;
} content_request;
typedef struct content_frame_word {
    uint32_t start_byte, end_byte;
    nx_rect bounds; /* Normalized to the entire image, including margins. */
} content_frame_word;
typedef struct content_frame_link {
    nx_rect bounds; /* Normalized to the entire image, including margins. */
    uint32_t target_page;
    char uri[CONTENT_PDF_URI_LIMIT+1u];
} content_frame_link;
/* Placement of the decoded image within the padded frame, in frame pixels.
   A producer without a fitted image leaves all six fields zero. */
typedef struct content_image_area {
    uint32_t x, y, width, height;
    uint32_t source_width, source_height;
} content_image_area;
/* Oriented dimensions before decode scaling. Both are zero for non-JPEG frames. */
typedef struct content_jpeg_size {
    uint32_t width, height;
} content_jpeg_size;
/* In-process owned snapshot layout. The header is followed by height*stride
   bytes of straight-alpha RGBA8, then optional UTF-8 text and unaligned
   content_frame_word, content_frame_link and content_ax_node records for
   the same frame.
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
    uint32_t word_count, link_count;
    content_image_area image;
    content_jpeg_size jpeg;
    uint32_t ax_count,ax_epoch;
} content_frame_header;
typedef struct content_surface_status {
    content_request request;
    uint64_t sequence, copied_frames;
    size_t staging_bytes;
    size_t last_copy_bytes; /* Bytes copied for sequence, excluding row padding. */
    content_image_area image;
    content_jpeg_size jpeg;
} content_surface_status;

#define CONTENT_FRAME_LIMIT (64u * 1024u * 1024u)
#define CONTENT_FRAME_TEXT_LIMIT (32u * 1024u)
#define CONTENT_FRAME_WORD_LIMIT 1024u

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
/* Borrow a copy of the completed frame commands with this surface's image
   destination and optional replacement pixels. The caller owns replacement
   pixels through the renderer call. The original viewport clip and base
   lease apply. Consume before completing the draw or mapping again. */
nx_result content_surface_map_commands(content_surface *surface,
    const nx_command *commands, size_t count, nx_rect image,
    const nx_bitmap_view *replacement, const nx_command **out);
nx_result content_surface_get_status(const content_surface *surface,
    content_surface_status *out);
/* Place the fitted pixels inside the transformed padded canvas. */
nx_rect content_image_destination(content_surface_status base,nx_rect canvas);
/* Borrow the accepted frame's text until the next draw/resize or destroy. */
nx_result content_surface_get_text(const content_surface *surface, nx_utf8_span *out);
/* Same borrow lifetime as text. Byte endpoints refer to that text. */
nx_result content_surface_get_words(const content_surface *surface,
    const content_frame_word **words, size_t *count);
nx_result content_surface_get_links(const content_surface *surface,
    const content_frame_link **links, size_t *count);
nx_result content_surface_get_ax(const content_surface *surface,const content_ax_node **nodes,
    size_t *count,uint32_t *epoch);
/* Validate the encoded in-process node array before publishing it. */
int content_ax_valid(const uint8_t *source,size_t count,content_request request);
#endif
