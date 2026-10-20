#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include "window_internal.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e); return 1; } } while (0)
static int message(Display *d, Atom type, Window target, long a, long b, long c) {
    XSync(d,False);
    while (XPending(d)) {
        XEvent e; XNextEvent(d,&e);
        if (e.type==ClientMessage && e.xclient.message_type==type && e.xclient.window==target) {
            CHECK(e.xclient.format==32 && e.xclient.data.l[0]==a && e.xclient.data.l[1]==b && e.xclient.data.l[2]==c);
            return 0;
        }
    }
    CHECK(0);
}
static int pump(nx_window *w, int events) {
    XSync(nx_window_x11_display(w),False); CHECK(nx_window_poll(w,0)==NX_OK);
    nx_event e; int available; unsigned states=0;
    do { CHECK(nx_window_next_event(w,&e,&available)==NX_OK); if (available && e.type==NX_EVENT_WINDOW_STATE) ++states; } while (available);
    CHECK(events<0 || (events ? states>0 : states==0)); return 0;
}
static void *wrong(void *arg) {
    nx_window_status s; memset(&s,0xff,sizeof(s));
    int ok=nx_window_get_status(arg,&s)==NX_ERROR_STATE && !s.state && !s.known && !s.capabilities && !s.client_chrome &&
        nx_window_minimize(arg)==NX_ERROR_STATE && nx_window_maximize(arg)==NX_ERROR_STATE && nx_window_restore(arg)==NX_ERROR_STATE &&
        nx_window_begin_move(arg)==NX_ERROR_STATE && nx_window_begin_resize(arg,NX_WINDOW_RESIZE_LEFT)==NX_ERROR_STATE;
    return (void *)(uintptr_t)!ok;
}
int main(int argc, char **argv) {
    /* Mutates root hints only inside run_xvfb_native.py's fresh isolated server. */
    CHECK(argc==2 && !strcmp(argv[1],"--private-xvfb"));
    CHECK(getenv("NEXAUI_TEST_PRIVATE_X11") && !strcmp(getenv("NEXAUI_TEST_PRIVATE_X11"),"1"));
    CHECK(setenv("NEXAUI_PLATFORM","x11",1)==0);
    Display *wm=XOpenDisplay(NULL); CHECK(wm);
    Window root=DefaultRootWindow(wm); char selection[32]; snprintf(selection,sizeof(selection),"WM_S%d",DefaultScreen(wm));
    Atom wm_selection=XInternAtom(wm,selection,False); CHECK(XGetSelectionOwner(wm,wm_selection)==None);
    nx_window_desc d={"X11 management",320,200,NX_WINDOW_CLIENT_CHROME|NX_WINDOW_STATE_EVENTS,{0}};
    nx_window *w=NULL; CHECK(nx_window_create(&d,&w)==NX_OK);
    nx_window_status s; CHECK(nx_window_get_status(w,&s)==NX_OK && s.client_chrome && !s.known && s.capabilities==NX_WINDOW_CAP_CLIENT_CHROME);
    CHECK(nx_window_maximize(w)==NX_ERROR_UNSUPPORTED && nx_window_minimize(w)==NX_ERROR_UNSUPPORTED && nx_window_restore(w)==NX_ERROR_UNSUPPORTED);
    CHECK(nx_window_begin_move(w)==NX_ERROR_STATE && nx_window_begin_resize(w,(nx_window_resize_edge)0)==NX_ERROR_ARGUMENT);
    Window xid=nx_window_x11_id(w); Display *client=nx_window_x11_display(w);
    Atom motif=XInternAtom(wm,"_MOTIF_WM_HINTS",False),actual; int format; unsigned long count,remaining; unsigned char *data=NULL;
    CHECK(XGetWindowProperty(wm,xid,motif,0,5,False,motif,&actual,&format,&count,&remaining,&data)==Success);
    CHECK(actual==motif && format==32 && count==5 && ((unsigned long *)data)[0]==2 && ((unsigned long *)data)[2]==0); XFree(data);
    pthread_t thread; void *thread_result; CHECK(!pthread_create(&thread,NULL,wrong,w) && !pthread_join(thread,&thread_result) && !thread_result);
    Window manager=XCreateSimpleWindow(wm,root,0,0,1,1,0,0,0); XSetSelectionOwner(wm,wm_selection,manager,CurrentTime);
    XSelectInput(wm,root,SubstructureNotifyMask);
    Atom maxh=XInternAtom(wm,"_NET_WM_STATE_MAXIMIZED_HORZ",False),maxv=XInternAtom(wm,"_NET_WM_STATE_MAXIMIZED_VERT",False);
    Atom moveresize=XInternAtom(wm,"_NET_WM_MOVERESIZE",False),full=XInternAtom(wm,"_NET_WM_STATE_FULLSCREEN",False);
    Atom netstate=XInternAtom(wm,"_NET_WM_STATE",False),active=XInternAtom(wm,"_NET_ACTIVE_WINDOW",False),supported=XInternAtom(wm,"_NET_SUPPORTED",False);
    Atom atoms[]={maxh,maxv,moveresize,full}; XChangeProperty(wm,root,supported,XA_ATOM,32,PropModeReplace,(unsigned char *)atoms,4);
    XSync(wm,False);
    CHECK(nx_window_get_status(w,&s)==NX_OK && s.capabilities==127u && !(s.known&NX_WINDOW_STATE_MAXIMIZED));
    Atom wmstate=XInternAtom(wm,"WM_STATE",False); unsigned long normal[]={NormalState,None};
    XChangeProperty(wm,xid,wmstate,wmstate,32,PropModeReplace,(unsigned char *)normal,2);
    XChangeProperty(wm,xid,netstate,XA_ATOM,32,PropModeReplace,NULL,0);
    XChangeProperty(wm,root,active,XA_WINDOW,32,PropModeReplace,(unsigned char *)&xid,1); XSync(wm,False);
    CHECK(nx_window_get_status(w,&s)==NX_OK && s.known==15u && s.state==NX_WINDOW_STATE_ACTIVE && s.capabilities==127u);
    CHECK(pump(w,1)==0);
    CHECK(nx_window_maximize(w)==NX_OK); XSync(client,False); CHECK(message(wm,netstate,xid,1,(long)maxh,(long)maxv)==0);
    CHECK(nx_window_get_status(w,&s)==NX_OK && !(s.state&NX_WINDOW_STATE_MAXIMIZED)); /* request isn't confirmation */
    XChangeProperty(wm,xid,netstate,XA_ATOM,32,PropModeReplace,(unsigned char *)atoms,2); XSync(wm,False);
    CHECK(pump(w,1)==0 && nx_window_get_status(w,&s)==NX_OK && (s.state&NX_WINDOW_STATE_MAXIMIZED));
    CHECK(nx_window_restore(w)==NX_OK); XSync(client,False); CHECK(message(wm,netstate,xid,0,(long)maxh,(long)maxv)==0);
    XChangeProperty(wm,xid,netstate,XA_ATOM,32,PropModeReplace,NULL,0); XSync(wm,False); CHECK(pump(w,1)==0);
    CHECK(nx_window_minimize(w)==NX_OK); XSync(client,False); CHECK(message(wm,XInternAtom(wm,"WM_CHANGE_STATE",False),xid,IconicState,0,0)==0);
    for (int i=0;i<9;++i) {
        XEvent e={0}; e.xbutton.type=ButtonPress; e.xbutton.display=client; e.xbutton.window=xid;
        e.xbutton.button=Button1; e.xbutton.x_root=71; e.xbutton.y_root=83;
        CHECK(XSendEvent(wm,xid,False,ButtonPressMask,&e)); XSync(wm,False); CHECK(pump(w,0)==0);
        CHECK(i==8 ? nx_window_begin_move(w)==NX_OK : nx_window_begin_resize(w,(nx_window_resize_edge)(i+1))==NX_OK);
        XSync(client,False); CHECK(message(wm,moveresize,xid,71,83,i)==0);
        CHECK(nx_window_begin_move(w)==NX_ERROR_STATE); CHECK(pump(w,0)==0);
    }
    XEvent e={0}; e.xbutton.type=ButtonPress; e.xbutton.window=xid; e.xbutton.button=Button1;
    CHECK(XSendEvent(wm,xid,False,ButtonPressMask,&e)); e.xbutton.type=ButtonRelease;
    CHECK(XSendEvent(wm,xid,False,ButtonReleaseMask,&e)); XSync(wm,False); CHECK(pump(w,0)==0 && nx_window_begin_move(w)==NX_ERROR_STATE);
    XSetSelectionOwner(wm,wm_selection,None,CurrentTime); XSync(wm,False);
    CHECK(nx_window_get_status(w,&s)==NX_OK && s.capabilities==NX_WINDOW_CAP_CLIENT_CHROME);
    e=(XEvent){0}; e.xclient.type=ClientMessage; e.xclient.window=xid; e.xclient.format=32;
    e.xclient.message_type=XInternAtom(wm,"WM_PROTOCOLS",False); e.xclient.data.l[0]=(long)XInternAtom(wm,"WM_DELETE_WINDOW",False);
    CHECK(XSendEvent(wm,xid,False,NoEventMask,&e)); XSync(wm,False); CHECK(pump(w,0)==0);
    memset(&s,0xff,sizeof(s)); CHECK(nx_window_get_status(w,&s)==NX_ERROR_STATE && !s.state && !s.known && !s.capabilities && !s.client_chrome);
    nx_window_destroy(w); XDestroyWindow(wm,manager); XCloseDisplay(wm);
    puts("X11: isolated ICCCM/EWMH wire/state, Motif hint, eight resize edges, pointer provenance and owner guards passed; real WM/physical acceptance remains separate.");
    return 0;
}
