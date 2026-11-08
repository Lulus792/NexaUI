#include "accessible_model.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"accessible model %d: %s\n",__LINE__,#x);exit(1);}} while(0)
static size_t fail_on,allocations;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size) {
    if(fail_on && ++allocations==fail_on)return NULL;
    return __real_malloc(size);
}
static const char sample[]="A\xc3\xa9" "x\xcc\x81\r\n\xf0\x9f\x98\x80Z";
struct fixture {struct ae_document *document;struct ae_direct *input;};
struct saved {struct editor_accessible_state state;struct ae_document_info info;struct ae_document_history_info history;char bytes[128];};
static struct fixture make(const char *text,size_t length) {
    enum ae_document_result result;struct fixture f={0};f.document=ae_document_create(text,length,&result);
    CHECK(f.document && result==AE_DOCUMENT_OK);f.input=ae_direct_create(f.document);CHECK(f.input);return f;
}
static void destroy(struct fixture f) {ae_direct_destroy(f.input);ae_document_destroy(f.document);}
static bool same(struct ae_direct_token a,struct ae_direct_token b) {
    return a.identity==b.identity && a.interaction==b.interaction && a.revision==b.revision &&
        a.cursor==b.cursor && a.anchor==b.anchor && a.selecting==b.selecting;
}
static struct saved save(struct fixture f) {
    struct saved s={0};CHECK(editor_accessible_snapshot(f.input,&s.state) && ae_document_info(f.document,&s.info));
    CHECK(s.state.bytes<=sizeof(s.bytes) && ae_document_copy(f.document,0,s.state.bytes,s.bytes,sizeof(s.bytes)) &&
        ae_document_history_info(f.document,&s.history));return s;
}
static void unchanged(struct fixture f,struct saved before) {
    struct saved now=save(f);
    CHECK(same(now.state.token,before.state.token) && now.state.bytes==before.state.bytes &&
        now.state.characters==before.state.characters && now.state.anchor==before.state.anchor && now.state.cursor==before.state.cursor);
    CHECK(now.info.dirty==before.info.dirty && now.info.lines==before.info.lines && now.history.undo==before.history.undo &&
        now.history.redo==before.history.redo && now.history.bytes==before.history.bytes && !memcmp(now.bytes,before.bytes,now.state.bytes));
}
static void text(struct fixture f,const char *expected,size_t cursor) {
    struct saved s=save(f);CHECK(s.state.bytes==strlen(expected) && !memcmp(s.bytes,expected,s.state.bytes) && s.info.cursor==cursor);
}
static void reading(void) {
    struct fixture f=make(sample,sizeof(sample)-1);struct saved before=save(f);CHECK(before.state.bytes==13 && before.state.characters==8);
    char out[32];memset(out,'!',sizeof(out));size_t length=99;
    CHECK(editor_accessible_read(f.input,&before.state.token,1,4,NULL,0,&length)==AE_DOCUMENT_OK && length==5);
    CHECK(editor_accessible_read(f.input,&before.state.token,1,4,out,4,&length)==AE_DOCUMENT_LIMIT && length==5);
    for(size_t i=0;i<sizeof(out);++i)CHECK(out[i]=='!');
    CHECK(editor_accessible_read(f.input,&before.state.token,1,4,out,5,&length)==AE_DOCUMENT_OK && length==5 && !memcmp(out,sample+1,5) && out[5]=='!');
    CHECK(editor_accessible_read(f.input,&before.state.token,3,4,out,sizeof(out),&length)==AE_DOCUMENT_OK && length==2 && !memcmp(out,"\xcc\x81",2));
    CHECK(editor_accessible_read(f.input,&before.state.token,4,5,out,sizeof(out),&length)==AE_DOCUMENT_OK && length==1 && out[0]=='\r');
    CHECK(editor_accessible_read(f.input,&before.state.token,6,-1,out,sizeof(out),&length)==AE_DOCUMENT_OK && length==5 && !memcmp(out,sample+8,5));
    CHECK(editor_accessible_read(f.input,&before.state.token,8,8,out,0,&length)==AE_DOCUMENT_OK && length==0);
    const int64_t bad[][2]={{-1,1},{INT64_MIN,-1},{0,-2},{INT64_MAX,-1},{0,INT64_MAX},{4,3},{9,-1},{0,9}};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
        memset(out,'!',sizeof(out));length=99;
        CHECK(editor_accessible_read(f.input,&before.state.token,bad[i][0],bad[i][1],out,sizeof(out),&length)==AE_DOCUMENT_INVALID && !length);
        for(size_t n=0;n<sizeof(out);++n)CHECK(out[n]=='!');
    }
    allocations=0;fail_on=1;
    CHECK(editor_accessible_read(f.input,&before.state.token,0,-1,out,sizeof(out),&length)==AE_DOCUMENT_OK && length==13);
    struct editor_accessible_state state;CHECK(editor_accessible_snapshot(f.input,&state));fail_on=0;CHECK(!allocations);unchanged(f,before);
    CHECK(editor_accessible_read(f.input,&before.state.token,0,-1,NULL,1,&length)==AE_DOCUMENT_INVALID);
    CHECK(editor_accessible_read(f.input,&before.state.token,0,-1,out,sizeof(out),NULL)==AE_DOCUMENT_INVALID);
    CHECK(!editor_accessible_snapshot(f.input,NULL) && !editor_accessible_snapshot(NULL,&state));
    destroy(f);
}
static void selecting(void) {
    struct fixture f=make(sample,sizeof(sample)-1);struct saved before=save(f);
    CHECK(!editor_accessible_select(f.input,&before.state.token,6,3));unchanged(f,before);
    CHECK(!editor_accessible_select(f.input,&before.state.token,3,1));unchanged(f,before);
    CHECK(!editor_accessible_select(f.input,&before.state.token,1,5));unchanged(f,before);
    CHECK(!editor_accessible_select(f.input,&before.state.token,-1,1));unchanged(f,before);
    CHECK(!editor_accessible_select(f.input,&before.state.token,0,INT64_MAX));unchanged(f,before);
    CHECK(editor_accessible_select(f.input,&before.state.token,6,1));struct saved selected=save(f);
    CHECK(selected.state.anchor==6 && selected.state.cursor==1 && selected.state.token.anchor==8 && selected.state.token.cursor==1 && selected.state.token.selecting);
    CHECK(selected.history.undo==0 && selected.info.revision==before.info.revision && selected.info.dirty==before.info.dirty && selected.state.token.interaction==before.state.token.interaction+1);
    CHECK(!editor_accessible_select(f.input,&before.state.token,0,0));unchanged(f,selected);
    CHECK(editor_accessible_select(f.input,&selected.state.token,1,6));selected=save(f);CHECK(selected.state.anchor==1 && selected.state.cursor==6);
    CHECK(editor_accessible_select(f.input,&selected.state.token,8,8));selected=save(f);CHECK(!selected.state.token.selecting && selected.state.cursor==8 && selected.state.anchor==8);
    CHECK(editor_accessible_select(f.input,&selected.state.token,8,8));struct saved noop=save(f);CHECK(noop.state.token.interaction==selected.state.token.interaction+1 && noop.history.undo==0);
    destroy(f);
}
static void edits_and_gap(void) {
    struct fixture f=make(sample,sizeof(sample)-1);struct saved before=save(f);
    CHECK(editor_accessible_replace(f.input,&before.state.token,2,4,"q",1)==AE_DOCUMENT_OK);
    text(f,"A\xc3\xa9q\r\n\xf0\x9f\x98\x80Z",4);struct saved after=save(f);CHECK(after.state.characters==7 && after.history.undo==1);
    char bytes[32];size_t length=0;CHECK(editor_accessible_read(f.input,&after.state.token,0,-1,bytes,sizeof(bytes),&length)==AE_DOCUMENT_OK && length==after.state.bytes && !memcmp(bytes,after.bytes,length));
    CHECK(editor_accessible_replace(f.input,&before.state.token,0,-1,"stale",5)==AE_DOCUMENT_INVALID);unchanged(f,after);
    bool changed=false;CHECK(ae_direct_history(f.input,-1,&changed)==AE_DOCUMENT_OK && changed);text(f,sample,0);
    CHECK(ae_direct_history(f.input,1,&changed)==AE_DOCUMENT_OK && changed);text(f,"A\xc3\xa9q\r\n\xf0\x9f\x98\x80Z",4);
    destroy(f);f=make(sample,sizeof(sample)-1);before=save(f);
    CHECK(editor_accessible_replace(f.input,&before.state.token,3,4,"",0)==AE_DOCUMENT_OK);text(f,"A\xc3\xa9x\r\n\xf0\x9f\x98\x80Z",4);
    before=save(f);CHECK(editor_accessible_replace(f.input,&before.state.token,4,5,"",0)==AE_DOCUMENT_INVALID);unchanged(f,before);
    CHECK(editor_accessible_replace(f.input,&before.state.token,3,4,"",0)==AE_DOCUMENT_INVALID);unchanged(f,before);
    CHECK(editor_accessible_replace(f.input,&before.state.token,3,5,NULL,0)==AE_DOCUMENT_OK);text(f,"A\xc3\xa9x\xf0\x9f\x98\x80Z",4);
    before=save(f);CHECK(editor_accessible_replace(f.input,&before.state.token,0,-1,"\xc3\xa9\r\n",4)==AE_DOCUMENT_OK);text(f,"\xc3\xa9\r\n",4);
    destroy(f);f=make("",0);before=save(f);CHECK(!before.state.characters && !before.state.bytes);
    CHECK(editor_accessible_read(f.input,&before.state.token,0,-1,NULL,0,&length)==AE_DOCUMENT_OK && !length);
    CHECK(editor_accessible_replace(f.input,&before.state.token,0,-1,NULL,0)==AE_DOCUMENT_OK);after=save(f);CHECK(after.state.token.interaction==before.state.token.interaction+1);
    CHECK(editor_accessible_replace(f.input,&after.state.token,0,-1,"\xc3\xa9",2)==AE_DOCUMENT_OK);text(f,"\xc3\xa9",2);destroy(f);
}
static void stale_and_invalid(void) {
    struct fixture f=make(sample,sizeof(sample)-1),other=make(sample,sizeof(sample)-1);struct saved before=save(f),foreign=save(other);
    struct ae_direct_token tokens[7];for(unsigned i=0;i<6;++i)tokens[i]=before.state.token;
    ++tokens[0].identity;++tokens[1].interaction;++tokens[2].revision;++tokens[3].cursor;++tokens[4].anchor;tokens[5].selecting=!tokens[5].selecting;tokens[6]=foreign.state.token;
    char out[16];size_t length;
    for(unsigned i=0;i<7;++i) {
        CHECK(editor_accessible_read(f.input,&tokens[i],0,-1,out,sizeof(out),&length)==AE_DOCUMENT_INVALID);
        CHECK(editor_accessible_replace(f.input,&tokens[i],0,1,"q",1)==AE_DOCUMENT_INVALID);
        CHECK(!editor_accessible_select(f.input,&tokens[i],0,1));unchanged(f,before);
    }
    CHECK(editor_accessible_replace(f.input,&before.state.token,0,1,"\xff",1)==AE_DOCUMENT_INVALID);unchanged(f,before);
    CHECK(editor_accessible_replace(f.input,&before.state.token,0,1,"\0",1)==AE_DOCUMENT_INVALID);unchanged(f,before);
    CHECK(editor_accessible_replace(f.input,&before.state.token,0,1,"\xc2\x85",2)==AE_DOCUMENT_INVALID);unchanged(f,before);
    CHECK(editor_accessible_replace(f.input,&before.state.token,0,1,NULL,1)==AE_DOCUMENT_INVALID);unchanged(f,before);
    CHECK(editor_accessible_replace(f.input,NULL,0,1,"q",1)==AE_DOCUMENT_INVALID);
    CHECK(editor_accessible_read(NULL,&before.state.token,0,1,out,sizeof(out),&length)==AE_DOCUMENT_INVALID);
    CHECK(ae_direct_cancel_input(f.input));struct saved cancelled=save(f);
    CHECK(editor_accessible_read(f.input,&before.state.token,0,-1,out,sizeof(out),&length)==AE_DOCUMENT_INVALID);unchanged(f,cancelled);
    destroy(other);destroy(f);
}
static void allocation_rollback(void) {
    char insert[8000];memset(insert,'x',sizeof(insert));for(size_t i=79;i<sizeof(insert);i+=80)insert[i]='\n';
    unsigned failures=0;bool succeeded=false;
    for(size_t nth=1;nth<=4;++nth) {
        struct fixture f=make("abcdef",6);CHECK(ae_direct_seek(f.input,3,false) && ae_direct_text(f.input,"R",1)==AE_DOCUMENT_OK);
        bool changed=false;CHECK(ae_direct_history(f.input,-1,&changed)==AE_DOCUMENT_OK && changed);
        CHECK(ae_direct_seek(f.input,1,false) && ae_direct_seek(f.input,5,true));struct saved before=save(f);CHECK(before.history.redo==1);
        allocations=0;fail_on=nth;enum ae_document_result result=editor_accessible_replace(f.input,&before.state.token,1,5,insert,sizeof(insert));fail_on=0;
        if(result==AE_DOCUMENT_NOMEM) {CHECK(allocations==nth);unchanged(f,before);++failures;}
        else {
            CHECK(result==AE_DOCUMENT_OK && allocations==3 && nth==4);struct editor_accessible_state state;CHECK(editor_accessible_snapshot(f.input,&state) && state.bytes==8002 && state.characters==8002);
            CHECK(ae_direct_history(f.input,-1,&changed)==AE_DOCUMENT_OK && changed);text(f,"abcdef",5);succeeded=true;
        }
        destroy(f);
    }
    CHECK(failures==3 && succeeded);
}
static void large_read(void) {
    const size_t length=600000;char *input=malloc(length),*out=malloc(length);CHECK(input && out);
    memset(input,'x',length);for(size_t i=79;i<length;i+=80)input[i]='\n';struct fixture f=make(input,length);
    struct editor_accessible_state state;CHECK(editor_accessible_snapshot(f.input,&state) && state.bytes==length && state.characters==length);
    size_t actual=0;allocations=0;fail_on=1;
    CHECK(editor_accessible_read(f.input,&state.token,0,-1,out,length,&actual)==AE_DOCUMENT_OK && actual==length && !memcmp(input,out,length));
    fail_on=0;CHECK(!allocations);destroy(f);free(input);free(out);
}
int main(void) {
    reading();selecting();edits_and_gap();stale_and_invalid();allocation_rollback();large_read();
    puts("Original accessible model: scalar reads across gap/CRLF/combining/emoji, atomic two-endpoint selection, all-token stale guards, one-undo edits, three allocation rollbacks and 600000-byte no-allocation read passed.");return 0;
}
