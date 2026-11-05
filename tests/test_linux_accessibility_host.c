#define _POSIX_C_SOURCE 200809L
#include "nexaui/linux_accessibility_host.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}} while(0)
static void *wrong_thread(void *p) {
    nx_linux_accessibility_host *h=p;return (void *)(uintptr_t)(nx_linux_accessibility_host_clear(h)==NX_ERROR_STATE);
}
int main(void) {
    CHECK(!setenv("NO_AT_BRIDGE","1",1));
    nx_linux_accessibility_host *host=NULL;nx_context *ctx=NULL;
    CHECK(nx_linux_accessibility_host_create(&host)==NX_OK && host);
    CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    CHECK(nx_begin_frame(ctx,100,100,0)==NX_OK && nx_end_frame(ctx)==NX_OK);
    nx_linux_accessibility_geometry g={0,0,100,100,0,1,1};
    CHECK(nx_linux_accessibility_host_publish(host,ctx,&g)==NX_ERROR_UNSUPPORTED);
    int fd=99,writable=99;uint32_t timeout=100;
    CHECK(nx_linux_accessibility_host_prepare(host,&fd,&writable,&timeout)==NX_ERROR_UNSUPPORTED && fd==-1 && !writable);
    size_t routed=9;int focus=9;
    CHECK(nx_linux_accessibility_host_route(host,ctx,&routed,&focus)==NX_ERROR_UNSUPPORTED && !routed && !focus);
    g.width=-1;CHECK(nx_linux_accessibility_host_publish(host,ctx,&g)==NX_ERROR_ARGUMENT);
    g.width=100;g.focused=2;CHECK(nx_linux_accessibility_host_publish(host,ctx,&g)==NX_ERROR_ARGUMENT);
    g.focused=1;g.width=2147483648.0;CHECK(nx_linux_accessibility_host_publish(host,ctx,&g)==NX_ERROR_ARGUMENT);
    g.width=100;g.screen_position=1;g.x=2147483648.0;
    CHECK(nx_linux_accessibility_host_publish(host,ctx,&g)==NX_ERROR_ARGUMENT);
    pthread_t thread;void *result=NULL;
    CHECK(!pthread_create(&thread,NULL,wrong_thread,host) && !pthread_join(thread,&result) && (uintptr_t)result==1);
    CHECK(nx_linux_accessibility_host_clear(host)==NX_OK);
    CHECK(nx_linux_accessibility_host_destroy(&host)==NX_OK && !host);
    CHECK(nx_linux_accessibility_host_destroy(&host)==NX_OK);
    nx_context_destroy(ctx);return 0;
}
