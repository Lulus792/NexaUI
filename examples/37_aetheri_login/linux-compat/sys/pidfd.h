#ifndef AETHERI_NEXAUI_EXAMPLE_PIDFD_UAPI_H
#define AETHERI_NEXAUI_EXAMPLE_PIDFD_UAPI_H
/* Linux C-library compatibility for the original connection. Reuse installed
   pidfd UAPI structures. Its linux/fcntl.h include would redeclare musl's
   fcntl structures; libc supplies the O_* constants that pidfd.h needs. */
#include <fcntl.h>
#include <stdint.h>
#ifndef _LINUX_FCNTL_H
#define _LINUX_FCNTL_H
#define AETHERI_EXAMPLE_RESTORE_LINUX_FCNTL_GUARD
#endif
#include <linux/pidfd.h>
#ifdef AETHERI_EXAMPLE_RESTORE_LINUX_FCNTL_GUARD
#undef _LINUX_FCNTL_H
#undef AETHERI_EXAMPLE_RESTORE_LINUX_FCNTL_GUARD
#endif

/* musl's ioctl request parameter is int. Preserve the kernel request bits. */
_Static_assert(sizeof(int) == sizeof(uint32_t), "Linux ioctl request width");
_Static_assert((uint32_t)(int)PIDFD_GET_INFO == (uint32_t)PIDFD_GET_INFO,
               "pidfd ioctl request bits");
enum { AETHERI_EXAMPLE_PIDFD_GET_INFO = (int)PIDFD_GET_INFO };
#undef PIDFD_GET_INFO
#define PIDFD_GET_INFO AETHERI_EXAMPLE_PIDFD_GET_INFO

/* Linux socket ABI values, absent from older libc socket headers.
   SO_PEERPIDFD: Linux v6.18 include/uapi/asm-generic/socket.h.
   SCM_PIDFD: Linux v6.18 include/linux/socket.h. */
#ifndef SO_PEERPIDFD
#define SO_PEERPIDFD 77
#endif
#ifndef SCM_PIDFD
#define SCM_PIDFD 0x04
#endif
#endif
