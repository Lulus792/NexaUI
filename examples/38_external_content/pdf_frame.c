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

static nx_result render_page(const char *renderer, int document, unsigned page, uint32_t edge,
    uint8_t *encoded, size_t *bytes) {
    int output[2];
    if (pipe2(output, O_CLOEXEC)) return NX_ERROR_PLATFORM;
    char page_number[16], size[16];
    snprintf(page_number, sizeof(page_number), "%u", page);
    snprintf(size, sizeof(size), "%u", (unsigned)edge);
    char *argv[] = {(char *)renderer, "-png", "-singlefile", "-f", page_number, "-l", page_number,
        "-scale-to", size, "-", NULL};
    posix_spawn_file_actions_t actions;
    int err = posix_spawn_file_actions_init(&actions);
    pid_t child = -1;
    if (!err) {
        err = posix_spawn_file_actions_addclose(&actions, output[0]);
        if (!err) err = posix_spawn_file_actions_adddup2(&actions, document, STDIN_FILENO);
        if (!err) err = posix_spawn_file_actions_adddup2(&actions, output[1], STDOUT_FILENO);
        if (!err) err = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        if (!err) err = posix_spawn(&child, renderer, &actions, NULL, argv, environ);
        posix_spawn_file_actions_destroy(&actions);
    }
    close(output[1]);
    if (err) { close(output[0]); return err == ENOMEM ? NX_ERROR_MEMORY : NX_ERROR_PLATFORM; }

    int64_t start = now_ms(), now = start;
    const size_t cap = CONTENT_PNG_ENCODED_LIMIT + 1u;
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

nx_result content_pdf_frame(const char *renderer, const char *document, unsigned page,
    content_request request, uint64_t sequence, void **out, size_t *out_size) {
    if (!renderer || *renderer != '/' || !document || *document != '/' || !page || page > INT_MAX ||
        !request.generation || !request.resize || !request.width || !request.height || !sequence ||
        !out || !out_size) return NX_ERROR_ARGUMENT;
    if (request.width > NX_VIEWPORT_MAX_DIMENSION || request.height > NX_VIEWPORT_MAX_DIMENSION ||
        (size_t)request.width * 4u > CONTENT_PNG_PIXEL_LIMIT / request.height) return NX_ERROR_LIMIT;
    int file = open(document, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    if (file < 0) return NX_ERROR_PLATFORM;
    struct stat attributes;
    nx_result result = NX_ERROR_ARGUMENT;
    if (!fstat(file, &attributes) && S_ISREG(attributes.st_mode) && attributes.st_size > 0) {
        result = NX_ERROR_LIMIT;
        if (attributes.st_size <= 16 * 1024 * 1024) {
            uint8_t *encoded = malloc(CONTENT_PNG_ENCODED_LIMIT + 1u);
            if (encoded) {
                uint32_t edge = request.width > request.height ? request.width : request.height;
                if (edge > 1024u) edge = 1024u;
                size_t bytes = 0;
                result = render_page(renderer, file, page, edge, encoded, &bytes);
                if (result == NX_OK) result = content_png_frame(encoded, bytes, request, sequence, out, out_size);
                free(encoded);
            } else result = NX_ERROR_MEMORY;
        }
    }
    close(file);
    return result;
}
