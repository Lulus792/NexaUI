#define _POSIX_C_SOURCE 200809L
/* Trusted, explicit private-owner example host. No ordinary UI-selected path,
   authority service, recovery actor or normal installed Aetheri catalogue. */
#include "recovery_view.h"
#include "recovery_worker.h"
#include <nexaui/software.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"recovery native %d: %s\n",__LINE__,#x);okay=0;goto cleanup;}}while(0)
static int parse_id(const char *s,uint8_t out[16]){
    if(strlen(s)!=32||strspn(s,"0123456789abcdefABCDEF")!=32)return 0;
    for(size_t i=0;i<16;++i){char pair[3]={s[2*i],s[2*i+1],0},*end;unsigned long n=strtoul(pair,&end,16);if(*end||n>255)return 0;out[i]=(uint8_t)n;}return 1;
}
static int request(rw_worker *worker,rv_owner *owner,uint64_t *sequence,rw_key *current){
    if(*sequence==UINT64_MAX)return 0;
    rw_key key={1,*sequence+1};uint64_t ticket=0;nx_result r=rw_submit(worker,key,&ticket);
    if(r==NX_OK){*sequence=key.request;*current=key;r=rv_owner_expect(owner,ticket);}
    printf("REQUEST ticket=%" PRIu64 " session=%" PRIu64 " request=%" PRIu64 " status=%d\n",ticket,key.view,key.request,(int)r);return r==NX_OK;
}
int main(int argc,char **argv){
    int okay=1,done=0,dirty=1;rw_worker *worker=NULL;rv_owner *owner=NULL;rvg_view *view=NULL;
    nx_window *window=NULL;nx_software_view *renderer=NULL;nx_font *font=NULL;nx_font_collection *fonts=NULL;
    unsigned char *font_bytes=NULL;FILE *file=NULL;uint64_t sequence=0,frames=0,polls=0,keys=0,adopted=0,ignored=0;rw_key current={0};nx_sim_message message={0};
    setvbuf(stdout,NULL,_IONBF,0);
    REQUIRE(argc==6);struct ae_catalogue_identity identity={0};
    REQUIRE(!strcmp(argv[3],"system")||!strcmp(argv[3],"user"));identity.kind=!strcmp(argv[3],"system")?AE_CATALOGUE_SYSTEM:AE_CATALOGUE_USER;
    REQUIRE(parse_id(argv[4],identity.authority_epoch));if(identity.kind==AE_CATALOGUE_USER)REQUIRE(parse_id(argv[5],identity.owner));else REQUIRE(!strcmp(argv[5],"-"));
    struct ae_catalogue_info initial;int original_error=0;nx_result started=rw_start(argv[2],&identity,&worker,&initial,&original_error);
    printf("OWNER_OPEN status=%d original_error=%d\n",(int)started,original_error);REQUIRE(started==NX_OK);
    REQUIRE(rv_owner_create(&identity,&initial,&owner)==NX_OK);
    REQUIRE((file=fopen(argv[1],"rb"))&&!fseek(file,0,SEEK_END));long length=ftell(file);REQUIRE(length>0&&length<64000000&&!fseek(file,0,SEEK_SET));
    font_bytes=malloc((size_t)length);REQUIRE(font_bytes&&fread(font_bytes,1,(size_t)length,file)==(size_t)length);int closed=fclose(file);file=NULL;REQUIRE(!closed);
    REQUIRE(nx_font_open(font_bytes,(size_t)length,NULL,&font)==NX_OK&&nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    rvg_style style={0,16,1,NX_THEME_DARK};style.german=getenv("RVG_GERMAN")!=NULL;if(getenv("RVG_LARGE"))style.text_size=32;
    if(getenv("RVG_CONTRAST"))style.theme=NX_THEME_HIGH_CONTRAST;else if(getenv("RVG_LIGHT"))style.theme=NX_THEME_LIGHT;
    REQUIRE(rvg_create(owner,fonts,style,&view)==NX_OK);
    int small=getenv("RVG_SMALL")!=NULL;nx_window_desc desc={"NexaUI original recovery observations",small?360:800,small?300:600,0,{0}};
    REQUIRE(nx_window_create(&desc,&window)==NX_OK&&nx_window_set_wayland_app_id(window,"org.nexaui.examples.recovery")==NX_OK&&nx_software_view_open(window,&renderer)==NX_OK);
    rvg_bind_window(view,window);REQUIRE(request(worker,owner,&sequence,&current));
    while(!done){
        for(;;){
            REQUIRE(rw_receive(worker,&message)==NX_OK);if(!message.ticket)break;
            int accepted=0;
            if(message.ticket==rv_owner_pending(owner)){
                rw_reply *reply=rw_reply_get(&message);int error=message.status==NX_ERROR_MEMORY?-ENOMEM:-EBADMSG;
                nx_result r;
                if(reply&&reply->key.view==current.view&&reply->key.request==current.request){
                    r=rv_owner_complete(owner,message.ticket,reply->original_error,&reply->inventory);
                    if(r==NX_OK)accepted=1;
                    else error=r==NX_ERROR_MEMORY?-ENOMEM:r==NX_ERROR_STATE?-ESTALE:-EBADMSG;
                }
                if(!accepted){REQUIRE(rv_owner_complete(owner,message.ticket,error,NULL)==NX_OK);accepted=1;}
            }
            if(accepted){++adopted;dirty=1;}else ++ignored;
            printf("OBSERVE ticket=%" PRIu64 " accepted=%d state=%d epoch=%" PRIu64 " error=%d\n",message.ticket,accepted,(int)rv_owner_state(owner),rv_owner_epoch(owner),rv_owner_error(owner));
            nx_sim_message_dispose(&message);
        }
        nx_window_info info;REQUIRE(nx_window_get_info(window,&info)==NX_OK);if(info.should_close)break;
        if(dirty){
            REQUIRE(rvg_sync(view)==NX_OK&&rvg_set_raster_scale(view,info.scale)==NX_OK);rvg_intent intent;REQUIRE(rvg_frame(view,info.width,info.height,&intent)==NX_OK);
            if(intent.close){done=1;break;}
            if(intent.resize){nx_result resized=nx_window_begin_resize(window,NX_WINDOW_RESIZE_BOTTOM_RIGHT);printf("RESIZE_REQUEST status=%d\n",(int)resized);}
            if(intent.refresh){REQUIRE(request(worker,owner,&sequence,&current));dirty=1;continue;}
            nx_context *ctx=rvg_context(view);const nx_command *commands;size_t count;nx_color background;nx_software_view_outcome outcome;
            REQUIRE(nx_window_publish_semantics(window,ctx)==NX_OK&&nx_frame_commands(ctx,&commands,&count)==NX_OK&&nx_theme_color(ctx,NX_COLOR_BACKGROUND,&background)==NX_OK&&nx_software_view_render(renderer,commands,count,background,&outcome)==NX_OK);
            dirty=outcome!=NX_SOFTWARE_VIEW_READY;++frames;
            printf("VIEW state=%d epoch=%" PRIu64 " count=%zu mode=%d focus=%" PRIu64 " width=%.0f height=%.0f scale=%g frames=%" PRIu64 " polls=%" PRIu64 " keys=%" PRIu64 " request=%" PRIu64 "\n",
                (int)rv_owner_state(owner),rv_owner_epoch(owner),rvg_matches(view),(int)rvg_current_mode(view),nx_focus_id(ctx),(double)info.width,(double)info.height,(double)info.scale,frames,polls,keys,sequence);
            if(outcome==NX_SOFTWARE_VIEW_READY)puts("RECOVERY_READY");
        }
        uint32_t ready=0;++polls;REQUIRE(nx_window_poll_with_fd(window,UINT32_MAX,rw_wake_fd(worker),&ready)==NX_OK&&!(ready&NX_WINDOW_FD_ERROR));
        nx_event event;int available=0;
        do{REQUIRE(nx_window_next_event(window,&event,&available)==NX_OK);if(!available)break;
            if(event.type==NX_EVENT_KEY_DOWN||event.type==NX_EVENT_KEY_UP)++keys;
            if(event.type==NX_EVENT_FOCUS)printf("FOCUS active=%d\n",event.focused);
            if(event.type==NX_EVENT_IME_PREEDIT_CHANGED){size_t bytes=0;int32_t start=0,end=0;nx_result ime=nx_window_get_ime_preedit(window,NULL,0,&bytes,&start,&end);REQUIRE(ime==NX_OK||ime==NX_ERROR_UNSUPPORTED);printf("PREEDIT bytes=%zu\n",bytes);}
            REQUIRE(rvg_queue_event(view,&event)==NX_OK);dirty=1;
        }while(available);
        size_t routed=0;REQUIRE(nx_window_route_events(window,rvg_context(view),&routed)==NX_OK);if(routed)dirty=1;
    }
cleanup:
    nx_sim_message_dispose(&message);
    if(view&&rvg_close(&view)!=NX_OK)okay=0;
    if(renderer&&nx_software_view_close(&renderer)!=NX_OK)okay=0;
    if(window)nx_window_destroy(window);
    rv_owner_destroy(&owner);puts("RECOVERY_GUI_GONE");
    if(worker){if(rw_stop(worker)!=NX_OK||rw_join(worker)!=NX_OK)okay=0;if(rw_destroy(&worker)!=NX_OK)okay=0;}
    nx_font_collection_destroy(fonts);nx_font_close(font);free(font_bytes);if(file)fclose(file);
    printf("RECOVERY_CLOSED requests=%" PRIu64 " adopted=%" PRIu64 " ignored=%" PRIu64 " failed=%d\n",sequence,adopted,ignored,!okay);return okay?0:1;
}
