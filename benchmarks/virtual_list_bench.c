/* Synthetic 100k-row list: owner-side refresh/filter plus visible semantics. */
#include "nexaui/nexaui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { ROWS=100000, FRAMES=1000 };
typedef struct accounting {size_t live,peak,allocs,frees;} accounting;
static void *allocate(void *user,size_t n) {
    accounting *a=user;void *p=malloc(n);if(p){a->live+=n;++a->allocs;if(a->live>a->peak)a->peak=a->live;}return p;
}
static void release(void *user,void *p,size_t n) {
    accounting *a=user;if(p){a->live-=n;++a->frees;free(p);}
}
static double now_ms(void) {
    struct timespec t;if(timespec_get(&t,TIME_UTC)!=TIME_UTC)return -1;
    return (double)t.tv_sec*1000+(double)t.tv_nsec/1000000;
}
typedef struct entry {uint64_t key;char name[32];} entry;
typedef struct source {entry *items;size_t *indices,count;int reverse;} source;
static const entry *item_at(const source *s,size_t index) {
    size_t i=s->reverse?s->count-1-index:index;
    return &s->items[s->indices[i]];
}
static nx_result row(void *user,size_t index,uint64_t *key,double *height) {
    source *s=user;*key=item_at(s,index)->key;*height=24;return NX_OK;
}
typedef struct frame {nx_context *ctx;const source *data;size_t count;uint64_t first;} frame;
static nx_result draw_row(void *user,const nx_virtual_row *row) {
    frame *f=user;nx_control_desc d={0};nx_interaction_state state;
    const entry *item=item_at(f->data,row->index);
    if(item->key!=row->key)return NX_ERROR_STATE;
    d.id=row->key;d.parent_id=1;d.name=(nx_utf8_span){item->name,strlen(item->name)};
    d.bounds=(nx_rect){0,(float)row->viewport_y,500,(float)row->height};
    nx_result result=nx_button(f->ctx,&d,&state);
    if(result==NX_OK){if(!f->count)f->first=row->key;++f->count;}
    return result;
}
static int build(nx_context *ctx,nx_virtual_list *list,const source *data,double offset,size_t *visible,uint64_t *first) {
    nx_semantic_desc root={0};root.id=1;root.role=NX_ROLE_WINDOW;
    root.name=(nx_utf8_span){"Synthetic files",15};root.bounds=(nx_rect){0,0,500,480};
    if(nx_begin_frame(ctx,500,480,0)!=NX_OK || nx_semantics_add(ctx,&root,NULL)!=NX_OK)return 0;
    frame f={ctx,data,0,0};nx_virtual_range range;size_t visited=0;
    if(nx_virtual_list_visit(list,offset,480,2,32,draw_row,&f,&range,&visited)!=NX_OK ||
       visited!=f.count || nx_end_frame(ctx)!=NX_OK)return 0;
    size_t nodes=0;if(nx_semantics_count(ctx,&nodes)!=NX_OK || nodes!=visited+1 || visited>25)return 0;
    *visible=visited;*first=f.first;return 1;
}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"virtual-list benchmark:%d: %s\n",__LINE__,#x);goto done;}} while(0)
int main(void) {
    accounting a={0};nx_allocator allocator={&a,allocate,release};nx_virtual_list *list=NULL;nx_context *ctx=NULL;
    int okay=0;double start=now_ms(),corpus,created,refresh,reordered,filter_scan,filtered,scroll;size_t visible=0;uint64_t first=0;
    source s={0};s.items=allocate(&a,ROWS*sizeof(*s.items));s.indices=allocate(&a,ROWS*sizeof(*s.indices));
    CHECK(s.items && s.indices);
    for(size_t i=0;i<ROWS;++i) {
        s.items[i].key=(uint64_t)i+1000;s.indices[i]=i;
        int n=snprintf(s.items[i].name,sizeof(s.items[i].name),"%s-%06zu.txt",i%2?"image":"report",i);
        CHECK(n>0 && (size_t)n<sizeof(s.items[i].name));
    }
    corpus=now_ms()-start;start=now_ms();
    CHECK(start>=0 && nx_virtual_list_create(0,24,&allocator,&list)==NX_OK && nx_context_create(&(nx_context_desc){.allocator=allocator},&ctx)==NX_OK);
    s.count=ROWS;double offset=0;
    CHECK(nx_virtual_list_rebuild(list,s.count,row,&s,480,&offset)==NX_OK);created=now_ms()-start;
    offset=50000.0*24+7;start=now_ms();
    CHECK(nx_virtual_list_rebuild(list,s.count,row,&s,480,&offset)==NX_OK);refresh=now_ms()-start;
    CHECK(offset==50000.0*24+7);s.reverse=1;start=now_ms();
    CHECK(nx_virtual_list_rebuild(list,s.count,row,&s,480,&offset)==NX_OK);reordered=now_ms()-start;
    CHECK(offset==49999.0*24+7 && build(ctx,list,&s,offset,&visible,&first));
    uint32_t index=0;CHECK(nx_semantics_find(ctx,51000,&index)==NX_OK);
    s.reverse=0;s.count=0;start=now_ms();
    for(size_t i=0;i<ROWS;++i)if(strstr(s.items[i].name,"report"))s.indices[s.count++]=i;
    filter_scan=now_ms()-start;CHECK(s.count==ROWS/2);start=now_ms();
    CHECK(nx_virtual_list_rebuild(list,s.count,row,&s,480,&offset)==NX_OK);filtered=now_ms()-start;
    CHECK(offset==25000.0*24+7 && build(ctx,list,&s,offset,&visible,&first));
    CHECK(nx_semantics_find(ctx,51000,&index)==NX_OK && visible<=25);
    nx_semantic_view selected;CHECK(nx_semantics_get(ctx,index,&selected)==NX_OK &&
        selected.name.length==17 && !memcmp(selected.name.data,"report-050000.txt",17));
    size_t warm=a.allocs;start=now_ms();
    for(unsigned i=0;i<FRAMES;++i) {
        double position=(double)((i*47u)%49980u)*24;
        CHECK(build(ctx,list,&s,position,&visible,&first));
    }
    scroll=(now_ms()-start)/FRAMES;
    printf("rows=%u filtered=%zu frames=%u visible=%zu semantic_nodes=%zu corpus_ms=%.3f create_ms=%.3f refresh_ms=%.3f reorder_ms=%.3f filter_scan_ms=%.3f filter_index_ms=%.3f scroll_frame_mean_ms=%.4f peak_bytes=%zu steady_allocations=%zu\n",
           ROWS,s.count,FRAMES,visible,visible+1,corpus,created,refresh,reordered,filter_scan,filtered,scroll,a.peak,a.allocs-warm);
    okay=1;
done:
    nx_context_destroy(ctx);nx_virtual_list_destroy(list);
    release(&a,s.indices,ROWS*sizeof(*s.indices));release(&a,s.items,ROWS*sizeof(*s.items));
    if(a.live || a.allocs!=a.frees){fprintf(stderr,"allocator imbalance: %zu bytes\n",a.live);okay=0;}
    return okay?0:1;
}
