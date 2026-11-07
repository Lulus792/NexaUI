#define _GNU_SOURCE
#include "picker.h"
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"picker-view:%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int receive(fp_worker *w,nx_sim_message *m) {
    for(unsigned i=0;i<100;++i) {
        if(fp_worker_receive(w,m)!=NX_OK)return 0;
        if(m->ticket)return 1;
        struct pollfd fd={fp_worker_wake_fd(w),POLLIN,0};if(poll(&fd,1,100)<0)return 0;
    }
    return 0;
}
static int frame(fp_picker *p,nx_context *ctx,float width,float height,fp_intent *intent) {
    for(unsigned i=0;i<8;++i) {
        if(nx_begin_frame(ctx,width,height,0)!=NX_OK)return 0;
        nx_semantic_desc root={0};root.id=1;root.role=NX_ROLE_WINDOW;root.name=(nx_utf8_span){"Picker",6};root.bounds=(nx_rect){0,0,width,height};int redraw;
        if(nx_semantics_add(ctx,&root,NULL)!=NX_OK||fp_picker_build(p,ctx,NULL,width,height,intent,&redraw)!=NX_OK||nx_end_frame(ctx)!=NX_OK)return 0;
        if(!redraw&&!fp_picker_reveal(p,ctx,height))return 1;
    }
    return 0;
}
int main(int argc,char **argv) {
    CHECK(argc==3);FILE *f=fopen(argv[1],"rb");CHECK(f&&!fseek(f,0,SEEK_END));long length=ftell(f);CHECK(length>0&&!fseek(f,0,SEEK_SET));
    unsigned char *bytes=malloc((size_t)length);CHECK(bytes&&fread(bytes,1,(size_t)length,f)==(size_t)length&&!fclose(f));
    nx_font *font=NULL;nx_font_collection *fonts=NULL;nx_context *ctx=NULL;fp_worker *worker=NULL;fp_picker p={0};
    CHECK(nx_font_open(bytes,(size_t)length,NULL,&font)==NX_OK&&nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    int root=open(argv[2],O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(root>=0&&fp_worker_start(root,&worker)==NX_OK);close(root);
    CHECK(fp_picker_init(&p,worker,fonts,77)==NX_OK&&nx_context_create(NULL,&ctx)==NX_OK);
    fp_intent intent={0};CHECK(frame(&p,ctx,720,580,&intent)&&!intent.requested);
    fp_command read={0};read.operation=FP_READ;CHECK(fp_picker_request(&p,&read)==NX_OK);
    nx_sim_message m={0};CHECK(receive(worker,&m));uint64_t ticket=m.ticket;m.ticket=ticket+1;CHECK(!fp_picker_observe(&p,&m)&&p.busy);m.ticket=ticket;
    fp_reply *r=fp_reply_get(&m);CHECK(r&&r->count==41);r->key.view=78;CHECK(!fp_picker_observe(&p,&m)&&p.busy);r->key.view=77;
    ++r->key.request;CHECK(!fp_picker_observe(&p,&m)&&p.busy);--r->key.request;
    CHECK(fp_picker_observe(&p,&m)&&!m.ticket&&!p.busy);nx_sim_message_dispose(&m);
    CHECK(frame(&p,ctx,720,580,&intent));r=fp_reply_get(&p.observation);CHECK(r&&r->count==41);
    nx_event end={0};end.type=NX_EVENT_KEY_DOWN;end.key=NX_KEY_END;
    CHECK(fp_picker_key(&p,FP_ROWS+r->rows[0].ordinal,&end,&intent)&&p.page==2&&p.selected==FP_ROWS+r->rows[40].ordinal);
    CHECK(frame(&p,ctx,720,580,&intent));uint32_t node;
    CHECK(nx_semantics_find(ctx,p.selected,&node)==NX_OK&&nx_focus_id(ctx)==p.selected);
    end.key=NX_KEY_ENTER;CHECK(fp_picker_key(&p,p.selected,&end,&intent)&&intent.requested&&intent.command.operation==FP_PICK&&intent.command.ordinal==r->rows[40].ordinal);
    intent=(fp_intent){0};p.german=1;p.text_size=32;p.expanded=1;
    CHECK(frame(&p,ctx,360,300,&intent));
    char oversized[257];memset(oversized,'a',256);oversized[256]=0;CHECK(nx_text_field_set_text(p.query,oversized,256)==NX_OK&&frame(&p,ctx,360,300,&intent));
    CHECK(nx_semantics_request_action(ctx,FP_APPLY,NX_ACTION_PRESS)==NX_ERROR_STATE&&!intent.requested);
    CHECK(nx_text_field_set_text(p.query,"snapshot-0039",13)==NX_OK&&frame(&p,ctx,360,300,&intent));
    CHECK(nx_semantics_request_action(ctx,FP_APPLY,NX_ACTION_PRESS)==NX_OK&&frame(&p,ctx,360,300,&intent));
    CHECK(intent.requested&&intent.command.operation==FP_FILTER&&intent.command.query_bytes==13&&!memcmp(intent.command.query,"snapshot-0039",13));
    CHECK(fp_picker_request(&p,&intent.command)==NX_OK);CHECK(receive(worker,&m)&&fp_picker_observe(&p,&m));nx_sim_message_dispose(&m);
    r=fp_reply_get(&p.observation);CHECK(r&&r->count==1&&r->rows[0].ordinal==40&&p.page==0&&!p.selected);
    intent=(fp_intent){0};CHECK(frame(&p,ctx,360,300,&intent));
    CHECK(nx_semantics_request_action(ctx,FP_ROWS+40,NX_ACTION_SELECT)==NX_OK&&frame(&p,ctx,360,300,&intent)&&p.selected==FP_ROWS+40);
    CHECK(nx_semantics_request_action(ctx,FP_OPEN,NX_ACTION_PRESS)==NX_OK&&frame(&p,ctx,360,300,&intent)&&intent.requested&&intent.command.operation==FP_PICK);
    /* Real complete escaped label and an over-height focused row remain readable. */
    int directory=open(argv[2],O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(directory>=0);
    char raw[256];memset(raw,0xff,255);raw[255]=0;
    int special=openat(directory,raw,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(special>=0);close(special);close(directory);
    fp_command clear={0};clear.operation=FP_FILTER;CHECK(fp_picker_request(&p,&clear)==NX_OK&&receive(worker,&m)&&fp_picker_observe(&p,&m));nx_sim_message_dispose(&m);
    clear.operation=FP_REFRESH;CHECK(fp_picker_request(&p,&clear)==NX_OK&&receive(worker,&m)&&fp_picker_observe(&p,&m));nx_sim_message_dispose(&m);
    r=fp_reply_get(&p.observation);CHECK(r&&r->count==42&&strlen(r->rows[41].entry.label)==1020);
    intent=(fp_intent){0};end.key=NX_KEY_END;CHECK(fp_picker_key(&p,FP_ROWS+r->rows[0].ordinal,&end,&intent)&&frame(&p,ctx,360,300,&intent));
    nx_semantic_view before,after;CHECK(nx_semantics_find(ctx,p.selected,&node)==NX_OK&&nx_semantics_get(ctx,node,&before)==NX_OK);
    CHECK(before.name.length==1020&&before.bounds.height>300&&before.bounds.y==8);
    p.scroll_y=1000000;CHECK(frame(&p,ctx,360,300,&intent)&&nx_semantics_find(ctx,p.selected,&node)==NX_OK&&nx_semantics_get(ctx,node,&after)==NX_OK&&after.bounds.y<before.bounds.y&&nx_focus_id(ctx)==p.selected);
    /* A malformed active completion fails visibly; it cannot replace good data. */
    uint64_t retained=r->state;clear.operation=FP_READ;CHECK(fp_picker_request(&p,&clear)==NX_OK&&receive(worker,&m));
    --m.bytes;CHECK(fp_picker_observe(&p,&m)&&p.error==NX_ERROR_ARGUMENT&&fp_picker_snapshot(&p)->state==retained);++m.bytes;nx_sim_message_dispose(&m);
    intent=(fp_intent){0};CHECK(frame(&p,ctx,360,300,&intent));
    CHECK(fp_picker_request(&p,&clear)==NX_OK&&receive(worker,&m)&&fp_picker_observe(&p,&m));nx_sim_message_dispose(&m);
    CHECK(p.error==NX_OK&&fp_picker_snapshot(&p)->count==42);
    /* Retry must repeat the failed original Enter, not refresh its retained parent. */
    directory=open(argv[2],O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(directory>=0&&mkdirat(directory,"large",0700)==0);
    int large=openat(directory,"large",O_RDONLY|O_DIRECTORY|O_CLOEXEC);close(directory);CHECK(large>=0);
    for(unsigned i=0;i<=AE_FILES_ENTRIES;++i){char leaf[32];snprintf(leaf,sizeof leaf,"item-%04u",i);special=openat(large,leaf,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(special>=0);close(special);}
    clear=(fp_command){0};clear.operation=FP_REFRESH;CHECK(fp_picker_request(&p,&clear)==NX_OK&&receive(worker,&m)&&fp_picker_observe(&p,&m));nx_sim_message_dispose(&m);
    r=fp_reply_get(&p.observation);CHECK(r&&r->count==43);size_t ordinal=SIZE_MAX;
    for(size_t i=0;i<r->count;++i)if(!strcmp(r->rows[i].entry.name,"large"))ordinal=r->rows[i].ordinal;
    CHECK(ordinal!=SIZE_MAX);clear.operation=FP_ENTER;clear.ordinal=ordinal;
    CHECK(fp_picker_request(&p,&clear)==NX_OK&&receive(worker,&m)&&fp_picker_observe(&p,&m));nx_sim_message_dispose(&m);
    CHECK(p.retryable&&fp_picker_snapshot(&p)->result==AE_FILES_LIMIT&&fp_picker_snapshot(&p)->depth==0&&fp_picker_snapshot(&p)->count==43);
    intent=(fp_intent){0};CHECK(frame(&p,ctx,720,580,&intent)&&nx_semantics_request_action(ctx,FP_RETRY,NX_ACTION_PRESS)==NX_OK&&frame(&p,ctx,720,580,&intent));
    CHECK(intent.requested&&intent.command.operation==FP_ENTER&&intent.command.ordinal==ordinal&&unlinkat(large,"item-4096",0)==0);
    CHECK(fp_picker_request(&p,&intent.command)==NX_OK&&receive(worker,&m)&&fp_picker_observe(&p,&m));nx_sim_message_dispose(&m);
    r=fp_reply_get(&p.observation);CHECK(r&&r->count==4096&&r->depth==1&&!p.retryable);
    intent=(fp_intent){0};end.key=NX_KEY_END;CHECK(fp_picker_key(&p,FP_ROWS+r->rows[0].ordinal,&end,&intent)&&frame(&p,ctx,720,580,&intent));
    uint64_t selection=p.selected,state=r->state;size_t page=p.page;
    CHECK(nx_text_field_set_text(p.query,"unapplied draft",15)==NX_OK);
    const float sizes[][2]={{360,300},{240,160},{64,64},{1,1},{360,300},{960,720}};
    for(size_t i=0;i<sizeof sizes/sizeof sizes[0];++i){
        intent=(fp_intent){0};if(i==1){intent.requested=1;intent.command.operation=FP_PICK;}
        if(fp_picker_prepare_size(&p,nx_focus_id(ctx),sizes[i][0],sizes[i][1],&intent)){nx_context_destroy(ctx);ctx=NULL;CHECK(nx_context_create(NULL,&ctx)==NX_OK);}
        CHECK(!intent.requested&&frame(&p,ctx,sizes[i][0],sizes[i][1],&intent));
        nx_utf8_span draft;CHECK(nx_text_field_text(p.query,&draft)==NX_OK&&draft.length==15&&!memcmp(draft.data,"unapplied draft",15));
        CHECK(p.page==page&&p.selected==selection&&fp_picker_snapshot(&p)->state==state&&nx_semantics_find(ctx,FP_CANCEL,&node)==NX_OK);
        CHECK(nx_semantics_get(ctx,node,&after)==NX_OK&&after.bounds.x>=0&&after.bounds.y>=0&&after.bounds.x+after.bounds.width<=sizes[i][0]&&after.bounds.y+after.bounds.height<=sizes[i][1]);
        if(p.compact)CHECK(nx_semantics_request_action(ctx,FP_OPEN,NX_ACTION_PRESS)==NX_ERROR_STATE&&nx_focus_id(ctx)==FP_CANCEL);
        else CHECK(nx_focus_id(ctx)==selection);
    }
    /* Preserve the previously usable narrow normal view instead of raising
       its minimum to make the compact checks easier. */
    p.text_size=16;intent=(fp_intent){0};
    if(fp_picker_prepare_size(&p,nx_focus_id(ctx),180,160,&intent)){nx_context_destroy(ctx);ctx=NULL;CHECK(nx_context_create(NULL,&ctx)==NX_OK);}
    CHECK(frame(&p,ctx,180,160,&intent)&&!p.compact&&nx_focus_id(ctx)==selection);p.text_size=32;
    /* An initial failed read has no prior folder to retain, and Retry is READ. */
    special=openat(large,"item-4096",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(special>=0);close(special);
    fp_worker *initial=NULL;fp_picker q={0};nx_context *initial_ctx=NULL;
    CHECK(fp_worker_start(large,&initial)==NX_OK&&fp_picker_init(&q,initial,fonts,88)==NX_OK&&nx_context_create(NULL,&initial_ctx)==NX_OK);
    clear=(fp_command){0};clear.operation=FP_READ;CHECK(fp_picker_request(&q,&clear)==NX_OK&&receive(initial,&m)&&fp_picker_observe(&q,&m));nx_sim_message_dispose(&m);
    CHECK(q.retryable&&fp_picker_snapshot(&q)->state==0&&fp_picker_snapshot(&q)->result==AE_FILES_LIMIT);
    intent=(fp_intent){0};CHECK(frame(&q,initial_ctx,720,580,&intent));
    const char *initial_error="Too many entries in this folder (maximum 4,096). No folder was loaded.";
    CHECK(nx_semantics_find(initial_ctx,FP_STATUS,&node)==NX_OK&&nx_semantics_get(initial_ctx,node,&after)==NX_OK&&after.name.length==strlen(initial_error)&&!memcmp(after.name.data,initial_error,after.name.length));
    CHECK(nx_semantics_request_action(initial_ctx,FP_RETRY,NX_ACTION_PRESS)==NX_OK&&frame(&q,initial_ctx,720,580,&intent)&&intent.command.operation==FP_READ&&unlinkat(large,"item-4096",0)==0);
    CHECK(fp_picker_request(&q,&intent.command)==NX_OK&&receive(initial,&m)&&fp_picker_observe(&q,&m));nx_sim_message_dispose(&m);
    CHECK(fp_picker_snapshot(&q)->count==4096&&fp_picker_snapshot(&q)->state==1&&!q.retryable);
    nx_context_destroy(initial_ctx);fp_picker_destroy(&q);CHECK(fp_worker_destroy(&initial)==NX_OK);
    /* The path limit is not a transient entry-count failure; do not offer an
       unchanged Enter as Retry at the original 64-level boundary. */
    CHECK(unlinkat(large,"item-4095",0)==0);int tree=fcntl(large,F_DUPFD_CLOEXEC,4);CHECK(tree>=0);
    for(unsigned d=2;d<=65;++d){CHECK(mkdirat(tree,"d",0700)==0);int next=openat(tree,"d",O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(next>=0);close(tree);tree=next;}close(tree);
    clear=(fp_command){0};clear.operation=FP_REFRESH;CHECK(fp_picker_request(&p,&clear)==NX_OK&&receive(worker,&m)&&fp_picker_observe(&p,&m));nx_sim_message_dispose(&m);
    for(unsigned d=2;d<=64;++d){clear.operation=FP_ENTER;clear.ordinal=0;CHECK(fp_picker_request(&p,&clear)==NX_OK&&receive(worker,&m)&&fp_picker_observe(&p,&m));nx_sim_message_dispose(&m);CHECK(fp_picker_snapshot(&p)->result==AE_FILES_OK&&fp_picker_snapshot(&p)->depth==d);}
    CHECK(fp_picker_request(&p,&clear)==NX_OK&&receive(worker,&m)&&fp_picker_observe(&p,&m));nx_sim_message_dispose(&m);
    CHECK(fp_picker_snapshot(&p)->result==AE_FILES_LIMIT&&fp_picker_snapshot(&p)->depth==64&&!p.retryable);
    intent=(fp_intent){0};CHECK(frame(&p,ctx,720,580,&intent)&&nx_semantics_request_action(ctx,FP_RETRY,NX_ACTION_PRESS)==NX_ERROR_STATE);
    const char *path_error="Ordnerpfad ist zu tief (höchstens 64 Ebenen). Vorheriger Ordner bleibt geöffnet.";
    CHECK(nx_semantics_find(ctx,FP_STATUS,&node)==NX_OK&&nx_semantics_get(ctx,node,&after)==NX_OK&&after.name.length==strlen(path_error)&&!memcmp(after.name.data,path_error,after.name.length));close(large);
    CHECK(nx_focus_id(ctx)==FP_RECOVER);p.scroll_y=0;
    CHECK(frame(&p,ctx,720,580,&intent)&&p.scroll_y==0&&nx_focus_id(ctx)==FP_RECOVER);
    nx_context_destroy(ctx);fp_picker_destroy(&p);CHECK(fp_worker_destroy(&worker)==NX_OK);
    nx_font_collection_destroy(fonts);nx_font_close(font);free(bytes);
    puts("PICKER_RETRY_ORIGINAL_ENTER_INITIAL_READ_AND_COMPACT_SIZE_MATRIX_VERIFIED");
    puts("PICKER_CORRELATION_PAGING_FILTER_AND_REALFONT_REFLOW_VERIFIED");return 0;
}
