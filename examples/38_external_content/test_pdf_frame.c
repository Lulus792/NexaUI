#define _POSIX_C_SOURCE 200809L
#include "pdf_frame.h"
#include "png_frame.h"
#include "pdf_thumbnails.h"
#include "pdf_detail.h"
#include "pdf_export.h"

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
    if (mode == 'm' || mode == 'g' || mode == 'x' || mode == 'e') {
        uint8_t pixels[16 * 8 * 4] = {0};
        if (mode=='e') for (unsigned y=0;y<8;++y) for (unsigned x=0;x<16;++x) {
            uint8_t *p=pixels+(y*16u+x)*4u;
            p[0]=(uint8_t)(24+x*10u);p[1]=(uint8_t)(33+y*11u);p[2]=112;p[3]=255;
        }
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
        .count=1,.width=16,.height=8,
        .links={{.x=2,.y=2,.width=4,.height=4,.target_page=mode=='x'?3u:2u}}};
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

static int properties_reply(void) {
    int mode=getchar();
    pdf_properties_packet packet={.format=CONTENT_PDF_PROPERTIES_FORMAT,.page_count=2,.major=1,.minor=4,
        .unavailable=1u<<CONTENT_PDF_SUBJECT,.fields={"Résumé","", ""}};
    size_t bytes=CONTENT_PDF_PROPERTIES_BYTES;
    switch (mode) {
    case 's': --bytes;break;
    case 'p': packet.page_count=0;break;
    case 'v': packet.format=0;break;
    case 'u': packet.fields[CONTENT_PDF_AUTHOR][0]=(char)0xc3;break;
    case 'n': memset(packet.fields[CONTENT_PDF_AUTHOR],'a',sizeof(packet.fields[0]));break;
    case 'c': memcpy(packet.fields[CONTENT_PDF_AUTHOR],"a\nb",4);break;
    case 'f': packet.unavailable|=1u<<CONTENT_PDF_PROPERTY_COUNT;break;
    case 'a': strcpy(packet.fields[CONTENT_PDF_SUBJECT],"Unexpected value");break;
    }
    if (fwrite(&packet,1,bytes,stdout)!=bytes) return 2;
    return mode=='o' && putchar('x')==EOF;
}

static int probe_properties(const char *executable) {
    char path[]="/tmp/nexaui-pdf-properties-XXXXXX";
    int file=mkstemp(path);
    if (file<0) return 0;
    int ok=unlink(path)==0;
    static const char modes[]="gospvuncfa";
    for (size_t i=0;ok && i<sizeof(modes)-1u;++i) {
        pdf_properties_packet packet, sentinel;
        memset(&sentinel,0xa5,sizeof(sentinel));
        memcpy(&packet,&sentinel,sizeof(packet));
        nx_result expected=modes[i]=='g'?NX_OK:modes[i]=='o'?NX_ERROR_LIMIT:NX_ERROR_ARGUMENT;
        ok=pwrite(file,&modes[i],1,0)==1 && content_pdf_read_properties(executable,file,&packet)==expected;
        if (ok && expected==NX_OK)
            ok=packet.page_count==2 && packet.major==1 && packet.minor==4 &&
                packet.unavailable==(1u<<CONTENT_PDF_SUBJECT) && !strcmp(packet.fields[CONTENT_PDF_TITLE],"Résumé") &&
                !*packet.fields[CONTENT_PDF_AUTHOR] && !*packet.fields[CONTENT_PDF_SUBJECT];
        else if (ok) ok=!memcmp(&packet,&sentinel,sizeof(packet));
        if (!ok) fprintf(stderr,"Unexpected PDF properties reply for mode %c\n",modes[i]);
    }
    if (close(file)) ok=0;
    if (ok) puts("PDF properties reply validates size, version, fields and immutable error output");
    return ok;
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

static int preview_completion(pdf_thumbnails *cache, content_request request, unsigned page, int expected) {
    size_t bytes=sizeof(content_frame_header)+(size_t)request.width*request.height*4u;
    void *frame=calloc(1,bytes);
    return frame && pdf_thumbnails_adopt(cache,(pdf_thumbnail){request,page,frame,bytes})==expected;
}

static int probe_thumbnail_cache(void) {
    pdf_thumbnails cache={0};
    int ok=0;
    pdf_thumbnails_configure(&cache,81,1);
    pdf_thumbnails_open(&cache,2);
    content_request original=cache.request;
    if (cache.first!=1 || pdf_thumbnails_missing(&cache,7)!=1 ||
        !preview_completion(&cache,original,1,1) || pdf_thumbnails_missing(&cache,7)!=2) goto done;
    nx_bitmap_view pixels=pdf_thumbnail_pixels(&cache.cells[0]);
    if (!pixels.pixels || pixels.width!=104 || pixels.height!=80 || pixels.stride_bytes!=416) goto done;
    cache.open=0;
    if (pdf_thumbnails_missing(&cache,7)) goto done;
    pdf_thumbnails_configure(&cache,81,1);
    pdf_thumbnails_open(&cache,2);
    if (pdf_thumbnails_missing(&cache,7)!=2 || pdf_thumbnail_pixels(&cache.cells[0]).pixels!=pixels.pixels) goto done;
    pdf_thumbnails_configure(&cache,81,2);
    if (cache.cells[0].frame || cache.request.width!=208 || cache.request.height!=160 ||
        !preview_completion(&cache,original,2,0)) goto done;
    pdf_thumbnails_configure(&cache,81,1);
    if (cache.request.resize==original.resize || !preview_completion(&cache,original,2,0)) goto done;
    content_request before=cache.request;
    pdf_thumbnails_configure(&cache,82,1);
    if (!preview_completion(&cache,before,1,0) || !preview_completion(&cache,cache.request,1,1)) goto done;
    pdf_thumbnails_set_group(&cache,7);
    if (cache.first!=7 || cache.cells[0].frame || pdf_thumbnails_missing(&cache,7)!=7 ||
        !preview_completion(&cache,cache.request,1,0) || !preview_completion(&cache,cache.request,7,1) ||
        pdf_thumbnails_missing(&cache,7)) goto done;
    puts("PDF cache verified: retained frame, scale ABA and retired generation/group rejection");
    ok=1;
done:
    pdf_thumbnails_clear(&cache);
    return ok;
}

static int detail_completion(pdf_detail *detail, pdf_detail_key key, int expected) {
    uint8_t *pixels=calloc(1,4);
    if (!pixels) return 0;
    pdf_detail_result result={.key=key,.bitmap={pixels,1,1,4,NX_BITMAP_RGBA8},.result=NX_OK};
    return pdf_detail_adopt(detail,result)==expected;
}

static int probe_detail_cache(void) {
    pdf_detail detail={0};
    content_surface_status base={.request={7,3,448,208},.sequence=11,.image={16,4,416,200,416,200}};
    nx_rect image={-96,84,672,312};
    int ok=0;
    pdf_detail_select(&detail,base,image,2,1);
    pdf_detail_key first=detail.wanted;
    nx_rect destination=content_image_destination(base,image);
    if (!pdf_detail_missing(&detail) || first.edge!=624 || destination.x!=-72 || destination.y!=90 ||
        destination.width!=624 || destination.height!=300 || !detail_completion(&detail,first,1)) goto done;
    const uint8_t *first_pixels=detail.visible.bitmap.pixels;
    image.x-=32;
    pdf_detail_select(&detail,base,image,2,1);
    if (pdf_detail_missing(&detail) || detail.visible.bitmap.pixels!=first_pixels) goto done;
    pdf_detail_select(&detail,base,image,2,2);
    if (!pdf_detail_missing(&detail) || detail.wanted.edge!=1024 || detail.visible.bitmap.pixels!=first_pixels ||
        !detail_completion(&detail,first,0)) goto done;
    pdf_detail_key second=detail.wanted;
    if (!pdf_detail_adopt(&detail,(pdf_detail_result){.key=second,.result=NX_ERROR_ARGUMENT}) ||
        pdf_detail_missing(&detail) || detail.visible.bitmap.pixels!=first_pixels) goto done;
    ++base.sequence;
    pdf_detail_select(&detail,base,image,2,2);
    if (detail.visible.bitmap.pixels || !pdf_detail_missing(&detail) || !detail_completion(&detail,second,0) ||
        !detail_completion(&detail,detail.wanted,1)) goto done;
    pdf_detail_select(&detail,base,image,1,2);
    if (detail.visible.bitmap.pixels || !pdf_detail_missing(&detail)) goto done;
    puts("PDF detail cache verifies fitted bounds, pan reuse, failed refinement and retired page/frame keys");
    ok=1;
done:
    pdf_detail_clear(&detail);
    return ok;
}

static int probe_export(const char *renderer) {
    char directory[]="/tmp/nexaui-pdf-export-XXXXXX";
    if (!mkdtemp(directory)) return 0;
    char source[sizeof(directory)+16],target[sizeof(directory)+16];
    snprintf(source,sizeof(source),"%s/source",directory);
    snprintf(target,sizeof(target),"%s/viewport.png",directory);
    FILE *seed=fopen(source,"wb");
    int ok=0,file=-1;
    nx_bitmap_view export={0};
    if (!seed) goto done;
    int wrote=fputc('e',seed)!=EOF;
    if (fclose(seed)||!wrote||content_pdf_open(source,&file)!=NX_OK) goto done;
    pdf_export_view view={.viewport={10,20,30,20},.page={6,20,32,16},.width=12,.height=8,.scale=1.25};
    if (content_pdf_export_render(renderer,0,file,1,view,&export)!=NX_OK || export.width!=15 ||
        export.height!=10 || export.stride_bytes!=60 || export.format!=NX_BITMAP_RGBA8) goto done;
    for (unsigned y=0;y<10;++y) for (unsigned x=0;x<15;++x) {
        const uint8_t *p=export.pixels+(y*15u+x)*4u;
        uint8_t expected[4]={255,255,255,255};
        if (x<14&&y<8) { expected[0]=(uint8_t)(44+x*10u);expected[1]=(uint8_t)(33+y*11u);expected[2]=112; }
        if (memcmp(p,expected,sizeof(expected))) goto done;
    }
    const uint8_t *owned=export.pixels;
    view.width=NX_VIEWPORT_MAX_DIMENSION;
    if (content_pdf_export_render(renderer,0,file,1,view,&export)!=NX_ERROR_LIMIT||export.pixels!=owned) goto done;
    const uint8_t replacement[]={7,13,19,255};
    if (content_pdf_export_png(target,export)!=NX_OK ||
        content_pdf_export_png(target,(nx_bitmap_view){replacement,1,1,4,NX_BITMAP_RGBA8})!=NX_ERROR_STATE) goto done;
    png_image png={.version=PNG_IMAGE_VERSION};
    uint8_t decoded[15u*10u*4u];
    ok=png_image_begin_read_from_file(&png,target)&&png.width==15&&png.height==10;
    png.format=PNG_FORMAT_RGBA;
    if (ok) ok=png_image_finish_read(&png,NULL,decoded,0,NULL)&&!memcmp(decoded,export.pixels,sizeof(decoded));
    png_image_free(&png);
done:
    if (file>=0&&close(file)) ok=0;
    free((void *)export.pixels);
    if (unlink(source)&&errno!=ENOENT) ok=0;
    if (unlink(target)&&errno!=ENOENT) ok=0;
    if (rmdir(directory)) ok=0;
    if (ok) puts("PDF export verifies exact explicit-size pixels, bounded dimensions and no-replace PNG publication");
    return ok;
}

int main(int argc, char **argv) {
    if ((argc == 10 || argc == 11) && !strcmp(argv[1], "-png")) return engine_probe();
    if (argc == 2 && !strcmp(argv[1],"1")) return link_probe();
    if (argc == 2 && !strcmp(argv[1],"previews")) return probe_thumbnail_cache()?0:1;
    if (argc == 2 && !strcmp(argv[1],"--properties")) return properties_reply();
    if (argc == 2 && !strcmp(argv[1],"properties")) return probe_properties(argv[0])?0:1;
    if (argc == 2 && !strcmp(argv[1],"detail")) return probe_detail_cache()?0:1;
    if (argc == 2 && !strcmp(argv[1],"export")) return probe_export(argv[0])?0:1;
    if (argc == 12 && !strcmp(argv[1], "-f")) return text_probe(argv[9]);
    if (argc == 11 && !strcmp(argv[4], "257")) return index_probe();
    if (argc != 1) return 2;
    const char good[CONTENT_PDF_URI_LIMIT+1u]="https://example.invalid/?a=b%20c#d";
    const char file[CONTENT_PDF_URI_LIMIT+1u]="file:///tmp/example";
    const char user[CONTENT_PDF_URI_LIMIT+1u]="https://user@example.invalid/";
    const char escaped[CONTENT_PDF_URI_LIMIT+1u]="https://example%2einvalid/";
    if (!content_pdf_web_uri(good) || content_pdf_web_uri(file) ||
        content_pdf_web_uri(user) || content_pdf_web_uri(escaped)) return 1;
    if (!probe(argv[0], 's', NX_ERROR_PLATFORM) || !probe(argv[0], 'o', NX_ERROR_LIMIT) ||
        !probe(argv[0], 'b', NX_ERROR_ARGUMENT) || !probe_words(argv[0],'g',2,0) ||
        !probe_words(argv[0],'m',0,0) || !probe_words(argv[0],'g',2,1) ||
        !probe_words(argv[0],'x',2,1) || !probe_index(argv[0],'i',NX_OK) ||
        !probe_index(argv[0],'t',NX_ERROR_ARGUMENT) || !probe_index(argv[0],'u',NX_ERROR_ARGUMENT) ||
        !probe_index(argv[0],'p',NX_ERROR_LIMIT) || !probe_index(argv[0],'b',NX_ERROR_LIMIT)) return 1;
    puts("PDF worker verified: deadlines, word bounds, pinned document, indexed blank page and search limits");
    return 0;
}
