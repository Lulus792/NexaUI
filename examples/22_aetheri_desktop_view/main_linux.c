#define _POSIX_C_SOURCE 200809L
#include "view.h"
#include <nexaui/software.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t stopping,signal_writer=-1;
static void stop_signal(int sig) {
    (void)sig;int saved=errno;stopping=1;
    if(signal_writer>=0){char b=1;(void)write((int)signal_writer,&b,1);}errno=saved;
}
static unsigned char *font_load(const char *path,size_t *length) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;long n=0;unsigned char *p=NULL;
    if(!fseek(f,0,SEEK_END) && (n=ftell(f))>0 && n<64*1024*1024 && !fseek(f,0,SEEK_SET) &&
       (p=malloc((size_t)n)) && fread(p,1,(size_t)n,f)==(size_t)n)*length=(size_t)n;
    else {free(p);p=NULL;}
    if(fclose(f)){free(p);p=NULL;}return p;
}
static int signal_setup(int fds[2]) {
    if(pipe(fds))return 0;
    for(int i=0;i<2;++i) {
        int flags=fcntl(fds[i],F_GETFL),desc=fcntl(fds[i],F_GETFD);
        if(flags<0 || desc<0 || fcntl(fds[i],F_SETFL,flags|O_NONBLOCK)<0 || fcntl(fds[i],F_SETFD,desc|FD_CLOEXEC)<0)return 0;
    }
    struct sigaction action={0};action.sa_handler=stop_signal;sigemptyset(&action.sa_mask);
    signal_writer=fds[1];return !sigaction(SIGINT,&action,NULL) && !sigaction(SIGTERM,&action,NULL);
}
int main(int argc,char **argv) {
    int diagnostics=argc==3 && !strcmp(argv[1],"--diagnostics");
    if(argc!=2 && !diagnostics){fputs("usage: aetheri_desktop_preview [--diagnostics] FONT.ttf\n",stderr);return 2;}
    int okay=0,fds[2]={-1,-1},dirty=1,busy=0,initial_focus=0;size_t length=0;
    unsigned char *font=font_load(argv[diagnostics?2:1],&length);nx_window *window=NULL;
    nx_software_view *renderer=NULL;aetheri_desktop_view *view=NULL;
    struct ae_desktop_model model;ae_desktop_model_init(&model);ae_desktop_model_search(&model,0,true);
    uint64_t revision=1;nx_window_desc desc={"Aetheri desktop preview",640,480,0,{0}};
    if(!font || !signal_setup(fds) || nx_window_create(&desc,&window)!=NX_OK ||
       !aetheri_desktop_view_create(&model,font,length,&view) ||
       aetheri_desktop_view_set_window(view,window)!=NX_OK || nx_software_view_open(window,&renderer)!=NX_OK)goto done;
    okay=1;
    while(okay && !stopping) {
        nx_window_info info;uint32_t fd_events=0;
        if(nx_window_poll_with_fd(window,busy?16:dirty?0:UINT32_MAX,fds[0],&fd_events)!=NX_OK ||
           nx_window_get_info(window,&info)!=NX_OK || (fd_events&(NX_WINDOW_FD_ERROR|NX_WINDOW_FD_HANGUP))) {okay=0;break;}
        if(stopping || info.should_close)break;
        nx_event event;int available=0;
        do {
            if(nx_window_next_event(window,&event,&available)!=NX_OK){okay=0;break;}
            if(available) {
                if(event.type==NX_EVENT_KEY_DOWN && event.key==NX_KEY_ESCAPE){stopping=1;break;}
                if(aetheri_desktop_view_queue_event(view,&event)!=NX_OK){okay=0;break;}dirty=1;
            }
        } while(available);
        size_t routed=0;
        if(!okay || stopping)break;
        if(nx_window_route_events(window,aetheri_desktop_view_context(view),&routed)!=NX_OK){okay=0;break;}
        if(routed)dirty=1;
        if(!dirty)continue;
        if(info.width<320 || info.height<240){dirty=0;busy=0;continue;}
        aetheri_desktop_intent intent;
        if(nx_context_set_text_raster_scale(aetheri_desktop_view_context(view),info.scale)!=NX_OK ||
           !aetheri_desktop_view_frame(view,info.width,info.height,revision,&intent)){okay=0;break;}
        if(!initial_focus) {
            if(nx_focus_set(aetheri_desktop_view_context(view),20)!=NX_OK){okay=0;break;}
            initial_focus=1;
        }
        int model_changed=0;
        if(intent.kind==AETHERI_DESKTOP_QUERY) {
            ae_desktop_model_search(&model,0,true);
            if(!ae_desktop_model_text(&model,intent.query)){okay=0;break;}++revision;
            model_changed=1;
            if(diagnostics){printf("QUERY %s\n",model.query);fflush(stdout);}
        } else if(intent.kind==AETHERI_DESKTOP_SELECT) {
            model.selected=intent.window;model.selected_app=intent.app;++revision;model_changed=1;
        } else if(intent.kind==AETHERI_DESKTOP_LAUNCH || intent.kind==AETHERI_DESKTOP_ACTIVATE) {
            printf("INTENT kind=%d app=%d window=%u\n",intent.kind,intent.app,intent.window);fflush(stdout);
        }
        /* Publish the acknowledged model before reading another native event;
           otherwise newly arriving text could be mistaken for stale input. */
        if(model_changed) {
            aetheri_desktop_intent reconciled;
            if(!aetheri_desktop_view_frame(view,info.width,info.height,revision,&reconciled) ||
               reconciled.kind!=AETHERI_DESKTOP_NONE){okay=0;break;}
        }
        const nx_command *commands=NULL;size_t count=0;nx_software_view_outcome outcome;
        if(nx_frame_commands(aetheri_desktop_view_context(view),&commands,&count)!=NX_OK ||
           nx_software_view_render(renderer,commands,count,(nx_color){0,0,0,0},&outcome)!=NX_OK){okay=0;break;}
        busy=outcome==NX_SOFTWARE_VIEW_BUSY;
        if(outcome==NX_SOFTWARE_VIEW_READY) {
            nx_result ax=nx_window_publish_semantics(window,aetheri_desktop_view_context(view));
            if(ax!=NX_OK && ax!=NX_ERROR_UNSUPPORTED){okay=0;break;}
            if(diagnostics && initial_focus==2) {puts("READY");fflush(stdout);initial_focus=3;}
        }
        /* The revision guard is removed after the reconciliation frame.
           Publish a fresh frame so native clients see the editable state. */
        dirty=busy || model_changed;
        if(initial_focus==1){dirty=1;initial_focus=2;}
    }
done:
    signal_writer=-1;if(fds[0]>=0)close(fds[0]);if(fds[1]>=0)close(fds[1]);
    if(renderer && nx_software_view_close(&renderer)!=NX_OK)okay=0;
    if(window && view && nx_window_publish_semantics(window,NULL)!=NX_OK)okay=0;
    aetheri_desktop_view_destroy(view);nx_window_destroy(window);free(font);
    if(diagnostics){printf("CLOSED %d\n",okay);fflush(stdout);}
    return okay?0:1;
}
