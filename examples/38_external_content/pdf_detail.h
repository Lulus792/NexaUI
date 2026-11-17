#ifndef CONTENT_PDF_DETAIL_H
#define CONTENT_PDF_DETAIL_H

#include "surface.h"

typedef struct pdf_detail_key {
    uint64_t generation, resize, sequence;
    unsigned page, edge;
} pdf_detail_key;

typedef struct pdf_detail_result {
    pdf_detail_key key;
    nx_bitmap_view bitmap;
    nx_result result;
} pdf_detail_result;

typedef struct pdf_detail {
    pdf_detail_key wanted, attempted;
    pdf_detail_result visible;
} pdf_detail;

/* Called between synchronous presentations. Pan affects placement only. */
void pdf_detail_select(pdf_detail *detail, content_surface_status base, nx_rect image,
    unsigned page, double output_scale);
int pdf_detail_missing(const pdf_detail *detail);
/* Transfers or frees the completion. Failed requests leave the fallback
   usable and are retried only after a change in the desired raster. */
int pdf_detail_adopt(pdf_detail *detail, pdf_detail_result result);
void pdf_detail_clear(pdf_detail *detail);

#endif
