#define _POSIX_C_SOURCE 200809L
#include "fixture.h"
#include <nexaui/software.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"removal-host:%d: %s\n",__LINE__,#x);okay=0;goto cleanup;}}while(0)
int main(int argc,char **argv){
    int okay=1,done=0,dirty=1,focus=1,reopened=0;unsigned requests=0;
    FILE *file=NULL;unsigned char *bytes=NULL;removal_entry *large_entries=NULL;nx_font *font=NULL;nx_font_collection *fonts=NULL;
    removal_owner *owner=NULL;removal_review *view=NULL;nx_window *window=NULL;nx_software_view *renderer=NULL;
    const char *pending="REMOVAL_READY";char input[64]={0};size_t used=0;
    int control=getenv("NEXAUI_REMOVAL_TEST_CONTROL")!=NULL;setvbuf(stdout,NULL,_IONBF,0);
    REQUIRE(argc==2);file=fopen(argv[1],"rb");REQUIRE(file && !fseek(file,0,SEEK_END));long size=ftell(file);
    REQUIRE(size>0 && size<64000000 && !fseek(file,0,SEEK_SET));bytes=malloc((size_t)size);REQUIRE(bytes);
    REQUIRE(fread(bytes,1,(size_t)size,file)==(size_t)size);int closed=fclose(file);file=NULL;REQUIRE(!closed);
    REQUIRE(nx_font_open(bytes,(size_t)size,NULL,&font)==NX_OK && nx_font_collection_create(NULL,&fonts)==NX_OK &&
        nx_font_collection_add(fonts,font)==NX_OK);
    removal_entry entries[8];removal_plan plan=removal_fixture(entries);
    if(getenv("NEXAUI_REMOVAL_LARGE")){
        large_entries=removal_large_fixture(REMOVAL_MAX_ENTRIES);REQUIRE(large_entries);
        plan.count=REMOVAL_MAX_ENTRIES;plan.entries=large_entries;
    }
    if(getenv("NEXAUI_REMOVAL_INCOMPLETE")){
        if(large_entries){large_entries[plan.count-1].disposition=REMOVAL_UNKNOWN;
            strcpy(large_entries[plan.count-1].reason,"Owner cannot establish this object's managed state");}
        else plan.inventory_complete=0;
    }
    REQUIRE(removal_owner_create(&plan,&owner)==NX_OK);free(large_entries);large_entries=NULL;
    plan.entries=NULL;REQUIRE(removal_review_create(owner,fonts,&view)==NX_OK);
    nx_window_desc desc={"Removal review fixture",640,480,0,{0}};
    REQUIRE(nx_window_create(&desc,&window)==NX_OK && nx_software_view_open(window,&renderer)==NX_OK);
    if(control)REQUIRE(fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL)|O_NONBLOCK)==0);
    while(!done){
        nx_window_info info;REQUIRE(nx_window_get_info(window,&info)==NX_OK);if(info.should_close)break;
        nx_context *ctx=removal_review_context(view);
        if(dirty){
            removal_action action;REQUIRE(removal_review_frame(view,info.width,info.height,&action)==NX_OK);
            if(action.kind==REMOVAL_CLOSE)break;
            if(action.kind==REMOVAL_REQUEST){
                /* Test owner receives a correlation only: no backend is called. */
                REQUIRE(!requests && action.request.revision==42 && !memcmp(action.request.plan,plan.plan,16) &&
                    !memcmp(action.request.installation,plan.installation,16) && !memcmp(action.request.scope,plan.scope,16));
                ++requests;pending="REMOVAL_REQUESTED revision=42";
                REQUIRE(removal_review_frame(view,info.width,info.height,&action)==NX_OK && action.kind==REMOVAL_NONE);
            }
            if(focus){
                uint32_t index;nx_semantic_view acknowledgement;
                REQUIRE(nx_semantics_find(ctx,200,&index)==NX_OK && nx_semantics_get(ctx,index,&acknowledgement)==NX_OK);
                uint64_t target=acknowledgement.state&NX_SEM_DISABLED?201:200;
                REQUIRE(nx_focus_set(ctx,target)==NX_OK && removal_review_frame(view,info.width,info.height,&action)==NX_OK && action.kind==REMOVAL_NONE);
                focus=0;
            }
            nx_result published=nx_window_publish_semantics(window,ctx);REQUIRE(published==NX_OK || published==NX_ERROR_UNSUPPORTED);
            const nx_command *commands=NULL;size_t count=0;nx_color background;nx_software_view_outcome outcome;
            REQUIRE(nx_frame_commands(ctx,&commands,&count)==NX_OK && nx_theme_color(ctx,NX_COLOR_BACKGROUND,&background)==NX_OK &&
                nx_software_view_render(renderer,commands,count,background,&outcome)==NX_OK);
            dirty=outcome!=NX_SOFTWARE_VIEW_READY;if(!dirty && pending){puts(pending);pending=NULL;}
        }
        uint32_t ready=0;REQUIRE(nx_window_poll_with_fd(window,UINT32_MAX,control?STDIN_FILENO:-1,&ready)==NX_OK);
        nx_event event;int available;
        do{REQUIRE(nx_window_next_event(window,&event,&available)==NX_OK);
            if(available){
                if(event.type==NX_EVENT_KEY_DOWN && event.key==NX_KEY_ESCAPE){done=1;break;}
                REQUIRE(nx_context_queue_event(ctx,&event)==NX_OK);dirty=1;
            }
        }while(available);
        size_t routed=0;REQUIRE(nx_window_route_events(window,ctx,&routed)==NX_OK);if(routed)dirty=1;
        if(control && ready){
            REQUIRE(used<sizeof(input)-1);ssize_t n=read(STDIN_FILENO,input+used,sizeof(input)-used-1);
            REQUIRE(n>0 || (n<0 && (errno==EAGAIN || errno==EINTR)));if(n>0)used+=(size_t)n;input[used]=0;
            char *end=memchr(input,'\n',used);
            if(end){
                size_t length=(size_t)(end-input);
                if(length==5 && !memcmp(input,"close",5))done=1;
                else {
                    REQUIRE(!reopened && length==6 && !memcmp(input,"reopen",6));
                    nx_result detached=nx_window_publish_semantics(window,NULL);REQUIRE(detached==NX_OK || detached==NX_ERROR_UNSUPPORTED);
                    removal_review_destroy(view);view=NULL;
                    REQUIRE(removal_review_create(owner,fonts,&view)==NX_OK);
                    reopened=1;dirty=1;focus=1;pending="REMOVAL_REOPENED";
                }
                used-=length+1;memmove(input,end+1,used);
            }
        }
    }
    REQUIRE(requests==removal_owner_requests(owner));printf("REMOVAL_CLOSED requests=%u reopened=%d\n",requests,reopened);
cleanup:
    if(window){nx_result result=nx_window_publish_semantics(window,NULL);if(result!=NX_OK && result!=NX_ERROR_UNSUPPORTED)okay=0;}
    if(renderer && nx_software_view_close(&renderer)!=NX_OK)okay=0;
    removal_review_destroy(view);if(window)nx_window_destroy(window);removal_owner_destroy(owner);
    if(fonts)nx_font_collection_destroy(fonts);
    if(font)nx_font_close(font);
    if(file)fclose(file);
    free(large_entries);free(bytes);return okay?0:1;
}
