#include "paste_flow.h"
#include <string.h>

static nx_result wayland_begin(void *w,size_t limit,uint32_t timeout,int sensitive,uint64_t *token) {
    return nx_wayland_clipboard_read_begin(w,limit,timeout,sensitive,token);
}
static nx_result wayland_take(void *w,uint64_t token,char **bytes,size_t *length,int *ready) {
    return nx_wayland_clipboard_read_take(w,token,bytes,length,ready);
}
static nx_result wayland_cancel(void *w,uint64_t token){return nx_wayland_clipboard_read_cancel(w,token);}
static void wayland_release(void *w,char *bytes,size_t length){(void)w;nx_wayland_clipboard_read_release(bytes,length);}
terminal_paste_transport terminal_paste_wayland_transport(nx_window *window) {
    return (terminal_paste_transport){window,wayland_begin,wayland_take,wayland_cancel,wayland_release};
}
static int valid(terminal_paste_guard g){return g.identity && g.revision && g.focus_epoch && g.focused==1;}
static int same(terminal_paste_guard a,terminal_paste_guard b){
    return valid(b) && a.identity==b.identity && a.revision==b.revision && a.focus_epoch==b.focus_epoch;
}
nx_result terminal_paste_init(terminal_paste_flow *f,terminal_paste_transport t) {
    if(!f || !t.user || !t.begin || !t.take || !t.cancel || !t.release)return NX_ERROR_ARGUMENT;
    *f=(terminal_paste_flow){0};f->transport=t;return NX_OK;
}
void terminal_paste_reset(terminal_paste_flow *f) {
    if(!f)return;
    if(f->state==1 && f->token && f->transport.cancel)(void)f->transport.cancel(f->transport.user,f->token);
    ae_terminal_paste_clear(&f->paste);f->guard=(terminal_paste_guard){0};f->token=0;f->state=0;
}
nx_result terminal_paste_begin(terminal_paste_flow *f,terminal_paste_guard g,uint32_t timeout) {
    if(!f || !valid(g) || !timeout || !f->transport.begin)return NX_ERROR_ARGUMENT;
    if(f->state)return NX_ERROR_STATE;
    uint64_t token=0;nx_result r=f->transport.begin(f->transport.user,AE_TERM_PASTE_BYTES,timeout,1,&token);
    if(r!=NX_OK)return r;
    if(!token)return NX_ERROR_STATE;
    f->guard=g;f->token=token;f->state=1;return NX_OK;
}
nx_result terminal_paste_poll(terminal_paste_flow *f,terminal_paste_guard g,int *ready,
    enum ae_term_paste_result *validation) {
    if(!ready || !validation)return NX_ERROR_ARGUMENT;
    *ready=0;*validation=AE_TERM_PASTE_EMPTY;
    if(!f || f->state!=1)return NX_ERROR_STATE;
    if(!same(f->guard,g)){terminal_paste_reset(f);return NX_ERROR_STATE;}
    char *bytes=NULL;size_t length=0;int done=0;
    nx_result r=f->transport.take(f->transport.user,f->token,&bytes,&length,&done);
    if(!done && r==NX_OK)return NX_OK;
    if(!done)(void)f->transport.cancel(f->transport.user,f->token);
    f->token=0;f->state=0;*ready=1;
    if(r==NX_OK) {
        *validation=ae_terminal_paste_set(&f->paste,bytes,length);
        if(*validation==AE_TERM_PASTE_OK)f->state=2;
        else r=NX_ERROR_ARGUMENT;
    }
    f->transport.release(f->transport.user,bytes,length);
    if(f->state!=2)terminal_paste_reset(f);
    return r;
}
nx_result terminal_paste_confirm(terminal_paste_flow *f,terminal_paste_guard g,int fresh,int repeat,nx_utf8_span *bytes) {
    if(!bytes)return NX_ERROR_ARGUMENT;
    *bytes=(nx_utf8_span){0};
    if(!f || f->state!=2)return NX_ERROR_STATE;
    if(!same(f->guard,g)){terminal_paste_reset(f);return NX_ERROR_STATE;}
    if((fresh!=0 && fresh!=1) || repeat!=0 || (f->paste.review && !fresh))return NX_ERROR_ARGUMENT;
    *bytes=(nx_utf8_span){f->paste.text,f->paste.size};f->state=3;return NX_OK;
}
