#define _GNU_SOURCE
#include "musl_syscalls.h"
#include <sys/syscall.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
int close_range(unsigned int first,unsigned int last,int flags) {
    return (int)syscall(SYS_close_range,first,last,flags);
}
int execveat(int dirfd,const char *path,char *const argv[],char *const envp[],int flags) {
    return (int)syscall(SYS_execveat,dirfd,path,argv,envp,flags);
}
#ifdef NEXAUI_NORMALIZE_TERMIOS
/* Alpine musl may leave non-kernel c_cc slots untouched. Aetheri's original
   comparison reads all slots; normalize the caller's output before tcgetattr.
   Test-only compatibility, not a claim that unmodified musl behavior passes. */
int __real_tcgetattr(int fd,struct termios *out);
int __wrap_tcgetattr(int fd,struct termios *out) {
    memset(out,0,sizeof(*out));return __real_tcgetattr(fd,out);
}
#endif
