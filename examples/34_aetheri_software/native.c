#define _POSIX_C_SOURCE 200809L
/* Trusted explicit Root owner host, never an ordinary UI-selected source path.
   Selected targets only; no inventory, service authorization or administration. */
#include "software_view.h"
#include "software_worker.h"
#include <nexaui/software.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"software native %d: %s\n",__LINE__,#x);okay=0;goto cleanup;}}while(0)
static int parse_hex(const char *s,size_t bytes,uint8_t *out){
    if(strlen(s)!=bytes*2||strspn(s,"0123456789abcdefABCDEF")!=bytes*2)return 0;
    for(size_t i=0;i<bytes;++i){char pair[3]={s[2*i],s[2*i+1],0},*end;unsigned long n=strtoul(pair,&end,16);if(*end||n>255)return 0;out[i]=(uint8_t)n;}return 1;
}
static int parse_id(const char *s,uint8_t out[16]){return parse_hex(s,16,out);}
static int request(sww_worker *worker,sw_owner *owner,const sw_scope *scope,const sw_id *targets,size_t count,uint64_t *sequence,sww_key *current){
    if(*sequence==UINT64_MAX)return 0;
    sww_key key={1,*sequence+1};uint64_t ticket=0;nx_result r=sww_submit(worker,key,scope,targets,count,&ticket);
    if(r==NX_OK){*sequence=key.request;*current=key;r=sw_owner_expect(owner,ticket);}
    printf("REQUEST ticket=%" PRIu64 " session=%" PRIu64 " request=%" PRIu64 " status=%d\n",ticket,key.view,key.request,(int)r);return r==NX_OK;
}
int main(int argc,char **argv){
    int okay=1,done=0,dirty=1;sww_worker *worker=NULL;sw_owner *owner=NULL;swg_view *view=NULL;
    nx_window *window=NULL;nx_software_view *renderer=NULL;nx_font *font=NULL;nx_font_collection *fonts=NULL;
    sw_components *components=NULL;sw_manifest_source *sources=NULL;swh_batch *history=NULL;struct ae_execution_profile *profiles=NULL;
    sw_id *targets=NULL;unsigned char *font_bytes=NULL;FILE *file=NULL;uint64_t sequence=0,frames=0,polls=0,keys=0,adopted=0,ignored=0;sww_key current={0};nx_sim_message message={0};
    setvbuf(stdout,NULL,_IONBF,0);
    REQUIRE(argc>=6);struct ae_catalogue_identity identity={.kind=AE_CATALOGUE_SYSTEM};
    REQUIRE(parse_id(argv[3],identity.authority_epoch));sw_scope scope={.identity=identity};
    REQUIRE(parse_id(argv[4],scope.account));
    REQUIRE(*argv[5]&&strspn(argv[5],"0123456789")==strlen(argv[5]));errno=0;char *uid_end=NULL;uintmax_t uid=strtoumax(argv[5],&uid_end,10);
    REQUIRE(!errno&&uid_end&&!*uid_end&&uid<UINT32_MAX);scope.os_uid=(uint32_t)uid;
    int options_at=6;while(options_at<argc&&strcmp(argv[options_at],"--manifest")&&strcmp(argv[options_at],"--history"))++options_at;
    size_t count=(size_t)(options_at-6);REQUIRE(count<=SW_MAX_TARGETS);targets=count?calloc(count,sizeof(*targets)):NULL;REQUIRE(!count||targets);
    for(size_t i=0;i<count;++i)REQUIRE(parse_id(argv[i+6],targets[i].bytes));
    size_t source_count=0,profile_count=0;
    for(int at=options_at;at<argc;){if(!strcmp(argv[at],"--manifest")){REQUIRE(argc-at>=3);++source_count;at+=3;}
        else{REQUIRE(!strcmp(argv[at],"--history")&&argc-at>=2);++profile_count;at+=2;}}
    REQUIRE(source_count<=SW_MAX_TARGETS&&profile_count<=SWH_MAX_PROFILES);
    sources=source_count?calloc(source_count,sizeof(*sources)):NULL;REQUIRE(!source_count||sources);
    profiles=profile_count?calloc(profile_count,sizeof(*profiles)):NULL;REQUIRE(!profile_count||profiles);
    size_t source_index=0,profile_index=0;
    for(int at=options_at;at<argc;){if(!strcmp(argv[at],"--manifest")){REQUIRE(parse_id(argv[at+1],sources[source_index].install.bytes));sources[source_index++].path=argv[at+2];at+=3;}
        else{uint8_t raw[AE_EXECUTION_BYTES];REQUIRE(parse_hex(argv[at+1],sizeof raw,raw)&&ae_execution_decode(raw,sizeof raw,&profiles[profile_index++])==0);at+=2;}}
    sww_metadata metadata={sources,source_count,profiles,profile_count};
    int original_error=0;nx_result started=sww_start_with_metadata(argv[2],&identity,&metadata,&worker,&scope.context,&original_error);
    printf("OWNER_OPEN status=%d original_error=%d selected=%zu\n",(int)started,original_error,count);REQUIRE(started==NX_OK);
    REQUIRE(sw_owner_create(&scope,targets,count,&owner)==NX_OK);
    REQUIRE((file=fopen(argv[1],"rb"))&&!fseek(file,0,SEEK_END));long length=ftell(file);REQUIRE(length>0&&length<64000000&&!fseek(file,0,SEEK_SET));
    font_bytes=malloc((size_t)length);REQUIRE(font_bytes&&fread(font_bytes,1,(size_t)length,file)==(size_t)length);int closed=fclose(file);file=NULL;REQUIRE(!closed);
    REQUIRE(nx_font_open(font_bytes,(size_t)length,NULL,&font)==NX_OK&&nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    swg_style style={0,16,1,NX_THEME_DARK};style.german=getenv("SWG_GERMAN")!=NULL;if(getenv("SWG_LARGE"))style.text_size=32;
    if(getenv("SWG_CONTRAST"))style.theme=NX_THEME_HIGH_CONTRAST;else if(getenv("SWG_LIGHT"))style.theme=NX_THEME_LIGHT;
    REQUIRE(swg_create(owner,fonts,style,&view)==NX_OK);
    int small=getenv("SWG_SMALL")!=NULL;nx_window_desc desc={"NexaUI original selected app observations",small?360:800,small?300:600,0,{0}};
    REQUIRE(nx_window_create(&desc,&window)==NX_OK&&nx_window_set_wayland_app_id(window,"org.nexaui.examples.software")==NX_OK&&nx_software_view_open(window,&renderer)==NX_OK);
    swg_bind_window(view,window);REQUIRE(request(worker,owner,&scope,targets,count,&sequence,&current));
    while(!done){
        for(;;){
            REQUIRE(sww_receive(worker,&message)==NX_OK);if(!message.ticket)break;
            int accepted=0;
            if(message.ticket==sw_owner_pending(owner)){
                sww_reply *reply=sww_reply_get(&message);int error=message.status==NX_ERROR_MEMORY?-ENOMEM:-EBADMSG;
                nx_result r;
                if(reply&&reply->key.view==current.view&&reply->key.request==current.request){
                    r=sw_owner_complete(owner,message.ticket,reply->original_error,&reply->result);
                    if(r==NX_OK){accepted=1;if(!reply->original_error){
                        REQUIRE(swg_set_metadata(view,reply->components,reply->components_status,reply->history,reply->history_error)==NX_OK);
                        sw_components_free(&components);components=reply->components;reply->components=NULL;
                        swh_free(&history);if(reply->history)REQUIRE(sww_reply_take_history(reply,&history)==NX_OK);
                    }}
                    else error=r==NX_ERROR_MEMORY?-ENOMEM:r==NX_ERROR_STATE?-ESTALE:-EBADMSG;
                }
                if(!accepted){REQUIRE(sw_owner_complete(owner,message.ticket,error,NULL)==NX_OK);accepted=1;}
            }
            if(accepted){++adopted;dirty=1;}else ++ignored;
            printf("OBSERVE ticket=%" PRIu64 " accepted=%d state=%d epoch=%" PRIu64 " error=%d\n",message.ticket,accepted,(int)sw_owner_state(owner),sw_owner_epoch(owner),sw_owner_error(owner));
            nx_sim_message_dispose(&message);
        }
        nx_window_info info;REQUIRE(nx_window_get_info(window,&info)==NX_OK);if(info.should_close)break;
        if(dirty){
            REQUIRE(swg_sync(view)==NX_OK&&swg_set_raster_scale(view,info.scale)==NX_OK);swg_intent intent;REQUIRE(swg_frame(view,info.width,info.height,&intent)==NX_OK);
            if(intent.close){done=1;break;}
            if(intent.resize){nx_result resized=nx_window_begin_resize(window,NX_WINDOW_RESIZE_BOTTOM_RIGHT);printf("RESIZE_REQUEST status=%d\n",(int)resized);}
            if(intent.refresh){REQUIRE(request(worker,owner,&scope,targets,count,&sequence,&current));dirty=1;continue;}
            nx_context *ctx=swg_context(view);const nx_command *commands;size_t command_count;nx_color background;nx_software_view_outcome outcome;
            REQUIRE(nx_window_publish_semantics(window,ctx)==NX_OK&&nx_frame_commands(ctx,&commands,&command_count)==NX_OK&&nx_theme_color(ctx,NX_COLOR_BACKGROUND,&background)==NX_OK&&nx_software_view_render(renderer,commands,command_count,background,&outcome)==NX_OK);
            dirty=outcome!=NX_SOFTWARE_VIEW_READY;++frames;
            printf("VIEW state=%d epoch=%" PRIu64 " count=%zu mode=%d focus=%" PRIu64 " width=%.0f height=%.0f scale=%g frames=%" PRIu64 " polls=%" PRIu64 " keys=%" PRIu64 " request=%" PRIu64 "\n",
                (int)sw_owner_state(owner),sw_owner_epoch(owner),swg_matches(view),(int)swg_current_mode(view),nx_focus_id(ctx),(double)info.width,(double)info.height,(double)info.scale,frames,polls,keys,sequence);
            if(outcome==NX_SOFTWARE_VIEW_READY)puts("SOFTWARE_READY");
        }
        uint32_t ready=0;++polls;REQUIRE(nx_window_poll_with_fd(window,UINT32_MAX,sww_wake_fd(worker),&ready)==NX_OK&&!(ready&NX_WINDOW_FD_ERROR));
        nx_event event;int available=0;
        do{REQUIRE(nx_window_next_event(window,&event,&available)==NX_OK);if(!available)break;
            if(event.type==NX_EVENT_KEY_DOWN||event.type==NX_EVENT_KEY_UP)++keys;
            if(event.type==NX_EVENT_FOCUS)printf("FOCUS active=%d\n",event.focused);
            if(event.type==NX_EVENT_IME_PREEDIT_CHANGED){size_t bytes=0;int32_t start=0,end=0;nx_result ime=nx_window_get_ime_preedit(window,NULL,0,&bytes,&start,&end);REQUIRE(ime==NX_OK||ime==NX_ERROR_UNSUPPORTED);printf("PREEDIT bytes=%zu\n",bytes);}
            REQUIRE(swg_queue_event(view,&event)==NX_OK);dirty=1;
        }while(available);
        size_t routed=0;REQUIRE(nx_window_route_events(window,swg_context(view),&routed)==NX_OK);if(routed)dirty=1;
    }
cleanup:
    nx_sim_message_dispose(&message);
    if(view&&swg_close(&view)!=NX_OK)okay=0;
    if(renderer&&nx_software_view_close(&renderer)!=NX_OK)okay=0;
    if(window)nx_window_destroy(window);
    sw_owner_destroy(&owner);sw_components_free(&components);swh_free(&history);puts("SOFTWARE_GUI_GONE");
    if(worker){if(sww_stop(worker)!=NX_OK||sww_join(worker)!=NX_OK)okay=0;if(sww_destroy(&worker)!=NX_OK)okay=0;}
    nx_font_collection_destroy(fonts);nx_font_close(font);free(font_bytes);free(targets);free(sources);free(profiles);if(file)fclose(file);
    printf("SOFTWARE_CLOSED requests=%" PRIu64 " adopted=%" PRIu64 " ignored=%" PRIu64 " failed=%d\n",sequence,adopted,ignored,!okay);return okay?0:1;
}
