#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include "nexaui/software.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;} } while(0)
typedef struct allocator_state {
    nx_window *members[2]; size_t live; int armed,called,destroy_peer;
    nx_result nested; uint32_t flags;
} allocator_state;
static void *allocate(void *user,size_t size) {
    allocator_state *s=user;void *p=malloc(size);if(p)++s->live;return p;
}
static void deallocate(void *user,void *ptr,size_t size) {
    (void)size;allocator_state *s=user;
    if(s->armed) {
        s->armed=0;++s->called;s->flags=99;
        s->nested=nx_window_poll_many(s->members,2,0,-1,&s->flags);
        if(s->destroy_peer){nx_window_destroy(s->members[1]);s->members[1]=NULL;}
    }
    if(!s->live){fputs("allocator underflow\n",stderr);abort();}
    --s->live;free(ptr);
}
static int drain(nx_window *w) {
    nx_event e;int available;
    do {if(nx_window_next_event(w,&e,&available)!=NX_OK)return 0;}while(available);
    return 1;
}
int main(int argc,char **argv) {
    int destroy=argc==2 && !strcmp(argv[1],"--destroy-peer");CHECK(argc==1 || destroy);
    allocator_state s={0};s.destroy_peer=destroy;
    nx_window_desc d={"poll many allocator",240,120,0,{&s,allocate,deallocate}};
    nx_result result=nx_window_create(&d,&s.members[0]);
    if(result==NX_ERROR_PLATFORM || result==NX_ERROR_UNSUPPORTED)return 77;
    CHECK(result==NX_OK);d.allocator=(nx_allocator){0};d.title_utf8="poll many peer";
    CHECK(nx_window_create(&d,&s.members[1])==NX_OK);
    nx_software_view *view=NULL;CHECK(nx_software_view_open(s.members[0],&view)==NX_OK);
    nx_context *ctx=NULL;CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    nx_window_info info;CHECK(nx_window_get_info(s.members[0],&info)==NX_OK);
    CHECK(nx_begin_frame(ctx,info.width,info.height,0)==NX_OK);
    CHECK(nx_fill_rect(ctx,(nx_rect){0,0,info.width,info.height},(nx_color){.2f,.4f,.6f,1})==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK);
    const nx_command *commands=NULL;size_t count=0;CHECK(nx_frame_commands(ctx,&commands,&count)==NX_OK);
    nx_software_view_outcome outcome;CHECK(nx_software_view_render(view,commands,count,(nx_color){0,0,0,1},&outcome)==NX_OK && outcome==NX_SOFTWARE_VIEW_READY);
    CHECK(drain(s.members[0]) && drain(s.members[1]));s.armed=1;
    struct timespec start,now;CHECK(clock_gettime(CLOCK_MONOTONIC,&start)==0);
    do {
        uint32_t flags=0;result=nx_window_poll_many(s.members,2,1000,-1,&flags);
        if(s.called)break;
        CHECK(result==NX_OK && drain(s.members[0]) && drain(s.members[1]));
        CHECK(clock_gettime(CLOCK_MONOTONIC,&now)==0);
    } while(now.tv_sec-start.tv_sec<5);
    CHECK(s.called==1 && s.nested==NX_ERROR_STATE && !s.flags);
    CHECK(result==(destroy?NX_ERROR_STATE:NX_OK));
    uint32_t flags=0;CHECK(nx_window_poll_many(&s.members[0],1,0,-1,&flags)==NX_OK);
    CHECK(nx_software_view_close(&view)==NX_OK);nx_context_destroy(ctx);
    nx_window_destroy(s.members[0]);if(s.members[1])nx_window_destroy(s.members[1]);
    CHECK(!s.live);puts("allocator reentry guarded and window ownership retained");return 0;
}
