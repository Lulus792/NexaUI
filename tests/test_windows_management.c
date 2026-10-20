#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include "nexaui/nexaui.h"
#include <stdio.h>
#include <string.h>

#define CHECK(e) do { if (!(e)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e); return 1; } } while (0)
static WNDPROC previous_proc;
static unsigned moves, sizes, menus;
/* Only intercept the native loop entry: this proves routing without injecting
   global mouse/keyboard input or claiming a physical drag/Snap acceptance. */
static LRESULT CALLBACK observe(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_SYSCOMMAND) {
        switch (w & 0xfff0u) {
        case SC_MOVE: ++moves; return 0;
        case SC_SIZE: ++sizes; return 0;
        case SC_KEYMENU: ++menus; return 0;
        }
    }
    return CallWindowProcW(previous_proc,h,m,w,l);
}
static int drain(nx_window *w, int allow_state, unsigned *states) {
    nx_event e; int available;
    do {
        CHECK(nx_window_next_event(w,&e,&available) == NX_OK);
        if (available && e.type == NX_EVENT_WINDOW_STATE) {
            CHECK(allow_state && !(e.window_state & ~15u));
            if (states) ++*states;
        }
    } while (available);
    return 0;
}
static DWORD WINAPI wrong_thread(void *arg) {
    nx_window *w = arg;
    nx_window_status s; memset(&s,0xff,sizeof(s));
    if (nx_window_get_status(w,&s) != NX_ERROR_STATE || s.state || s.known || s.capabilities || s.client_chrome) return 1;
    if (nx_window_minimize(w) != NX_ERROR_STATE || nx_window_maximize(w) != NX_ERROR_STATE ||
        nx_window_restore(w) != NX_ERROR_STATE || nx_window_begin_move(w) != NX_ERROR_STATE ||
        nx_window_begin_resize(w,NX_WINDOW_RESIZE_LEFT) != NX_ERROR_STATE) return 2;
    return 0;
}
int main(void) {
    DPI_AWARENESS_CONTEXT previous_dpi=SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CHECK(previous_dpi);
    int expected_width=MulDiv(360,(int)GetDpiForSystem(),96), expected_height=MulDiv(240,(int)GetDpiForSystem(),96);
    nx_window_status s; memset(&s,0xff,sizeof(s));
    CHECK(nx_window_get_status(NULL,&s) == NX_ERROR_ARGUMENT && !s.state && !s.known && !s.capabilities && !s.client_chrome);
    CHECK(nx_window_get_status(NULL,NULL) == NX_ERROR_ARGUMENT);
    CHECK(nx_window_minimize(NULL) == NX_ERROR_ARGUMENT && nx_window_maximize(NULL) == NX_ERROR_ARGUMENT && nx_window_restore(NULL) == NX_ERROR_ARGUMENT);
    CHECK(nx_window_begin_move(NULL) == NX_ERROR_ARGUMENT && nx_window_begin_resize(NULL,NX_WINDOW_RESIZE_LEFT) == NX_ERROR_ARGUMENT);
    nx_window_desc d = {0}; d.title_utf8="NexaUI management native test"; d.width=360; d.height=240; d.flags=NX_WINDOW_HIDDEN;
    nx_window *normal=NULL,*client=NULL;
    CHECK(nx_window_create(&d,&normal) == NX_OK);
    HWND n=FindWindowW(L"NexaUIWindowClass",L"NexaUI management native test"); CHECK(n);
    RECT nr,nc; CHECK(GetWindowRect(n,&nr) && GetClientRect(n,&nc));
    CHECK(nc.right==expected_width && nc.bottom==expected_height && nr.bottom-nr.top>nc.bottom);
    CHECK(nx_window_get_status(normal,&s)==NX_OK && !s.client_chrome && !(s.state & NX_WINDOW_STATE_MINIMIZED));
    d.title_utf8="NexaUI management client test"; d.flags=NX_WINDOW_CLIENT_CHROME|NX_WINDOW_STATE_EVENTS;
    CHECK(nx_window_create(&d,&client)==NX_OK);
    HWND h=FindWindowW(L"NexaUIWindowClass",L"NexaUI management client test"); CHECK(h);
    RECT r,c; CHECK(GetWindowRect(h,&r) && GetClientRect(h,&c));
    CHECK(c.right==expected_width && c.bottom==expected_height && r.right-r.left==c.right && r.bottom-r.top==c.bottom);
    CHECK((GetWindowLongPtrW(h,GWL_STYLE)&WS_OVERLAPPEDWINDOW)==WS_OVERLAPPEDWINDOW);
    CHECK(nx_window_get_status(client,&s)==NX_OK && s.client_chrome && s.known==11u && s.capabilities==127u);
    CHECK(nx_window_begin_move(client)==NX_ERROR_STATE);
    CHECK(nx_window_begin_resize(client,(nx_window_resize_edge)0)==NX_ERROR_ARGUMENT);
    CHECK(nx_window_begin_resize(client,(nx_window_resize_edge)9)==NX_ERROR_ARGUMENT);
    CHECK(nx_window_begin_resize(client,NX_WINDOW_RESIZE_RIGHT)==NX_ERROR_STATE);
    HANDLE t=CreateThread(NULL,0,wrong_thread,client,0,NULL); CHECK(t && WaitForSingleObject(t,3000)==WAIT_OBJECT_0);
    DWORD code=99; CHECK(GetExitCodeThread(t,&code) && code==0); CloseHandle(t);
    CHECK(drain(client,1,NULL)==0 && drain(normal,0,NULL)==0);
    SendMessageW(n,WM_SYSCHAR,L'a',0);
    nx_event legacy; int legacy_available;
    CHECK(nx_window_next_event(normal,&legacy,&legacy_available)==NX_OK && legacy_available && legacy.type==NX_EVENT_TEXT && legacy.scalar=='a');
    POINT points[8]={{r.left,r.top},{(r.left+r.right)/2,r.top},{r.right-1,r.top},
        {r.right-1,(r.top+r.bottom)/2},{r.right-1,r.bottom-1},{(r.left+r.right)/2,r.bottom-1},
        {r.left,r.bottom-1},{r.left,(r.top+r.bottom)/2}};
    int hits[8]={HTTOPLEFT,HTTOP,HTTOPRIGHT,HTRIGHT,HTBOTTOMRIGHT,HTBOTTOM,HTBOTTOMLEFT,HTLEFT};
    for (int i=0;i<8;++i) CHECK(SendMessageW(h,WM_NCHITTEST,0,MAKELPARAM(points[i].x,points[i].y))==hits[i]);
    previous_proc=(WNDPROC)SetWindowLongPtrW(h,GWLP_WNDPROC,(LONG_PTR)observe); CHECK(previous_proc);
    for (int i=0;i<9;++i) {
        SendMessageW(h,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(30,30));
        CHECK(i==8 ? nx_window_begin_move(client)==NX_OK : nx_window_begin_resize(client,(nx_window_resize_edge)(i+1))==NX_OK);
        CHECK(nx_window_begin_move(client)==NX_ERROR_STATE && GetCapture()!=h);
    }
    CHECK(moves==1 && sizes==8);
    SendMessageW(h,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(30,30));
    SendMessageW(h,WM_LBUTTONUP,0,MAKELPARAM(30,30));
    CHECK(nx_window_begin_move(client)==NX_ERROR_STATE);
    CHECK(drain(client,1,NULL)==0);
    SendMessageW(h,WM_SYSCHAR,L' ',(LPARAM)1<<29);
    CHECK(menus==1);
    nx_event e; int available; do {
        CHECK(nx_window_next_event(client,&e,&available)==NX_OK);
        CHECK(!available || e.type!=NX_EVENT_TEXT);
    } while (available);
    unsigned changes=0;
    CHECK(nx_window_maximize(client)==NX_OK && IsZoomed(h));
    CHECK(nx_window_get_status(client,&s)==NX_OK && (s.state&NX_WINDOW_STATE_MAXIMIZED));
    MONITORINFO mi={0}; mi.cbSize=sizeof(mi); CHECK(GetMonitorInfoW(MonitorFromWindow(h,MONITOR_DEFAULTTONEAREST),&mi));
    CHECK(GetClientRect(h,&c)); POINT origin={0,0}; CHECK(ClientToScreen(h,&origin));
    CHECK(origin.x>=mi.rcWork.left && origin.y>=mi.rcWork.top && origin.x+c.right<=mi.rcWork.right && origin.y+c.bottom<=mi.rcWork.bottom);
    CHECK(drain(client,1,&changes)==0 && changes>0);
    SendMessageW(h,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(30,30));
    CHECK(nx_window_begin_resize(client,NX_WINDOW_RESIZE_RIGHT)==NX_ERROR_STATE);
    SendMessageW(h,WM_LBUTTONUP,0,MAKELPARAM(30,30));
    CHECK(nx_window_restore(client)==NX_OK && !IsZoomed(h) && !IsIconic(h));
    CHECK(GetClientRect(h,&c) && c.right==expected_width && c.bottom==expected_height);
    CHECK(nx_window_minimize(client)==NX_OK && IsIconic(h));
    CHECK(nx_window_get_status(client,&s)==NX_OK && (s.state&NX_WINDOW_STATE_MINIMIZED));
    CHECK(nx_window_restore(client)==NX_OK && !IsIconic(h));
    CHECK(nx_window_maximize(normal)==NX_OK && nx_window_restore(normal)==NX_OK);
    CHECK(drain(normal,0,NULL)==0);
    CHECK(nx_window_get_status(client,&s)==NX_OK && !(s.state&(NX_WINDOW_STATE_MINIMIZED|NX_WINDOW_STATE_MAXIMIZED)));
    nx_context *ctx=NULL; CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    size_t routed=0; CHECK(nx_window_route_events(client,ctx,&routed)==NX_OK && routed>0);
    CHECK(nx_begin_frame(ctx,360,240,0)==NX_OK && nx_end_frame(ctx)==NX_OK); nx_context_destroy(ctx);
    SetWindowLongPtrW(h,GWLP_WNDPROC,(LONG_PTR)previous_proc);
    SendMessageW(h,WM_CLOSE,0,0); memset(&s,0xff,sizeof(s));
    CHECK(nx_window_get_status(client,&s)==NX_ERROR_STATE && !s.state && !s.known && !s.capabilities && !s.client_chrome);
    CHECK(nx_window_maximize(client)==NX_ERROR_STATE && nx_window_minimize(client)==NX_ERROR_STATE && nx_window_restore(client)==NX_ERROR_STATE);
    nx_window_destroy(client); nx_window_destroy(normal);
    d.flags=0x80000000u; client=(nx_window *)(uintptr_t)1;
    CHECK(nx_window_create(&d,&client)==NX_ERROR_ARGUMENT && !client);
    CHECK(SetThreadDpiAwarenessContext(previous_dpi));
    puts("Windows: native geometry/state, eight resize requests, move, stale press, system-menu route, opt-in events and owner guards passed; physical input/Snap not asserted.");
    return 0;
}
