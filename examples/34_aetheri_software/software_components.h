#ifndef EXAMPLE_SOFTWARE_COMPONENTS_H
#define EXAMPLE_SOFTWARE_COMPONENTS_H
#include "software_model.h"
#include "../31_aetheri_manifest/manifest_document.h"
/* Explicit trusted owner configuration. No source discovery or GUI-selected
   path. The worker copies paths/IDs; expected artifact comes from the original
   selected app observation, never from a later UI selection or this filename. */
typedef struct sw_manifest_source {sw_id install;const char *path;} sw_manifest_source;
typedef struct sw_components sw_components;
/* Original verified document must agree with every identity that joins it to
   the observed app. Metadata agreement creates no publisher/admission trust. */
int sw_component_identity_matches(const sw_row *,const amh_document *);
/* Worker-only; observations must be a complete validated sw_read result.
   Selected sources may be missing/fail independently. Never opens a source for
   an unobserved/ineligible target. Duplicates/oversize fail the whole batch.
   *out must be NULL; failed batch allocation preserves output. */
nx_result sw_components_read(const sw_result *,const sw_manifest_source *,size_t,sw_components **out);
/* UI-thread lookup against the current retained immutable snapshot. A stale
   source/recipient/target/revision never exposes its document. NX_OK with NULL
   document means no manifest was selected for this target, not no dependencies.
   Other statuses mean unavailable metadata. Borrowed document outlives neither
   the batch nor its replacement. Exact zero declared dependencies is only a
   property of an available verified matching document, not installed state. */
nx_result sw_components_get(const sw_components *,const sw_snapshot *,sw_id,const amh_document **);
void sw_components_free(sw_components **);
#endif
