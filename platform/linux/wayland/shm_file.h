#ifndef NX_WAYLAND_SHM_FILE_H
#define NX_WAYLAND_SHM_FILE_H
/* Linux-only private helper. Include after defining _GNU_SOURCE. Reserve all
   backing storage before mapping: ftruncate alone can leave quota/space faults
   to become SIGBUS while the renderer writes pixels. */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static int nx_wayland_shm_file(size_t bytes) {
    if (!bytes || bytes > INT_MAX) { errno=EINVAL; return -1; }
    int fd=memfd_create("nexaui-wayland",MFD_CLOEXEC);
    if (fd<0 && errno==ENOSYS) {
        /* Retain the old-kernel path, with close-on-exec and eager allocation. */
        char path[]="/tmp/nexaui-wayland-XXXXXX";
        fd=mkostemp(path,O_CLOEXEC);
        if (fd<0) return -1;
        if (unlink(path)) {
            int error=errno; close(fd); errno=error; return -1;
        }
    }
    if (fd<0) return -1;
    int error;
    do { error=posix_fallocate(fd,0,(off_t)bytes); } while (error==EINTR);
    if (error) { close(fd); errno=error; return -1; }
    return fd;
}
#endif
