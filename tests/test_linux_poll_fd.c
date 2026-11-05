#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

typedef struct worker_args { int fd; } worker_args;
typedef struct wrong_thread_args { nx_window *window; int fd; nx_result result; uint32_t flags; } wrong_thread_args;
static double now(void) { struct timespec t; return clock_gettime(CLOCK_MONOTONIC,&t)==0 ? t.tv_sec+t.tv_nsec/1e9 : -1; }
static void *produce(void *p) {
    worker_args *a=p; struct timespec delay={0,200000000};
    while(nanosleep(&delay,&delay)<0 && errno==EINTR) {}
    const uint64_t value=UINT64_C(0x123456789abcdef0);
    ssize_t n; do { n=write(a->fd,&value,sizeof(value)); } while(n<0 && errno==EINTR);
    return n==(ssize_t)sizeof(value) ? NULL : a;
}
static void *wrong_thread(void *p) {
    wrong_thread_args *a=p; a->flags=99;
    a->result=nx_window_poll_with_fd(a->window,0,a->fd,&a->flags); return NULL;
}
int main(void) {
    nx_window_desc desc={"FD wake test",240,120,NX_WINDOW_HIDDEN,{0}};
    nx_window *window=NULL; nx_result result=nx_window_create(&desc,&window);
    if(result==NX_ERROR_PLATFORM || result==NX_ERROR_UNSUPPORTED) return 77;
    CHECK(result==NX_OK && window);
    int fds[2]; CHECK(pipe(fds)==0);
    uint32_t flags=99;
    CHECK(nx_window_poll_with_fd(NULL,0,fds[0],&flags)==NX_ERROR_ARGUMENT && flags==0);
    CHECK(nx_window_poll_with_fd(window,0,-1,&flags)==NX_ERROR_ARGUMENT && flags==0);
    CHECK(nx_window_poll_with_fd(window,0,fds[0],NULL)==NX_ERROR_ARGUMENT);
    wrong_thread_args wrong={window,fds[0],NX_OK,99}; pthread_t thread;
    CHECK(pthread_create(&thread,NULL,wrong_thread,&wrong)==0 && pthread_join(thread,NULL)==0);
    CHECK(wrong.result==NX_ERROR_STATE && wrong.flags==0);
    for(unsigned i=0;i<4;++i) CHECK(nx_window_poll(window,0)==NX_OK);
    CHECK(nx_window_poll_with_fd(window,0,fds[0],&flags)==NX_OK && flags==0);
    double empty_start=now(); CHECK(empty_start>0);
    do { CHECK(nx_window_poll_with_fd(window,40,fds[0],&flags)==NX_OK && flags==0); }
    while(now()-empty_start<0.08);
    worker_args args={fds[1]}; CHECK(pthread_create(&thread,NULL,produce,&args)==0);
    double start=now(); CHECK(start>0);
    do { CHECK(nx_window_poll_with_fd(window,1500,fds[0],&flags)==NX_OK); }
    while(!flags && now()-start<2.0);
    double elapsed=now()-start;
    CHECK((flags&NX_WINDOW_FD_READABLE) && elapsed>=0.05 && elapsed<2.0);
    void *worker_result=NULL; CHECK(pthread_join(thread,&worker_result)==0 && !worker_result);
    uint64_t value=0; CHECK(read(fds[0],&value,sizeof(value))==(ssize_t)sizeof(value));
    CHECK(value==UINT64_C(0x123456789abcdef0));
    const unsigned char last=42; CHECK(write(fds[1],&last,1)==1);
    CHECK(close(fds[1])==0);
    flags=0; CHECK(nx_window_poll_with_fd(window,0,fds[0],&flags)==NX_OK &&
                   (flags&(NX_WINDOW_FD_READABLE|NX_WINDOW_FD_HANGUP))==(NX_WINDOW_FD_READABLE|NX_WINDOW_FD_HANGUP));
    unsigned char got=0; CHECK(read(fds[0],&got,1)==1 && got==last);
    flags=0; CHECK(nx_window_poll_with_fd(window,0,fds[0],&flags)==NX_OK && (flags&NX_WINDOW_FD_HANGUP));
    int bad=dup(fds[0]); CHECK(bad>=0 && close(bad)==0);
    flags=0; CHECK(nx_window_poll_with_fd(window,0,bad,&flags)==NX_OK && (flags&NX_WINDOW_FD_ERROR));
    CHECK(close(fds[0])==0); nx_window_destroy(window);
    return 0;
}
