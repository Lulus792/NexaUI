#include "selection_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s failed\n",__FILE__,__LINE__,#x); return 1; } } while (0)

/* This executable links only the production model component. Its identity
   source is owner-local; native transport identity is tested in SDK suites. */
uint64_t nx_native_next_identity(void) {
    static uint64_t sequence;
    return sequence == UINT64_MAX ? 0 : ++sequence;
}

typedef struct tracker {
    nx_selection_model *model;
    size_t allocations, releases, live, reentries;
    int forbid, callback_failed;
} tracker;

static void busy_probe(tracker *t) {
    if (!t->model) return;
    int changed = 9; size_t count = 9;
    nx_result r = nx_selection_model_prepare_set(t->model,NULL,0,0,&changed,&count);
    if (r != NX_ERROR_STATE || changed || count) t->callback_failed = 1;
    ++t->reentries;
}
static void *allocate(void *user,size_t bytes) {
    tracker *t=user; ++t->allocations; busy_probe(t);
    if(t->forbid)return NULL;
    void *p=malloc(bytes); if(p)++t->live; return p;
}
static void release(void *user,void *p,size_t bytes) {
    tracker *t=user; (void)bytes; ++t->releases; busy_probe(t);
    if(t->forbid)t->callback_failed=1;
    if(p){--t->live;free(p);}
}
static int selection(nx_selection_model *m,const uint64_t *ids,size_t count) {
    nx_selection_view v; CHECK(nx_selection_model_get(m,&v)==NX_OK);
    CHECK(v.selected_count==count);
    CHECK(!count || !memcmp(v.selected_ids,ids,count*sizeof(*ids)));
    /* The public selected-ID cache alone cannot detect an early write to the
       live membership flags before the cache is rebuilt at commit. */
    for(size_t i=0;i<v.item_count;++i) {
        int actual=0,disabled=0,expected=0;
        for(size_t j=0;j<count;++j)if(ids[j]==v.items[i].id)expected=1;
        CHECK(nx_selection_model_item(m,v.items[i].id,0,&actual,&disabled)==NX_OK && actual==expected);
    }
    return 0;
}
static int operations(void) {
    nx_selection_item items[]={{9,0},{3,NX_SELECTION_ITEM_DISABLED},{7,0},{1,0}};
    tracker t={0}; nx_allocator a={&t,allocate,release}; nx_selection_model *m=NULL;
    CHECK(nx_selection_model_create(4,&a,&m)==NX_OK);t.model=m;
    CHECK(nx_selection_model_set_items(m,items,4)==NX_OK);
    uint64_t old[]={3,7};CHECK(nx_selection_model_set_selected(m,old,2)==NX_OK);
    CHECK(nx_selection_model_set_active(m,1)==NX_OK);
    size_t before=t.allocations,freed=t.releases; t.forbid=1;
    int changed=0;size_t count=0;nx_selection_target target=nx_selection_model_target(m);
    uint64_t wanted[]={1,3,9},canonical[]={9,3,1};
    CHECK(nx_selection_model_prepare_set(m,wanted,3,1,&changed,&count)==NX_OK && changed==1 && count==3);
    CHECK(selection(m,old,2)==0); /* Staging does not change a live model. */
    CHECK(nx_selection_model_target(m).revision==target.revision);
    nx_selection_model_commit(m,changed);CHECK(selection(m,canonical,3)==0);
    CHECK(nx_selection_model_target(m).revision==target.revision+1);
    nx_selection_view v;CHECK(nx_selection_model_get(m,&v)==NX_OK && v.active_id==1);
    target=nx_selection_model_target(m);
    CHECK(nx_selection_model_prepare_set(m,wanted,3,1,&changed,&count)==NX_OK && !changed && count==3);
    nx_selection_model_commit(m,changed);CHECK(nx_selection_model_target(m).revision==target.revision);
    uint64_t duplicate[]={9,9},missing[]={9,55},zero[]={0},remove_disabled[]={9,1};
    CHECK(nx_selection_model_prepare_set(m,duplicate,2,0,&changed,&count)==NX_ERROR_ARGUMENT && !changed && !count);
    CHECK(selection(m,canonical,3)==0);
    CHECK(nx_selection_model_prepare_set(m,missing,2,0,&changed,&count)==NX_ERROR_STATE && !changed && !count);
    CHECK(nx_selection_model_prepare_set(m,zero,1,0,&changed,&count)==NX_ERROR_ARGUMENT && !changed && !count);
    CHECK(nx_selection_model_prepare_set(m,remove_disabled,2,0,&changed,&count)==NX_ERROR_STATE && !changed && !count);
    CHECK(nx_selection_model_prepare_set(m,NULL,0,1,&changed,&count)==NX_ERROR_STATE && !changed && !count);
    CHECK(nx_selection_model_prepare_set(m,NULL,0,0,&changed,&count)==NX_ERROR_STATE && !changed && !count);
    CHECK(selection(m,canonical,3)==0 && nx_selection_model_target(m).revision==target.revision);
    CHECK(t.allocations==before && t.releases==freed && !t.callback_failed);
    t.forbid=0;
    uint64_t only[]={9};CHECK(nx_selection_model_set_selected(m,only,1)==NX_OK);
    CHECK(nx_selection_model_prepare_set(m,old,2,0,&changed,&count)==NX_ERROR_STATE); /* Cannot add disabled3. */
    CHECK(selection(m,only,1)==0);
    CHECK(nx_selection_model_prepare_set(m,NULL,0,0,&changed,&count)==NX_OK && changed && !count);
    CHECK(selection(m,only,1)==0);nx_selection_model_commit(m,changed);CHECK(selection(m,NULL,0)==0);
    CHECK(nx_selection_model_prepare_set(m,only,5,0,&changed,&count)==NX_ERROR_LIMIT && !changed && !count);
    CHECK(nx_selection_model_prepare_set(m,NULL,1,0,&changed,&count)==NX_ERROR_ARGUMENT);
    CHECK(nx_selection_model_prepare_set(NULL,NULL,0,0,&changed,&count)==NX_ERROR_ARGUMENT);
    CHECK(nx_selection_model_prepare_set(m,NULL,0,2,&changed,&count)==NX_ERROR_ARGUMENT);
    CHECK(nx_selection_model_prepare_set(m,NULL,0,0,NULL,&count)==NX_ERROR_ARGUMENT && !count);
    t.model=NULL;nx_selection_model_destroy(m);CHECK(!t.live && t.reentries && !t.callback_failed);return 0;
}
static int complete_catalog(void) {
    const size_t maximum=NX_SELECTION_MAX_ITEMS;
    nx_selection_item *items=malloc(maximum*sizeof(*items));uint64_t *ids=malloc(maximum*sizeof(*ids));
    CHECK(items && ids);for(size_t i=0;i<maximum;++i){items[i]=(nx_selection_item){i+1,0};ids[i]=maximum-i;}
    tracker t={0};nx_allocator a={&t,allocate,release};nx_selection_model *m=NULL;
    CHECK(nx_selection_model_create(maximum,&a,&m)==NX_OK);t.model=m;
    CHECK(nx_selection_model_set_items(m,items,maximum)==NX_OK);
    size_t before=t.allocations,freed=t.releases;int changed=0;size_t count=0;t.forbid=1;
    CHECK(nx_selection_model_prepare_set(m,ids,65,1,&changed,&count)==NX_OK && changed && count==65);
    nx_selection_model_commit(m,changed);
    nx_selection_view v;CHECK(nx_selection_model_get(m,&v)==NX_OK && v.selected_count==65 && v.selected_ids[0]==maximum-64);
    CHECK(nx_selection_model_prepare_set(m,ids,maximum,1,&changed,&count)==NX_OK && changed && count==maximum);
    CHECK(nx_selection_model_get(m,&v)==NX_OK && v.selected_count==65);
    int live=1,disabled=0;
    CHECK(nx_selection_model_item(m,1,0,&live,&disabled)==NX_OK && !live);
    nx_selection_model_commit(m,changed);CHECK(nx_selection_model_get(m,&v)==NX_OK && v.selected_count==maximum);
    for(size_t i=0;i<maximum;++i)CHECK(v.selected_ids[i]==i+1);
    CHECK(nx_selection_model_prepare_set(m,v.selected_ids,v.selected_count,1,&changed,&count)==NX_OK && !changed && count==maximum);
    CHECK(nx_selection_model_prepare_set(m,ids,maximum+1,0,&changed,&count)==NX_ERROR_LIMIT && !changed && !count);
    CHECK(nx_selection_model_prepare_set(m,NULL,0,1,&changed,&count)==NX_ERROR_STATE && !changed && !count);
    CHECK(nx_selection_model_get(m,&v)==NX_OK && v.selected_count==maximum);
    CHECK(nx_selection_model_prepare_set(m,NULL,0,0,&changed,&count)==NX_OK && changed && !count);
    nx_selection_model_commit(m,changed);CHECK(selection(m,NULL,0)==0);
    CHECK(t.allocations==before && t.releases==freed && !t.callback_failed);
    t.forbid=0;t.model=NULL;nx_selection_model_destroy(m);CHECK(!t.live);
    free(ids);free(items);return 0;
}
int main(void) {
    if(operations() || complete_catalog())return 1;
    puts("atomic native replacement: staging, disabled preservation, 65/65536, bounds and no allocator callouts passed");return 0;
}
