#include "ime_transaction.h"
#include <string.h>

static void clear_pending(aetheri_ime_transaction *s) {
    s->preedit_length=s->commit_length=0;s->preedit[0]=s->commit[0]=0;
    s->begin=s->end=0;s->before=s->after=0;s->invalid=0;
    s->pending_active=0;s->pending_revision=0;
}
static void begin_pending(aetheri_ime_transaction *s) {
    if(!s->pending_active){s->pending_active=1;s->pending_revision=s->active?s->revision:0;}
}
void aetheri_ime_retire(aetheri_ime_transaction *s) {
    if(!s)return;
    clear_pending(s);s->active=0;s->revision=0;s->first_serial=0;
}
nx_result aetheri_ime_publish(aetheri_ime_transaction *s,uint32_t serial,uint64_t revision) {
    if(!s || !serial || !revision)return NX_ERROR_ARGUMENT;
    if(serial<=s->last_serial)return NX_ERROR_STATE;
    if(!s->active || s->revision!=revision)s->first_serial=serial;
    s->last_serial=serial;s->revision=revision;s->active=1;
    return NX_OK;
}
static nx_result set_text(aetheri_ime_transaction *s,char *dst,size_t *out_length,const char *text,size_t length) {
    if(!s)return NX_ERROR_ARGUMENT;
    begin_pending(s);
    if(length>AETHERI_IME_TEXT_MAX) {s->invalid=1;return NX_ERROR_LIMIT;}
    if((!text && length) || (length && memchr(text,0,length)) ||
       nx_utf8_validate(text?text:"",length,NULL)!=NX_OK) {s->invalid=1;return NX_ERROR_ARGUMENT;}
    if(length)memcpy(dst,text,length);
    dst[length]=0;*out_length=length;return NX_OK;
}
nx_result aetheri_ime_preedit(aetheri_ime_transaction *s,const char *text,size_t length,int32_t begin,int32_t end) {
    nx_result r=set_text(s,s?s->preedit:NULL,s?&s->preedit_length:NULL,text,length);
    if(r!=NX_OK)return r;
    if(!((begin==-1 && end==-1) || (begin>=0 && end>=0 && (size_t)begin<=length && (size_t)end<=length &&
       ((size_t)begin==length || ((unsigned char)text[begin]&0xc0u)!=0x80u) &&
       ((size_t)end==length || ((unsigned char)text[end]&0xc0u)!=0x80u)))) {
        s->invalid=1;return NX_ERROR_ARGUMENT;
    }
    s->begin=begin;s->end=end;return NX_OK;
}
nx_result aetheri_ime_commit(aetheri_ime_transaction *s,const char *text,size_t length) {
    return set_text(s,s?s->commit:NULL,s?&s->commit_length:NULL,text,length);
}
void aetheri_ime_delete(aetheri_ime_transaction *s,uint32_t before,uint32_t after) {
    if(s){begin_pending(s);s->before=before;s->after=after;}
}
nx_result aetheri_ime_done(aetheri_ime_transaction *s,uint32_t serial,uint64_t revision,aetheri_ime_result *out) {
    if(!s || !out)return NX_ERROR_ARGUMENT;
    *out=(aetheri_ime_result){0};nx_result result=NX_OK;
    if(!s->active || !revision || revision!=s->revision || serial<s->first_serial || serial>s->last_serial ||
       (s->pending_active && s->pending_revision!=revision))
        result=NX_ERROR_STATE;
    else if(s->invalid)result=NX_ERROR_ARGUMENT;
    else {
        size_t offset=0;
        while(offset<s->commit_length) {
            if(out->scalar_count==AETHERI_IME_SCALAR_MAX) {result=NX_ERROR_LIMIT;break;}
            uint32_t scalar=0;
            if(nx_utf8_decode(s->commit,s->commit_length,&offset,&scalar)!=NX_OK ||
               scalar<32 || (scalar>=127 && scalar<=159) || scalar==0x2028 || scalar==0x2029) {
                result=NX_ERROR_ARGUMENT;break;
            }
            out->scalars[out->scalar_count++]=scalar;
        }
        if(result==NX_OK) {
            memcpy(out->preedit,s->preedit,s->preedit_length+1u);out->preedit_length=s->preedit_length;
            out->begin=s->begin;out->end=s->end;out->before=s->before;out->after=s->after;
            out->can_commit_state=(serial==s->last_serial);
        }
    }
    clear_pending(s);
    if(result!=NX_OK)*out=(aetheri_ime_result){0};
    return result;
}
