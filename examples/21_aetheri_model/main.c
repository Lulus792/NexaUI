#define _POSIX_C_SOURCE 200809L
#include "view.h"
#include <nexaui/software.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    if(argc<2 || argc>3 || (argc==3 && strcmp(argv[2],"--verify"))) return 2;
    int verify=argc==3, okay=1, dirty=1, unavailable=0, busy=0;
    double next_ax_retry=0;
    nx_window_info previous={0};
    size_t length=0; unsigned char *font=load_font(argv[1],&length);
    struct ae_settings model; ae_settings_init(&model);
    nx_window *window=NULL; nx_software_view *renderer=NULL; aetheri_view *view=NULL;
    nx_window_desc desc={"Aetheri settings model",800,480,0,{0}};
    if(!font || nx_window_create(&desc,&window)!=NX_OK ||
       !aetheri_view_create(&model,font,length,&view) || nx_software_view_open(window,&renderer)!=NX_OK) {
        okay=0; goto done;
    }
    while(okay) {
        nx_window_info info; nx_event event; int available=0, quit=0;
        if(nx_window_poll(window,busy?16:dirty?0:unavailable?1000:UINT32_MAX)!=NX_OK ||
           nx_window_get_info(window,&info)!=NX_OK) {okay=0;break;}
        if(info.should_close) break;
        do {
            if(nx_window_next_event(window,&event,&available)!=NX_OK) {okay=0;break;}
            if(available) {
                if(event.type==NX_EVENT_KEY_DOWN && event.key==NX_KEY_ESCAPE) quit=1;
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
        if(verify) {printf("MODEL theme=%d german=%d text=%u\n",(int)model.theme,model.german,model.text_percent);fflush(stdout);}
        dirty=changed;
    }
done:
    if(renderer && nx_software_view_close(&renderer)!=NX_OK) okay=0;
    if(window && view && nx_window_publish_semantics(window,NULL)!=NX_OK) okay=0;
    aetheri_view_destroy(view); nx_window_destroy(window); free(font);
    if(!okay) fputs("Aetheri model view failed\n",stderr);
    return okay?0:1;
}
