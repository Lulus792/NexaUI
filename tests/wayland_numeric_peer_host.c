#include "nexaui/nexaui.h"
#include "nexaui/software.h"
#include "fixture_font.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A visible native fixture: the driver sends actual keyboard input. */
static volatile sig_atomic_t confirm_requested;
static void request_confirm(int signal_number) {
    (void)signal_number;
    confirm_requested=1;
}
static int capture_commands(const char *path,const nx_command *commands,size_t count,
                            uint32_t width,uint32_t height) {
    if(!width || !height || width>4096 || height>4096)return 0;
    size_t stride=(size_t)width*4u, size=stride*height;
    unsigned char *pixels=malloc(size);if(!pixels)return 0;
    FILE *output=NULL;int okay=0;
    if(nx_headless_render(commands,count,pixels,width,height,stride,(nx_color){1,1,1,1})!=NX_OK)goto done;
    output=fopen(path,"wb");if(!output || fprintf(output,"P6\n%u %u\n255\n",width,height)<0)goto done;
    for(size_t i=0;i<(size_t)width*height;++i)
        if(fwrite(pixels+4u*i,1,3,output)!=3)goto done;
    okay=1;
done:
    if(output && fclose(output))okay=0;
    free(pixels);return okay;
}
/* Submit every fixture control during the guard frame, including unrelated
   actions which may already have been admitted to the context. */
static int publish_preedit_guard(nx_context *ctx,const nx_numeric_field_desc *number_desc,
                                 nx_numeric_field *number,const nx_text_layout *label,
                                 const nx_window_info *info,int ax,unsigned *other_count) {
    nx_interaction_state state,other_state={0};int valid;
    nx_control_desc apply={0};apply.id=2;apply.bounds=(nx_rect){20,70,120,36};
    apply.name=(nx_utf8_span){"Apply",5};apply.label_layout=label;
    apply.label_origin=(nx_point){55,78};apply.glyph_cache_bytes=65536;apply.disabled=1;
    nx_control_desc other={0};other.id=3;other.bounds=(nx_rect){160,70,140,36};
    other.name=(nx_utf8_span){"Other action",12};
    if(nx_begin_frame(ctx,info->width,info->height,0)!=NX_OK ||
       nx_numeric_field_widget(ctx,number_desc,number,&state,&valid)!=NX_OK ||
       nx_button(ctx,&apply,&state)!=NX_OK || (ax && nx_button(ctx,&other,&other_state)!=NX_OK) ||
       nx_end_frame(ctx)!=NX_OK)return 0;
    if(other_state.clicked) { *other_count+=other_state.clicked;printf("OTHER_ACTION %u\n",*other_count); }
    return 1;
}
int main(int argc, char **argv) {
    int ime=(argc==2 && !strcmp(argv[1],"--ime")) || (argc==3 && !strcmp(argv[1],"--apply-ime"));
    int apply_mode=argc==3 && (!strcmp(argv[1],"--apply") || !strcmp(argv[1],"--apply-ime"));
    int ax=apply_mode && getenv("NEXAUI_NUMERIC_ATSPI")!=NULL;
    if(argc!=1 && !ime && !apply_mode)return 2;
    if(ime && signal(SIGUSR1,request_confirm)==SIG_ERR)return 2;
    nx_window *window=NULL; nx_context *ctx=NULL; nx_font *font=NULL;
    nx_font_collection *fonts=NULL; nx_numeric_field *number=NULL;
    nx_software_view *view=NULL; nx_text_layout *apply_label=NULL;
    unsigned char *font_data=NULL; FILE *font_file=NULL;
    int okay=0,ready=0,valid=0;
    uint8_t bytes[512]; font_fixture(bytes);
    const void *font_source=bytes; size_t font_length=sizeof(bytes);
    if(apply_mode) {
        font_file=fopen(argv[2],"rb");long length=0;
        if(!font_file || fseek(font_file,0,SEEK_END) || (length=ftell(font_file))<=0 ||
           length>64*1024*1024 || fseek(font_file,0,SEEK_SET))goto done;
        font_data=malloc((size_t)length);
        if(!font_data || fread(font_data,1,(size_t)length,font_file)!=(size_t)length)goto done;
        font_source=font_data;font_length=(size_t)length;
    }
    nx_window_desc window_desc={"Numeric clipboard peer",320,120,0,{0}};
    nx_text_clipboard clipboard={0};
    if(nx_window_create(&window_desc,&window)!=NX_OK || nx_context_create(NULL,&ctx)!=NX_OK ||
       nx_font_open(font_source,font_length,NULL,&font)!=NX_OK || nx_font_collection_create(NULL,&fonts)!=NX_OK ||
       nx_font_collection_add(fonts,font)!=NX_OK || nx_numeric_field_create(1234,1024,&number)!=NX_OK ||
       nx_text_clipboard_for_window(window,&clipboard)!=NX_OK ||
       nx_numeric_field_set_clipboard(number,&clipboard)!=NX_OK ||
       (ime && nx_numeric_field_set_ime_window(number,window)!=NX_OK) ||
       nx_software_view_open(window,&view)!=NX_OK ||
       (apply_mode && nx_text_layout_create(fonts,"Apply",5,NX_BIDI_LTR,16,100,&apply_label)!=NX_OK)) goto done;
    setvbuf(stdout,NULL,_IOLBF,0);
    nx_numeric_field_desc desc={0};desc.id=1;desc.bounds=(nx_rect){20,20,250,40};
    desc.name=(nx_utf8_span){"Number",6};desc.fonts=fonts;desc.pixels_per_em=16;
    desc.glyph_cache_bytes=65536;desc.minimum=-100000;desc.maximum=100000;
    char previous[128]="";int ime_stage=0,dirty=1,button_focused=-1,composition_active=-1;unsigned other_count=0;nx_window_info previous_info={0};
    for(;;) {
        nx_window_info info;size_t routed=0;nx_interaction_state state;
        if(nx_window_poll(window,50)!=NX_OK || nx_window_get_info(window,&info)!=NX_OK)goto done;
        if(apply_mode && ime && composition_active!=1) {
            size_t length=0;int32_t begin=0,end=0;
            if(nx_window_get_ime_preedit(window,NULL,0,&length,&begin,&end)!=NX_OK)goto done;
            if(length && nx_focus_id(ctx)==desc.id) {
                if(!publish_preedit_guard(ctx,&desc,number,apply_label,&info,ax,&other_count))goto done;
                if(ax && nx_window_publish_semantics(window,ctx)!=NX_OK)goto done;
                puts("PREEDIT_GUARD_PUBLISHED");puts("COMPOSITION_ACTIVE 1");composition_active=1;dirty=1;
            }
        }
        if(nx_window_route_events(window,ctx,&routed)!=NX_OK)goto done;
        if(info.should_close){okay=1;break;}
        if(routed || confirm_requested || info.width!=previous_info.width || info.height!=previous_info.height || info.scale!=previous_info.scale)dirty=1;
        previous_info=info;
        if(!dirty)continue;
        if(nx_begin_frame(ctx,info.width,info.height,0)!=NX_OK)goto done;
        const nx_event *events=NULL;size_t event_count=0;int close_requested=0,pointer_release=0,preedit_event=0,pointer_press=0;
        if(nx_frame_events(ctx,&events,&event_count)!=NX_OK)goto done;
        for(size_t i=0;i<event_count;++i) {
            if(events[i].type==NX_EVENT_KEY_DOWN && events[i].key==NX_KEY_ESCAPE)close_requested=1;
            if(events[i].type==NX_EVENT_POINTER_UP && events[i].button==NX_BUTTON_LEFT)pointer_release=1;
            if(events[i].type==NX_EVENT_POINTER_DOWN && events[i].button==NX_BUTTON_LEFT)pointer_press=1;
            if(events[i].type==NX_EVENT_IME_PREEDIT_CHANGED)preedit_event=1;
            if(apply_mode && getenv("NEXAUI_NUMERIC_POINTER_TRACE") && events[i].type==NX_EVENT_POINTER_MOVE)
                printf("POINTER %.2f %.2f\n",events[i].x,events[i].y);
        }
        if(apply_mode && ime && preedit_event && pointer_press)puts("SAME_BATCH");
        if(nx_numeric_field_widget(ctx,&desc,number,&state,&valid)!=NX_OK)goto done;
        nx_interaction_state button_state={0},other_state={0};
        if(apply_mode) {
            nx_control_desc apply={0};apply.id=2;apply.bounds=(nx_rect){20,70,120,36};
            apply.name=(nx_utf8_span){"Apply",5};apply.label_layout=apply_label;
            apply.label_origin=(nx_point){55,78};apply.glyph_cache_bytes=65536;
            if(ime) {
                size_t length=0;int32_t begin=0,end=0;
                if(nx_window_get_ime_preedit(window,NULL,0,&length,&begin,&end)!=NX_OK)goto done;
                apply.disabled=length!=0;
                if(apply.disabled!=composition_active) {
                    composition_active=apply.disabled;printf("COMPOSITION_ACTIVE %d\n",composition_active);
                }
            }
            if(nx_button(ctx,&apply,&button_state)!=NX_OK)goto done;
            if(ax) {
                nx_control_desc other={0};other.id=3;other.bounds=(nx_rect){160,70,140,36};
                other.name=(nx_utf8_span){"Other action",12};
                if(nx_button(ctx,&other,&other_state)!=NX_OK)goto done;
            }
        }
        if(nx_end_frame(ctx)!=NX_OK)goto done;
        if(other_state.clicked) {other_count+=other_state.clicked;printf("OTHER_ACTION %u\n",other_count);}
        if(ax && nx_window_publish_semantics(window,ctx)!=NX_OK)goto done;
        if(apply_mode && button_state.focused!=button_focused) {
            button_focused=button_state.focused;printf("BUTTON_FOCUS %d\n",button_focused);
        }
        if(apply_mode && button_state.clicked) {
            int accepted=0;double applied_value=0;
            if(nx_numeric_field_commit(number,desc.minimum,desc.maximum,&accepted)!=NX_OK ||
               nx_numeric_field_value(number,&applied_value)!=NX_OK)goto done;
            printf("APPLY %d %.0f\n",accepted,applied_value);
        }
        if(confirm_requested) {
            int accepted=0;confirm_requested=0;
            if(nx_numeric_field_commit(number,desc.minimum,desc.maximum,&accepted)!=NX_OK || !accepted)goto done;
        }
        if(close_requested){okay=1;break;}
        const nx_command *commands=NULL;size_t count=0;nx_software_view_outcome outcome;
        if(nx_frame_commands(ctx,&commands,&count)!=NX_OK ||
           nx_software_view_render(view,commands,count,(nx_color){1,1,1,1},&outcome)!=NX_OK)goto done;
        dirty=outcome!=NX_SOFTWARE_VIEW_READY;
        if(outcome==NX_SOFTWARE_VIEW_READY && !ready){
            const char *capture=getenv("NEXAUI_NUMERIC_CAPTURE");
            if(apply_mode && capture && !capture_commands(capture,commands,count,info.width,info.height))goto done;
            puts("READY");ready=1;
        }
        nx_utf8_span draft;double value;
        if(nx_numeric_field_text(number,&draft)!=NX_OK || nx_numeric_field_value(number,&value)!=NX_OK || draft.length>80)goto done;
        if(apply_mode && ime && pointer_release) {
            size_t length=0;int32_t begin=0,end=0;
            if(nx_window_get_ime_preedit(window,NULL,0,&length,&begin,&end)!=NX_OK)goto done;
            printf("AFTER_CLICK %zu %.0f %d\n",length,value,state.focused);
        }
        if(ime) {
            char preedit[16];size_t preedit_length=0;int32_t begin=0,end=0;
            if(nx_window_get_ime_preedit(window,preedit,sizeof(preedit),&preedit_length,&begin,&end)!=NX_OK)goto done;
            if(!ime_stage && state.focused && preedit_length==1 && preedit[0]=='\'' &&
               draft.length==5 && !memcmp(draft.data,"12345",5) && value==12345) {
                puts("PREEDIT");ime_stage=1;
            } else if(ime_stage==1 && !preedit_length && draft.length==7 &&
                      !memcmp(draft.data,"12345\xc3\xa1",7) && value==12345 && !valid) {
                puts("INVALID");ime_stage=2;
            } else if(ime_stage==2 && draft.length==8 && !memcmp(draft.data,"12345\xc3\xa1z",8) &&
                      value==12345 && !valid) {puts("SHORTCUT_AS_TEXT");okay=1;break;}
            else if(ime_stage==2 && draft.length==5 && !memcmp(draft.data,"12345",5) &&
                    value==12345 && valid) {puts("UNDONE");okay=1;break;}
        }
        char current[128];int n=snprintf(current,sizeof(current),"STATE %.*s %.0f %d %d",(int)draft.length,draft.data,value,valid,state.focused);
        if(n<0 || (size_t)n>=sizeof(current))goto done;
        if(strcmp(current,previous)){puts(current);memcpy(previous,current,(size_t)n+1);}
    }
done:
    if(ax && window)nx_window_publish_semantics(window,NULL);
    nx_software_view_close(&view);nx_numeric_field_destroy(number);nx_context_destroy(ctx);
    nx_text_layout_destroy(apply_label);nx_font_collection_destroy(fonts);nx_font_close(font);nx_window_destroy(window);
    if(font_file)fclose(font_file);free(font_data);
    return okay?0:1;
}
