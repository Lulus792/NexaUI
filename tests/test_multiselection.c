#include "nexaui/nexaui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s failed\n",__FILE__,__LINE__,#x); return 1; } } while (0)

static nx_semantic_desc desc(uint64_t id,uint64_t parent,nx_semantic_role role) {
    nx_semantic_desc d={0}; d.id=id;d.parent_id=parent;d.role=role;
    d.name=(nx_utf8_span){"Selection",9};d.bounds=(nx_rect){0,0,100,20};return d;
}
static int model(nx_selection_model **out,uint64_t first) {
    nx_selection_item items[]={{first,0},{first+1,0},{first+2,0}};
    CHECK(nx_selection_model_create(3,NULL,out)==NX_OK);
    CHECK(nx_selection_model_set_items(*out,items,3)==NX_OK);return 0;
}
static int selected(nx_selection_model *m,const uint64_t *ids,size_t count,uint64_t active) {
    nx_selection_view v;CHECK(nx_selection_model_get(m,&v)==NX_OK);
    CHECK(v.selected_count==count && v.active_id==active);
    CHECK(!count || !memcmp(v.selected_ids,ids,count*sizeof(*ids)));return 0;
}
static int owner(nx_context *ctx,nx_selection_model *m,uint64_t id,uint64_t parent,int required,int disabled) {
    nx_semantic_desc d=desc(id,parent,NX_ROLE_LIST);d.state=disabled?NX_SEM_DISABLED:0;
    CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK);
    CHECK(nx_selection_bind_owner(ctx,id,m,required)==NX_OK);return 0;
}
static int item(nx_context *ctx,uint64_t group,uint64_t id,uint64_t parent,int offscreen) {
    nx_semantic_desc d=desc(id,parent,NX_ROLE_LIST_ITEM);
    if(offscreen)d.bounds=(nx_rect){0,0,0,0};
    else d.actions=NX_ACTION_FOCUS;
    CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK);
    if(!offscreen) { nx_interaction_state state;
        CHECK(nx_interact_region(ctx,id,d.bounds,NX_REGION_FOCUSABLE,&state)==NX_OK);
    }
    CHECK(nx_selection_bind_item(ctx,group,id,offscreen)==NX_OK);return 0;
}
static int build(nx_context *ctx,nx_selection_model *m,int required,int disabled,
                 uint64_t parent,int modal) {
    CHECK(nx_begin_frame(ctx,240,160,0)==NX_OK);
    nx_overlay_desc overlay={0};overlay.panel.id=500;overlay.backdrop_id=501;
    overlay.panel.bounds=(nx_rect){20,20,180,100};overlay.panel.name=(nx_utf8_span){"Dialog",6};
    int dismissed=0;
    if(modal==2)CHECK(nx_modal_begin(ctx,&overlay,&dismissed)==NX_OK);
    CHECK(owner(ctx,m,100,modal==2?500:0,required,disabled==1)==0);
    nx_semantic_desc panel=desc(parent,100,NX_ROLE_PANEL);
    if(disabled==2)panel.state=NX_SEM_DISABLED;
    CHECK(nx_semantics_add(ctx,&panel,NULL)==NX_OK);
    for(uint64_t id=101;id<=103;++id)CHECK(item(ctx,100,id,parent,id!=101)==0);
    if(modal==2)CHECK(nx_overlay_end(ctx)==NX_OK);
    if(modal==1) {
        CHECK(nx_modal_begin(ctx,&overlay,&dismissed)==NX_OK);
        CHECK(nx_overlay_end(ctx)==NX_OK);
    }
    return 0;
}
static int frame(nx_context *ctx,nx_selection_model *m,int required,int disabled,uint64_t parent,int modal) {
    CHECK(build(ctx,m,required,disabled,parent,modal)==0);
    CHECK(nx_end_frame(ctx)==NX_OK);return 0;
}
static int view(nx_context *ctx,uint64_t id,nx_semantic_view *v) {
    uint32_t at;CHECK(nx_semantics_find(ctx,id,&at)==NX_OK);
    CHECK(nx_semantics_get(ctx,at,v)==NX_OK);return 0;
}
static int operations(void) {
    nx_context *ctx=NULL;nx_selection_model *m=NULL;
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && model(&m,101)==0);
    const uint64_t two[]={101,102},one[]={101},other[]={102};
    CHECK(nx_selection_model_set_selected(m,two,2)==NX_OK);
    CHECK(nx_selection_model_set_active(m,103)==NX_OK);
    CHECK(build(ctx,m,0,0,110,0)==0);
    CHECK(nx_focus_set(ctx,101)==NX_OK && nx_end_frame(ctx)==NX_OK);
    nx_semantic_view v;CHECK(view(ctx,100,&v)==0 && (v.state&NX_SEM_SELECTION_MULTIPLE) && !(v.state&NX_SEM_SELECTION_SINGLE));
    CHECK(view(ctx,102,&v)==0 && (v.state&NX_SEM_OFFSCREEN) && (v.state&NX_SEM_SELECTED) && !(v.state&NX_SEM_FOCUSED));
    CHECK((v.actions&(NX_ACTION_SELECT|NX_ACTION_ADD_SELECTION|NX_ACTION_DESELECT))==(NX_ACTION_SELECT|NX_ACTION_ADD_SELECTION|NX_ACTION_DESELECT));
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_FOCUS)==NX_ERROR_STATE);
    CHECK(nx_semantics_request_action(ctx,101,NX_ACTION_SELECT)==NX_OK);
    CHECK(frame(ctx,m,0,0,110,0)==0 && selected(m,one,1,103)==0 && nx_focus_id(ctx)==101);
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_ADD_SELECTION)==NX_OK);
    CHECK(frame(ctx,m,0,0,110,0)==0 && selected(m,two,2,103)==0 && nx_focus_id(ctx)==101);
    CHECK(nx_semantics_request_action(ctx,101,NX_ACTION_DESELECT)==NX_OK);
    CHECK(frame(ctx,m,0,0,110,0)==0 && selected(m,other,1,103)==0);
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_DESELECT)==NX_OK);
    CHECK(frame(ctx,m,0,0,110,0)==0 && selected(m,NULL,0,103)==0);
    nx_context_destroy(ctx);nx_selection_model_destroy(m);return 0;
}
static int stale_and_policy(void) {
    nx_context *ctx=NULL;nx_selection_model *m=NULL,*replacement=NULL;uint64_t one=101,other=102;
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && model(&m,101)==0 && model(&replacement,101)==0);
    CHECK(nx_selection_model_set_selected(m,&one,1)==NX_OK);
    CHECK(frame(ctx,m,0,0,110,0)==0);
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_SELECT)==NX_OK);
    CHECK(nx_selection_model_set_selected(m,&one,1)==NX_OK); /* equal setter invalidates */
    CHECK(frame(ctx,m,0,0,110,0)==0 && selected(m,&one,1,0)==0);
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_SELECT)==NX_OK);
    CHECK(frame(ctx,m,0,0,111,0)==0 && selected(m,&one,1,0)==0); /* parent change */
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_SELECT)==NX_OK);
    CHECK(frame(ctx,m,0,1,111,0)==0 && selected(m,&one,1,0)==0);
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_SELECT)==NX_ERROR_STATE);
    CHECK(frame(ctx,m,0,0,111,0)==0);
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_SELECT)==NX_OK);
    CHECK(frame(ctx,m,0,2,111,0)==0 && selected(m,&one,1,0)==0);
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_SELECT)==NX_ERROR_STATE);
    CHECK(frame(ctx,m,0,0,111,0)==0);
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_SELECT)==NX_OK);
    CHECK(frame(ctx,m,0,0,111,1)==0 && selected(m,&one,1,0)==0);
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_SELECT)==NX_ERROR_STATE);
    CHECK(frame(ctx,m,0,0,111,2)==0);
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_SELECT)==NX_OK);
    CHECK(frame(ctx,m,0,0,111,2)==0 && selected(m,&other,1,0)==0); /* offscreen inside modal */
    CHECK(nx_semantics_request_action(ctx,101,NX_ACTION_SELECT)==NX_OK);
    CHECK(frame(ctx,replacement,0,0,111,2)==0 && selected(replacement,NULL,0,0)==0 && selected(m,&other,1,0)==0);
    CHECK(frame(ctx,m,0,0,111,0)==0);
    CHECK(nx_semantics_request_action(ctx,101,NX_ACTION_SELECT)==NX_OK);
    nx_selection_item reduced[]={{102,0},{103,0}},full[]={{101,0},{102,0},{103,0}};
    CHECK(nx_selection_model_set_items(m,reduced,2)==NX_OK && nx_selection_model_set_items(m,full,3)==NX_OK);
    CHECK(frame(ctx,m,0,0,111,0)==0 && selected(m,&other,1,0)==0); /* remove/reinsert */
    CHECK(nx_semantics_request_action(ctx,101,NX_ACTION_SELECT)==NX_OK);
    CHECK(build(ctx,m,0,0,111,0)==0);
    CHECK(nx_selection_model_set_selected(m,&other,1)==NX_OK); /* mutation after binding */
    CHECK(nx_end_frame(ctx)==NX_OK && selected(m,&other,1,0)==0);
    CHECK(nx_semantics_request_action(ctx,101,NX_ACTION_SELECT)==NX_OK);
    CHECK(frame(ctx,m,1,0,111,0)==0 && selected(m,&other,1,0)==0); /* policy changed */
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_DESELECT)==NX_OK);
    CHECK(frame(ctx,m,1,0,111,0)==0 && selected(m,&other,1,0)==0); /* last required retained */
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_DESELECT)==NX_OK);
    CHECK(nx_semantics_request_action(ctx,101,NX_ACTION_ADD_SELECTION)==NX_OK);
    CHECK(frame(ctx,m,1,0,111,0)==0 && selected(m,&one,1,0)==0); /* atomic replacement */
    nx_context_destroy(ctx);nx_selection_model_destroy(m);nx_selection_model_destroy(replacement);return 0;
}
static int atomic_projection(void) {
    nx_context *ctx=NULL;nx_selection_model *a=NULL,*b=NULL;uint64_t selected_b=201;
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && model(&a,101)==0 && model(&b,201)==0);
    CHECK(frame(ctx,a,0,0,110,0)==0);
    CHECK(nx_semantics_request_action(ctx,101,NX_ACTION_SELECT)==NX_OK);
    CHECK(nx_selection_model_set_selected(b,&selected_b,1)==NX_OK);
    CHECK(build(ctx,a,0,0,110,0)==0 && owner(ctx,b,200,0,0,0)==0);
    CHECK(nx_end_frame(ctx)==NX_ERROR_STATE);
    CHECK(selected(a,NULL,0,0)==0 && selected(b,&selected_b,1,0)==0);
    /* Repair the missing projection in the same frame, without replaying input. */
    CHECK(item(ctx,200,201,200,1)==0 && nx_end_frame(ctx)==NX_OK);
    uint64_t selected_a=101;CHECK(selected(a,&selected_a,1,0)==0);
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_SELECT)==NX_OK);
    CHECK(build(ctx,a,0,0,110,0)==0);
    nx_selection_item reduced[]={{101,0},{103,0}};
    CHECK(nx_selection_model_set_items(a,reduced,2)==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK && selected(a,&selected_a,1,0)==0);
    nx_semantic_view v;CHECK(view(ctx,102,&v)==0 && !(v.actions&NX_ACTION_SELECT) && !(v.state&NX_SEM_SELECTED));
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_SELECT)==NX_ERROR_STATE);
    nx_context_destroy(ctx);nx_selection_model_destroy(a);nx_selection_model_destroy(b);return 0;
}
typedef struct tracker {size_t calls,live;int fail;nx_selection_model *mutate;int callback_result;} tracker;
static void *allocate(void *user,size_t bytes) {
    tracker *t=user;++t->calls;if(t->fail)return NULL;
    if(t->mutate) {nx_selection_model *m=t->mutate;t->mutate=NULL;uint64_t id=102;
        t->callback_result=nx_selection_model_set_selected(m,&id,1);}
    void *p=malloc(bytes);if(p)++t->live;return p;
}
static void deallocate(void *user,void *p,size_t bytes) {
    tracker *t=user;(void)bytes;if(p){--t->live;free(p);}
}
static int bounds_and_oom(void) {
    tracker t={0};nx_context_desc cd={0};cd.allocator=(nx_allocator){&t,allocate,deallocate};
    nx_context *ctx=NULL;nx_selection_model *m=NULL;CHECK(nx_context_create(&cd,&ctx)==NX_OK && model(&m,101)==0);
    CHECK(frame(ctx,m,0,0,110,0)==0);
    for(size_t i=0;i<64;++i)CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_ADD_SELECTION)==NX_OK);
    CHECK(nx_semantics_request_action(ctx,101,NX_ACTION_SELECT)==NX_ERROR_LIMIT);
    CHECK(build(ctx,m,0,0,110,0)==0);
    t.fail=1;size_t before=t.calls;
    CHECK(nx_end_frame(ctx)==NX_OK && t.calls==before); /* allocation-free commit */
    uint64_t id=102;CHECK(selected(m,&id,1,0)==0);
    t.fail=0;
    CHECK(nx_semantics_request_action(ctx,101,NX_ACTION_SELECT)==NX_OK);
    CHECK(build(ctx,m,0,0,110,0)==0);
    char large[8192];memset(large,'A',sizeof(large));
    nx_semantic_desc d=desc(900,0,NX_ROLE_TEXT);d.name=(nx_utf8_span){large,sizeof(large)};
    t.fail=1;before=t.calls;
    CHECK(nx_semantics_add(ctx,&d,NULL)==NX_ERROR_MEMORY && t.calls>before);
    CHECK(selected(m,&id,1,0)==0);
    t.fail=0;CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK && nx_end_frame(ctx)==NX_OK);
    id=101;CHECK(selected(m,&id,1,0)==0);
    CHECK(nx_semantics_request_action(ctx,103,NX_ACTION_SELECT)==NX_OK);
    CHECK(build(ctx,m,0,0,110,0)==0);
    char larger[32768];memset(larger,'B',sizeof(larger));d.id=901;d.name=(nx_utf8_span){larger,sizeof(larger)};
    t.mutate=m;t.callback_result=NX_ERROR_STATE;
    CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK && !t.mutate && t.callback_result==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK);id=102;CHECK(selected(m,&id,1,0)==0); /* actual allocator call-out supersedes old work */
    nx_context_destroy(ctx);nx_selection_model_destroy(m);CHECK(t.live==0);return 0;
}
static int binding_validation(void) {
    nx_context *ctx=NULL;nx_selection_model *m=NULL,*nested=NULL;
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && model(&m,101)==0 && model(&nested,201)==0);
    CHECK(nx_begin_frame(ctx,240,160,0)==NX_OK && owner(ctx,m,100,0,0,0)==0);
    CHECK(nx_selection_bind_owner(ctx,100,m,0)==NX_ERROR_STATE);
    nx_semantic_desc d=desc(150,0,NX_ROLE_LIST);
    CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK && nx_selection_bind_owner(ctx,150,m,0)==NX_ERROR_STATE);
    CHECK(owner(ctx,nested,200,100,0,0)==0);
    d=desc(201,200,NX_ROLE_LIST_ITEM);CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK);
    CHECK(nx_selection_bind_item(ctx,100,201,1)==NX_ERROR_STATE);
    CHECK(nx_selection_bind_item(ctx,200,201,1)==NX_OK);
    d=desc(101,100,NX_ROLE_LIST_ITEM);d.actions=NX_ACTION_SELECT|NX_ACTION_FOCUS;
    CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK);
    nx_interaction_state state;CHECK(nx_interact_region(ctx,101,d.bounds,NX_REGION_FOCUSABLE,&state)==NX_OK);
    CHECK(nx_selection_bind_item(ctx,100,101,1)==NX_ERROR_STATE);
    CHECK(nx_end_frame(ctx)==NX_ERROR_STATE); /* explicit SELECT must be bound */
    CHECK(nx_selection_bind_item(ctx,100,101,0)==NX_OK && nx_end_frame(ctx)==NX_OK);
    nx_context_destroy(ctx);nx_selection_model_destroy(m);nx_selection_model_destroy(nested);return 0;
}
static int table_frame(nx_context *ctx,nx_selection_model *m,nx_selection_model *peer) {
    CHECK(nx_begin_frame(ctx,240,160,0)==NX_OK);
    nx_table_column_desc column={0};column.id=11;column.name=(nx_utf8_span){"Name",4};
    column.width=180;column.minimum_width=100;column.maximum_width=200;column.sortable=1;
    nx_table_desc table={0};table.id=10;table.name=(nx_utf8_span){"Table",5};
    table.bounds=(nx_rect){0,0,180,120};table.header_height=24;table.columns=&column;table.column_count=1;
    static nx_table_sort sort={0};CHECK(nx_table_headers(ctx,&table,&sort)==NX_OK);
    CHECK(nx_selection_bind_owner(ctx,10,m,0)==NX_OK); /* Existing header subtree is preserved. */
    for(uint64_t id=101;id<=103;++id) {
        nx_container_desc row={0};row.id=id;row.parent_id=10;row.name=(nx_utf8_span){"Row",3};
        /* Existing painted containers require positive dimensions. Logical
           offscreen semantics can instead use nx_semantics_add directly. */
        row.bounds=(nx_rect){0,id==102?240.0f:24.0f,180,20};
        CHECK(nx_table_row(ctx,&row)==NX_OK);
        CHECK(nx_selection_bind_item(ctx,10,id,id==102)==NX_OK);
        nx_container_desc cell={0};cell.id=id+1000;cell.parent_id=id;
        cell.name=(nx_utf8_span){"Cell",4};cell.bounds=row.bounds;
        CHECK(nx_table_cell(ctx,&cell)==NX_OK);
    }
    CHECK(owner(ctx,peer,200,0,0,0)==0 && item(ctx,200,201,200,1)==0);
    CHECK(nx_end_frame(ctx)==NX_OK);return 0;
}
static int table_and_owner_bounds(void) {
    nx_context *ctx=NULL;nx_selection_model *m=NULL,*peer=NULL;
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && model(&m,101)==0 && model(&peer,201)==0);
    CHECK(table_frame(ctx,m,peer)==0);
    CHECK(nx_semantics_request_action(ctx,101,NX_ACTION_ADD_SELECTION)==NX_OK);
    CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_ADD_SELECTION)==NX_OK);
    CHECK(nx_semantics_request_action(ctx,201,NX_ACTION_SELECT)==NX_OK);
    CHECK(table_frame(ctx,m,peer)==0);
    uint64_t two[]={101,102},one=102,p=201;
    CHECK(selected(m,two,2,0)==0 && selected(peer,&p,1,0)==0);
    nx_semantic_view v;CHECK(view(ctx,10,&v)==0 && v.role==NX_ROLE_TABLE && (v.state&NX_SEM_SELECTION_MULTIPLE));
    CHECK(view(ctx,11,&v)==0 && v.role==NX_ROLE_COLUMN_HEADER && v.parent_id==10);
    CHECK(view(ctx,1102,&v)==0 && v.role==NX_ROLE_CELL && v.parent_id==102 && !(v.actions&NX_ACTION_SELECT));
    CHECK(nx_semantics_request_action(ctx,101,NX_ACTION_DESELECT)==NX_OK);
    CHECK(table_frame(ctx,m,peer)==0 && selected(m,&one,1,0)==0 && selected(peer,&p,1,0)==0);
    CHECK(nx_semantics_request_action(ctx,11,NX_ACTION_SELECT)==NX_OK);
    CHECK(table_frame(ctx,m,peer)==0 && selected(m,&one,1,0)==0 && selected(peer,&p,1,0)==0);
    CHECK(view(ctx,11,&v)==0 && (v.state&NX_SEM_SELECTED) && v.value.length==9);
    nx_context_destroy(ctx);nx_selection_model_destroy(m);nx_selection_model_destroy(peer);
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && nx_begin_frame(ctx,240,160,0)==NX_OK);
    nx_selection_model *models[65]={0};
    for(size_t i=0;i<65;++i) {
        CHECK(nx_selection_model_create(1,NULL,&models[i])==NX_OK);
        nx_semantic_desc d=desc(1000+i,0,NX_ROLE_LIST);CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK);
        CHECK(nx_selection_bind_owner(ctx,d.id,models[i],0)==(i<64?NX_OK:NX_ERROR_LIMIT));
    }
    CHECK(nx_end_frame(ctx)==NX_OK);nx_context_destroy(ctx);
    for(size_t i=0;i<65;++i)nx_selection_model_destroy(models[i]);
    return 0;
}
static int ancestor_frame(nx_context *ctx,nx_selection_model *m,int kind) {
    CHECK(nx_begin_frame(ctx,240,160,0)==NX_OK);
    nx_semantic_desc d=desc(100,0,kind==4?NX_ROLE_TREE:NX_ROLE_LIST);
    CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK && nx_selection_bind_owner(ctx,100,m,0)==NX_OK);
    d=desc(110,100,kind==1?NX_ROLE_CUSTOM:NX_ROLE_PANEL);CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK);
    d=desc(111,100,NX_ROLE_PANEL);CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK);
    d=desc(120,kind==2?111:110,NX_ROLE_PANEL);CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK);
    for(uint64_t id=101;id<=103;++id) {
        d=desc(id,120,kind==3 && id==102?NX_ROLE_TREE_ITEM:NX_ROLE_LIST_ITEM);d.bounds=(nx_rect){0,0,0,0};
        CHECK(nx_semantics_add(ctx,&d,NULL)==NX_OK && nx_selection_bind_item(ctx,100,id,1)==NX_OK);
    }
    CHECK(nx_end_frame(ctx)==NX_OK);return 0;
}
static int ancestor_identity(int only) {
    for(int kind=1;kind<=4;++kind) {
        if(only && only!=kind)continue;
        nx_context *ctx=NULL;nx_selection_model *m=NULL;uint64_t old=101;
        CHECK(nx_context_create(NULL,&ctx)==NX_OK && model(&m,101)==0);
        CHECK(nx_selection_model_set_selected(m,&old,1)==NX_OK && ancestor_frame(ctx,m,0)==0);
        CHECK(nx_semantics_request_action(ctx,102,NX_ACTION_SELECT)==NX_OK);
        CHECK(ancestor_frame(ctx,m,kind)==0);
        nx_selection_view actual;CHECK(nx_selection_model_get(m,&actual)==NX_OK);
        if(actual.selected_count!=1 || actual.selected_ids[0]!=old)
            fprintf(stderr,"stale-kind=%d actual-count=%zu actual-first=%llu expected=%llu\n",kind,
                actual.selected_count,(unsigned long long)(actual.selected_count?actual.selected_ids[0]:0),(unsigned long long)old);
        CHECK(selected(m,&old,1,0)==0);
        nx_context_destroy(ctx);nx_selection_model_destroy(m);
    }
    return 0;
}
int main(int argc,char **argv) {
    if(argc==2 && !strncmp(argv[1],"ancestor-",9)) {
        int kind=atoi(argv[1]+9);CHECK(kind>=1 && kind<=4);return ancestor_identity(kind);
    }
    CHECK(operations()==0);CHECK(stale_and_policy()==0);CHECK(atomic_projection()==0);
    CHECK(bounds_and_oom()==0);CHECK(binding_validation()==0);
    CHECK(table_and_owner_bounds()==0);
    CHECK(ancestor_identity(0)==0);
    puts("Multi-selection owner binding, offscreen projection, stale/policy, atomicity and bounds passed");return 0;
}
