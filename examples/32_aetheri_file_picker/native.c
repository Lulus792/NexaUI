#define _GNU_SOURCE
#include "picker.h"
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
static int replace_context(nx_window *window,nx_context **ctx,int theme) {
    if(nx_window_publish_semantics(window,NULL)!=NX_OK)return 0;
    nx_context_destroy(*ctx);*ctx=NULL;return context(ctx,theme);
}
#define REQUIRE(test) do { if(!(test)){fprintf(stderr,"picker-host:%d: %s\n",__LINE__,#test);okay=0;goto cleanup;} } while(0)
int main(int argc,char **argv) {
    int okay=1,root=-1,selected_directory=-1,selected_witness=-1,done=0,dirty=1,picked=0;
    nx_context *ctx=NULL;nx_window *window=NULL;nx_software_view *renderer=NULL;
    nx_font *font=NULL;nx_font_collection *fonts=NULL;unsigned char *bytes=NULL;FILE *file=NULL;
    fp_worker *worker=NULL;fp_picker picker={0};fp_intent pending={0};uint64_t frames=0,keys=0,requests=0;size_t last_preedit=0;
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
    REQUIRE(context(&ctx,theme)&&fp_picker_init(&picker,worker,fonts,1)==NX_OK);
    picker.german=getenv("FP_GERMAN")!=NULL;picker.text_size=getenv("FP_LARGE")?32:16;
    nx_window_desc desc={picker.german?"Datei auswählen":"Choose a file",getenv("FP_SMALL")?360:720,getenv("FP_SMALL")?300:580,0,{0}};
    REQUIRE(nx_window_create(&desc,&window)==NX_OK&&nx_software_view_open(window,&renderer)==NX_OK);
    pending.requested=1;pending.command.operation=FP_READ;
    while(!done) {
        nx_sim_message message={0};
        for(;;) {
            REQUIRE(fp_worker_receive(worker,&message)==NX_OK);if(!message.ticket)break;
            uint64_t ticket=message.ticket;int accepted=fp_picker_observe(&picker,&message);
            nx_sim_message_dispose(&message);
            printf("OBSERVE ticket=%"PRIu64" accepted=%d\n",ticket,accepted);
            if(accepted) {
                REQUIRE(replace_context(window,&ctx,theme));dirty=1;
                const fp_reply *r=fp_picker_snapshot(&picker);
                if(picker.operation==FP_PICK&&r&&r->result==AE_FILES_OK) {
                    char leaf[AE_FILES_NAME+1];
                    REQUIRE(fp_reply_take(fp_reply_get(&picker.observation),&selected_directory,&selected_witness,leaf)==NX_OK);
                    struct stat d,s;REQUIRE(fstat(selected_directory,&d)==0&&S_ISDIR(d.st_mode)&&fstat(selected_witness,&s)==0&&S_ISREG(s.st_mode));
                    printf("PICKED leaf_hex=");for(size_t i=0;i<strlen(leaf);++i)printf("%02x",(unsigned char)leaf[i]);
                    printf(" device=%"PRIuMAX" inode=%"PRIuMAX"\n",(uintmax_t)s.st_dev,(uintmax_t)s.st_ino);
                    picked=1;done=1;break;
                }
            }
        }
        if(done)break;
        nx_window_info info;REQUIRE(nx_window_get_info(window,&info)==NX_OK);if(info.should_close)break;
        if(dirty) {
            REQUIRE(nx_context_set_text_raster_scale(ctx,info.scale)==NX_OK);
            REQUIRE(nx_begin_frame(ctx,info.width,info.height,0)==NX_OK);
            nx_semantic_desc main={0};main.id=1;main.role=NX_ROLE_WINDOW;main.name=(nx_utf8_span){desc.title_utf8,strlen(desc.title_utf8)};main.bounds=(nx_rect){0,0,info.width,info.height};
            REQUIRE(nx_semantics_add(ctx,&main,NULL)==NX_OK);int redraw;
            REQUIRE(fp_picker_build(&picker,ctx,window,info.width,info.height,&pending,&redraw)==NX_OK&&nx_end_frame(ctx)==NX_OK);
            if(pending.cancel){done=1;break;}
            if(pending.requested) {
                nx_result result=fp_picker_request(&picker,&pending.command);pending=(fp_intent){0};
                REQUIRE(result==NX_OK||result==NX_ERROR_LIMIT||result==NX_ERROR_MEMORY);
                if(result==NX_OK)++requests;
                printf("REQUEST ticket=%"PRIu64" request=%"PRIu64" status=%d\n",picker.ticket,picker.request,(int)result);
                REQUIRE(replace_context(window,&ctx,theme));continue;
            }
            if(picker.rebind) {
                picker.rebind=0;picker.restore=nx_focus_id(ctx);
                REQUIRE(replace_context(window,&ctx,theme));continue;
            }
            if(redraw||fp_picker_reveal(&picker,ctx,info.height))continue;
            REQUIRE(nx_window_publish_semantics(window,ctx)==NX_OK);
            const nx_command *commands;size_t count;nx_color background;nx_software_view_outcome outcome;
            REQUIRE(nx_frame_commands(ctx,&commands,&count)==NX_OK&&nx_theme_color(ctx,NX_COLOR_BACKGROUND,&background)==NX_OK&&nx_software_view_render(renderer,commands,count,background,&outcome)==NX_OK);
            dirty=outcome!=NX_SOFTWARE_VIEW_READY;++frames;
            const fp_reply *r=fp_picker_snapshot(&picker);
            printf("VIEW busy=%d state=%"PRIu64" depth=%u count=%zu page=%zu selected=%"PRIu64" focus=%"PRIu64" frames=%"PRIu64" keys=%"PRIu64"\n",
                picker.busy,r?r->state:0,r?r->depth:0,r?r->count:0,picker.page,picker.selected,nx_focus_id(ctx),frames,keys);
        }
        uint32_t ready=0;REQUIRE(nx_window_poll_with_fd(window,UINT32_MAX,fp_worker_wake_fd(worker),&ready)==NX_OK&&!(ready&NX_WINDOW_FD_ERROR));
        nx_event event;int available;
        do {
            REQUIRE(nx_window_next_event(window,&event,&available)==NX_OK);if(!available)break;
            if(event.type==NX_EVENT_KEY_DOWN||event.type==NX_EVENT_KEY_UP)++keys;
            if(event.type==NX_EVENT_KEY_DOWN&&event.key==NX_KEY_ESCAPE&&!event.repeat){done=1;break;}
            if(event.type==NX_EVENT_KEY_DOWN&&(event.key==NX_KEY_PAGE_UP||event.key==NX_KEY_PAGE_DOWN)&&!event.modifiers&&nx_focus_id(ctx)!=FP_QUERY) {
                picker.scroll_y+=event.key==NX_KEY_PAGE_DOWN?info.height*0.4f:-info.height*0.4f;
                if(picker.scroll_y<0)picker.scroll_y=0;
                dirty=1;continue;
            }
            if(fp_picker_key(&picker,nx_focus_id(ctx),&event,&pending)){dirty=1;continue;}
            REQUIRE(nx_context_queue_event(ctx,&event)==NX_OK);dirty=1;
        }while(available);
        size_t routed=0;REQUIRE(nx_window_route_events(window,ctx,&routed)==NX_OK);if(routed)dirty=1;
        size_t composition_bytes=0;int32_t begin=0,end=0;
        nx_result composition_status=nx_window_get_ime_preedit(window,NULL,0,&composition_bytes,&begin,&end);
        REQUIRE(composition_status==NX_OK||composition_status==NX_ERROR_UNSUPPORTED);
        if(composition_bytes!=last_preedit){printf("PREEDIT bytes=%zu\n",composition_bytes);last_preedit=composition_bytes;}
    }
cleanup:
    if(window&&nx_window_publish_semantics(window,NULL)!=NX_OK)okay=0;
    if(renderer&&nx_software_view_close(&renderer)!=NX_OK)okay=0;
    nx_context_destroy(ctx);if(window)nx_window_destroy(window);puts("GUI_GONE");
    fp_picker_destroy(&picker);if(fp_worker_destroy(&worker)!=NX_OK)okay=0;
    if(selected_directory>=0)close(selected_directory);
    if(selected_witness>=0)close(selected_witness);
    if(root>=0)close(root);
    if(file)fclose(file);
    nx_font_collection_destroy(fonts);nx_font_close(font);free(bytes);
    printf("PICKER_CLOSED picked=%d requests=%"PRIu64" failed=%d\n",picked,requests,!okay);return okay?0:1;
}
