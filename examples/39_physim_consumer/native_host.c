#include "view.h"
#include <nexaui/software.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *read_font(const char *path,size_t *bytes) {
    FILE *file=fopen(path,"rb");if(!file)return NULL;uint8_t *data=NULL;
    if(!fseek(file,0,SEEK_END)) {
        long length=ftell(file);
        if(length>0&&length<=16*1024*1024&&!fseek(file,0,SEEK_SET)) {
            data=malloc((size_t)length);
            if(data&&fread(data,1,(size_t)length,file)!=(size_t)length) { free(data);data=NULL; }
            if(data)*bytes=(size_t)length;
        }
    }
    fclose(file);return data;
}
static nx_result retire_ime(nx_window *window) {
    nx_result r=nx_window_set_ime_surrounding_text(window,0,0,NULL,0,0,0);
    if(r==NX_OK)r=nx_window_set_ime_enabled(window,0);
    char preedit[128];size_t length=0;int32_t begin=0,end=0;
    if(r==NX_OK)r=nx_window_get_ime_preedit(window,preedit,sizeof(preedit),&length,&begin,&end);
    if(r==NX_OK&&length)r=NX_ERROR_STATE;
    if(r==NX_OK) { puts("PHYSIM_IME_RETIRED length=0");fflush(stdout); }
    return r;
}
static nx_result replace_view(physim_snapshot *model,ps_plot_view *plot,const nx_font_collection *fonts,
    nx_window *window,const nx_window_info *info,double backing,physim_view **view,nx_context **context) {
    physim_view *next=NULL;nx_context *next_context=NULL;
    nx_result r=nx_context_create(NULL,&next_context);
    if(r==NX_OK)r=physim_view_create(model,plot,fonts,&next);
    if(r==NX_OK)r=physim_view_set_ime_window(next,window);
    if(r==NX_OK)r=retire_ime(window);
    unsigned action=0;
    if(r==NX_OK)r=physim_view_frame(next,next_context,info->width,info->height,backing,&action);
    if(r==NX_OK&&action)r=NX_ERROR_STATE;
    if(r==NX_OK)r=nx_focus_set(next_context,10);
    /* Deliberately discard this staging frame. It establishes new semantics;
       the next owner-loop frame rebuilds and presents the same model. */
    if(next) { nx_result complete=physim_view_complete(next);if(r==NX_OK)r=complete; }
    if(r==NX_OK)r=nx_window_publish_semantics(window,next_context);
    if(r!=NX_OK) { physim_view_destroy(&next);nx_context_destroy(next_context);return r; }
    physim_view *old=*view;nx_context *old_context=*context;
    *view=next;*context=next_context;
    r=physim_view_destroy(&old);nx_context_destroy(old_context);return r;
}
int main(void) {
    const char *isolated=getenv("NEXAUI_TEST_PRIVATE_WAYLAND"),*display=getenv("WAYLAND_DISPLAY");
    const char *path=getenv("NEXAUI_PHYSIM_FONT"),*test_case=getenv("NEXAUI_PHYSIM_CASE");
    if(!isolated||strcmp(isolated,"1")||!display||strcmp(display,"nexaui-test")||!path||!test_case||
        !getenv("NEXAUI_TEST_ATSPI_ADDRESS")||(strcmp(test_case,"full")&&strcmp(test_case,"ime-cancel")&&strcmp(test_case,"ime-replace")&&strcmp(test_case,"ime-close")))return 2;
    int result=1,leased=0,need_frame=1,first=1,preedit_active=0;unsigned frames=0,replacements=0,preedits=0,cancellations=0;
    double backing=1;uint64_t logged_generation=0;uint32_t logged_width=0,logged_height=0;
    physim_snapshot *model=malloc(sizeof(*model));ps_plot_view plot={0};physim_view *view=NULL;
    nx_context *context=NULL;nx_window *window=NULL;nx_software_view *software=NULL;
    nx_font *font=NULL;nx_font_collection *fonts=NULL;size_t bytes=0;uint8_t *data=read_font(path,&bytes);
    nx_window_desc desc={.title_utf8="Original Physim reference - F6 replace; F7 backing",.width=800,.height=560};
    if(!model||!data||physim_model_run(1.5,.45,1,model)!=NX_OK||nx_font_open(data,bytes,NULL,&font)!=NX_OK||
        nx_font_collection_create(NULL,&fonts)!=NX_OK||nx_font_collection_add(fonts,font)!=NX_OK||
        nx_window_create(&desc,&window)!=NX_OK||nx_software_view_open(window,&software)!=NX_OK||
        nx_context_create(NULL,&context)!=NX_OK||physim_view_create(model,&plot,fonts,&view)!=NX_OK||
        physim_view_set_ime_window(view,window)!=NX_OK)goto done;
    for(;;) {
        nx_window_info info;size_t routed=0;
        if(nx_window_poll(window,need_frame?0:200)!=NX_OK||nx_window_get_info(window,&info)!=NX_OK||
            nx_window_route_events(window,context,&routed)!=NX_OK)goto done;
        if(info.should_close) { result=0;break; }
        if(!need_frame&&!routed)continue;need_frame=0;
        unsigned action=0;
        if(++frames>150||physim_view_frame(view,context,info.width,info.height,backing,&action)!=NX_OK)goto done;
        leased=1;
        if(first) { if(nx_focus_set(context,10)!=NX_OK)goto done;first=0;need_frame=1; }
        if(nx_window_publish_semantics(window,context)!=NX_OK)goto done;
        const nx_command *commands=NULL;size_t count=0;nx_software_view_outcome outcome=NX_SOFTWARE_VIEW_READY;
        if(nx_frame_commands(context,&commands,&count)!=NX_OK)goto done;
        for(size_t i=0;i<count;++i) {
            if(commands[i].type==NX_COMMAND_BITMAP&&commands[i].bitmap.format==NX_BITMAP_RGBA8&&
                commands[i].rect.x==info.width*.5f+8) {
                if(commands[i].bitmap.width!=logged_width||commands[i].bitmap.height!=logged_height||model->generation!=logged_generation) {
                    printf("PHYSIM_SURFACE generation=%llu width=%u height=%u backing=%.0f\n",(unsigned long long)model->generation,
                        commands[i].bitmap.width,commands[i].bitmap.height,backing);
                    logged_width=commands[i].bitmap.width;logged_height=commands[i].bitmap.height;
                }
            }
        }
        if(nx_software_view_render(software,commands,count,(nx_color){1,1,1,1},&outcome)!=NX_OK||physim_view_complete(view)!=NX_OK)goto done;
        leased=0;if(outcome!=NX_SOFTWARE_VIEW_READY)need_frame=1;
        if(model->generation!=logged_generation) {
            printf("PHYSIM_MODEL generation=%llu length=%.17g angle120=%.17g\n",(unsigned long long)model->generation,model->length.value,model->samples[120].values[0]);
            logged_generation=model->generation;
        }
        char preedit[128];size_t length=0;int32_t begin=0,end=0;
        if(nx_window_get_ime_preedit(window,preedit,sizeof(preedit),&length,&begin,&end)!=NX_OK)goto done;
        if(length&&!preedit_active) { ++preedits;preedit_active=1;printf("PHYSIM_PREEDIT focus=%llu length=%zu generation=%llu\n",(unsigned long long)nx_focus_id(context),length,(unsigned long long)model->generation); }
        if(!length&&preedit_active) { ++cancellations;preedit_active=0;printf("PHYSIM_PREEDIT_END focus=%llu generation=%llu\n",(unsigned long long)nx_focus_id(context),(unsigned long long)model->generation); }
        fflush(stdout);
        /* Native-consumed keys belong to the text system. Keep fixture
           commands on fresh, unmodified physical downs only. */
        const nx_event *events=NULL;size_t event_count=0;int replace=0,change_backing=0;
        if(nx_frame_events(context,&events,&event_count)!=NX_OK)goto done;
        for(size_t i=0;i<event_count;++i)if(events[i].type==NX_EVENT_KEY_DOWN&&!events[i].repeat&&!events[i].modifiers) {
            if(events[i].key==NX_KEY_F6)replace=1;
            if(events[i].key==NX_KEY_F7)change_backing=1;
        }
        if(action==16) { result=0;break; }
        if(action==17)replace=1;
        if((action&&action<=15)||action==PHYSIM_INSPECT_NEXT||action==PHYSIM_INSPECT_PICK||
            action==PHYSIM_TOGGLE_TREE||action==PHYSIM_TOGGLE_CAMERA||action==PHYSIM_RESET_CAMERA) {
            int valid=0;if(physim_view_dispatch(view,action,&valid)!=NX_OK)goto done;
            printf("PHYSIM_ACTION id=%u valid=%d generation=%llu length=%.17g low=%.17g high=%.17g\n",action,valid,
                (unsigned long long)model->generation,model->length.value,plot.low[0],plot.high[0]);fflush(stdout);need_frame=1;
        }
        if(change_backing) { backing=backing==1?2:1;need_frame=1;puts("PHYSIM_BACKING_CHANGED");fflush(stdout); }
        if(replace) {
            if(++frames>150||replace_view(model,&plot,fonts,window,&info,backing,&view,&context)!=NX_OK)goto done;
            if(preedit_active) { ++cancellations;preedit_active=0; }
            ++replacements;first=0;need_frame=1;
            printf("PHYSIM_REPLACED count=%u generation=%llu length=%.17g low=%.17g high=%.17g\n",replacements,
                (unsigned long long)model->generation,model->length.value,plot.low[0],plot.high[0]);fflush(stdout);
        }
    }
done:
    /* A failed end_frame can still own a viewport lease. Discard the stream
       before teardown; STATE is expected only when no successful lease exists. */
    if(view) { nx_result r=physim_view_complete(view);if(r!=NX_OK&&(leased||r!=NX_ERROR_STATE))result=1; }
    if(window&&(nx_window_publish_semantics(window,NULL)!=NX_OK||retire_ime(window)!=NX_OK))result=1;
    if(software&&nx_software_view_close(&software)!=NX_OK)result=1;
    if(physim_view_destroy(&view)!=NX_OK)result=1;
    nx_context_destroy(context);if(window)nx_window_destroy(window);
    nx_font_collection_destroy(fonts);nx_font_close(font);free(data);
    unsigned expected_replacements=!strcmp(test_case,"full")||!strcmp(test_case,"ime-replace")?1u:0u;
    uint64_t expected_generation=!strcmp(test_case,"ime-close")?1u:2u;
    if(!model||model->generation!=expected_generation||replacements!=expected_replacements||
        (strcmp(test_case,"full")&&(!preedits||(!cancellations&&strcmp(test_case,"ime-close")))))result=1;
    printf("PHYSIM_EXIT result=%d generation=%llu length=%.17g replacements=%u preedits=%u cancellations=%u frames=%u\n",result,
        (unsigned long long)(model?model->generation:0),model?model->length.value:0,replacements,preedits,cancellations,frames);
    free(model);return result;
}
