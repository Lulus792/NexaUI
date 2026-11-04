#ifndef NEXAUI_WAYLAND_TRANSFER_H
#define NEXAUI_WAYLAND_TRANSFER_H

#include "nexaui/nexaui.h"

/* Owner-thread only. The descriptor must already be nonblocking; init takes
   ownership. Each step performs at most one read and never waits for data.
   A successful take transfers the allocated buffer and its allocation size. */
typedef struct nx_wayland_read_transfer {
    nx_allocator allocator;
    char *bytes;
    size_t length, capacity, limit;
    uint64_t deadline;
    int fd, finished, waiting, sensitive;
    nx_result result;
} nx_wayland_read_transfer;

void nx_wayland_read_transfer_init(nx_wayland_read_transfer *transfer, int fd,
                                    nx_allocator allocator, size_t limit,
                                    uint64_t deadline);
void nx_wayland_read_transfer_step(nx_wayland_read_transfer *transfer, uint64_t now);
nx_result nx_wayland_read_transfer_take(nx_wayland_read_transfer *transfer,
                                         char **bytes, size_t *length, size_t *capacity);
void nx_wayland_read_transfer_dispose(nx_wayland_read_transfer *transfer);

#endif
