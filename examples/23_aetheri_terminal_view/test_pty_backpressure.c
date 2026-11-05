#define _POSIX_C_SOURCE 200809L
#include "pty.h"
#include "pty_queue.h"
#include <nexaui/nexaui.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Own only the fresh shell opened here; stop it before filling its PTY. */
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"shell-backpressure:%d: %s (errno %d)\n",__LINE__,#x,errno); goto cleanup; } } while(0)
static double now_seconds(void) { struct timespec t;return clock_gettime(CLOCK_MONOTONIC,&t)?0:t.tv_sec+t.tv_nsec/1e9; }
typedef struct observations { size_t prompt, first, second;int saw_prompt,saw_first,saw_second,wrong_order; } observations;
static void scan(const char *bytes,size_t length,observations *o) {
    const char *needles[]={"aetheri> ","NEXA_FIRST_RESULT","NEXA_SECOND_RESULT"};
    size_t *positions[]={&o->prompt,&o->first,&o->second};
    int *found[]={&o->saw_prompt,&o->saw_first,&o->saw_second};
    for(size_t i=0;i<length;++i)for(unsigned k=0;k<3;++k) {
        size_t *at=positions[k];const char *needle=needles[k];
        if(bytes[i]==needle[*at])++*at;else *at=bytes[i]==needle[0]?1:0;
        if(!needle[*at]){
            if((k==2 && !o->saw_first) || (k>0 && *found[k]))o->wrong_order=1;
            *found[k]=1;*at=0;
        }
    }
}
static int drain(struct ae_terminal_pty *pty,observations *seen) {
    while(!pty->eof) {
        char bytes[4096];ssize_t n=ae_terminal_pty_read(pty,bytes,sizeof(bytes));
        if(n>0){scan(bytes,(size_t)n,seen);continue;}
        if(n==0 || (n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)))break;
        if(n<0 && errno==EINTR)continue;
        return 0;
    }
    return ae_terminal_pty_observe(pty)>=0;
}
int main(int argc,char **argv) {
    int okay=0,source=-1,stopped=0;nx_window *window=NULL;
    struct ae_terminal_pty pty={.master=-1,.pin=-1,.pid=-1};
    terminal_pty_queue queue={0};observations seen={0};
    CHECK(argc==2 && getuid()!=0 && setenv("TERM","dumb",1)==0);
    source=open(argv[1],O_RDONLY|O_CLOEXEC);CHECK(source>=3);
    CHECK(ae_terminal_pty_open(&pty,source,80,12)==0);close(source);source=-1;
    double until=now_seconds()+10;
    while(!seen.saw_prompt && now_seconds()<until) {
        struct pollfd fd={pty.master,POLLIN|POLLHUP,0};int n=poll(&fd,1,100);
        CHECK(n>=0 || errno==EINTR);CHECK(drain(&pty,&seen));CHECK(!pty.ended);
    }
    CHECK(seen.saw_prompt);
    nx_window_desc desc={0};desc.title_utf8="PTY backpressure";desc.width=240;desc.height=120;desc.flags=NX_WINDOW_HIDDEN;
    CHECK(nx_window_create(&desc,&window)==NX_OK);
    CHECK(kill(pty.pid,SIGSTOP)==0);stopped=1;
    int status=0;pid_t waited=0;until=now_seconds()+3;
    while(!waited && now_seconds()<until) {
        waited=waitpid(pty.pid,&status,WUNTRACED|WNOHANG);
        if(waited<0 && errno==EINTR){waited=0;continue;}
        if(!waited){struct timespec delay={0,10000000};(void)nanosleep(&delay,NULL);}
    }
    if(waited==pty.pid && !WIFSTOPPED(status)) {
        stopped=0;pty.ended=1;pty.status=WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status);
    }
    CHECK(waited==pty.pid && WIFSTOPPED(status));
    char filler[4096];memset(filler,12,sizeof(filler));size_t filled=0;uint32_t flags=0;
    until=now_seconds()+3;
    for(;;) {
        ssize_t n=write(pty.master,filler,sizeof(filler));
        if(n>0){filled+=(size_t)n;CHECK(filled<4*1024*1024);continue;}
        if(n<0 && errno==EINTR)continue;
        CHECK(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK));
        CHECK(nx_window_poll_with_fd_events(window,0,pty.master,NX_WINDOW_FD_WRITABLE,&flags)==NX_OK);
        if(!(flags&NX_WINDOW_FD_WRITABLE))break;
        CHECK(now_seconds()<until);
    }
    const char first[]="printf 'NEXA_%s_RESULT\\n' FIRST\n";
    const char second[]="printf 'NEXA_%s_RESULT\\n' SECOND\n";
    const char end[]="exit 0\n";
    CHECK(filled && terminal_pty_queue_push(&queue,first,sizeof(first)-1));
    CHECK(terminal_pty_queue_push(&queue,second,sizeof(second)-1));
    CHECK(terminal_pty_queue_push(&queue,end,sizeof(end)-1));
    CHECK(kill(pty.pid,SIGCONT)==0);stopped=0;
    until=now_seconds()+20;
    while(now_seconds()<until && !(pty.ended && pty.eof)) {
        uint32_t interests=NX_WINDOW_FD_READABLE|(queue.length?NX_WINDOW_FD_WRITABLE:0);
        CHECK(nx_window_poll_with_fd_events(window,500,pty.master,interests,&flags)==NX_OK);
        CHECK(!(flags&NX_WINDOW_FD_ERROR));
        if(flags&(NX_WINDOW_FD_READABLE|NX_WINDOW_FD_HANGUP))CHECK(drain(&pty,&seen));
        if(flags&NX_WINDOW_FD_WRITABLE)CHECK(terminal_pty_queue_write(&queue,pty.master)>=0);
        nx_event event;int available;do {CHECK(nx_window_next_event(window,&event,&available)==NX_OK);} while(available);
        CHECK(ae_terminal_pty_observe(&pty)>=0);
    }
    CHECK(!queue.length && seen.saw_first && seen.saw_second && !seen.wrong_order);
    CHECK(pty.ended && pty.eof && pty.status==0 && ae_terminal_pty_close(&pty)==0);
    printf("PTY accepted %zu filler bytes; original shell executed both queued commands once and exited cleanly\n",filled);okay=1;
cleanup:
    terminal_pty_queue_clear(&queue);
    if(window)nx_window_destroy(window);
    if(stopped)(void)kill(pty.pid,SIGCONT);
    if(!okay && pty.pid>0 && !pty.ended)(void)ae_terminal_pty_hangup(&pty);
    until=now_seconds()+5;
    while(pty.pid>0 && (!pty.ended || !pty.eof) && now_seconds()<until) {
        struct pollfd fd={pty.master,POLLIN|POLLHUP,0};(void)poll(&fd,1,50);(void)drain(&pty,&seen);
    }
    if(pty.pid>0 && !pty.ended){(void)kill(pty.pid,SIGKILL);(void)waitpid(pty.pid,NULL,0);}
    if(pty.pid>0 && pty.ended && pty.eof)(void)ae_terminal_pty_close(&pty);
    if(source>=0)close(source);
    return okay?0:1;
}
