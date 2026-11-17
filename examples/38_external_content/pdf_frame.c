#define _GNU_SOURCE
#include "pdf_frame.h"
#include "png_frame.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

static int64_t now_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static nx_result run_tool(const char *tool, char *const argv[], int document,
    uint8_t *encoded, size_t limit, size_t *bytes) {
    int output[2];
    if (pipe2(output, O_CLOEXEC)) return NX_ERROR_PLATFORM;
    posix_spawn_file_actions_t actions;
    int err = posix_spawn_file_actions_init(&actions);
    pid_t child = -1;
    if (!err) {
        err = posix_spawn_file_actions_addclose(&actions, output[0]);
        if (!err) err = posix_spawn_file_actions_adddup2(&actions, document, STDIN_FILENO);
        if (!err) err = posix_spawn_file_actions_adddup2(&actions, output[1], STDOUT_FILENO);
        if (!err) err = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        if (!err) err = posix_spawn(&child, tool, &actions, NULL, argv, environ);
        posix_spawn_file_actions_destroy(&actions);
    }
    close(output[1]);
    if (err) { close(output[0]); return err == ENOMEM ? NX_ERROR_MEMORY : NX_ERROR_PLATFORM; }

    int64_t start = now_ms(), now = start;
    const size_t cap = limit + 1u;
    nx_result result = NX_ERROR_PLATFORM;
    while (start >= 0 && now >= start && now - start < 3000) {
        struct pollfd pending = {output[0], POLLIN, 0};
        int ready = poll(&pending, 1, (int)(3000 - (now - start)));
        if (ready > 0 && (pending.revents & (POLLIN | POLLHUP))) {
            ssize_t count = read(output[0], encoded + *bytes, cap - *bytes);
            if (count == 0) { result = NX_OK; break; }
            if (count < 0 && errno != EINTR) break;
            if (count > 0) *bytes += (size_t)count;
            if (*bytes == cap) { result = NX_ERROR_LIMIT; break; }
        } else if (ready >= 0 || errno != EINTR) break;
        now = now_ms();
    }
    close(output[0]);
    int status = 0;
    pid_t ended = 0;
    if (result == NX_OK) {
        while ((now = now_ms()) >= start && now - start < 3000) {
            ended = waitpid(child, &status, WNOHANG);
            if (ended == child || (ended < 0 && errno != EINTR)) break;
            const struct timespec pause = {0, 1000000};
            nanosleep(&pause, NULL);
        }
        if (ended != child || !WIFEXITED(status)) result = NX_ERROR_PLATFORM;
        else if (WEXITSTATUS(status) != 0) result = NX_ERROR_ARGUMENT;
    }
    if (ended != child && !(ended < 0 && errno == ECHILD)) {
        (void)kill(child, SIGKILL);
        while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {}
    }
    return result;
}

static nx_result render_page(const char *renderer, int document, unsigned page, uint32_t edge,
    int crop, uint8_t *encoded, size_t *bytes) {
    char page_number[16], size[16];
    snprintf(page_number, sizeof(page_number), "%u", page);
    snprintf(size, sizeof(size), "%u", (unsigned)edge);
    char *argv[] = {(char *)renderer, "-png", "-singlefile", "-f", page_number, "-l", page_number,
        "-scale-to", size, "-", NULL, NULL};
    if (crop) { argv[9] = "-cropbox"; argv[10] = "-"; }
    return run_tool(renderer, argv, document, encoded, CONTENT_PNG_ENCODED_LIMIT, bytes);
}

static nx_result attach_text(const char *reader, int document, unsigned page, uint8_t *buffer,
    void **frame, size_t *frame_size) {
    if (lseek(document, 0, SEEK_SET) < 0) return NX_ERROR_PLATFORM;
    char page_number[16];
    snprintf(page_number, sizeof(page_number), "%u", page);
    char *argv[] = {(char *)reader, "-f", page_number, "-l", page_number, "-enc", "UTF-8",
        "-eol", "unix", "-nopgbrk", "-", "-", NULL};
    size_t text_size = 0;
    nx_result result = run_tool(reader, argv, document, buffer, CONTENT_FRAME_TEXT_LIMIT, &text_size);
    if (result != NX_OK || !text_size) return result;
    if (memchr(buffer, 0, text_size) || nx_utf8_validate((const char *)buffer, text_size, NULL) != NX_OK)
        return NX_ERROR_ARGUMENT;
    uint8_t *extended = realloc(*frame, *frame_size + text_size);
    if (!extended) return NX_ERROR_MEMORY;
    content_frame_header header;
    memcpy(&header, extended, sizeof(header));
    header.text_bytes = (uint32_t)text_size;
    memcpy(extended, &header, sizeof(header));
    memcpy(extended + *frame_size, buffer, text_size);
    *frame = extended;
    *frame_size += text_size;
    return NX_OK;
}

/* Poppler writes ASCII decimals with a period. Keep parsing independent of
   the process locale, and reject unsupported spellings or dimensions. */
static int read_decimal(const char *field, double *out) {
    if (*field < '0' || *field > '9' || strlen(field) > 16u) return 0;
    double value = 0;
    while (*field >= '0' && *field <= '9') value = value * 10 + (*field++ - '0');
    if (*field == '.') {
        double place = 0.1;
        ++field;
        if (*field < '0' || *field > '9') return 0;
        while (*field >= '0' && *field <= '9') {
            value += (*field++ - '0') * place;
            place *= 0.1;
        }
    }
    if (*field) return 0;
    *out = value;
    return 1;
}

static size_t skip_space(const char *text, size_t length, size_t offset) {
    while (offset < length && strchr(" \t\r\n\f\v", text[offset])) ++offset;
    return offset;
}

static int pdf_fit_scale(double width, double height, const content_png_fit *fit,
    uint32_t edge, double *scale_x, double *scale_y) {
    if (!(width > 0 && height > 0)) return 0;
    double dpi_scale = edge / (width > height ? width : height);
    double raster_width = width * dpi_scale, raster_height = height * dpi_scale;
    if (!(raster_width >= fit->source_width - 1.01 && raster_width <= fit->source_width + 0.01 &&
          raster_height >= fit->source_height - 1.01 && raster_height <= fit->source_height + 0.01))
        return 0;
    *scale_x = dpi_scale * fit->width / fit->source_width;
    *scale_y = dpi_scale * fit->height / fit->source_height;
    return 1;
}

/* Text and geometry come from different Poppler output modes. Associate
   boxes only if all words, in order, consume the plain text exactly. */
static size_t match_words(char *tsv, const char *text, size_t length, content_request request,
    const content_png_fit *fit, uint32_t edge, content_frame_word words[CONTENT_FRAME_WORD_LIMIT]) {
    static const char columns[] =
        "level\tpage_num\tpar_num\tblock_num\tline_num\tword_num\tleft\ttop\twidth\theight\tconf\ttext\n";
    if (strncmp(tsv, columns, sizeof(columns) - 1u)) return 0;
    char *line = tsv + sizeof(columns) - 1u;
    double page_width = 0, page_height = 0, scale_x = 0, scale_y = 0;
    size_t offset = 0, count = 0;
    while (*line) {
        char *newline = strchr(line, '\n');
        if (!newline) return 0;
        *newline = 0;
        char *field[12] = {line};
        for (size_t i = 1; i < 12; ++i) {
            char *tab = strchr(field[i - 1], '\t');
            if (!tab) return 0;
            *tab = 0;
            field[i] = tab + 1;
        }
        line = newline + 1;
        if (strcmp(field[0], "1") && strcmp(field[0], "5")) continue;
        double x, y, width, height;
        if (!read_decimal(field[6], &x) || !read_decimal(field[7], &y) ||
            !read_decimal(field[8], &width) || !read_decimal(field[9], &height) ||
            width <= 0 || height <= 0) return 0;
        if (!strcmp(field[0], "1")) {
            if (page_width || x || y) return 0;
            if (!pdf_fit_scale(width,height,fit,edge,&scale_x,&scale_y)) return 0;
            page_width = width;
            page_height = height;
            continue;
        }
        offset = skip_space(text, length, offset);
        size_t bytes = strlen(field[11]);
        if (!page_width || x + width > page_width || y + height > page_height ||
            count == CONTENT_FRAME_WORD_LIMIT || !bytes || bytes > length - offset ||
            memcmp(text + offset, field[11], bytes)) return 0;
        /* A TSV row must end at a word boundary in the plain text. */
        size_t end = offset + bytes;
        if (end < length && skip_space(text, length, end) == end) return 0;
        nx_rect bounds = {
            (float)((fit->x + x * scale_x) / request.width),
            (float)((fit->y + y * scale_y) / request.height),
            (float)(width * scale_x / request.width),
            (float)(height * scale_y / request.height)};
        if (!(bounds.width > 0 && bounds.height > 0 &&
            bounds.x + bounds.width <= 1 && bounds.y + bounds.height <= 1)) return 0;
        words[count++] = (content_frame_word){(uint32_t)offset, (uint32_t)end, bounds};
        offset = end;
    }
    return skip_space(text, length, offset) == length ? count : 0;
}

static void attach_words(const char *reader, int document, unsigned page, uint8_t *buffer,
    const content_png_fit *fit, uint32_t edge, void **frame, size_t *frame_size) {
    content_frame_header header;
    memcpy(&header, *frame, sizeof(header));
    if (!header.text_bytes || lseek(document, 0, SEEK_SET) < 0) return;
    char page_number[16];
    snprintf(page_number, sizeof(page_number), "%u", page);
    char *argv[] = {(char *)reader, "-f", page_number, "-l", page_number, "-enc", "UTF-8",
        "-eol", "unix", "-tsv", "-", "-", NULL};
    size_t bytes = 0;
    if (run_tool(reader, argv, document, buffer, 256u * 1024u, &bytes) != NX_OK ||
        !bytes || memchr(buffer, 0, bytes)) return;
    buffer[bytes] = 0;
    content_frame_word words[CONTENT_FRAME_WORD_LIMIT];
    const char *text = (const char *)*frame + *frame_size - header.text_bytes;
    size_t count = match_words((char *)buffer, text, header.text_bytes, header.request, fit, edge, words);
    if (!count) return;
    size_t word_bytes = count * sizeof(*words);
    uint8_t *extended = realloc(*frame, *frame_size + word_bytes);
    if (!extended) return;
    header.word_count = (uint32_t)count;
    memcpy(extended, &header, sizeof(header));
    memcpy(extended + *frame_size, words, word_bytes);
    *frame = extended;
    *frame_size += word_bytes;
}

static void attach_links(const char *reader, int document, unsigned page, uint8_t *buffer,
    const content_png_fit *fit, uint32_t edge, void **frame, size_t *frame_size) {
    if (lseek(document,0,SEEK_SET) < 0) return;
    char page_number[16];
    snprintf(page_number,sizeof(page_number),"%u",page);
    char *argv[] = {(char *)reader,page_number,NULL};
    size_t bytes = 0;
    const size_t prefix = offsetof(pdf_link_packet,links);
    if (run_tool(reader,argv,document,buffer,sizeof(pdf_link_packet),&bytes)!=NX_OK || bytes<prefix) return;
    pdf_link_packet packet;
    memcpy(&packet,buffer,bytes);
    double scale_x, scale_y;
    if (packet.format != CONTENT_PDF_LINK_FORMAT || packet.source_page != page ||
        !packet.count || packet.count>CONTENT_PDF_LINK_LIMIT ||
        packet.page_count<page || packet.page_count>INT_MAX ||
        bytes != prefix + packet.count*sizeof(*packet.links) ||
        !pdf_fit_scale(packet.width,packet.height,fit,edge,&scale_x,&scale_y)) return;
    content_frame_header header;
    memcpy(&header,*frame,sizeof(header));
    content_frame_link links[CONTENT_PDF_LINK_LIMIT];
    for (size_t i=0;i<packet.count;++i) {
        pdf_link_area area=packet.links[i];
        if (!area.target_page || area.target_page>packet.page_count || area.target_page==page ||
            !(area.x>=0 && area.y>=0 && area.width>0 && area.height>0 &&
              area.x+area.width<=packet.width+1e-6 && area.y+area.height<=packet.height+1e-6)) return;
        nx_rect bounds = {
            (float)((fit->x+area.x*scale_x)/header.request.width),
            (float)((fit->y+area.y*scale_y)/header.request.height),
            (float)(area.width*scale_x/header.request.width),
            (float)(area.height*scale_y/header.request.height)};
        if (!(bounds.width>0 && bounds.height>0 &&
              bounds.x+bounds.width<=1 && bounds.y+bounds.height<=1)) return;
        links[i]=(content_frame_link){bounds,area.target_page};
    }
    size_t link_bytes=packet.count*sizeof(*links);
    uint8_t *extended=realloc(*frame,*frame_size+link_bytes);
    if (!extended) return;
    header.link_count=packet.count;
    memcpy(extended,&header,sizeof(header));
    memcpy(extended+*frame_size,links,link_bytes);
    *frame=extended;
    *frame_size+=link_bytes;
}

nx_result content_pdf_open(const char *path, int *out_file) {
    if (!path || *path != '/' || !out_file) return NX_ERROR_ARGUMENT;
    int file = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    if (file < 0) return NX_ERROR_PLATFORM;
    struct stat attributes;
    nx_result result = NX_ERROR_ARGUMENT;
    if (!fstat(file, &attributes) && S_ISREG(attributes.st_mode) && attributes.st_size > 0) {
        if (attributes.st_size <= 16 * 1024 * 1024) {
            *out_file = file;
            return NX_OK;
        }
        result = NX_ERROR_LIMIT;
    }
    close(file);
    return result;
}

nx_result content_pdf_render_with_links(const char *renderer, const char *reader,
    const char *link_reader, int file, unsigned page, content_request request,
    uint64_t sequence, void **out, size_t *out_size) {
    if (!renderer || *renderer != '/' || (reader && *reader != '/') || file < 0 || !page || page > INT_MAX ||
        (link_reader && *link_reader != '/') ||
        !request.generation || !request.resize || !request.width || !request.height || !sequence ||
        !out || !out_size) return NX_ERROR_ARGUMENT;
    if (request.width > NX_VIEWPORT_MAX_DIMENSION || request.height > NX_VIEWPORT_MAX_DIMENSION ||
        (size_t)request.width * 4u > CONTENT_PNG_PIXEL_LIMIT / request.height) return NX_ERROR_LIMIT;
    if (lseek(file, 0, SEEK_SET) < 0) return NX_ERROR_PLATFORM;
    uint8_t *encoded = malloc(CONTENT_PNG_ENCODED_LIMIT + 1u);
    if (!encoded) return NX_ERROR_MEMORY;
    uint32_t edge = request.width > request.height ? request.width : request.height;
    if (edge > 1024u) edge = 1024u;
    size_t bytes = 0;
    nx_result result = render_page(renderer, file, page, edge, link_reader!=NULL, encoded, &bytes);
    void *frame = NULL;
    size_t frame_size = 0;
    content_png_fit fit;
    if (result == NX_OK)
        result = content_png_frame_fit(encoded, bytes, request, sequence, &frame, &frame_size, &fit);
    if (result == NX_OK && reader) {
        result = attach_text(reader, file, page, encoded, &frame, &frame_size);
        if (result == NX_OK) attach_words(reader, file, page, encoded, &fit, edge, &frame, &frame_size);
    }
    if (result == NX_OK && link_reader) attach_links(link_reader,file,page,encoded,&fit,edge,&frame,&frame_size);
    if (result == NX_OK) {
        *out = frame;
        *out_size = frame_size;
    } else free(frame);
    free(encoded);
    return result;
}

nx_result content_pdf_render(const char *renderer, const char *reader, int file, unsigned page,
    content_request request, uint64_t sequence, void **out, size_t *out_size) {
    return content_pdf_render_with_links(renderer,reader,NULL,file,page,request,sequence,out,out_size);
}

static nx_result make_frame(const char *renderer, const char *reader, const char *document, unsigned page,
    content_request request, uint64_t sequence, void **out, size_t *out_size) {
    int file;
    nx_result result = content_pdf_open(document,&file);
    if (result != NX_OK) return result;
    result = content_pdf_render(renderer,reader,file,page,request,sequence,out,out_size);
    close(file);
    return result;
}

nx_result content_pdf_frame(const char *renderer, const char *document, unsigned page,
    content_request request, uint64_t sequence, void **out, size_t *out_size) {
    return make_frame(renderer, NULL, document, page, request, sequence, out, out_size);
}

nx_result content_pdf_frame_with_text(const char *renderer, const char *reader, const char *document,
    unsigned page, content_request request, uint64_t sequence, void **out, size_t *out_size) {
    if (!reader || *reader != '/') return NX_ERROR_ARGUMENT;
    return make_frame(renderer, reader, document, page, request, sequence, out, out_size);
}

struct content_pdf_index {
    unsigned pages;
    uint32_t offsets[CONTENT_PDF_PAGE_LIMIT + 1u];
    uint8_t text[];
};

nx_result content_pdf_index_read(const char *reader, int file, content_pdf_index **out) {
    if (!reader || *reader != '/' || file < 0 || !out) return NX_ERROR_ARGUMENT;
    if (lseek(file,0,SEEK_SET) < 0) return NX_ERROR_PLATFORM;
    content_pdf_index *index = malloc(sizeof(*index) + CONTENT_PDF_INDEX_LIMIT + 1u);
    if (!index) return NX_ERROR_MEMORY;
    char last_page[16];
    snprintf(last_page,sizeof(last_page),"%u",CONTENT_PDF_PAGE_LIMIT+1u);
    char *argv[] = {(char *)reader,"-f","1","-l",last_page,"-enc","UTF-8","-eol","unix","-","-",NULL};
    size_t bytes = 0;
    nx_result result = run_tool(reader,argv,file,index->text,CONTENT_PDF_INDEX_LIMIT,&bytes);
    if (result == NX_OK && (!bytes || index->text[bytes-1] != '\f' || memchr(index->text,0,bytes) ||
        nx_utf8_validate((const char *)index->text,bytes,NULL) != NX_OK)) result = NX_ERROR_ARGUMENT;
    index->pages = 0;
    index->offsets[0] = 0;
    for (size_t offset = 0; result == NX_OK && offset < bytes; ++offset) {
        if (index->text[offset] != '\f') continue;
        if (index->pages == CONTENT_PDF_PAGE_LIMIT) result = NX_ERROR_LIMIT;
        else index->offsets[++index->pages] = (uint32_t)(offset + 1u);
    }
    if (result != NX_OK) {
        free(index);
        return result;
    }
    content_pdf_index *exact = realloc(index,sizeof(*index)+bytes);
    *out = exact ? exact : index;
    return NX_OK;
}

void content_pdf_index_destroy(content_pdf_index *index) { free(index); }

unsigned content_pdf_index_pages(const content_pdf_index *index) { return index ? index->pages : 0; }

nx_result content_pdf_index_find(const content_pdf_index *index, nx_utf8_span query,
    unsigned current_page, content_pdf_hits *out) {
    if (!index || !out || !query.data || !query.length || query.length > CONTENT_PDF_QUERY_LIMIT ||
        !current_page || current_page > index->pages || nx_utf8_validate(query.data,query.length,NULL)!=NX_OK)
        return NX_ERROR_ARGUMENT;
    content_pdf_hits hits = {0};
    /* Visit the current page last. For navigation it contributes to the
       total only. Exclude form feeds so matches never span two pages. */
    for (unsigned i = 0; i < index->pages; ++i) {
        unsigned page = (current_page + i) % index->pages + 1u;
        const uint8_t *begin = index->text + index->offsets[page-1u];
        const uint8_t *end = index->text + index->offsets[page] - 1u;
        const uint8_t *match;
        while ((match = memmem(begin,(size_t)(end-begin),query.data,query.length))) {
            ++hits.total;
            if (!hits.next_page && page != current_page) hits.next_page = page;
            begin = match + query.length;
        }
    }
    *out = hits;
    return NX_OK;
}
