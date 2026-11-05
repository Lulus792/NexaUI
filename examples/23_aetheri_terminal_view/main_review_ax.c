#define _POSIX_C_SOURCE 200809L
/* Headless AT-SPI host: accessibility focus and geometry are simulated. */
#include "paste_review.h"
#include <nexaui/linux_accessibility_host.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"review-ax:%d: %s\n",__LINE__,#x);return 1;}} while(0)
static volatile sig_atomic_t stop;
static void on_signal(int sig){(void)sig;stop=1;}
static int width(uint32_t c){(void)c;return 1;}
static unsigned char *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);if(n<=0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}fclose(f);*size=(size_t)n;return p;
}
int main(int argc,char **argv) {
    CHECK(argc==2);size_t size=0;unsigned char *bytes=read_file(argv[1],&size);CHECK(bytes);
    nx_font *font=NULL;nx_font_collection *fonts=NULL;CHECK(nx_font_open(bytes,size,NULL,&font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    terminal_paste_guard guard={11,1,1,1};terminal_paste_flow flow={0};flow.guard=guard;
    char text[257];memset(text,'A',128);memset(text+128,'B',128);text[256]='\n';
    CHECK(ae_terminal_paste_set(&flow.paste,text,sizeof(text))==AE_TERM_PASTE_OK);flow.state=2;
    terminal_paste_review *review=NULL;CHECK(terminal_paste_review_create(&flow,fonts,width,&review)==NX_OK);
    enum terminal_review_result result;nx_utf8_span output;
    CHECK(terminal_paste_review_frame(review,guard,&result,&output)==NX_OK && result==TERMINAL_REVIEW_OPEN);
    nx_linux_accessibility_host *host=NULL;CHECK(nx_linux_accessibility_host_create(&host)==NX_OK);
    nx_linux_accessibility_geometry geometry={0,0,640,360,0,1,1};
    CHECK(nx_linux_accessibility_host_publish(host,terminal_paste_review_context(review),&geometry)==NX_OK);
    struct sigaction action={0};action.sa_handler=on_signal;sigemptyset(&action.sa_mask);
    CHECK(!sigaction(SIGTERM,&action,NULL) && !sigaction(SIGINT,&action,NULL));
    while(!stop) {
        int fd=-1,writable=0;uint32_t timeout=1000;
        CHECK(nx_linux_accessibility_host_prepare(host,&fd,&writable,&timeout)==NX_OK);
        struct pollfd p={fd,(short)(POLLIN|(writable?POLLOUT:0)),0};
        int n=poll(fd<0?NULL:&p,fd<0?0:1,(int)timeout);if(n<0 && errno==EINTR)continue;CHECK(n>=0);
        timeout=0;CHECK(nx_linux_accessibility_host_prepare(host,&fd,&writable,&timeout)==NX_OK);
        size_t routed=0;int focus=0;
        CHECK(nx_linux_accessibility_host_route(host,terminal_paste_review_context(review),&routed,&focus)==NX_OK);
        if(routed || focus) {
            CHECK(terminal_paste_review_frame(review,guard,&result,&output)==NX_OK);
            if(result==TERMINAL_REVIEW_CONFIRMED) {
                CHECK(output.length==sizeof(text) && !memcmp(output.data,text,sizeof(text)));
                break;
            }
            if(result==TERMINAL_REVIEW_CANCELLED)break;
            CHECK(nx_linux_accessibility_host_publish(host,terminal_paste_review_context(review),&geometry)==NX_OK);
        }
    }
    CHECK(nx_linux_accessibility_host_destroy(&host)==NX_OK);
    terminal_paste_review_destroy(review);terminal_paste_reset(&flow);
    nx_font_collection_destroy(fonts);nx_font_close(font);free(bytes);return 0;
}
