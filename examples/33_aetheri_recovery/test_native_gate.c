/* Test-only host: the ordinary native entry is compiled verbatim below.
   No control pipe, SQL wrapper or retirement assertion enters recovery_native. */
#define main recovery_native_entry
#include "native.c"
#undef main
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sqlite3.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <unistd.h>
#define GATE_CHECK(x) do { if(!(x)){fprintf(stderr,"native-gate %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static pthread_t gui_thread,query_thread;
static unsigned calls;
static _Thread_local unsigned active_read;
static atomic_int entered,released,view_gone,window_gone,late_frees;
static _Atomic(void *) late_inventory;
static int gate_fd=-1;
static void original_thread(void){GATE_CHECK(!pthread_equal(pthread_self(),gui_thread)&&pthread_equal(pthread_self(),query_thread));}
int __real_ae_catalogue_open(const char *,const struct ae_catalogue_identity *,struct ae_catalogue **);
int __wrap_ae_catalogue_open(const char *path,const struct ae_catalogue_identity *id,struct ae_catalogue **out){
    query_thread=pthread_self();original_thread();return __real_ae_catalogue_open(path,id,out);
}
int __real_ae_catalogue_recovery_inventory(struct ae_catalogue *,size_t,struct ae_recovery_inventory **);
int __wrap_ae_catalogue_recovery_inventory(struct ae_catalogue *catalogue,size_t limit,struct ae_recovery_inventory **out){
    original_thread();active_read=++calls;int error=__real_ae_catalogue_recovery_inventory(catalogue,limit,out);
    if(active_read==2){GATE_CHECK(atomic_load(&entered)&&atomic_load(&released)&&!error&&*out);atomic_store(&late_inventory,*out);printf("HELD_QUERY_DONE ordinal=2 status=%d\n",error);}
    active_read=0;return error;
}
int __real_sqlite3_prepare_v2(sqlite3 *,const char *,int,sqlite3_stmt **,const char **);
int __wrap_sqlite3_prepare_v2(sqlite3 *db,const char *sql,int n,sqlite3_stmt **out,const char **tail){
    if(active_read==2&&strstr(sql,"SELECT o.operation_id")){
        original_thread();GATE_CHECK(!sqlite3_get_autocommit(db)&&!atomic_exchange(&entered,1));
        puts("HELD_QUERY_ENTERED ordinal=2 transaction=1");char command=0;ssize_t bytes;
        do {bytes=read(gate_fd,&command,1);} while(bytes<0&&errno==EINTR);
        GATE_CHECK(bytes==1&&command=='R'&&!close(gate_fd));gate_fd=-1;atomic_store(&released,1);
        printf("HELD_QUERY_RELEASED gui_retired=%d\n",atomic_load(&window_gone));
    }
    return __real_sqlite3_prepare_v2(db,sql,n,out,tail);
}
void __real_ae_recovery_inventory_free(struct ae_recovery_inventory **);
void __wrap_ae_recovery_inventory_free(struct ae_recovery_inventory **value){
    int late=value&&*value&&*value==atomic_load(&late_inventory);
    __real_ae_recovery_inventory_free(value);
    if(late){GATE_CHECK(pthread_equal(pthread_self(),gui_thread)&&atomic_load(&view_gone)&&atomic_load(&window_gone));atomic_fetch_add(&late_frees,1);puts("HELD_REPLY_FREED_ONCE_AFTER_GUI");}
}
int __real_ae_catalogue_close(struct ae_catalogue **);
int __wrap_ae_catalogue_close(struct ae_catalogue **catalogue){original_thread();int error=__real_ae_catalogue_close(catalogue);printf("ORIGINAL_WORKER_CLOSED status=%d\n",error);return error;}
nx_result __real_rvg_close(rvg_view **);
nx_result __wrap_rvg_close(rvg_view **view){nx_result r=__real_rvg_close(view);if(r==NX_OK&&!*view){atomic_store(&view_gone,1);puts("NATIVE_VIEW_RETIRED");}return r;}
void __real_nx_window_destroy(nx_window *);
void __wrap_nx_window_destroy(nx_window *window){GATE_CHECK(atomic_load(&view_gone));__real_nx_window_destroy(window);atomic_store(&window_gone,1);puts("NATIVE_WINDOW_RETIRED");}
nx_result __real_nx_window_poll_with_fd(nx_window *,uint32_t,int,uint32_t *);
nx_result __wrap_nx_window_poll_with_fd(nx_window *window,uint32_t timeout,int fd,uint32_t *ready){
    int held=atomic_load(&entered)&&!atomic_load(&released);
    if(held)printf("HELD_WAIT_ENTER fd=%d timeout=%" PRIu32 "\n",fd,timeout);
    nx_result r=__real_nx_window_poll_with_fd(window,timeout,fd,ready);
    if(held)printf("HELD_WAIT_RETURN ready=%" PRIu32 " status=%d\n",*ready,(int)r);
    return r;
}
static unsigned descriptor_count(void){DIR *d=opendir("/proc/self/fd");GATE_CHECK(d);unsigned n=0;struct dirent *e;while((e=readdir(d)))if(e->d_name[0]!='.')++n;GATE_CHECK(!closedir(d));return n;}
int main(int argc,char **argv){
    gui_thread=pthread_self();const char *input=getenv("RVG_TEST_GATE_FD");GATE_CHECK(input&&*input&&strspn(input,"0123456789")==strlen(input));
    char *end;long fd=strtol(input,&end,10);GATE_CHECK(!*end&&fd>=3&&fd<=INT_MAX);gate_fd=(int)fd;
    struct stat info;GATE_CHECK(!fstat(gate_fd,&info)&&S_ISFIFO(info.st_mode));unsigned before=descriptor_count();
    int result=recovery_native_entry(argc,argv);
    GATE_CHECK(!result&&calls==2&&atomic_load(&entered)&&atomic_load(&released)&&atomic_load(&late_frees)==1&&gate_fd==-1);
    unsigned after=descriptor_count();GATE_CHECK(after+1==before);
    printf("RECOVERY_NATIVE_HELD_ORIGINAL_QUERY_RETIREMENT_DISPOSAL_AND_FDS_OK before=%u after=%u\n",before,after);return 0;
}
