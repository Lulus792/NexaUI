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
void terminal_ime_interrupt(terminal_ime_adapter *ime) {
    if(!ime)return;
    memset(ime->pending,0,sizeof(ime->pending));ime->pending_length=0;ime->receiving=0;
    ae_terminal_paste_clear(&ime->commit);
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
nx_result terminal_ime_commit_event(terminal_ime_adapter *ime,terminal_ime_guard guard,
    const nx_event *event,int *ready,nx_utf8_span *out) {
    if(!ready || !out)return NX_ERROR_ARGUMENT;
    *ready=0;*out=(nx_utf8_span){0};
    nx_result r=current_guard(ime,guard);if(r!=NX_OK)return r;
    ae_terminal_paste_clear(&ime->commit);
    if(!event || event->type!=NX_EVENT_TEXT || event->ime_target!=guard.identity ||
       event->ime_revision!=guard.epoch ||
       (event->ime_before & ~(NX_IME_COMMIT_START|NX_IME_COMMIT_END|NX_IME_COMMIT_FILTERED)) ||
       (event->ime_before & NX_IME_COMMIT_FILTERED) ||
       (!!(event->ime_before & NX_IME_COMMIT_START) == !!ime->receiving)) {
        terminal_ime_interrupt(ime);return NX_ERROR_STATE;
    }
    uint32_t c=event->scalar;char encoded[4];size_t count=0;
    if(!c || c>0x10ffffu || (c>=0xd800u && c<=0xdfffu)) {
        terminal_ime_interrupt(ime);return NX_ERROR_ARGUMENT;
    }
    if(c<0x80u){encoded[0]=(char)c;count=1;}
    else if(c<0x800u){encoded[0]=(char)(0xc0u|(c>>6));encoded[1]=(char)(0x80u|(c&63u));count=2;}
    else if(c<0x10000u){encoded[0]=(char)(0xe0u|(c>>12));encoded[1]=(char)(0x80u|((c>>6)&63u));encoded[2]=(char)(0x80u|(c&63u));count=3;}
    else {encoded[0]=(char)(0xf0u|(c>>18));encoded[1]=(char)(0x80u|((c>>12)&63u));encoded[2]=(char)(0x80u|((c>>6)&63u));encoded[3]=(char)(0x80u|(c&63u));count=4;}
    if(count>sizeof(ime->pending)-ime->pending_length) {
        terminal_ime_interrupt(ime);return NX_ERROR_LIMIT;
    }
    memcpy(ime->pending+ime->pending_length,encoded,count);ime->pending_length+=count;ime->receiving=1;
    if(!(event->ime_before&NX_IME_COMMIT_END))return NX_OK;
    if(ae_terminal_paste_set(&ime->commit,ime->pending,ime->pending_length)!=AE_TERM_PASTE_OK || ime->commit.review) {
        terminal_ime_interrupt(ime);return NX_ERROR_ARGUMENT;
    }
    memset(ime->pending,0,sizeof(ime->pending));ime->pending_length=0;ime->receiving=0;
    ae_terminal_composition_clear(&ime->composition);
    *ready=1;*out=(nx_utf8_span){ime->commit.text,ime->commit.size};return NX_OK;
}
