#define _POSIX_C_SOURCE 200809L
#include "pdf_links.h"
#include <poppler.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned destination(PopplerDocument *document, const PopplerAction *action) {
    if (!action || action->type != POPPLER_ACTION_GOTO_DEST || !action->goto_dest.dest) return 0;
    const PopplerDest *dest = action->goto_dest.dest;
    PopplerDest *named = NULL;
    if (dest->type == POPPLER_DEST_NAMED && dest->named_dest)
        dest = named = poppler_document_find_dest(document,dest->named_dest);
    int page = dest && dest->type != POPPLER_DEST_NAMED ? dest->page_num : 0;
    if (named) poppler_dest_free(named);
    return page > 0 ? (unsigned)page : 0;
}

static int web_address(const PopplerAction *action, char out[CONTENT_PDF_URI_LIMIT+1u]) {
    if (!action || action->type!=POPPLER_ACTION_URI || !action->uri.uri) return 0;
    const char *address=action->uri.uri;
    size_t length=strnlen(address,CONTENT_PDF_URI_LIMIT+1u);
    if (!length || length>CONTENT_PDF_URI_LIMIT) return 0;
    memcpy(out,address,length+1u);
    if (!content_pdf_web_uri(out)) return 0;
    GUri *uri=g_uri_parse(address,G_URI_FLAGS_NONE,NULL);
    if (!uri) return 0;
    const char *host=g_uri_get_host(uri);
    int ok=host&&*host&&!g_uri_get_userinfo(uri);
    g_uri_unref(uri);
    return ok;
}

static int emit_links(PopplerDocument *document, unsigned page_number) {
    unsigned pages = (unsigned)poppler_document_get_n_pages(document);
    if (page_number > pages) return 1;
    PopplerPage *page = poppler_document_get_page(document,(int)page_number-1);
    if (!page) return 1;
    pdf_link_packet packet = {.format=CONTENT_PDF_LINK_FORMAT,.source_page=page_number,.page_count=pages};
    poppler_page_get_size(page,&packet.width,&packet.height);
    GList *mappings = poppler_page_get_link_mapping(page);
    for (const GList *item = g_list_last(mappings); item; item = item->prev) {
        const PopplerLinkMapping *mapping = item->data;
        pdf_link_area area={0};
        unsigned target = destination(document,mapping->action);
        if (target) {
            if (target>pages||target==page_number) continue;
            area.target_page=target;
        } else if (!web_address(mapping->action,area.uri)) continue;
        PopplerRectangle box = mapping->area;
        if (!isfinite(box.x1)||!isfinite(box.x2)||!isfinite(box.y1)||!isfinite(box.y2)) continue;
        double left = fmax(0,box.x1), right = fmin(packet.width,box.x2);
        double top = fmax(0,packet.height-box.y2), bottom = fmin(packet.height,packet.height-box.y1);
        if (!(right > left && bottom > top)) continue;
        if (packet.count == CONTENT_PDF_LINK_LIMIT) break;
        area.x=left;
        area.y=top;
        area.width=right-left;
        area.height=bottom-top;
        packet.links[packet.count++]=area;
    }
    poppler_page_free_link_mapping(mappings);
    g_object_unref(page);
    size_t bytes = offsetof(pdf_link_packet,links) + packet.count * sizeof(*packet.links);
    return fwrite(&packet,1,bytes,stdout) != bytes || fflush(stdout);
}

int main(int argc, char **argv) {
    if (argc != 2 || *argv[1] < '0' || *argv[1] > '9') return 2;
    char *end;
    errno = 0;
    unsigned long page = strtoul(argv[1],&end,10);
    if (errno || *end || !page || page > INT_MAX) return 2;
    PopplerDocument *document = poppler_document_new_from_fd(STDIN_FILENO,NULL,NULL);
    if (!document) return 1;
    int result = emit_links(document,(unsigned)page);
    g_object_unref(document);
    return result;
}
