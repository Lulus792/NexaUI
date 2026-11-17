#ifndef EXAMPLE_PDF_PROPERTIES_H
#define EXAMPLE_PDF_PROPERTIES_H

#include <stddef.h>
#include <stdint.h>

#define CONTENT_PDF_PROPERTIES_FORMAT 1u
#define CONTENT_PDF_PROPERTY_LIMIT 512u
enum { CONTENT_PDF_TITLE, CONTENT_PDF_AUTHOR, CONTENT_PDF_SUBJECT, CONTENT_PDF_PROPERTY_COUNT };

/* Fixed local ABI for the renderer companion's --properties reply. A bit in
   unavailable marks a field that could not be admitted to this size limit. */
typedef struct pdf_properties_packet {
    uint32_t format, page_count, major, minor, unavailable;
    char fields[CONTENT_PDF_PROPERTY_COUNT][CONTENT_PDF_PROPERTY_LIMIT+1u];
} pdf_properties_packet;
#define CONTENT_PDF_PROPERTIES_BYTES (offsetof(pdf_properties_packet,fields)+ \
    CONTENT_PDF_PROPERTY_COUNT*(CONTENT_PDF_PROPERTY_LIMIT+1u))

static inline int pdf_property_control(uint32_t scalar) {
    return scalar<0x20u || (scalar>=0x7fu && scalar<=0x9fu) || scalar==0x2028u || scalar==0x2029u;
}

#endif
