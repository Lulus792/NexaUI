#ifndef NEXAUI_EDITOR_PRESENTATION_H
#define NEXAUI_EDITOR_PRESENTATION_H
#include "nexaui/nexaui.h"
#include <stdbool.h>

/* Owner-side temporary projection of the original physical rows. No font,
   layout, model or context pointer survives a call. The SDK copies the result. */
#define EDITOR_PRESENTATION_MAX_BOXES 65536u
struct editor_presentation {
    nx_text_document_box *boxes,*work;
    size_t count,capacity,work_capacity,limit,last_end;
    nx_result error;
    bool have_row;
};
void editor_presentation_reset(struct editor_presentation *p);
void editor_presentation_close(struct editor_presentation *p);
nx_result editor_presentation_row(struct editor_presentation *p,
    const nx_text_layout *layout,size_t base,size_t break_bytes,
    nx_point origin,nx_rect clip);
/* Override only the current insertion caret, e.g. after the original IME paint. */
nx_result editor_presentation_caret(struct editor_presentation *p,size_t byte,
    nx_rect bounds,nx_rect clip);
nx_result editor_presentation_finish(struct editor_presentation *p,
    nx_text_document_presentation *out);
#endif
