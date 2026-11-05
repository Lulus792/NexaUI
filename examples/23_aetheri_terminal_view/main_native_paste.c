#ifdef NEXAUI_TEST_PASTE_PTY
#define _POSIX_C_SOURCE 200809L
#endif
#include "paste_review.h"
#include <nexaui/software.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NEXAUI_TEST_PASTE_PTY
#include "model.h"
#include "pty.h"
#include "pty_queue.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Bounded test-only helpers. A real app must integrate PTY readiness and
   partial writes into its event loop and own its shutdown policy. */
static double now_seconds(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;return t.tv_sec+t.tv_nsec/1e9;}
static int pty_send(int fd,const char *bytes,size_t length) {
    size_t at=0;double until=now_seconds()+5;
    while(at<length && now_seconds()<until) {
        ssize_t n=write(fd,bytes+at,length-at);
        if(n>0){at+=(size_t)n;continue;}
        if(n<0 && errno==EINTR)continue;
        if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) {
            struct pollfd p={fd,POLLOUT,0};if(poll(&p,1,100)<0 && errno!=EINTR)return 0;continue;
        }
        return 0;
    }
    return at==length;
}
static int model_has(const struct ae_terminal *model,const char *marker) {
    size_t length=strlen(marker);
    for(unsigned y=0;y<model->rows;++y) {
        const struct ae_term_cell *row=ae_terminal_row(model,y);if(!row)continue;
        for(unsigned x=0;x+length<=model->cols;++x){size_t i=0;
            while(i<length && row[x+i].code==(unsigned char)marker[i])++i;
            if(i==length)return 1;}
    }
    return 0;
}
static int pty_drain(struct ae_terminal_pty *pty,struct ae_terminal *model) {
    while(!pty->eof) {
        char bytes[4096];ssize_t n=ae_terminal_pty_read(pty,bytes,sizeof(bytes));
        if(n>0){if(model)ae_terminal_feed(model,bytes,(size_t)n);continue;}
        if(n==0 || (n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)))break;
        if(n<0 && errno==EINTR)continue;
        return 0;
    }
    return ae_terminal_pty_observe(pty)>=0;
}
static int pty_wait(struct ae_terminal_pty *pty,struct ae_terminal *model,const char *marker,int end) {
    double until=now_seconds()+10;
    while(now_seconds()<until) {
        struct pollfd p={pty->master,POLLIN|POLLHUP,0};int ready=poll(&p,1,100);
        if(ready<0 && errno!=EINTR)return 0;
        if(!pty_drain(pty,model))return 0;
        if(marker && model && model_has(model,marker))return 1;
        if(end && pty->eof && pty->ended)return 1;
        if(!end && pty->ended)return 0;
    }
    return 0;
}
#endif

/* Private-test consumer only. The optional PTY variant sends confirmed bytes
   to its own disposable test shell, never to another application's session. */
static int width(uint32_t c){return c==0x754c?2:c==0x301?0:1;}
static unsigned char *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);if(n<=0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}fclose(f);*size=(size_t)n;return p;
}
static uint64_t checksum(const char *p,size_t length) {
    uint64_t value=UINT64_C(14695981039346656037);
    for(size_t i=0;i<length;++i)value=(value^(unsigned char)p[i])*UINT64_C(1099511628211);
    return value;
}
int main(int argc,char **argv) {
    int okay=0,shown=0,review_shown=0,dirty=1,done=0,delivering=0;size_t size=0;
    unsigned char *bytes=NULL;nx_font *font=NULL;nx_font_collection *fonts=NULL;nx_context *base=NULL;
    nx_window *window=NULL;nx_software_view *renderer=NULL;terminal_paste_review *review=NULL;
    terminal_paste_flow flow={0};terminal_paste_guard guard={1,1,1,0};
#ifdef NEXAUI_TEST_PASTE_PTY
    struct ae_terminal_pty pty={.master=-1,.pin=-1,.pid=-1};
    struct ae_terminal *model=NULL;int source=-1,delivered=0,pty_stopped=0,pty_blocked=0;
    terminal_pty_queue pending={0};double delivery_deadline=0;
    if(argc!=3 || getuid()==0 || setenv("TERM","dumb",1)!=0 ||
       (source=open(argv[2],O_RDONLY|O_CLOEXEC))<3 || ae_terminal_pty_open(&pty,source,80,12)!=0)goto cleanup;
    close(source);source=-1;
    model=malloc(sizeof(*model));if(!model)goto cleanup;ae_terminal_init(model,80,12,width);
#endif
    if(argc!=
#ifdef NEXAUI_TEST_PASTE_PTY
       3
#else
       2
#endif
       || !(bytes=read_file(argv[1],&size)) || nx_font_open(bytes,size,NULL,&font)!=NX_OK ||
       nx_font_collection_create(NULL,&fonts)!=NX_OK || nx_font_collection_add(fonts,font)!=NX_OK ||
       nx_context_create(NULL,&base)!=NX_OK)goto cleanup;
    nx_window_desc desc={"NexaUI terminal paste test",640,360,0,{0}};
    if(nx_window_create(&desc,&window)!=NX_OK || nx_software_view_open(window,&renderer)!=NX_OK ||
       terminal_paste_init(&flow,terminal_paste_wayland_transport(window))!=NX_OK)goto cleanup;
    okay=1;
    while(okay && !done) {
#ifdef NEXAUI_TEST_PASTE_PTY
        uint32_t fd_events=0,interests=NX_WINDOW_FD_READABLE;
        if(pending.length)interests|=NX_WINDOW_FD_WRITABLE;
        if(nx_window_poll_with_fd_events(window,50,pty.master,interests,&fd_events)!=NX_OK ||
           (fd_events&NX_WINDOW_FD_ERROR)){okay=0;break;}
        if((fd_events&(NX_WINDOW_FD_READABLE|NX_WINDOW_FD_HANGUP)) && !pty_drain(&pty,model)){okay=0;break;}
        if((fd_events&NX_WINDOW_FD_WRITABLE) && terminal_pty_queue_write(&pending,pty.master)<0){okay=0;break;}
        if(shown && !pty_blocked && getenv("NEXAUI_TEST_PASTE_BACKPRESSURE") && model_has(model,"aetheri>")) {
            if(kill(pty.pid,SIGSTOP)){okay=0;break;}pty_stopped=1;
            double until=now_seconds()+3;int status=0;pid_t result=0;
            while(!result && now_seconds()<until) {
                result=waitpid(pty.pid,&status,WUNTRACED|WNOHANG);
                if(result<0 && errno==EINTR){result=0;continue;}
                if(!result){struct timespec delay={0,10000000};(void)nanosleep(&delay,NULL);}
            }
            if(result==pty.pid && !WIFSTOPPED(status)){pty_stopped=0;pty.ended=1;
                pty.status=WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status);}
            if(result!=pty.pid || !WIFSTOPPED(status)){okay=0;break;}
            char filler[4096];memset(filler,12,sizeof(filler));size_t total=0;until=now_seconds()+3;
            for(;;) {
                ssize_t n=write(pty.master,filler,sizeof(filler));
                if(n>0){total+=(size_t)n;if(total>4*1024*1024){okay=0;break;}continue;}
                if(n<0 && errno==EINTR)continue;
                if(n>=0 || (errno!=EAGAIN && errno!=EWOULDBLOCK)){okay=0;break;}
                uint32_t writable=0;
                if(nx_window_poll_with_fd_events(window,0,pty.master,NX_WINDOW_FD_WRITABLE,&writable)!=NX_OK){okay=0;break;}
                if(!(writable&NX_WINDOW_FD_WRITABLE))break;
                if(now_seconds()>=until){okay=0;break;}
            }
            if(!okay || !total){okay=0;break;}
            pty_blocked=1;puts("NATIVE_PASTE_PTY_BLOCKED");fflush(stdout);
        }
        if(delivering && !pending.length && model_has(model,"CLIP_PTY_42")) {
            delivered=1;puts("NATIVE_PASTE_PTY_OUTPUT");fflush(stdout);done=1;break;
        }
        if(delivering && (pty.ended || pty.eof || now_seconds()>=delivery_deadline)){okay=0;break;}
#else
        if(nx_window_poll(window,50)!=NX_OK){okay=0;break;}
#endif
        nx_window_info info;if(nx_window_get_info(window,&info)!=NX_OK){okay=0;break;}
        if(info.should_close)break;
        nx_event event;int available=0;
        do {
            if(nx_window_next_event(window,&event,&available)!=NX_OK){okay=0;break;}
            if(!available)break;
            if(event.type==NX_EVENT_FOCUS) {
                if(guard.focused!=!!event.focused){++guard.focus_epoch;guard.focused=!!event.focused;}
                if(!guard.focused && flow.state && !delivering){terminal_paste_reset(&flow);done=1;puts("NATIVE_PASTE_FOCUS_CANCEL");fflush(stdout);}
            }
            if(event.type==NX_EVENT_RESIZE)dirty=1;
            if(!review && flow.state==1 && event.type==NX_EVENT_KEY_DOWN && event.key==NX_KEY_ESCAPE && !event.repeat) {
                terminal_paste_reset(&flow);puts("NATIVE_PASTE_RECEIVE_CANCELLED");fflush(stdout);done=1;break;
            }
            if(!review && event.type==NX_EVENT_KEY_DOWN && event.key==NX_KEY_V && !event.repeat && guard.focused &&
               (event.modifiers&(NX_MOD_CONTROL|NX_MOD_SHIFT|NX_MOD_ALT|NX_MOD_SUPER))==(NX_MOD_CONTROL|NX_MOD_SHIFT) && !flow.state) {
                if(terminal_paste_begin(&flow,guard,5000)!=NX_OK){okay=0;break;}
                puts("NATIVE_PASTE_PENDING");fflush(stdout);
            } else if(review && !delivering) {
                if(nx_context_queue_event(terminal_paste_review_context(review),&event)!=NX_OK){okay=0;break;}
                dirty=1;
            }
        } while(available);
        if(!okay || done)break;
        if(flow.state==1) {
            int ready=0;enum ae_term_paste_result validation;
            nx_result r=terminal_paste_poll(&flow,guard,&ready,&validation);
            if(r!=NX_OK){
                if(ready && validation==AE_TERM_PASTE_CONTROL){puts("NATIVE_PASTE_REJECTED_CONTROL");fflush(stdout);done=1;continue;}
                okay=0;break;
            }
            if(ready) {
                if(validation!=AE_TERM_PASTE_OK || !flow.paste.review ||
                   terminal_paste_review_create(&flow,fonts,width,&review)!=NX_OK){okay=0;break;}
                dirty=1;
            }
        }
        if(delivering || !dirty)continue;
        nx_context *context=review?terminal_paste_review_context(review):base;
        enum terminal_review_result result=TERMINAL_REVIEW_OPEN;nx_utf8_span confirmed={0};
        if(review) {
            if(terminal_paste_review_configure(review,info.width,info.height,16,0)!=NX_OK ||
               terminal_paste_review_frame(review,guard,&result,&confirmed)!=NX_OK){okay=0;break;}
            if(result==TERMINAL_REVIEW_CANCELLED){puts("NATIVE_PASTE_CANCELLED");fflush(stdout);done=1;continue;}
            if(result==TERMINAL_REVIEW_CONFIRMED){printf("NATIVE_PASTE_ACCEPTED %zu %016llx\n",confirmed.length,
                (unsigned long long)checksum(confirmed.data,confirmed.length));fflush(stdout);
#ifdef NEXAUI_TEST_PASTE_PTY
                if(!terminal_pty_queue_push(&pending,confirmed.data,confirmed.length)){okay=0;break;}
                if(pty_stopped){if(kill(pty.pid,SIGCONT)){okay=0;break;}pty_stopped=0;}
                delivering=1;delivery_deadline=now_seconds()+10;dirty=0;continue;
#endif
                done=1;continue;}
        } else if(nx_begin_frame(base,info.width,info.height,0)!=NX_OK ||
                  nx_fill_rect(base,(nx_rect){0,0,info.width,info.height},(nx_color){.08f,.1f,.14f,1})!=NX_OK ||
                  nx_end_frame(base)!=NX_OK){okay=0;break;}
        const nx_command *commands=NULL;size_t count=0;nx_software_view_outcome outcome;
        if(nx_frame_commands(context,&commands,&count)!=NX_OK ||
           nx_software_view_render(renderer,commands,count,(nx_color){0,0,0,1},&outcome)!=NX_OK){okay=0;break;}
        nx_result ax=nx_window_publish_semantics(window,context);
        if(ax!=NX_OK && ax!=NX_ERROR_UNSUPPORTED){okay=0;break;}
        dirty=outcome!=NX_SOFTWARE_VIEW_READY;
        if(outcome==NX_SOFTWARE_VIEW_READY && !shown){puts("NATIVE_PASTE_READY");fflush(stdout);shown=1;}
        if(outcome==NX_SOFTWARE_VIEW_READY && review && !review_shown){puts("NATIVE_PASTE_REVIEW_READY");fflush(stdout);review_shown=1;}
    }
cleanup:
    if(review)terminal_paste_review_destroy(review);
    terminal_paste_reset(&flow);
    if(renderer && nx_software_view_close(&renderer)!=NX_OK)okay=0;
    if(window)nx_window_destroy(window);
    if(base)nx_context_destroy(base);
    if(fonts)nx_font_collection_destroy(fonts);
    if(font)nx_font_close(font);
    free(bytes);
#ifdef NEXAUI_TEST_PASTE_PTY
    terminal_pty_queue_clear(&pending);
    if(pty_stopped)(void)kill(pty.pid,SIGCONT);
    if(done && okay && !delivered && model) {
        const char *probe="printf '%s%s\\n' 'SAFE_' 'PROBE'\n";
        if(!pty_send(pty.master,probe,strlen(probe)) || !pty_wait(&pty,model,"SAFE_PROBE",0) ||
           model_has(model,"CLIP_PTY_42"))okay=0;
        else {puts("NATIVE_PASTE_PTY_UNCHANGED");fflush(stdout);}
    }
    if(pty.pid>0 && !pty.ended) {
        if(!pty_send(pty.master,"exit 0\n",7) || !pty_wait(&pty,model,NULL,1)) {
            (void)ae_terminal_pty_hangup(&pty);
            if(!pty_wait(&pty,model,NULL,1) && !pty.ended) {
                (void)kill(pty.pid,SIGKILL);(void)pty_wait(&pty,model,NULL,1);
            }
            okay=0;
        }
    }
    if(pty.pid>0 && pty.ended && pty.eof) {
        if(pty.status!=0 || ae_terminal_pty_close(&pty)!=0)okay=0;
    } else if(pty.pid>0)okay=0;
    if(source>=0)close(source);
    free(model);
#endif
    if(!okay)fputs("Native paste test consumer failed\n",stderr);
    return okay && done?0:1;
}
