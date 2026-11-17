#ifndef EXAMPLE_PDF_LINKS_H
#define EXAMPLE_PDF_LINKS_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define CONTENT_PDF_LINK_LIMIT 32u
#define CONTENT_PDF_LINK_FORMAT 2u
#define CONTENT_PDF_URI_LIMIT 256u

/* The child parses the URI. Check the small transport envelope again before
   staging or dispatch. Delimiters in the authority must remain literal. */
static inline int content_pdf_web_uri(const char uri[CONTENT_PDF_URI_LIMIT+1u]) {
    const char *end=memchr(uri,0,CONTENT_PDF_URI_LIMIT+1u);
    if (!end) return 0;
    size_t length=(size_t)(end-uri);
    size_t scheme=length>=8u&&!memcmp(uri,"https://",8u)?8u:
        length>=7u&&!memcmp(uri,"http://",7u)?7u:0u;
    if (!scheme) return 0;
    size_t i=scheme;
    for (;i<length && !strchr("/?#",uri[i]);++i)
        if (uri[i]=='@'||uri[i]=='%') return 0;
    if (i==scheme) return 0;
    for (i=scheme;i<length;++i) {
        unsigned char c=(unsigned char)uri[i];
        if (c<=0x20u || c>=0x7fu || strchr("\\<>{}\"|^`",c)) return 0;
    }
    return 1;
}

/* Private process protocol. Read and write with the same native ABI.
   The payload contains only the first count entries. Coordinates are
   top-left oriented PDF points within the rotated crop box. */
typedef struct pdf_link_area {
    double x, y, width, height;
    uint32_t target_page; /* Zero for a web address. */
    char uri[CONTENT_PDF_URI_LIMIT+1u];
} pdf_link_area;

typedef struct pdf_link_packet {
    uint32_t format, source_page, page_count, count;
    double width, height;
    pdf_link_area links[CONTENT_PDF_LINK_LIMIT];
} pdf_link_packet;

#endif
