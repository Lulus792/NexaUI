#ifndef EXAMPLE_ASYNC_HIERARCHY_VIEW_H
#define EXAMPLE_ASYNC_HIERARCHY_VIEW_H

#include "nexaui/nexaui.h"

/* Example presentation contract, not a filesystem/component/service model.
   One owner thread uses a view. Channel and fonts outlive it. Workers receive
   only copied keys and immutable response bytes, never this object or a context. */
#define AH_CHILD_LIMIT 16u
#define AH_NAME_BYTES 96u
typedef struct ah_key { uint64_t view, generation, branch; } ah_key;
typedef struct ah_child { uint64_t id; char name[AH_NAME_BYTES]; } ah_child;
typedef struct ah_response {
    ah_key key;
    size_t count;
    ah_child children[AH_CHILD_LIMIT];
} ah_response;
typedef enum ah_phase { AH_UNQUERIED, AH_LOADING, AH_FAILED, AH_READY, AH_EMPTY } ah_phase;
typedef struct ah_view {
    nx_sim_channel *channel;
    nx_font_collection *fonts;
    uint64_t epoch, generation, ticket, selected;
    ah_phase phase;
    int expanded, german, restore_focus, exhausted;
    nx_result error;
    size_t count;
    ah_child children[AH_CHILD_LIMIT];
    uint64_t previous_ids[AH_CHILD_LIMIT];
    size_t previous_count;
    nx_text_layout *layouts[AH_CHILD_LIMIT + 4u];
    float text_size;
    float layout_text_size, scroll_y;
    double layout_width;
    int layout_german, layout_exhausted;
    ah_phase layout_phase;
    uint64_t layout_revision, revision;
} ah_view;

enum { AH_TREE = 10, AH_BRANCH = 11, AH_EXPANDER = 12, AH_STATUS = 13,
       AH_RETRY = 14, AH_REFRESH = 15, AH_SCROLL = 16, AH_THUMB = 17 };
enum { AH_INTENT_NONE, AH_INTENT_LOAD, AH_INTENT_RETRY, AH_INTENT_REFRESH };

nx_result ah_init(ah_view *view, nx_sim_channel *channel,
                  nx_font_collection *fonts, uint64_t unique_epoch);
void ah_destroy(ah_view *view);
/* Owner thread, between frames. Invalidate before starting an explicit refresh;
   queue-full/allocation errors become a retryable UI failure, never completion. */
nx_result ah_request(ah_view *view);
void ah_collapse(ah_view *view);
/* Never takes ownership of the message; its consumer must dispose it once.
   Retired/unrelated tickets return 0. Malformed active replies become failures. */
int ah_observe(ah_view *view, const nx_sim_message *completion);
/* Build inside an open frame; act on out_intent only after successful end_frame.
   out_redraw requests a second frame for same-frame visible focus restoration.
   IDs above are reserved by this example; owner data supplies distinct child IDs. */
nx_result ah_build(ah_view *view, nx_context *context, uint64_t parent,
                   nx_rect bounds, int *out_intent, int *out_redraw);
const char *ah_status(const ah_view *view);

#endif
