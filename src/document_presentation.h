#ifndef NEXAUI_DOCUMENT_PRESENTATION_H
#define NEXAUI_DOCUMENT_PRESENTATION_H
#include "nexaui/nexaui.h"

/* Private, relocation-safe owned data; offsets address this complete blob.
   All arrays are aligned. No caller/model/font/layout pointer is stored. */
typedef struct nx_document_attributes {
    size_t count, offset;
} nx_document_attributes;
typedef struct nx_document_attribute {
    size_t name, value;
} nx_document_attribute;
typedef struct nx_document_attribute_run {
    size_t start, end;
    nx_document_attributes attributes;
} nx_document_attribute_run;
typedef struct nx_document_presentation {
    size_t bytes, text_bytes, box_count, box_offset, run_count, run_offset;
    nx_document_attributes defaults;
} nx_document_presentation;

/* Align only the actual stored types; some supported C17 SDK headers do not
   declare max_align_t. The caller's allocator already must align these types. */
typedef union nx_document_presentation_alignment {
    nx_document_presentation header;
    nx_text_document_box box;
    nx_document_attribute attribute;
    nx_document_attribute_run run;
} nx_document_presentation_alignment;
#define NX_DOCUMENT_PRESENTATION_ALIGNMENT _Alignof(nx_document_presentation_alignment)

/* System-allocated staging, freed with free(). All source validation and copying
   precede any application allocator. Complete value is an already copied UTF-8
   document. No native coordinate conversions or model callbacks occur here. */
nx_result nx_document_presentation_build(nx_utf8_span value,
    const nx_text_document_presentation *description,nx_document_presentation **out);
nx_result nx_document_presentation_character(const nx_document_presentation *p,
    size_t byte,nx_rect *out);
nx_result nx_document_presentation_range(const nx_document_presentation *p,
    size_t start,size_t end,nx_rect *out);
nx_result nx_document_presentation_point(const nx_document_presentation *p,
    nx_point point,size_t *out_byte);
nx_document_attributes nx_document_presentation_attributes(const nx_document_presentation *p,
    size_t byte,size_t *start,size_t *end);
const char *nx_document_attribute_name(const nx_document_presentation *p,
    nx_document_attributes attributes,size_t index);
const char *nx_document_attribute_value(const nx_document_presentation *p,
    nx_document_attributes attributes,size_t index);
const nx_document_presentation *nx_context_document_presentation(const nx_context *context,uint64_t id);
#endif
