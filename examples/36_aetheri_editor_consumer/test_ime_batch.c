#include "ime_batch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"IME batch %d: %s\n",__LINE__,#x);exit(1);}} while(0)
static bool fail_malloc;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size) {return fail_malloc?NULL:__real_malloc(size);}
static struct ae_document *document;
static struct ae_direct *input;
static struct editor_ime_batch batch;
static struct ae_direct_token original;
static bool equal(struct ae_direct_token a,struct ae_direct_token b) {
    return a.identity==b.identity && a.interaction==b.interaction && a.revision==b.revision &&
        a.cursor==b.cursor && a.anchor==b.anchor && a.selecting==b.selecting;
}
static void setup(const char *text,size_t cursor) {
    ae_direct_destroy(input);ae_document_destroy(document);enum ae_document_result result;
    document=ae_document_create(text,strlen(text),&result);CHECK(document && result==AE_DOCUMENT_OK);
    input=ae_direct_create(document);CHECK(input && ae_direct_seek(input,cursor,false));
    CHECK(ae_direct_token(input,&original) && editor_ime_batch_bind(&batch,input,400,23));
}
static void content(const char *text,size_t cursor,size_t undo,size_t redo) {
    struct ae_document_info info;struct ae_document_history_info history;char bytes[128];size_t size=strlen(text);
    CHECK(size<sizeof(bytes) && ae_document_info(document,&info) && info.size==size && info.cursor==cursor);
    CHECK(ae_document_copy(document,0,size,bytes,sizeof(bytes)) && !memcmp(bytes,text,size));
    CHECK(ae_document_history_info(document,&history) && history.undo==undo && history.redo==redo);
}
static enum editor_ime_batch_outcome feed(nx_event event) {return editor_ime_batch_event(&batch,input,&event,true);}
static nx_event event(nx_event_type type) {return (nx_event){.type=type,.ime_target=400,.ime_revision=23};}
static nx_event boundary(nx_event_type type,uint32_t flags) {nx_event e=event(type);e.ime_before=flags;return e;}
static nx_event text(uint32_t scalar,uint32_t flags) {nx_event e=event(NX_EVENT_TEXT);e.scalar=scalar;e.ime_before=flags;return e;}
static void start(uint32_t flags) {CHECK(feed(boundary(NX_EVENT_IME_TRANSACTION_BEGIN,flags))==EDITOR_IME_BUFFERED);}
static void scalar(uint32_t value) {CHECK(feed(text(value,NX_IME_COMMIT_START|NX_IME_COMMIT_END))==EDITOR_IME_BUFFERED);}
static void end(uint32_t flags) {CHECK(feed(boundary(NX_EVENT_IME_TRANSACTION_END,flags))==EDITOR_IME_APPLIED);}
static void unchanged(const char *value,size_t cursor) {
    struct ae_direct_token now;CHECK(ae_direct_token(input,&now) && equal(original,now));content(value,cursor,0,0);
}
static void transactions(void) {
    uint32_t flags=NX_IME_TRANSACTION_COMMIT|NX_IME_TRANSACTION_DELETE;
    setup("aX\xcc\x81" "b",4);start(flags);
    nx_event deletion=event(NX_EVENT_IME_DELETE_RANGE);deletion.ime_index=-3;deletion.ime_length=1;
    CHECK(feed(deletion)==EDITOR_IME_BUFFERED);scalar('q');
    nx_event clear={.type=NX_EVENT_IME_PREEDIT_CHANGED};CHECK(feed(clear)==EDITOR_IME_BUFFERED);
    unchanged("aX\xcc\x81" "b",4);end(flags);
    content("aq\xcc\x81" "b",4,1,0);bool changed=false;
    CHECK(ae_direct_history(input,-1,&changed)==AE_DOCUMENT_OK && changed);content("aX\xcc\x81" "b",4,0,1);
    CHECK(ae_direct_history(input,1,&changed)==AE_DOCUMENT_OK && changed);content("aq\xcc\x81" "b",4,1,0);

    setup("abcdef",2);CHECK(ae_direct_seek(input,4,true) && editor_ime_batch_bind(&batch,input,400,23));
    start(flags);deletion=event(NX_EVENT_IME_DELETE_SURROUNDING);deletion.ime_before=1;deletion.ime_after=1;
    CHECK(feed(deletion)==EDITOR_IME_BUFFERED);scalar('X');end(flags);content("aXf",2,1,0);

    setup("a",1);start(NX_IME_TRANSACTION_COMMIT);
    CHECK(feed(text('x',NX_IME_COMMIT_START))==EDITOR_IME_BUFFERED);
    CHECK(feed(text(0xe9,NX_IME_COMMIT_END))==EDITOR_IME_BUFFERED);unchanged("a",1);end(NX_IME_TRANSACTION_COMMIT);
    content("ax\xc3\xa9",4,1,0);
    /* A second already queued batch retains the advertised native revision,
       while its original-model token advances after the first own edit. */
    start(NX_IME_TRANSACTION_COMMIT);scalar('y');end(NX_IME_TRANSACTION_COMMIT);content("ax\xc3\xa9" "y",5,2,0);
    CHECK(ae_direct_history(input,-1,&changed)==AE_DOCUMENT_OK && changed);content("ax\xc3\xa9",4,1,1);
    CHECK(feed(boundary(NX_EVENT_IME_TRANSACTION_BEGIN,NX_IME_TRANSACTION_COMMIT))==EDITOR_IME_REJECTED);
    content("ax\xc3\xa9",4,1,1);

    setup("a\r\nb",3);start(NX_IME_TRANSACTION_DELETE);deletion=event(NX_EVENT_IME_DELETE_SURROUNDING);deletion.ime_before=2;
    CHECK(feed(deletion)==EDITOR_IME_BUFFERED);end(NX_IME_TRANSACTION_DELETE);content("ab",1,1,0);
    setup("ab",1);start(NX_IME_TRANSACTION_COMMIT);end(NX_IME_TRANSACTION_COMMIT);content("ab",1,0,0);
    struct ae_direct_token now;CHECK(ae_direct_token(input,&now) && now.interaction>original.interaction);
}
static void rejection_matrix(void) {
    for(unsigned kind=0;kind<15;++kind) {
        setup("a\xcc\x81" "b",3);uint32_t flags=NX_IME_TRANSACTION_COMMIT|NX_IME_TRANSACTION_DELETE;
        start(flags);nx_event deletion=event(NX_EVENT_IME_DELETE_SURROUNDING);deletion.ime_before=2;
        CHECK(feed(deletion)==EDITOR_IME_BUFFERED);
        nx_event invalid=text('q',NX_IME_COMMIT_START|NX_IME_COMMIT_END);
        switch(kind) {
        case 0:invalid.ime_revision=22;break;
        case 1:invalid.ime_target=399;break;
        case 2:invalid.ime_before|=NX_IME_COMMIT_FILTERED;break;
        case 3:invalid.ime_before=NX_IME_COMMIT_END;break;
        case 4:invalid.scalar=0xd800;break;
        case 5:invalid.scalar=0x110000;break;
        case 6:invalid.scalar=0x85;break;
        case 7:invalid.scalar='\n';break;
        case 8:invalid.modifiers=NX_MOD_CONTROL;break;
        case 9:invalid.modifiers=NX_MOD_SUPER;break;
        case 10:invalid=deletion;break;
        case 11:invalid=boundary(NX_EVENT_IME_TRANSACTION_BEGIN,flags);break;
        case 12:invalid=boundary(NX_EVENT_IME_TRANSACTION_END,NX_IME_TRANSACTION_DELETE);break;
        case 13:invalid=event(NX_EVENT_TEXT_COMMAND);break;
        default:CHECK(ae_direct_cancel_input(input) && ae_direct_token(input,&original));break;
        }
        CHECK(feed(invalid)==EDITOR_IME_REJECTED && !batch.target && !batch.collecting);
        unchanged("a\xcc\x81" "b",3);
        CHECK(feed(boundary(NX_EVENT_IME_TRANSACTION_END,flags))==EDITOR_IME_REJECTED);unchanged("a\xcc\x81" "b",3);
    }
    setup("abc",1);CHECK(feed(boundary(NX_EVENT_IME_TRANSACTION_BEGIN,NX_IME_TRANSACTION_COMMIT|NX_IME_TRANSACTION_REJECTED))==EDITOR_IME_REJECTED);unchanged("abc",1);
    setup("abc",1);start(NX_IME_TRANSACTION_COMMIT);CHECK(feed(text('q',NX_IME_COMMIT_START))==EDITOR_IME_BUFFERED);
    CHECK(feed(boundary(NX_EVENT_IME_TRANSACTION_END,NX_IME_TRANSACTION_COMMIT))==EDITOR_IME_REJECTED);unchanged("abc",1);
    setup("abc",1);start(NX_IME_TRANSACTION_COMMIT);scalar('q');CHECK(feed(text('x',NX_IME_COMMIT_END))==EDITOR_IME_REJECTED);unchanged("abc",1);
    setup("abc",1);start(NX_IME_TRANSACTION_DELETE);CHECK(feed(boundary(NX_EVENT_IME_TRANSACTION_END,NX_IME_TRANSACTION_DELETE))==EDITOR_IME_REJECTED);unchanged("abc",1);
    setup("abc",1);start(NX_IME_TRANSACTION_COMMIT);scalar('q');nx_event key={.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_LEFT};
    CHECK(feed(key)==EDITOR_IME_NOT_NATIVE && !batch.target);unchanged("abc",1);
    CHECK(feed(boundary(NX_EVENT_IME_TRANSACTION_END,NX_IME_TRANSACTION_COMMIT))==EDITOR_IME_REJECTED);unchanged("abc",1);
}
static void range_refusals(void) {
    for(unsigned kind=0;kind<6;++kind) {
        setup("a\xcc\x81" "b",3);uint32_t flags=NX_IME_TRANSACTION_COMMIT|NX_IME_TRANSACTION_DELETE;start(flags);
        nx_event deletion=event(NX_EVENT_IME_DELETE_RANGE);deletion.ime_index=-1;deletion.ime_length=1;
        switch(kind) {
        case 0:deletion.ime_index=INT32_MIN;break;
        case 1:deletion.ime_index=INT32_MAX;break;
        case 2:deletion.ime_length=UINT32_MAX;break;
        case 3:deletion.ime_length=0;break;
        case 4:deletion.type=NX_EVENT_IME_DELETE_SURROUNDING;deletion.ime_before=UINT32_MAX;break;
        default:break; /* Endpoint inside a UTF-8 scalar. */
        }
        CHECK(feed(deletion)==EDITOR_IME_BUFFERED);scalar('q');
        CHECK(feed(boundary(NX_EVENT_IME_TRANSACTION_END,flags))==EDITOR_IME_REJECTED);unchanged("a\xcc\x81" "b",3);
    }
}
static void lifetime_and_budget(void) {
    setup("abc",1);nx_event preedit=event(NX_EVENT_IME_PREEDIT_CHANGED);
    CHECK(feed(preedit)==EDITOR_IME_BUFFERED);unchanged("abc",1);
    CHECK(editor_ime_batch_event(&batch,input,&preedit,false)==EDITOR_IME_REJECTED);unchanged("abc",1);
    CHECK(editor_ime_batch_bind(&batch,input,400,23));start(NX_IME_TRANSACTION_COMMIT);scalar('q');
    editor_ime_batch_invalidate(&batch);CHECK(feed(boundary(NX_EVENT_IME_TRANSACTION_END,NX_IME_TRANSACTION_COMMIT))==EDITOR_IME_REJECTED);unchanged("abc",1);
    CHECK(editor_ime_batch_bind(&batch,input,400,23));start(NX_IME_TRANSACTION_COMMIT);
    for(size_t n=0;n<sizeof(batch.commit);++n)CHECK(feed(text('x',n?0:NX_IME_COMMIT_START))==EDITOR_IME_BUFFERED);
    CHECK(feed(text('x',NX_IME_COMMIT_END))==EDITOR_IME_REJECTED && batch.result==AE_DOCUMENT_LIMIT);unchanged("abc",1);
    CHECK(editor_ime_batch_bind(&batch,input,400,23));start(NX_IME_TRANSACTION_COMMIT);scalar('q');
    fail_malloc=true;enum editor_ime_batch_outcome outcome=feed(boundary(NX_EVENT_IME_TRANSACTION_END,NX_IME_TRANSACTION_COMMIT));fail_malloc=false;
    CHECK(outcome==EDITOR_IME_REJECTED && batch.result==AE_DOCUMENT_NOMEM);unchanged("abc",1);
    CHECK(editor_ime_batch_bind(&batch,input,400,23));start(NX_IME_TRANSACTION_COMMIT);
    nx_event altgr=text(0xe9,NX_IME_COMMIT_START|NX_IME_COMMIT_END);altgr.modifiers=NX_MOD_CONTROL|NX_MOD_ALT;
    CHECK(feed(altgr)==EDITOR_IME_BUFFERED);end(NX_IME_TRANSACTION_COMMIT);content("a\xc3\xa9" "bc",3,1,0);
}
int main(void) {
    transactions();rejection_matrix();range_refusals();lifetime_and_budget();
    ae_direct_destroy(input);ae_document_destroy(document);
    puts("Complete IME batches: atomic final grapheme/selection/undo, same-epoch queue, interrupted/stale/filtered/range/budget/OOM refusals passed.");return 0;
}
