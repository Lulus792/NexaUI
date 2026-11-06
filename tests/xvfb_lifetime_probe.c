/* The private server must survive an interval without any X11 client. */
#include <X11/Xlib.h>
#include <stdio.h>

int main(void) {
    for(unsigned cycle=0;cycle<64;++cycle) {
        Display *display=XOpenDisplay(NULL);
        if(!display) { fprintf(stderr,"Cannot reopen private X11 server at cycle %u\n",cycle);return 1; }
        Atom key=XInternAtom(display,"NEXAUI_FIXTURE_LIFETIME_PROBE",False),actual=None;
        int format=0;unsigned long count=0,remaining=0;unsigned char *value=NULL;
        int result=XGetWindowProperty(display,DefaultRootWindow(display),key,0,1,
            False,AnyPropertyType,&actual,&format,&count,&remaining,&value);
        int preserved=result==Success && actual==key && format==8 && count==1 &&
            !remaining && value && value[0]==37;
        if(value)XFree(value);
        if(cycle && !preserved) {
            fprintf(stderr,"Private X11 server reset between client lifetimes at cycle %u\n",cycle);
            XCloseDisplay(display);return 1;
        }
        unsigned char marker=37;
        XChangeProperty(display,DefaultRootWindow(display),key,key,8,PropModeReplace,&marker,1);
        XCloseDisplay(display);
    }
    puts("Private X11 server retained state across 64 independent client lifetimes");
    return 0;
}
