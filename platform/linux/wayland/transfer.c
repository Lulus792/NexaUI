#include "transfer.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

void nx_wayland_read_transfer_init(nx_wayland_read_transfer *t, int fd,
                                    nx_allocator allocator, size_t limit,
                                    uint64_t deadline) {
    *t = (nx_wayland_read_transfer){0};
    t->allocator = allocator; t->fd = fd; t->limit = limit;
    t->deadline = deadline; t->result = NX_ERROR_PLATFORM;
}

static void finish(nx_wayland_read_transfer *t, nx_result result) {
    if (t->fd >= 0) close(t->fd);
    t->fd = -1; t->finished = 1; t->waiting = 0; t->result = result;
}

void nx_wayland_read_transfer_step(nx_wayland_read_transfer *t, uint64_t now) {
    if (t->finished) return;
    t->waiting = 0;
    if (!now || now >= t->deadline) { finish(t, NX_ERROR_PLATFORM); return; }
    char chunk[8192];
    ssize_t count = read(t->fd, chunk, sizeof(chunk));
    if (count == 0) { finish(t, NX_OK); return; }
    if (count < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            t->waiting = 1; return;
        }
        finish(t, NX_ERROR_PLATFORM); return;
    }
    if ((size_t)count > t->limit - t->length) { finish(t, NX_ERROR_LIMIT); return; }
    size_t needed = t->length + (size_t)count;
    if (needed > t->capacity) {
        size_t grown = t->capacity ? t->capacity : 8192u;
        if (t->capacity) grown = t->capacity > t->limit / 2u ? t->limit : t->capacity * 2u;
        if (grown < needed) grown = needed;
        if (grown > t->limit) grown = t->limit;
        char *replacement = t->allocator.allocate(t->allocator.user, grown);
        if (!replacement) { finish(t, NX_ERROR_MEMORY); return; }
        if (t->length) memcpy(replacement, t->bytes, t->length);
        if (t->bytes) t->allocator.deallocate(t->allocator.user, t->bytes, t->capacity);
        t->bytes = replacement; t->capacity = grown;
    }
    memcpy(t->bytes + t->length, chunk, (size_t)count);
    t->length = needed;
}

nx_result nx_wayland_read_transfer_take(nx_wayland_read_transfer *t,
                                         char **bytes, size_t *length, size_t *capacity) {
    if (!t->finished || t->result != NX_OK) return NX_ERROR_STATE;
    *bytes = t->bytes; *length = t->length; *capacity = t->capacity;
    t->bytes = NULL; t->length = t->capacity = 0;
    t->result = NX_ERROR_STATE;
    return NX_OK;
}

void nx_wayland_read_transfer_dispose(nx_wayland_read_transfer *t) {
    if (t->fd >= 0) close(t->fd);
    if (t->bytes) t->allocator.deallocate(t->allocator.user, t->bytes, t->capacity);
    t->fd = -1; t->bytes = NULL; t->length = t->capacity = 0;
    t->finished = 1;
}
