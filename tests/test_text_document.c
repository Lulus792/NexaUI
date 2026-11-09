#include "nexaui/nexaui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__linux__)
#include "text_value.h"
#include "fixture_font.h"
#endif

#define CHECK(x) do {if(!(x)){fprintf(stderr,"text document %d: %s\n",__LINE__,#x);return 1;}} while(0)
typedef struct model {
    char text[16384];size_t length,anchor,cursor;
    uint64_t identity,revision;uint32_t operations;
    unsigned calls;nx_result fail;
} model;
static nx_text_document_state snapshot(model *m) {
    return (nx_text_document_state){{m->text,m->length},m->identity,m->revision,
        m->anchor,m->cursor,m->operations,1};
}
static nx_result apply(void *user,const nx_text_document_request *request,nx_text_document_state *out) {
    model *m=user;++m->calls;
    if(m->fail!=NX_OK)return m->fail;
    if(request->identity!=m->identity || request->revision!=m->revision ||
       request->start_byte>request->end_byte || request->end_byte>m->length)return NX_ERROR_STATE;
    if(request->operation==NX_TEXT_DOCUMENT_SET_SELECTION) {
        m->anchor=request->start_byte;m->cursor=request->end_byte;
    } else if(request->operation==NX_TEXT_DOCUMENT_REPLACE_RANGE) {
        size_t next=m->length-(request->end_byte-request->start_byte)+request->value.length;
        if(next>sizeof(m->text))return NX_ERROR_LIMIT;
        memmove(m->text+request->start_byte+request->value.length,m->text+request->end_byte,m->length-request->end_byte);
        if(request->value.length)memcpy(m->text+request->start_byte,request->value.data,request->value.length);
        m->length=next;m->anchor=m->cursor=request->start_byte+request->value.length;
    } else return NX_ERROR_UNSUPPORTED;
    ++m->revision;*out=snapshot(m);return NX_OK;
}
static model initial(void) {
    model m={0};memcpy(m.text,"A\xc3\xa9\xf0\x9f\x8c\x8d",7);m.length=7;
    m.identity=91;m.revision=1;m.operations=NX_TEXT_DOCUMENT_REPLACE|NX_TEXT_DOCUMENT_SELECT;return m;
}
static nx_widget_desc widget(model *m) {
    nx_widget_desc w={0};w.semantic.id=2;w.semantic.parent_id=1;w.semantic.role=NX_ROLE_TEXT_FIELD;
    w.semantic.bounds=(nx_rect){0,0,200,100};w.semantic.name=(nx_utf8_span){"Document",8};
    w.semantic.actions=NX_ACTION_FOCUS;w.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE;
    if(m->operations&NX_TEXT_DOCUMENT_REPLACE)w.semantic.actions|=NX_ACTION_SET_VALUE;
    else w.semantic.state|=NX_SEM_READONLY;
    return w;
}
static int begin(nx_context *ctx,int disabled,uint64_t parent) {
    CHECK(nx_begin_frame(ctx,300,200,0)==NX_OK);
    nx_semantic_desc root={0};root.id=parent;root.role=NX_ROLE_WINDOW;root.name=(nx_utf8_span){"Owner",5};root.bounds=(nx_rect){0,0,300,200};
    root.state=disabled?NX_SEM_DISABLED:0;CHECK(nx_semantics_add(ctx,&root,NULL)==NX_OK);return 0;
}
static int frame(nx_context *ctx,model *m,int disabled,int omit,nx_text_document_result *out) {
    CHECK(begin(ctx,disabled,1)==0);
    if(!omit) {nx_widget_desc w=widget(m);nx_text_document_state s=snapshot(m);CHECK(nx_text_document_register(ctx,&w,&s,apply,m,out)==NX_OK);}
    CHECK(nx_end_frame(ctx)==NX_OK);return 0;
}
static int value(nx_context *ctx,const char *expected,size_t length) {
    uint32_t index=0;nx_semantic_view view;CHECK(nx_semantics_find(ctx,2,&index)==NX_OK && nx_semantics_get(ctx,index,&view)==NX_OK);
    CHECK(view.value.length==length && (!length || !memcmp(view.value.data,expected,length)));return 0;
}
static int ordinary(void) {
    nx_context *ctx=NULL;model m=initial();nx_text_document_result out;
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && frame(ctx,&m,0,0,&out)==0 && !out.applied && !m.calls);
    CHECK(nx_semantics_request_text_value(ctx,2,(nx_utf8_span){"Gr\xc3\xbc\xc3\x9f" "e",7})==NX_OK);
    CHECK(frame(ctx,&m,0,0,&out)==0 && out.applied && out.edit_result==NX_OK && m.calls==1 && m.revision==2);
    CHECK(value(ctx,m.text,m.length)==0 && frame(ctx,&m,0,0,&out)==0 && !out.applied && m.calls==1);
    CHECK(nx_semantics_request_text_value(ctx,2,(nx_utf8_span){m.text,m.length})==NX_OK);
    CHECK(frame(ctx,&m,0,0,&out)==0 && out.applied && m.calls==2 && m.revision==3);
    CHECK(nx_semantics_request_text_value(ctx,2,(nx_utf8_span){NULL,0})==NX_OK);
    CHECK(frame(ctx,&m,0,0,&out)==0 && out.applied && m.calls==3 && !m.length && value(ctx,"",0)==0);
    CHECK(nx_semantics_request_text_value(ctx,2,(nx_utf8_span){"back",4})==NX_OK);
    CHECK(frame(ctx,&m,0,0,&out)==0 && out.applied && m.calls==4 && value(ctx,"back",4)==0);
    nx_context_destroy(ctx);return 0;
}
static int stale_and_policy(void) {
    for(unsigned mode=0;mode<7;++mode) {
        nx_context *ctx=NULL;model m=initial();nx_text_document_result out;
        CHECK(nx_context_create(NULL,&ctx)==NX_OK && frame(ctx,&m,0,0,&out)==0);
        CHECK(nx_semantics_request_text_value(ctx,2,(nx_utf8_span){"late",4})==NX_OK);
        if(mode==0)++m.identity;
        if(mode==1)++m.revision;
        if(mode==2)m.operations=NX_TEXT_DOCUMENT_SELECT;
        if(mode==3)m.operations=NX_TEXT_DOCUMENT_REPLACE;
        CHECK(begin(ctx,mode==4,mode==6?3:1)==0);
        if(mode!=5) {
            nx_widget_desc w=widget(&m);w.semantic.parent_id=mode==6?3:1;
            nx_text_document_state s=snapshot(&m);CHECK(nx_text_document_register(ctx,&w,&s,apply,&m,&out)==NX_OK && !out.applied);
        }
        CHECK(nx_end_frame(ctx)==NX_OK && !m.calls && m.length==7);
        CHECK(frame(ctx,&m,0,0,&out)==0 && !m.calls);
        if(!(m.operations&NX_TEXT_DOCUMENT_REPLACE))CHECK(nx_semantics_request_text_value(ctx,2,(nx_utf8_span){"no",2})==NX_ERROR_STATE);
        nx_context_destroy(ctx);
    }
    return 0;
}
static int failures(void) {
    const nx_result errors[]={NX_ERROR_MEMORY,NX_ERROR_LIMIT,NX_ERROR_ARGUMENT,NX_ERROR_STATE,NX_ERROR_UNSUPPORTED};
    for(size_t i=0;i<sizeof(errors)/sizeof(errors[0]);++i) {
        nx_context *ctx=NULL;model m=initial();nx_text_document_result out;
        CHECK(nx_context_create(NULL,&ctx)==NX_OK && frame(ctx,&m,0,0,&out)==0);
        CHECK(nx_semantics_request_text_value(ctx,2,(nx_utf8_span){"rejected",8})==NX_OK);m.fail=errors[i];
        CHECK(frame(ctx,&m,0,0,&out)==0 && !out.applied && out.edit_result==errors[i] && m.calls==1 && m.revision==1 && value(ctx,m.text,m.length)==0);
        m.fail=NX_OK;CHECK(frame(ctx,&m,0,0,&out)==0 && m.calls==1 && !out.applied);nx_context_destroy(ctx);
    }
    return 0;
}
static int invalid(void) {
    nx_context *ctx=NULL;model m=initial();nx_text_document_result out;
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && begin(ctx,0,1)==0);
    for(unsigned mode=0;mode<13;++mode) {
        nx_widget_desc w=widget(&m);nx_text_document_state s=snapshot(&m);nx_text_document_apply callback=apply;
        nx_result expected=NX_ERROR_ARGUMENT;
        switch(mode) {
        case 0:s.identity=0;break;
        case 1:s.revision=0;break;
        case 2:s.operations|=8u;break;
        case 3:s.multiline=2;break;
        case 4:s.anchor_byte=2;break;
        case 5:s.cursor_byte=8;break;
        case 6:w.semantic.state|=NX_SEM_PROTECTED;break;
        case 7:w.semantic.state|=NX_SEM_READONLY;break;
        case 8:w.semantic.actions&=~NX_ACTION_SET_VALUE;break;
        case 9:w.semantic.value=(nx_utf8_span){"wrong",5};break;
        case 10:callback=NULL;break;
        case 11:w.semantic.role=NX_ROLE_CUSTOM;break;
        default:s.value.length=NX_TEXT_DOCUMENT_MAX_BYTES+1u;expected=NX_ERROR_LIMIT;break;
        }
        CHECK(nx_text_document_register(ctx,&w,&s,callback,&m,&out)==expected);
    }
    nx_widget_desc w=widget(&m);nx_text_document_state s=snapshot(&m);
    CHECK(nx_text_document_register(ctx,&w,&s,apply,&m,&out)==NX_OK && !m.calls);
    CHECK(nx_end_frame(ctx)==NX_OK);nx_context_destroy(ctx);return 0;
}
typedef struct memory {int fail;unsigned calls;int *revoke_current;} memory;
static void *allocate(void *user,size_t bytes) {memory *m=user;++m->calls;if(m->revoke_current)*m->revoke_current=0;return m->fail?NULL:malloc(bytes);}
static void deallocate(void *user,void *p,size_t bytes) {(void)user;(void)bytes;free(p);}
static int reserve_before_apply(void) {
    memory mem={0};nx_context_desc desc={0};desc.allocator=(nx_allocator){&mem,allocate,deallocate};
    nx_context *ctx=NULL;model m=initial();nx_text_document_result out;
    CHECK(nx_context_create(&desc,&ctx)==NX_OK && frame(ctx,&m,0,0,&out)==0 && frame(ctx,&m,0,0,&out)==0);
    char payload[8192];memset(payload,'x',sizeof(payload));CHECK(nx_semantics_request_text_value(ctx,2,(nx_utf8_span){payload,sizeof(payload)})==NX_OK);
    CHECK(begin(ctx,0,1)==0);nx_widget_desc w=widget(&m);nx_text_document_state s=snapshot(&m);
    mem.fail=1;unsigned before=mem.calls;
    CHECK(nx_text_document_register(ctx,&w,&s,apply,&m,&out)==NX_ERROR_MEMORY && mem.calls==before+1 && !m.calls && m.length==7);
    mem.fail=0;CHECK(nx_text_document_register(ctx,&w,&s,apply,&m,&out)==NX_OK && out.applied && m.calls==1);
    CHECK(nx_end_frame(ctx)==NX_OK && value(ctx,payload,sizeof(payload))==0);nx_context_destroy(ctx);return 0;
}
static int large_snapshot(void) {
    nx_context *ctx=NULL;model m=initial();nx_widget_desc w=widget(&m);nx_text_document_result out;
    const size_t lengths[]={600000,NX_TEXT_DOCUMENT_MAX_BYTES};
    for(size_t sample=0;sample<sizeof(lengths)/sizeof(lengths[0]);++sample) {
    size_t length=lengths[sample];char *bytes=malloc(length);CHECK(bytes);memset(bytes,'x',length);
    nx_text_document_state s={{bytes,length},19,1,0,length,0,1};w.semantic.state=NX_SEM_READONLY;w.semantic.actions=NX_ACTION_FOCUS;
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && begin(ctx,0,1)==0);
    CHECK(nx_text_document_register(ctx,&w,&s,NULL,NULL,&out)==NX_OK && nx_end_frame(ctx)==NX_OK && value(ctx,bytes,length)==0);
    uint32_t index=0;nx_semantic_view v;CHECK(nx_semantics_find(ctx,2,&index)==NX_OK && nx_semantics_get(ctx,index,&v)==NX_OK && v.value.data!=bytes);
    memset(bytes,'y',length);CHECK(v.value.data[0]=='x' && v.value.data[length-1]=='x');free(bytes);
    nx_context_destroy(ctx);
    }
    return 0;
}
#if defined(__linux__)
typedef struct ticket {unsigned refs,completions;int current,applied;} ticket;
static void retain(void *user) {++((ticket *)user)->refs;}
static void release(void *user) {--((ticket *)user)->refs;}
static int current(void *user,uint64_t token,uint64_t context,uint64_t id,nx_text_value_target target) {
    (void)token;(void)context;(void)id;(void)target;return ((ticket *)user)->current;
}
static void completed(void *user,int applied) {
    ticket *t=user;++t->completions;t->applied=t->refs==1?applied:-100;
}
static nx_text_value_guard guard(ticket *t) {
    nx_text_value_guard g={0};g.user=t;g.token=1;g.retain=retain;g.release=release;g.current=current;return g;
}
static int lease_during_reserve(void) {
    memory mem={0};nx_context_desc desc={0};desc.allocator=(nx_allocator){&mem,allocate,deallocate};
    nx_context *ctx=NULL;model m=initial();nx_text_document_result out;ticket t={0};t.current=1;nx_text_value_guard g=guard(&t);
    CHECK(nx_context_create(&desc,&ctx)==NX_OK && frame(ctx,&m,0,0,&out)==0 && frame(ctx,&m,0,0,&out)==0);
    char payload[8192];memset(payload,'x',sizeof(payload));
    CHECK(nx_semantics_request_text_value_with_completion(ctx,2,(nx_utf8_span){payload,sizeof(payload)},&g,completed)==NX_OK);
    CHECK(begin(ctx,0,1)==0);mem.revoke_current=&t.current;nx_widget_desc w=widget(&m);nx_text_document_state s=snapshot(&m);
    CHECK(nx_text_document_register(ctx,&w,&s,apply,&m,&out)==NX_OK && !out.applied && !m.calls && m.length==7);
    CHECK(t.completions==1 && !t.applied && !t.refs);mem.revoke_current=NULL;
    CHECK(nx_end_frame(ctx)==NX_OK);nx_context_destroy(ctx);return 0;
}
static int native_completions(void) {
    nx_context *ctx=NULL;model m=initial();nx_text_document_result out;ticket t={0};t.current=1;nx_text_value_guard g=guard(&t);
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && frame(ctx,&m,0,0,&out)==0);
    CHECK(nx_semantics_request_text_range_with_completion(ctx,2,1,3,(nx_utf8_span){"z",1},&g,completed)==NX_OK && t.refs==1 && !t.completions);
    CHECK(frame(ctx,&m,0,0,&out)==0 && out.applied && m.calls==1 && t.completions==1 && t.applied==1 && !t.refs);
    CHECK(value(ctx,"Az\xf0\x9f\x8c\x8d",6)==0);
    CHECK(nx_semantics_request_text_selection_with_completion(ctx,2,0,2,&g,completed)==NX_OK);
    CHECK(frame(ctx,&m,0,0,&out)==0 && out.applied && m.anchor==0 && m.cursor==2 && t.completions==2 && t.applied==1 && !t.refs);
    CHECK(nx_semantics_request_text_clipboard_with_completion(ctx,2,0,2,0,&g,completed)==NX_ERROR_UNSUPPORTED && !t.refs);
    CHECK(nx_semantics_request_text_paste_with_completion(ctx,2,0,&g,completed)==NX_ERROR_UNSUPPORTED && !t.refs);
    CHECK(nx_semantics_request_text_value_with_completion(ctx,2,(nx_utf8_span){"late",4},&g,completed)==NX_OK);t.current=0;
    CHECK(frame(ctx,&m,0,0,&out)==0 && !out.applied && m.calls==2 && t.completions==3 && t.applied==0 && !t.refs);
    t.current=1;CHECK(frame(ctx,&m,0,0,&out)==0 && m.calls==2);
    CHECK(nx_semantics_request_text_value_with_completion(ctx,2,(nx_utf8_span){"no",2},&g,completed)==NX_OK);m.fail=NX_ERROR_MEMORY;
    CHECK(frame(ctx,&m,0,0,&out)==0 && !out.applied && m.calls==3 && t.completions==4 && !t.applied && !t.refs);
    m.fail=NX_OK;CHECK(nx_semantics_request_text_value_with_completion(ctx,2,(nx_utf8_span){"gone",4},&g,completed)==NX_OK);
    CHECK(frame(ctx,&m,0,1,&out)==0 && m.calls==3 && t.completions==5 && !t.applied && !t.refs);
    nx_context_destroy(ctx);return 0;
}
static int builtin_frame(nx_context *ctx,nx_text_field *field,nx_font_collection *fonts) {
    CHECK(begin(ctx,0,1)==0);nx_text_field_desc d={0};d.id=2;d.parent_id=1;d.bounds=(nx_rect){0,0,200,100};
    d.name=(nx_utf8_span){"Document",8};d.fonts=fonts;d.pixels_per_em=16;d.multiline=1;d.glyph_cache_bytes=65536;
    nx_interaction_state interaction;int valid,submitted;
    CHECK(nx_text_field_widget(ctx,&d,field,&interaction,&valid,&submitted)==NX_OK && nx_end_frame(ctx)==NX_OK);return 0;
}
static int builtin_domain(void) {
    uint8_t bytes[512];font_fixture(bytes);nx_font *font=NULL;nx_font_collection *fonts=NULL;
    nx_context *ctx=NULL;nx_text_field *field=NULL;model m=initial();nx_text_document_result out;
    ticket t={0};t.current=1;nx_text_value_guard g=guard(&t);
    CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font)==NX_OK && nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && nx_text_field_create("old",3,4096,&field)==NX_OK && builtin_frame(ctx,field,fonts)==0);
    nx_text_value_target target=nx_context_text_value_target(ctx,2);m.identity=target.identity;m.revision=target.revision;
    CHECK(!target.document_operations && target.multiline);
    CHECK(nx_semantics_request_text_value_with_completion(ctx,2,(nx_utf8_span){"late",4},&g,completed)==NX_OK);
    CHECK(frame(ctx,&m,0,0,&out)==0 && !m.calls && t.completions==1 && !t.applied && !t.refs);
    CHECK(nx_semantics_request_text_value_with_completion(ctx,2,(nx_utf8_span){"late",4},&g,completed)==NX_OK);
    CHECK(builtin_frame(ctx,field,fonts)==0 && t.completions==2 && !t.applied && !t.refs);
    nx_utf8_span value;CHECK(nx_text_field_text(field,&value)==NX_OK && value.length==3 && !memcmp(value.data,"old",3));
    nx_text_field_destroy(field);nx_context_destroy(ctx);nx_font_collection_destroy(fonts);nx_font_close(font);return 0;
}
#endif
int main(void) {
    CHECK(ordinary()==0 && stale_and_policy()==0 && failures()==0 && invalid()==0 && reserve_before_apply()==0 && large_snapshot()==0);
#if defined(__linux__)
    CHECK(native_completions()==0 && builtin_domain()==0 && lease_during_reserve()==0);
#endif
    puts("Application-owned text document: copied snapshots, deferred atomic callbacks, stale/policy/ancestor guards, allocation-before-mutation and large read-only state passed.");return 0;
}
