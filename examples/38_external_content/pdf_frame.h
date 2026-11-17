#ifndef EXAMPLE_CONTENT_PDF_FRAME_H
#define EXAMPLE_CONTENT_PDF_FRAME_H

#include "surface.h"

/* Run an app-selected pdftoppm executable on one page (one-based) of a
   regular PDF file. Absolute paths, no shell, private input/output fds.
   The caller owns the executable, document admission and worker thread.
   On success the outputs follow the same owned contract as content_png_frame. */
nx_result content_pdf_frame(const char *renderer, const char *document, unsigned page,
    content_request request, uint64_t sequence, void **out, size_t *out_size);

#endif
