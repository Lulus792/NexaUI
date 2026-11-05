#include "nexaui/nexaui.h"
#include "nexaui/software.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static nx_utf8_span text(const char *s) { return (nx_utf8_span){s,strlen(s)}; }
static nx_control_desc button(uint64_t id,uint64_t parent,const char *name,nx_rect bounds) {
    nx_control_desc d={0};d.id=id;d.parent_id=parent;d.name=text(name);d.bounds=bounds;return d;
}

static unsigned char *load_font(const char *path,size_t *length) {
    FILE *file=fopen(path,"rb");if(!file)return NULL;
    if(fseek(file,0,SEEK_END)){fclose(file);return NULL;}
    long n=ftell(file);if(n<=0 || n>64000000 || fseek(file,0,SEEK_SET)){fclose(file);return NULL;}
    unsigned char *data=malloc((size_t)n);
    if(!data || fread(data,1,(size_t)n,file)!=(size_t)n){free(data);fclose(file);return NULL;}
    fclose(file);*length=(size_t)n;return data;
}
int main(int argc,char **argv) {
    nx_window *window=NULL;nx_context *ctx=NULL;nx_software_view *renderer=NULL;
    nx_font *font=NULL;nx_font_collection *fonts=NULL;nx_text_layout *labels[4]={0};
    unsigned char *font_data=NULL;size_t font_length=0;
    nx_window_desc wd={"Nested keyboard fixture",360,240,0,{0}};
    int okay=0,stage=0,dirty=1,ready=0;unsigned clicks=0;
    nx_window_info previous={0};char last[80]="";
    const char *names[]={"Open dialog","Open menu","First action","Second action"};
    if(argc!=2 || !(font_data=load_font(argv[1],&font_length)) ||
       nx_font_open(font_data,font_length,NULL,&font)!=NX_OK || nx_font_collection_create(NULL,&fonts)!=NX_OK ||
       nx_font_collection_add(fonts,font)!=NX_OK)goto done;
    for(size_t i=0;i<4;++i)
        if(nx_text_layout_create(fonts,names[i],strlen(names[i]),NX_BIDI_LTR,14,115,&labels[i])!=NX_OK)goto done;
    if(nx_window_create(&wd,&window)!=NX_OK || nx_context_create(NULL,&ctx)!=NX_OK ||
       nx_software_view_open(window,&renderer)!=NX_OK)goto done;
    setvbuf(stdout,NULL,_IOLBF,0);
    for(;;) {
        nx_window_info info;size_t routed=0;
        if(nx_window_poll(window,50)!=NX_OK || nx_window_get_info(window,&info)!=NX_OK ||
           nx_window_route_events(window,ctx,&routed)!=NX_OK)goto done;
        if(info.should_close){okay=1;break;}
        if(routed || info.width!=previous.width || info.height!=previous.height || info.scale!=previous.scale)dirty=1;
        previous=info;if(!dirty)continue;
        nx_semantic_desc root={0};root.id=1;root.role=NX_ROLE_WINDOW;
        root.name=text("Nested keyboard fixture");root.bounds=(nx_rect){0,0,info.width,info.height};
        nx_interaction_state state;nx_control_desc control=button(2,1,"Open dialog",(nx_rect){20,20,120,32});
        control.label_layout=labels[0];control.label_origin=(nx_point){27,27};control.glyph_cache_bytes=65536;
        if(nx_begin_frame(ctx,info.width,info.height,0)!=NX_OK || nx_semantics_add(ctx,&root,NULL)!=NX_OK ||
           nx_button(ctx,&control,&state)!=NX_OK)goto done;
        int next=stage;
        if(stage==0 && state.clicked)next=1;
        if(stage) {
            nx_overlay_desc dialog={0};dialog.panel.id=10;dialog.panel.parent_id=1;
            dialog.panel.name=text("Dialog");dialog.panel.bounds=(nx_rect){50,55,260,160};
            dialog.backdrop_id=11;dialog.dismiss_on_escape=1;int closed=0;
            if(nx_modal_begin(ctx,&dialog,&closed)!=NX_OK)goto done;
            if(closed)next=0;
            control=button(12,10,"Open menu",(nx_rect){70,75,130,30});
            control.label_layout=labels[1];control.label_origin=(nx_point){77,81};control.glyph_cache_bytes=65536;
            if(nx_button(ctx,&control,&state)!=NX_OK)goto done;
            if(stage==1 && state.clicked)next=2;
            if(stage==2) {
                nx_overlay_desc menu={0};menu.panel.id=20;menu.panel.parent_id=12;
                menu.panel.name=text("Menu");menu.panel.bounds=(nx_rect){155,110,145,85};
                menu.backdrop_id=21;menu.dismiss_on_escape=1;int dismissed=0;
                if(nx_popup_begin(ctx,&menu,&dismissed)!=NX_OK)goto done;
                if(dismissed)next=1;
                control=button(22,20,"First action",(nx_rect){165,120,120,25});
                control.label_layout=labels[2];control.label_origin=(nx_point){170,124};control.glyph_cache_bytes=65536;
                if(nx_button(ctx,&control,&state)!=NX_OK)goto done;
                control=button(23,20,"Second action",(nx_rect){165,155,120,25});
                control.label_layout=labels[3];control.label_origin=(nx_point){170,159};control.glyph_cache_bytes=65536;
                if(nx_button(ctx,&control,&state)!=NX_OK)goto done;
                if(state.clicked)++clicks;
                if(nx_overlay_end(ctx)!=NX_OK)goto done;
            }
            if(nx_overlay_end(ctx)!=NX_OK)goto done;
        }
        if(nx_end_frame(ctx)!=NX_OK)goto done;
        nx_result ax=nx_window_publish_semantics(window,ctx);
        if(ax!=NX_OK && (ax!=NX_ERROR_UNSUPPORTED || getenv("NEXAUI_NESTED_ATSPI")))goto done;
        const nx_command *commands=NULL;size_t count=0;nx_software_view_outcome outcome;
        if(nx_frame_commands(ctx,&commands,&count)!=NX_OK)goto done;
        size_t glyphs=0;for(size_t i=0;i<count;++i)if(commands[i].type==NX_COMMAND_BITMAP)++glyphs;
        if(!glyphs || nx_software_view_render(renderer,commands,count,(nx_color){1,1,1,1},&outcome)!=NX_OK)goto done;
        if(outcome==NX_SOFTWARE_VIEW_READY && !ready){puts("READY");ready=1;}
        char current[80];snprintf(current,sizeof(current),"STATE %d %llu %u",stage,
                                  (unsigned long long)nx_focus_id(ctx),clicks);
        if(strcmp(current,last)){puts(current);strcpy(last,current);}
        dirty=next!=stage || outcome!=NX_SOFTWARE_VIEW_READY;stage=next;
        if(stage==0 && clicks==1 && nx_focus_id(ctx)==2){puts("DONE");okay=1;break;}
    }
done:
    if(window)nx_window_publish_semantics(window,NULL);
    nx_software_view_close(&renderer);nx_context_destroy(ctx);nx_window_destroy(window);
    for(size_t i=0;i<4;++i)nx_text_layout_destroy(labels[i]);
    nx_font_collection_destroy(fonts);nx_font_close(font);free(font_data);return okay?0:1;
}
