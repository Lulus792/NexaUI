#include "nexaui/nexaui.h"
#include "nexaui/software.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
typedef struct window_slot { nx_window *window; nx_context *context; nx_software_view *view; int id, presented; } window_slot;
static int draw(window_slot *s) {
    nx_window_info info; size_t routed=0,count=0; const nx_command *commands=NULL;
    if(nx_window_get_info(s->window,&info)!=NX_OK || nx_window_route_events(s->window,s->context,&routed)!=NX_OK ||
       nx_begin_frame(s->context,info.width,info.height,0)!=NX_OK) return 0;
    const nx_event *events=NULL; size_t event_count=0;
    if(nx_frame_events(s->context,&events,&event_count)!=NX_OK) return 0;
    for(size_t i=0;i<event_count;++i) {
        if(events[i].type==NX_EVENT_FOCUS) printf("FOCUS %c %d\n",s->id==1?'A':'B',events[i].focused);
        if(events[i].type==NX_EVENT_TEXT) printf("TEXT %c %u\n",s->id==1?'A':'B',events[i].scalar);
    }
    fflush(stdout);
    if(nx_fill_rect(s->context,(nx_rect){0,0,info.width,info.height},
                    s->id==1?(nx_color){0.3f,0.1f,0.1f,1}:(nx_color){0.1f,0.2f,0.4f,1})!=NX_OK ||
       nx_end_frame(s->context)!=NX_OK) return 0;
    if(getenv("NEXAUI_W02_POLL_MANY") && s->presented && !routed)return 1;
    if(nx_frame_commands(s->context,&commands,&count)!=NX_OK) return 0;
    nx_software_view_outcome outcome;
    if(nx_software_view_render(s->view,commands,count,(nx_color){0,0,0,1},&outcome)!=NX_OK) return 0;
    if(outcome==NX_SOFTWARE_VIEW_READY) s->presented=1;
    return 1;
}
int main(void) {
    window_slot slots[2]={{0}}; nx_window_desc d={0}; d.width=360; d.height=240;
    for(unsigned i=0;i<2;++i) {
        d.title_utf8=i?"NexaUI W02 B":"NexaUI W02 A";slots[i].id=(int)i+1;
        CHECK(nx_window_create(&d,&slots[i].window)==NX_OK);
        CHECK(nx_context_create(NULL,&slots[i].context)==NX_OK);
        CHECK(nx_software_view_open(slots[i].window,&slots[i].view)==NX_OK);
    }
    int first_closed=0, ready=0, survivor_presented=0;
    const char *control=getenv("NEXAUI_W02_CONTROL_FD");
    int group=getenv("NEXAUI_W02_POLL_MANY")!=NULL, fd=control?atoi(control):-1, done=0;
    if(group) {
        uint32_t events=99;nx_window *duplicates[2]={slots[0].window,slots[0].window};
        CHECK(nx_window_poll_many(NULL,1,0,-1,&events)==NX_ERROR_ARGUMENT && !events);
        CHECK(nx_window_poll_many(duplicates,2,0,-1,&events)==NX_ERROR_ARGUMENT);
        CHECK(nx_window_poll_many(duplicates,0,0,-1,&events)==NX_ERROR_ARGUMENT);
        CHECK(nx_window_poll_many(duplicates,17,0,-1,&events)==NX_ERROR_ARGUMENT);
        CHECK(nx_window_poll_many(duplicates,1,0,-2,&events)==NX_ERROR_ARGUMENT);
    }
    for(unsigned n=0;n<8000;++n) {
        if(group) {
            nx_window *members[2];size_t count=0;uint32_t events=0;
            for(unsigned i=0;i<2;++i)if(slots[i].window)members[count++]=slots[i].window;
            CHECK(nx_window_poll_many(members,count,ready?5000:0,fd,&events)==NX_OK);
            if(events&NX_WINDOW_FD_READABLE) {
                char command=0;CHECK(read(fd,&command,1)==1);
                if(command=='D')done=1;
                else {
                    CHECK(!first_closed && (command=='A' || command=='B'));
                    unsigned i=command=='A'?0:1;
                    CHECK(nx_software_view_close(&slots[i].view)==NX_OK);
                    nx_context_destroy(slots[i].context);nx_window_destroy(slots[i].window);
                    slots[i]=(window_slot){0};slots[1-i].presented=0;first_closed=1;
                    printf("CLOSED %c\n",command);fflush(stdout);
                }
            }
        }
        for(unsigned i=0;i<2;++i) if(slots[i].window) {
            if(!group)CHECK(nx_window_poll(slots[i].window,5)==NX_OK);
            nx_window_info info;
            CHECK(nx_window_get_info(slots[i].window,&info)==NX_OK);
            if(info.should_close) {
                CHECK(getenv("NEXAUI_W02_NATIVE_CLOSE") && !first_closed && ready && slots[1-i].window);
                nx_window_info other;
                CHECK(nx_window_get_info(slots[1-i].window,&other)==NX_OK && !other.should_close);
                printf("REQUESTED %c\n",i?'B':'A');fflush(stdout);
                CHECK(nx_software_view_close(&slots[i].view)==NX_OK);
                nx_context_destroy(slots[i].context);nx_window_destroy(slots[i].window);
                slots[i]=(window_slot){0};slots[1-i].presented=0;first_closed=1;
                printf("CLOSED %c\n",i?'B':'A');fflush(stdout);
                continue;
            }
            CHECK(draw(&slots[i]));
        }
        if(!ready && slots[0].presented && slots[1].presented) {
            ready=1;puts("WINDOWS_OPEN");fflush(stdout);
        }
        if(first_closed && !survivor_presented) {
            unsigned survivor=slots[0].window?0:1;
            if(slots[survivor].presented) {
                survivor_presented=1;puts("SURVIVOR_PRESENTED");fflush(stdout);
            }
        }
        /* The original fixture can also request an application-owned close. */
        if(!first_closed && getenv("NEXAUI_W02_CLOSE_FILE")) {
            FILE *signal=fopen(getenv("NEXAUI_W02_CLOSE_FILE"),"rb");
            if(signal) {
                int which=fgetc(signal);fclose(signal);unsigned i=which=='A'?0:1;
                CHECK(which=='A' || which=='B');
                CHECK(nx_software_view_close(&slots[i].view)==NX_OK);
                nx_context_destroy(slots[i].context);nx_window_destroy(slots[i].window);
                slots[i]=(window_slot){0};slots[1-i].presented=0;first_closed=1;
                printf("CLOSED %c\n",which);fflush(stdout);
            }
        }
        if(done)break;
        if(first_closed && getenv("NEXAUI_W02_DONE_FILE")) {
            FILE *signal=fopen(getenv("NEXAUI_W02_DONE_FILE"),"rb");
            if(signal){fclose(signal);break;}
        }
    }
    CHECK(first_closed && ready && survivor_presented);
    for(unsigned i=0;i<2;++i) if(slots[i].window) {
        CHECK(nx_software_view_close(&slots[i].view)==NX_OK);
        nx_context_destroy(slots[i].context);nx_window_destroy(slots[i].window);
    }
    puts("WINDOWS_CLOSED");return 0;
}
