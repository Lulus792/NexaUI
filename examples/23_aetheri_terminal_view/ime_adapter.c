#include "ime_adapter.h"
#include <string.h>

static int valid(terminal_ime_guard g) {return g.identity && g.epoch && g.focused==1;}
static int same(terminal_ime_guard a,terminal_ime_guard b) {
    return valid(a) && valid(b) && a.identity==b.identity && a.epoch==b.epoch;
}
void terminal_ime_reset(terminal_ime_adapter *ime) {if(ime)memset(ime,0,sizeof(*ime));}
nx_result terminal_ime_bind(terminal_ime_adapter *ime,terminal_ime_guard guard) {
    if(!ime || !valid(guard))return NX_ERROR_ARGUMENT;
    terminal_ime_reset(ime);ime->guard=guard;return NX_OK;
}
static nx_result current_guard(terminal_ime_adapter *ime,terminal_ime_guard guard) {
    if(!ime)return NX_ERROR_ARGUMENT;
    if(!same(ime->guard,guard)){terminal_ime_reset(ime);return NX_ERROR_STATE;}
    return NX_OK;
}
static int scalar_offset(const char *s,size_t n,int32_t byte,int *out) {
    if(byte<0 || (size_t)byte>n)return 0;
    if((size_t)byte<n && ((unsigned char)s[byte]&0xc0u)==0x80u)return 0;
    int count=0;for(int32_t i=0;i<byte;++i)if(((unsigned char)s[i]&0xc0u)!=0x80u)++count;
    *out=count;return 1;
}
nx_result terminal_ime_preedit(terminal_ime_adapter *ime,terminal_ime_guard guard,
    const char *bytes,size_t length,int32_t begin,int32_t end) {
    nx_result r=current_guard(ime,guard);if(r!=NX_OK)return r;
    if((!bytes && length) || length>AE_TERM_COMPOSITION_BYTES ||
       ((begin==-1)!=(end==-1)))return NX_ERROR_ARGUMENT;
    int first=-1,last=-1;
    if(begin!=-1 && (!scalar_offset(bytes,length,begin,&first) || !scalar_offset(bytes,length,end,&last)))return NX_ERROR_ARGUMENT;
    /* The original SDL model reserves length -1 for an unspecified selection. */
    if(first>=0 && last-first==-1)return NX_ERROR_UNSUPPORTED;
    if(!ae_terminal_composition_set(&ime->composition,bytes,length,first,first<0?-1:last-first))return NX_ERROR_ARGUMENT;
    return NX_OK;
}
