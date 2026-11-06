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
typedef enum removal_action_kind { REMOVAL_NONE, REMOVAL_CLOSE, REMOVAL_REQUEST } removal_action_kind;
typedef struct removal_action { removal_action_kind kind; removal_request request; } removal_action;
typedef struct removal_owner removal_owner;
typedef struct removal_review removal_review;
typedef struct removal_work {size_t row_layouts,settling_frames;double indexed_extent;} removal_work;
/* Example diagnostics for workload checks; excludes title/button layouts. */
void removal_review_last_work(const removal_review *view,removal_work *out);

/* Create once for one user intent, retain across all view replacements. Strings
 * and entries are copied. Reinitializing an owner is not a retry/recovery API. */
nx_result removal_owner_create(const removal_plan *plan,removal_owner **out);
void removal_owner_destroy(removal_owner *owner);
/* Owner thread only. Invalidating never rearms a request or retargets its plan. */
void removal_owner_invalidate(removal_owner *owner);
unsigned removal_owner_requests(const removal_owner *owner);
nx_result removal_review_create(removal_owner *owner,nx_font_collection *fonts,removal_review **out);
void removal_review_destroy(removal_review *view);
nx_context *removal_review_context(removal_review *view);
/* Owner and fonts outlive the view; all calls stay on their owner thread.
 * REQUEST is returned once, with exact identity, and performs no backend call.
 * Neither acceptance nor completion is inferred from returning this action. */
nx_result removal_review_frame(removal_review *view,float width,float height,removal_action *action);
#endif
