#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include "accessibility.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"multi-host:%d: %s\n",__LINE__,#x); return 1; } } while (0)
typedef struct allocation {void *p;size_t bytes;} allocation;
typedef struct heap {allocation live[4096];size_t count,calls;int fail,when,skip;void (*hook)(void *);void *user;} heap;
static void call(heap *h,int when){if(h->hook && h->when==when){if(h->skip){--h->skip;return;}void (*f)(void *)=h->hook;h->hook=NULL;f(h->user);}}
static void *allocate(void *user,size_t bytes) {
    heap *h=user;++h->calls;call(h,1);if(h->fail){h->fail=0;return NULL;}
    void *p=malloc(bytes);if(p){if(h->count==4096)abort();h->live[h->count++]=(allocation){p,bytes};}return p;
}
static void deallocate(void *user,void *p,size_t bytes) {
    heap *h=user;size_t at=0;while(at<h->count && h->live[at].p!=p)++at;
    if(at==h->count || h->live[at].bytes!=bytes)abort();h->live[at]=h->live[--h->count];call(h,2);free(p);
}
typedef struct scene {
    nx_context *ctx;nx_window *window;nx_selection_model *model,*table;
    heap core,native;int kind,disabled,required,demoted,large,owner_disabled,clicks,slot_clicks,paused;
    int hook_kind,hook_fired,reconnecting;nx_result hook_result,last_route,last_publish;uint64_t single;
} scene;
static nx_semantic_desc desc(uint64_t id,uint64_t parent,nx_semantic_role role,const char *name) {
    nx_semantic_desc d={0};d.id=id;d.parent_id=parent;d.role=role;d.name=(nx_utf8_span){name,strlen(name)};
    d.bounds=(nx_rect){10,10,100,20};return d;
}
static int frame(scene *s) {
    CHECK(nx_begin_frame(s->ctx,640,480,0)==NX_OK);
    nx_semantic_desc d=desc(1,0,NX_ROLE_WINDOW,"AT-SPI multiple selection");CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK);
    d=desc(100,1,s->kind==3?NX_ROLE_TREE:NX_ROLE_LIST,"Multi");if(s->owner_disabled)d.state|=NX_SEM_DISABLED;
    char large[65536];if(s->large){memset(large,'X',sizeof(large));d.description=(nx_utf8_span){large,sizeof(large)};}
    if(s->demoted)CHECK(nx_selection_group(s->ctx,&d,0,&s->single)==NX_OK);
    else CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK && nx_selection_bind_owner(s->ctx,100,s->model,s->required)==NX_OK);
    d=desc(110,100,s->kind==1?NX_ROLE_CUSTOM:NX_ROLE_PANEL,"Layout");CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK);
    for(uint64_t i=0;i<67;++i) {
        char name[32];snprintf(name,sizeof(name),"Item %llu",(unsigned long long)i);
        d=desc(1000+i,110,s->kind==2 && i==1?NX_ROLE_TREE_ITEM:NX_ROLE_LIST_ITEM,name);
        if(i)d.bounds=(nx_rect){0,0,0,0};
        if(s->demoted) {
            d.actions=NX_ACTION_SELECT;if(s->single==d.id)d.state=NX_SEM_SELECTED;
            nx_widget_desc w={0};w.semantic=d;w.flags=NX_WIDGET_INTERACTIVE;nx_interaction_state state;
            CHECK(nx_widget_register(s->ctx,&w,&state)==NX_OK);
        } else CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK && nx_selection_bind_item(s->ctx,100,d.id,1)==NX_OK);
    }
    d=desc(200,1,NX_ROLE_TABLE,"Table");CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK && nx_selection_bind_owner(s->ctx,200,s->table,0)==NX_OK);
    for(uint64_t i=0;i<3;++i) {
        char name[32];snprintf(name,sizeof(name),"Table %llu",(unsigned long long)i);
        d=desc(2000+i,200,i==2?NX_ROLE_CUSTOM:NX_ROLE_ROW,name);d.bounds=(nx_rect){0,0,0,0};
        CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK && nx_selection_bind_item(s->ctx,200,d.id,1)==NX_OK);
    }
    d=desc(210,200,NX_ROLE_COLUMN_HEADER,"Sorting header");d.actions=NX_ACTION_SELECT;CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK);
    s->clicks=0;
    for(uint64_t i=0;i<65;++i) {
        char name[32];snprintf(name,sizeof(name),"Queued %llu",(unsigned long long)i);
        nx_control_desc c={0};c.id=5000+i;c.parent_id=1;c.name=(nx_utf8_span){name,strlen(name)};c.bounds=(nx_rect){200,20,100,20};
        nx_interaction_state state;CHECK(nx_button(s->ctx,&c,&state)==NX_OK);s->clicks+=state.clicked;s->slot_clicks+=state.clicked;
    }
    CHECK(nx_end_frame(s->ctx)==NX_OK);return 0;
}
static int publish(scene *s) {
    CHECK(frame(s)==0);s->last_publish=nx_window_publish_semantics(s->window,s->ctx);
    if(s->reconnecting && s->last_publish==NX_ERROR_UNSUPPORTED)return 0;
    if(s->last_publish==NX_OK)s->reconnecting=0;
    return s->last_publish!=NX_OK;
}
static int window(scene *s) {
    nx_window_desc d={"AT-SPI multiple selection",640,480,0,{&s->native,allocate,deallocate}};
    CHECK(nx_window_create(&d,&s->window)==NX_OK);return 0;
}
static int context(scene *s) {
    nx_context_desc d={0};d.allocator=(nx_allocator){&s->core,allocate,deallocate};CHECK(nx_context_create(&d,&s->ctx)==NX_OK);return 0;
}
static int catalog(scene *s) {
    nx_selection_item items[67];for(uint64_t i=0;i<67;++i)items[i]=(nx_selection_item){1000+i,(int)i==s->disabled?NX_SELECTION_ITEM_DISABLED:0};
    CHECK(nx_selection_model_set_items(s->model,items,67)==NX_OK && nx_selection_model_set_active(s->model,1066)==NX_OK);return 0;
}
static void hook(void *user) {
    scene *s=user;s->hook_fired=1;s->hook_result=NX_OK;
    if(s->hook_kind==1)s->hook_result=nx_window_publish_semantics(s->window,NULL);
    else if(s->hook_kind==2){nx_window *w=s->window;s->window=NULL;nx_window_destroy(w);}
    else if(s->hook_kind==3){s->kind=1;if(publish(s))s->hook_result=NX_ERROR_STATE;}
    else if(s->hook_kind==4)s->hook_result=nx_window_publish_semantics(s->window,s->ctx);
    else if(s->hook_kind==5) {
        puts("HOOK slots");s->hook_result=NX_ERROR_STATE;
        struct timespec start,now;clock_gettime(CLOCK_MONOTONIC,&start);
        char line[32];size_t used=0;
        do {
            if(nx_window_poll(s->window,0)!=NX_OK)break;
            ssize_t n=read(STDIN_FILENO,line+used,sizeof(line)-used-1);
            if(n>0){used+=(size_t)n;line[used]=0;if(strchr(line,'\n')){if(!strcmp(line,"finish\n"))s->hook_result=NX_OK;break;}}
            else if(n==0 || (errno!=EAGAIN && errno!=EINTR))break;
            struct timespec pause={0,1000000};nanosleep(&pause,NULL);clock_gettime(CLOCK_MONOTONIC,&now);
        } while(now.tv_sec-start.tv_sec<5 && used<sizeof(line)-1);
    }
}
static int control(scene *s,const char *cmd,int *quit) {
    if(!strcmp(cmd,"pause")){s->paused=1;return 0;}
    if(!strcmp(cmd,"resume")){s->paused=0;return 0;}
    if(!strcmp(cmd,"route")){size_t count=0;s->last_route=nx_window_route_events(s->window,s->ctx,&count);if(s->hook_kind==5)fprintf(stderr,"slot route result=%d routed=%zu last-clicks=%d\n",s->last_route,count,s->clicks);return 0;}
    if(!strcmp(cmd,"oom")){s->core.fail=1;return 0;}
    if(!strcmp(cmd,"check-hook")){CHECK(s->hook_fired && s->hook_result==(s->hook_kind==4?NX_ERROR_STATE:NX_OK));return 0;}
    if(!strcmp(cmd,"expect-memory")){CHECK(s->last_route==NX_ERROR_MEMORY);return 0;}
    if(!strcmp(cmd,"expect-forward")){CHECK(s->last_route==NX_OK);return 0;}
    if(!strcmp(cmd,"expect-single")){CHECK(s->single==1000);return 0;}
    if(!strcmp(cmd,"check-slots")){fprintf(stderr,"slot delivery actual=%d expected=63\n",s->slot_clicks);CHECK(s->slot_clicks==63);return 0;}
    if(!strcmp(cmd,"verify-table")) {
        nx_selection_view view;CHECK(nx_selection_model_get(s->table,&view)==NX_OK && view.selected_count==3 && view.active_id==0);
        for(size_t i=0;i<3;++i)CHECK(view.selected_ids[i]==2000+i);return 0;
    }
    if(!strcmp(cmd,"drop")){CHECK(nx_window_publish_semantics(s->window,NULL)==NX_OK);return 0;}
    if(!strncmp(cmd,"hook-",5) || !strncmp(cmd,"native-",7) || !strncmp(cmd,"bridge-",7) || !strncmp(cmd,"free-",5)) {
        const char *name=cmd+((!strncmp(cmd,"native-",7) || !strncmp(cmd,"bridge-",7))?7:5);
        s->hook_kind=!strcmp(name,"detach")?1:!strcmp(name,"destroy")?2:!strcmp(name,"ancestor")?3:!strcmp(name,"republish")?4:!strcmp(name,"slots")?5:0;
        CHECK(s->hook_kind);s->hook_fired=0;if(s->hook_kind==5)s->slot_clicks=0;
        heap *h=!strncmp(cmd,"hook-",5)?&s->core:&s->native;h->hook=hook;h->user=s;h->when=!strncmp(cmd,"free-",5)?2:1;h->skip=!strncmp(cmd,"bridge-",7);return 0;
    }
    if(!strcmp(cmd,"verify"))return 0;
    if(!strcmp(cmd,"quit")){*quit=1;return 0;}
    if(!strcmp(cmd,"reset")) {
        s->kind=s->required=s->demoted=s->owner_disabled=s->large=0;s->disabled=-1;s->single=1000;
        CHECK(catalog(s)==0 && nx_selection_model_set_selected(s->model,NULL,0)==NX_OK);
        if(!s->window)CHECK(window(s)==0);
    } else if(!strcmp(cmd,"ancestor"))s->kind=1;
    else if(!strcmp(cmd,"member-role"))s->kind=2;
    else if(!strcmp(cmd,"owner-role"))s->kind=3;
    else if(!strcmp(cmd,"demote")){s->demoted=1;s->single=1000;}
    else if(!strcmp(cmd,"detach"))CHECK(nx_window_publish_semantics(s->window,NULL)==NX_OK);
    else if(!strcmp(cmd,"context")){nx_context_destroy(s->ctx);s->ctx=NULL;CHECK(context(s)==0);}
    else if(!strcmp(cmd,"revision"))CHECK(nx_selection_model_set_selected(s->model,NULL,0)==NX_OK);
    else if(!strcmp(cmd,"target")) {
        nx_selection_model_destroy(s->model);s->model=NULL;CHECK(nx_selection_model_create(67,NULL,&s->model)==NX_OK && catalog(s)==0);
    } else if(!strcmp(cmd,"disabled-cycle")) {
        s->owner_disabled=1;CHECK(publish(s)==0);s->owner_disabled=0;
    } else if(!strcmp(cmd,"disconnect")) {
        CHECK(nx_window_linux_ax_test_disconnect(s->window)==NX_OK);
        /* Observe the transport loss before beginning a frame: only the
           provider's disconnect dispatch revokes an already forwarded lease. */
        CHECK(nx_window_poll(s->window,0)==NX_OK);s->reconnecting=1;
    }
    else if(!strcmp(cmd,"required")){uint64_t one=1000;s->required=1;CHECK(nx_selection_model_set_selected(s->model,&one,1)==NX_OK);}
    else if(!strcmp(cmd,"disabled")){s->disabled=2;CHECK(catalog(s)==0);}
    else if(!strcmp(cmd,"disabled-selected")){uint64_t one=1002;s->disabled=2;CHECK(catalog(s)==0 && nx_selection_model_set_selected(s->model,&one,1)==NX_OK);}
    else if(!strcmp(cmd,"fresh")) {
        CHECK(nx_window_publish_semantics(s->window,NULL)==NX_OK);s->large=0;CHECK(publish(s)==0);
    } else if(!strcmp(cmd,"grow")) {
        s->large=1;CHECK(frame(s)==0);s->last_publish=nx_window_publish_semantics(s->window,s->ctx);
        CHECK(s->last_publish==NX_ERROR_STATE && s->hook_fired);return 0;
    } else CHECK(!strcmp(cmd,"step"));
    CHECK(publish(s)==0);return 0;
}
int main(void) {
    scene s={0};s.disabled=-1;s.single=1000;
    CHECK(context(&s)==0 && window(&s)==0 && nx_selection_model_create(67,NULL,&s.model)==NX_OK && nx_selection_model_create(3,NULL,&s.table)==NX_OK);
    nx_selection_item table[]={{2000,0},{2001,0},{2002,0}};CHECK(nx_selection_model_set_items(s.table,table,3)==NX_OK && catalog(&s)==0);
    CHECK(publish(&s)==0);setvbuf(stdout,NULL,_IOLBF,0);printf("READY %ld\n",(long)getpid());
    CHECK(fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL)|O_NONBLOCK)==0);
    char buffer[256];size_t used=0;int quit=0;
    while(!quit) {
        if(s.window)CHECK(nx_window_poll(s.window,5)==NX_OK);
        /* Pausing model routing must not stop the publication retry that
           re-establishes the optional accessibility transport after disconnect. */
        if(s.window && s.reconnecting)CHECK(publish(&s)==0);
        if(s.window && !s.paused) {
            size_t routed=0;CHECK(nx_window_route_events(s.window,s.ctx,&routed)==NX_OK);
            if(s.window)CHECK(publish(&s)==0);
        }
        ssize_t n=read(STDIN_FILENO,buffer+used,sizeof(buffer)-used-1);if(!n)break;
        CHECK(n>=0 || errno==EAGAIN || errno==EINTR);if(n>0)used+=(size_t)n;
        buffer[used]=0;char *end=memchr(buffer,'\n',used);
        if(end) {
            *end=0;CHECK(control(&s,buffer,&quit)==0);
            nx_selection_view view;CHECK(nx_selection_model_get(s.model,&view)==NX_OK);
            printf("ACK %s %zu %llu %d %d %d %d %llu\n",buffer,view.selected_count,(unsigned long long)view.active_id,
                s.hook_fired,s.hook_result,s.last_route,s.last_publish,(unsigned long long)s.single);
            size_t consumed=(size_t)(end-buffer)+1;used-=consumed;memmove(buffer,buffer+consumed,used);
        }
        CHECK(used<sizeof(buffer)-1);
    }
    if(s.window){CHECK(nx_window_publish_semantics(s.window,NULL)==NX_OK);nx_window_destroy(s.window);}
    nx_context_destroy(s.ctx);nx_selection_model_destroy(s.model);nx_selection_model_destroy(s.table);
    CHECK(!s.core.count && !s.native.count);return 0;
}
