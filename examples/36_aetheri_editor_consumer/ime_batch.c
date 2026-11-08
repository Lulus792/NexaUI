#include "ime_batch.h"
#include <string.h>

static bool same(struct ae_direct_token a,struct ae_direct_token b) {
    return a.identity==b.identity && a.interaction==b.interaction && a.revision==b.revision &&
        a.cursor==b.cursor && a.anchor==b.anchor && a.selecting==b.selecting;
}
void editor_ime_batch_invalidate(struct editor_ime_batch *batch) {
    if(batch) {batch->target=batch->revision=0;batch->collecting=false;batch->bytes=0;}
}
bool editor_ime_batch_bind(struct editor_ime_batch *batch,struct ae_direct *input,uint64_t target,uint64_t revision) {
    if(!batch)return false;
    editor_ime_batch_invalidate(batch);
    if(!target || !revision || !ae_direct_token(input,&batch->expected))return false;
    batch->target=target;batch->revision=revision;batch->result=AE_DOCUMENT_OK;return true;
}
static enum editor_ime_batch_outcome reject(struct editor_ime_batch *batch,enum ae_document_result result) {
    editor_ime_batch_invalidate(batch);batch->result=result;return EDITOR_IME_REJECTED;
}
static unsigned encode(uint32_t scalar,char out[4]) {
    if(scalar<0x80) {out[0]=(char)scalar;return 1;}
    if(scalar<0x800) {out[0]=(char)(0xc0u|(scalar>>6));out[1]=(char)(0x80u|(scalar&63));return 2;}
    if(scalar>=0xd800 && scalar<=0xdfff)return 0;
    if(scalar<0x10000) {out[0]=(char)(0xe0u|(scalar>>12));out[1]=(char)(0x80u|((scalar>>6)&63));out[2]=(char)(0x80u|(scalar&63));return 3;}
    if(scalar<=0x10ffff) {out[0]=(char)(0xf0u|(scalar>>18));out[1]=(char)(0x80u|((scalar>>12)&63));out[2]=(char)(0x80u|((scalar>>6)&63));out[3]=(char)(0x80u|(scalar&63));return 4;}
    return 0;
}
static enum editor_ime_batch_outcome apply(struct editor_ime_batch *batch,struct ae_direct *input) {
    if(batch->has_delete!=((batch->flags&NX_IME_TRANSACTION_DELETE)!=0) ||
       (batch->has_text && !batch->text_ended))return reject(batch,AE_DOCUMENT_INVALID);
    struct ae_document_info info;
    if(!ae_document_info(ae_direct_document(input),&info))return reject(batch,AE_DOCUMENT_INVALID);
    size_t cursor=batch->expected.cursor,anchor=batch->expected.selecting?batch->expected.anchor:cursor;
    size_t begin=cursor<anchor?cursor:anchor,end=cursor>anchor?cursor:anchor;
    const nx_event *deletion=&batch->deletion;
    if(batch->has_delete && deletion->type==NX_EVENT_IME_DELETE_SURROUNDING) {
        if((!deletion->ime_before && !deletion->ime_after) || deletion->ime_before>begin ||
           deletion->ime_after>info.size-end)return reject(batch,AE_DOCUMENT_INVALID);
        begin-=deletion->ime_before;end+=deletion->ime_after;
    } else if(batch->has_delete) {
        if(!deletion->ime_length)return reject(batch,AE_DOCUMENT_INVALID);
        if(deletion->ime_index<0) {
            uint64_t before=(uint64_t)-(int64_t)deletion->ime_index;
            if(before>cursor)return reject(batch,AE_DOCUMENT_INVALID);
            begin=cursor-(size_t)before;
        } else {
            size_t after=(size_t)deletion->ime_index;
            if(after>info.size-cursor)return reject(batch,AE_DOCUMENT_INVALID);
            begin=cursor+after;
        }
        if(deletion->ime_length>info.size-begin)return reject(batch,AE_DOCUMENT_INVALID);
        end=begin+deletion->ime_length;
    }
    enum ae_document_result result=editor_direct_ime_replace(input,&batch->expected,begin,end,batch->commit,batch->bytes);
    if(result!=AE_DOCUMENT_OK)return reject(batch,result);
    batch->collecting=false;batch->bytes=0;batch->result=AE_DOCUMENT_OK;
    if(!ae_direct_token(input,&batch->expected))return reject(batch,AE_DOCUMENT_INVALID);
    return EDITOR_IME_APPLIED;
}
enum editor_ime_batch_outcome editor_ime_batch_event(struct editor_ime_batch *batch,
    struct ae_direct *input,const nx_event *event,bool admitted) {
    if(!batch || !event)return EDITOR_IME_REJECTED;
    bool boundary=event->type==NX_EVENT_IME_TRANSACTION_BEGIN || event->type==NX_EVENT_IME_TRANSACTION_END;
    bool payload=event->type==NX_EVENT_IME_PREEDIT_CHANGED || event->type==NX_EVENT_IME_DELETE_SURROUNDING ||
        event->type==NX_EVENT_IME_DELETE_RANGE || ((event->ime_target || event->ime_revision) &&
        (event->type==NX_EVENT_TEXT || event->type==NX_EVENT_TEXT_COMMAND));
    if(!boundary && !payload) {
        if(batch->collecting)editor_ime_batch_invalidate(batch);
        return EDITOR_IME_NOT_NATIVE;
    }
    struct ae_direct_token current;
    if(!admitted || !batch->target || !ae_direct_token(input,&current) || !same(current,batch->expected))
        return reject(batch,AE_DOCUMENT_INVALID);
    bool clear=event->type==NX_EVENT_IME_PREEDIT_CHANGED && !event->ime_target && !event->ime_revision;
    if(!clear && (event->ime_target!=batch->target || event->ime_revision!=batch->revision))return reject(batch,AE_DOCUMENT_INVALID);
    if(event->type==NX_EVENT_IME_TRANSACTION_BEGIN) {
        if(batch->collecting || !(event->ime_before&(NX_IME_TRANSACTION_COMMIT|NX_IME_TRANSACTION_DELETE)) ||
           (event->ime_before&~(NX_IME_TRANSACTION_COMMIT|NX_IME_TRANSACTION_DELETE)))return reject(batch,AE_DOCUMENT_INVALID);
        batch->flags=event->ime_before;batch->collecting=true;batch->has_delete=batch->has_text=batch->text_ended=false;batch->bytes=0;
        return EDITOR_IME_BUFFERED;
    }
    if(event->type==NX_EVENT_IME_PREEDIT_CHANGED)return EDITOR_IME_BUFFERED;
    if(!batch->collecting)return reject(batch,AE_DOCUMENT_INVALID);
    if(event->type==NX_EVENT_IME_TRANSACTION_END) {
        if(event->ime_before!=batch->flags)return reject(batch,AE_DOCUMENT_INVALID);
        return apply(batch,input);
    }
    if(event->type==NX_EVENT_IME_DELETE_SURROUNDING || event->type==NX_EVENT_IME_DELETE_RANGE) {
        if(batch->has_delete || batch->has_text || !(batch->flags&NX_IME_TRANSACTION_DELETE))return reject(batch,AE_DOCUMENT_INVALID);
        batch->deletion=*event;batch->has_delete=true;return EDITOR_IME_BUFFERED;
    }
    char bytes[4];unsigned count=event->type==NX_EVENT_TEXT?encode(event->scalar,bytes):0;
    if(!count || !(batch->flags&NX_IME_TRANSACTION_COMMIT) || batch->text_ended ||
       (event->ime_before&~(NX_IME_COMMIT_START|NX_IME_COMMIT_END)) ||
       (((event->ime_before&NX_IME_COMMIT_START)!=0)!=!batch->has_text) ||
       event->scalar<32 || (event->scalar>=127 && event->scalar<=159) ||
       (event->modifiers&NX_MOD_SUPER) || ((event->modifiers&NX_MOD_CONTROL) && !(event->modifiers&NX_MOD_ALT)))return reject(batch,AE_DOCUMENT_INVALID);
    if(count>sizeof(batch->commit)-batch->bytes)return reject(batch,AE_DOCUMENT_LIMIT);
    memcpy(batch->commit+batch->bytes,bytes,count);batch->bytes+=count;batch->has_text=true;
    batch->text_ended=(event->ime_before&NX_IME_COMMIT_END)!=0;return EDITOR_IME_BUFFERED;
}
