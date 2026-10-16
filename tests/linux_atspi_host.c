#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "x11/window_internal.h"
#include "accessibility.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"host:%d: %s\n",__LINE__,#x); return 0; } } while (0)
typedef struct scene {
    nx_context *ui;
    unsigned count;
    int checked, removed, retyped, renamed, long_name;
    uint64_t required, optional;
} scene;
typedef struct heap { void *pointers[256]; size_t sizes[256],count; int fail_next; } heap;
static void *allocate(void *user,size_t bytes) {
    heap *h=user;
    if (h->fail_next) { h->fail_next=0; return NULL; }
    if (h->count==256u) abort();
    void *pointer=malloc(bytes);
    if (pointer) { h->pointers[h->count]=pointer; h->sizes[h->count++]=bytes; }
    return pointer;
}
static void deallocate(void *user,void *pointer,size_t bytes) {
    heap *h=user; size_t i=0; while (i<h->count && h->pointers[i]!=pointer) ++i;
    if (i==h->count || h->sizes[i]!=bytes) abort();
    --h->count; h->pointers[i]=h->pointers[h->count]; h->sizes[i]=h->sizes[h->count]; free(pointer);
}
static nx_utf8_span span(const char *s) { return (nx_utf8_span){s,strlen(s)}; }
static nx_control_desc control(uint64_t id,uint64_t parent,const char *name,nx_rect bounds) {
    nx_control_desc d={0}; d.id=id; d.parent_id=parent; d.name=span(name); d.bounds=bounds; return d;
}
static int frame(scene *s) {
    CHECK(nx_begin_frame(s->ui,640,480,0)==NX_OK);
    nx_semantic_desc root={0}; root.id=1; root.role=NX_ROLE_WINDOW;
    root.bounds=(nx_rect){0,0,640,480}; root.name=span("NexaCloud AT-SPI");
    CHECK(nx_semantics_add(s->ui,&root,NULL)==NX_OK);
    char name[32768];
    if (s->long_name) { memset(name,'Z',sizeof(name)-1u); name[sizeof(name)-1u]=0; }
    else strcpy(name,s->renamed?"Changed":"\xc3\x96" "ffnen \xf0\x9f\x93\x81");
    nx_control_desc d=control(2,1,name,(nx_rect){10,20,100,30});
    nx_interaction_state state;
    CHECK(nx_button(s->ui,&d,&state)==NX_OK); s->count+=(unsigned)state.clicked;
    memset(name,'X',strlen(name)); /* publication must not borrow this span */
    d=control(3,1,"Sync",(nx_rect){10,60,100,30}); CHECK(nx_checkbox(s->ui,&d,&s->checked,&state)==NX_OK);
    d=control(4,1,"Disabled",(nx_rect){10,100,100,30}); d.disabled=1; CHECK(nx_button(s->ui,&d,&state)==NX_OK);
    nx_semantic_desc secret={0}; secret.id=5; secret.parent_id=1; secret.role=NX_ROLE_TEXT_FIELD;
    secret.name=span("Password"); secret.value=span("do-not-expose"); secret.state=NX_SEM_PROTECTED;
    secret.bounds=(nx_rect){120,20,100,30}; CHECK(nx_semantics_add(s->ui,&secret,NULL)==NX_OK);
    char counter[64]; snprintf(counter,sizeof(counter),"Button count %u",s->count);
    nx_semantic_desc status={0}; status.id=6; status.parent_id=1; status.role=NX_ROLE_STATUS;
    status.name=span(counter); status.bounds=(nx_rect){10,180,100,30}; CHECK(nx_semantics_add(s->ui,&status,NULL)==NX_OK);
    if (!s->removed) {
        d=control(8,1,"Dynamic",(nx_rect){10,140,100,30});
        if (s->retyped) { int value=0; CHECK(nx_checkbox(s->ui,&d,&value,&state)==NX_OK); }
        else CHECK(nx_button(s->ui,&d,&state)==NX_OK);
    }
    for (unsigned g=0;g<2;++g) {
        nx_semantic_desc group={0}; group.id=50u+g*10u; group.parent_id=1;
        group.role=g?NX_ROLE_LIST:NX_ROLE_TAB_LIST; group.name=span(g?"Optional":"Required");
        group.bounds=(nx_rect){250,20+(float)g*80,180,70};
        if (g) { group.bounds.x+=0.75f; group.bounds.y+=0.25f; }
        uint64_t *choice=g?&s->optional:&s->required;
        CHECK(nx_selection_group(s->ui,&group,!g,choice)==NX_OK);
        uint64_t parent=group.id;
        if (g) {
            nx_semantic_desc panel=group; panel.id=64; panel.parent_id=60; panel.role=NX_ROLE_PANEL;
            panel.name=span("Layout"); CHECK(nx_semantics_add(s->ui,&panel,NULL)==NX_OK); parent=64;
        }
        for (unsigned i=0;i<2;++i) {
            d=control(group.id+1u+i,parent,i?"Second":"First",(nx_rect){250+(float)i*90,20+(float)g*80,80,30});
            if (g) { d.bounds.x+=1.25f; d.bounds.y+=0.75f; }
            if (g) {
                nx_widget_desc w={0}; w.semantic.id=d.id; w.semantic.parent_id=parent;
                w.semantic.name=d.name; w.semantic.bounds=d.bounds; w.semantic.role=NX_ROLE_LIST_ITEM;
                w.semantic.actions=NX_ACTION_SELECT|NX_ACTION_FOCUS;
                w.semantic.state=*choice==d.id?NX_SEM_SELECTED:0;
                w.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE;
                CHECK(nx_widget_register(s->ui,&w,&state)==NX_OK);
            } else CHECK(nx_tab(s->ui,&d,choice,&state)==NX_OK);
        }
    }
    CHECK(nx_end_frame(s->ui)==NX_OK); return 1;
}
int main(int argc,char **argv) {
    int optional=argc==2 && !strcmp(argv[1],"--optional");
    nx_window *window=NULL; nx_context *ui=NULL;
    nx_window_desc wd={"NexaCloud AT-SPI",640,480,0,{0}};
    heap memory={0}; wd.allocator=(nx_allocator){&memory,allocate,deallocate};
    if (nx_window_create(&wd,&window)!=NX_OK || nx_context_create(NULL,&ui)!=NX_OK) return 1;
    scene app={0}; app.ui=ui; app.required=51;
    if (!frame(&app)) return 1;
    nx_result published=nx_window_publish_semantics(window,ui);
    if (optional) {
        int good=published==NX_ERROR_UNSUPPORTED && nx_window_poll(window,0)==NX_OK && frame(&app);
        good=good && nx_window_publish_semantics(window,NULL)==NX_OK;
        nx_context_destroy(ui); nx_window_destroy(window); return good && !memory.count?0:1;
    }
    if (published!=NX_OK) { fprintf(stderr,"AT-SPI publish: %d\n",published); return 1; }
    setvbuf(stdout,NULL,_IOLBF,0); printf("READY %ld\n",(long)getpid());
    fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL)|O_NONBLOCK);
    int paused=0,attached=1,quit=0,resilient=0; uint32_t poll_timeout=50;
    char buffer[256]; size_t used=0;
    scene peer={0}; nx_window *peer_window=NULL; heap peer_memory={0};
    while (!quit) {
        if (nx_window_poll(window,poll_timeout)!=NX_OK) return 1;
        if (peer_window && nx_window_poll(peer_window,0)!=NX_OK) return 1;
        char command[256]={0};
        ssize_t n=read(STDIN_FILENO,buffer+used,sizeof(buffer)-used-1u);
        if (!n) break;
        if (n<0 && errno!=EAGAIN && errno!=EINTR) return 1;
        if (n>0) used+=(size_t)n;
        buffer[used]=0;
        char *newline=memchr(buffer,'\n',used);
        if (newline) {
            size_t length=(size_t)(newline-buffer); memcpy(command,buffer,length); command[length]=0;
            used-=length+1u; memmove(buffer,newline+1,used);
            if (!strcmp(command,"pause")) paused=1;
            else if (!strcmp(command,"resume")) paused=0;
            else if (!strcmp(command,"wait")) poll_timeout=2000;
            else if (!strcmp(command,"normal")) poll_timeout=50;
            else if (!strcmp(command,"remove")) app.removed=1;
            else if (!strcmp(command,"restore")) app.removed=0;
            else if (!strcmp(command,"retype")) app.retyped=1;
            else if (!strcmp(command,"rename")) app.renamed=1;
            else if (!strcmp(command,"long")) app.long_name=1;
            else if (!strcmp(command,"disconnect")) {
                if (nx_window_linux_ax_test_disconnect(window)!=NX_OK) return 1;
                resilient=1;
            }
            else if (!strcmp(command,"other")) {
                if (peer_window) return 1;
                wd.allocator=(nx_allocator){&peer_memory,allocate,deallocate}; peer.required=51;
                if (nx_window_create(&wd,&peer_window)!=NX_OK || nx_context_create(NULL,&peer.ui)!=NX_OK ||
                    !frame(&peer) || nx_window_publish_semantics(peer_window,peer.ui)!=NX_OK) return 1;
            } else if (!strcmp(command,"closeother")) {
                if (!peer_window) return 1;
                nx_context_destroy(peer.ui); peer.ui=NULL;
                nx_window_destroy(peer_window); peer_window=NULL;
                if (peer_memory.count) return 1;
            }
            else if (!strcmp(command,"move")) {
                Display *display=nx_window_x11_display(window);
                if (!display) return 1;
                XMoveWindow(display,nx_window_x11_id(window),80,90); XSync(display,False);
            } else if (!strcmp(command,"failure")) {
                paused=1; app.long_name=1;
                if (!frame(&app)) return 1;
                memory.fail_next=1;
                if (nx_window_publish_semantics(window,app.ui)!=NX_ERROR_MEMORY || memory.fail_next) return 1;
            } else if (!strcmp(command,"republish")) {
                if (nx_window_publish_semantics(window,app.ui)!=NX_OK) return 1;
            }
            else if (!strcmp(command,"detach")) { nx_window_publish_semantics(window,NULL); attached=0; }
            else if (!strcmp(command,"attach")) attached=1;
            else if (!strcmp(command,"replace")) {
                nx_context_destroy(app.ui); app.ui=NULL;
                if (nx_context_create(NULL,&app.ui)!=NX_OK) return 1;
            } else if (!strcmp(command,"quit")) quit=1;
            else if (strcmp(command,"step")) { fprintf(stderr,"Unknown host command: %s\n",command); return 1; }
        }
        if (!paused || !strcmp(command,"step")) {
            size_t count;
            if (attached && nx_window_route_events(window,app.ui,&count)!=NX_OK && strcmp(command,"replace")) return 1;
            if (!frame(&app)) return 1;
            if (attached) {
                nx_result publication=nx_window_publish_semantics(window,app.ui);
                if (publication!=NX_OK && !(resilient && publication==NX_ERROR_UNSUPPORTED)) return 1;
            }
            if (peer_window) {
                if (nx_window_route_events(peer_window,peer.ui,&count)!=NX_OK || !frame(&peer) ||
                    nx_window_publish_semantics(peer_window,peer.ui)!=NX_OK) return 1;
            }
        }
        if (command[0]) printf("ACK %s %u %d %llu %llu\n",command,app.count,app.checked,
            (unsigned long long)app.required,(unsigned long long)app.optional);
    }
    nx_window_publish_semantics(window,NULL); nx_context_destroy(app.ui); nx_window_destroy(window);
    if (peer_window) { nx_context_destroy(peer.ui); nx_window_destroy(peer_window); }
    return memory.count || peer_memory.count?1:0;
}
