#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include "wayland/window_internal.h"
#include "wayland/backend.h"
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;} } while(0)
typedef struct arguments { nx_window *windows[2]; int fd; nx_result result; uint32_t flags; } arguments;
static double clock_seconds(clockid_t clock) {struct timespec t;return clock_gettime(clock,&t)==0?t.tv_sec+t.tv_nsec/1e9:-1;}
static void *wrong_thread(void *p) {
    arguments *a=p;a->flags=99;a->result=nx_window_poll_many(a->windows,2,0,a->fd,&a->flags);return NULL;
}
static void *produce(void *p) {
    int fd=*(int *)p;struct timespec delay={0,200000000};
    while(nanosleep(&delay,&delay)<0 && errno==EINTR){}
    char value='x';return write(fd,&value,1)==1?NULL:p;
}
static void sync_done(void *data,struct wl_callback *callback,uint32_t serial) {
    (void)serial;*(int *)data=1;wl_callback_destroy(callback);
}
static const struct wl_callback_listener sync_listener={sync_done};
static int drain(nx_window *w) {
    nx_event event;int available;
    do {if(nx_window_next_event(w,&event,&available)!=NX_OK)return 0;} while(available);
    return 1;
}
int main(int argc,char **argv) {
    int expect_unsupported=argc==2 && !strcmp(argv[1],"--expect-unsupported");
    CHECK(argc==1 || expect_unsupported);
    nx_window_desc d={"many wait",240,120,NX_WINDOW_HIDDEN,{0}};
    nx_window *w[2]={NULL,NULL};nx_result result=nx_window_create(&d,&w[0]);
    if(result==NX_ERROR_PLATFORM || result==NX_ERROR_UNSUPPORTED)return 77;
    CHECK(result==NX_OK && nx_window_create(&d,&w[1])==NX_OK);
    uint32_t flags=99;result=nx_window_poll_many(w,2,0,-1,&flags);
    if(result==NX_ERROR_UNSUPPORTED){nx_window_destroy(w[0]);nx_window_destroy(w[1]);return expect_unsupported?0:77;}
    CHECK(!expect_unsupported);
    CHECK(result==NX_OK && flags==0);
    int fds[2];CHECK(pipe(fds)==0);
    arguments a={{w[0],w[1]},fds[0],NX_OK,99};pthread_t thread;
    CHECK(!pthread_create(&thread,NULL,wrong_thread,&a) && !pthread_join(thread,NULL));
    CHECK(a.result==NX_ERROR_STATE && !a.flags);
    for(unsigned i=0;i<8;++i){CHECK(nx_window_poll_many(w,2,0,-1,&flags)==NX_OK);CHECK(drain(w[0]) && drain(w[1]));}
    const char *only_fault=getenv("NEXAUI_TEST_POLL_MANY_FAULT_ONLY");
    for(int mode=1;mode<=3;++mode) {
        if(only_fault && atoi(only_fault)!=mode)continue;
        fprintf(stderr,"FAULT_MODE %d\n",mode);
        nx_wayland_test_poll_many_fault(mode);
        result=nx_window_poll_many(w,2,0,-1,&flags);
        CHECK(!nx_wayland_test_poll_many_fault_pending());
        CHECK(result==(mode==1?NX_OK:NX_ERROR_PLATFORM));
        CHECK(drain(w[0]) && drain(w[1]));
        int completed=0;struct wl_display *display=nx_window_wayland_display(w[0]);
        CHECK(display);struct wl_callback *sync=wl_display_sync(display);CHECK(sync);
        CHECK(wl_callback_add_listener(sync,&sync_listener,&completed)==0 && wl_display_flush(display)>=0);
        for(unsigned i=0;i<10 && !completed;++i) {
            CHECK(nx_window_poll_many(w,2,500,-1,&flags)==NX_OK);
            CHECK(drain(w[0]) && drain(w[1]));
        }
        CHECK(completed);
        CHECK(nx_window_poll(w[0],0)==NX_OK && nx_window_poll(w[1],0)==NX_OK);
        CHECK(drain(w[0]) && drain(w[1]));
    }
    double wall=clock_seconds(CLOCK_MONOTONIC),cpu=clock_seconds(CLOCK_PROCESS_CPUTIME_ID);
    CHECK(wall>0 && cpu>=0);
    do {CHECK(nx_window_poll_many(w,2,100,-1,&flags)==NX_OK);CHECK(drain(w[0]) && drain(w[1]));}
    while(clock_seconds(CLOCK_MONOTONIC)-wall<.3);
    CHECK(clock_seconds(CLOCK_PROCESS_CPUTIME_ID)-cpu<.15);
    CHECK(!pthread_create(&thread,NULL,produce,&fds[1]));
    wall=clock_seconds(CLOCK_MONOTONIC);
    do {CHECK(nx_window_poll_many(w,2,1500,fds[0],&flags)==NX_OK);CHECK(drain(w[0]) && drain(w[1]));}
    while(!flags && clock_seconds(CLOCK_MONOTONIC)-wall<2);
    CHECK((flags&NX_WINDOW_FD_READABLE) && clock_seconds(CLOCK_MONOTONIC)-wall<1.4);
    void *thread_result=NULL;CHECK(!pthread_join(thread,&thread_result) && !thread_result);
    char value=0;CHECK(read(fds[0],&value,1)==1 && value=='x');
    char last='z';CHECK(write(fds[1],&last,1)==1 && close(fds[1])==0);
    CHECK(nx_window_poll_many(w,2,0,fds[0],&flags)==NX_OK &&
          (flags&(NX_WINDOW_FD_READABLE|NX_WINDOW_FD_HANGUP))==(NX_WINDOW_FD_READABLE|NX_WINDOW_FD_HANGUP));
    CHECK(read(fds[0],&value,1)==1 && value=='z');
    int invalid=dup(fds[0]);CHECK(invalid>=0 && close(invalid)==0);
    CHECK(nx_window_poll_many(w,2,0,invalid,&flags)==NX_OK && (flags&NX_WINDOW_FD_ERROR));
    CHECK(close(fds[0])==0);
    struct wl_display *display=nx_window_wayland_display(w[1]);CHECK(display);
    CHECK(shutdown(wl_display_get_fd(display),SHUT_RDWR)==0);
    result=NX_OK;
    for(unsigned i=0;i<30 && result==NX_OK;++i)result=nx_window_poll_many(w,2,50,-1,&flags);
    CHECK(result==NX_ERROR_PLATFORM);
    CHECK(nx_window_poll_many(&w[0],1,0,-1,&flags)==NX_OK);
    nx_window_destroy(w[1]);
    CHECK(nx_window_poll_many(&w[0],1,0,-1,&flags)==NX_OK);
    nx_window_destroy(w[0]);
    nx_window *limit[NX_WINDOW_POLL_MAX_WINDOWS]={0};
    for(size_t i=0;i<NX_WINDOW_POLL_MAX_WINDOWS;++i)CHECK(nx_window_create(&d,&limit[i])==NX_OK);
    CHECK(nx_window_poll_many(limit,NX_WINDOW_POLL_MAX_WINDOWS,0,-1,&flags)==NX_OK);
    for(size_t i=0;i<NX_WINDOW_POLL_MAX_WINDOWS;++i)nx_window_destroy(limit[i]);
    puts("group idle, owner, worker wake, failed-peer isolation and capacity passed");return 0;
}
