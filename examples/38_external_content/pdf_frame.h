#ifndef EXAMPLE_CONTENT_PDF_FRAME_H
#define EXAMPLE_CONTENT_PDF_FRAME_H

#include "surface.h"

/* Run an app-selected pdftoppm executable on one page (one-based) of a
   regular PDF file. Absolute paths, no shell, private input/output fds.
   The caller owns the executable, document admission and worker thread.
   On success the outputs follow the same owned contract as content_png_frame. */
nx_result content_pdf_frame(const char *renderer, const char *document, unsigned page,
    content_request request, uint64_t sequence, void **out, size_t *out_size);
/* App-selected pdftotext reads the same open document and page. Nonempty
   UTF-8 text and any verified word boxes are appended to the image snapshot.
   The file is immutable across the tool calls. A failed text extraction
   preserves the caller's outputs. Unavailable boxes leave the text intact. */
nx_result content_pdf_frame_with_text(const char *renderer, const char *reader, const char *document,
    unsigned page, content_request request, uint64_t sequence, void **out, size_t *out_size);

#endif
