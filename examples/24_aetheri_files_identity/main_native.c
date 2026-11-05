#define _POSIX_C_SOURCE 200809L
#include "scene.h"
#include <nexaui/software.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Ordinary-window integration probe, not Aetheri's production Files app. */
static volatile sig_atomic_t stop_requested;
static void request_stop(int signal_number){(void)signal_number;stop_requested=1;}
static unsigned char *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);
    if(n<=0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}
    fclose(f);*size=(size_t)n;return p;
}
static int navigation(nx_key key) {
    return key==NX_KEY_UP || key==NX_KEY_DOWN || key==NX_KEY_HOME || key==NX_KEY_END ||
        key==NX_KEY_PAGE_UP || key==NX_KEY_PAGE_DOWN;
}
int main(int argc,char **argv) {
    int okay=0,done=0,root=-1,focused=0,dirty=1,shown=0,ax_unavailable=0,trace=getenv("NEXAUI_FILES_NATIVE_TRACE")!=NULL;
    size_t font_length=0;unsigned char *font_bytes=NULL;nx_font *font=NULL;nx_font_collection *fonts=NULL;
    struct ae_files *model=NULL;files_scene *scene=NULL;nx_window *window=NULL;nx_software_view *renderer=NULL;
    char previous_selected[AE_FILES_LABEL+1]={0},previous_visible[AE_FILES_LABEL+1]={0};
    struct sigaction action={0};action.sa_handler=request_stop;sigemptyset(&action.sa_mask);
    if(sigaction(SIGINT,&action,NULL) || sigaction(SIGTERM,&action,NULL))goto cleanup;
    if(argc!=3 || (root=open(argv[1],O_RDONLY|O_DIRECTORY|O_CLOEXEC))<0)goto cleanup;
    enum ae_files_result model_result;model=ae_files_create(root,&model_result);close(root);root=-1;
    if(!model || model_result!=AE_FILES_OK || !(font_bytes=read_file(argv[2],&font_length)) ||
       nx_font_open(font_bytes,font_length,NULL,&font)!=NX_OK || nx_font_collection_create(NULL,&fonts)!=NX_OK ||
       nx_font_collection_add(fonts,font)!=NX_OK || files_scene_create(model,fonts,&scene)!=NX_OK)goto cleanup;
    nx_window_desc desc={"NexaUI Files experiment",360,140,0,{0}};
    if(nx_window_create(&desc,&window)!=NX_OK || nx_software_view_open(window,&renderer)!=NX_OK)goto cleanup;
    nx_context *ctx=files_scene_context(scene);okay=1;
    while(okay && !done) {
        if(nx_window_poll(window,dirty?16:1000)!=NX_OK){okay=0;break;}
        if(stop_requested){done=1;break;}
        nx_window_info info;if(nx_window_get_info(window,&info)!=NX_OK){okay=0;break;}
        if(info.should_close){done=1;break;}
        int drawable=info.width>=40 && info.height>=28;
        nx_event event;int available;
        do {
            if(nx_window_next_event(window,&event,&available)!=NX_OK){okay=0;break;}
            if(!available)break;
            if(event.type==NX_EVENT_FOCUS){focused=!!event.focused;dirty=1;}
            if(event.type==NX_EVENT_RESIZE)dirty=1;
            if(event.type==NX_EVENT_KEY_DOWN && focused && event.key==NX_KEY_Q &&
               (event.modifiers&(NX_MOD_CONTROL|NX_MOD_SHIFT|NX_MOD_ALT|NX_MOD_SUPER))==NX_MOD_CONTROL){done=1;break;}
            if(event.type==NX_EVENT_KEY_DOWN && focused && event.key==NX_KEY_R &&
               (event.modifiers&(NX_MOD_CONTROL|NX_MOD_SHIFT|NX_MOD_ALT|NX_MOD_SUPER))==NX_MOD_CONTROL && !event.repeat) {
                if(ae_files_refresh(model)!=AE_FILES_OK || files_scene_sync(scene)!=NX_OK){okay=0;break;}
                if(trace){puts("FILES_NATIVE_REFRESHED");fflush(stdout);}
                dirty=1;continue;
            }
            if(event.type==NX_EVENT_KEY_DOWN && focused && drawable && navigation(event.key) && !event.modifiers) {
                if(files_scene_key(scene,event.key,1)!=NX_OK){okay=0;break;}
                dirty=1;continue;
            }
            if(event.type==NX_EVENT_WHEEL && drawable && event.x>=0 && event.x<info.width &&
               event.y>=0 && event.y<fminf(info.height,1764.0f)) {
                if(files_scene_scroll(scene,-(double)event.delta_y*84)!=NX_OK){okay=0;break;}
                if(trace && event.delta_y>0){puts("FILES_NATIVE_SCROLL_UP");fflush(stdout);}
                dirty=1;continue;
            }
            if(trace && event.type==NX_EVENT_POINTER_MOVE){puts("FILES_NATIVE_POINTER");fflush(stdout);}
            if((drawable && (event.type==NX_EVENT_POINTER_DOWN || event.type==NX_EVENT_POINTER_UP ||
               event.type==NX_EVENT_POINTER_MOVE || event.type==NX_EVENT_KEY_DOWN || event.type==NX_EVENT_KEY_UP)) ||
               event.type==NX_EVENT_FOCUS) {
                if(nx_context_queue_event(ctx,&event)!=NX_OK){okay=0;break;}dirty=1;
            }
        } while(available);
        if(!okay || done)break;
        size_t routed=0;
        if(nx_window_route_events(window,ctx,&routed)!=NX_OK){okay=0;break;}
        if(routed)dirty=1;
        if(!dirty) {
            if(ax_unavailable) {
                nx_result retry=nx_window_publish_semantics(window,ctx);
                if(retry!=NX_OK && retry!=NX_ERROR_UNSUPPORTED){okay=0;break;}
                ax_unavailable=retry==NX_ERROR_UNSUPPORTED;
            }
            continue;
        }
        float view_height=fminf(info.height,1764.0f);uint64_t old_focus=nx_focus_id(ctx);
        if(nx_context_set_text_raster_scale(ctx,info.scale)!=NX_OK){okay=0;break;}
        if(info.width>=40 && view_height>=28) {
            if(files_scene_frame(scene,info.width,view_height)!=NX_OK){okay=0;break;}
        } else if(nx_begin_frame(ctx,info.width,info.height,0)!=NX_OK || nx_end_frame(ctx)!=NX_OK){okay=0;break;}
        nx_result ax=nx_window_publish_semantics(window,ctx);
        if(ax!=NX_OK && ax!=NX_ERROR_UNSUPPORTED){okay=0;break;}
        ax_unavailable=ax==NX_ERROR_UNSUPPORTED;
        const nx_command *commands=NULL;size_t count=0;nx_software_view_outcome outcome;
        if(nx_frame_commands(ctx,&commands,&count)!=NX_OK ||
           nx_software_view_render(renderer,commands,count,(nx_color){.08f,.1f,.14f,1},&outcome)!=NX_OK){okay=0;break;}
        dirty=outcome!=NX_SOFTWARE_VIEW_READY || old_focus!=nx_focus_id(ctx);
        if(outcome==NX_SOFTWARE_VIEW_READY && !shown){if(trace){puts("FILES_NATIVE_READY");fflush(stdout);}shown=1;}
        if(trace && shown) {
            const struct ae_files_entry *selected=ae_files_entry(model,ae_files_selected(model));
            if(selected && strcmp(previous_selected,selected->label)) {
                snprintf(previous_selected,sizeof(previous_selected),"%s",selected->label);
                printf("FILES_NATIVE_SELECTED %s\n",previous_selected);fflush(stdout);
            }
            size_t nodes=0;nx_semantic_view visible;
            if(nx_semantics_count(ctx,&nodes)==NX_OK && nodes>1 && nx_semantics_get(ctx,1,&visible)==NX_OK &&
               visible.name.length<sizeof(previous_visible)) {
                char label[AE_FILES_LABEL+1];memcpy(label,visible.name.data,visible.name.length);label[visible.name.length]=0;
                if(strcmp(previous_visible,label)){memcpy(previous_visible,label,visible.name.length+1);
                    printf("FILES_NATIVE_VISIBLE %s\n",label);fflush(stdout);}
            }
        }
    }
cleanup:
    if(window && nx_window_publish_semantics(window,NULL)!=NX_OK)okay=0;
    if(renderer && nx_software_view_close(&renderer)!=NX_OK)okay=0;
    if(window)nx_window_destroy(window);
    if(scene)files_scene_destroy(scene);
    if(model)ae_files_destroy(model);
    if(fonts)nx_font_collection_destroy(fonts);
    if(font)nx_font_close(font);
    free(font_bytes);if(root>=0)close(root);
    if(okay && done && trace){puts("FILES_NATIVE_CLOSED");fflush(stdout);}
    if(!okay)fputs("Native Files experiment failed\n",stderr);
    return okay && done?0:1;
}
