#ifndef NEXAUI_EXAMPLE_REMOVAL_REVIEW_H
#define NEXAUI_EXAMPLE_REMOVAL_REVIEW_H
#include <nexaui/nexaui.h>

/* Example-owned presentation seam, not an Aetheri protocol or installed SDK API.
 * The authenticated owner supplies a complete immutable plan and revalidates its
 * identity, revision, authorization and inventory before accepting any request. */
enum { REMOVAL_MAX_ENTRIES=100000 };
typedef enum removal_disposition {
    REMOVAL_REMOVE, REMOVAL_SHARED, REMOVAL_KEEP,
    REMOVAL_DOCUMENT, REMOVAL_OTHER_COPY, REMOVAL_UNKNOWN
} removal_disposition;
typedef struct removal_entry {
    uint64_t semantic_id; /* Unique, >=1000; stable object identity within the view. */
    unsigned char object[16];
    removal_disposition disposition;
    char label[160], reason[256];
    uint64_t logical_bytes; /* Logical size only, never a reclaimed-space promise. */
} removal_entry;
typedef struct removal_plan {
    unsigned char plan[16], installation[16], scope[16];
    uint64_t revision;
    char component[160], scope_label[160];
    int inventory_complete;
    size_t count;
    removal_entry *entries; /* Borrowed count-element input; copied at creation. */
} removal_plan;
typedef struct removal_request {
    unsigned char plan[16], installation[16], scope[16];
    uint64_t revision;
} removal_request;
typedef enum removal_action_kind { REMOVAL_NONE, REMOVAL_CLOSE, REMOVAL_REQUEST, REMOVAL_REFRESH } removal_action_kind;
typedef struct removal_action { removal_action_kind kind; removal_request request; } removal_action;
typedef struct removal_owner removal_owner;
typedef struct removal_review removal_review;
typedef struct removal_work {size_t row_layouts,settling_frames;double indexed_extent;} removal_work;
/* Example diagnostics for workload checks; excludes title/button layouts. */
void removal_review_last_work(const removal_review *view,removal_work *out);

/* Create once for one user intent, retain across all view replacements. Strings
 * and entries are copied. Reinitializing an owner is not a retry/recovery API. */
nx_result removal_owner_create(const removal_plan *plan,removal_owner **out);
/* Copy a strictly newer revision of the same plan/installation/scope. Mapping
 * between semantic IDs and object IDs must agree wherever consecutive snapshots
 * overlap. Failure preserves the owner and every existing view. Existing views
 * retain their old immutable snapshot and become ineligible until refreshed.
 * A previously issued request remains issued, with its original correlation. */
nx_result removal_owner_refresh(removal_owner *owner,const removal_plan *plan);
uint64_t removal_owner_revision(const removal_owner *owner);
void removal_owner_destroy(removal_owner *owner);
/* Owner thread only. Invalidating never rearms a request or retargets its plan. */
void removal_owner_invalidate(removal_owner *owner);
unsigned removal_owner_requests(const removal_owner *owner);
nx_result removal_review_create(removal_owner *owner,nx_font_collection *fonts,removal_review **out);
void removal_review_destroy(removal_review *view);
nx_context *removal_review_context(removal_review *view);
uint64_t removal_review_revision(const removal_review *view);
/* Between frames, adopt the owner's latest snapshot with a fresh context.
 * Preserve applied filter, query model/history/selection, theme, density and
 * reduced motion. Composition and queued old-context input are cancelled.
 * Allocation failure preserves the old view/publication. Successful rebinding
 * retires native references; submit a frame before publishing the new context.
 * The old visible row is anchored by key when it survives, otherwise Home.
 * Confirmation is reset. Closed-dialog focus returns to the safe Filter action. */
nx_result removal_review_refresh(removal_review *view);
/* Presentation-only name filter, using nx_search_filter's matching rules.
 * At most 128 UTF-8 bytes, no NUL/newline. Empty clears it. Failure preserves
 * the applied filter and confirmation. A changed filter clears confirmation,
 * resets scroll and never narrows the request's complete owner-supplied plan. */
nx_result removal_review_filter(removal_review *view,nx_utf8_span query);
/* Borrowed whole-review window for IME/clipboard and semantic publication.
 * Do not publish another view into that window while this binding is active.
 * Every bound window must outlive the view/context. NULL disables future binding;
 * it does not retire a previous frame's borrowed window. */
void removal_review_bind_window(removal_review *view,nx_window *window);
int removal_review_filter_open(const removal_review *view);
/* Queue native input. Escape is resolved after earlier queued activations:
 * cancel an open/opening filter, otherwise close this view. */
nx_result removal_review_queue_event(removal_review *view,const nx_event *event);
/* Owner and fonts outlive the view; all calls stay on their owner thread.
 * REQUEST is returned once, with exact identity, and performs no backend call.
 * Neither acceptance nor completion is inferred from returning this action. */
nx_result removal_review_frame(removal_review *view,float width,float height,removal_action *action);
#endif
