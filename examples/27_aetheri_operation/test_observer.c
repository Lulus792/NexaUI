#define _GNU_SOURCE
#include "observer.h"
#include "core/digest.h"
#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do{if(!(x)){fprintf(stderr,"observer:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned accepts;
static int fail_read_begin;
int __real_sqlite3_exec(sqlite3 *,const char *,int (*)(void *,int,char **,char **),void *,char **);
int __wrap_sqlite3_exec(sqlite3 *db,const char *sql,int (*callback)(void *,int,char **,char **),void *user,char **error){
    if(fail_read_begin && !strcmp(sql,"BEGIN")){fail_read_begin=0;return SQLITE_IOERR;}
    return __real_sqlite3_exec(db,sql,callback,user,error);
}
int __real_ae_catalogue_fixture_accept(struct ae_catalogue *,const struct ae_fixture_request *,const uint8_t *,size_t,const uint8_t *,struct ae_fixture_progress *);
int __wrap_ae_catalogue_fixture_accept(struct ae_catalogue *c,const struct ae_fixture_request *r,const uint8_t *m,size_t n,const uint8_t *b,struct ae_fixture_progress *p){
    ++accepts;return __real_ae_catalogue_fixture_accept(c,r,m,n,b,p);
}
static void id(uint8_t out[16],uint8_t last){memset(out,0,16);out[6]=0x40;out[8]=0x80;out[15]=last;}
static unsigned char *read_bytes(const char *path,size_t maximum,size_t *length){
    FILE *file=fopen(path,"rb");CHECK(file);
    CHECK(!fseek(file,0,SEEK_END));long size=ftell(file);CHECK(size>0 && (size_t)size<=maximum && !fseek(file,0,SEEK_SET));
    unsigned char *bytes=malloc((size_t)size);CHECK(bytes && fread(bytes,1,(size_t)size,file)==(size_t)size && !fclose(file));
    *length=(size_t)size;return bytes;
}
static int64_t scalar(const char *directory,const char *sql){
    char path[4096];int n=snprintf(path,sizeof(path),"%s/catalogue.sqlite3",directory);CHECK(n>0 && (size_t)n<sizeof(path));
    sqlite3 *db=NULL;sqlite3_stmt *statement=NULL;
    CHECK(sqlite3_open_v2(path,&db,SQLITE_OPEN_READONLY,NULL)==SQLITE_OK && sqlite3_prepare_v2(db,sql,-1,&statement,NULL)==SQLITE_OK);
    CHECK(sqlite3_step(statement)==SQLITE_ROW);int64_t value=sqlite3_column_int64(statement,0);
    CHECK(sqlite3_step(statement)==SQLITE_DONE && sqlite3_finalize(statement)==SQLITE_OK && sqlite3_close(db)==SQLITE_OK);return value;
}
static nx_semantic_view node(operation_observer *view,uint64_t id){
    uint32_t index;nx_semantic_view result;nx_context *ctx=observer_context(view);
    CHECK(nx_semantics_find(ctx,id,&index)==NX_OK && nx_semantics_get(ctx,index,&result)==NX_OK);return result;
}
static void frame(operation_observer *view,float width,float height,int busy,const char *message){
    observer_action action;CHECK(observer_frame(view,width,height,&action)==NX_OK && action==OBSERVER_NONE);
    nx_semantic_view status=node(view,10);CHECK(status.role==NX_ROLE_STATUS && !status.value.length && !status.actions);
    CHECK(!!(status.state&NX_SEM_BUSY)==busy && status.name.length==strlen(message) && !memcmp(status.name.data,message,strlen(message)));
    CHECK(node(view,100).value.length==32 && !memcmp(node(view,100).value.data,"0000000000004000800000000000000b",32));
}
static void capture(operation_observer *view,unsigned width,unsigned height,const char *directory,const char *name){
    const nx_command *commands=NULL;size_t count=0;unsigned char *pixels=malloc((size_t)width*height*4);CHECK(pixels);
    nx_color background;CHECK(nx_theme_color(observer_context(view),NX_COLOR_BACKGROUND,&background)==NX_OK);
    CHECK(nx_frame_commands(observer_context(view),&commands,&count)==NX_OK && nx_headless_render(commands,count,pixels,width,height,width*4,background)==NX_OK);
    char path[4096];int n=snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);CHECK(n>0 && (size_t)n<sizeof(path));
    FILE *file=fopen(path,"wb");CHECK(file && fprintf(file,"P6\n%u %u\n255\n",width,height)>0);
    for(size_t i=0;i<(size_t)width*height;++i)CHECK(fwrite(pixels+4*i,1,3,file)==3);
    CHECK(!fclose(file));free(pixels);
}
static void key(operation_observer *view,nx_key code){
    nx_event event={0};event.type=NX_EVENT_KEY_DOWN;event.key=code;CHECK(nx_context_queue_event(observer_context(view),&event)==NX_OK);
    event.type=NX_EVENT_KEY_UP;CHECK(nx_context_queue_event(observer_context(view),&event)==NX_OK);
}
int main(int argc,char **argv){
    CHECK(argc==6);size_t manifest_size=0,expected_size=0,font_size=0;
    uint8_t *manifest=read_bytes(argv[1],131072,&manifest_size),*expected=read_bytes(argv[2],32,&expected_size);
    unsigned char *font_bytes=read_bytes(argv[4],64000000,&font_size);CHECK(expected_size==32);
    struct ae_catalogue_identity identity={.kind=AE_CATALOGUE_SYSTEM};id(identity.authority_epoch,1);uint8_t boot[16];id(boot,2);
    struct ae_catalogue *catalogue=NULL;CHECK(ae_catalogue_create(argv[3],&identity,boot,&catalogue)==0);
    struct ae_fixture_request request={.scope_revision=1,.expires_boottime_ns=8000000000000000000LL,.accepted_unix_ms=123456789,.os_uid=1000};
    id(request.plan,10);id(request.operation,11);id(request.actor,12);id(request.request_key,13);id(request.scope,3);
    id(request.install,14);id(request.generation,15);id(request.slot,16);id(request.publisher,1);id(request.boot,2);
    CHECK(ae_sha256_bytes(manifest,manifest_size,request.artifact)==0);memcpy(request.expected_intent,expected,32);
    CHECK(ae_catalogue_enroll_account(catalogue,request.scope,1000)==0);
    nx_font *font=NULL;nx_font_collection *fonts=NULL;
    CHECK(nx_font_open(font_bytes,font_size,NULL,&font)==NX_OK && nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    operation_observer *view=NULL;
    CHECK(observer_create(catalogue,&request,manifest,manifest_size,fonts,&view)==NX_OK);
    frame(view,640,480,0,"Status not yet queried");
    CHECK(!accepts && scalar(argv[3],"SELECT count(*) FROM operations")==0);
    CHECK(observer_refresh(view)==-ENOENT && !accepts && scalar(argv[3],"SELECT count(*) FROM operations")==0);
    frame(view,640,480,0,"Original operation unavailable; query its owner");
    struct ae_fixture_progress progress={0};
    CHECK(ae_catalogue_fixture_accept(catalogue,&request,manifest,manifest_size,boot,&progress)==0 && accepts==1);
    CHECK(observer_refresh(view)==0);frame(view,640,480,1,"Accepted; validation pending");
    uint8_t evidence[32];memset(evidence,0x42,sizeof(evidence));
    CHECK(ae_catalogue_fixture_transition(catalogue,&request,manifest,manifest_size,AE_FIXTURE_EXECUTING,evidence,&progress)==0);
    CHECK(observer_refresh(view)==0);frame(view,640,480,1,"Staging in progress; no completion estimate");
    capture(view,640,480,argv[5],"executing");
    fail_read_begin=1;CHECK(observer_refresh(view)==-EIO && !fail_read_begin);
    frame(view,640,480,0,"Original operation unavailable; query its owner");
    CHECK(ae_catalogue_fixture_transition(catalogue,&request,manifest,manifest_size,AE_FIXTURE_READY,evidence,&progress)==0);
    CHECK(observer_refresh(view)==0);frame(view,640,480,1,"Staging recorded; publication pending");
    CHECK(node(view,107).value.length==13 && !memcmp(node(view,107).value.data,"Not activated",13));
    CHECK(ae_catalogue_fixture_transition(catalogue,&request,manifest,manifest_size,AE_FIXTURE_UNCERTAIN,evidence,&progress)==0);
    int64_t revision=scalar(argv[3],"SELECT catalogue_revision FROM authority_meta");
    observer_destroy(view);view=NULL;
    CHECK(observer_create(catalogue,&request,manifest,manifest_size,fonts,&view)==NX_OK && observer_refresh(view)==0);
    frame(view,640,480,0,"Outcome uncertain; retain the original operation");capture(view,640,480,argv[5],"uncertain");
    frame(view,420,320,0,"Outcome uncertain; retain the original operation");capture(view,420,320,argv[5],"narrow");
    CHECK(node(view,108).value.length==16 && !memcmp(node(view,108).value.data,"aetheri.p0.probe",16));
    CHECK(nx_focus_set(observer_context(view),21)==NX_OK);key(view,NX_KEY_END);
    frame(view,420,320,0,"Outcome uncertain; retain the original operation");
    nx_semantic_view last=node(view,108),viewport=node(view,20);
    CHECK(last.bounds.y>=viewport.bounds.y && last.bounds.y+last.bounds.height<=viewport.bounds.y+viewport.bounds.height+1);
    capture(view,420,320,argv[5],"narrow-bottom");
    nx_theme light;CHECK(nx_theme_preset(NX_THEME_LIGHT,&light)==NX_OK && nx_context_set_theme(observer_context(view),&light)==NX_OK);
    frame(view,640,480,0,"Outcome uncertain; retain the original operation");capture(view,640,480,argv[5],"light");
    float original_em=light.metrics[NX_METRIC_FONT_SIZE];CHECK(nx_theme_set_metric(&light,NX_METRIC_FONT_SIZE,2*original_em)==NX_OK && nx_context_set_theme(observer_context(view),&light)==NX_OK);
    frame(view,420,320,0,"Outcome uncertain; retain the original operation");capture(view,420,320,argv[5],"large-text");
    CHECK(nx_context_set_theme(observer_context(view),NULL)==NX_OK);
    /* A caller's later draft must not retarget an already bound view. */
    id(request.operation,77);CHECK(observer_refresh(view)==0);
    frame(view,640,480,0,"Outcome uncertain; retain the original operation");id(request.operation,11);
    CHECK(nx_focus_set(observer_context(view),200)==NX_OK);key(view,NX_KEY_SPACE);
    observer_action action;CHECK(observer_frame(view,640,480,&action)==NX_OK && action==OBSERVER_REFRESH && observer_refresh(view)==0);
    key(view,NX_KEY_TAB);CHECK(observer_frame(view,640,480,&action)==NX_OK && nx_focus_id(observer_context(view))==201);
    key(view,NX_KEY_ENTER);CHECK(observer_frame(view,640,480,&action)==NX_OK && action==OBSERVER_CLOSE);
    CHECK(accepts==1 && scalar(argv[3],"SELECT count(*) FROM operations")==1 && scalar(argv[3],"SELECT catalogue_revision FROM authority_meta")==revision);
    observer_destroy(view);view=NULL;
    /* A different unaccepted operation stays unavailable; it cannot adopt the
       original result or create a replacement operation merely by opening. */
    id(request.operation,77);CHECK(observer_create(catalogue,&request,manifest,manifest_size,fonts,&view)==NX_OK);
    CHECK(observer_refresh(view)!=0);observer_action unused;CHECK(observer_frame(view,640,480,&unused)==NX_OK);
    CHECK(!(node(view,10).state&NX_SEM_BUSY) && accepts==1);
    CHECK(accepts==1 && scalar(argv[3],"SELECT count(*) FROM operations")==1 && scalar(argv[3],"SELECT catalogue_revision FROM authority_meta")==revision);
    observer_destroy(view);CHECK(ae_catalogue_close(&catalogue)==0);
    nx_font_collection_destroy(fonts);nx_font_close(font);free(font_bytes);free(expected);free(manifest);
    puts("Original catalogue observations, exact target, uncertainty, keyboard and reopen without replay passed");return 0;
}
