#define _POSIX_C_SOURCE 200809L
#include "pdf_frame.h"
#include "png_frame.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int engine_probe(void) {
    int mode = getchar();
    if (mode == 's') { sleep(30); return 0; }
    if (mode != 'o') return 1;
    char block[4096] = {0};
    for (size_t bytes=0;bytes<=CONTENT_PNG_ENCODED_LIMIT;bytes+=sizeof(block))
        if (fwrite(block, 1, sizeof(block), stdout) != sizeof(block)) return 2;
    return 0;
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

int main(int argc, char **argv) {
    if (argc > 1) return argc==10 && !strcmp(argv[1],"-png") ? engine_probe() : 2;
    if (!probe(argv[0], 's', NX_ERROR_PLATFORM) || !probe(argv[0], 'o', NX_ERROR_LIMIT) ||
        !probe(argv[0], 'b', NX_ERROR_ARGUMENT)) return 1;
    puts("PDF worker limits verified: deadline, output cap, failure and reaping");
    return 0;
}
