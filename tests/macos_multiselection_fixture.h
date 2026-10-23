#ifndef NEXAUI_TEST_MACOS_MULTISELECTION_FIXTURE_H
#define NEXAUI_TEST_MACOS_MULTISELECTION_FIXTURE_H
#import <AppKit/AppKit.h>
#include "nexaui/nexaui.h"
#include "window_internal.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <objc/runtime.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
typedef struct allocation { void *p; size_t bytes; } allocation;
typedef struct heap {
    allocation live[4096]; size_t count,calls; int fail,when;
    void (*hook)(void *); void *user;
} heap;
static void call(heap *h,int when) {
    if(h->hook && h->when==when) {void (*f)(void *)=h->hook;h->hook=NULL;f(h->user);}
}
static void *allocate(void *user,size_t bytes) {
    heap *h=user;++h->calls;call(h,1);if(h->fail){h->fail=0;return NULL;}
    void *p=malloc(bytes);if(p){if(h->count==4096)abort();h->live[h->count++]=(allocation){p,bytes};}return p;
}
static void deallocate(void *user,void *p,size_t bytes) {
    heap *h=user;size_t at=0;while(at<h->count && h->live[at].p!=p)++at;
    if(at==h->count || h->live[at].bytes!=bytes)abort();
    h->live[at]=h->live[--h->count];call(h,2);free(p);
}
typedef struct scene {
    nx_context *ctx;nx_window *window;nx_selection_model *model,*table;
    heap core,native;int required,disabled,kind,large,moved,clicks,native_attempt,demoted;
    size_t big,padding;uint64_t legacy,other,demoted_choice;
    nx_result callback_result;
} scene;
static nx_semantic_desc desc(uint64_t id,uint64_t parent,nx_semantic_role role) {
    nx_semantic_desc d={0};d.id=id;d.parent_id=parent;d.role=role;
    d.name=(nx_utf8_span){"Choice",6};d.bounds=(nx_rect){10,10,100,20};return d;
}
static int frame(scene *s) {
    CHECK(nx_begin_frame(s->ctx,640,480,0)==NX_OK);
    nx_semantic_desc d=desc(1,0,NX_ROLE_WINDOW);CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK);
    d=desc(100,1,s->kind==4?NX_ROLE_TREE:NX_ROLE_LIST);if(s->disabled)d.state=NX_SEM_DISABLED;
    char long_name[65536];if(s->large){memset(long_name,'X',sizeof(long_name));d.name=(nx_utf8_span){long_name,sizeof(long_name)};}
    if(s->demoted)CHECK(nx_selection_group(s->ctx,&d,0,&s->demoted_choice)==NX_OK);
    else CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK && nx_selection_bind_owner(s->ctx,100,s->model,s->required)==NX_OK);
    d=desc(110,100,s->kind==1?NX_ROLE_CUSTOM:NX_ROLE_PANEL);CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK);
    d=desc(111,100,NX_ROLE_PANEL);CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK);
    d=desc(120,s->kind==2?111:110,NX_ROLE_PANEL);CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK);
    for(size_t i=0;i<s->padding;++i) {
        d=desc(20000+i,1,NX_ROLE_TEXT);CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK);
    }
    for(uint64_t i=0;i<(s->big?s->big:67);++i) {
        d=desc(1000+i,120,s->kind==3 && i==1?NX_ROLE_TREE_ITEM:NX_ROLE_LIST_ITEM);
        if(i && !s->demoted)d.bounds=(nx_rect){0,0,0,0};else d.actions=NX_ACTION_FOCUS;
        if(s->demoted) {
            d.actions|=NX_ACTION_SELECT;if(d.id==s->demoted_choice)d.state=NX_SEM_SELECTED;
            nx_widget_desc widget={0};widget.semantic=d;widget.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE;
            nx_interaction_state state;CHECK(nx_widget_register(s->ctx,&widget,&state)==NX_OK);continue;
        }
        CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK);
        if(!i){nx_interaction_state state;CHECK(nx_interact_region(s->ctx,d.id,d.bounds,NX_REGION_FOCUSABLE,&state)==NX_OK);}
        CHECK(nx_selection_bind_item(s->ctx,100,d.id,i!=0)==NX_OK);
    }
    if(s->big){CHECK(nx_end_frame(s->ctx)==NX_OK);return 0;}
    d=desc(200,1,NX_ROLE_TABLE);CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK && nx_selection_bind_owner(s->ctx,200,s->table,0)==NX_OK);
    for(uint64_t i=0;i<3;++i) {
        d=desc(2000+i,200,i==2?NX_ROLE_CUSTOM:NX_ROLE_ROW);d.bounds=(nx_rect){0,0,0,0};
        CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK && nx_selection_bind_item(s->ctx,200,d.id,1)==NX_OK);
    }
    d=desc(210,200,NX_ROLE_COLUMN_HEADER);d.actions=NX_ACTION_SELECT;CHECK(nx_semantics_add(s->ctx,&d,NULL)==NX_OK);
    for(unsigned g=0;g<2;++g) {
        uint64_t owner=g?400:300;d=desc(owner,1,NX_ROLE_LIST);
        CHECK(nx_selection_group(s->ctx,&d,0,g?&s->other:&s->legacy)==NX_OK);
        for(uint64_t i=0;i<2;++i) {
            if((!g && s->moved && !i) || (g && (!s->moved || i)))continue;
            d=desc(301+i,owner,NX_ROLE_LIST_ITEM);d.actions=NX_ACTION_SELECT;
            if((g?s->other:s->legacy)==d.id)d.state=NX_SEM_SELECTED;
            nx_widget_desc w={0};w.semantic=d;w.flags=NX_WIDGET_INTERACTIVE;
            nx_interaction_state state;CHECK(nx_widget_register(s->ctx,&w,&state)==NX_OK);
        }
    }
    s->clicks=0;
    for(uint64_t i=0;i<65;++i) {
        nx_control_desc c={0};c.id=5000+i;c.parent_id=1;c.name=(nx_utf8_span){"Queued",6};c.bounds=(nx_rect){200,20,100,20};
        nx_interaction_state state;CHECK(nx_button(s->ctx,&c,&state)==NX_OK);s->clicks+=state.clicked;
    }
    CHECK(nx_end_frame(s->ctx)==NX_OK);return 0;
}
static id find(id root,uint64_t value) {
    NSString *wanted=[NSString stringWithFormat:@"nexa-%016llx",(unsigned long long)value];
    if([[root accessibilityIdentifier] isEqual:wanted])return root;
    for(id child in [root accessibilityChildren]){id result=find(child,value);if(result)return result;}return nil;
}
static id element(scene *s,uint64_t value) {return find(nx_window_native_nsview(s->window),value);}
static int selected(nx_selection_model *m,const uint64_t *ids,size_t count,uint64_t active) {
    nx_selection_view v;CHECK(nx_selection_model_get(m,&v)==NX_OK);
    CHECK(v.selected_count==count && v.active_id==active);
    CHECK(!count || !memcmp(v.selected_ids,ids,count*sizeof(*ids)));return 0;
}
static int publish(scene *s) {
    CHECK(frame(s)==0);nx_result r=nx_window_publish_semantics(s->window,s->ctx);
    if(r!=NX_OK)fprintf(stderr,"publish result=%d\n",r);
    CHECK(r==NX_OK);return 0;
}
static int advance(scene *s,size_t expected) {
    size_t routed=999;nx_result r=nx_window_route_events(s->window,s->ctx,&routed);
    if(r!=NX_OK || routed!=expected)fprintf(stderr,"route result=%d actual=%zu expected=%zu\n",r,routed,expected);
    CHECK(r==NX_OK && routed==expected);
    CHECK(publish(s)==0 && ![nx_window_native_nswindow(s->window) isVisible]);return 0;
}
static int setup(scene *s) {
    nx_context_desc cd={0};cd.allocator=(nx_allocator){&s->core,allocate,deallocate};
    CHECK(nx_context_create(&cd,&s->ctx)==NX_OK);
    size_t count=s->big?s->big:67;
    CHECK(nx_selection_model_create(count,NULL,&s->model)==NX_OK && nx_selection_model_create(3,NULL,&s->table)==NX_OK);
    nx_selection_item *items=calloc(count,sizeof(*items));CHECK(items);
    for(uint64_t i=0;i<count;++i)items[i]=(nx_selection_item){1000+i,0};
    nx_result ir=nx_selection_model_set_items(s->model,items,count);free(items);CHECK(ir==NX_OK);
    nx_selection_item rows[]={{2000,0},{2001,0},{2002,0}};
    CHECK(nx_selection_model_set_items(s->table,rows,3)==NX_OK && nx_selection_model_set_active(s->model,1000+count-1)==NX_OK);
    nx_window_desc wd={"Hidden multi-selection contract",640,480,NX_WINDOW_HIDDEN,{&s->native,allocate,deallocate}};
    CHECK(nx_window_create(&wd,&s->window)==NX_OK && publish(s)==0);
    CHECK(![nx_window_native_nswindow(s->window) isVisible]);
    /* Creation queues a real resize event. Drain it before counting native AX
       admissions; route_events reports both ordinary input and AX deliveries. */
    size_t initial=0;CHECK(nx_window_route_events(s->window,s->ctx,&initial)==NX_OK);return 0;
}
static int cleanup(scene *s) {
    if(s->window){CHECK(![nx_window_native_nswindow(s->window) isVisible]);nx_window_destroy(s->window);s->window=NULL;}
    nx_context_destroy(s->ctx);nx_selection_model_destroy(s->model);nx_selection_model_destroy(s->table);
    CHECK(!s->native.count && !s->core.count);return 0;
}

#endif
