#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include "../platform/linux/x11/window_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static nx_window *owned_window;
static nx_context *owned_context;
static int failure(const char *file,int line,const char *expression) {
    fprintf(stderr,"%s:%d: %s\n",file,line,expression);
    if(owned_window && getenv("NEXAUI_PLATFORM") && !strcmp(getenv("NEXAUI_PLATFORM"),"x11")) {
        Display *display=nx_window_x11_display(owned_window); Window xid=nx_window_x11_id(owned_window);
        XWindowAttributes a; Window root,parent,*children=NULL; unsigned count=0;
        if(XGetWindowAttributes(display,xid,&a) && XQueryTree(display,xid,&root,&parent,&children,&count))
            fprintf(stderr,"Owned X11 diagnostic xid=%lu root=%lu parent=%lu map_state=%d override_redirect=%d\n",xid,root,parent,a.map_state,a.override_redirect);
        if(children) XFree(children);
    }
    nx_context_destroy(owned_context); owned_context=NULL;
    nx_window_destroy(owned_window); owned_window=NULL;
    return 1;
}
#define CHECK(e) do { if (!(e)) return failure(__FILE__,__LINE__,#e); } while (0)
static int wait_state(nx_window *w,uint32_t bit,int enabled) {
    nx_window_status s={0};
    for(unsigned i=0;i<500;++i) {
        CHECK(nx_window_poll(w,10)==NX_OK && nx_window_get_status(w,&s)==NX_OK);
        if((s.known&bit) && !!(s.state&bit)==enabled) return 0;
    }
    fprintf(stderr,"Unconfirmed state bit=%u expected=%d observed=%u known=%u capabilities=%u\n",bit,enabled,s.state,s.known,s.capabilities);
    CHECK(0);
}
int main(void) {
    nx_window_desc d={"NexaUI native client management",320,200,NX_WINDOW_CLIENT_CHROME|NX_WINDOW_STATE_EVENTS,{0}};
    nx_window *w=NULL; nx_result result=nx_window_create(&d,&w);
    if(result==NX_ERROR_PLATFORM) return 77;
    owned_window=w; CHECK(result==NX_OK && w);
    nx_window_status s;
    /* Root capabilities can appear before Openbox adopts this mapped window.
       Wait for an actual per-window state observation, not just WM startup. */
    for(unsigned i=0;i<300;++i) {
        CHECK(nx_window_poll(w,10)==NX_OK && nx_window_get_status(w,&s)==NX_OK);
        if((s.capabilities&NX_WINDOW_CAP_MAXIMIZE) && (s.known&NX_WINDOW_STATE_MAXIMIZED)) break;
    }
    printf("Native WM before request: state=%u known=%u capabilities=%u\n",s.state,s.known,s.capabilities);
    fflush(stdout);
    CHECK(s.client_chrome && (s.capabilities&NX_WINDOW_CAP_MAXIMIZE) && (s.known&NX_WINDOW_STATE_MAXIMIZED));
    CHECK(nx_window_begin_move(w)==NX_ERROR_STATE && nx_window_begin_resize(w,NX_WINDOW_RESIZE_RIGHT)==NX_ERROR_STATE);
    CHECK(nx_window_maximize(w)==NX_OK && wait_state(w,NX_WINDOW_STATE_MAXIMIZED,1)==0);
    nx_window_info maximized; CHECK(nx_window_get_info(w,&maximized)==NX_OK && maximized.width>320 && maximized.height>200);
    CHECK(nx_window_maximize(w)==NX_OK && nx_window_poll(w,20)==NX_OK);
    CHECK(nx_window_get_status(w,&s)==NX_OK && (s.state&NX_WINDOW_STATE_MAXIMIZED));
    CHECK(nx_window_restore(w)==NX_OK && wait_state(w,NX_WINDOW_STATE_MAXIMIZED,0)==0);
    nx_window_info restored; CHECK(nx_window_get_info(w,&restored)==NX_OK && restored.width>0 && restored.height>0);
    nx_context *context=NULL; CHECK(nx_context_create(NULL,&owned_context)==NX_OK); context=owned_context;
    size_t routed; CHECK(nx_window_route_events(w,context,&routed)==NX_OK && routed>0);
    CHECK(nx_begin_frame(context,restored.width,restored.height,0)==NX_OK);
    const nx_event *events; size_t count; unsigned states=0;
    CHECK(nx_frame_events(context,&events,&count)==NX_OK);
    for(size_t i=0;i<count;++i) if(events[i].type==NX_EVENT_WINDOW_STATE) ++states;
    CHECK(states>=2 && nx_end_frame(context)==NX_OK);
    CHECK(nx_window_get_status(w,&s)==NX_OK && (s.capabilities&NX_WINDOW_CAP_MINIMIZE));
    CHECK(nx_window_minimize(w)==NX_OK);
    if(s.known&NX_WINDOW_STATE_MINIMIZED) {
        CHECK(wait_state(w,NX_WINDOW_STATE_MINIMIZED,1)==0 && (s.capabilities&NX_WINDOW_CAP_RESTORE_MINIMIZED));
        CHECK(nx_window_restore(w)==NX_OK && wait_state(w,NX_WINDOW_STATE_MINIMIZED,0)==0);
    } else CHECK(!(s.capabilities&NX_WINDOW_CAP_RESTORE_MINIMIZED));
    nx_context_destroy(context); owned_context=NULL; nx_window_destroy(w); owned_window=NULL;
    printf("Native WM: confirmed maximize %.0fx%.0f, restore %.0fx%.0f, %u state events; minimize observability=%u. Physical/visual acceptance remains separate.\n",
        maximized.width,maximized.height,restored.width,restored.height,states,!!(s.known&NX_WINDOW_STATE_MINIMIZED));
    return 0;
}
