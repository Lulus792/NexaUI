#ifndef NEXAUI_EXAMPLE_PTY_QUEUE_H
#define NEXAUI_EXAMPLE_PTY_QUEUE_H

/* Example-owned bounded byte queue. This is not a session or paste policy. */
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

enum { TERMINAL_PTY_QUEUE_CAPACITY = 4096 };
typedef struct terminal_pty_queue {
    char bytes[TERMINAL_PTY_QUEUE_CAPACITY];
    size_t offset, length;
} terminal_pty_queue;

static void terminal_pty_queue_clear(terminal_pty_queue *q) {
    memset(q->bytes, 0, sizeof(q->bytes));
    q->offset = q->length = 0;
}

/* Reject a whole item when it does not fit; never enqueue a prefix. */
static int terminal_pty_queue_push(terminal_pty_queue *q, const char *bytes, size_t length) {
    if (!bytes || !length || length > sizeof(q->bytes) - q->length) return 0;
    if (q->offset && q->offset + q->length + length > sizeof(q->bytes)) {
        memmove(q->bytes, q->bytes + q->offset, q->length);
        memset(q->bytes + q->length, 0, sizeof(q->bytes) - q->length);
        q->offset = 0;
    }
    memcpy(q->bytes + q->offset + q->length, bytes, length);
    q->length += length;
    return 1;
}

/* One nonblocking attempt: -1 failure, 0 no progress, 1 progress. */
static int terminal_pty_queue_write(terminal_pty_queue *q, int fd) {
    if (!q->length) return 0;
    ssize_t n = write(fd, q->bytes + q->offset, q->length);
    if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
    if (n <= 0) return -1;
    memset(q->bytes + q->offset, 0, (size_t)n);
    q->offset += (size_t)n;
    q->length -= (size_t)n;
    if (!q->length) q->offset = 0;
    return 1;
}
#endif
