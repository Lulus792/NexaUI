#define _GNU_SOURCE
#include "worker.h"
#include "files/create.h"
#include "editor/file.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"picker-save:%d: %s\n",__LINE__,#x); return 1; } } while (0)
static pthread_t ui_thread;
static atomic_uint wrong_thread, validations, directories, selected_calls;
static atomic_int fail_directory;
static uint64_t request, state;
static void note(void) { if (pthread_equal(pthread_self(),ui_thread)) atomic_fetch_add(&wrong_thread,1); }
bool __real_ae_files_create_name(const char *,size_t);
bool __wrap_ae_files_create_name(const char *s,size_t n) {
    note();atomic_fetch_add(&validations,1);return __real_ae_files_create_name(s,n);
}
int __real_ae_files_directory(const struct ae_files *);
int __wrap_ae_files_directory(const struct ae_files *f) {
    note();atomic_fetch_add(&directories,1);
    if (atomic_exchange(&fail_directory,0)) { errno=EMFILE;return -1; }
    return __real_ae_files_directory(f);
}
enum ae_files_result __real_ae_files_open_selected(const struct ae_files *,int *,int *,char *);
enum ae_files_result __wrap_ae_files_open_selected(const struct ae_files *f,int *d,int *w,char *s) {
    note();atomic_fetch_add(&selected_calls,1);return __real_ae_files_open_selected(f,d,w,s);
}
static unsigned fds(void) {
    DIR *d=opendir("/proc/self/fd");if (!d) return 0;
    unsigned n=0;while (readdir(d)) ++n;closedir(d);return n;
}
static int file_at(int d,const char *name,const char *text) {
    int fd=openat(d,name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
    if (fd<0) return 0;
    size_t n=strlen(text);ssize_t wrote=write(fd,text,n);int closed=close(fd);
    return wrote==(ssize_t)n && !closed;
}
static int content(int d,const char *name,const char *text) {
    int fd=openat(d,name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if (fd<0) return 0;
    char bytes[32];ssize_t n=read(fd,bytes,sizeof bytes);int closed=close(fd);
    return n==(ssize_t)strlen(text) && !memcmp(bytes,text,strlen(text)) && !closed;
}
static int closed(int fd) { errno=0;return fcntl(fd,F_GETFD)==-1 && errno==EBADF; }
static fp_command command(fp_operation op) {
    fp_command c={0};c.key=(fp_key){7,++request};c.operation=op;c.expected_state=state;return c;
}
static fp_command named(const char *name,size_t n) {
    fp_command c=command(FP_SAVE_NAME);c.name_bytes=n;
    if (n<=sizeof c.name) memcpy(c.name,name,n);
    return c;
}
static int receive(fp_worker *w,uint64_t ticket,nx_sim_message *m) {
    for (unsigned i=0;i<50;++i) {
        if (fp_worker_receive(w,m)!=NX_OK) return 0;
        if (m->ticket) {
            fp_reply *r=fp_reply_get(m);
            if (m->ticket!=ticket || !r) return 0;
            state=r->state;return 1;
        }
        struct pollfd p={fp_worker_wake_fd(w),POLLIN,0};
        if (poll(&p,1,100)<0 && errno!=EINTR) return 0;
    }
    return 0;
}
static int ask(fp_worker *w,const fp_command *c,nx_sim_message *m) {
    uint64_t ticket;
    return fp_worker_submit(w,c,&ticket)==NX_OK && receive(w,ticket,m) &&
        fp_reply_get(m)->key.view==c->key.view && fp_reply_get(m)->key.request==c->key.request;
}
static size_t ordinal(const fp_reply *r,const char *name) {
    for (size_t i=0;i<r->count;++i) if (!strcmp(r->rows[i].entry.name,name)) return r->rows[i].ordinal;
    return SIZE_MAX;
}
static int target_flags(const fp_save_target *t,int parent) {
    struct stat actual,expected;
    return t->directory>=4 && (fcntl(t->directory,F_GETFD)&FD_CLOEXEC) &&
        !fstat(parent,&expected) && !fstat(t->directory,&actual) &&
        actual.st_dev==expected.st_dev && actual.st_ino==expected.st_ino &&
        (t->kind==FP_SAVE_NEW ? t->witness==-1 : t->witness>=4 && t->witness!=t->directory &&
            (fcntl(t->witness,F_GETFD)&FD_CLOEXEC) && (fcntl(t->witness,F_GETFL)&O_PATH));
}
static void release(fp_save_target *t) {
    if (t->directory>=0) close(t->directory);
    if (t->witness>=0 && t->witness!=t->directory) close(t->witness);
    *t=(fp_save_target){.directory=-1,.witness=-1};
}
static int wrong_take(fp_reply *r) {
    int d=99,w=99;char leaf[AE_FILES_NAME+1];fp_pick batch;size_t count=99;
    return fp_reply_take(r,&d,&w,leaf)==NX_ERROR_STATE && d==-1 && w==-1 && !*leaf &&
        fp_reply_take_many(r,&batch,1,&count)==NX_ERROR_STATE && !count;
}
int main(int argc,char **argv) {
    CHECK(argc==2);ui_thread=pthread_self();unsigned before=fds();CHECK(before);
    int base=open(argv[1],O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(base>=0 && !mkdirat(base,"scope",0700));
    int root=openat(base,"scope",O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(root>=0);
    char raw[256];memset(raw,0xff,255);raw[255]=0;
    CHECK(file_at(root,"existing.txt","seed") && file_at(root,raw,"raw") &&
        !mkdirat(root,"folder",0700) && !mkfifoat(root,"pipe",0600) &&
        !symlinkat("existing.txt",root,"link") && !symlinkat("missing",root,"dangling"));
    fp_worker *worker=NULL;CHECK(fp_worker_start(root,&worker)==NX_OK);
    nx_sim_message m={0};fp_command c=command(FP_SAVE_NAME);c.name_bytes=1;c.name[0]='x';
    c.expected_state=1;CHECK(ask(worker,&c,&m));fp_reply *r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_CHANGED && !r->state && r->directory==-1 && r->save_kind==FP_SAVE_NONE);
    nx_sim_message_dispose(&m);

    c=command(FP_READ);CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_OK && state==1);
    size_t raw_ordinal=ordinal(r,raw),folder_ordinal=ordinal(r,"folder"),existing_ordinal=ordinal(r,"existing.txt");
    CHECK(raw_ordinal!=SIZE_MAX && folder_ordinal!=SIZE_MAX && existing_ordinal!=SIZE_MAX);
    nx_sim_message_dispose(&m);
    c=named("copied.txt",10);uint64_t copied_request=c.key.request,ticket=0;
    CHECK(fp_worker_submit(worker,&c,&ticket)==NX_OK);
    memset(c.name,'z',sizeof c.name);c.key.request=UINT64_MAX;c.name_bytes=AE_FILES_NAME;
    CHECK(receive(worker,ticket,&m));r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_OK && r->save_kind==FP_SAVE_NEW && !strcmp(r->leaf,"copied.txt") && r->key.request==copied_request);
    nx_sim_message_dispose(&m);

    const struct { const char *s;size_t n; } invalid[]={
        {"",0},{".",1},{"..",2},{"a/b",3},{"a\0b",3},{"a\nb",3},{"\x7f",1},{"\xc2\x80",2},
        {"\xe2\x80\x8b",3},{"\xe2\x80\xae",3},{"\xe2\x81\xa6",3},{"\xff",1},{"\xc3",1}};
    for (size_t i=0;i<sizeof invalid/sizeof invalid[0];++i) {
        unsigned held=fds();c=named(invalid[i].s,invalid[i].n);CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
        CHECK(r->result==AE_FILES_INVALID && r->save_kind==FP_SAVE_NONE && r->directory==-1 && r->witness==-1);
        fp_save_target t;CHECK(fp_reply_take_save(r,&t)==NX_ERROR_STATE && t.kind==FP_SAVE_NONE && t.directory==-1 && t.witness==-1);
        nx_sim_message_dispose(&m);CHECK(fds()==held);
    }
    char maximum[257];memset(maximum,'a',256);maximum[256]=0;
    c=named(maximum,256);ticket=99;
    CHECK(fp_worker_submit(worker,&c,&ticket)==NX_ERROR_ARGUMENT && !ticket);
    c=named(maximum,255);CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_OK && r->save_kind==FP_SAVE_NEW && strlen(r->leaf)==255 && wrong_take(r));
    int abandoned=r->directory;nx_sim_message_dispose(&m);CHECK(closed(abandoned));
    for (size_t i=0;i<127;++i) { maximum[2*i]=(char)0xc3;maximum[2*i+1]=(char)0xa9; } maximum[254]='x';
    c=named(maximum,255);CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_OK && r->save_kind==FP_SAVE_NEW && !memcmp(r->leaf,maximum,255));
    nx_sim_message_dispose(&m);

    unsigned held=fds();atomic_store(&fail_directory,1);c=named("failed.txt",10);CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_IO && r->directory==-1 && r->witness==-1 && r->save_kind==FP_SAVE_NONE);
    nx_sim_message_dispose(&m);CHECK(fds()==held);
    CHECK(!fchmod(root,0000));c=named("denied.txt",10);CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_IO && r->save_kind==FP_SAVE_NONE && r->directory==-1 && r->witness==-1);
    nx_sim_message_dispose(&m);CHECK(!fchmod(root,0700) && fds()==held);
    const char *unsupported[]={"folder","pipe","link","dangling"};
    for (size_t i=0;i<sizeof unsupported/sizeof unsupported[0];++i) {
        c=named(unsupported[i],strlen(unsupported[i]));CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
        CHECK(r->result==AE_FILES_UNSUPPORTED && r->directory==-1 && r->witness==-1 && r->save_kind==FP_SAVE_NONE);
        nx_sim_message_dispose(&m);CHECK(fds()==held);
    }
    puts("SAVE_ORIGINAL_NAME_RULES_BYTE_LIMIT_NOFOLLOW_IO_REFUSAL_AND_DISPOSAL_VERIFIED");

    /* Typed destination lookup is not a row selection: it sees names admitted
       after the snapshot and names outside the current filter/hidden view. */
    CHECK(file_at(root,".late.txt","late"));c=command(FP_FILTER);strcpy(c.query,"nothing");c.query_bytes=7;
    CHECK(ask(worker,&c,&m) && !fp_reply_get(&m)->count);nx_sim_message_dispose(&m);
    c=named(".late.txt",9);CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_OK && r->save_kind==FP_SAVE_EXISTING && !r->count && wrong_take(r));
    fp_save_target t={0};CHECK(fp_reply_take_save(r,&t)==NX_OK && target_flags(&t,root) && !strcmp(t.leaf,".late.txt"));
    fp_save_target again;CHECK(fp_reply_take_save(r,&again)==NX_ERROR_STATE && again.directory==-1 && again.witness==-1);
    nx_sim_message_dispose(&m);char byte;CHECK(pread(t.witness,&byte,1,0)==-1 && errno==EBADF);release(&t);
    c=command(FP_FILTER);CHECK(ask(worker,&c,&m));nx_sim_message_dispose(&m);
    unsigned checked=atomic_load(&validations);c=command(FP_SAVE_SELECTED);c.ordinal=raw_ordinal;
    CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_OK && r->save_kind==FP_SAVE_EXISTING && !strcmp(r->leaf,raw) && strlen(r->save_label)==1020 &&
        atomic_load(&validations)==checked && wrong_take(r));
    CHECK(fp_reply_take_save(r,&t)==NX_OK && target_flags(&t,root) && !strcmp(t.leaf,raw) && strlen(t.label)==1020);
    nx_sim_message_dispose(&m);
    struct ae_editor_file *file=NULL;struct ae_document *document=NULL;
    CHECK(ae_editor_file_open_selected(t.directory,t.leaf,t.witness,&file,&document)==AE_EDITOR_FILE_OK);
    char bytes[8];CHECK(ae_document_copy(document,0,3,bytes,sizeof bytes) && !memcmp(bytes,"raw",3));
    ae_document_destroy(document);ae_editor_file_close(file);file=NULL;document=NULL;release(&t);
    c=named("\\xFF",4);CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_OK && r->save_kind==FP_SAVE_NEW && !strcmp(r->leaf,"\\xFF") && strcmp(r->leaf,raw));nx_sim_message_dispose(&m);
    puts("SAVE_TYPED_FILTER_INDEPENDENCE_RAW_SELECTED_BYTES_AND_ONCE_TRANSFER_VERIFIED");

    c=named("existing.txt",12);CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_OK && fp_reply_take_save(r,&t)==NX_OK && t.kind==FP_SAVE_EXISTING);nx_sim_message_dispose(&m);
    CHECK(!renameat(root,"existing.txt",root,"old.txt") && file_at(root,"existing.txt","replacement"));
    CHECK(ae_editor_file_open_selected(t.directory,t.leaf,t.witness,&file,&document)==AE_EDITOR_FILE_CONFLICT && !file && !document);
    CHECK(content(root,"old.txt","seed") && content(root,"existing.txt","replacement"));release(&t);
    c=command(FP_SAVE_SELECTED);c.ordinal=existing_ordinal;CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_CHANGED && r->save_kind==FP_SAVE_NONE && r->directory==-1 && r->witness==-1);nx_sim_message_dispose(&m);
    c=named("raced.txt",9);CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_OK && fp_reply_take_save(r,&t)==NX_OK && t.kind==FP_SAVE_NEW);nx_sim_message_dispose(&m);
    CHECK(file_at(root,"raced.txt","other"));
    CHECK(ae_editor_file_new(t.directory,t.leaf,&file,&document)==AE_EDITOR_FILE_CONFLICT && !file && !document && content(root,"raced.txt","other"));release(&t);
    uint64_t old_state=state;checked=atomic_load(&validations);c=command(FP_HIDDEN);c.show_hidden=1;
    CHECK(ask(worker,&c,&m));nx_sim_message_dispose(&m);uint64_t current=state;
    c=named("stale.txt",9);c.expected_state=old_state;CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_CHANGED && state==current && r->directory==-1 && r->save_kind==FP_SAVE_NONE && atomic_load(&validations)==checked);
    nx_sim_message_dispose(&m);
    puts("SAVE_REPLACED_WITNESS_CREATE_ONLY_RACE_AND_STALE_REQUEST_REFUSAL_VERIFIED");

    c=named("new.txt",7);CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_OK && fp_reply_take_save(r,&t)==NX_OK && t.kind==FP_SAVE_NEW && target_flags(&t,root));nx_sim_message_dispose(&m);
    struct stat st;CHECK(fstatat(root,"new.txt",&st,AT_SYMLINK_NOFOLLOW)==-1 && errno==ENOENT);
    CHECK(!renameat(base,"scope",base,"moved") && !mkdirat(base,"scope",0700));
    int decoy=openat(base,"scope",O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(decoy>=0);
    c=command(FP_ENTER);c.ordinal=folder_ordinal;CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);
    CHECK(r->result==AE_FILES_OK && r->depth==1 && target_flags(&t,root));nx_sim_message_dispose(&m);
    CHECK(ae_editor_file_new(t.directory,t.leaf,&file,&document)==AE_EDITOR_FILE_OK);
    CHECK(ae_document_replace(document,0,0,"saved",5)==AE_DOCUMENT_OK && ae_editor_file_save(file,document)==AE_EDITOR_FILE_OK);
    struct ae_document_info info;CHECK(ae_document_info(document,&info) && !info.dirty && !ae_editor_file_uncertain(file));
    CHECK(content(root,"new.txt","saved") && fstatat(decoy,"new.txt",&st,AT_SYMLINK_NOFOLLOW)==-1 && errno==ENOENT);
    ae_document_destroy(document);ae_editor_file_close(file);int owned=t.directory;release(&t);CHECK(closed(owned));
    c=named("abandoned.txt",13);CHECK(ask(worker,&c,&m));r=fp_reply_get(&m);CHECK(r->result==AE_FILES_OK && r->save_kind==FP_SAVE_NEW);
    abandoned=r->directory;nx_sim_message_dispose(&m);CHECK(closed(abandoned));
    c=named("queued.txt",10);CHECK(fp_worker_submit(worker,&c,&ticket)==NX_OK);
    struct pollfd wake={fp_worker_wake_fd(worker),POLLIN,0};
    CHECK(poll(&wake,1,5000)==1 && (wake.revents&POLLIN)); /* Leave completion ownership with the channel. */
    CHECK(fp_worker_stop(worker)==NX_OK && fp_worker_join(worker)==NX_OK && fp_worker_destroy(&worker)==NX_OK && !worker);
    close(decoy);close(root);close(base);
    CHECK(!atomic_load(&wrong_thread) && atomic_load(&validations)>=20 && atomic_load(&directories)>=10 && atomic_load(&selected_calls)>=2 && fds()==before);
    puts("SAVE_HELD_PARENT_NAVIGATION_ORIGINAL_APP_WRITE_AND_ALL_FDS_CLOSED_VERIFIED");
    return 0;
}
