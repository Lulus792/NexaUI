#ifndef NEXAUI_AETHERI_RENAME_REVIEW_H
#define NEXAUI_AETHERI_RENAME_REVIEW_H
#include <nexaui/nexaui.h>
#include "files/rename.h"
typedef struct rename_review rename_review;
typedef enum rename_review_action { REVIEW_NONE, REVIEW_CANCEL, REVIEW_CONFIRM, REVIEW_BACKGROUND } rename_review_action;
typedef struct rename_review_owner { struct ae_files_rename *operation; int submitted; } rename_review_owner;
/* Initialize once per operation; retain this owner while replacing views and do
   not reinitialize it to retry a pending or uncertain operation. */
nx_result rename_review_owner_init(rename_review_owner *owner,struct ae_files_rename *operation);
/* Owner-thread only. The owner, operation and font collection outlive the
   view. The view never applies or closes the operation; the owner rechecks it
   and acts on explicit confirmation. */
nx_result rename_review_create(rename_review_owner *owner,nx_font_collection *fonts,rename_review **out);
void rename_review_destroy(rename_review *review);
nx_result rename_review_frame(rename_review *review,rename_review_action *action);
nx_context *rename_review_context(rename_review *review);
#endif
