#ifndef NEXAUI_EXAMPLE_MUSL_SYSCALLS_H
#define NEXAUI_EXAMPLE_MUSL_SYSCALLS_H
/* Optional test-only Linux/musl bridge for original Aetheri sources. */
int close_range(unsigned int first,unsigned int last,int flags);
int execveat(int dirfd,const char *path,char *const argv[],char *const envp[],int flags);
#endif
