#include "intent.h"
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void files_intent_init(files_intent *intent) {
    if(intent){memset(intent,0,sizeof(*intent));intent->directory=intent->witness=-1;}
}
void files_intent_clear(files_intent *intent) {
    if(!intent)return;
    if(intent->directory>=0)close(intent->directory);
    if(intent->witness>=0)close(intent->witness);
    files_intent_init(intent);
}
static int same_fd(int a,int b) {
    struct stat x,y;
    return !fstat(a,&x) && !fstat(b,&y) && x.st_dev==y.st_dev && x.st_ino==y.st_ino &&
        ((S_ISREG(x.st_mode) && S_ISREG(y.st_mode)) || (S_ISDIR(x.st_mode) && S_ISDIR(y.st_mode)));
}
enum ae_files_result files_intent_stage(files_intent *intent,struct ae_files *model,int query_accepted) {
    if(!intent || !model || query_accepted!=1)return AE_FILES_INVALID;
    files_intent next;files_intent_init(&next);
    enum ae_files_result r=ae_files_open_selected(model,&next.directory,&next.witness,next.leaf);
    if(r!=AE_FILES_OK){files_intent_clear(&next);return r;}
    files_intent_clear(intent);*intent=next;return AE_FILES_OK;
}
enum ae_files_result files_intent_resolve(const files_intent *intent,struct ae_files *model,
    int query_accepted,int *directory,int *witness,char leaf[AE_FILES_NAME+1]) {
    if(directory)*directory=-1;
    if(witness)*witness=-1;
    if(leaf)leaf[0]=0;
    if(!directory || !witness || directory==witness || !leaf)return AE_FILES_INVALID;
    if(!intent || !model || query_accepted!=1 || intent->directory<0 || intent->witness<0 || !intent->leaf[0])
        return AE_FILES_INVALID;
    int current=ae_files_directory(model);
    if(current<0)return AE_FILES_IO;
    int same=same_fd(intent->directory,current);close(current);
    if(!same)return AE_FILES_CHANGED;
    struct stat original;
    if(fstat(intent->witness,&original))return AE_FILES_IO;
    size_t count=ae_files_count(model),index=count,selected=ae_files_selected(model);
    for(size_t i=0;i<count;++i) {
        const struct ae_files_entry *entry=ae_files_entry(model,i);
        if(entry && entry->kind==AE_FILES_REGULAR && entry->device==original.st_dev &&
           entry->inode==original.st_ino && !strcmp(entry->name,intent->leaf)){index=i;break;}
    }
    if(index==count || !ae_files_select(model,index))return AE_FILES_CHANGED;
    int parent=-1,pin=-1;char selected_leaf[AE_FILES_NAME+1]={0};
    enum ae_files_result result=ae_files_open_selected(model,&parent,&pin,selected_leaf);
    if(selected<count)(void)ae_files_select(model,selected);
    if(result==AE_FILES_OK && (!same_fd(parent,intent->directory) ||
       !same_fd(pin,intent->witness) || strcmp(selected_leaf,intent->leaf)))result=AE_FILES_CHANGED;
    if(result!=AE_FILES_OK){if(parent>=0)close(parent);if(pin>=0)close(pin);return result;}
    *directory=parent;*witness=pin;memcpy(leaf,selected_leaf,strlen(selected_leaf)+1);return AE_FILES_OK;
}
