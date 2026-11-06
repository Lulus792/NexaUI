#include "nexaui/nexaui.h"
#include "nexaui/software.h"
#include "fixture_font.h"
#include <stdio.h>
#include <string.h>

/* A visible native fixture: the driver sends actual keyboard input. */
int main(int argc, char **argv) {
    int ime=argc==2 && !strcmp(argv[1],"--ime");
    if(argc!=1 && !ime)return 2;
    nx_window *window=NULL; nx_context *ctx=NULL; nx_font *font=NULL;
    nx_font_collection *fonts=NULL; nx_numeric_field *number=NULL;
    nx_software_view *view=NULL; int okay=0,ready=0,valid=0;
    uint8_t bytes[512]; font_fixture(bytes);
    nx_window_desc window_desc={"Numeric clipboard peer",320,120,0,{0}};
    nx_text_clipboard clipboard={0};
    if(nx_window_create(&window_desc,&window)!=NX_OK || nx_context_create(NULL,&ctx)!=NX_OK ||
       nx_font_open(bytes,sizeof(bytes),NULL,&font)!=NX_OK || nx_font_collection_create(NULL,&fonts)!=NX_OK ||
       nx_font_collection_add(fonts,font)!=NX_OK || nx_numeric_field_create(1234,1024,&number)!=NX_OK ||
       nx_text_clipboard_for_window(window,&clipboard)!=NX_OK ||
       nx_numeric_field_set_clipboard(number,&clipboard)!=NX_OK ||
       (ime && nx_numeric_field_set_ime_window(number,window)!=NX_OK) ||
       nx_software_view_open(window,&view)!=NX_OK) goto done;
    setvbuf(stdout,NULL,_IOLBF,0);
    nx_numeric_field_desc desc={0};desc.id=1;desc.bounds=(nx_rect){20,20,250,40};
    desc.name=(nx_utf8_span){"Number",6};desc.fonts=fonts;desc.pixels_per_em=16;
    desc.glyph_cache_bytes=65536;desc.minimum=-100000;desc.maximum=100000;
    char previous[128]="";int ime_stage=0,dirty=1;nx_window_info previous_info={0};
    for(;;) {
        nx_window_info info;size_t routed=0;nx_interaction_state state;
        if(nx_window_poll(window,50)!=NX_OK || nx_window_get_info(window,&info)!=NX_OK ||
           nx_window_route_events(window,ctx,&routed)!=NX_OK)goto done;
        if(info.should_close){okay=1;break;}
        if(routed || info.width!=previous_info.width || info.height!=previous_info.height || info.scale!=previous_info.scale)dirty=1;
        previous_info=info;
        if(!dirty)continue;
        if(nx_begin_frame(ctx,info.width,info.height,0)!=NX_OK)goto done;
        const nx_event *events=NULL;size_t event_count=0;int close_requested=0;
        if(nx_frame_events(ctx,&events,&event_count)!=NX_OK)goto done;
        for(size_t i=0;i<event_count;++i)
            if(events[i].type==NX_EVENT_KEY_DOWN && events[i].key==NX_KEY_ESCAPE)close_requested=1;
        if(nx_numeric_field_widget(ctx,&desc,number,&state,&valid)!=NX_OK || nx_end_frame(ctx)!=NX_OK)goto done;
        if(close_requested){okay=1;break;}
        const nx_command *commands=NULL;size_t count=0;nx_software_view_outcome outcome;
        if(nx_frame_commands(ctx,&commands,&count)!=NX_OK ||
           nx_software_view_render(view,commands,count,(nx_color){1,1,1,1},&outcome)!=NX_OK)goto done;
        dirty=outcome!=NX_SOFTWARE_VIEW_READY;
        if(outcome==NX_SOFTWARE_VIEW_READY && !ready){puts("READY");ready=1;}
        nx_utf8_span draft;double value;
        if(nx_numeric_field_text(number,&draft)!=NX_OK || nx_numeric_field_value(number,&value)!=NX_OK || draft.length>80)goto done;
        if(ime) {
            char preedit[16];size_t preedit_length=0;int32_t begin=0,end=0;
            if(nx_window_get_ime_preedit(window,preedit,sizeof(preedit),&preedit_length,&begin,&end)!=NX_OK)goto done;
            if(!ime_stage && state.focused && preedit_length==1 && preedit[0]=='\'' &&
               draft.length==5 && !memcmp(draft.data,"12345",5) && value==1234) {
                puts("PREEDIT");ime_stage=1;
            } else if(ime_stage==1 && !preedit_length && draft.length==7 &&
                      !memcmp(draft.data,"12345\xc3\xa1",7) && value==1234 && !valid) {
                puts("INVALID");ime_stage=2;
            } else if(ime_stage==2 && draft.length==8 && !memcmp(draft.data,"12345\xc3\xa1z",8) &&
                      value==1234 && !valid) {puts("SHORTCUT_AS_TEXT");okay=1;break;}
            else if(ime_stage==2 && draft.length==5 && !memcmp(draft.data,"12345",5) &&
                    value==1234 && valid) {puts("UNDONE");okay=1;break;}
        }
        char current[128];int n=snprintf(current,sizeof(current),"STATE %.*s %.0f %d %d",(int)draft.length,draft.data,value,valid,state.focused);
        if(n<0 || (size_t)n>=sizeof(current))goto done;
        if(strcmp(current,previous)){puts(current);memcpy(previous,current,(size_t)n+1);}
    }
done:
    nx_software_view_close(&view);nx_numeric_field_destroy(number);nx_context_destroy(ctx);
    nx_font_collection_destroy(fonts);nx_font_close(font);nx_window_destroy(window);
    return okay?0:1;
}
