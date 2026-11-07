#define _GNU_SOURCE
#include "owner.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"picker-multiple:%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int frame(fp_owner *o,fp_picker *p,nx_context *ctx,fp_intent *intent){
    for(unsigned i=0;i<8;++i){nx_semantic_desc root={0};root.id=1;root.role=NX_ROLE_WINDOW;root.bounds=(nx_rect){0,0,360,300};root.name=(nx_utf8_span){"Owner",5};
        int redraw,mode,quit,changed;if(nx_begin_frame(ctx,360,300,0)!=NX_OK||nx_semantics_add(ctx,&root,NULL)!=NX_OK||
            fp_owner_build(o,p,ctx,NULL,360,300,intent,&redraw,&mode,&quit)!=NX_OK||mode||quit||nx_end_frame(ctx)!=NX_OK||fp_picker_selection_sync(p,&changed)!=NX_OK)return 0;
        if(!redraw&&!changed&&!(o->active?fp_picker_reveal(p,ctx,300):fp_owner_reveal(o,ctx,300)))return 1;
    }return 0;
}
static int fresh(nx_context **ctx,fp_owner *o){nx_context_destroy(*ctx);*ctx=NULL;o->modal_built=0;o->rebind=0;return nx_context_create(NULL,ctx)==NX_OK;}
static int ask(fp_picker *p,fp_command *c){
    if(fp_picker_request(p,c)!=NX_OK)return 0;
    for(unsigned i=0;i<100;++i){nx_sim_message m={0};if(fp_worker_receive(p->worker,&m)!=NX_OK)return 0;
        if(m.ticket){int accepted=fp_picker_observe(p,&m);nx_sim_message_dispose(&m);return accepted;}
        struct pollfd fd={fp_worker_wake_fd(p->worker),POLLIN,0};if(poll(&fd,1,100)<0)return 0;
    }return 0;
}
static int key(fp_picker *p,nx_context *ctx,nx_key code,uint32_t mods,fp_intent *intent){nx_event e={0};e.type=NX_EVENT_KEY_DOWN;e.key=code;e.modifiers=mods;return fp_picker_key(p,nx_focus_id(ctx),&e,intent);}
static int view(nx_context *ctx,uint64_t id,nx_semantic_view *out){uint32_t at;return nx_semantics_find(ctx,id,&at)==NX_OK&&nx_semantics_get(ctx,at,out)==NX_OK;}
static unsigned descriptors(void){DIR *d=opendir("/proc/self/fd");if(!d)return 0;unsigned n=0;while(readdir(d))++n;closedir(d);return n;}
int main(int argc,char **argv){
    CHECK(argc==3);FILE *f=fopen(argv[1],"rb");CHECK(f&&!fseek(f,0,SEEK_END));long bytes=ftell(f);CHECK(bytes>0&&!fseek(f,0,SEEK_SET));
    unsigned char *data=malloc((size_t)bytes);CHECK(data&&fread(data,1,(size_t)bytes,f)==(size_t)bytes&&!fclose(f));
    nx_font *font=NULL;nx_font_collection *fonts=NULL;CHECK(nx_font_open(data,(size_t)bytes,NULL,&font)==NX_OK&&nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    int root=open(argv[2],O_DIRECTORY|O_CLOEXEC);CHECK(root>=0);unsigned before=descriptors();fp_worker *worker=NULL;CHECK(fp_worker_start(root,&worker)==NX_OK);
    fp_owner o={0};fp_picker p={0};nx_context *ctx=NULL;fp_intent intent={0};CHECK(fp_owner_init(&o,fonts)==NX_OK&&fresh(&ctx,&o));o.german=1;o.text_size=32;
    CHECK(nx_text_field_set_text(o.note,"multi note",10)==NX_OK&&fp_owner_open(&o,&p,worker,2)==NX_OK);
    fp_command c={0};c.operation=FP_READ;CHECK(ask(&p,&c)&&frame(&o,&p,ctx,&intent));
    nx_selection_view set;nx_semantic_view sem;CHECK(p.multiple&&nx_selection_model_get(p.selection,&set)==NX_OK&&set.item_count==40&&!set.selected_count);
    CHECK(nx_semantics_request_action(ctx,FP_ROWS,NX_ACTION_PRESS)==NX_OK&&frame(&o,&p,ctx,&intent)&&intent.requested&&intent.command.operation==FP_ENTER&&ask(&p,&intent.command)&&fresh(&ctx,&o));intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent));
    CHECK(nx_selection_model_get(p.selection,&set)==NX_OK&&!set.item_count&&!set.selected_count);
    CHECK(nx_semantics_request_action(ctx,FP_UP_BUTTON,NX_ACTION_PRESS)==NX_OK&&frame(&o,&p,ctx,&intent)&&intent.requested&&intent.command.operation==FP_UP&&ask(&p,&intent.command)&&fresh(&ctx,&o));intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent));
    CHECK(nx_selection_model_get(p.selection,&set)==NX_OK&&set.item_count==40&&!set.selected_count);
    CHECK(view(ctx,FP_LIST,&sem)&&(sem.state&NX_SEM_SELECTION_MULTIPLE));uint64_t first=FP_ROWS+1,last=FP_ROWS+40;
    CHECK(nx_focus_set(ctx,first)==NX_OK&&key(&p,ctx,NX_KEY_SPACE,0,&intent)&&frame(&o,&p,ctx,&intent));
    CHECK(key(&p,ctx,NX_KEY_END,0,&intent)&&frame(&o,&p,ctx,&intent)&&nx_focus_id(ctx)==last&&p.page==2);
    CHECK(key(&p,ctx,NX_KEY_SPACE,0,&intent)&&frame(&o,&p,ctx,&intent)&&nx_selection_model_get(p.selection,&set)==NX_OK&&set.selected_count==2);
    CHECK(view(ctx,first,&sem)&&(sem.state&NX_SEM_OFFSCREEN)&&(sem.state&NX_SEM_SELECTED));
    CHECK(key(&p,ctx,NX_KEY_HOME,NX_MOD_SHIFT,&intent)&&frame(&o,&p,ctx,&intent)&&key(&p,ctx,NX_KEY_DOWN,NX_MOD_SHIFT,&intent)&&frame(&o,&p,ctx,&intent));
    CHECK(nx_selection_model_get(p.selection,&set)==NX_OK&&set.selected_count==40);
    CHECK(key(&p,ctx,NX_KEY_END,0,&intent)&&frame(&o,&p,ctx,&intent)&&key(&p,ctx,NX_KEY_SPACE,0,&intent)&&frame(&o,&p,ctx,&intent));
    CHECK(nx_selection_model_get(p.selection,&set)==NX_OK&&set.selected_count==39&&key(&p,ctx,NX_KEY_A,NX_MOD_CONTROL,&intent)&&frame(&o,&p,ctx,&intent));
    CHECK(nx_selection_model_get(p.selection,&set)==NX_OK&&set.selected_count==40);
    CHECK(nx_semantics_request_action(ctx,first,NX_ACTION_DESELECT)==NX_OK&&frame(&o,&p,ctx,&intent));
    CHECK(nx_selection_model_get(p.selection,&set)==NX_OK&&set.selected_count==39);
    CHECK(nx_semantics_request_action(ctx,last,NX_ACTION_SELECT)==NX_OK&&frame(&o,&p,ctx,&intent)&&nx_semantics_request_action(ctx,first,NX_ACTION_ADD_SELECTION)==NX_OK&&frame(&o,&p,ctx,&intent));
    CHECK(nx_selection_model_get(p.selection,&set)==NX_OK&&set.selected_count==2&&set.selected_ids[0]==first&&set.selected_ids[1]==last);
    CHECK(nx_semantics_request_action(ctx,FP_OPEN,NX_ACTION_PRESS)==NX_OK&&frame(&o,&p,ctx,&intent)&&intent.requested&&intent.command.operation==FP_PICK_MANY);
    CHECK(ask(&p,&intent.command)&&fp_owner_finish(&o,&p,ctx,1)==NX_OK&&o.batch_count==2&&o.selections==1&&fresh(&ctx,&o));intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent)&&nx_focus_id(ctx)==FP_OWNER_MULTIPLE);
    CHECK(!strcmp(fp_owner_batch_row(&o,0)->entry.name,"snapshot-0000.txt")&&!strcmp(fp_owner_batch_row(&o,1)->entry.name,"snapshot-0039.txt"));
    int old[4]={o.batch[0].directory,o.batch[0].witness,o.batch[1].directory,o.batch[1].witness};struct stat actual,expected;
    for(size_t i=0;i<2;++i){CHECK(!fstat(o.batch[i].witness,&actual)&&!fstatat(root,fp_owner_batch_row(&o,i)->entry.name,&expected,AT_SYMLINK_NOFOLLOW)&&actual.st_dev==expected.st_dev&&actual.st_ino==expected.st_ino);}
    CHECK(fp_owner_open(&o,&p,worker,0)==NX_OK&&fresh(&ctx,&o));c=(fp_command){0};c.operation=FP_READ;CHECK(ask(&p,&c)&&frame(&o,&p,ctx,&intent)&&fp_owner_finish(&o,&p,ctx,0)==NX_OK&&o.batch_count==2&&o.selections==1);
    for(size_t i=0;i<4;++i)CHECK(fcntl(old[i],F_GETFD)>=0);
    CHECK(fresh(&ctx,&o)&&fp_owner_open(&o,&p,worker,0)==NX_OK);c=(fp_command){0};c.operation=FP_READ;CHECK(ask(&p,&c));c.operation=FP_PICK;c.ordinal=1;
    CHECK(ask(&p,&c)&&frame(&o,&p,ctx,&intent)&&fp_owner_finish(&o,&p,ctx,1)==NX_OK&&!o.batch_count&&o.selections==2);
    for(size_t i=0;i<4;++i)CHECK(fcntl(old[i],F_GETFD)==-1&&errno==EBADF);
    CHECK(fresh(&ctx,&o)&&fp_owner_open(&o,&p,worker,2)==NX_OK);c=(fp_command){0};c.operation=FP_READ;CHECK(ask(&p,&c)&&frame(&o,&p,ctx,&intent)&&nx_focus_set(ctx,first)==NX_OK);
    CHECK(key(&p,ctx,NX_KEY_A,NX_MOD_CONTROL,&intent)&&frame(&o,&p,ctx,&intent)&&nx_semantics_request_action(ctx,FP_OPEN,NX_ACTION_PRESS)==NX_OK&&frame(&o,&p,ctx,&intent)&&intent.requested);
    int prior_directory=o.directory,prior_witness=o.witness;CHECK(ask(&p,&intent.command)&&fp_owner_finish(&o,&p,ctx,1)==NX_OK&&o.batch_count==40&&o.selections==3&&fresh(&ctx,&o));
    CHECK(fcntl(prior_directory,F_GETFD)==-1&&errno==EBADF&&fcntl(prior_witness,F_GETFD)==-1&&errno==EBADF);intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent));
    CHECK(nx_semantics_request_action(ctx,FP_OWNER_NEXT,NX_ACTION_PRESS)==NX_OK&&frame(&o,&p,ctx,&intent)&&o.batch_page==1&&o.rebind&&fresh(&ctx,&o)&&frame(&o,&p,ctx,&intent));
    CHECK(!view(ctx,FP_OWNER_BATCH_ROWS,&sem)&&view(ctx,FP_OWNER_BATCH_ROWS+16,&sem)&&sem.name.length==17&&!memcmp(sem.name.data,"snapshot-0016.txt",17));
    CHECK(nx_semantics_request_action(ctx,FP_OWNER_NEXT,NX_ACTION_PRESS)==NX_OK&&frame(&o,&p,ctx,&intent)&&o.batch_page==2&&fresh(&ctx,&o)&&frame(&o,&p,ctx,&intent)&&nx_focus_id(ctx)==FP_OWNER_PREVIOUS);
    nx_utf8_span note;CHECK(nx_text_field_text(o.note,&note)==NX_OK&&note.length==10&&!memcmp(note.data,"multi note",10));
    int final[80];for(size_t i=0;i<40;++i){final[2*i]=o.batch[i].directory;final[2*i+1]=o.batch[i].witness;}
    nx_context_destroy(ctx);fp_owner_destroy(&o);for(size_t i=0;i<80;++i)CHECK(fcntl(final[i],F_GETFD)==-1&&errno==EBADF);
    fp_picker_destroy(&p);CHECK(fp_worker_destroy(&worker)==NX_OK&&descriptors()==before);close(root);nx_font_collection_destroy(fonts);nx_font_close(font);free(data);
    puts("PICKER_MULTIPLE_LOGICAL_OFFSCREEN_RANGE_NATIVE_ACTION_SET_OWNER_PAGES_CANCEL_REPLACEMENT_AND_FDS_VERIFIED");return 0;
}
