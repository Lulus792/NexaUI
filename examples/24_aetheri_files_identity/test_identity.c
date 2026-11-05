#define _POSIX_C_SOURCE 200809L
#include "intent.h"
#include <nexaui/nexaui.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"files-identity:%d: %s\n",__LINE__,#x);goto cleanup;}} while(0)
static size_t find(struct ae_files *model,const char *name) {
    for(size_t i=0;i<ae_files_count(model);++i) {
        const struct ae_files_entry *e=ae_files_entry(model,i);
        if(e && !strcmp(e->name,name))return i;
    }
    return SIZE_MAX;
}
static int make_file(int directory,const char *name) {
    int fd=openat(directory,name,O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600);
    return fd>=0 && !close(fd);
}
static int visible_row(struct ae_files *model,size_t index) {
    nx_virtual_list *list=NULL;nx_virtual_range range={0};
    if(nx_virtual_list_create(ae_files_count(model),20,NULL,&list)!=NX_OK)return 0;
    nx_result r=nx_virtual_list_range(list,(double)index*20,20,0,&range);
    int okay=r==NX_OK && range.first<=index && index<range.end && ae_files_entry(model,index)!=NULL;
    nx_virtual_list_destroy(list);return okay;
}
int main(void) {
    int okay=0,created=0,root=-1,parent=-1,pin=-1;char path[]="/tmp/nexaui-files-identity-XXXXXX";
    char leaf[AE_FILES_NAME+1]={0};struct ae_files *model=NULL;files_intent intent;files_intent_init(&intent);
    CHECK(mkdtemp(path));created=1;root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(root>=0);
    CHECK(make_file(root,"a") && make_file(root,"b") && make_file(root,"z"));
    CHECK(linkat(root,"b",root,"b-link",0)==0 && mkdirat(root,"folder",0700)==0);
    CHECK(make_file(root,"odd\x1bname"));
    enum ae_files_result r;model=ae_files_create(root,&r);CHECK(model && r==AE_FILES_OK);
    size_t b=find(model,"b");CHECK(b!=SIZE_MAX && visible_row(model,b));
    const struct ae_files_entry *escaped=ae_files_entry(model,find(model,"odd\x1bname"));
    CHECK(escaped && !strcmp(escaped->label,"odd\\x1bname"));
    CHECK(ae_files_select(model,b) && files_intent_stage(&intent,model,1)==AE_FILES_OK);
    int directory_flags=fcntl(intent.directory,F_GETFD),witness_flags=fcntl(intent.witness,F_GETFD);
    CHECK(directory_flags>=0 && witness_flags>=0 && (directory_flags&FD_CLOEXEC) && (witness_flags&FD_CLOEXEC));
    parent=pin=-2;
    CHECK(files_intent_resolve(&intent,model,1,&parent,&parent,leaf)==AE_FILES_INVALID && parent==-1 && !leaf[0]);
    CHECK(files_intent_resolve(&intent,model,1,NULL,&pin,leaf)==AE_FILES_INVALID && pin==-1 && !leaf[0]);
    CHECK(ae_files_filter(model,"b",1));CHECK(files_intent_resolve(&intent,model,1,&parent,&pin,leaf)==AE_FILES_OK);
    CHECK(!strcmp(leaf,"b") && parent>=0 && pin>=0);close(parent);close(pin);parent=pin=-1;
    char oversized[AE_FILES_QUERY+1];memset(oversized,'x',sizeof(oversized));
    CHECK(!ae_files_filter(model,oversized,sizeof(oversized)));
    CHECK(files_intent_resolve(&intent,model,0,&parent,&pin,leaf)==AE_FILES_INVALID && parent==-1 && pin==-1 && !leaf[0]);
    CHECK(files_intent_stage(&intent,model,0)==AE_FILES_INVALID);
    CHECK(ae_files_filter(model,"a",1));
    CHECK(files_intent_resolve(&intent,model,1,&parent,&pin,leaf)==AE_FILES_CHANGED && parent==-1 && pin==-1 && !leaf[0]);
    CHECK(ae_files_filter(model,"",0));CHECK(make_file(root,"aa") && ae_files_refresh(model)==AE_FILES_OK);
    size_t a=find(model,"a");b=find(model,"b");CHECK(a!=SIZE_MAX && b!=SIZE_MAX && visible_row(model,b));
    CHECK(ae_files_select(model,a));
    CHECK(files_intent_resolve(&intent,model,1,&parent,&pin,leaf)==AE_FILES_OK);
    CHECK(!strcmp(leaf,"b") && ae_files_selected(model)==a);close(parent);close(pin);parent=pin=-1;
    size_t folder=find(model,"folder");CHECK(folder!=SIZE_MAX && ae_files_select(model,folder));
    CHECK(ae_files_enter(model)==AE_FILES_OK);
    CHECK(files_intent_resolve(&intent,model,1,&parent,&pin,leaf)==AE_FILES_CHANGED && parent==-1 && pin==-1);
    CHECK(ae_files_up(model)==AE_FILES_OK);
    CHECK(unlinkat(root,"b",0)==0 && make_file(root,"b"));
    CHECK(files_intent_resolve(&intent,model,1,&parent,&pin,leaf)==AE_FILES_CHANGED && parent==-1 && pin==-1);
    CHECK(ae_files_refresh(model)==AE_FILES_OK);
    CHECK(files_intent_resolve(&intent,model,1,&parent,&pin,leaf)==AE_FILES_CHANGED && parent==-1 && pin==-1);
    int old_directory=intent.directory,old_witness=intent.witness;files_intent_clear(&intent);
    errno=0;CHECK(fcntl(old_directory,F_GETFD)<0 && errno==EBADF);
    errno=0;CHECK(fcntl(old_witness,F_GETFD)<0 && errno==EBADF);
    okay=1;
cleanup:
    if(parent>=0)close(parent);
    if(pin>=0)close(pin);
    files_intent_clear(&intent);if(model)ae_files_destroy(model);
    if(root>=0){(void)unlinkat(root,"a",0);(void)unlinkat(root,"aa",0);(void)unlinkat(root,"b",0);
        (void)unlinkat(root,"b-link",0);(void)unlinkat(root,"z",0);(void)unlinkat(root,"odd\x1bname",0);
        (void)unlinkat(root,"folder",AT_REMOVEDIR);close(root);}
    if(created)(void)rmdir(path);
    if(okay)puts("Original Files model: held identity survives reorder and rejects stale or hidden targets");
    return okay?0:1;
}
