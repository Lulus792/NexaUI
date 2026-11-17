#ifndef EXAMPLE_PDF_SELECTION_H
#define EXAMPLE_PDF_SELECTION_H

#include <nexaui/nexaui.h>

enum pdf_selection_copy { PDF_COPY_IDLE, PDF_COPY_SUCCEEDED, PDF_COPY_FAILED };

typedef struct pdf_selection {
    uint64_t identity, revision;
    size_t anchor, cursor, drag_start, drag_end;
    int active, dragging, take_focus;
    enum pdf_selection_copy copy_status;
} pdf_selection;

/* Borrow the accepted text and boxes for this frame. Register/publish the
   same selection that is painted, then process local input for the next
   frame. Native selection requests are applied during registration. */
nx_result pdf_selection_record(pdf_selection *selection, nx_context *context,
    nx_window *window, nx_semantic_desc node, uint64_t identity,
    int enabled, nx_utf8_span text, const nx_text_document_box *boxes,
    size_t count, int *redraw);

#endif
