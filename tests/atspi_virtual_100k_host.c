#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include "nexaui/linux_accessibility_host.h"
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"virtual-host:%d: %s\n",__LINE__,#x);goto done;}} while(0)
static volatile sig_atomic_t stop,scroll_requested,filter_requested;
static void signal_handler(int sig) {
    if(sig==SIGUSR1)scroll_requested=1;else if(sig==SIGUSR2)filter_requested=1;else stop=1;
}
typedef struct model {nx_context *ctx;nx_virtual_list *list;uint64_t selected;double offset;int filtered;} model;
static nx_result source_row(void *user,size_t index,uint64_t *key,double *height) {
    model *m=user;*key=1000+(m->filtered?index*2:index);*height=24;return NX_OK;
}
static nx_result build_row(void *user,const nx_virtual_row *row) {
    model *m=user;char name[32];int n=snprintf(name,sizeof(name),"file-%06llu",(unsigned long long)(row->key-1000));
    if(n<=0 || (size_t)n>=sizeof(name))return NX_ERROR_STATE;
    nx_tree_item_desc item={0};item.control.id=row->key;item.control.parent_id=2;
    item.control.name=(nx_utf8_span){name,(size_t)n};item.control.bounds=(nx_rect){0,(float)row->viewport_y,500,24};
    nx_interaction_state state;return nx_tree_item(m->ctx,&item,NULL,&m->selected,&state);
}
static int frame(model *m) {
    nx_semantic_desc root={0};root.id=1;root.role=NX_ROLE_WINDOW;
    root.name=(nx_utf8_span){"Virtual corpus",14};root.bounds=(nx_rect){0,0,500,480};
    nx_container_desc tree={0};tree.id=2;tree.parent_id=1;tree.name=(nx_utf8_span){"Files",5};tree.bounds=root.bounds;
    nx_virtual_range range;size_t visited=0,nodes=0;
    return nx_begin_frame(m->ctx,500,480,0)==NX_OK && nx_semantics_add(m->ctx,&root,NULL)==NX_OK &&
        nx_tree(m->ctx,&tree)==NX_OK &&
        nx_virtual_list_visit(m->list,m->offset,480,0,25,build_row,m,&range,&visited)==NX_OK &&
        visited<=21 && nx_end_frame(m->ctx)==NX_OK &&
        nx_semantics_count(m->ctx,&nodes)==NX_OK && nodes==visited+2;
}
int main(void) {
    int okay=0;model m={0};nx_linux_accessibility_host *host=NULL;
    CHECK(nx_context_create(NULL,&m.ctx)==NX_OK && nx_virtual_list_create(0,24,NULL,&m.list)==NX_OK);
    CHECK(nx_virtual_list_rebuild(m.list,100000,source_row,&m,480,&m.offset)==NX_OK && frame(&m));
    CHECK(nx_linux_accessibility_host_create(&host)==NX_OK);
    nx_linux_accessibility_geometry geometry={0,0,500,480,0,1,1};
    struct sigaction action={0};action.sa_handler=signal_handler;sigemptyset(&action.sa_mask);
    CHECK(!sigaction(SIGTERM,&action,NULL) && !sigaction(SIGINT,&action,NULL) &&
          !sigaction(SIGUSR1,&action,NULL) && !sigaction(SIGUSR2,&action,NULL));
    CHECK(nx_linux_accessibility_host_publish(host,m.ctx,&geometry)==NX_OK);
    while(!stop) {
        int dirty=0;
        if(scroll_requested){scroll_requested=0;m.offset=50000.0*24;dirty=1;}
        if(filter_requested){filter_requested=0;m.filtered=1;
            CHECK(nx_virtual_list_rebuild(m.list,50000,source_row,&m,480,&m.offset)==NX_OK && m.offset==25000.0*24);dirty=1;}
        int fd=-1,writable=0;uint32_t timeout=1000;
        CHECK(nx_linux_accessibility_host_prepare(host,&fd,&writable,&timeout)==NX_OK);
        struct pollfd p={fd,(short)(POLLIN|(writable?POLLOUT:0)),0};
        int result=poll(fd<0?NULL:&p,fd<0?0:1,dirty?0:(int)timeout);
        if(result<0)continue;
        size_t routed=0;int focus=0;
        CHECK(nx_linux_accessibility_host_route(host,m.ctx,&routed,&focus)==NX_OK && !focus);
        if(dirty || routed)CHECK(frame(&m) && nx_linux_accessibility_host_publish(host,m.ctx,&geometry)==NX_OK);
    }
    okay=1;
done:
    if(host && nx_linux_accessibility_host_destroy(&host)!=NX_OK)okay=0;
    nx_virtual_list_destroy(m.list);nx_context_destroy(m.ctx);return okay?0:1;
}
