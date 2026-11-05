#define _POSIX_C_SOURCE 200809L
#include "view.h"
#include <nexaui/software.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stopping=0, signal_fd=-1;
static void stop_signal(int number) {
    (void)number;
    int saved=errno; stopping=1;
    if(signal_fd>=0) {char byte=1; (void)write((int)signal_fd,&byte,1);}
    errno=saved;
}
static int signal_pipe(int fds[2]) {
    if(pipe(fds)!=0) return 0;
    for(int i=0;i<2;++i) {
        int flags=fcntl(fds[i],F_GETFL), fdflags=fcntl(fds[i],F_GETFD);
        if(flags<0 || fdflags<0 || fcntl(fds[i],F_SETFL,flags|O_NONBLOCK)<0 ||
           fcntl(fds[i],F_SETFD,fdflags|FD_CLOEXEC)<0) goto fail;
    }
    signal_fd=fds[1];
    struct sigaction action={0}; action.sa_handler=stop_signal; sigemptyset(&action.sa_mask);
    if(sigaction(SIGTERM,&action,NULL)!=0 || sigaction(SIGINT,&action,NULL)!=0) goto fail;
    return 1;
fail:
    signal_fd=-1; close(fds[0]); close(fds[1]); fds[0]=fds[1]=-1; return 0;
}

static double monotonic_seconds(void) {
    struct timespec t;
    return clock_gettime(CLOCK_MONOTONIC,&t)==0 ? (double)t.tv_sec+(double)t.tv_nsec/1e9 : -1;
}

static unsigned char *load_font(const char *path, size_t *length) {
    FILE *f=fopen(path,"rb"); if(!f) return NULL;
    unsigned char *data=NULL; long n;
    if(fseek(f,0,SEEK_END)==0 && (n=ftell(f))>0 && n<64*1024*1024 &&
       fseek(f,0,SEEK_SET)==0 && (data=malloc((size_t)n)) &&
       fread(data,1,(size_t)n,f)==(size_t)n) *length=(size_t)n;
    else { free(data); data=NULL; }
    if(fclose(f)!=0) {free(data);data=NULL;}
    return data;
}
int main(int argc, char **argv) {
    const char *font_path=NULL, *cache_path=NULL; int verify=0, diagnostics=0;
    for(int i=1;i<argc;++i) {
        if(!strcmp(argv[i],"--font") && i+1<argc && !font_path) font_path=argv[++i];
        else if(!strcmp(argv[i],"--cache") && i+1<argc && !cache_path) cache_path=argv[++i];
        else if(!strcmp(argv[i],"--diagnostics") && !diagnostics) diagnostics=1;
        else if(!strcmp(argv[i],"--verify") && !verify) verify=1;
        else if(argv[i][0]!='-' && !font_path) font_path=argv[i];
        else return 2;
    }
    if(!font_path || (cache_path && !*cache_path)) return 2;
    int fds[2]={-1,-1}; if(!signal_pipe(fds)) return 1;
    int okay=1, dirty=1, unavailable=0, busy=0;
    double next_ax_retry=0;
    nx_window_info previous={0};
    size_t length=0; unsigned char *font=load_font(font_path,&length);
    struct ae_settings model; ae_settings_init(&model);
    nx_window *window=NULL; nx_software_view *renderer=NULL; aetheri_view *view=NULL;
    nx_window_desc desc={"Aetheri settings model",800,480,0,{0}};
    if(!font || nx_window_create(&desc,&window)!=NX_OK ||
       !aetheri_view_create(&model,font,length,&view) || nx_software_view_open(window,&renderer)!=NX_OK) {
        okay=0; goto done;
    }
    while(okay && !stopping) {
        nx_window_info info; nx_event event; int available=0, quit=0;
        uint32_t signal_events=0;
        if(nx_window_poll_with_fd(window,busy?16:dirty?0:unavailable?1000:UINT32_MAX,fds[0],&signal_events)!=NX_OK ||
           nx_window_get_info(window,&info)!=NX_OK) {okay=0;break;}
        if(signal_events & (NX_WINDOW_FD_ERROR|NX_WINDOW_FD_HANGUP)) {okay=0;break;}
        if(stopping) break;
        if(info.should_close) break;
        do {
            if(nx_window_next_event(window,&event,&available)!=NX_OK) {okay=0;break;}
            if(available) {
                if(event.type==NX_EVENT_KEY_DOWN && (event.key==NX_KEY_ESCAPE ||
                   (event.key==NX_KEY_Q && event.modifiers==NX_MOD_CONTROL))) quit=1;
                else if(nx_context_queue_event(aetheri_view_context(view),&event)!=NX_OK) {okay=0;break;}
                dirty=1;
            }
        } while(available);
        /* Route native accessibility requests too; next_event only drains the
           ordinary window queue and does not transfer AT-SPI actions. */
        size_t routed=0;
        if(okay && nx_window_route_events(window,aetheri_view_context(view),&routed)!=NX_OK) okay=0;
        if(routed) dirty=1;
        if(!okay || quit) break;
        if(info.width!=previous.width || info.height!=previous.height || info.scale!=previous.scale) dirty=1;
        previous=info;
        if(!dirty) {
            if(unavailable) {
                double now=monotonic_seconds(); if(now<0) {okay=0;break;}
                if(now>=next_ax_retry) {
                    nx_result ax=nx_window_publish_semantics(window,aetheri_view_context(view));
                    if(ax!=NX_OK && ax!=NX_ERROR_UNSUPPORTED) {okay=0;break;}
                    unavailable=ax==NX_ERROR_UNSUPPORTED; next_ax_retry=now+1;
                }
            }
            continue;
        }
        if(info.width<480 || info.height<380) {dirty=0;busy=0;continue;}
        int changed=0;
        if(nx_context_set_text_raster_scale(aetheri_view_context(view),info.scale)!=NX_OK ||
           !aetheri_view_frame(view,info.width,info.height,&changed)) {okay=0;break;}
        if(aetheri_view_close_requested(view)) break;
        const nx_command *commands=NULL; size_t count=0; nx_color background;
        nx_software_view_outcome outcome;
        if(nx_frame_commands(aetheri_view_context(view),&commands,&count)!=NX_OK ||
           nx_theme_color(aetheri_view_context(view),NX_COLOR_BACKGROUND,&background)!=NX_OK ||
           nx_software_view_render(renderer,commands,count,background,&outcome)!=NX_OK) {okay=0;break;}
        busy=outcome==NX_SOFTWARE_VIEW_BUSY;
        if(busy) {dirty=1;continue;}
        if(outcome!=NX_SOFTWARE_VIEW_READY) {dirty=0;continue;}
        nx_result ax=nx_window_publish_semantics(window,aetheri_view_context(view));
        if(ax!=NX_OK && ax!=NX_ERROR_UNSUPPORTED) {okay=0;break;}
        unavailable=ax==NX_ERROR_UNSUPPORTED;
        if(unavailable) {
            double now=monotonic_seconds(); if(now<0) {okay=0;break;}
            next_ax_retry=now+1;
        }
        if(verify || diagnostics) {printf("MODEL theme=%d german=%d text=%u\n",(int)model.theme,model.german,model.text_percent);fflush(stdout);}
        dirty=changed;
    }
done:
    if(renderer && nx_software_view_close(&renderer)!=NX_OK) okay=0;
    if(window && view && nx_window_publish_semantics(window,NULL)!=NX_OK) okay=0;
    aetheri_view_destroy(view); nx_window_destroy(window); free(font);
    signal_fd=-1; close(fds[0]); close(fds[1]);
    if(stopping && diagnostics) {puts("SHUTDOWN signal");fflush(stdout);}
    if(!okay) fputs("Aetheri model view failed\n",stderr);
    return okay?0:1;
}
