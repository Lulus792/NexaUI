#ifndef NEXAUI_AETHERI_TERMINAL_PASTE_REVIEW_H
#define NEXAUI_AETHERI_TERMINAL_PASTE_REVIEW_H
#include "paste_flow.h"

/* Example modal: logical viewport, 16-column escaped preview.
   The owner retains the staged flow, font collection and matching guard; it
   must deliver confirmed bytes or reset before destroying those objects. */
typedef struct terminal_paste_review terminal_paste_review;
enum terminal_review_result { TERMINAL_REVIEW_OPEN, TERMINAL_REVIEW_CANCELLED, TERMINAL_REVIEW_CONFIRMED };
nx_result terminal_paste_review_create(terminal_paste_flow *flow,const nx_font_collection *fonts,
    ae_term_width width,terminal_paste_review **out);
void terminal_paste_review_destroy(terminal_paste_review *review);
nx_context *terminal_paste_review_context(terminal_paste_review *review);
/* Set logical window size, text pixels per em (16..32), and language (0 EN,
   1 DE). A candidate is checked against all preview rows before commitment;
   unsupported layouts return LIMIT and retain the prior settings and paste.
   The width callback must be deterministic and cannot reenter or mutate the
   review or model. Resizing preserves the visible source position where possible. */
nx_result terminal_paste_review_configure(terminal_paste_review *review,float width,float height,
    float pixels_per_em,int german);
/* Build a frame after queuing events. Guard changes cancel; confirmed bytes are
   borrowed from flow until reset. On any error, do not present the frame;
   destroy the review rather than reusing a possibly incomplete frame. */
nx_result terminal_paste_review_frame(terminal_paste_review *review,terminal_paste_guard guard,
    enum terminal_review_result *result,nx_utf8_span *confirmed);
#endif
