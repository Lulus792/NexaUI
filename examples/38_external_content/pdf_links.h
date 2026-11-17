#ifndef EXAMPLE_PDF_LINKS_H
#define EXAMPLE_PDF_LINKS_H

#include <stdint.h>

#define CONTENT_PDF_LINK_LIMIT 32u
#define CONTENT_PDF_LINK_FORMAT 1u

/* Private process protocol. Read and write with the same native ABI.
   The payload contains only the first count entries. Coordinates are
   top-left oriented PDF points within the rotated crop box. */
typedef struct pdf_link_area {
    double x, y, width, height;
    uint32_t target_page;
} pdf_link_area;

typedef struct pdf_link_packet {
    uint32_t format, source_page, page_count, count;
    double width, height;
    pdf_link_area links[CONTENT_PDF_LINK_LIMIT];
} pdf_link_packet;

#endif
