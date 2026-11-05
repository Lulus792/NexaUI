#define _GNU_SOURCE
#include "view.h"
#include "pty.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Test fixture owns the original shell and PTY. No native window or global
   process/session claim: view and PTY lifetimes are deliberately distinct. */
#define CHECK(x) do {if(!(x)){fprintf(stderr,"pty-view:%d: %s (errno %d)\n",__LINE__,#x,errno);goto cleanup;}} while(0)
static int width(uint32_t c){(void)c;return 1;}
static nx_result mark(void *u,uint32_t b,uint32_t m,unsigned i,float *x,float *y) {
    (void)u;(void)b;(void)m;(void)i;*x=0;*y=0;return NX_OK;
}
static double now_seconds(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;return t.tv_sec+t.tv_nsec/1e9;}
static unsigned char *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);if(n<=0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}fclose(f);*size=(size_t)n;return p;
}
static int send_all(int fd,const char *text) {
    size_t length=strlen(text),at=0;double deadline=now_seconds()+5;
    while(at<length && now_seconds()<deadline) {
        ssize_t n=write(fd,text+at,length-at);
        if(n>0){at+=(size_t)n;continue;}
        if(n<0 && errno==EINTR)continue;
        if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)){struct pollfd p={fd,POLLOUT,0};if(poll(&p,1,100)<0 && errno!=EINTR)return 0;continue;}
        return 0;
    }
    return at==length;
}
static int has_line(const struct ae_terminal *model,const char *text) {
    size_t length=strlen(text);
    for(unsigned y=0;y<model->rows;++y){const struct ae_term_cell *row=ae_terminal_row(model,y);if(!row)continue;
        for(unsigned x=0;x+length<=model->cols;++x){size_t i=0;while(i<length && row[x+i].code==(unsigned char)text[i])++i;
            if(i==length)return 1;}}
    return 0;
}
static int pump_until(struct ae_terminal_pty *pty,struct ae_terminal *model,const char *needle,int finish) {
    double deadline=now_seconds()+10;
    while(now_seconds()<deadline) {
        struct pollfd p={pty->master,POLLIN|POLLHUP,0};int ready=poll(&p,1,100);
        if(ready<0 && errno!=EINTR)return 0;
        if(ready>0 && !pty->eof)for(;;){char bytes[4096];ssize_t n=ae_terminal_pty_read(pty,bytes,sizeof(bytes));
            if(n>0){if(getenv("NEXAUI_PTY_TRACE"))fwrite(bytes,1,(size_t)n,stderr);
                if(model)ae_terminal_feed(model,bytes,(size_t)n);continue;}
            if(n==0 || (n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)))break;
            if(n<0 && errno==EINTR)continue;return 0;}
        if(ae_terminal_pty_observe(pty)<0)return 0;
        if(needle && model && has_line(model,needle))return 1;
        if(finish && pty->eof && pty->ended)return 1;
        if(pty->ended && !finish){if(getenv("NEXAUI_PTY_TRACE"))fprintf(stderr,"\nchild status=%d\n",pty->status);return 0;}
    }
    return 0;
}
static int view_has(aetheri_terminal_view *view,uint64_t revision,const char *needle) {
    nx_utf8_span text;size_t anchor,cursor;int caret;
    if(aetheri_terminal_view_text(view,revision,&text,&anchor,&cursor,&caret)!=NX_OK)return 0;
    size_t n=strlen(needle);for(size_t i=0;i+n<=text.length;++i)if(!memcmp(text.data+i,needle,n))return 1;
    return 0;
}
int main(int argc,char **argv) {
    int okay=0,source=-1;size_t font_size=0;unsigned char *bytes=NULL;nx_font *font=NULL;
    struct ae_terminal *model=NULL;aetheri_terminal_view *view=NULL;
    struct ae_terminal_pty pty={.master=-1,.pin=-1,.pid=-1};
    CHECK(argc==3 && getuid()!=0 && setenv("TERM","dumb",1)==0);
    source=open(argv[1],O_RDONLY|O_CLOEXEC);CHECK(source>=3);
    /* Original open must run before font/UI/thread initialization. */
    CHECK(ae_terminal_pty_open(&pty,source,80,12)==0);close(source);source=-1;
    CHECK((bytes=read_file(argv[2],&font_size)) && nx_font_open(bytes,font_size,NULL,&font)==NX_OK);
    model=malloc(sizeof(*model));CHECK(model);ae_terminal_init(model,80,12,width);
    aetheri_terminal_view_desc desc={.model=model,.regular=font,.bold=font,.text_identity=101,
        .cell_width=8,.cell_height=16,.baseline=12,.pixels_per_em=12,.mark_position=mark};
    CHECK(send_all(pty.master,"printf '%s%s\\n' 'NEXA_' 'PTY_42'\n") && pump_until(&pty,model,"NEXA_PTY_42",0));
    CHECK(aetheri_terminal_view_create(&desc,&view)==NX_OK && aetheri_terminal_view_frame(view,1,1)==NX_OK && view_has(view,1,"NEXA_PTY_42"));
    const nx_command *commands=NULL;size_t count=0;CHECK(nx_frame_commands(aetheri_terminal_view_context(view),&commands,&count)==NX_OK && count);
    unsigned char *pixels=malloc(640u*192u*4u);CHECK(pixels);
    nx_result rendered=nx_headless_render(commands,count,pixels,640,192,640u*4u,(nx_color){0,0,0,1});free(pixels);CHECK(rendered==NX_OK);
    aetheri_terminal_view_destroy(view);view=NULL;
    errno=0;CHECK(ae_terminal_pty_close(&pty)<0 && errno==EBUSY && !pty.ended);
    CHECK(send_all(pty.master,"printf '%s%s\\n' 'KEEP_' 'ALIVE'\n") && pump_until(&pty,model,"KEEP_ALIVE",0));
    CHECK(aetheri_terminal_view_create(&desc,&view)==NX_OK && aetheri_terminal_view_frame(view,2,1)==NX_OK && view_has(view,2,"KEEP_ALIVE"));
    CHECK(send_all(pty.master,"exit 0\n") && pump_until(&pty,model,NULL,1) && pty.status==0);
    CHECK(ae_terminal_pty_close(&pty)==0);okay=1;
cleanup:
    if(view)aetheri_terminal_view_destroy(view);
    if(!okay && pty.pid>0 && !pty.ended)(void)ae_terminal_pty_hangup(&pty);
    if(!okay && pty.pid>0 && (!pty.ended || !pty.eof)) {
        if(!pump_until(&pty,model,NULL,1) && !pty.ended) {
            (void)kill(pty.pid,SIGKILL);(void)pump_until(&pty,model,NULL,1);
        }
    }
    if(pty.pid>0 && pty.ended && pty.eof)(void)ae_terminal_pty_close(&pty);
    if(source>=0)close(source);
    free(model);if(font)nx_font_close(font);free(bytes);
    if(okay)puts("Original Aetheri shell/PTY survives view replacement and exits cleanly");
    return okay?0:1;
}
