#ifndef EXAMPLE_AETHERI_PICKER_H
#define EXAMPLE_AETHERI_PICKER_H
#include "worker.h"
#define FP_PAGE_ROWS 16u
enum { FP_ROOT = 10, FP_UP_BUTTON, FP_REFRESH_BUTTON, FP_HIDDEN_BUTTON,
       FP_QUERY, FP_APPLY, FP_PATH, FP_STATUS, FP_PREVIOUS, FP_NEXT,
       FP_COUNTS, FP_LIST, FP_SCROLL, FP_THUMB, FP_OPEN, FP_CANCEL,
       FP_RETRY, FP_COMPACT_HELP, FP_RESIZE, FP_RECOVER,
       FP_ANCESTORS = 100, FP_ROWS = 1000 };
typedef struct fp_picker {
    fp_worker *worker; /* Borrowed; host stops/joins after GUI retirement. */
    nx_font_collection *fonts;
    nx_text_field *query;
    nx_sim_message observation;
    uint64_t view, request, ticket, selected, restore;
    uint64_t revealed_focus;
    uint64_t resume_focus;
    fp_command last_command;
    fp_operation operation;
    nx_result error;
    int busy, german, expanded, rebind, retryable, compact, layout_compact;
    int directories; /* Set once after init, before the first request/frame. */
    size_t page;
    float text_size, scroll_y;
    double layout_width;
    float layout_size;
    float revealed_height;
    float size_width, size_text, size_footer;
    int size_german, size_supported;
    nx_text_layout *labels[16], *crumbs[AE_FILES_DEPTH + 1], *rows[FP_PAGE_ROWS];
    size_t layout_page;
    uint64_t layout_state;
} fp_picker;
typedef struct fp_intent { int requested, cancel, resize; fp_command command; } fp_intent;
nx_result fp_picker_init(fp_picker *, fp_worker *, nx_font_collection *, uint64_t view);
void fp_picker_destroy(fp_picker *);
/* Call only between successful UI frames. One active request per picker. */
nx_result fp_picker_request(fp_picker *, const fp_command *);
/* Correlates ticket AND copied view/request. Takes message ownership only for
   a current valid reply; caller must dispose the remaining message in all cases.
   Current malformed replies become errors. Host retires native context on 1. */
int fp_picker_observe(fp_picker *, nx_sim_message *);
const fp_reply *fp_picker_snapshot(const fp_picker *);
/* Return 1 when an unmodified row-navigation key is handled. No backend call. */
int fp_picker_key(fp_picker *, uint64_t focus, const nx_event *, fp_intent *);
int fp_picker_reveal(fp_picker *, const nx_context *, float height);
/* Host retires context/queued input when this layout mode changes. The model,
   query draft and selection remain owned independently of window geometry. */
int fp_picker_compact(fp_picker *, float width, float height);
int fp_picker_prepare_size(fp_picker *, uint64_t current_focus, float width,
                           float height, fp_intent *pending);
nx_result fp_picker_build(fp_picker *, nx_context *, nx_window *, float width,
                         float height, fp_intent *, int *redraw);
#endif
