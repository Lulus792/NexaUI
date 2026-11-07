#define _GNU_SOURCE
#include "recovery_worker.h"
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
#define CHECK(x) do{if(!(x)){fprintf(stderr,"recovery worker %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static pthread_t main_thread,worker_thread;
static atomic_int have_worker,reads,closes,inject_error,fail_reply;
static pthread_mutex_t gate_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_changed=PTHREAD_COND_INITIALIZER;
static int hold,entered,released;
static void owner_thread(void){
    CHECK(!pthread_equal(main_thread,pthread_self()));
    if(!atomic_load(&have_worker)){worker_thread=pthread_self();atomic_store(&have_worker,1);}
    else CHECK(pthread_equal(worker_thread,pthread_self()));
}
int __real_ae_catalogue_open(const char *,const struct ae_catalogue_identity *,struct ae_catalogue **);
int __wrap_ae_catalogue_open(const char *p,const struct ae_catalogue_identity *id,struct ae_catalogue **out){owner_thread();return __real_ae_catalogue_open(p,id,out);}
int __real_ae_catalogue_get_info(struct ae_catalogue *,struct ae_catalogue_info *);
int __wrap_ae_catalogue_get_info(struct ae_catalogue *s,struct ae_catalogue_info *out){owner_thread();return __real_ae_catalogue_get_info(s,out);}
int __real_ae_catalogue_close(struct ae_catalogue **);
int __wrap_ae_catalogue_close(struct ae_catalogue **s){owner_thread();atomic_fetch_add(&closes,1);return __real_ae_catalogue_close(s);}
int __real_ae_catalogue_recovery_inventory(struct ae_catalogue *,size_t,struct ae_recovery_inventory **);
int __wrap_ae_catalogue_recovery_inventory(struct ae_catalogue *s,size_t count,struct ae_recovery_inventory **out){
    owner_thread();atomic_fetch_add(&reads,1);int error=atomic_exchange(&inject_error,0);
    if(error)return error;
    return __real_ae_catalogue_recovery_inventory(s,count,out);
}
int __real_sqlite3_prepare_v2(sqlite3 *,const char *,int,sqlite3_stmt **,const char **);
int __wrap_sqlite3_prepare_v2(sqlite3 *db,const char *sql,int n,sqlite3_stmt **out,const char **tail){
    if(strstr(sql,"SELECT o.operation_id")){
        pthread_mutex_lock(&gate_lock);
        if(hold){CHECK(!sqlite3_get_autocommit(db));entered=1;pthread_cond_signal(&gate_changed);
            while(!released)pthread_cond_wait(&gate_changed,&gate_lock);
            hold=0;}
        pthread_mutex_unlock(&gate_lock);
    }
    return __real_sqlite3_prepare_v2(db,sql,n,out,tail);
}
void *__real_calloc(size_t,size_t);
void *__wrap_calloc(size_t n,size_t size){if(n==1&&size==sizeof(rw_reply)&&atomic_exchange(&fail_reply,0))return NULL;return __real_calloc(n,size);}
static unsigned fds(void){DIR *d=opendir("/proc/self/fd");CHECK(d);struct dirent *e;unsigned n=0;while((e=readdir(d)))if(e->d_name[0]!='.')++n;CHECK(!closedir(d));return n;}
static void parse_id(const char *s,uint8_t out[16]){CHECK(strlen(s)==32);for(size_t i=0;i<16;++i){unsigned n;CHECK(sscanf(s+2*i,"%2x",&n)==1);out[i]=(uint8_t)n;}}
static nx_sim_message receive(rw_worker *w){
    nx_sim_message m={0};for(unsigned i=0;i<100;++i){CHECK(rw_receive(w,&m)==NX_OK);if(m.ticket)return m;
        struct pollfd p={rw_wake_fd(w),POLLIN,0};CHECK(poll(&p,1,100)>=0);}
    CHECK(0);return m;
}
int main(int argc,char **argv){
    CHECK(argc==5);main_thread=pthread_self();unsigned before=fds();
    struct ae_catalogue_identity identity={.kind=!strcmp(argv[2],"system")?AE_CATALOGUE_SYSTEM:AE_CATALOGUE_USER};parse_id(argv[3],identity.authority_epoch);if(identity.kind==AE_CATALOGUE_USER)parse_id(argv[4],identity.owner);
    rw_worker *w=NULL;struct ae_catalogue_info context={0};int error=77;
    CHECK(rw_start(argv[1],&identity,&w,&context,&error)==NX_OK&&w&&error==0&&context.revision>0);
    CHECK(atomic_load(&have_worker)&&!atomic_load(&reads));
    CHECK((fcntl(rw_wake_fd(w),F_GETFD)&FD_CLOEXEC)&&(fcntl(rw_wake_fd(w),F_GETFL)&O_NONBLOCK));
    CHECK(rw_join(w)==NX_ERROR_STATE);
    rv_owner *owner=NULL;CHECK(rv_owner_create(&identity,&context,&owner)==NX_OK);
    nx_context *gui=NULL;CHECK(nx_context_create(NULL,&gui)==NX_OK);
    pthread_mutex_lock(&gate_lock);hold=1;entered=released=0;pthread_mutex_unlock(&gate_lock);
    uint64_t first=0,second=0,overflow=99;
    CHECK(rw_submit(w,(rw_key){11,1},&first)==NX_OK&&rv_owner_expect(owner,first)==NX_OK);
    pthread_mutex_lock(&gate_lock);while(!entered)pthread_cond_wait(&gate_changed,&gate_lock);pthread_mutex_unlock(&gate_lock);
    CHECK(rw_submit(w,(rw_key){12,2},&second)==NX_OK&&second>first&&rv_owner_expect(owner,second)==NX_OK);
    CHECK(rw_submit(w,(rw_key){13,3},&overflow)==NX_ERROR_LIMIT&&overflow==0);
    /* Original read transaction is still held; GUI teardown cannot touch it. */
    nx_context_destroy(gui);gui=NULL;CHECK(rw_stop(w)==NX_OK);
    CHECK(rw_submit(w,(rw_key){14,4},&overflow)==NX_ERROR_STATE&&overflow==0);
    pthread_mutex_lock(&gate_lock);released=1;pthread_cond_signal(&gate_changed);pthread_mutex_unlock(&gate_lock);
    CHECK(rw_join(w)==NX_OK&&atomic_load(&reads)==1&&atomic_load(&closes)==1);
    struct pollfd wake={rw_wake_fd(w),POLLIN,0};CHECK(poll(&wake,1,0)==1&&(wake.revents&POLLIN));
    nx_sim_message m=receive(w);rw_reply *reply=rw_reply_get(&m);CHECK(m.ticket==first&&reply&&reply->key.view==11&&reply->key.request==1&&reply->inventory&&reply->inventory->count>2);
    struct ae_recovery_inventory *kept=reply->inventory;CHECK(rv_owner_complete(owner,m.ticket,0,&reply->inventory)==NX_ERROR_STATE&&reply->inventory==kept);nx_sim_message_dispose(&m);
    /* Pipe was drained by receive; take the second completion without a poll. */
    CHECK(rw_receive(w,&m)==NX_OK&&m.ticket==second);reply=rw_reply_get(&m);CHECK(reply&&reply->original_error==-ECANCELED&&!reply->inventory&&reply->key.view==12);
    CHECK(rv_owner_complete(owner,second,reply->original_error,&reply->inventory)==NX_OK&&rv_owner_state(owner)==RV_UNAVAILABLE);nx_sim_message_dispose(&m);
    CHECK(rw_receive(w,&m)==NX_OK&&!m.ticket&&rw_destroy(&w)==NX_OK&&!w);rv_owner_destroy(&owner);
    CHECK(fds()==before);
    puts("RECOVERY_WORKER_SQL_TRANSACTION_GUI_RETIREMENT_TICKET_BOUND_CAPACITY_CANCEL_JOIN_AND_FDS_OK");
    atomic_store(&have_worker,0);CHECK(rw_start(argv[1],&identity,&w,&context,&error)==NX_OK&&rw_submit(w,(rw_key){21,1},&first)==NX_OK);
    m=receive(w);reply=rw_reply_get(&m);CHECK(reply&&reply->original_error==0&&reply->key.view==21);
    struct ae_recovery_inventory *inventory=NULL;CHECK(rw_reply_take(reply,&inventory)==NX_OK&&inventory);
    struct ae_recovery_inventory *again=NULL;CHECK(rw_reply_take(reply,&again)==NX_ERROR_STATE&&!again);nx_sim_message_dispose(&m);
    CHECK(rv_owner_create(&identity,&context,&owner)==NX_OK&&rv_owner_expect(owner,first)==NX_OK&&rv_owner_complete(owner,first,0,&inventory)==NX_OK&&!inventory);
    atomic_store(&inject_error,-EIO);CHECK(rw_submit(w,(rw_key){21,2},&second)==NX_OK&&rv_owner_expect(owner,second)==NX_OK);
    m=receive(w);reply=rw_reply_get(&m);CHECK(reply&&reply->original_error==-EIO&&!reply->inventory);
    CHECK(rv_owner_complete(owner,second,reply->original_error,&reply->inventory)==NX_OK&&rv_owner_error(owner)==-EIO);nx_sim_message_dispose(&m);
    rv_snapshot *snapshot=NULL;CHECK(rv_owner_snapshot(owner,&snapshot)==NX_OK&&rv_snapshot_data(snapshot)->count>2);rv_snapshot_release(&snapshot);
    atomic_store(&fail_reply,1);CHECK(rw_submit(w,(rw_key){21,3},&first)==NX_OK);
    m=receive(w);CHECK(m.ticket==first&&m.status==NX_ERROR_MEMORY&&!rw_reply_get(&m)&&!m.data);nx_sim_message_dispose(&m);
    CHECK(rw_submit(w,(rw_key){21,4},&first)==NX_OK);m=receive(w);CHECK(rw_reply_get(&m)&&rw_reply_get(&m)->inventory);nx_sim_message_dispose(&m);
    CHECK(rw_destroy(&w)==NX_OK&&!w);rv_owner_destroy(&owner);CHECK(fds()==before);
    struct ae_catalogue_identity wrong=identity;wrong.authority_epoch[15]^=1;atomic_store(&have_worker,0);
    struct ae_catalogue_info unchanged=context;CHECK(rw_start(argv[1],&wrong,&w,&context,&error)!=NX_OK&&!w&&error<0&&!memcmp(&context,&unchanged,sizeof context));CHECK(fds()==before);
    puts("RECOVERY_WORKER_ORIGINAL_THREAD_OWNERSHIP_ONCE_TRANSFER_READ_ERROR_OOM_RETRY_AND_SOURCE_REFUSAL_OK");return 0;
}
