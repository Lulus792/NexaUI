#define _POSIX_C_SOURCE 200809L
/* Independent AT-SPI fixture with app-owned terminal model; no native window. */
#include "view.h"
#include <nexaui/linux_accessibility_host.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"terminal-ax:%d: %s\n",__LINE__,#x);return 1;}} while(0)
static volatile sig_atomic_t stop;
static void on_signal(int sig){(void)sig;stop=1;}
static int width(uint32_t c){return c==0x754c?2:c==0x301?0:1;}
static nx_result position(void *user,uint32_t base,uint32_t mark,unsigned index,float *x,float *y){
    (void)user;(void)base;(void)mark;(void)index;*x=0;*y=0;return NX_OK;
}
static unsigned char *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);if(n<=0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}fclose(f);*size=(size_t)n;return p;
}
int main(int argc,char **argv) {
    CHECK(argc==2);size_t length=0;unsigned char *bytes=read_file(argv[1],&length);CHECK(bytes);
    nx_font *font=NULL;CHECK(nx_font_open(bytes,length,NULL,&font)==NX_OK);
    struct ae_terminal *model=malloc(sizeof(*model));CHECK(model);ae_terminal_init(model,8,3,width);
    const char input[]="A\xe7\x95\x8c" "e\xcc\x81";ae_terminal_feed(model,input,sizeof(input)-1);
    CHECK(ae_terminal_select_begin(model,0,4) && ae_terminal_select_extend(model,0,1));
    if(getenv("NEXAUI_TERMINAL_AX_SCROLL")) {
        ae_terminal_select_clear(model);
        const char lines[]="\r\n1\r\n2\r\n3\r\n4";
        ae_terminal_feed(model,lines,sizeof(lines)-1);
        CHECK(model->history_count>0);
        ae_terminal_scroll(model,1);
    }
    aetheri_terminal_view_desc d={.model=model,.regular=font,.bold=font,.text_identity=123,
        .cell_width=12,.cell_height=22,.baseline=17,.pixels_per_em=16,.mark_position=position};
    aetheri_terminal_view *view=NULL;CHECK(aetheri_terminal_view_create(&d,&view)==NX_OK);
    CHECK(aetheri_terminal_view_frame(view,1,1)==NX_OK);
    nx_linux_accessibility_host *host=NULL;CHECK(nx_linux_accessibility_host_create(&host)==NX_OK);
    nx_linux_accessibility_geometry geometry={0,0,96,66,0,1,1};
    CHECK(nx_linux_accessibility_host_publish(host,aetheri_terminal_view_context(view),&geometry)==NX_OK);
    struct sigaction action={0};action.sa_handler=on_signal;sigemptyset(&action.sa_mask);
    CHECK(!sigaction(SIGTERM,&action,NULL) && !sigaction(SIGINT,&action,NULL));
    while(!stop) {
        int fd=-1,writable=0;uint32_t timeout=1000;
        CHECK(nx_linux_accessibility_host_prepare(host,&fd,&writable,&timeout)==NX_OK);
        struct pollfd p={fd,(short)(POLLIN|(writable?POLLOUT:0)),0};
        int r=poll(fd<0?NULL:&p,fd<0?0:1,(int)timeout);if(r<0 && errno==EINTR)continue;CHECK(r>=0);
        timeout=0;CHECK(nx_linux_accessibility_host_prepare(host,&fd,&writable,&timeout)==NX_OK);
        size_t routed=0;int focus=0;CHECK(nx_linux_accessibility_host_route(host,aetheri_terminal_view_context(view),&routed,&focus)==NX_OK);
        CHECK(!routed && !focus);
    }
    CHECK(nx_linux_accessibility_host_destroy(&host)==NX_OK);
    aetheri_terminal_view_destroy(view);free(model);nx_font_close(font);free(bytes);return 0;
}
