#include "paste_review.h"
#include <nexaui/software.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Private-test consumer only. It never writes clipboard data to a PTY. */
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
    int okay=0,shown=0,review_shown=0,dirty=1,done=0;size_t size=0;
    unsigned char *bytes=NULL;nx_font *font=NULL;nx_font_collection *fonts=NULL;nx_context *base=NULL;
    nx_window *window=NULL;nx_software_view *renderer=NULL;terminal_paste_review *review=NULL;
    terminal_paste_flow flow={0};terminal_paste_guard guard={1,1,1,0};
    if(argc!=2 || !(bytes=read_file(argv[1],&size)) || nx_font_open(bytes,size,NULL,&font)!=NX_OK ||
       nx_font_collection_create(NULL,&fonts)!=NX_OK || nx_font_collection_add(fonts,font)!=NX_OK ||
       nx_context_create(NULL,&base)!=NX_OK)goto cleanup;
    nx_window_desc desc={"NexaUI terminal paste test",640,360,0,{0}};
    if(nx_window_create(&desc,&window)!=NX_OK || nx_software_view_open(window,&renderer)!=NX_OK ||
       terminal_paste_init(&flow,terminal_paste_wayland_transport(window))!=NX_OK)goto cleanup;
    okay=1;
    while(okay && !done) {
        if(nx_window_poll(window,50)!=NX_OK){okay=0;break;}
        nx_window_info info;if(nx_window_get_info(window,&info)!=NX_OK){okay=0;break;}
        if(info.should_close)break;
        nx_event event;int available=0;
        do {
            if(nx_window_next_event(window,&event,&available)!=NX_OK){okay=0;break;}
            if(!available)break;
            if(event.type==NX_EVENT_FOCUS) {
                if(guard.focused!=!!event.focused){++guard.focus_epoch;guard.focused=!!event.focused;}
                if(!guard.focused && flow.state){terminal_paste_reset(&flow);done=1;puts("NATIVE_PASTE_FOCUS_CANCEL");fflush(stdout);}
            }
            if(event.type==NX_EVENT_RESIZE)dirty=1;
            if(!review && flow.state==1 && event.type==NX_EVENT_KEY_DOWN && event.key==NX_KEY_ESCAPE && !event.repeat) {
                terminal_paste_reset(&flow);puts("NATIVE_PASTE_RECEIVE_CANCELLED");fflush(stdout);done=1;break;
            }
            if(!review && event.type==NX_EVENT_KEY_DOWN && event.key==NX_KEY_V && !event.repeat && guard.focused &&
               (event.modifiers&(NX_MOD_CONTROL|NX_MOD_SHIFT|NX_MOD_ALT|NX_MOD_SUPER))==(NX_MOD_CONTROL|NX_MOD_SHIFT) && !flow.state) {
                if(terminal_paste_begin(&flow,guard,5000)!=NX_OK){okay=0;break;}
                puts("NATIVE_PASTE_PENDING");fflush(stdout);
            } else if(review) {
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
        if(!dirty)continue;
        nx_context *context=review?terminal_paste_review_context(review):base;
        enum terminal_review_result result=TERMINAL_REVIEW_OPEN;nx_utf8_span confirmed={0};
        if(review) {
            if(terminal_paste_review_configure(review,info.width,info.height,16,0)!=NX_OK ||
               terminal_paste_review_frame(review,guard,&result,&confirmed)!=NX_OK){okay=0;break;}
            if(result==TERMINAL_REVIEW_CANCELLED){puts("NATIVE_PASTE_CANCELLED");fflush(stdout);done=1;continue;}
            if(result==TERMINAL_REVIEW_CONFIRMED){printf("NATIVE_PASTE_ACCEPTED %zu %016llx\n",confirmed.length,
                (unsigned long long)checksum(confirmed.data,confirmed.length));fflush(stdout);done=1;continue;}
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
    if(!okay)fputs("Native paste test consumer failed\n",stderr);
    return okay && done?0:1;
}
