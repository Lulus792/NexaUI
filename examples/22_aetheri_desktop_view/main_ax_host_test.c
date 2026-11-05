#define _POSIX_C_SOURCE 200809L
/* Headless AT-SPI host fixture; geometry/focus are simulated, not a compositor. */
#include "view.h"
#include <nexaui/linux_accessibility_host.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"ax-host:%d: %s\n",__LINE__,#x);return 1;}} while(0)
static volatile sig_atomic_t stop;
static volatile sig_atomic_t rebind_requested;
static void on_signal(int sig){(void)sig;stop=1;}
static void on_rebind(int sig){(void)sig;rebind_requested=1;}
struct reentry_test {nx_linux_accessibility_host *host;int enabled,armed,checked;};
static void *test_allocate(void *user,size_t size) {
    struct reentry_test *test=user;
    if(test->enabled && test->armed && !test->checked) {
        test->armed=0;nx_linux_accessibility_host *alias=test->host;
        nx_result clear=nx_linux_accessibility_host_clear(test->host);
        nx_result destroy=clear==NX_ERROR_STATE?nx_linux_accessibility_host_destroy(&alias):NX_OK;
        test->checked=(clear==NX_ERROR_STATE && destroy==NX_ERROR_STATE && alias==test->host)?1:-1;
        puts(test->checked==1?"REENTER_BLOCKED":"REENTER_FAILED");
    }
    return malloc(size);
}
static void test_deallocate(void *user,void *p,size_t size){(void)user;(void)size;free(p);}
static unsigned char *font_read(const char *path,size_t *length) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);
    if(n<=0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}
    fclose(f);*length=(size_t)n;return p;
}
int main(int argc,char **argv) {
    CHECK(argc==3 && !strcmp(argv[1],"--diagnostics"));
    size_t size=0;unsigned char *font=font_read(argv[2],&size);CHECK(font);
    struct ae_desktop_model model;ae_desktop_model_init(&model);ae_desktop_model_search(&model,0,true);
    nx_linux_accessibility_host *host=NULL;CHECK(nx_linux_accessibility_host_create(&host)==NX_OK);
    struct reentry_test test={host,getenv("NEXAUI_AX_REENTER")!=NULL,0,0};
    nx_context_desc context_desc={0};context_desc.allocator=(nx_allocator){&test,test_allocate,test_deallocate};
    aetheri_desktop_view *view=NULL;
    CHECK(aetheri_desktop_view_create_with_context(&model,font,size,&context_desc,&view));
    nx_linux_accessibility_geometry geometry={0,0,640,480,0,1,1};
    uint64_t revision=1;aetheri_desktop_intent intent;
    nx_context *other=NULL;CHECK(nx_context_create(NULL,&other)==NX_OK);
    CHECK(nx_linux_accessibility_host_publish(host,other,&geometry)==NX_ERROR_STATE);
    CHECK(nx_begin_frame(other,100,100,0)==NX_OK && nx_end_frame(other)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent));
    CHECK(nx_focus_set(aetheri_desktop_view_context(view),20)==NX_OK);
    CHECK(aetheri_desktop_view_surface_focus(view,1)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent));
    CHECK(nx_linux_accessibility_host_publish(host,aetheri_desktop_view_context(view),&geometry)==NX_OK);
    size_t wrong_count=9;int wrong_focus=9;
    CHECK(nx_linux_accessibility_host_route(host,other,&wrong_count,&wrong_focus)==NX_ERROR_STATE && !wrong_count && !wrong_focus);
    nx_context_destroy(other);
    setvbuf(stdout,NULL,_IOLBF,0);puts("READY");
    struct sigaction action={0};action.sa_handler=on_signal;sigemptyset(&action.sa_mask);
    CHECK(!sigaction(SIGTERM,&action,NULL) && !sigaction(SIGINT,&action,NULL));
    action.sa_handler=on_rebind;CHECK(!sigaction(SIGUSR1,&action,NULL));
    while(!stop) {
        if(rebind_requested) {
            rebind_requested=0;CHECK(nx_linux_accessibility_host_clear(host)==NX_OK);
            CHECK(nx_linux_accessibility_host_publish(host,aetheri_desktop_view_context(view),&geometry)==NX_OK);
            puts("REBOUND");
        }
        int fd=-1,write=0;uint32_t timeout=1000;
        CHECK(nx_linux_accessibility_host_prepare(host,&fd,&write,&timeout)==NX_OK);
        struct pollfd item={fd,(short)(POLLIN|(write?POLLOUT:0)),0};
        int r=poll(fd<0?NULL:&item,fd<0?0:1,(int)timeout);
        if(r<0 && errno==EINTR)continue;CHECK(r>=0);
        timeout=0;CHECK(nx_linux_accessibility_host_prepare(host,&fd,&write,&timeout)==NX_OK);
        size_t routed=0;int focus=0;
        test.armed=1;
        CHECK(nx_linux_accessibility_host_route(host,aetheri_desktop_view_context(view),&routed,&focus)==NX_OK);
        test.armed=0;CHECK(test.checked>=0);
        if(!routed)continue;
        CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent));
        if(intent.kind==AETHERI_DESKTOP_QUERY) {
            ae_desktop_model_search(&model,0,true);CHECK(ae_desktop_model_text(&model,intent.query));++revision;
            printf("QUERY %s\n",model.query);
            CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent));
            CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent));
        } else if(intent.kind==AETHERI_DESKTOP_SELECT) {
            model.selected=intent.window;model.selected_app=intent.app;++revision;
            printf("SELECT %d %u\n",model.selected_app,model.selected);
            CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
            CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
        } else if(intent.kind!=AETHERI_DESKTOP_NONE)
            printf("INTENT kind=%d app=%d window=%u\n",intent.kind,intent.app,intent.window);
        CHECK(nx_linux_accessibility_host_publish(host,aetheri_desktop_view_context(view),&geometry)==NX_OK);
    }
    CHECK(nx_linux_accessibility_host_destroy(&host)==NX_OK);
    aetheri_desktop_view_destroy(view);free(font);return 0;
}
