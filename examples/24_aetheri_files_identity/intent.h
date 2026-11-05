#ifndef NEXAUI_AETHERI_FILES_INTENT_H
#define NEXAUI_AETHERI_FILES_INTENT_H
#include "files/model.h"

/* This owner-side experiment pins the selected regular file and its parent.
   It is neither a grant nor an authorization to read or change the file. */
typedef struct files_intent {
    int directory, witness;
    char leaf[AE_FILES_NAME+1];
} files_intent;

/* Initialize only fresh storage; clear before reinitialization. */
void files_intent_init(files_intent *intent);
void files_intent_clear(files_intent *intent);
/* Snapshot selected target only when the complete current query was accepted.
   A failure preserves an already staged intent. */
enum ae_files_result files_intent_stage(files_intent *intent,struct ae_files *model,int query_accepted);
/* Resolve against the current held directory and visible snapshot, returning
   newly owned CLOEXEC FDs and literal leaf. The current selection is restored.
   Output storage must be distinct from the intent and from other outputs.
   Any failure clears provided outputs and leaves the staged intent intact. */
enum ae_files_result files_intent_resolve(const files_intent *intent,struct ae_files *model,
    int query_accepted,int *directory,int *witness,char leaf[AE_FILES_NAME+1]);
#endif
