#include "selection_model.h"
#include "context_identity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s failed\n",__FILE__,__LINE__,#x); return 1; } } while (0)

typedef struct scenario { int required,owner_disabled,disabled_index,skip_index,ancestor_role,move_ancestor,modal; } scenario;
static scenario normal(void) { scenario s={0,0,-1,-1,0,0,0};return s; }
static nx_semantic_desc desc(uint64_t id,uint64_t parent,nx_semantic_role role) {
    nx_semantic_desc d={0};d.id=id;d.parent_id=parent;d.role=role;
    d.name=(nx_utf8_span){"Set",3};return d;
}
static int add(nx_context *ctx,uint64_t id,uint64_t parent,nx_semantic_role role,int disabled) {
    nx_semantic_desc d=desc(id,parent,role);if(disabled)d.state=NX_SEM_DISABLED;
    CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK);return 0;
}
static int make(nx_selection_model **m,size_t count,uint64_t first) {
    nx_selection_item *items=count?malloc(count*sizeof(*items)):NULL;CHECK(!count || items);
    for(size_t i=0;i<count;++i)items[i]=(nx_selection_item){first+i,0};
    CHECK(nx_selection_model_create(count?count:1,NULL,m)==NX_OK);
    CHECK(nx_selection_model_set_items(*m,items,count)==NX_OK);free(items);return 0;
}
static int group(nx_context *ctx,nx_selection_model *m,uint64_t owner,uint64_t first,size_t count,scenario s) {
    CHECK(add(ctx,owner,10,NX_ROLE_LIST,s.owner_disabled)==0);
    CHECK(nx_selection_bind_owner(ctx,owner,m,s.required)==NX_OK);
    CHECK(add(ctx,owner+10,owner,s.ancestor_role?NX_ROLE_CUSTOM:NX_ROLE_PANEL,0)==0);
    CHECK(add(ctx,owner+11,owner,NX_ROLE_PANEL,0)==0);
    CHECK(add(ctx,owner+20,owner+(s.move_ancestor?11:10),NX_ROLE_PANEL,0)==0);
    for(size_t i=0;i<count;++i) {
        if((int)i==s.skip_index)continue;
        CHECK(add(ctx,first+i,owner+20,NX_ROLE_LIST_ITEM,(int)i==s.disabled_index)==0);
        CHECK(nx_selection_bind_item(ctx,owner,first+i,1)==NX_OK);
    }
    return 0;
}
static int begin(nx_context *ctx,nx_selection_model *a,nx_selection_model *b,size_t count,scenario s) {
    CHECK(nx_begin_frame(ctx,300,200,0)==NX_OK && add(ctx,10,0,NX_ROLE_PANEL,0)==0);
    CHECK(group(ctx,a,100,1000,count,s)==0);
    if(b)CHECK(group(ctx,b,200,2000000,3,normal())==0);
    if(s.modal) {
        nx_overlay_desc o={0};o.panel.id=900;o.backdrop_id=901;o.panel.bounds=(nx_rect){0,0,100,100};
        o.panel.name=(nx_utf8_span){"Modal",5};int dismissed=0;
        CHECK(nx_modal_begin(ctx,&o,&dismissed)==NX_OK && nx_overlay_end(ctx)==NX_OK);
    }
    return 0;
}
static int frame(nx_context *ctx,nx_selection_model *a,nx_selection_model *b,size_t count,scenario s) {
    CHECK(begin(ctx,a,b,count,s)==0 && nx_end_frame(ctx)==NX_OK);return 0;
}
static int selection(nx_selection_model *m,const uint64_t *ids,size_t count) {
    nx_selection_view v;CHECK(nx_selection_model_get(m,&v)==NX_OK && v.selected_count==count);
    CHECK(!count || !memcmp(v.selected_ids,ids,count*sizeof(*ids)));
    for(size_t i=0;i<v.item_count;++i) {
        int expected=0,actual=0,disabled=0;
        for(size_t j=0;j<count;++j)if(v.items[i].id==ids[j])expected=1;
        CHECK(nx_selection_model_item(m,v.items[i].id,0,&actual,&disabled)==NX_OK && actual==expected);
    }
    return 0;
}
typedef struct lease { uint64_t context,owner,token;nx_selection_target target;int live,refs,checks,failed; } lease;
static void retain(void *p) { lease *l=p;++l->refs; }
static void release(void *p) { lease *l=p;--l->refs;if(l->refs<0)l->failed=1; }
static int current(void *p,uint64_t token,uint64_t context,uint64_t id,uint64_t owner,nx_selection_target target) {
    lease *l=p;++l->checks;
    if(l->refs<1 || context!=l->context || id!=l->owner || owner!=l->owner ||
       target.identity!=l->target.identity || target.revision!=l->target.revision)l->failed=1;
    return !l->failed && l->live && token==l->token;
}
static nx_selection_guard guard(lease *l,nx_context *ctx,nx_selection_model *m) {
    memset(l,0,sizeof(*l));l->context=nx_context_identity(ctx);l->owner=100;
    l->target=nx_selection_model_target(m);l->token=9;l->live=1;
    nx_selection_guard g={l,9,retain,release,current};return g;
}
typedef struct tracker {
    size_t allocations,releases,live;int fail,allocate_action,release_action,callback_failed;
    nx_context *ctx;nx_selection_model *model;lease *lease;nx_result callback_result;
} tracker;
static void callback(tracker *t,int action) {
    if(!action)return;
    if(action==1) {
        if(!t->lease || t->lease->refs<1)t->callback_failed=1;
        if(t->lease)t->lease->live=0;
    } else if(action==2) {
        uint64_t id=1002;t->callback_result=nx_semantics_request_selection_set_guarded(t->ctx,100,&id,1,NULL);
    } else if(action==3) {
        if(frame(t->ctx,t->model,NULL,3,normal()))t->callback_failed=1;
    } else if(action==4) {
        uint64_t id=1000;t->callback_result=nx_selection_model_set_selected(t->model,&id,1);
    } else if(action==5) {
        t->callback_result=nx_semantics_request_selection_set_guarded(t->ctx,400,NULL,0,NULL);
    } else if(action==6) {
        t->callback_result=nx_semantics_request_action(t->ctx,1000,NX_ACTION_ADD_SELECTION);
    }
}
static void *allocate(void *p,size_t bytes) {
    tracker *t=p;++t->allocations;if(t->fail)return NULL;
    void *result=malloc(bytes);if(result)++t->live;
    int action=t->allocate_action;t->allocate_action=0;callback(t,action);return result;
}
static void deallocate(void *p,void *memory,size_t bytes) {
    tracker *t=p;(void)bytes;++t->releases;
    int action=t->release_action;t->release_action=0;callback(t,action);
    if(memory){--t->live;free(memory);}
}
static int context(tracker *t,nx_context **ctx) {
    nx_context_desc cd={0};cd.allocator=(nx_allocator){t,allocate,deallocate};
    CHECK(nx_context_create(&cd,ctx)==NX_OK);t->ctx=*ctx;return 0;
}
static int operations_and_atomicity(void) {
    nx_context *ctx=NULL;nx_selection_model *a=NULL,*b=NULL;tracker t={0};
    CHECK(context(&t,&ctx)==0 && make(&a,65,1000)==0 && make(&b,3,2000000)==0);
    CHECK(nx_selection_model_set_active(a,1007)==NX_OK && frame(ctx,a,b,65,normal())==0);
    uint64_t ids[65];for(size_t i=0;i<65;++i)ids[i]=1064-i;
    lease l;nx_selection_guard g=guard(&l,ctx,a);
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,ids,65,&g)==NX_OK && l.refs==1 && l.checks==1);
    memset(ids,0,sizeof(ids));CHECK(selection(a,NULL,0)==0);
    CHECK(nx_semantics_request_action(ctx,1000,NX_ACTION_SELECT)==NX_ERROR_LIMIT);
    uint64_t peer=2000001;CHECK(nx_semantics_request_action(ctx,peer,NX_ACTION_SELECT)==NX_OK);
    CHECK(frame(ctx,a,b,65,normal())==0 && l.refs==0 && l.checks==2 && !l.failed);
    nx_selection_view v;CHECK(nx_selection_model_get(a,&v)==NX_OK && v.selected_count==65 && v.active_id==1007);
    for(size_t i=0;i<65;++i)CHECK(v.selected_ids[i]==1000+i);
    CHECK(selection(b,&peer,1)==0);
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,NULL,0,NULL)==NX_OK);
    CHECK(frame(ctx,a,b,65,normal())==0 && selection(a,NULL,0)==0);
    uint64_t wanted[]={1000,1001},both_b[]={2000001,2000002};
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,wanted,2,NULL)==NX_OK);
    CHECK(nx_semantics_request_selection_set_guarded(ctx,200,both_b,2,NULL)==NX_OK);
    CHECK(begin(ctx,a,NULL,65,normal())==0);
    scenario omit=normal();omit.skip_index=1;CHECK(group(ctx,b,200,2000000,3,omit)==0);
    CHECK(nx_end_frame(ctx)==NX_ERROR_STATE && selection(a,NULL,0)==0 && selection(b,&peer,1)==0);
    CHECK(add(ctx,peer,220,NX_ROLE_LIST_ITEM,0)==0 && nx_selection_bind_item(ctx,200,peer,1)==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK && selection(a,wanted,2)==0 && selection(b,both_b,2)==0);
    uint64_t duplicate[]={1001,1001},bad[]={1000,9999999},zero=0;
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,duplicate,2,NULL)==NX_ERROR_ARGUMENT);
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,bad,2,NULL)==NX_ERROR_STATE);
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,&zero,1,NULL)==NX_ERROR_ARGUMENT);
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,NULL,1,NULL)==NX_ERROR_ARGUMENT);
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,wanted,NX_SELECTION_MAX_ITEMS+1u,NULL)==NX_ERROR_LIMIT);
    CHECK(nx_semantics_request_selection_set_guarded(ctx,10,NULL,0,NULL)==NX_ERROR_STATE);
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,wanted,2,NULL)==NX_OK);
    t.release_action=2;CHECK(frame(ctx,a,b,65,normal())==0 && t.callback_result==NX_ERROR_LIMIT);
    for(size_t i=0;i<63;++i)CHECK(nx_semantics_request_action(ctx,1000,NX_ACTION_ADD_SELECTION)==NX_OK);
    size_t calls=t.allocations;t.allocate_action=6;
    nx_result outer=nx_semantics_request_selection_set_guarded(ctx,200,NULL,0,NULL);
    if(outer!=NX_OK || t.callback_result!=NX_ERROR_LIMIT || t.allocations!=calls+1)
        fprintf(stderr,"in-flight slot: outer=%d nested=%d allocations=%zu\n",outer,t.callback_result,t.allocations-calls);
    CHECK(outer==NX_OK && t.callback_result==NX_ERROR_LIMIT && t.allocations==calls+1);
    CHECK(frame(ctx,a,b,65,normal())==0 && selection(b,NULL,0)==0);
    CHECK(!t.callback_failed);nx_context_destroy(ctx);nx_selection_model_destroy(a);nx_selection_model_destroy(b);
    CHECK(!t.live);return 0;
}
static int lifecycle_and_callouts(void) {
    nx_context *ctx=NULL;nx_selection_model *a=NULL,*replacement=NULL;tracker t={0};
    CHECK(context(&t,&ctx)==0 && make(&a,3,1000)==0 && make(&replacement,3,1000)==0);t.model=a;
    uint64_t old=1000,wanted=1001;CHECK(nx_selection_model_set_selected(a,&old,1)==NX_OK);
    CHECK(frame(ctx,a,NULL,3,normal())==0);
    for(int kind=0;kind<7;++kind) {
        CHECK(nx_semantics_request_selection_set_guarded(ctx,100,&wanted,1,NULL)==NX_OK);
        scenario s=normal();
        if(kind==0)s.ancestor_role=1;
        if(kind==1)s.move_ancestor=1;
        if(kind==2)s.owner_disabled=1;
        if(kind==3)s.disabled_index=1;
        if(kind==4)s.required=1;
        if(kind==5)s.modal=1;
        if(kind==6)s.skip_index=1;
        CHECK(frame(ctx,a,NULL,3,s)==0 && selection(a,&old,1)==0);
        CHECK(frame(ctx,a,NULL,3,normal())==0);
    }
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,&wanted,1,NULL)==NX_OK);
    CHECK(nx_selection_model_set_selected(a,&old,1)==NX_OK); /* Equal replacement revokes old target. */
    CHECK(frame(ctx,a,NULL,3,normal())==0 && selection(a,&old,1)==0);
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,&wanted,1,NULL)==NX_OK);
    CHECK(nx_selection_model_set_selected(replacement,&old,1)==NX_OK);
    CHECK(frame(ctx,replacement,NULL,3,normal())==0 && selection(replacement,&old,1)==0);
    CHECK(frame(ctx,a,NULL,3,normal())==0);
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,&wanted,1,NULL)==NX_OK);
    nx_selection_item reduced[]={{1000,0},{1002,0}},restored[]={{1000,0},{1001,0},{1002,0}};
    CHECK(nx_selection_model_set_items(a,reduced,2)==NX_OK && nx_selection_model_set_items(a,restored,3)==NX_OK);
    CHECK(frame(ctx,a,NULL,3,normal())==0 && selection(a,&old,1)==0);
    lease l;nx_selection_guard g=guard(&l,ctx,a);t.lease=&l;t.allocate_action=1;
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,&wanted,1,&g)==NX_ERROR_STATE && !l.refs && l.checks==1);
    CHECK(selection(a,&old,1)==0 && !t.callback_failed && !l.failed);
    g=guard(&l,ctx,a);t.fail=1;
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,&wanted,1,&g)==NX_ERROR_MEMORY && !l.refs);
    t.fail=0;t.allocate_action=2;
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,&wanted,1,NULL)==NX_ERROR_LIMIT && t.callback_result==NX_OK);
    uint64_t nested=1002;CHECK(frame(ctx,a,NULL,3,normal())==0 && selection(a,&nested,1)==0);
    t.allocate_action=3;
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,&wanted,1,NULL)==NX_ERROR_STATE && !t.callback_failed);
    CHECK(selection(a,&nested,1)==0);
    t.allocate_action=4;
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,&wanted,1,NULL)==NX_OK && t.callback_result==NX_OK);
    CHECK(frame(ctx,a,NULL,3,normal())==0 && selection(a,&old,1)==0);
    g=guard(&l,ctx,a);CHECK(nx_semantics_request_selection_set_guarded(ctx,100,&wanted,1,&g)==NX_OK);
    CHECK(begin(ctx,a,NULL,3,normal())==0);
    char large[16384];memset(large,'A',sizeof(large));nx_semantic_desc d=desc(800,0,NX_ROLE_TEXT);d.name=(nx_utf8_span){large,sizeof(large)};
    t.allocate_action=1;CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK && !l.live);
    CHECK(nx_end_frame(ctx)==NX_OK && selection(a,&old,1)==0 && !l.refs && !l.failed);
    /* Required/disabled policy at admission and preservation of disabled old selection. */
    scenario s=normal();s.required=1;CHECK(frame(ctx,a,NULL,3,s)==0);
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,NULL,0,NULL)==NX_ERROR_STATE);
    s=normal();s.disabled_index=0;CHECK(frame(ctx,a,NULL,3,s)==0);
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,&wanted,1,NULL)==NX_ERROR_STATE);
    uint64_t keep[]={1000,1001};CHECK(nx_semantics_request_selection_set_guarded(ctx,100,keep,2,NULL)==NX_OK);
    CHECK(frame(ctx,a,NULL,3,s)==0 && selection(a,keep,2)==0);
    CHECK(!t.callback_failed);nx_context_destroy(ctx);nx_selection_model_destroy(a);nx_selection_model_destroy(replacement);
    CHECK(!t.live);return 0;
}
static int maximum_and_budget(void) {
    nx_context *ctx=NULL;tracker t={0};nx_selection_model *a=NULL,*empty[3]={0};
    CHECK(context(&t,&ctx)==0 && make(&a,NX_SELECTION_MAX_ITEMS,1000)==0);
    for(size_t i=0;i<3;++i)CHECK(make(&empty[i],0,0)==0);
    CHECK(begin(ctx,a,NULL,NX_SELECTION_MAX_ITEMS,normal())==0);
    for(size_t i=0;i<3;++i)CHECK(group(ctx,empty[i],200+100*i,0,0,normal())==0);
    CHECK(nx_end_frame(ctx)==NX_OK);
    uint64_t *ids=malloc(NX_SELECTION_MAX_ITEMS*sizeof(*ids));CHECK(ids);
    for(size_t i=0;i<NX_SELECTION_MAX_ITEMS;++i)ids[i]=1000+i;
    CHECK(nx_semantics_request_selection_set_guarded(ctx,100,ids,NX_SELECTION_MAX_ITEMS,NULL)==NX_OK);
    CHECK(nx_semantics_request_selection_set_guarded(ctx,200,NULL,0,NULL)==NX_OK);
    size_t before=t.allocations;t.allocate_action=5;
    nx_result outer=nx_semantics_request_selection_set_guarded(ctx,300,NULL,0,NULL);
    if(outer!=NX_OK || t.callback_result!=NX_ERROR_LIMIT || t.allocations!=before+1)
        fprintf(stderr,"in-flight bytes: outer=%d nested=%d allocations=%zu\n",outer,t.callback_result,t.allocations-before);
    CHECK(outer==NX_OK && t.callback_result==NX_ERROR_LIMIT && t.allocations==before+1);
    size_t calls=t.allocations;
    CHECK(nx_semantics_request_selection_set_guarded(ctx,400,NULL,0,NULL)==NX_ERROR_LIMIT && t.allocations==calls);
    CHECK(begin(ctx,a,NULL,NX_SELECTION_MAX_ITEMS,normal())==0);
    for(size_t i=0;i<3;++i)CHECK(group(ctx,empty[i],200+100*i,0,0,normal())==0);
    CHECK(nx_end_frame(ctx)==NX_OK);
    nx_selection_view v;CHECK(nx_selection_model_get(a,&v)==NX_OK && v.selected_count==NX_SELECTION_MAX_ITEMS);
    CHECK(!memcmp(v.selected_ids,ids,NX_SELECTION_MAX_ITEMS*sizeof(*ids)));
    CHECK(nx_semantics_request_selection_set_guarded(ctx,400,NULL,0,NULL)==NX_OK); /* Budget released. */
    nx_context_destroy(ctx);nx_selection_model_destroy(a);for(size_t i=0;i<3;++i)nx_selection_model_destroy(empty[i]);
    free(ids);CHECK(!t.live);return 0;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"budget"))return maximum_and_budget();
    if(argc==2 && !strcmp(argv[1],"slots"))return operations_and_atomicity();
    if(operations_and_atomicity() || lifecycle_and_callouts() || maximum_and_budget())return 1;
    puts("complete context selection sets: atomic owners, 65/65536, scalar ancestry/lease, OOM/reentry, disabled/required and byte bounds passed");return 0;
}
