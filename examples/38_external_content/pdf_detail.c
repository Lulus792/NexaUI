#include "pdf_detail.h"
#include "pdf_frame.h"

#include <math.h>
#include <stdlib.h>

static int same_page(pdf_detail_key a, pdf_detail_key b) {
    return a.generation==b.generation && a.resize==b.resize && a.sequence==b.sequence && a.page==b.page;
}

static int same_request(pdf_detail_key a, pdf_detail_key b) {
    return same_page(a,b) && a.edge==b.edge;
}

nx_rect pdf_detail_destination(content_surface_status base, nx_rect image) {
    float sx=image.width/(float)base.request.width, sy=image.height/(float)base.request.height;
    return (nx_rect){image.x+base.image.x*sx,image.y+base.image.y*sy,base.image.width*sx,base.image.height*sy};
}

void pdf_detail_select(pdf_detail *detail, content_surface_status base, nx_rect image,
    unsigned page, double output_scale) {
    pdf_detail_key wanted={0};
    if (base.sequence && page && base.image.width && base.image.height) {
        nx_rect area=pdf_detail_destination(base,image);
        double size=ceil(fmin(CONTENT_PDF_RASTER_EDGE,fmax(area.width,area.height)*output_scale));
        double staged=fmax(base.image.width,base.image.height);
        double source=fmax(base.image.source_width,base.image.source_height);
        if (size>fmin(staged,source))
            wanted=(pdf_detail_key){base.request.generation,base.request.resize,base.sequence,page,(unsigned)size};
    }
    if (!wanted.edge || !same_page(detail->visible.key,wanted)) {
        free((void *)detail->visible.bitmap.pixels);
        detail->visible=(pdf_detail_result){0};
    }
    if (!same_request(detail->wanted,wanted)) detail->attempted=(pdf_detail_key){0};
    detail->wanted=wanted;
}

int pdf_detail_missing(const pdf_detail *detail) {
    return detail->wanted.edge && detail->visible.key.edge<detail->wanted.edge &&
        !same_request(detail->attempted,detail->wanted);
}

int pdf_detail_adopt(pdf_detail *detail, pdf_detail_result result) {
    if (!detail->wanted.edge || !same_request(detail->wanted,result.key)) {
        free((void *)result.bitmap.pixels);
        return 0;
    }
    detail->attempted=result.key;
    if (result.result==NX_OK && result.bitmap.pixels) {
        free((void *)detail->visible.bitmap.pixels);
        detail->visible=result;
    } else free((void *)result.bitmap.pixels);
    return 1;
}

void pdf_detail_clear(pdf_detail *detail) {
    free((void *)detail->visible.bitmap.pixels);
    *detail=(pdf_detail){0};
}
