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
    if (mode == 'm' || mode == 'g') {
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
    if (mode != 'm' && mode != 'g') return 2;
    if (!strcmp(output, "-nopgbrk")) return fputs(plain_text, stdout) == EOF ? 2 : 0;
    if (strcmp(output, "-tsv")) return 2;
    return printf("level\tpage_num\tpar_num\tblock_num\tline_num\tword_num\tleft\ttop\twidth\theight\tconf\ttext\n"
        "1\t1\t0\t0\t0\t0\t0\t0\t16\t8\t-1\t###PAGE###\n"
        "5\t1\t0\t0\t0\t0\t2\t2\t4\t4\t100\técho\n"
        "5\t1\t0\t0\t0\t1\t8\t2\t2\t4\t100\t%s\n", mode == 'g' ? "go" : "changed") < 0 ? 2 : 0;
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

static int probe_words(const char *executable, char mode, uint32_t expected_count) {
    char file[] = "/tmp/nexaui-pdf-words-XXXXXX";
    int fd = mkstemp(file);
    if (fd < 0) return 0;
    int ok = write(fd, &mode, 1) == 1;
    if (close(fd)) ok = 0;
    void *frame = NULL;
    size_t bytes = 0;
    if (ok) ok = content_pdf_frame_with_text(executable, executable, file, 1,
        (content_request){1,1,16,16}, 1, &frame, &bytes) == NX_OK;
    if (unlink(file)) ok = 0;
    const size_t text_start = sizeof(content_frame_header) + 16u * 16u * 4u;
    const size_t text_size = sizeof(plain_text) - 1u;
    if (ok) ok = bytes == text_start + text_size + expected_count * sizeof(content_frame_word);
    if (ok) {
        content_frame_header header;
        memcpy(&header, frame, sizeof(header));
        ok = header.word_count == expected_count && header.text_bytes == text_size &&
            !memcmp((const uint8_t *)frame + text_start, plain_text, text_size);
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

int main(int argc, char **argv) {
    if (argc == 10 && !strcmp(argv[1], "-png")) return engine_probe();
    if (argc == 12 && !strcmp(argv[1], "-f")) return text_probe(argv[9]);
    if (argc != 1) return 2;
    if (!probe(argv[0], 's', NX_ERROR_PLATFORM) || !probe(argv[0], 'o', NX_ERROR_LIMIT) ||
        !probe(argv[0], 'b', NX_ERROR_ARGUMENT) || !probe_words(argv[0], 'g', 2) ||
        !probe_words(argv[0], 'm', 0)) return 1;
    puts("PDF worker verified: deadline, output cap, reaping, UTF-8 word bounds, margins and text-only fallback");
    return 0;
}
