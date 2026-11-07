#define _GNU_SOURCE
#include "owner.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"picker-save-view:%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int receive(fp_worker *w,nx_sim_message *m){
    for(unsigned i=0;i<50;++i){if(fp_worker_receive(w,m)!=NX_OK)return 0;if(m->ticket)return 1;struct pollfd fd={fp_worker_wake_fd(w),POLLIN,0};if(poll(&fd,1,100)<0)return 0;}return 0;
}
static int ask(fp_worker *w,fp_picker *p,fp_command *c){nx_sim_message m={0};if(fp_picker_request(p,c)!=NX_OK||!receive(w,&m)||!fp_picker_observe(p,&m)){nx_sim_message_dispose(&m);return 0;}nx_sim_message_dispose(&m);return 1;}
static int fresh(nx_context **ctx,fp_owner *o){nx_context_destroy(*ctx);*ctx=NULL;o->modal_built=0;return nx_context_create(NULL,ctx)==NX_OK;}
static int frame_size(fp_owner *o,fp_picker *p,nx_context *ctx,fp_intent *intent,float width,float height){
    for(unsigned i=0;i<10;++i){
        if(nx_begin_frame(ctx,width,height,0)!=NX_OK)return 0;
        nx_semantic_desc root={0};root.id=1;root.role=NX_ROLE_WINDOW;root.bounds=(nx_rect){0,0,width,height};root.name=(nx_utf8_span){"Owner",5};
        int redraw,mode,quit;if(nx_semantics_add(ctx,&root,NULL)!=NX_OK||fp_owner_build(o,p,ctx,NULL,width,height,intent,&redraw,&mode,&quit)!=NX_OK||mode||quit||nx_end_frame(ctx)!=NX_OK)return 0;
        if(!redraw&&!(o->active?fp_picker_reveal(p,ctx,height):fp_owner_reveal(o,ctx,height)))return 1;
    }return 0;
}
static int frame(fp_owner *o,fp_picker *p,nx_context *ctx,fp_intent *intent){return frame_size(o,p,ctx,intent,360,300);}
static int semantic(nx_context *ctx,uint64_t id,nx_semantic_view *out){uint32_t i;return nx_semantics_find(ctx,id,&i)==NX_OK&&nx_semantics_get(ctx,i,out)==NX_OK;}
static int press(fp_owner *o,fp_picker *p,nx_context *ctx,uint64_t id,fp_intent *intent){
    *intent=(fp_intent){0};return nx_semantics_request_action(ctx,id,NX_ACTION_PRESS)==NX_OK&&frame(o,p,ctx,intent);
}
static int edit(fp_owner *o,fp_picker *p,nx_context *ctx,const char *s,fp_intent *intent){
    *intent=(fp_intent){0};return nx_semantics_request_text_value(ctx,FP_FILENAME,(nx_utf8_span){s,strlen(s)})==NX_OK&&frame(o,p,ctx,intent);
}
static size_t ordinal(const fp_reply *r,const char *name){for(size_t i=0;i<r->count;++i)if(!strcmp(r->rows[i].entry.name,name))return r->rows[i].ordinal;return SIZE_MAX;}
static int closed(int fd){errno=0;return fcntl(fd,F_GETFD)==-1&&errno==EBADF;}
int main(int argc,char **argv){
    CHECK(argc==3);FILE *f=fopen(argv[1],"rb");CHECK(f&&!fseek(f,0,SEEK_END));long n=ftell(f);CHECK(n>0&&!fseek(f,0,SEEK_SET));
    unsigned char *bytes=malloc((size_t)n);CHECK(bytes&&fread(bytes,1,(size_t)n,f)==(size_t)n&&!fclose(f));
    nx_font *font=NULL;nx_font_collection *fonts=NULL;CHECK(nx_font_open(bytes,(size_t)n,NULL,&font)==NX_OK&&nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    int root=open(argv[2],O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(root>=0);
    char raw[256];memset(raw,0xff,255);raw[255]=0;int fd=openat(root,raw,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(fd>=0&&!close(fd));
    fp_worker *w=NULL;CHECK(fp_worker_start(root,&w)==NX_OK);fp_owner o={0};fp_picker p={0};nx_context *ctx=NULL;
    CHECK(fp_owner_init(&o,fonts)==NX_OK&&nx_context_create(NULL,&ctx)==NX_OK);o.german=1;o.text_size=32;
    fp_intent intent={0};CHECK(frame(&o,&p,ctx,&intent)&&nx_text_field_set_text(o.note,"preserved",9)==NX_OK);
    CHECK(fp_owner_open(&o,&p,w,3)==NX_OK&&fresh(&ctx,&o));fp_command c={0};c.operation=FP_READ;
    CHECK(ask(w,&p,&c)&&fresh(&ctx,&o)&&frame(&o,&p,ctx,&intent));nx_semantic_view node;
    CHECK(semantic(ctx,FP_FILENAME,&node)&&node.role==NX_ROLE_TEXT_FIELD&&semantic(ctx,FP_OPEN,&node)&&(node.state&NX_SEM_DISABLED));
    CHECK(edit(&o,&p,ctx,"a/b",&intent)&&semantic(ctx,FP_FILENAME,&node)&&(node.state&NX_SEM_INVALID)&&semantic(ctx,FP_OPEN,&node)&&(node.state&NX_SEM_DISABLED));
    CHECK(edit(&o,&p,ctx,"newé.txt",&intent)&&semantic(ctx,FP_OPEN,&node)&&!(node.state&NX_SEM_DISABLED));
    /* Enter belongs to the filename field and copies exact UTF-8 bytes. */
    CHECK(nx_focus_set(ctx,FP_FILENAME)==NX_OK);nx_event enter={0};enter.type=NX_EVENT_KEY_DOWN;enter.key=NX_KEY_ENTER;
    CHECK(nx_context_queue_event(ctx,&enter)==NX_OK);intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent));
    CHECK(intent.requested&&intent.command.operation==FP_SAVE_NAME&&intent.command.name_bytes==9&&!memcmp(intent.command.name,"newé.txt",9));
    CHECK(ask(w,&p,&intent.command)&&fp_picker_save_ready(&p)&&!p.save_review);
    c=(fp_command){0};c.operation=FP_READ;CHECK(fp_picker_request(&p,&c)==NX_ERROR_STATE&&fp_owner_finish(&o,&p,ctx,1)==NX_OK);
    CHECK(o.save_kind==FP_SAVE_NEW&&o.directory>=4&&o.witness==-1&&!strcmp(o.leaf,"newé.txt")&&o.selections==1);
    struct stat st;CHECK(fstatat(root,"newé.txt",&st,AT_SYMLINK_NOFOLLOW)==-1&&errno==ENOENT);
    int original=o.directory;CHECK(fresh(&ctx,&o));intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent)&&nx_focus_id(ctx)==FP_OWNER_SAVE);
    puts("SAVE_VIEW_TYPED_VALIDATION_ENTER_NEW_TARGET_NO_WRITE_AND_OWNER_FOCUS_VERIFIED");

    CHECK(fp_owner_open(&o,&p,w,3)==NX_OK&&fresh(&ctx,&o));c=(fp_command){0};c.operation=FP_READ;
    CHECK(ask(w,&p,&c)&&fresh(&ctx,&o)&&frame(&o,&p,ctx,&intent));size_t item=ordinal(fp_picker_snapshot(&p),"snapshot-0000.txt");CHECK(item!=SIZE_MAX);
    CHECK(edit(&o,&p,ctx,"snapshot-0000.txt",&intent)&&press(&o,&p,ctx,FP_OPEN,&intent)&&intent.requested&&ask(w,&p,&intent.command));
    CHECK(p.save_review&&!p.save_approved&&!fp_picker_save_ready(&p)&&fp_owner_finish(&o,&p,ctx,1)==NX_ERROR_STATE&&o.selections==1&&o.directory==original);
    c=(fp_command){0};c.operation=FP_READ;CHECK(fp_picker_request(&p,&c)==NX_ERROR_STATE);
    CHECK(fresh(&ctx,&o));intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent)&&nx_focus_id(ctx)==FP_SAVE_BACK);
    CHECK(semantic(ctx,FP_SAVE_REVIEW,&node)&&node.role==NX_ROLE_DIALOG&&semantic(ctx,FP_SAVE_LINES+1,&node)&&node.name.length==17&&!memcmp(node.name.data,"snapshot-0000.txt",17));
    CHECK(semantic(ctx,FP_RESIZE,&node)&&node.parent_id==FP_SAVE_REVIEW);
    CHECK(nx_semantics_request_text_value(ctx,FP_OWNER_NOTE,(nx_utf8_span){"blocked",7})==NX_ERROR_STATE);
    int review_directory=fp_picker_snapshot(&p)->directory,review_witness=fp_picker_snapshot(&p)->witness;
    CHECK(press(&o,&p,ctx,FP_SAVE_BACK,&intent)&&intent.save_back&&fp_picker_save_back(&p)==NX_OK&&closed(review_directory)&&closed(review_witness));
    p.rebind=0;CHECK(fresh(&ctx,&o));intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent)&&nx_focus_id(ctx)==FP_FILENAME&&!p.save_review);
    nx_utf8_span text;CHECK(nx_text_field_text(p.filename,&text)==NX_OK&&text.length==17&&!memcmp(text.data,"snapshot-0000.txt",17));
    CHECK(press(&o,&p,ctx,FP_OPEN,&intent)&&intent.requested&&ask(w,&p,&intent.command)&&fresh(&ctx,&o));intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent));
    CHECK(press(&o,&p,ctx,FP_SAVE_CONFIRM,&intent)&&intent.save_accept&&fp_picker_save_confirm(&p,ctx)==NX_OK&&fp_picker_save_ready(&p));
    CHECK(fp_owner_finish(&o,&p,ctx,1)==NX_OK&&o.selections==2&&o.save_kind==FP_SAVE_EXISTING&&o.witness>=4&&closed(original));
    CHECK(!strcmp(o.leaf,"snapshot-0000.txt")&&fresh(&ctx,&o));intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent));
    int previous_directory=o.directory,previous_witness=o.witness;
    puts("SAVE_VIEW_EXPLICIT_OVERWRITE_DEFAULT_BACK_CANCEL_FD_RELEASE_AND_ONCE_ADOPTION_VERIFIED");

    CHECK(fp_owner_open(&o,&p,w,3)==NX_OK&&fresh(&ctx,&o));c=(fp_command){0};c.operation=FP_READ;
    CHECK(ask(w,&p,&c)&&fresh(&ctx,&o)&&frame(&o,&p,ctx,&intent));size_t raw_item=ordinal(fp_picker_snapshot(&p),raw);CHECK(raw_item!=SIZE_MAX);
    /* Logical End moves to the raw maximum-name row and keeps labels out of the editor. */
    nx_event end={0};end.type=NX_EVENT_KEY_DOWN;end.key=NX_KEY_END;CHECK(fp_picker_key(&p,FP_ROWS+item,&end,&intent));
    CHECK(p.save_binding==FP_ROWS+raw_item&&p.rebind);p.rebind=0;CHECK(fresh(&ctx,&o));intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent));
    size_t bound_page=p.page;
    CHECK(nx_text_field_text(p.filename,&text)==NX_OK&&!text.length&&semantic(ctx,FP_SAVE_CHOSEN,&node)&&node.name.length==1020);
    CHECK(press(&o,&p,ctx,FP_OPEN,&intent)&&intent.requested&&intent.command.operation==FP_SAVE_SELECTED&&intent.command.ordinal==raw_item&&ask(w,&p,&intent.command)&&fresh(&ctx,&o));
    intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent)&&semantic(ctx,FP_SAVE_LINES+1,&node)&&node.name.length==1020&&p.page==bound_page);
    nx_semantic_view area;CHECK(semantic(ctx,FP_SAVE_SCROLL,&area)&&node.bounds.height>area.bounds.height&&nx_focus_id(ctx)==FP_SAVE_BACK);
    unsigned pages=(unsigned)ceilf(2*(node.bounds.y+node.bounds.height-area.bounds.y-area.bounds.height)/area.bounds.height);
    CHECK(pages>1&&pages<256);
    for(unsigned i=0;i<pages;++i){p.review_scroll+=area.bounds.height*0.5f;CHECK(frame(&o,&p,ctx,&intent)&&nx_focus_id(ctx)==FP_SAVE_BACK);}
    CHECK(semantic(ctx,FP_SAVE_LINES+1,&node)&&node.bounds.y+node.bounds.height<=area.bounds.y+area.bounds.height&&node.bounds.y+node.bounds.height>area.bounds.y+area.bounds.height/2);
    CHECK(fp_picker_prepare_size(&p,nx_focus_id(ctx),64,64,&intent)&&fresh(&ctx,&o));intent=(fp_intent){0};
    CHECK(frame_size(&o,&p,ctx,&intent,64,64)&&p.compact&&p.save_review&&fp_picker_save_confirm(&p,ctx)==NX_ERROR_STATE);
    CHECK(nx_semantics_request_action(ctx,FP_CANCEL,NX_ACTION_PRESS)==NX_OK);intent=(fp_intent){0};CHECK(frame_size(&o,&p,ctx,&intent,64,64)&&intent.save_back&&!intent.cancel);
    CHECK(fp_picker_save_back(&p)==NX_OK&&p.page==bound_page&&p.save_binding==FP_ROWS+raw_item);p.rebind=0;CHECK(fresh(&ctx,&o));intent=(fp_intent){0};CHECK(frame_size(&o,&p,ctx,&intent,64,64));
    CHECK(fp_picker_prepare_size(&p,nx_focus_id(ctx),360,300,&intent)&&fresh(&ctx,&o));intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent)&&nx_focus_id(ctx)==FP_FILENAME);
    CHECK(edit(&o,&p,ctx,"\\xff",&intent)&&!p.save_binding&&press(&o,&p,ctx,FP_OPEN,&intent)&&intent.command.operation==FP_SAVE_NAME&&intent.command.name_bytes==4&&!memcmp(intent.command.name,"\\xff",4));
    /* Cancelling keeps the previous accepted owner result and note. */
    CHECK(fp_owner_finish(&o,&p,ctx,0)==NX_OK&&o.directory==previous_directory&&o.witness==previous_witness&&o.selections==2&&fresh(&ctx,&o));intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent)&&nx_focus_id(ctx)==FP_OWNER_SAVE);
    CHECK(nx_text_field_text(o.note,&text)==NX_OK&&text.length==9&&!memcmp(text.data,"preserved",9));
    CHECK(fp_owner_open(&o,&p,w,3)==NX_OK&&fresh(&ctx,&o));c=(fp_command){0};c.operation=FP_READ;
    CHECK(ask(w,&p,&c)&&fresh(&ctx,&o));intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent));
    CHECK(edit(&o,&p,ctx,"snapshot-0001.txt",&intent)&&press(&o,&p,ctx,FP_OPEN,&intent)&&ask(w,&p,&intent.command)&&fresh(&ctx,&o));
    intent=(fp_intent){0};CHECK(frame(&o,&p,ctx,&intent)&&p.save_review);
    review_directory=fp_picker_snapshot(&p)->directory;review_witness=fp_picker_snapshot(&p)->witness;
    CHECK(fp_owner_finish(&o,&p,ctx,0)==NX_OK&&closed(review_directory)&&closed(review_witness)&&o.selections==2&&fresh(&ctx,&o));
    CHECK(fp_owner_open(&o,&p,w,3)==NX_OK);c=(fp_command){0};c.operation=FP_READ;CHECK(ask(w,&p,&c));
    uint64_t retired_view=p.view;c.operation=FP_SAVE_SELECTED;c.ordinal=item;CHECK(fp_picker_request(&p,&c)==NX_OK&&fp_owner_finish(&o,&p,ctx,0)==NX_OK&&fresh(&ctx,&o));
    CHECK(fp_owner_open(&o,&p,w,3)==NX_OK);c=(fp_command){0};c.operation=FP_READ;CHECK(fp_picker_request(&p,&c)==NX_OK);
    nx_sim_message m={0};CHECK(receive(w,&m));fp_reply *old=fp_reply_get(&m);
    CHECK(old&&old->key.view==retired_view&&old->save_kind==FP_SAVE_EXISTING&&!fp_picker_observe(&p,&m)&&p.busy);
    review_directory=old->directory;review_witness=old->witness;nx_sim_message_dispose(&m);CHECK(closed(review_directory)&&closed(review_witness));
    CHECK(receive(w,&m)&&fp_picker_observe(&p,&m));nx_sim_message_dispose(&m);
    CHECK(fp_owner_finish(&o,&p,ctx,0)==NX_OK&&o.directory==previous_directory&&o.witness==previous_witness&&o.selections==2);
    nx_context_destroy(ctx);fp_owner_destroy(&o);CHECK(closed(previous_directory)&&closed(previous_witness));fp_picker_destroy(&p);CHECK(fp_worker_destroy(&w)==NX_OK);
    close(root);nx_font_collection_destroy(fonts);nx_font_close(font);free(bytes);
    puts("SAVE_VIEW_RAW_NAME_REVIEW_READING_LITERAL_EDIT_CANCEL_RETIRED_REPLY_AND_FINAL_FDS_VERIFIED");return 0;
}
