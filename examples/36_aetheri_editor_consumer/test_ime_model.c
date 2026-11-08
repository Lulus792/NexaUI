#include "ime_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"atomic IME model %d: %s\n",__LINE__,#x);exit(1);}} while(0)
static size_t fail_on,allocations;
void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size) {
    if(fail_on && ++allocations==fail_on)return NULL;
    return __real_malloc(size);
}
struct state {
    struct ae_document_info info;
    struct ae_document_history_info history;
    struct ae_direct_token token;
    char bytes[128];
};
static struct ae_direct *make(const char *text,size_t size,size_t cursor,struct ae_document **document) {
    enum ae_document_result result;*document=ae_document_create(text,size,&result);
    CHECK(*document && result==AE_DOCUMENT_OK);
    struct ae_direct *edit=ae_direct_create(*document);CHECK(edit && ae_direct_seek(edit,cursor,false));return edit;
}
static bool same_token(struct ae_direct_token a,struct ae_direct_token b) {
    return a.identity==b.identity && a.interaction==b.interaction && a.revision==b.revision &&
        a.cursor==b.cursor && a.anchor==b.anchor && a.selecting==b.selecting;
}
static struct state snapshot(struct ae_direct *edit) {
    struct state s={0};const struct ae_document *document=ae_direct_document(edit);
    CHECK(ae_document_info(document,&s.info) && s.info.size<=sizeof(s.bytes));
    CHECK(ae_document_copy(document,0,s.info.size,s.bytes,sizeof(s.bytes)) &&
        ae_document_history_info(document,&s.history) && ae_direct_token(edit,&s.token));return s;
}
static void unchanged(struct ae_direct *edit,const struct state *before) {
    struct state now=snapshot(edit);
    CHECK(now.info.size==before->info.size && now.info.lines==before->info.lines &&
        now.info.cursor==before->info.cursor && now.info.revision==before->info.revision &&
        now.info.dirty==before->info.dirty && same_token(now.token,before->token));
    CHECK(now.history.undo==before->history.undo && now.history.redo==before->history.redo &&
        now.history.bytes==before->history.bytes && !memcmp(now.bytes,before->bytes,now.info.size));
}
static void content(struct ae_document *document,const char *text,size_t size,size_t cursor) {
    struct ae_document_info info;char bytes[128];CHECK(size<=sizeof(bytes));
    CHECK(ae_document_info(document,&info) && info.size==size && info.cursor==cursor &&
        ae_document_copy(document,0,size,bytes,sizeof(bytes)) && !memcmp(bytes,text,size));
}
static void successful_ranges(void) {
    static const struct {
        const char *before,*insert,*after;size_t cursor,begin,end,next;
    } cases[]={
        {"a\xcc\x81" "b","X","aXb",3,1,3,2},
        {"a\xcc\x81" "b","\n","\n\xcc\x81" "b",3,0,1,1},
        {"ab","\xcc\x81","a\xcc\x81" "b",1,1,1,3},
        {"\xf0\x9f\x87\xa9\xf0\x9f\x87\xaa" "z","\xf0\x9f\x87\xab","\xf0\x9f\x87\xa9\xf0\x9f\x87\xab" "z",8,4,8,8},
        {"a\r\nb","","ab",3,1,3,1},
        {"ab","\r\n","a\r\nb",1,1,1,3},
        {"abcd","XY","aXYd",3,1,3,3}
    };
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        struct ae_document *document;struct ae_direct *edit=make(cases[i].before,strlen(cases[i].before),cases[i].cursor,&document);
        struct ae_direct_token token;CHECK(ae_direct_token(edit,&token));
        CHECK(editor_direct_ime_replace(edit,&token,cases[i].begin,cases[i].end,cases[i].insert,strlen(cases[i].insert))==AE_DOCUMENT_OK);
        content(document,cases[i].after,strlen(cases[i].after),cases[i].next);
        struct ae_document_history_info history;CHECK(ae_document_history_info(document,&history) && history.undo==1 && history.redo==0);
        struct state after=snapshot(edit);CHECK(editor_direct_ime_replace(edit,&token,0,0,"late",4)==AE_DOCUMENT_INVALID);unchanged(edit,&after);
        bool changed=false;CHECK(ae_direct_history(edit,-1,&changed)==AE_DOCUMENT_OK && changed);
        content(document,cases[i].before,strlen(cases[i].before),cases[i].cursor);
        CHECK(ae_direct_history(edit,1,&changed)==AE_DOCUMENT_OK && changed);
        content(document,cases[i].after,strlen(cases[i].after),cases[i].next);
        ae_direct_destroy(edit);ae_document_destroy(document);
    }
}
static void refusals(void) {
    char oversized[AE_DOCUMENT_LINE_BYTES+1];memset(oversized,'x',sizeof(oversized));
    struct ae_document *document,*other_document;struct ae_direct *edit=make("a\xcc\x81" "b",4,3,&document);
    struct ae_direct *other=make("a\xcc\x81" "b",4,3,&other_document);struct state before=snapshot(edit),foreign=snapshot(other);
    CHECK(editor_direct_ime_replace(edit,&foreign.token,0,1,"x",1)==AE_DOCUMENT_INVALID);unchanged(edit,&before);
    for(unsigned field=0;field<6;++field) {
        struct ae_direct_token altered=before.token;
        switch(field) {
        case 0:++altered.identity;break;
        case 1:++altered.interaction;break;
        case 2:++altered.revision;break;
        case 3:++altered.cursor;break;
        case 4:++altered.anchor;break;
        default:altered.selecting=!altered.selecting;break;
        }
        CHECK(editor_direct_ime_replace(edit,&altered,0,1,"x",1)==AE_DOCUMENT_INVALID);unchanged(edit,&before);
    }
    CHECK(editor_direct_ime_replace(edit,NULL,0,1,"x",1)==AE_DOCUMENT_INVALID);unchanged(edit,&before);
    CHECK(editor_direct_ime_replace(NULL,&before.token,0,1,"x",1)==AE_DOCUMENT_INVALID);unchanged(edit,&before);
    CHECK(editor_direct_ime_replace(edit,&before.token,2,3,"x",1)==AE_DOCUMENT_INVALID);unchanged(edit,&before);
    CHECK(editor_direct_ime_replace(edit,&before.token,1,2,"x",1)==AE_DOCUMENT_INVALID);unchanged(edit,&before);
    CHECK(editor_direct_ime_replace(edit,&before.token,1,3,"\xc2\x85",2)==AE_DOCUMENT_INVALID);unchanged(edit,&before);
    CHECK(editor_direct_ime_replace(edit,&before.token,1,3,"\xff",1)==AE_DOCUMENT_INVALID);unchanged(edit,&before);
    CHECK(editor_direct_ime_replace(edit,&before.token,1,5,"x",1)==AE_DOCUMENT_INVALID);unchanged(edit,&before);
    CHECK(editor_direct_ime_replace(edit,&before.token,3,1,"x",1)==AE_DOCUMENT_INVALID);unchanged(edit,&before);
    CHECK(editor_direct_ime_replace(edit,&before.token,1,3,NULL,1)==AE_DOCUMENT_INVALID);unchanged(edit,&before);
    CHECK(editor_direct_ime_replace(edit,&before.token,1,3,oversized,sizeof(oversized))==AE_DOCUMENT_LIMIT);unchanged(edit,&before);
    memset(oversized,'\n',sizeof(oversized));
    CHECK(editor_direct_ime_replace(edit,&before.token,1,3,oversized,AE_DOCUMENT_LINES)==AE_DOCUMENT_LIMIT);unchanged(edit,&before);
    CHECK(ae_direct_seek(edit,3,false));struct state moved=snapshot(edit);
    CHECK(editor_direct_ime_replace(edit,&before.token,1,3,"x",1)==AE_DOCUMENT_INVALID);unchanged(edit,&moved);
    ae_direct_destroy(other);ae_document_destroy(other_document);ae_direct_destroy(edit);ae_document_destroy(document);
    edit=make("a\r\nb",4,3,&document);before=snapshot(edit);
    CHECK(editor_direct_ime_replace(edit,&before.token,1,2,"",0)==AE_DOCUMENT_INVALID);unchanged(edit,&before);
    CHECK(editor_direct_ime_replace(edit,&before.token,2,3,"",0)==AE_DOCUMENT_INVALID);unchanged(edit,&before);
    CHECK(editor_direct_ime_replace(edit,&before.token,1,3,NULL,0)==AE_DOCUMENT_OK);content(document,"ab",2,1);
    ae_direct_destroy(edit);ae_document_destroy(document);
}
static void legacy_policy(void) {
    enum ae_document_result result;struct ae_document *document=ae_document_create("a\xcc\x81" "b",4,&result);CHECK(document && result==AE_DOCUMENT_OK);
    CHECK(ae_document_replace(document,1,3,"X",1)==AE_DOCUMENT_INVALID);content(document,"a\xcc\x81" "b",4,0);
    CHECK(ae_document_replace(document,0,3,"X",1)==AE_DOCUMENT_OK);content(document,"Xb",2,1);ae_document_destroy(document);
}
static void selection_and_grouping(void) {
    struct ae_document *document;struct ae_direct *edit=make("abcdef",6,1,&document);
    CHECK(ae_direct_seek(edit,5,true));struct ae_direct_token token;CHECK(ae_direct_token(edit,&token));
    CHECK(editor_direct_ime_replace(edit,&token,1,5,"XY",2)==AE_DOCUMENT_OK);content(document,"aXYf",4,3);
    struct ae_direct_selection selection;CHECK(ae_direct_selection(edit,&selection) && !selection.active);
    CHECK(ae_direct_text(edit,"z",1)==AE_DOCUMENT_OK);content(document,"aXYzf",5,4);
    bool changed=false;CHECK(ae_direct_history(edit,-1,&changed)==AE_DOCUMENT_OK && changed);content(document,"aXYf",4,3);
    CHECK(ae_direct_history(edit,-1,&changed)==AE_DOCUMENT_OK && changed);content(document,"abcdef",6,5);
    CHECK(ae_direct_token(edit,&token));uint64_t interaction=token.interaction;
    CHECK(editor_direct_ime_replace(edit,&token,5,5,"",0)==AE_DOCUMENT_OK);
    CHECK(ae_direct_token(edit,&token) && token.interaction>interaction);content(document,"abcdef",6,5);
    ae_direct_destroy(edit);ae_document_destroy(document);
}
static void allocation_rollback(void) {
    char insert[8000];memset(insert,'x',sizeof(insert));for(size_t i=79;i<sizeof(insert);i+=80)insert[i]='\n';
    unsigned failures=0;bool success=false;
    for(size_t nth=1;nth<=4;++nth) {
        struct ae_document *document;struct ae_direct *edit=make("abcdef",6,3,&document);
        CHECK(ae_direct_text(edit,"R",1)==AE_DOCUMENT_OK);bool changed=false;
        CHECK(ae_direct_history(edit,-1,&changed)==AE_DOCUMENT_OK && changed);
        CHECK(ae_direct_seek(edit,1,false) && ae_direct_seek(edit,5,true));struct state before=snapshot(edit);CHECK(before.history.redo==1);
        allocations=0;fail_on=nth;
        enum ae_document_result result=editor_direct_ime_replace(edit,&before.token,1,5,insert,sizeof(insert));
        fail_on=0;
        if(result==AE_DOCUMENT_NOMEM) {CHECK(allocations==nth);unchanged(edit,&before);++failures;}
        else {
            CHECK(result==AE_DOCUMENT_OK && allocations==3 && nth==4);struct ae_document_info info;struct ae_document_history_info history;
            CHECK(ae_document_info(document,&info) && info.size==8002 && info.lines==101 && info.cursor==8001);
            CHECK(ae_document_history_info(document,&history) && history.undo==1 && history.redo==0);
            CHECK(ae_direct_history(edit,-1,&changed)==AE_DOCUMENT_OK && changed);content(document,"abcdef",6,5);success=true;
        }
        ae_direct_destroy(edit);ae_document_destroy(document);
    }
    CHECK(failures==3 && success);
}
int main(void) {
    successful_ranges();refusals();legacy_policy();selection_and_grouping();allocation_rollback();
    puts("Atomic original-model IME: partial old graphemes, final caret/undo/redo, scalar/CRLF/control/stale guards and three allocation rollbacks passed.");return 0;
}
