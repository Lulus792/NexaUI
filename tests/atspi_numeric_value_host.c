#define _POSIX_C_SOURCE 200809L
#include <nexaui/nexaui.h>
#include <nexaui/software.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct model { double value,maximum; int disabled,drag; uint64_t parent; unsigned frames; } model;
static int frame(nx_context *ui,model *m) {
    if(++m->frames>256 || nx_begin_frame(ui,240,160,0)!=NX_OK)return 0;
    nx_semantic_desc parent={0};parent.id=m->parent;parent.role=NX_ROLE_GROUP;
    parent.name=(nx_utf8_span){"Controls",8};parent.bounds=(nx_rect){0,0,240,160};
    nx_value_control_desc d={0};d.control.id=1;d.control.parent_id=m->parent;
    d.control.name=(nx_utf8_span){"Numeric",7};d.control.bounds=(nx_rect){10,10,100,20};
    d.control.disabled=m->disabled;d.maximum=m->maximum;d.step=1;d.units_per_pixel=1;
    nx_interaction_state state;
    return nx_semantics_add(ui,&parent,NULL)==NX_OK &&
        (m->drag?nx_drag_value(ui,&d,&m->value,&state):nx_slider(ui,&d,&m->value,&state))==NX_OK &&
        nx_end_frame(ui)==NX_OK;
}
static int publish(nx_window *window,nx_context *ui,nx_software_view *view) {
    const nx_command *commands=NULL;size_t count=0;nx_software_view_outcome outcome;
    return nx_window_publish_semantics(window,ui)==NX_OK && nx_frame_commands(ui,&commands,&count)==NX_OK &&
        nx_software_view_render(view,commands,count,(nx_color){0,0,0,1},&outcome)==NX_OK;
}
int main(void) {
    const char *private=getenv("NEXAUI_TEST_PRIVATE_WAYLAND"),*display=getenv("WAYLAND_DISPLAY");
    const char *input=getenv("NEXAUI_NUMERIC_TEST_FD");char *end=NULL;
    if(!private || strcmp(private,"1") || !display || strcmp(display,"nexaui-test") || !input)return 77;
    errno=0;long descriptor=strtol(input,&end,10);if(errno || !end || *end || descriptor<0 || descriptor>INT_MAX)return 1;
    int fd=(int)descriptor,okay=0,done=0,hold=0,attached=1;nx_window *window=NULL,*aux=NULL;
    nx_software_view *view=NULL,*aux_view=NULL;nx_context *ui=NULL,*aux_ui=NULL;
    model m={5,10,0,0,9,0};
    nx_window_desc wd={"Numeric native lease",240,160,0,{0}};
    if(nx_context_create(NULL,&ui)!=NX_OK || nx_context_create(NULL,&aux_ui)!=NX_OK ||
       nx_window_create(&wd,&window)!=NX_OK || nx_software_view_open(window,&view)!=NX_OK)goto cleanup;
    wd.title_utf8="Numeric auxiliary lifetime";
    if(nx_window_create(&wd,&aux)!=NX_OK || nx_software_view_open(aux,&aux_view)!=NX_OK ||
       nx_window_poll(window,1000)!=NX_OK || nx_window_poll(aux,1000)!=NX_OK)goto cleanup;
    nx_semantic_desc keep={0};keep.id=1000;keep.role=NX_ROLE_GROUP;
    keep.name=(nx_utf8_span){"Auxiliary",9};keep.bounds=(nx_rect){0,0,240,160};
    if(!frame(ui,&m) || !publish(window,ui,view) || nx_begin_frame(aux_ui,240,160,0)!=NX_OK ||
       nx_semantics_add(aux_ui,&keep,NULL)!=NX_OK || nx_end_frame(aux_ui)!=NX_OK || !publish(aux,aux_ui,aux_view))goto cleanup;
    okay=1;
    for(unsigned iteration=0;okay && !done && iteration<1000;++iteration) {
        uint32_t ready=0;int built=0;nx_window_info info;
        if(nx_window_poll(attached?aux:window,0)!=NX_OK ||
           nx_window_poll_with_fd(attached?window:aux,1000,fd,&ready)!=NX_OK ||
           nx_window_get_info(window,&info)!=NX_OK || info.should_close){okay=0;break;}
        if(ready) {
            char command[32],reply[96]="OK";ssize_t n=recv(fd,command,sizeof(command)-1,MSG_DONTWAIT|MSG_TRUNC);
            if(n<=0 || (size_t)n>=sizeof(command)){okay=0;break;}command[n]=0;
            if(!strcmp(command,"HOLD"))hold=1;
            else if(!strcmp(command,"DISABLE")){m.disabled=1;hold=0;}
            else if(!strcmp(command,"RESET")){m.value=5;m.maximum=10;m.disabled=0;m.drag=0;m.parent=9;hold=0;}
            else if(!strcmp(command,"RANGE")){m.maximum=8;hold=0;}
            else if(!strcmp(command,"OBSERVE")){m.value=7;hold=0;}
            else if(!strcmp(command,"PARENT")){m.parent=8;hold=0;}
            else if(!strcmp(command,"KIND")){m.drag=1;hold=0;}
            else if(!strcmp(command,"UNPUBLISHED")){m.value=7;hold=1;okay=frame(ui,&m);}
            else if(!strcmp(command,"PUBLISH")){okay=publish(window,ui,view);attached=1;hold=1;}
            else if(!strcmp(command,"FORWARD")) {
                size_t routed=0;okay=nx_window_route_events(window,ui,&routed)==NX_OK && routed==1;
                (void)snprintf(reply,sizeof(reply),"ROUTED %zu",routed);
            } else if(!strcmp(command,"RETIRE")){okay=nx_window_publish_semantics(window,NULL)==NX_OK;attached=0;hold=1;}
            else if(!strcmp(command,"RESUME"))hold=0;
            else if(!strcmp(command,"STATE"))(void)snprintf(reply,sizeof(reply),"STATE %.17g %.17g %d %d",m.value,m.maximum,m.disabled,m.drag);
            else if(!strcmp(command,"CLOSE"))done=1;
            else okay=0;
            if(okay && !done && !hold) {
                size_t routed=0;okay=nx_window_route_events(window,ui,&routed)==NX_OK && frame(ui,&m) && publish(window,ui,view);
                attached=1;built=1;
            }
            if(!okay || send(fd,reply,strlen(reply),MSG_NOSIGNAL)!=(ssize_t)strlen(reply)){okay=0;break;}
        }
        if(!done && !hold && !built) {
            size_t routed=0;
            if(nx_window_route_events(window,ui,&routed)!=NX_OK ||
               (routed && (!frame(ui,&m) || !publish(window,ui,view))))okay=0;
        }
    }
cleanup:
    if(window && nx_window_publish_semantics(window,NULL)!=NX_OK)okay=0;
    if(aux && nx_window_publish_semantics(aux,NULL)!=NX_OK)okay=0;
    if(view && nx_software_view_close(&view)!=NX_OK)okay=0;
    if(aux_view && nx_software_view_close(&aux_view)!=NX_OK)okay=0;
    if(ui)nx_context_destroy(ui);if(aux_ui)nx_context_destroy(aux_ui);
    if(window)nx_window_destroy(window);if(aux)nx_window_destroy(aux);close(fd);
    if(okay && done){printf("NUMERIC_LEASE_CLOSED %.17g %u\n",m.value,m.frames);return 0;}
    fprintf(stderr,"NUMERIC_LEASE_HOST_FAILED %.17g %u\n",m.value,m.frames);return 1;
}
