#define _GNU_SOURCE
#include "observer.h"
#include "core/digest.h"
#include <nexaui/software.h>
#include <errno.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Test owner only: never target an existing authority catalogue. */
static unsigned accepts;
int __real_ae_catalogue_fixture_accept(struct ae_catalogue *,const struct ae_fixture_request *,const uint8_t *,size_t,const uint8_t *,struct ae_fixture_progress *);
int __wrap_ae_catalogue_fixture_accept(struct ae_catalogue *c,const struct ae_fixture_request *r,const uint8_t *m,size_t n,const uint8_t *b,struct ae_fixture_progress *p){
    ++accepts;return __real_ae_catalogue_fixture_accept(c,r,m,n,b,p);
}
static void id(uint8_t out[16],uint8_t last){memset(out,0,16);out[6]=0x40;out[8]=0x80;out[15]=last;}
static unsigned char *read_bytes(const char *path,size_t maximum,size_t *length){
    FILE *file=fopen(path,"rb");if(!file)return NULL;
    if(fseek(file,0,SEEK_END)){fclose(file);return NULL;}
    long size=ftell(file);if(size<=0 || (size_t)size>maximum || fseek(file,0,SEEK_SET)){fclose(file);return NULL;}
    unsigned char *bytes=malloc((size_t)size);
    if(!bytes || fread(bytes,1,(size_t)size,file)!=(size_t)size){free(bytes);fclose(file);return NULL;}
    if(fclose(file)){free(bytes);return NULL;}*length=(size_t)size;return bytes;
}
static int64_t scalar(const char *directory,const char *sql){
    char path[4096];int n=snprintf(path,sizeof(path),"%s/catalogue.sqlite3",directory);
    if(n<=0 || (size_t)n>=sizeof(path))return -1;
    sqlite3 *db=NULL;sqlite3_stmt *statement=NULL;int64_t value=-1;
    if(sqlite3_open_v2(path,&db,SQLITE_OPEN_READONLY,NULL)==SQLITE_OK &&
       sqlite3_prepare_v2(db,sql,-1,&statement,NULL)==SQLITE_OK && sqlite3_step(statement)==SQLITE_ROW)
        value=sqlite3_column_int64(statement,0);
    if(statement && sqlite3_finalize(statement)!=SQLITE_OK)value=-1;
    if(db && sqlite3_close(db)!=SQLITE_OK)value=-1;
    return value;
}
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"operation-native:%d: %s\n",__LINE__,#x);ok=0;goto cleanup;}}while(0)
int main(int argc,char **argv){
    int ok=1,done=0,dirty=1,focus=1,reopened=0;unsigned refreshes=0;
    size_t manifest_size=0,expected_size=0,font_size=0;
    uint8_t *manifest=NULL,*expected=NULL,*font_bytes=NULL;
    struct ae_catalogue *catalogue=NULL;struct ae_fixture_request request={0};
    nx_font *font=NULL;nx_font_collection *fonts=NULL;operation_observer *view=NULL;
    nx_window *window=NULL;nx_software_view *renderer=NULL;
    const char *pending="OBS_READY";char input[64];size_t used=0;
    const char *private_session=getenv("NEXAUI_TEST_PRIVATE_WAYLAND");
    REQUIRE(argc==5 && private_session && !strcmp(private_session,"1"));
    setvbuf(stdout,NULL,_IOLBF,0);
    manifest=read_bytes(argv[1],131072,&manifest_size);expected=read_bytes(argv[2],32,&expected_size);
    font_bytes=read_bytes(argv[4],64000000,&font_size);REQUIRE(manifest && expected && expected_size==32 && font_bytes);
    struct ae_catalogue_identity identity={.kind=AE_CATALOGUE_SYSTEM};id(identity.authority_epoch,1);uint8_t boot[16];id(boot,2);
    REQUIRE(ae_catalogue_create(argv[3],&identity,boot,&catalogue)==0);
    request.scope_revision=1;request.expires_boottime_ns=8000000000000000000LL;request.accepted_unix_ms=123456789;request.os_uid=1000;
    id(request.plan,10);id(request.operation,11);id(request.actor,12);id(request.request_key,13);id(request.scope,3);
    id(request.install,14);id(request.generation,15);id(request.slot,16);id(request.publisher,1);id(request.boot,2);
    REQUIRE(ae_sha256_bytes(manifest,manifest_size,request.artifact)==0);memcpy(request.expected_intent,expected,32);
    struct ae_fixture_progress progress={0};uint8_t evidence[32];memset(evidence,0x42,sizeof(evidence));
    REQUIRE(ae_catalogue_enroll_account(catalogue,request.scope,1000)==0 &&
        ae_catalogue_fixture_accept(catalogue,&request,manifest,manifest_size,boot,&progress)==0 &&
        ae_catalogue_fixture_transition(catalogue,&request,manifest,manifest_size,AE_FIXTURE_EXECUTING,evidence,&progress)==0);
    int64_t revision=scalar(argv[3],"SELECT catalogue_revision FROM authority_meta");REQUIRE(revision>0);
    REQUIRE(nx_font_open(font_bytes,font_size,NULL,&font)==NX_OK && nx_font_collection_create(NULL,&fonts)==NX_OK &&
        nx_font_collection_add(fonts,font)==NX_OK && observer_create(catalogue,&request,manifest,manifest_size,fonts,&view)==NX_OK && observer_refresh(view)==0);
    nx_window_desc desc={"Operation observer test catalogue",640,480,0,{0}};
    REQUIRE(nx_window_create(&desc,&window)==NX_OK && nx_software_view_open(window,&renderer)==NX_OK);
    REQUIRE(fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL)|O_NONBLOCK)==0);
    while(!done){
        nx_window_info info;REQUIRE(nx_window_get_info(window,&info)==NX_OK);
        if(info.should_close){done=1;break;}
        nx_context *ctx=observer_context(view);
        if(dirty){
            observer_action action=OBSERVER_NONE;REQUIRE(observer_frame(view,info.width,info.height,&action)==NX_OK);
            if(action==OBSERVER_CLOSE){done=1;break;}
            int rebuild=focus;
            if(action==OBSERVER_REFRESH){REQUIRE(observer_refresh(view)==0);++refreshes;pending="OBS_REFRESH";rebuild=1;}
            if(focus){REQUIRE(nx_focus_set(ctx,200)==NX_OK);focus=0;}
            if(rebuild){REQUIRE(observer_frame(view,info.width,info.height,&action)==NX_OK && action==OBSERVER_NONE);}
            nx_result published=nx_window_publish_semantics(window,ctx);REQUIRE(published==NX_OK || published==NX_ERROR_UNSUPPORTED);
            const nx_command *commands=NULL;size_t count=0;nx_color background;nx_software_view_outcome outcome;
            REQUIRE(nx_frame_commands(ctx,&commands,&count)==NX_OK && nx_theme_color(ctx,NX_COLOR_BACKGROUND,&background)==NX_OK &&
                nx_software_view_render(renderer,commands,count,background,&outcome)==NX_OK);
            dirty=outcome!=NX_SOFTWARE_VIEW_READY;
            if(!dirty && pending){puts(pending);pending=NULL;}
        }
        uint32_t ready=0;REQUIRE(nx_window_poll_with_fd(window,UINT32_MAX,STDIN_FILENO,&ready)==NX_OK);
        nx_event event;int available=0;
        do{REQUIRE(nx_window_next_event(window,&event,&available)==NX_OK);
            if(available){
                if(event.type==NX_EVENT_KEY_DOWN && event.key==NX_KEY_ESCAPE){done=1;break;}
                REQUIRE(nx_context_queue_event(ctx,&event)==NX_OK);dirty=1;
            }
        }while(available);
        size_t routed=0;REQUIRE(nx_window_route_events(window,ctx,&routed)==NX_OK);if(routed)dirty=1;
        if(ready){
            REQUIRE(used<sizeof(input)-1);ssize_t n=read(STDIN_FILENO,input+used,sizeof(input)-used-1);
            REQUIRE(n>0 || (n<0 && (errno==EAGAIN || errno==EINTR)));
            if(n>0)used+=(size_t)n;
            input[used]=0;char *end=memchr(input,'\n',used);
            if(end){
                REQUIRE(!reopened && (size_t)(end-input)==9 && !memcmp(input,"uncertain",9));
                used-=(size_t)(end-input)+1;memmove(input,end+1,used);
                REQUIRE(ae_catalogue_fixture_transition(catalogue,&request,manifest,manifest_size,AE_FIXTURE_UNCERTAIN,evidence,&progress)==0);
                revision=scalar(argv[3],"SELECT catalogue_revision FROM authority_meta");REQUIRE(revision>0);
                nx_result detached=nx_window_publish_semantics(window,NULL);REQUIRE(detached==NX_OK || detached==NX_ERROR_UNSUPPORTED);
                observer_destroy(view);view=NULL;
                REQUIRE(observer_create(catalogue,&request,manifest,manifest_size,fonts,&view)==NX_OK && observer_refresh(view)==0);
                reopened=1;dirty=1;focus=1;pending="OBS_REOPENED";
            }
        }
    }
    REQUIRE(accepts==1 && scalar(argv[3],"SELECT count(*) FROM operations")==1 &&
        scalar(argv[3],"SELECT catalogue_revision FROM authority_meta")==revision);
    printf("OBS_CLOSED accepts=%u refreshes=%u reopened=%d\n",accepts,refreshes,reopened);
cleanup:
    if(window){nx_result result=nx_window_publish_semantics(window,NULL);if(result!=NX_OK && result!=NX_ERROR_UNSUPPORTED)ok=0;}
    if(renderer && nx_software_view_close(&renderer)!=NX_OK)ok=0;
    observer_destroy(view);if(window)nx_window_destroy(window);
    if(catalogue && ae_catalogue_close(&catalogue)!=0)ok=0;
    if(fonts)nx_font_collection_destroy(fonts);
    if(font)nx_font_close(font);
    free(font_bytes);free(expected);free(manifest);return ok?0:1;
}
