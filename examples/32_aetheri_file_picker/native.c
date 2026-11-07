#define _GNU_SOURCE
#include "owner.h"
#include <nexaui/software.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int context(nx_context **ctx,int theme) {
    nx_theme t;
    return nx_context_create(NULL,ctx)==NX_OK && nx_theme_preset((nx_theme_kind)theme,&t)==NX_OK &&
        nx_context_set_theme(*ctx,&t)==NX_OK && nx_context_set_reduced_motion(*ctx,1)==NX_OK;
}
static int replace_context(nx_window *window,nx_context **ctx,int theme,fp_owner *owner) {
    if(nx_window_publish_semantics(window,NULL)!=NX_OK)return 0;
    owner->modal_built=0;
    nx_context_destroy(*ctx);*ctx=NULL;return context(ctx,theme);
}
static int command_boundary(const nx_event *event) {
    if((event->type==NX_EVENT_KEY_DOWN||event->type==NX_EVENT_KEY_UP)&&
       (event->key==NX_KEY_CONTROL||event->key==NX_KEY_SHIFT||event->key==NX_KEY_ALT||event->key==NX_KEY_SUPER))return 1;
    if(event->type!=NX_EVENT_KEY_DOWN)return 0;
    if(event->key==NX_KEY_ESCAPE)return 1;
    if(event->key==NX_KEY_SPACE||(event->key==NX_KEY_A&&event->modifiers==NX_MOD_CONTROL))return 1;
    if(event->modifiers&~(NX_MOD_SHIFT|NX_MOD_CONTROL))return 0;
    return event->key==NX_KEY_HOME||event->key==NX_KEY_END||event->key==NX_KEY_UP||event->key==NX_KEY_DOWN||
        event->key==NX_KEY_ENTER||event->key==NX_KEY_PAGE_UP||event->key==NX_KEY_PAGE_DOWN;
}
static int take_save(fp_picker *p,int *directory,int *witness){
    fp_save_target t;if(!fp_picker_save_ready(p)||fp_reply_take_save(fp_reply_get(&p->observation),&t)!=NX_OK)return 0;
    *directory=t.directory;*witness=t.witness;
    printf("SAVE_TARGET kind=%d leaf_hex=",(int)t.kind);for(size_t i=0;i<strlen(t.leaf);++i)printf("%02x",(unsigned char)t.leaf[i]);puts("");return 1;
}
static int owner_result(const fp_owner *o){
    if(o->save_kind){printf("OWNER_SAVE count=%"PRIu64" kind=%d leaf_hex=",o->selections,(int)o->save_kind);for(size_t i=0;i<strlen(o->leaf);++i)printf("%02x",(unsigned char)o->leaf[i]);printf(" directory_fd=%d witness_fd=%d\n",o->directory,o->witness);return 1;}
    if(o->batch_count){printf("OWNER_BATCH count=%"PRIu64" files=%zu\n",o->selections,o->batch_count);return 1;}
    struct stat selected;if(fstat(o->witness,&selected))return 0;
    printf("OWNER_RESULT count=%"PRIu64" directories=%d device=%"PRIuMAX" inode=%"PRIuMAX"\n",o->selections,o->directories,(uintmax_t)selected.st_dev,(uintmax_t)selected.st_ino);return 1;
}
#define REQUIRE(test) do { if(!(test)){fprintf(stderr,"picker-host:%d: %s\n",__LINE__,#test);okay=0;goto cleanup;} } while(0)
int main(int argc,char **argv) {
    int okay=1,root=-1,selected_directory=-1,selected_witness=-1,done=0,dirty=1,picked=0;
    nx_context *ctx=NULL;nx_window *window=NULL;nx_software_view *renderer=NULL;
    nx_font *font=NULL;nx_font_collection *fonts=NULL;unsigned char *bytes=NULL;FILE *file=NULL;
    fp_worker *worker=NULL;fp_picker picker={0};fp_intent pending={0};uint64_t frames=0,keys=0,requests=0;size_t last_preedit=0;
    fp_pick *batch_owned=NULL;size_t batch_count=0;
    fp_owner owner={0};int owner_mode=getenv("FP_OWNER")!=NULL;
    nx_event deferred={0};int has_deferred=0,draining=0;
    int theme=getenv("FP_CONTRAST")?NX_THEME_HIGH_CONTRAST:getenv("FP_LIGHT")?NX_THEME_LIGHT:NX_THEME_DARK;
    setvbuf(stdout,NULL,_IONBF,0);
    REQUIRE(argc==3);
    REQUIRE((root=open(argv[2],O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC))>=0);
    REQUIRE(fp_worker_start(root,&worker)==NX_OK);close(root);root=-1;
    REQUIRE((file=fopen(argv[1],"rb"))!=NULL&&!fseek(file,0,SEEK_END));
    long size=ftell(file);REQUIRE(size>0&&size<64000000&&!fseek(file,0,SEEK_SET));
    bytes=malloc((size_t)size);REQUIRE(bytes&&fread(bytes,1,(size_t)size,file)==(size_t)size);
    int closed=fclose(file);file=NULL;REQUIRE(!closed);
    REQUIRE(nx_font_open(bytes,(size_t)size,NULL,&font)==NX_OK&&nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    REQUIRE(context(&ctx,theme));
    if(owner_mode){REQUIRE(fp_owner_init(&owner,fonts)==NX_OK);owner.german=getenv("FP_GERMAN")!=NULL;owner.text_size=getenv("FP_LARGE")?32:16;}
    else REQUIRE(fp_picker_init(&picker,worker,fonts,1)==NX_OK);
    picker.german=getenv("FP_GERMAN")!=NULL;picker.text_size=getenv("FP_LARGE")?32:16;picker.directories=getenv("FP_DIRECTORY")!=NULL;
    picker.multiple=getenv("FP_MULTIPLE")!=NULL;REQUIRE(!(picker.multiple&&picker.directories));
    picker.save=getenv("FP_SAVE")!=NULL;REQUIRE(!picker.save||(!picker.multiple&&!picker.directories));
    nx_window_desc desc={owner_mode?(owner.german?"Auswahlbereich":"Selection workspace"):picker.directories?(picker.german?"Ordner auswählen":"Choose a folder"):(picker.german?"Datei auswählen":"Choose a file"),getenv("FP_SMALL")?360:720,getenv("FP_SMALL")?300:580,0,{0}};
    if(!owner_mode&&picker.multiple)desc.title_utf8=picker.german?"Dateien auswählen":"Choose files";
    if(!owner_mode&&picker.save)desc.title_utf8=picker.german?"Speichern unter":"Save as";
    REQUIRE(nx_window_create(&desc,&window)==NX_OK&&nx_software_view_open(window,&renderer)==NX_OK);
    if(!owner_mode){pending.requested=1;pending.command.operation=FP_READ;}
    while(!done) {
        nx_sim_message message={0};
        for(;;) {
            REQUIRE(fp_worker_receive(worker,&message)==NX_OK);if(!message.ticket)break;
            uint64_t ticket=message.ticket;int accepted=fp_picker_observe(&picker,&message);
            nx_sim_message_dispose(&message);
            printf("OBSERVE ticket=%"PRIu64" accepted=%d\n",ticket,accepted);
            if(accepted) {
                const fp_reply *r=fp_picker_snapshot(&picker);
                if(((picker.operation==FP_PICK||picker.operation==FP_PICK_DIRECTORY||picker.operation==FP_PICK_MANY)&&r&&r->result==AE_FILES_OK)||fp_picker_save_ready(&picker)) {
                    if(owner_mode){
                        REQUIRE(fp_owner_finish(&owner,&picker,ctx,1)==NX_OK);
                        picked=1;
                        REQUIRE(owner_result(&owner));
                        pending=(fp_intent){0};REQUIRE(replace_context(window,&ctx,theme,&owner));dirty=1;continue;
                    }
                    if(picker.save){REQUIRE(take_save(&picker,&selected_directory,&selected_witness));picked=1;done=1;break;}
                    if(picker.multiple){batch_owned=malloc(r->pick_count*sizeof(*batch_owned));REQUIRE(batch_owned&&fp_reply_take_many(fp_reply_get(&picker.observation),batch_owned,r->pick_count,&batch_count)==NX_OK);
                        printf("PICKED_MANY count=%zu\n",batch_count);picked=1;done=1;break;}
                    char leaf[AE_FILES_NAME+1];
                    REQUIRE(fp_reply_take(fp_reply_get(&picker.observation),&selected_directory,&selected_witness,leaf)==NX_OK);
                    struct stat d,s;REQUIRE(fstat(selected_directory,&d)==0&&S_ISDIR(d.st_mode)&&fstat(selected_witness,&s)==0);
                    if(picker.directories)REQUIRE(S_ISDIR(s.st_mode)&&d.st_dev==s.st_dev&&d.st_ino==s.st_ino&&!strcmp(leaf,"."));
                    else REQUIRE(S_ISREG(s.st_mode));
                    printf("PICKED leaf_hex=");for(size_t i=0;i<strlen(leaf);++i)printf("%02x",(unsigned char)leaf[i]);
                    printf(" device=%"PRIuMAX" inode=%"PRIuMAX"\n",(uintmax_t)s.st_dev,(uintmax_t)s.st_ino);
                    picked=1;done=1;break;
                }
                REQUIRE(replace_context(window,&ctx,theme,&owner));dirty=1;
            }
        }
        if(done)break;
        nx_window_info info;REQUIRE(nx_window_get_info(window,&info)==NX_OK);if(info.should_close)break;
        if((!owner_mode||owner.active)&&fp_picker_prepare_size(&picker,nx_focus_id(ctx),info.width,info.height,&pending)) {
            REQUIRE(replace_context(window,&ctx,theme,&owner));dirty=1;
        }
        if(dirty) {
            uint64_t frame_focus=nx_focus_id(ctx);
            REQUIRE(nx_context_set_text_raster_scale(ctx,info.scale)==NX_OK);
            REQUIRE(nx_begin_frame(ctx,info.width,info.height,0)==NX_OK);
            nx_semantic_desc main={0};main.id=1;main.role=NX_ROLE_WINDOW;main.name=(nx_utf8_span){desc.title_utf8,strlen(desc.title_utf8)};main.bounds=(nx_rect){0,0,info.width,info.height};
            REQUIRE(nx_semantics_add(ctx,&main,NULL)==NX_OK);int redraw,open_mode=0,quit=0;
            if(owner_mode)REQUIRE(fp_owner_build(&owner,&picker,ctx,window,info.width,info.height,&pending,&redraw,&open_mode,&quit)==NX_OK);
            else REQUIRE(fp_picker_build(&picker,ctx,window,info.width,info.height,&pending,&redraw)==NX_OK);
            REQUIRE(nx_end_frame(ctx)==NX_OK);
            int selection_changed;REQUIRE(fp_picker_selection_sync(&picker,&selection_changed)==NX_OK);
            if(selection_changed){if(pending.requested&&pending.command.operation==FP_PICK_MANY)pending=(fp_intent){0};continue;}
            if(quit){done=1;break;}
            if(open_mode){
                REQUIRE(fp_owner_open(&owner,&picker,worker,open_mode-1)==NX_OK);pending=(fp_intent){0};pending.requested=1;pending.command.operation=FP_READ;
                REQUIRE(replace_context(window,&ctx,theme,&owner));dirty=1;continue;
            }
            if(pending.save_back){REQUIRE(fp_picker_save_back(&picker)==NX_OK);picker.rebind=0;pending=(fp_intent){0};REQUIRE(replace_context(window,&ctx,theme,&owner));dirty=1;continue;}
            if(pending.save_accept){
                REQUIRE(fp_picker_save_confirm(&picker,ctx)==NX_OK);
                if(owner_mode){REQUIRE(fp_owner_finish(&owner,&picker,ctx,1)==NX_OK&&owner_result(&owner));picked=1;pending=(fp_intent){0};REQUIRE(replace_context(window,&ctx,theme,&owner));dirty=1;continue;}
                REQUIRE(take_save(&picker,&selected_directory,&selected_witness));picked=1;done=1;break;
            }
            if(pending.cancel){
                if(owner_mode){REQUIRE(fp_owner_finish(&owner,&picker,ctx,0)==NX_OK);pending=(fp_intent){0};REQUIRE(replace_context(window,&ctx,theme,&owner));dirty=1;printf("OWNER_VIEW_CLOSED selections=%"PRIu64"\n",owner.selections);continue;}
                done=1;break;
            }
            if(pending.resize) {
                pending.resize=0;nx_result resize=nx_window_begin_resize(window,NX_WINDOW_RESIZE_BOTTOM_RIGHT);
                REQUIRE(resize==NX_OK||resize==NX_ERROR_UNSUPPORTED||resize==NX_ERROR_STATE);
                if(resize==NX_OK)picker.restore=frame_focus;
                printf("RESIZE_REQUEST status=%d\n",(int)resize);
            }
            if(pending.requested) {
                nx_result result=fp_picker_request(&picker,&pending.command);pending=(fp_intent){0};
                REQUIRE(result==NX_OK||result==NX_ERROR_LIMIT||result==NX_ERROR_MEMORY);
                if(result==NX_OK)++requests;
                printf("REQUEST ticket=%"PRIu64" request=%"PRIu64" status=%d\n",picker.ticket,picker.request,(int)result);
                REQUIRE(replace_context(window,&ctx,theme,&owner));continue;
            }
            if(picker.rebind) {
                picker.rebind=0;if(!picker.restore)picker.restore=nx_focus_id(ctx);
                REQUIRE(replace_context(window,&ctx,theme,&owner));continue;
            }
            if(owner_mode&&owner.rebind){owner.rebind=0;REQUIRE(replace_context(window,&ctx,theme,&owner));continue;}
            if(redraw||(owner_mode&&!owner.active?fp_owner_reveal(&owner,ctx,info.height):fp_picker_reveal(&picker,ctx,info.height)))continue;
            REQUIRE(nx_window_publish_semantics(window,ctx)==NX_OK);
            /* Finish interaction/focus frames before the next application key,
               but rasterize only after the already queued input is drained. */
            if(draining)goto drain_input;
            const nx_command *commands;size_t count;nx_color background;nx_software_view_outcome outcome;
            REQUIRE(nx_frame_commands(ctx,&commands,&count)==NX_OK&&nx_theme_color(ctx,NX_COLOR_BACKGROUND,&background)==NX_OK&&nx_software_view_render(renderer,commands,count,background,&outcome)==NX_OK);
            dirty=outcome!=NX_SOFTWARE_VIEW_READY;++frames;
            const fp_reply *r=fp_picker_snapshot(&picker);
            printf("VIEW busy=%d state=%"PRIu64" depth=%u count=%zu page=%zu selected=%"PRIu64" focus=%"PRIu64" frames=%"PRIu64" keys=%"PRIu64" width=%.0f height=%.0f compact=%d\n",
                picker.busy,r?r->state:0,r?r->depth:0,r?r->count:0,picker.page,picker.selected,nx_focus_id(ctx),frames,keys,(double)info.width,(double)info.height,picker.compact);
            if(owner_mode)printf("OWNER active=%d epoch=%"PRIu64" selections=%"PRIu64" focus=%"PRIu64"\n",owner.active,owner.epoch,owner.selections,nx_focus_id(ctx));
        }
drain_input:
        if(!draining) {
            uint32_t ready=0;REQUIRE(nx_window_poll_with_fd(window,UINT32_MAX,fp_worker_wake_fd(worker),&ready)==NX_OK&&!(ready&NX_WINDOW_FD_ERROR));
        }
        nx_event event;int available;
        int queued_prefix=0;
        do {
            if(has_deferred){event=deferred;has_deferred=0;available=1;}
            else REQUIRE(nx_window_next_event(window,&event,&available)==NX_OK);
            if(!available){draining=0;break;}
            draining=1;
            /* A following command must observe the effects of every queued
               predecessor. Keep just that copied event across the frame. */
            if(queued_prefix&&command_boundary(&event)){deferred=event;has_deferred=1;break;}
            if(event.type==NX_EVENT_KEY_DOWN||event.type==NX_EVENT_KEY_UP)++keys;
            if(event.type==NX_EVENT_KEY_DOWN&&event.key==NX_KEY_ESCAPE&&!event.repeat){
                if((!owner_mode||owner.active)&&picker.save_review){REQUIRE(fp_picker_save_back(&picker)==NX_OK);picker.rebind=0;pending=(fp_intent){0};REQUIRE(replace_context(window,&ctx,theme,&owner));dirty=1;}
                else if(owner_mode&&owner.active){REQUIRE(fp_owner_finish(&owner,&picker,ctx,0)==NX_OK);pending=(fp_intent){0};REQUIRE(replace_context(window,&ctx,theme,&owner));dirty=1;printf("OWNER_VIEW_CLOSED selections=%"PRIu64"\n",owner.selections);}
                else done=1;
                break;
            }
            if(owner_mode&&fp_owner_key(&owner,ctx,&event)){dirty=1;break;}
            if((!owner_mode||owner.active)&&event.type==NX_EVENT_KEY_DOWN&&(event.key==NX_KEY_PAGE_UP||event.key==NX_KEY_PAGE_DOWN)&&!event.modifiers&&nx_focus_id(ctx)!=FP_QUERY&&nx_focus_id(ctx)!=FP_FILENAME) {
                uint32_t area_index;nx_semantic_view area;float step=info.height*0.25f;
                if(nx_semantics_find(ctx,picker.save_review?FP_SAVE_SCROLL:FP_SCROLL,&area_index)==NX_OK&&nx_semantics_get(ctx,area_index,&area)==NX_OK)step=area.bounds.height*0.5f;
                float *offset=picker.save_review?&picker.review_scroll:&picker.scroll_y;*offset+=event.key==NX_KEY_PAGE_DOWN?step:-step;
                if(*offset<0)*offset=0;
                dirty=1;break;
            }
            uint64_t focus=nx_focus_id(ctx);
            if((!owner_mode||owner.active)&&fp_picker_key(&picker,focus,&event,&pending)){
                printf("ROW_KEY key=%d focus=%"PRIu64" selected=%"PRIu64" page=%zu\n",(int)event.key,focus,picker.selected,picker.page);
                dirty=1;break;
            }
            REQUIRE(nx_context_queue_event(ctx,&event)==NX_OK);queued_prefix=1;dirty=1;
        }while(available);
        /* route_events also transfers the remaining native queue. Calling it
           at a command boundary would bypass the dispatcher above. Route
           assistive requests only once that native queue is actually empty. */
        if(!draining) {
            size_t routed=0;REQUIRE(nx_window_route_events(window,ctx,&routed)==NX_OK);if(routed)dirty=1;
        }
        size_t composition_bytes=0;int32_t begin=0,end=0;
        nx_result composition_status=nx_window_get_ime_preedit(window,NULL,0,&composition_bytes,&begin,&end);
        REQUIRE(composition_status==NX_OK||composition_status==NX_ERROR_UNSUPPORTED);
        if(composition_bytes!=last_preedit){printf("PREEDIT bytes=%zu\n",composition_bytes);last_preedit=composition_bytes;}
    }
cleanup:
    if(window&&nx_window_publish_semantics(window,NULL)!=NX_OK)okay=0;
    if(renderer&&nx_software_view_close(&renderer)!=NX_OK)okay=0;
    nx_context_destroy(ctx);if(window)nx_window_destroy(window);puts("GUI_GONE");
    if(owner_mode){printf("OWNER_CLOSED selections=%"PRIu64"\n",owner.selections);fp_owner_destroy(&owner);}
    fp_picker_destroy(&picker);if(fp_worker_destroy(&worker)!=NX_OK)okay=0;
    if(selected_directory>=0)close(selected_directory);
    if(selected_witness>=0)close(selected_witness);
    for(size_t i=0;i<batch_count;++i){close(batch_owned[i].directory);close(batch_owned[i].witness);}free(batch_owned);
    if(root>=0)close(root);
    if(file)fclose(file);
    nx_font_collection_destroy(fonts);nx_font_close(font);free(bytes);
    printf("PICKER_CLOSED picked=%d requests=%"PRIu64" failed=%d\n",picked,requests,!okay);return okay?0:1;
}
