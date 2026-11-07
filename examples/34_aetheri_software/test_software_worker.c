#define _GNU_SOURCE
#include "software_worker.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sqlite3.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"software worker %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static pthread_t main_thread,worker_thread;
static atomic_int have_worker,reads,closes,resolves,inject_error,fail_reply;
static pthread_mutex_t gate_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_changed=PTHREAD_COND_INITIALIZER;
static int hold,entered,released;
static void owner_thread(void){
    CHECK(!pthread_equal(main_thread,pthread_self()));
    if(!atomic_load(&have_worker)){worker_thread=pthread_self();atomic_store(&have_worker,1);}
    else CHECK(pthread_equal(worker_thread,pthread_self()));
}
int __real_ae_catalogue_open(const char *,const struct ae_catalogue_identity *,struct ae_catalogue **);
int __wrap_ae_catalogue_open(const char *p,const struct ae_catalogue_identity *identity,struct ae_catalogue **out){owner_thread();return __real_ae_catalogue_open(p,identity,out);}
int __real_ae_catalogue_get_info(struct ae_catalogue *,struct ae_catalogue_info *);
int __wrap_ae_catalogue_get_info(struct ae_catalogue *c,struct ae_catalogue_info *out){owner_thread();return __real_ae_catalogue_get_info(c,out);}
int __real_ae_catalogue_close(struct ae_catalogue **);
int __wrap_ae_catalogue_close(struct ae_catalogue **c){owner_thread();atomic_fetch_add(&closes,1);return __real_ae_catalogue_close(c);}
int __real_ae_catalogue_resolve_app(struct ae_catalogue *,const uint8_t[16],uint32_t,const uint8_t[16],const uint8_t[16],const uint8_t[16],struct ae_install_snapshot *);
int __wrap_ae_catalogue_resolve_app(struct ae_catalogue *c,const uint8_t account[16],uint32_t uid,const uint8_t install[16],const uint8_t boot[16],const uint8_t runtime[16],struct ae_install_snapshot *out){
    owner_thread();atomic_fetch_add(&resolves,1);return __real_ae_catalogue_resolve_app(c,account,uid,install,boot,runtime,out);
}
int __real_sw_read(struct ae_catalogue *,const sw_scope *,const sw_id *,size_t,sw_result **);
int __wrap_sw_read(struct ae_catalogue *c,const sw_scope *scope,const sw_id *targets,size_t count,sw_result **out){
    owner_thread();atomic_fetch_add(&reads,1);int error=atomic_exchange(&inject_error,0);
    return error?error:__real_sw_read(c,scope,targets,count,out);
}
int __real_sqlite3_prepare_v2(sqlite3 *,const char *,int,sqlite3_stmt **,const char **);
int __wrap_sqlite3_prepare_v2(sqlite3 *db,const char *sql,int n,sqlite3_stmt **out,const char **tail){
    if(strstr(sql,"SELECT c.component_id,c.publisher_ref")){
        pthread_mutex_lock(&gate_lock);
        if(hold){CHECK(!sqlite3_get_autocommit(db));entered=1;pthread_cond_signal(&gate_changed);
            while(!released)pthread_cond_wait(&gate_changed,&gate_lock);
            hold=0;}
        pthread_mutex_unlock(&gate_lock);
    }
    return __real_sqlite3_prepare_v2(db,sql,n,out,tail);
}
void *__real_calloc(size_t,size_t);
void *__wrap_calloc(size_t n,size_t size){if(n==1&&size==sizeof(sww_reply)&&atomic_exchange(&fail_reply,0))return NULL;return __real_calloc(n,size);}
static unsigned fds(void){DIR *d=opendir("/proc/self/fd");CHECK(d);struct dirent *e;unsigned n=0;while((e=readdir(d)))if(e->d_name[0]!='.')++n;CHECK(!closedir(d));return n;}
static sw_id id(uint32_t n){sw_id v={{0}};v.bytes[6]=0x40;v.bytes[8]=0x80;for(unsigned i=0;i<4;++i)v.bytes[15-i]=(uint8_t)(n>>(8*i));return v;}
static void gate_begin(void){pthread_mutex_lock(&gate_lock);hold=1;entered=released=0;pthread_mutex_unlock(&gate_lock);}
static void gate_wait(void){pthread_mutex_lock(&gate_lock);while(!entered)pthread_cond_wait(&gate_changed,&gate_lock);pthread_mutex_unlock(&gate_lock);}
static void gate_release(void){pthread_mutex_lock(&gate_lock);released=1;pthread_cond_signal(&gate_changed);pthread_mutex_unlock(&gate_lock);}
static nx_sim_message receive(sww_worker *w){
    nx_sim_message m={0};for(unsigned i=0;i<100;++i){CHECK(sww_receive(w,&m)==NX_OK);if(m.ticket)return m;
        struct pollfd p={sww_wake_fd(w),POLLIN,0};CHECK(poll(&p,1,100)>=0);}
    CHECK(0);return m;
}
static void observed(const sww_reply *r,const sw_scope *scope){
    CHECK(r&&r->original_error==0&&r->result&&r->result->count==5);
    CHECK(!memcmp(r->result->scope.account,scope->account,16)&&r->result->scope.os_uid==1000);
    const uint32_t ids[]={10,11,12,13,99};
    for(size_t i=0;i<5;++i){const sw_row *row=&r->result->rows[i];CHECK(!memcmp(row->install.bytes,id(ids[i]).bytes,16));CHECK(row->error==((i==0||i==2)?0:-ENOENT));}
}
int main(int argc,char **argv){
    CHECK(argc==2);main_thread=pthread_self();unsigned before=fds();
    struct ae_catalogue_identity identity={.kind=AE_CATALOGUE_SYSTEM};memcpy(identity.authority_epoch,id(1).bytes,16);
    sww_worker *w=NULL;struct ae_catalogue_info context={0};int error=77;
    CHECK(sww_start(argv[1],&identity,&w,&context,&error)==NX_OK&&w&&error==0&&context.revision>0);
    CHECK(atomic_load(&have_worker)&&!atomic_load(&reads));
    CHECK((fcntl(sww_wake_fd(w),F_GETFD)&FD_CLOEXEC)&&(fcntl(sww_wake_fd(w),F_GETFL)&O_NONBLOCK));
    CHECK(sww_join(w)==NX_ERROR_STATE);
    sw_scope scope={.identity=identity,.context=context,.os_uid=1000};memcpy(scope.account,id(2).bytes,16);
    sw_id targets[]={id(12),id(99),id(10),id(13),id(11)};
    sw_owner *owner=NULL;CHECK(sw_owner_create(&scope,targets,5,&owner)==NX_OK);
    uint64_t first=0,second=0,overflow=99;
    CHECK(sww_submit(w,(sww_key){0,1},&scope,targets,5,&overflow)==NX_ERROR_ARGUMENT&&overflow==0);
    CHECK(sww_submit(w,(sww_key){1,1},&scope,NULL,5,&overflow)==NX_ERROR_ARGUMENT&&overflow==0);
    CHECK(sww_submit(w,(sww_key){1,1},&scope,targets,SW_MAX_TARGETS+1U,&overflow)==NX_ERROR_LIMIT&&overflow==0);
    gate_begin();CHECK(sww_submit(w,(sww_key){11,1},&scope,targets,5,&first)==NX_OK&&sw_owner_expect(owner,first)==NX_OK);gate_wait();
    sw_scope borrowed=scope;sw_id borrowed_targets[5];memcpy(borrowed_targets,targets,sizeof targets);
    CHECK(sww_submit(w,(sww_key){12,2},&borrowed,borrowed_targets,5,&second)==NX_OK&&second>first&&sw_owner_expect(owner,second)==NX_OK);
    /* Mutate the queued caller inputs before the original producer can read them. */
    borrowed.os_uid=1001;borrowed.account[15]^=1;borrowed.context.runtime_incarnation[15]^=1;borrowed_targets[0]=id(100);
    CHECK(sww_submit(w,(sww_key){13,3},&scope,targets,5,&overflow)==NX_ERROR_LIMIT&&overflow==0);
    gate_release();nx_sim_message m=receive(w);sww_reply *reply=sww_reply_get(&m);observed(reply,&scope);
    CHECK(m.ticket==first&&reply->key.view==11&&reply->key.request==1);
    sw_result *kept=reply->result;CHECK(sw_owner_complete(owner,m.ticket,0,&reply->result)==NX_ERROR_STATE&&reply->result==kept);nx_sim_message_dispose(&m);
    m=receive(w);reply=sww_reply_get(&m);observed(reply,&scope);CHECK(m.ticket==second&&reply->key.view==12&&reply->key.request==2);
    sw_result *value=NULL,*again=NULL;CHECK(sww_reply_take(reply,&value)==NX_OK&&value&&sww_reply_take(reply,&again)==NX_ERROR_STATE&&!again);nx_sim_message_dispose(&m);
    CHECK(sw_owner_complete(owner,second,0,&value)==NX_OK&&!value&&sw_owner_select(owner,id(12))==NX_OK);
    sw_snapshot *snapshot=NULL;CHECK(sw_owner_snapshot(owner,&snapshot)==NX_OK&&sw_snapshot_data(snapshot)->count==5);
    atomic_store(&inject_error,-EIO);CHECK(sww_submit(w,(sww_key){12,3},&scope,targets,5,&first)==NX_OK&&sw_owner_expect(owner,first)==NX_OK);
    m=receive(w);reply=sww_reply_get(&m);CHECK(reply&&reply->original_error==-EIO&&!reply->result);
    CHECK(sw_owner_complete(owner,first,reply->original_error,&reply->result)==NX_OK&&sw_owner_state(owner)==SW_UNAVAILABLE&&sw_owner_error(owner)==-EIO&&sw_owner_epoch(owner)==1);
    nx_sim_message_dispose(&m);sw_snapshot *retained=NULL;CHECK(sw_owner_snapshot(owner,&retained)==NX_OK&&sw_snapshot_data(retained)==sw_snapshot_data(snapshot));sw_snapshot_release(&retained);
    atomic_store(&fail_reply,1);CHECK(sww_submit(w,(sww_key){12,4},&scope,targets,5,&first)==NX_OK);
    m=receive(w);CHECK(m.ticket==first&&m.status==NX_ERROR_MEMORY&&!sww_reply_get(&m)&&!m.data);nx_sim_message_dispose(&m);
    CHECK(sww_submit(w,(sww_key){12,5},&scope,targets,5,&first)==NX_OK);m=receive(w);observed(sww_reply_get(&m),&scope);nx_sim_message_dispose(&m);
    CHECK(sww_destroy(&w)==NX_OK&&!w);sw_owner_destroy(&owner);CHECK(sw_snapshot_data(snapshot)->count==5);sw_snapshot_release(&snapshot);CHECK(fds()==before);
    puts("SOFTWARE_WORKER_COPIED_QUEUED_SCOPE_TARGETS_TICKET_CAPACITY_ONCE_ERROR_RETAINED_SNAPSHOT_OOM_RETRY_OK");

    atomic_store(&have_worker,0);int read_before=atomic_load(&reads),close_before=atomic_load(&closes),resolve_before=atomic_load(&resolves);
    CHECK(sww_start(argv[1],&identity,&w,&context,&error)==NX_OK&&sw_owner_create(&scope,targets,5,&owner)==NX_OK);
    nx_context *gui=NULL;CHECK(nx_context_create(NULL,&gui)==NX_OK);
    gate_begin();CHECK(sww_submit(w,(sww_key){21,1},&scope,targets,5,&first)==NX_OK);gate_wait();
    CHECK(sww_submit(w,(sww_key){22,2},&scope,targets,5,&second)==NX_OK);
    /* Original SQLite transaction is held while both UI objects retire. */
    nx_context_destroy(gui);gui=NULL;sw_owner_destroy(&owner);CHECK(!owner&&sww_stop(w)==NX_OK);
    CHECK(sww_submit(w,(sww_key){23,3},&scope,targets,5,&overflow)==NX_ERROR_STATE&&overflow==0);
    gate_release();CHECK(sww_join(w)==NX_OK&&atomic_load(&reads)==read_before+1&&atomic_load(&resolves)==resolve_before+5&&atomic_load(&closes)==close_before+1);
    struct pollfd wake={sww_wake_fd(w),POLLIN,0};CHECK(poll(&wake,1,0)==1&&(wake.revents&POLLIN));
    m=receive(w);CHECK(m.ticket==first);reply=sww_reply_get(&m);observed(reply,&scope);CHECK(reply->key.view==21);nx_sim_message_dispose(&m);
    /* Wake pipe is drained; second completion must remain directly receivable. */
    CHECK(sww_receive(w,&m)==NX_OK&&m.ticket==second);reply=sww_reply_get(&m);CHECK(reply&&reply->key.view==22&&reply->original_error==-ECANCELED&&!reply->result);nx_sim_message_dispose(&m);
    CHECK(sww_receive(w,&m)==NX_OK&&!m.ticket&&sww_destroy(&w)==NX_OK&&!w&&fds()==before);
    puts("SOFTWARE_WORKER_REAL_SQL_TRANSACTION_UI_RETIREMENT_QUEUED_CANCEL_DRAIN_JOIN_FDS_OK");
    atomic_store(&have_worker,0);struct ae_catalogue_identity wrong=identity;wrong.authority_epoch[15]^=1;
    struct ae_catalogue_info unchanged=context;CHECK(sww_start(argv[1],&wrong,&w,&context,&error)!=NX_OK&&!w&&error<0&&!memcmp(&context,&unchanged,sizeof context)&&fds()==before);
    CHECK(sww_start("relative",&identity,&w,&context,&error)==NX_ERROR_ARGUMENT&&!w&&!memcmp(&context,&unchanged,sizeof context)&&fds()==before);
    puts("SOFTWARE_WORKER_ORIGINAL_THREAD_OWNERSHIP_SOURCE_REFUSAL_AND_CONTEXT_PRESERVATION_OK");return 0;
}
