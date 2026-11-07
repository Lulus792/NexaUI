#define _GNU_SOURCE
#include "owner.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"picker-owner:%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int receive(fp_worker *w,nx_sim_message *m){
    for(unsigned i=0;i<100;++i){if(fp_worker_receive(w,m)!=NX_OK)return 0;if(m->ticket)return 1;struct pollfd fd={fp_worker_wake_fd(w),POLLIN,0};if(poll(&fd,1,100)<0)return 0;}return 0;
}
static int fresh(nx_context **ctx,fp_owner *o){nx_context_destroy(*ctx);*ctx=NULL;o->modal_built=0;return nx_context_create(NULL,ctx)==NX_OK;}
static int frame(fp_owner *o,fp_picker *p,nx_context *ctx,fp_intent *intent,int *mode){
    for(unsigned i=0;i<8;++i){
        if(nx_begin_frame(ctx,360,300,0)!=NX_OK)return 0;
        nx_semantic_desc root={0};root.id=1;root.role=NX_ROLE_WINDOW;root.bounds=(nx_rect){0,0,360,300};root.name=(nx_utf8_span){"Owner",5};
        int redraw,quit;if(nx_semantics_add(ctx,&root,NULL)!=NX_OK||fp_owner_build(o,p,ctx,NULL,360,300,intent,&redraw,mode,&quit)!=NX_OK||quit||nx_end_frame(ctx)!=NX_OK)return 0;
        if(!redraw&&!(o->active?fp_picker_reveal(p,ctx,300):fp_owner_reveal(o,ctx,300)))return 1;
    }return 0;
}
static int ask(fp_worker *w,fp_picker *p,fp_command *c){nx_sim_message m={0};if(fp_picker_request(p,c)!=NX_OK||!receive(w,&m)||!fp_picker_observe(p,&m)){nx_sim_message_dispose(&m);return 0;}nx_sim_message_dispose(&m);return 1;}
static size_t ordinal(const fp_reply *r,const char *name){for(size_t i=0;i<r->count;++i)if(!strcmp(r->rows[i].entry.name,name))return r->rows[i].ordinal;return SIZE_MAX;}
int main(int argc,char **argv){
    CHECK(argc==3);FILE *f=fopen(argv[1],"rb");CHECK(f&&!fseek(f,0,SEEK_END));long bytes=ftell(f);CHECK(bytes>0&&!fseek(f,0,SEEK_SET));
    unsigned char *data=malloc((size_t)bytes);CHECK(data&&fread(data,1,(size_t)bytes,f)==(size_t)bytes&&!fclose(f));
    nx_font *font=NULL;nx_font_collection *fonts=NULL;CHECK(nx_font_open(data,(size_t)bytes,NULL,&font)==NX_OK&&nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    int root=open(argv[2],O_DIRECTORY|O_CLOEXEC);fp_worker *worker=NULL;CHECK(root>=0&&fp_worker_start(root,&worker)==NX_OK);
    fp_owner o={0};fp_picker p={0};nx_context *ctx=NULL;CHECK(fp_owner_init(&o,fonts)==NX_OK&&nx_context_create(NULL,&ctx)==NX_OK);o.german=1;o.text_size=32;
    fp_intent intent={0};int mode=0;CHECK(frame(&o,&p,ctx,&intent,&mode)&&nx_focus_id(ctx)==FP_OWNER_FILE);
    CHECK(nx_text_field_set_text(o.note,"owner draft",11)==NX_OK);
    CHECK(fp_owner_open(&o,&p,worker,0)==NX_OK&&o.epoch==1&&fresh(&ctx,&o));fp_command c={0};c.operation=FP_READ;CHECK(ask(worker,&p,&c)&&fresh(&ctx,&o)&&frame(&o,&p,ctx,&intent,&mode));
    uint32_t index;nx_semantic_view view;CHECK(nx_semantics_find(ctx,FP_OWNER_MODAL,&index)==NX_OK&&nx_semantics_get(ctx,index,&view)==NX_OK&&view.role==NX_ROLE_DIALOG);
    CHECK(nx_semantics_find(ctx,FP_SCROLL,&index)==NX_OK&&nx_semantics_get(ctx,index,&view)==NX_OK&&view.parent_id==FP_OWNER_MODAL);
    CHECK(nx_focus_set(ctx,FP_OWNER_FILE)==NX_ERROR_ARGUMENT&&nx_semantics_request_action(ctx,FP_OWNER_FILE,NX_ACTION_PRESS)==NX_ERROR_STATE);
    CHECK(nx_semantics_request_text_value(ctx,FP_OWNER_NOTE,(nx_utf8_span){"blocked",7})==NX_ERROR_STATE);
    size_t file=ordinal(fp_picker_snapshot(&p),"snapshot-0000.txt");CHECK(file!=SIZE_MAX);c.operation=FP_PICK;c.ordinal=file;
    CHECK(fp_picker_request(&p,&c)==NX_OK&&fresh(&ctx,&o)&&frame(&o,&p,ctx,&intent,&mode));
    CHECK(fp_owner_finish(&o,&p,ctx,0)==NX_OK&&!o.active&&!o.selections&&fresh(&ctx,&o)&&frame(&o,&p,ctx,&intent,&mode)&&nx_focus_id(ctx)==FP_OWNER_FILE);
    nx_utf8_span note;CHECK(nx_text_field_text(o.note,&note)==NX_OK&&note.length==11&&!memcmp(note.data,"owner draft",11));
    CHECK(fp_owner_open(&o,&p,worker,0)==NX_OK&&o.epoch==2&&fresh(&ctx,&o));c=(fp_command){0};c.operation=FP_READ;CHECK(fp_picker_request(&p,&c)==NX_OK);
    nx_sim_message m={0};CHECK(receive(worker,&m));fp_reply *old=fp_reply_get(&m);CHECK(old&&old->key.view==1&&old->directory>=4&&old->witness>=4);
    int retired_fd=old->witness;CHECK(!fp_picker_observe(&p,&m)&&p.busy&&!o.selections);nx_sim_message_dispose(&m);CHECK(fcntl(retired_fd,F_GETFD)==-1&&errno==EBADF);
    CHECK(receive(worker,&m)&&fp_picker_observe(&p,&m));nx_sim_message_dispose(&m);CHECK(fresh(&ctx,&o)&&frame(&o,&p,ctx,&intent,&mode));
    c.operation=FP_PICK;c.ordinal=file;CHECK(fp_picker_request(&p,&c)==NX_OK&&fresh(&ctx,&o)&&frame(&o,&p,ctx,&intent,&mode));
    CHECK(receive(worker,&m)&&fp_picker_observe(&p,&m));nx_sim_message_dispose(&m);
    CHECK(fp_owner_finish(&o,&p,ctx,1)==NX_OK&&o.selections==1&&!o.directories&&!strcmp(o.label,"snapshot-0000.txt"));
    int previous_directory=o.directory,previous_witness=o.witness;struct stat expected,actual;CHECK(!fstatat(root,"snapshot-0000.txt",&expected,AT_SYMLINK_NOFOLLOW)&&!fstat(o.witness,&actual)&&actual.st_dev==expected.st_dev&&actual.st_ino==expected.st_ino);
    CHECK(fp_owner_finish(&o,&p,ctx,1)==NX_ERROR_STATE&&o.selections==1&&fresh(&ctx,&o)&&frame(&o,&p,ctx,&intent,&mode)&&nx_focus_id(ctx)==FP_OWNER_FILE);
    CHECK(fp_owner_open(&o,&p,worker,1)==NX_OK&&o.epoch==3&&fresh(&ctx,&o));c=(fp_command){0};c.operation=FP_READ;CHECK(ask(worker,&p,&c));
    size_t folder=ordinal(fp_picker_snapshot(&p),"folder");CHECK(folder!=SIZE_MAX);c.operation=FP_ENTER;c.ordinal=folder;CHECK(ask(worker,&p,&c)&&fp_picker_snapshot(&p)->count==0&&fresh(&ctx,&o)&&frame(&o,&p,ctx,&intent,&mode));
    intent=(fp_intent){0};CHECK(nx_semantics_request_action(ctx,FP_OPEN,NX_ACTION_PRESS)==NX_OK&&frame(&o,&p,ctx,&intent,&mode)&&intent.requested&&intent.command.operation==FP_PICK_DIRECTORY);
    CHECK(ask(worker,&p,&intent.command)&&fp_owner_finish(&o,&p,ctx,1)==NX_OK&&o.selections==2&&o.directories&&!strcmp(o.label,"folder"));
    CHECK(fcntl(previous_directory,F_GETFD)==-1&&errno==EBADF&&fcntl(previous_witness,F_GETFD)==-1&&errno==EBADF);
    CHECK(!fstat(o.witness,&actual)&&S_ISDIR(actual.st_mode)&&fresh(&ctx,&o));intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent,&mode)&&nx_focus_id(ctx)==FP_OWNER_DIRECTORY);
    CHECK(nx_text_field_text(o.note,&note)==NX_OK&&note.length==11&&!memcmp(note.data,"owner draft",11));
    o.epoch=UINT64_MAX;CHECK(fp_owner_open(&o,&p,worker,0)==NX_ERROR_LIMIT&&!o.active&&o.selections==2);
    int final_directory=o.directory,final_witness=o.witness;nx_context_destroy(ctx);fp_owner_destroy(&o);CHECK(fcntl(final_directory,F_GETFD)==-1&&errno==EBADF&&fcntl(final_witness,F_GETFD)==-1&&errno==EBADF);
    fp_picker_destroy(&p);CHECK(fp_worker_destroy(&worker)==NX_OK);close(root);nx_font_collection_destroy(fonts);nx_font_close(font);free(data);
    puts("PICKER_OWNER_MODAL_BLOCKING_CANCEL_LATE_PICK_EPOCH_RESULT_REPLACEMENT_AND_FD_LIFETIME_VERIFIED");return 0;
}
