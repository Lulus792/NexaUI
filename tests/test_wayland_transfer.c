#define _POSIX_C_SOURCE 200809L
#include "transfer.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
typedef struct alloc_state { int live, fail, require_zero, not_zero; } alloc_state;
static void *allocate(void *user, size_t n) {
    alloc_state *a = user;
    if (a->fail) return NULL;
    void *p = malloc(n); if (p) ++a->live; return p;
}
static void deallocate(void *user, void *p, size_t n) {
    alloc_state *a = user;
    if (a->require_zero) for (size_t i = 0; i < n; ++i)
        if (((unsigned char *)p)[i]) a->not_zero = 1;
    --a->live; free(p);
}
static int make_pipe(int fd[2]) {
    if (pipe(fd)) return 0;
    int flags = fcntl(fd[0], F_GETFL);
    return flags >= 0 && fcntl(fd[0], F_SETFL, flags | O_NONBLOCK) == 0;
}

int main(void) {
    alloc_state a = {0}; nx_allocator allocator = {&a, allocate, deallocate};
    nx_wayland_read_transfer t; int fd[2];
    CHECK(make_pipe(fd)); int read_fd = fd[0];
    nx_wayland_read_transfer_init(&t, fd[0], allocator, 8, 100);
    nx_wayland_read_transfer_step(&t, 1);
    CHECK(!t.finished && t.waiting && !t.length);
    CHECK(write(fd[1], "abc", 3) == 3);
    nx_wayland_read_transfer_step(&t, 2);
    CHECK(!t.finished && !t.waiting && t.length == 3);
    CHECK(write(fd[1], "de", 2) == 2 && close(fd[1]) == 0);
    nx_wayland_read_transfer_step(&t, 3);
    CHECK(!t.finished && t.length == 5);
    nx_wayland_read_transfer_step(&t, 4);
    CHECK(t.finished && t.result == NX_OK && fcntl(read_fd, F_GETFD) == -1 && errno == EBADF);
    char *bytes = NULL; size_t length = 0, capacity = 0;
    CHECK(nx_wayland_read_transfer_take(&t, &bytes, &length, &capacity) == NX_OK);
    CHECK(length == 5 && !memcmp(bytes, "abcde", 5));
    char *second = NULL; size_t second_length = 0, second_capacity = 0;
    CHECK(nx_wayland_read_transfer_take(&t, &second, &second_length, &second_capacity) == NX_ERROR_STATE);
    nx_wayland_read_transfer_dispose(&t); CHECK(a.live == 1);
    deallocate(&a, bytes, capacity); CHECK(a.live == 0);

    CHECK(make_pipe(fd)); read_fd = fd[0];
    nx_wayland_read_transfer_init(&t, fd[0], allocator, 8, 100);
    CHECK(write(fd[1], "abc", 3) == 3);
    nx_wayland_read_transfer_step(&t, 1); CHECK(t.length == 3);
    nx_wayland_read_transfer_step(&t, 100);
    CHECK(t.finished && t.result == NX_ERROR_PLATFORM && fcntl(read_fd, F_GETFD) == -1);
    nx_wayland_read_transfer_dispose(&t); CHECK(a.live == 0); CHECK(close(fd[1]) == 0);

    CHECK(make_pipe(fd)); read_fd = fd[0];
    nx_wayland_read_transfer_init(&t, fd[0], allocator, 2, 100);
    CHECK(write(fd[1], "abc", 3) == 3);
    nx_wayland_read_transfer_step(&t, 1);
    CHECK(t.finished && t.result == NX_ERROR_LIMIT && fcntl(read_fd, F_GETFD) == -1);
    nx_wayland_read_transfer_dispose(&t); CHECK(a.live == 0); CHECK(close(fd[1]) == 0);

    CHECK(make_pipe(fd)); read_fd = fd[0]; a.fail = 1;
    nx_wayland_read_transfer_init(&t, fd[0], allocator, 8, 100);
    CHECK(write(fd[1], "abc", 3) == 3);
    nx_wayland_read_transfer_step(&t, 1);
    CHECK(t.finished && t.result == NX_ERROR_MEMORY && fcntl(read_fd, F_GETFD) == -1);
    nx_wayland_read_transfer_dispose(&t); CHECK(a.live == 0); CHECK(close(fd[1]) == 0);

    a.fail = 0; CHECK(make_pipe(fd)); read_fd = fd[0];
    nx_wayland_read_transfer_init(&t, fd[0], allocator, 8, 100);
    CHECK(write(fd[1], "abc", 3) == 3);
    nx_wayland_read_transfer_step(&t, 1); CHECK(t.length == 3);
    nx_wayland_read_transfer_dispose(&t);
    CHECK(a.live == 0 && fcntl(read_fd, F_GETFD) == -1); CHECK(close(fd[1]) == 0);

    CHECK(make_pipe(fd)); a.require_zero = 1;
    nx_wayland_read_transfer_init(&t, fd[0], allocator, 16384, 100); t.sensitive = 1;
    char block[4096]; memset(block, 'X', sizeof(block));
    CHECK(write(fd[1], block, sizeof(block)) == (ssize_t)sizeof(block));
    nx_wayland_read_transfer_step(&t, 1); CHECK(t.length == sizeof(block));
    CHECK(write(fd[1], block, sizeof(block)) == (ssize_t)sizeof(block));
    nx_wayland_read_transfer_step(&t, 2); CHECK(t.length == 2 * sizeof(block));
    CHECK(write(fd[1], "y", 1) == 1); nx_wayland_read_transfer_step(&t, 3);
    CHECK(t.length == 2 * sizeof(block) + 1 && !a.not_zero);
    nx_wayland_read_transfer_dispose(&t);
    CHECK(!a.not_zero && a.live == 0); CHECK(close(fd[1]) == 0);
    puts("Wayland bounded incremental transfer verified"); return 0;
}
