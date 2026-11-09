#define main numeric_value_public_main
#include "test_numeric_value.c"
#undef main
#include "../src/numeric_value.h"

typedef struct lease {int refs,completed,applied,revoked,bad;nx_numeric_value_snapshot s;} lease;
static void retain(void *u){++((lease *)u)->refs;}
static void release(void *u){--((lease *)u)->refs;}
static int current(void *u,uint64_t token,uint64_t context,uint64_t id,uint64_t source) {
    lease *l=u;if(token!=17 || context!=l->s.context_identity || id!=1 || source!=l->s.frame)l->bad=1;
    return !l->bad && !l->revoked;
}
static void completed(void *u,int applied){lease *l=u;++l->completed;l->applied=applied;}
static int test_guard_lifetime(void) {
    for(int test=0;test<6;++test) {
        nx_context *ctx=NULL;CHECK(nx_context_create(NULL,&ctx)==NX_OK);
        nx_value_control_desc d=descriptor();double value=5;lease l={0};
        CHECK(!frame(ctx,d,&value,0,0) && nx_semantics_get_numeric_value(ctx,1,&l.s)==NX_OK);
        nx_numeric_value_guard g={&l,17,retain,release,current};
        CHECK(nx_semantics_request_numeric_value_guarded(ctx,1,&l.s,test==5?5:9,&g,completed)==NX_OK);
        CHECK(l.refs==1 && !l.completed);
        if(test==0 || test==5)CHECK(!frame(ctx,d,&value,0,0) && value==(test==5?5:9));
        if(test==1){l.revoked=1;CHECK(!frame(ctx,d,&value,0,0) && value==5);}
        if(test==2)CHECK(!frame(ctx,d,&value,3,0));
        if(test==4)CHECK(nx_begin_frame(ctx,100,40,0)==NX_OK);
        nx_context_destroy(ctx);
        CHECK(!l.refs && l.completed==1 && !l.bad && l.applied==(test==0 || test==5));
    }
    return 0;
}
typedef struct allocation {size_t fail;unsigned allocations,frees,failures;} allocation;
static void *allocate(void *u,size_t n){allocation *a=u;if(n==a->fail){++a->failures;return NULL;}++a->allocations;return malloc(n);}
static void deallocate(void *u,void *p,size_t n){(void)n;++((allocation *)u)->frees;free(p);}
static int test_failed_paint_retry(int mode) {
    allocation a={0};nx_context_desc cd={0};cd.allocator=(nx_allocator){&a,allocate,deallocate};cd.initial_semantic_text_capacity=4;
    nx_context *ctx=NULL;CHECK(nx_context_create(&cd,&ctx)==NX_OK);
    nx_value_control_desc d=descriptor();d.control.parent_id=0;double value=0;nx_interaction_state state;lease l={0};
    CHECK(nx_begin_frame(ctx,100,40,0)==NX_OK && nx_slider(ctx,&d,&value,&state)==NX_OK && nx_end_frame(ctx)==NX_OK);
    CHECK(nx_semantics_get_numeric_value(ctx,1,&l.s)==NX_OK);
    nx_numeric_value_guard g={&l,17,retain,release,current};
    CHECK(nx_semantics_request_numeric_value_guarded(ctx,1,&l.s,9,&g,completed)==NX_OK);
    CHECK(nx_begin_frame(ctx,100,40,0)==NX_OK);a.fail=8;
    CHECK(nx_slider(ctx,&d,&value,&state)==NX_ERROR_MEMORY && a.failures==1 && value==0 && !l.completed && l.refs==1);
    a.fail=0;int okay=1;
    if(mode!=3) {
        if(mode==1)d.control.disabled=1;
        if(mode==2)l.revoked=1;
        nx_result painted=nx_slider(ctx,&d,&value,&state);
        okay=painted==NX_OK && value==(mode?0:9) && l.completed==1 && l.applied==!mode && !l.refs;
        CHECK(nx_end_frame(ctx)==NX_OK);
    }
    nx_context_destroy(ctx);
    CHECK(okay && a.allocations==a.frees && l.completed==1 && l.applied==!mode && !l.refs && !l.bad);return 0;
}
int main(void) {
    if(test_guard_lifetime())return 1;
    for(int mode=0;mode<4;++mode)if(test_failed_paint_retry(mode))return 1;
    puts("numeric native-lease lifetime/revocation/paint-retry checks passed");return 0;
}
