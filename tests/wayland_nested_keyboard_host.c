#include "nexaui/nexaui.h"
#include "nexaui/software.h"
#include <stdio.h>
#include <string.h>

static nx_utf8_span text(const char *s) { return (nx_utf8_span){s,strlen(s)}; }
static nx_control_desc button(uint64_t id,uint64_t parent,const char *name,nx_rect bounds) {
    nx_control_desc d={0};d.id=id;d.parent_id=parent;d.name=text(name);d.bounds=bounds;return d;
}

int main(void) {
    nx_window *window=NULL;nx_context *ctx=NULL;nx_software_view *renderer=NULL;
    nx_window_desc wd={"Nested keyboard fixture",360,240,0,{0}};
    int okay=0,stage=0,dirty=1,ready=0;unsigned clicks=0;
    nx_window_info previous={0};char last[80]="";
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
            if(nx_button(ctx,&control,&state)!=NX_OK)goto done;
            if(stage==1 && state.clicked)next=2;
            if(stage==2) {
                nx_overlay_desc menu={0};menu.panel.id=20;menu.panel.parent_id=12;
                menu.panel.name=text("Menu");menu.panel.bounds=(nx_rect){155,110,145,85};
                menu.backdrop_id=21;menu.dismiss_on_escape=1;int dismissed=0;
                if(nx_popup_begin(ctx,&menu,&dismissed)!=NX_OK)goto done;
                if(dismissed)next=1;
                control=button(22,20,"First action",(nx_rect){165,120,120,25});
                if(nx_button(ctx,&control,&state)!=NX_OK)goto done;
                control=button(23,20,"Second action",(nx_rect){165,155,120,25});
                if(nx_button(ctx,&control,&state)!=NX_OK)goto done;
                if(state.clicked)++clicks;
                if(nx_overlay_end(ctx)!=NX_OK)goto done;
            }
            if(nx_overlay_end(ctx)!=NX_OK)goto done;
        }
        if(nx_end_frame(ctx)!=NX_OK)goto done;
        const nx_command *commands=NULL;size_t count=0;nx_software_view_outcome outcome;
        if(nx_frame_commands(ctx,&commands,&count)!=NX_OK ||
           nx_software_view_render(renderer,commands,count,(nx_color){1,1,1,1},&outcome)!=NX_OK)goto done;
        if(outcome==NX_SOFTWARE_VIEW_READY && !ready){puts("READY");ready=1;}
        char current[80];snprintf(current,sizeof(current),"STATE %d %llu %u",stage,
                                  (unsigned long long)nx_focus_id(ctx),clicks);
        if(strcmp(current,last)){puts(current);strcpy(last,current);}
        dirty=next!=stage || outcome!=NX_SOFTWARE_VIEW_READY;stage=next;
        if(stage==0 && clicks==1 && nx_focus_id(ctx)==2){puts("DONE");okay=1;break;}
    }
done:
    nx_software_view_close(&renderer);nx_context_destroy(ctx);nx_window_destroy(window);return okay?0:1;
}
