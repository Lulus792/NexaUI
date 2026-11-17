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

/* Pin one admitted file to a document session. The caller owns the returned
   descriptor and keeps its contents immutable. Rendering and indexing borrow
   its seek position on one worker, so calls must be serialized. */
nx_result content_pdf_open(const char *path, int *out_file);
nx_result content_pdf_render(const char *renderer, const char *reader, int file, unsigned page,
    content_request request, uint64_t sequence, void **out, size_t *out_size);

#define CONTENT_PDF_PAGE_LIMIT 256u
#define CONTENT_PDF_INDEX_LIMIT (512u * 1024u)
#define CONTENT_PDF_QUERY_LIMIT 128u

typedef struct content_pdf_index content_pdf_index;
typedef struct content_pdf_hits {
    size_t total;
    unsigned next_page; /* Next different hit page, wrapping once, or zero. */
} content_pdf_hits;

/* Build an immutable index off the UI thread. A missing separator, invalid
   UTF-8 or exceeded limit leaves the output untouched. Free with the named
   destroy call. Searches borrow the index and never start a child. */
nx_result content_pdf_index_read(const char *reader, int file, content_pdf_index **out);
void content_pdf_index_destroy(content_pdf_index *index);
unsigned content_pdf_index_pages(const content_pdf_index *index);
nx_result content_pdf_index_find(const content_pdf_index *index, nx_utf8_span query,
    unsigned current_page, content_pdf_hits *out);

#endif
