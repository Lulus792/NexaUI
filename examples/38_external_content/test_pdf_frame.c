#define _POSIX_C_SOURCE 200809L
#include "pdf_frame.h"
#include "png_frame.h"

#include <errno.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int engine_probe(void) {
    int mode = getchar();
    if (mode == 's') { sleep(30); return 0; }
    if (mode == 'm' || mode == 'g' || mode == 'x') {
        const uint8_t pixels[16 * 8 * 4] = {0};
        png_image image = {.version=PNG_IMAGE_VERSION, .width=16, .height=8, .format=PNG_FORMAT_RGBA};
        return png_image_write_to_stdio(&image, stdout, 0, pixels, 0, NULL) ? 0 : 2;
    }
    if (mode != 'o') return 1;
    char block[4096] = {0};
    for (size_t bytes=0;bytes<=CONTENT_PNG_ENCODED_LIMIT;bytes+=sizeof(block))
        if (fwrite(block, 1, sizeof(block), stdout) != sizeof(block)) return 2;
    return 0;
}

static const char plain_text[] = "écho go\n";

static int text_probe(const char *output) {
    int mode = getchar();
    if (mode != 'm' && mode != 'g' && mode != 'x') return 2;
    if (!strcmp(output, "-nopgbrk")) return fputs(plain_text, stdout) == EOF ? 2 : 0;
    if (strcmp(output, "-tsv")) return 2;
    return printf("level\tpage_num\tpar_num\tblock_num\tline_num\tword_num\tleft\ttop\twidth\theight\tconf\ttext\n"
        "1\t1\t0\t0\t0\t0\t0\t0\t16\t8\t-1\t###PAGE###\n"
        "5\t1\t0\t0\t0\t0\t2\t2\t4\t4\t100\técho\n"
        "5\t1\t0\t0\t0\t1\t8\t2\t2\t4\t100\t%s\n", mode == 'm' ? "changed" : "go") < 0 ? 2 : 0;
}

static int link_probe(void) {
    int mode=getchar();
    pdf_link_packet packet={.format=CONTENT_PDF_LINK_FORMAT,.source_page=1,.page_count=2,
        .count=1,.width=16,.height=8,.links={{2,2,4,4,mode=='x'?3u:2u}}};
    size_t bytes=offsetof(pdf_link_packet,links)+sizeof(pdf_link_area);
    return fwrite(&packet,1,bytes,stdout)!=bytes;
}

static int index_probe(void) {
    switch (getchar()) {
    case 'i': return fputs("Résumé page one\n\n\f\fGrün page two\n\n\f",stdout)==EOF;
    case 't': return fputs("Missing page terminator",stdout)==EOF;
    case 'u': return fputs("\xc3\f",stdout)==EOF;
    case 'p':
        for (unsigned page = 0; page <= CONTENT_PDF_PAGE_LIMIT; ++page)
            if (putchar('\f') == EOF) return 1;
        return 0;
    case 'b': {
        char block[4096] = {0};
        for (size_t i = 0; i <= CONTENT_PDF_INDEX_LIMIT; i += sizeof(block))
            if (fwrite(block,1,sizeof(block),stdout) != sizeof(block)) return 1;
        return 0;
    }
    default: return 1;
    }
}

static int probe(const char *executable, char mode, nx_result expected) {
    char file[] = "/tmp/nexaui-pdf-probe-XXXXXX";
    int fd = mkstemp(file);
    if (fd < 0) return 0;
    int ok = write(fd, &mode, 1) == 1;
    if (close(fd)) ok = 0;
    struct { int marker; void *output; size_t bytes; } state = {0, NULL, 913};
    state.output = &state.marker;
    if (ok) ok = content_pdf_frame(executable, file, 1, (content_request){1,1,16,16},
        1, &state.output, &state.bytes) == expected;
    if (unlink(file)) ok = 0;
    int status;
    return ok && state.output == &state.marker && state.bytes == 913 &&
        waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD;
}

static int probe_words(const char *executable, char mode, uint32_t expected_count, int with_links) {
    char file[] = "/tmp/nexaui-pdf-words-XXXXXX";
    int fd = mkstemp(file);
    if (fd < 0) return 0;
    int ok = write(fd, &mode, 1) == 1;
    if (close(fd)) ok = 0;
    void *frame = NULL;
    size_t bytes = 0;
    if (ok && with_links) {
        int document=-1;
        ok=content_pdf_open(file,&document)==NX_OK;
        if (ok) ok=content_pdf_render_with_links(executable,executable,executable,document,1,
            (content_request){1,1,16,16},1,&frame,&bytes)==NX_OK;
        if (document>=0 && close(document)) ok=0;
    } else if (ok) ok = content_pdf_frame_with_text(executable, executable, file, 1,
        (content_request){1,1,16,16}, 1, &frame, &bytes) == NX_OK;
    if (unlink(file)) ok = 0;
    const size_t text_start = sizeof(content_frame_header) + 16u * 16u * 4u;
    const size_t text_size = sizeof(plain_text) - 1u;
    unsigned expected_links=with_links && mode=='g';
    size_t link_start=text_start+text_size+expected_count*sizeof(content_frame_word);
    if (ok) ok = bytes==link_start+expected_links*sizeof(content_frame_link);
    if (ok) {
        content_frame_header header;
        memcpy(&header, frame, sizeof(header));
        ok = header.word_count == expected_count && header.text_bytes == text_size &&
            header.link_count == expected_links &&
            !memcmp((const uint8_t *)frame + text_start, plain_text, text_size);
    }
    if (ok && expected_links) {
        content_frame_link link;
        memcpy(&link,(const uint8_t *)frame+link_start,sizeof(link));
        ok=link.target_page==2 && link.bounds.x==.125f && link.bounds.y==.375f &&
            link.bounds.width==.25f && link.bounds.height==.25f;
    }
    if (ok && expected_count) {
        content_frame_word words[2];
        memcpy(words, (const uint8_t *)frame + text_start + text_size, sizeof(words));
        ok = words[0].start_byte == 0 && words[0].end_byte == 5 &&
            words[0].bounds.x == .125f && words[0].bounds.y == .375f &&
            words[0].bounds.width == .25f && words[0].bounds.height == .25f &&
            words[1].start_byte == 6 && words[1].end_byte == 8 && words[1].bounds.x == .5f;
    }
    free(frame);
    return ok;
}

static int probe_index(const char *executable, char mode, nx_result expected) {
    char path[] = "/tmp/nexaui-pdf-index-XXXXXX";
    int seed = mkstemp(path);
    if (seed < 0) return 0;
    int ok = write(seed,&mode,1) == 1;
    if (close(seed)) ok = 0;
    int file = -1;
    if (ok) ok = content_pdf_open(path,&file)==NX_OK;
    if (unlink(path)) ok = 0;
    content_pdf_index *index = NULL;
    if (ok) ok = content_pdf_index_read(executable,file,&index)==expected;
    if (file >= 0 && close(file)) ok = 0;
    if (ok && expected == NX_OK) {
        content_pdf_hits hits;
        nx_utf8_span query = {"page",4};
        ok = content_pdf_index_pages(index)==3 &&
            content_pdf_index_find(index,query,1,&hits)==NX_OK && hits.total==2 && hits.next_page==3 &&
            content_pdf_index_find(index,query,3,&hits)==NX_OK && hits.total==2 && hits.next_page==1 &&
            content_pdf_index_find(index,(nx_utf8_span){"ü",2},2,&hits)==NX_OK && hits.total==1 && hits.next_page==3 &&
            content_pdf_index_find(index,(nx_utf8_span){"\f",1},2,&hits)==NX_OK && !hits.total && !hits.next_page;
    } else if (index) ok = 0;
    content_pdf_index_destroy(index);
    return ok;
}

int main(int argc, char **argv) {
    if ((argc == 10 || argc == 11) && !strcmp(argv[1], "-png")) return engine_probe();
    if (argc == 2 && !strcmp(argv[1],"1")) return link_probe();
    if (argc == 12 && !strcmp(argv[1], "-f")) return text_probe(argv[9]);
    if (argc == 11 && !strcmp(argv[4], "257")) return index_probe();
    if (argc != 1) return 2;
    if (!probe(argv[0], 's', NX_ERROR_PLATFORM) || !probe(argv[0], 'o', NX_ERROR_LIMIT) ||
        !probe(argv[0], 'b', NX_ERROR_ARGUMENT) || !probe_words(argv[0],'g',2,0) ||
        !probe_words(argv[0],'m',0,0) || !probe_words(argv[0],'g',2,1) ||
        !probe_words(argv[0],'x',2,1) || !probe_index(argv[0],'i',NX_OK) ||
        !probe_index(argv[0],'t',NX_ERROR_ARGUMENT) || !probe_index(argv[0],'u',NX_ERROR_ARGUMENT) ||
        !probe_index(argv[0],'p',NX_ERROR_LIMIT) || !probe_index(argv[0],'b',NX_ERROR_LIMIT)) return 1;
    puts("PDF worker verified: deadlines, word bounds, pinned document, indexed blank page and search limits");
    return 0;
}
