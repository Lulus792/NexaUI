#ifndef EXAMPLE_AETHERI_MANIFEST_DOCUMENT_H
#define EXAMPLE_AETHERI_MANIFEST_DOCUMENT_H
#include <nexaui/nexaui.h>
#include "core/fixture.h"
/* Application-private owned canonical manifest. The original verifier remains
   the sole decoder/hash implementation. Expected artifact provenance and all
   publisher/admission/payload checks remain caller responsibilities. */
typedef struct amh_document amh_document;
/* *out must be NULL. Copy bytes before validation; outputs stay unchanged on
   failure. Returned fixture text borrows the immutable document, not the input. */
nx_result amh_document_project(const unsigned char *,size_t,const unsigned char[32],amh_document **out);
/* Bounded regular-file/no-follow/nonblocking read, never a declared path/action.
   Filesystem I/O still has no deadline. Call on a worker, not in a GUI frame. */
nx_result amh_document_read(const char *,const unsigned char[32],amh_document **out);
const struct ae_fixture *amh_document_data(const amh_document *);
const unsigned char *amh_document_artifact(const amh_document *);
void amh_document_free(amh_document **);
#endif
