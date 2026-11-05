#define _POSIX_C_SOURCE 200809L
/* Private virtual-input test consumer; owns its shell and PTY. */
#include "ime_overlay.h"
#include "pty.h"
#include "pty_queue.h"
#include <nexaui/software.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stop;
static void stop_signal(int signal_number){(void)signal_number;stop=1;}
static double now_seconds(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;return t.tv_sec+t.tv_nsec/1e9;}
static int width(uint32_t c){(void)c;return 1;}
static nx_result mark(void *u,uint32_t b,uint32_t m,unsigned i,float *x,float *y) {
    (void)u;(void)b;(void)m;(void)i;*x=0;*y=0;return NX_OK;
}
static unsigned char *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);if(n<=0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}fclose(f);*size=(size_t)n;return p;
}
/* Bounded, test-only write; production owner must use an asynchronous queue. */
static int send_bytes(int fd,const char *bytes,size_t length) {
    size_t at=0;double until=now_seconds()+5;
    while(at<length && now_seconds()<until){ssize_t n=write(fd,bytes+at,length-at);
        if(n>0){at+=(size_t)n;continue;}if(n<0 && errno==EINTR)continue;
        if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)){struct pollfd p={fd,POLLOUT,0};
            if(poll(&p,1,100)<0 && errno!=EINTR)return 0;
            continue;
        }
        return 0;
    }
    return at==length;
}
static int drain(struct ae_terminal_pty *pty,struct ae_terminal *model,int *changed) {
    *changed=0;
    while(!pty->eof){char bytes[4096];ssize_t n=ae_terminal_pty_read(pty,bytes,sizeof(bytes));
        if(n>0){if(getenv("NEXAUI_IME_PTY_TRACE"))fwrite(bytes,1,(size_t)n,stderr);
            if(model)ae_terminal_feed(model,bytes,(size_t)n);
            *changed=1;continue;}
        if(n==0 || (n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)))break;
        if(n<0 && errno==EINTR)continue;
        return 0;
    }
    return ae_terminal_pty_observe(pty)>=0;
}
static unsigned count_scalar(const struct ae_terminal *model,uint32_t scalar) {
    unsigned count=0;
    for(unsigned y=0;y<model->rows;++y){const struct ae_term_cell *row=ae_terminal_row(model,y);if(!row)continue;
        for(unsigned x=0;x<model->cols;++x)if(row[x].width && row[x].code==scalar)++count;}
    return count;
}
static int finish(struct ae_terminal_pty *pty,struct ae_terminal *model) {
    double until=now_seconds()+10;
    while(now_seconds()<until && !(pty->ended && pty->eof)) {
        struct pollfd p={pty->master,POLLIN|POLLHUP,0};int ready=poll(&p,1,100),changed=0;
        if(ready<0 && errno!=EINTR)return 0;
        if(!drain(pty,model,&changed))return 0;
    }
    return pty->ended && pty->eof;
}
int main(int argc,char **argv) {
    int okay=0,source=-1,shown=0,preedit_seen=0,commit_seen=0,dirty=1;
    size_t font_size=0;unsigned char *font_bytes=NULL;nx_font *font=NULL;
    nx_window *window=NULL;nx_software_view *renderer=NULL;aetheri_terminal_view *view=NULL;
    struct ae_terminal *model=NULL;struct ae_terminal_pty pty={.master=-1,.pin=-1,.pid=-1};
    terminal_ime_adapter ime={0};terminal_ime_guard guard={123,1,0};
    terminal_ime_overlay overlay={.ime=&ime,.current=guard};uint64_t revision=1;
    nx_rect last_caret={0};int caret_set=0;
    terminal_pty_queue pending={0};
    int cancel_test=getenv("NEXAUI_TEST_FOCUS_CANCEL")!=NULL,cancel_reported=0;
    uint32_t expected=cancel_test?'z':0xe1;
    if(argc!=3 || getuid()==0 || setenv("TERM","dumb",1)!=0)goto cleanup;
    struct sigaction action={0};action.sa_handler=stop_signal;sigemptyset(&action.sa_mask);
    if(sigaction(SIGTERM,&action,NULL) || sigaction(SIGINT,&action,NULL))goto cleanup;
    source=open(argv[2],O_RDONLY|O_CLOEXEC);
    if(source<3 || ae_terminal_pty_open(&pty,source,80,12)!=0)goto cleanup;
    close(source);source=-1; /* Start original shell before window/UI initialization. */
    if(!(font_bytes=read_file(argv[1],&font_size)) || nx_font_open(font_bytes,font_size,NULL,&font)!=NX_OK)goto cleanup;
    model=malloc(sizeof(*model));if(!model)goto cleanup;ae_terminal_init(model,80,12,width);
    aetheri_terminal_view_desc d={.model=model,.regular=font,.bold=font,.text_identity=123,
        .cell_width=8,.cell_height=16,.baseline=12,.pixels_per_em=12,.mark_position=mark};
    nx_window_desc wd={"NexaUI terminal IME test",640,192,0,{0}};
    if(aetheri_terminal_view_create(&d,&view)!=NX_OK || nx_window_create(&wd,&window)!=NX_OK ||
       nx_software_view_open(window,&renderer)!=NX_OK)goto cleanup;
    okay=1;double deadline=now_seconds()+25;
    while(okay && !stop && now_seconds()<deadline) {
        uint32_t events=0;
        uint32_t interests=NX_WINDOW_FD_READABLE;
        if(pending.length)interests|=NX_WINDOW_FD_WRITABLE;
        if(nx_window_poll_with_fd_events(window,1000,pty.master,interests,&events)!=NX_OK){okay=0;break;}
        if(events&NX_WINDOW_FD_ERROR){okay=0;break;}
        if(events&(NX_WINDOW_FD_READABLE|NX_WINDOW_FD_HANGUP)) {
            int changed=0;if(!drain(&pty,model,&changed)){okay=0;break;}
            if(changed){++revision;dirty=1;}
        }
        if((events&NX_WINDOW_FD_WRITABLE) && terminal_pty_queue_write(&pending,pty.master)<0){okay=0;break;}
        nx_window_info info;if(nx_window_get_info(window,&info)!=NX_OK || info.should_close){okay=0;break;}
        nx_event event;int available=0,focus_changed=0;
        do {
            if(nx_window_next_event(window,&event,&available)!=NX_OK){okay=0;break;}
            if(!available)break;
            if(event.type==NX_EVENT_FOCUS) {
                ++guard.epoch;guard.focused=!!event.focused;overlay.current=guard;
                terminal_ime_reset(&ime);focus_changed=1;caret_set=0;dirty=1;
            } else if(guard.focused && !focus_changed && event.type==NX_EVENT_IME_PREEDIT_CHANGED) {
                nx_result r=terminal_ime_native_preedit(&ime,guard,window,&event);
                if(r!=NX_OK && r!=NX_ERROR_STATE){okay=0;break;}
                if(r==NX_OK)dirty=1;
            } else if(guard.focused && !focus_changed && event.type==NX_EVENT_TEXT && event.ime_target) {
                int ready=0;nx_utf8_span text={0};
                if(terminal_ime_commit_event(&ime,guard,&event,&ready,&text)!=NX_OK){okay=0;break;}
                if(ready){
                    if(commit_seen || !terminal_pty_queue_push(&pending,text.data,text.length)){okay=0;break;}
                    commit_seen=1;dirty=1;
                }
            }
        } while(available);
        if(!okay)break;
        /* The backend may already have dispatched a later enter while older
           focus events remain queued. Sync the IME once to the final state. */
        if(focus_changed) {
            if(guard.focused) {
                if(terminal_ime_bind(&ime,guard)!=NX_OK ||
                   nx_window_set_ime_surrounding_text(window,guard.identity,guard.epoch,"",0,0,0)!=NX_OK ||
                   nx_window_set_ime_enabled(window,1)!=NX_OK){okay=0;break;}
                puts("TERMINAL_IME_FOCUSED");fflush(stdout);
            } else {
                if(nx_window_set_ime_enabled(window,0)!=NX_OK){okay=0;break;}
                if(cancel_test && preedit_seen && !cancel_reported) {
                    puts("TERMINAL_IME_CANCELLED");fflush(stdout);cancel_reported=1;
                }
            }
        }
        unsigned matching_cells=count_scalar(model,expected);
        if(commit_seen && matching_cells>1){okay=0;break;}
        if(commit_seen && matching_cells==1){puts("TERMINAL_IME_PTY_COMMITTED");fflush(stdout);break;}
        if(!dirty)continue;
        nx_result r=guard.focused?aetheri_terminal_view_frame_with_overlay(view,revision,info.scale,terminal_ime_overlay_draw,&overlay):
                                     aetheri_terminal_view_frame(view,revision,info.scale);
        if(r!=NX_OK){okay=0;break;}
        if(guard.focused) {
            nx_rect caret;
            if(terminal_ime_overlay_caret(&overlay,&d,&caret)!=NX_OK){okay=0;break;}
            if(!caret_set || caret.x!=last_caret.x || caret.y!=last_caret.y ||
               caret.width!=last_caret.width || caret.height!=last_caret.height) {
                if(terminal_ime_overlay_publish_caret(&overlay,&d,window,(nx_point){0,0})!=NX_OK){okay=0;break;}
                last_caret=caret;caret_set=1;
            }
        }
        const nx_command *commands=NULL;size_t count=0;nx_software_view_outcome outcome;
        if(nx_frame_commands(aetheri_terminal_view_context(view),&commands,&count)!=NX_OK ||
           nx_software_view_render(renderer,commands,count,(nx_color){0,0,0,1},&outcome)!=NX_OK){okay=0;break;}
        dirty=outcome!=NX_SOFTWARE_VIEW_READY;
        if(!shown && outcome==NX_SOFTWARE_VIEW_READY){puts("TERMINAL_IME_READY");fflush(stdout);shown=1;}
        if(!preedit_seen && guard.focused && ime.composition.size && outcome==NX_SOFTWARE_VIEW_READY) {
            puts("TERMINAL_IME_PREEDIT");fflush(stdout);preedit_seen=1;
        }
    }
    if(!preedit_seen || !commit_seen || pending.length || count_scalar(model,expected)!=1 ||
       (cancel_test && !cancel_reported))okay=0;
cleanup:
    terminal_pty_queue_clear(&pending);
    terminal_ime_reset(&ime);
    if(renderer && nx_software_view_close(&renderer)!=NX_OK)okay=0;
    if(window)nx_window_destroy(window);
    if(view)aetheri_terminal_view_destroy(view);
    if(pty.pid>0 && !pty.ended) {
        const char end[]="\x15" "exit 0\n";
        if(!send_bytes(pty.master,end,sizeof(end)-1) || !finish(&pty,model)) {
            (void)ae_terminal_pty_hangup(&pty);
            if(!finish(&pty,model) && !pty.ended){(void)kill(pty.pid,SIGKILL);(void)finish(&pty,model);}
            okay=0;
        }
    }
    if(pty.pid>0 && pty.ended && pty.eof){if(getenv("NEXAUI_IME_PTY_TRACE"))fprintf(stderr,"\nPTY status %d\n",pty.status);
        if(pty.status!=0 || ae_terminal_pty_close(&pty)!=0)okay=0;}
    else if(pty.pid>0)okay=0;
    if(source>=0)close(source);
    free(model);
    if(font)nx_font_close(font);
    free(font_bytes);
    if(okay){puts("TERMINAL_IME_CLOSED");fflush(stdout);}else fputs("Native terminal IME test failed\n",stderr);
    return okay?0:1;
}
