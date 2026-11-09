#ifndef NEXAUI_EDITOR_ATTRIBUTES_H
#define NEXAUI_EDITOR_ATTRIBUTES_H
#include "nexaui/nexaui.h"
#include <stdbool.h>

/* Temporary owner values. No font/layout/model pointer is retained. The SDK
   copies these strings/runs before the next owner frame may overwrite them. */
struct editor_attributes {
    char family[4097],size[32],weight[8],foreground[32],background[32],selected[32];
    nx_text_document_attribute defaults[8],selection[2];
    nx_text_document_attribute_run run;
    size_t default_count,run_count;
    nx_result family_result,style_result;
};
struct editor_attribute_desc {
    const nx_font *font;
    nx_utf8_span text;
    double pixels_per_em,logical_dpi;
    nx_color foreground,background,selected;
    size_t selection_start,selection_end;
    bool selected_text,contrast,editable;
};
/* Only metadata fields are replaced: existing geometry is preserved. Known
   owner colors/size/editability remain available when optional font names or
   style are unsupported. Invalid owner data removes metadata atomically. */
nx_result editor_attributes_project(struct editor_attributes *storage,
    const struct editor_attribute_desc *desc,nx_text_document_presentation *inout);
#endif
