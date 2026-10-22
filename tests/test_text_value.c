#include "nexaui/nexaui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
#include "fixture_font.h"

static nx_text_field_desc descriptor(nx_font_collection *fonts, uint64_t id) {
    nx_text_field_desc d = {0};
    d.id=id; d.bounds=(nx_rect){0,0,160,28}; d.name=(nx_utf8_span){"Value",5};
    d.fonts=fonts; d.pixels_per_em=16; d.glyph_cache_bytes=65536;
    return d;
}
static int submit(nx_context *ctx, nx_text_field *f, nx_text_field_desc *d) {
    nx_interaction_state s; int valid,submitted;
    CHECK(nx_text_field_widget(ctx,d,f,&s,&valid,&submitted)==NX_OK);
    return 0;
}
static int frame(nx_context *ctx, nx_text_field *f, nx_text_field_desc *d) {
    CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK);
    CHECK(submit(ctx,f,d)==0);
    CHECK(nx_end_frame(ctx)==NX_OK);
    return 0;
}
static int text(nx_text_field *f,const char *s) {
    nx_utf8_span got;
    CHECK(nx_text_field_text(f,&got)==NX_OK);
    CHECK(got.length==strlen(s) && (!got.length || memcmp(got.data,s,got.length)==0));
    return 0;
}
static int command(nx_context *ctx,uint32_t c) {
    nx_event e={0}; e.type=NX_EVENT_TEXT_COMMAND; e.text_command=c;
    CHECK(nx_context_queue_event(ctx,&e)==NX_OK);return 0;
}
static int semantic(nx_context *ctx,uint64_t id,const char *s,uint32_t flags) {
    uint32_t index; nx_semantic_view v;
    CHECK(nx_semantics_find(ctx,id,&index)==NX_OK && nx_semantics_get(ctx,index,&v)==NX_OK);
    CHECK(v.value.length==strlen(s) && (!v.value.length || memcmp(v.value.data,s,v.value.length)==0));
    CHECK((v.state&flags)==flags);return 0;
}

static int value_and_undo(nx_font_collection *fonts) {
    nx_context *ctx=NULL; nx_text_field *f=NULL,*other=NULL;
    nx_text_field_desc d=descriptor(fonts,1),o=descriptor(fonts,2);
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && nx_text_field_create("10.42.0.1",9,4096,&f)==NX_OK);
    CHECK(nx_text_field_create("7443",4,4096,&other)==NX_OK);
    CHECK(frame(ctx,f,&d)==0 && nx_focus_set(ctx,1)==NX_OK);
    CHECK(command(ctx,NX_TEXT_COMMAND_DOCUMENT_END)==0);
    CHECK(command(ctx,NX_TEXT_COMMAND_MOVE_BACKWARD|NX_TEXT_COMMAND_EXTEND_SELECTION)==0);
    CHECK(frame(ctx,f,&d)==0);
    size_t a,c; CHECK(nx_text_field_selection(f,&a,&c)==NX_OK && a==9 && c==8);
    char value[]="\xe6\x9d\xb1\xe4\xba\xac\xf0\x9f\x8c\x8d";
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){value,sizeof(value)-1})==NX_OK);
    CHECK(text(f,"10.42.0.1")==0); /* Admission has no model effect. */
    value[0]='X'; /* The admitted payload must be owned. */
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"second",6})==NX_ERROR_LIMIT);
    CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"late",4})==NX_ERROR_STATE);
    CHECK(submit(ctx,f,&d)==0 && submit(ctx,other,&o)==0 && nx_end_frame(ctx)==NX_OK);
    CHECK(text(f,"\xe6\x9d\xb1\xe4\xba\xac\xf0\x9f\x8c\x8d")==0 && text(other,"7443")==0);
    CHECK(semantic(ctx,1,"\xe6\x9d\xb1\xe4\xba\xac\xf0\x9f\x8c\x8d",0)==0 && nx_focus_id(ctx)==1);
    CHECK(command(ctx,NX_TEXT_COMMAND_UNDO)==0 && frame(ctx,f,&d)==0);
    CHECK(text(f,"10.42.0.1")==0 && nx_text_field_selection(f,&a,&c)==NX_OK && a==9 && c==8);
    /* Equal-value requests preserve the reversed selection AND pending redo. */
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"10.42.0.1",9})==NX_OK && frame(ctx,f,&d)==0);
    CHECK(nx_text_field_selection(f,&a,&c)==NX_OK && a==9 && c==8);
    CHECK(command(ctx,NX_TEXT_COMMAND_REDO)==0 && frame(ctx,f,&d)==0);
    CHECK(text(f,"\xe6\x9d\xb1\xe4\xba\xac\xf0\x9f\x8c\x8d")==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){NULL,0})==NX_OK && frame(ctx,f,&d)==0);
    CHECK(text(f,"")==0 && semantic(ctx,1,"",0)==0);
    CHECK(command(ctx,NX_TEXT_COMMAND_UNDO)==0 && frame(ctx,f,&d)==0);
    CHECK(text(f,"\xe6\x9d\xb1\xe4\xba\xac\xf0\x9f\x8c\x8d")==0);
    nx_text_field_destroy(other);nx_text_field_destroy(f);nx_context_destroy(ctx);return 0;
}

typedef struct clipboard { unsigned copies; char bytes[32]; } clipboard;
static nx_result copy(void *p,const char *s,size_t n) {
    clipboard *c=p;if(n>=sizeof(c->bytes))return NX_ERROR_LIMIT;
    memcpy(c->bytes,s,n);c->bytes[n]=0;++c->copies;return NX_OK;
}
static int policy_and_identity(nx_font_collection *fonts) {
    nx_context *ctx=NULL;nx_text_field *f=NULL;
    nx_text_field_desc d=descriptor(fonts,1);clipboard clip={0};d.clipboard.user=&clip;d.clipboard.write=copy;
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && nx_text_field_create("abc",3,4096,&f)==NX_OK);
    CHECK(frame(ctx,f,&d)==0 && nx_focus_set(ctx,1)==NX_OK);
    CHECK(nx_text_field_set_read_only(NULL,1)==NX_ERROR_ARGUMENT && nx_text_field_set_read_only(f,2)==NX_ERROR_ARGUMENT);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"queued",6})==NX_OK);
    CHECK(nx_text_field_set_read_only(f,1)==NX_OK && frame(ctx,f,&d)==0);
    CHECK(text(f,"abc")==0 && semantic(ctx,1,"abc",NX_SEM_READONLY)==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"denied",6})==NX_ERROR_STATE);
    CHECK(command(ctx,NX_TEXT_COMMAND_SELECT_ALL)==0 && command(ctx,NX_TEXT_COMMAND_COPY)==0);
    CHECK(command(ctx,NX_TEXT_COMMAND_CUT)==0 && command(ctx,NX_TEXT_COMMAND_PASTE)==0);
    CHECK(command(ctx,NX_TEXT_COMMAND_DELETE_BACKWARD)==0 && command(ctx,NX_TEXT_COMMAND_NEWLINE)==0);
    CHECK(command(ctx,NX_TEXT_COMMAND_UNDO)==0 && command(ctx,NX_TEXT_COMMAND_REDO)==0);
    nx_event e={0};e.type=NX_EVENT_TEXT;e.scalar=0x1f30d;CHECK(nx_context_queue_event(ctx,&e)==NX_OK);
    e=(nx_event){0};e.type=NX_EVENT_KEY_DOWN;e.key=NX_KEY_BACKSPACE;CHECK(nx_context_queue_event(ctx,&e)==NX_OK);
    CHECK(frame(ctx,f,&d)==0 && text(f,"abc")==0 && clip.copies==1 && strcmp(clip.bytes,"abc")==0);
    size_t a,c;CHECK(nx_text_field_selection(f,&a,&c)==NX_OK && a==0 && c==3);
    CHECK(command(ctx,NX_TEXT_COMMAND_DOCUMENT_START)==0 && frame(ctx,f,&d)==0);
    CHECK(nx_text_field_selection(f,&a,&c)==NX_OK && a==0 && c==0);
    CHECK(nx_text_field_set_text(f,"app",3)==NX_OK && frame(ctx,f,&d)==0 && text(f,"app")==0);
    CHECK(nx_text_field_set_read_only(f,0)==NX_OK && frame(ctx,f,&d)==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"old-policy",10})==NX_OK);
    CHECK(nx_text_field_set_read_only(f,1)==NX_OK && nx_text_field_set_read_only(f,0)==NX_OK);
    CHECK(frame(ctx,f,&d)==0 && text(f,"app")==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"old-model",9})==NX_OK);
    CHECK(nx_text_field_set_text(f,"new-model",9)==NX_OK && frame(ctx,f,&d)==0 && text(f,"new-model")==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"stale",5})==NX_OK);
    nx_text_field_destroy(f);CHECK(nx_text_field_create("new-instance",12,4096,&f)==NX_OK);
    CHECK(frame(ctx,f,&d)==0 && text(f,"new-instance")==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"disabled",8})==NX_OK);
    d.disabled=1;CHECK(frame(ctx,f,&d)==0 && text(f,"new-instance")==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"no",2})==NX_ERROR_STATE);
    d.disabled=0;CHECK(frame(ctx,f,&d)==0 && text(f,"new-instance")==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"omitted",7})==NX_OK);
    CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK && nx_end_frame(ctx)==NX_OK);
    CHECK(frame(ctx,f,&d)==0 && text(f,"new-instance")==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"reparent",8})==NX_OK);
    CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK);
    nx_semantic_desc parent={0};parent.id=9;parent.role=NX_ROLE_PANEL;parent.bounds=(nx_rect){0,0,300,160};
    CHECK(nx_semantics_add(ctx,&parent,NULL)==NX_OK);d.parent_id=9;
    CHECK(submit(ctx,f,&d)==0 && nx_end_frame(ctx)==NX_OK && text(f,"new-instance")==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"ancestor-disabled",17})==NX_OK);
    CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK);parent.state=NX_SEM_DISABLED;
    CHECK(nx_semantics_add(ctx,&parent,NULL)==NX_OK && submit(ctx,f,&d)==0 && nx_end_frame(ctx)==NX_OK);
    CHECK(text(f,"new-instance")==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"denied",6})==NX_ERROR_STATE);
    CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK);parent.state=0;
    CHECK(nx_semantics_add(ctx,&parent,NULL)==NX_OK&&submit(ctx,f,&d)==0&&nx_end_frame(ctx)==NX_OK);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"region-disabled",15})==NX_OK);
    CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK);
    CHECK(nx_semantics_add(ctx,&parent,NULL)==NX_OK);
    nx_interaction_state parent_state;
    CHECK(nx_interact_region(ctx,parent.id,parent.bounds,NX_REGION_DISABLED,&parent_state)==NX_OK);
    CHECK(submit(ctx,f,&d)==0&&nx_end_frame(ctx)==NX_OK&&text(f,"new-instance")==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"denied",6})==NX_ERROR_STATE);
    nx_text_field_destroy(f);CHECK(nx_text_field_create_password("sensitive",9,&f)==NX_OK);
    d.parent_id=0;d.password=1;CHECK(frame(ctx,f,&d)==0 && semantic(ctx,1,"",NX_SEM_PROTECTED)==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"secret-write",12})==NX_ERROR_STATE);
    CHECK(text(f,"sensitive")==0);
    nx_text_field_destroy(f);nx_context_destroy(ctx);return 0;
}

static int modal_and_validation(nx_font_collection *fonts) {
    nx_context *ctx=NULL;nx_text_field *f=NULL;nx_text_field_desc d=descriptor(fonts,1);
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && nx_text_field_create("old",3,4096,&f)==NX_OK && frame(ctx,f,&d)==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"blocked",7})==NX_OK);
    CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK);
    nx_overlay_desc modal={0};modal.panel.id=10;modal.backdrop_id=11;
    modal.panel.name=(nx_utf8_span){"Modal",5};modal.panel.bounds=(nx_rect){50,30,120,80};int dismissed;
    CHECK(nx_modal_begin(ctx,&modal,&dismissed)==NX_OK && nx_overlay_end(ctx)==NX_OK);
    CHECK(submit(ctx,f,&d)==0 && nx_end_frame(ctx)==NX_OK && text(f,"old")==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"blocked",7})==NX_ERROR_STATE);
    CHECK(nx_overlay_close(ctx)==NX_OK && frame(ctx,f,&d)==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"A\0B",3})==NX_ERROR_ARGUMENT);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"\xed\xa0\x80",3})==NX_ERROR_ARGUMENT);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"line\nbreak",10})==NX_ERROR_ARGUMENT);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"",SIZE_MAX})==NX_ERROR_LIMIT);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){NULL,1})==NX_ERROR_ARGUMENT);
    CHECK(nx_semantics_request_text_value(ctx,999,(nx_utf8_span){"",0})==NX_ERROR_STATE);
    d.multiline=1;CHECK(frame(ctx,f,&d)==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"a\nb",3})==NX_OK && frame(ctx,f,&d)==0 && text(f,"a\nb")==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"single",6})==NX_OK);
    CHECK(nx_text_field_set_text(f,"old",3)==NX_OK);d.multiline=0;
    CHECK(frame(ctx,f,&d)==0 && text(f,"old")==0);
    nx_text_field_destroy(f);nx_context_destroy(ctx);return 0;
}

typedef struct tracker {
    size_t live,calls,fail_at,failed_size,payloads;
    nx_context *context;uint64_t reenter_id;nx_result nested_result;
} tracker;
static void *allocate(void *user,size_t n) {
    tracker *t=user;if(++t->calls==t->fail_at){t->failed_size=n;return NULL;}
    if(t->reenter_id) {
        uint64_t id=t->reenter_id;t->reenter_id=0;
        t->nested_result=nx_semantics_request_text_value(t->context,id,(nx_utf8_span){"nested",6});
    }
    void *p=malloc(n);if(p){++t->live;if(n==NX_TEXT_VALUE_MAX_BYTES+1u)++t->payloads;}return p;
}
static void deallocate(void *user,void *p,size_t n) {
    tracker *t=user;if(p){--t->live;if(n==NX_TEXT_VALUE_MAX_BYTES+1u)--t->payloads;free(p);}
}
static int bounds_and_allocation(nx_font_collection *fonts) {
    tracker t={0};nx_context_desc cd={0};cd.allocator=(nx_allocator){&t,allocate,deallocate};
    nx_context *ctx=NULL;nx_text_field *fields[65]={0};nx_text_field_desc d=descriptor(fonts,1);
    CHECK(nx_context_create(&cd,&ctx)==NX_OK);
    CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK);
    for(size_t i=0;i<65;++i){CHECK(nx_text_field_create("1",1,4096,&fields[i])==NX_OK);d.id=i+1;CHECK(submit(ctx,fields[i],&d)==0);}
    CHECK(nx_end_frame(ctx)==NX_OK);
    t.fail_at=t.calls+1;
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"x",1})==NX_ERROR_MEMORY);
    t.fail_at=0;
    char *large=malloc(NX_TEXT_VALUE_MAX_BYTES+1u);CHECK(large);memset(large,'1',NX_TEXT_VALUE_MAX_BYTES+1u);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){large,NX_TEXT_VALUE_MAX_BYTES+1u})==NX_ERROR_LIMIT);
    for(size_t i=0;i<64;++i)CHECK(nx_semantics_request_text_value(ctx,i+1,(nx_utf8_span){large,NX_TEXT_VALUE_MAX_BYTES})==NX_OK);
    CHECK(nx_semantics_request_text_value(ctx,65,(nx_utf8_span){"x",1})==NX_ERROR_LIMIT);
    size_t live=t.live;CHECK(t.payloads==64);CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK && nx_end_frame(ctx)==NX_OK);
    CHECK(t.payloads==0 && t.live<live); /* Payloads expire; begin_frame also frees paint bitmaps. */
    for(size_t i=0;i<65;++i){CHECK(text(fields[i],"1")==0);nx_text_field_destroy(fields[i]);}
    nx_context_destroy(ctx);CHECK(t.live==0);free(large);
    /* Warm both region buffers; force semantic reserve to fail before editing. */
    memset(&t,0,sizeof(t));CHECK(nx_context_create(&cd,&ctx)==NX_OK);
    nx_text_field *f=NULL;CHECK(nx_text_field_create("old",3,4096,&f)==NX_OK);d.id=1;
    CHECK(frame(ctx,f,&d)==0 && frame(ctx,f,&d)==0);
    char value[1024];memset(value,'1',sizeof(value));
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){value,sizeof(value)})==NX_OK);
    t.fail_at=t.calls+1;CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK);
    nx_interaction_state s;int valid,submitted;
    CHECK(nx_text_field_widget(ctx,&d,f,&s,&valid,&submitted)==NX_ERROR_MEMORY);
    CHECK(t.failed_size>=sizeof(value)+1 && text(f,"old")==0);t.fail_at=0;
    CHECK(nx_end_frame(ctx)==NX_OK && frame(ctx,f,&d)==0 && text(f,"old")==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"pending-destroy",15})==NX_OK);
    nx_context_destroy(ctx);CHECK(t.live==0);nx_text_field_destroy(f);return 0;
}

static int reentrant_admission(nx_font_collection *fonts) {
    for(unsigned duplicate=0;duplicate<2;++duplicate) {
        tracker t={0};nx_context_desc cd={0};cd.allocator=(nx_allocator){&t,allocate,deallocate};
        nx_context *ctx=NULL;nx_text_field *fields[65]={0};nx_text_field_desc d=descriptor(fonts,1);
        CHECK(nx_context_create(&cd,&ctx)==NX_OK);t.context=ctx;
        CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK);
        for(size_t i=0;i<65;++i){CHECK(nx_text_field_create("1",1,4096,&fields[i])==NX_OK);d.id=i+1;CHECK(submit(ctx,fields[i],&d)==0);}
        CHECK(nx_end_frame(ctx)==NX_OK);
        if(!duplicate)for(size_t i=1;i<=63;++i)CHECK(nx_semantics_request_text_value(ctx,i,(nx_utf8_span){"portable",8})==NX_OK);
        t.reenter_id=duplicate?64:65;
        nx_result outer=nx_semantics_request_text_value(ctx,64,(nx_utf8_span){"outer",5});
        fprintf(stderr,"Reentrant admission duplicate=%u outer=%d nested=%d\n",duplicate,outer,t.nested_result);
        CHECK(outer==NX_ERROR_LIMIT&&t.nested_result==NX_OK&&t.reenter_id==0);
        CHECK(nx_semantics_request_text_value(ctx,duplicate?64:65,(nx_utf8_span){"duplicate",9})==NX_ERROR_LIMIT);
        CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK);
        for(size_t i=0;i<65;++i){d.id=i+1;CHECK(submit(ctx,fields[i],&d)==0);}
        CHECK(nx_end_frame(ctx)==NX_OK);
        for(size_t i=0;i<65;++i)CHECK(text(fields[i],duplicate?(i==63?"nested":"1"):(i<63?"portable":i==64?"nested":"1"))==0);
        CHECK(nx_semantics_request_text_value(ctx,64,(nx_utf8_span){"retry",5})==NX_OK);
        CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK);d.id=64;CHECK(submit(ctx,fields[63],&d)==0);
        CHECK(nx_end_frame(ctx)==NX_OK&&text(fields[63],"retry")==0);
        for(size_t i=0;i<65;++i)nx_text_field_destroy(fields[i]);
        nx_context_destroy(ctx);CHECK(t.live==0);
    }
    return 0;
}

int main(void) {
    uint8_t bytes[512];font_fixture(bytes);nx_font *font=NULL;nx_font_collection *fonts=NULL;
    CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font)==NX_OK && nx_font_collection_create(NULL,&fonts)==NX_OK);
    CHECK(nx_font_collection_add(fonts,font)==NX_OK);
    if(reentrant_admission(fonts)||value_and_undo(fonts)||policy_and_identity(fonts)||modal_and_validation(fonts)||bounds_and_allocation(fonts))return 1;
    nx_font_collection_destroy(fonts);nx_font_close(font);
    puts("owned text-value admission, identity/policy/modal revalidation, bounds, undo and OOM passed");return 0;
}
