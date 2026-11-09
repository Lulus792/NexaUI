/* Included by test_text_document.c with the independent model/lease fixtures. */
typedef struct document_clipboard {
    char bytes[10000];size_t length;
    ticket *lease;unsigned reads,writes,validations;
    int revoke_write,revoke_read,revoke_validate,reject,malformed,mismatch;
    nx_result write_error,read_error;
} document_clipboard;
static nx_result document_copy(void *user,const char *bytes,size_t length) {
    document_clipboard *c=user;++c->writes;
    if(c->write_error)return c->write_error;
    if(length>sizeof(c->bytes))return NX_ERROR_LIMIT;
    memcpy(c->bytes,bytes,length);c->length=length;
    if(c->revoke_write)c->lease->current=0;
    return NX_OK;
}
static nx_result document_read(void *user,char *bytes,size_t capacity,size_t *length) {
    document_clipboard *c=user;++c->reads;*length=c->length;
    if(c->read_error)return c->read_error;
    if(bytes) {
        if(c->length>=capacity)return NX_ERROR_LIMIT;
        memcpy(bytes,c->bytes,c->length);bytes[c->length]=0;
        if(c->malformed==1 && c->length)bytes[0]=(char)0xff;
        if(c->malformed==2 && c->length)bytes[0]=0;
        if(c->mismatch)++*length;
    }
    if(c->revoke_read==(int)c->reads)c->lease->current=0;
    return NX_OK;
}
static int document_validate(nx_utf8_span current,size_t start,size_t end,nx_utf8_span insertion,void *user) {
    document_clipboard *c=user;++c->validations;
    if(c->revoke_validate)c->lease->current=0;
    return !c->reject && current.length>=start && start==end &&
        nx_utf8_validate(current.data,current.length,NULL)==NX_OK &&
        nx_utf8_validate(insertion.data,insertion.length,NULL)==NX_OK;
}
static nx_text_document_clipboard document_options(document_clipboard *c) {
    return (nx_text_document_clipboard){{c,document_read,document_copy},document_validate,c};
}
static int clipboard_frame(nx_context *ctx,model *m,document_clipboard *c,nx_text_document_result *out) {
    CHECK(begin(ctx,0,1)==0);nx_widget_desc w=widget(m);nx_text_document_state s=snapshot(m);
    nx_text_document_clipboard options=document_options(c);
    CHECK(nx_text_document_register_with_clipboard(ctx,&w,&s,apply,m,&options,out)==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK);return 0;
}
static void document_clipboard_enable(model *m) {
    m->operations|=NX_TEXT_DOCUMENT_COPY|NX_TEXT_DOCUMENT_CUT|NX_TEXT_DOCUMENT_PASTE;
}
static int clipboard_ordinary(void) {
    nx_context *ctx=NULL;model m=initial();ticket t={0};t.current=1;nx_text_value_guard g=guard(&t);
    document_clipboard c={0};c.lease=&t;document_clipboard_enable(&m);nx_text_document_result out;
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && clipboard_frame(ctx,&m,&c,&out)==0);
    CHECK(nx_semantics_request_text_clipboard_with_completion(ctx,2,1,3,0,&g,completed)==NX_OK);
    CHECK(clipboard_frame(ctx,&m,&c,&out)==0 && !out.applied && !m.calls && m.revision==1 && c.writes==1 &&
        c.length==2 && !memcmp(c.bytes,"\xc3\xa9",2) && t.completions==1 && t.applied==1 && !t.refs);
    CHECK(nx_semantics_request_text_clipboard_with_completion(ctx,2,1,3,1,&g,completed)==NX_OK);
    CHECK(clipboard_frame(ctx,&m,&c,&out)==0 && out.applied && m.calls==1 && m.revision==2 && t.applied==1 &&
        value(ctx,"A\xf0\x9f\x8c\x8d",5)==0 && m.anchor==1 && m.cursor==1);
    memcpy(c.bytes,"\xce\xa9\n",3);c.length=3;
    CHECK(nx_semantics_request_text_paste_with_completion(ctx,2,1,&g,completed)==NX_OK);
    CHECK(clipboard_frame(ctx,&m,&c,&out)==0 && out.applied && m.calls==2 && m.revision==3 && c.reads==2 &&
        c.validations==1 && t.completions==3 && t.applied==1 && !t.refs && value(ctx,"A\xce\xa9\n\xf0\x9f\x8c\x8d",8)==0);
    CHECK(clipboard_frame(ctx,&m,&c,&out)==0 && !out.applied && m.calls==2);
    c.length=0;CHECK(nx_semantics_request_text_paste_with_completion(ctx,2,0,&g,completed)==NX_OK);
    CHECK(clipboard_frame(ctx,&m,&c,&out)==0 && out.applied && m.calls==3 && m.revision==4 && m.length==8);
    nx_context_destroy(ctx);return 0;
}
static int clipboard_failures(void) {
    for(unsigned mode=0;mode<12;++mode) {
        nx_context *ctx=NULL;model m=initial();ticket t={0};t.current=1;nx_text_value_guard g=guard(&t);
        document_clipboard c={0};c.lease=&t;memcpy(c.bytes,"ok",2);c.length=2;
        document_clipboard_enable(&m);nx_text_document_result out;
        CHECK(nx_context_create(NULL,&ctx)==NX_OK && clipboard_frame(ctx,&m,&c,&out)==0);
        int cut=mode<3;
        if(cut)CHECK(nx_semantics_request_text_clipboard_with_completion(ctx,2,1,3,1,&g,completed)==NX_OK);
        else CHECK(nx_semantics_request_text_paste_with_completion(ctx,2,1,&g,completed)==NX_OK);
        switch(mode) {
        case 0:c.write_error=NX_ERROR_STATE;break;
        case 1:c.revoke_write=1;break;
        case 2:m.fail=NX_ERROR_MEMORY;break;
        case 3:c.read_error=NX_ERROR_STATE;break;
        case 4:c.revoke_read=1;break;
        case 5:c.revoke_read=2;break;
        case 6:c.reject=1;break;
        case 7:c.revoke_validate=1;break;
        case 8:c.malformed=1;break;
        case 9:c.malformed=2;break;
        case 10:c.mismatch=1;break;
        default:c.length=NX_TEXT_VALUE_MAX_BYTES+1u;break;
        }
        CHECK(clipboard_frame(ctx,&m,&c,&out)==0 && !out.applied && m.length==7 && m.revision==1 &&
            !memcmp(m.text,"A\xc3\xa9\xf0\x9f\x8c\x8d",7) && t.completions==1 && !t.applied && !t.refs);
        CHECK(m.calls==(mode==2?1u:0u));
        if(mode==1 || mode==2)CHECK(c.length==2 && !memcmp(c.bytes,"\xc3\xa9",2));
        nx_context_destroy(ctx);
    }
    return 0;
}
static int clipboard_policy(void) {
    nx_context *ctx=NULL;model m=initial();ticket t={0};t.current=1;nx_text_value_guard g=guard(&t);
    document_clipboard c={0};c.lease=&t;document_clipboard_enable(&m);nx_text_document_result out;
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && clipboard_frame(ctx,&m,&c,&out)==0);
    CHECK(nx_semantics_request_text_clipboard_with_completion(ctx,2,0,3,1,&g,completed)==NX_OK);++m.revision;
    CHECK(clipboard_frame(ctx,&m,&c,&out)==0 && !out.applied && !c.writes && !m.calls && t.completions==1 && !t.applied && !t.refs);
    m.operations=NX_TEXT_DOCUMENT_COPY;CHECK(clipboard_frame(ctx,&m,&c,&out)==0);
    CHECK(nx_semantics_request_text_paste_with_completion(ctx,2,0,&g,completed)==NX_ERROR_STATE);
    CHECK(nx_semantics_request_text_clipboard_with_completion(ctx,2,0,3,0,&g,completed)==NX_OK);
    CHECK(begin(ctx,0,1)==0);nx_widget_desc w=widget(&m);nx_text_document_state s=snapshot(&m);nx_text_document_clipboard options=document_options(&c);
    CHECK(nx_text_document_register_with_clipboard(ctx,&w,&s,NULL,NULL,&options,&out)==NX_OK &&
        nx_end_frame(ctx)==NX_OK && !out.applied && c.writes==1 && !m.calls && t.applied==1 && !t.refs);
    CHECK(begin(ctx,0,1)==0);options.clipboard.write=NULL;
    CHECK(nx_text_document_register_with_clipboard(ctx,&w,&s,NULL,NULL,&options,&out)==NX_ERROR_ARGUMENT);
    CHECK(nx_text_document_register(ctx,&w,&s,NULL,NULL,&out)==NX_ERROR_ARGUMENT);
    options=document_options(&c);CHECK(nx_text_document_register_with_clipboard(ctx,&w,&s,NULL,NULL,&options,&out)==NX_OK && nx_end_frame(ctx)==NX_OK);
    nx_context_destroy(ctx);return 0;
}
static int clipboard_storage(void) {
    for(unsigned mode=0;mode<2;++mode) {
        memory mem={0};nx_context_desc desc={0};desc.allocator=(nx_allocator){&mem,allocate,deallocate};
        nx_context *ctx=NULL;model m=initial();ticket t={0};t.current=1;nx_text_value_guard g=guard(&t);
        document_clipboard c={0};c.lease=&t;memset(c.bytes,'x',8192);c.length=8192;
        document_clipboard_enable(&m);nx_text_document_result out;
        CHECK(nx_context_create(&desc,&ctx)==NX_OK && clipboard_frame(ctx,&m,&c,&out)==0 && clipboard_frame(ctx,&m,&c,&out)==0);
        CHECK(nx_semantics_request_text_paste_with_completion(ctx,2,1,&g,completed)==NX_OK);
        CHECK(begin(ctx,0,1)==0);nx_widget_desc w=widget(&m);nx_text_document_state s=snapshot(&m);nx_text_document_clipboard options=document_options(&c);
        if(mode==0)mem.fail=1;else mem.revoke_current=&t.current;
        CHECK(nx_text_document_register_with_clipboard(ctx,&w,&s,apply,&m,&options,&out)==(mode==0?NX_ERROR_MEMORY:NX_OK));
        CHECK(!out.applied && !m.calls && m.length==7 && m.revision==1 && c.reads==2 && t.completions==1 && !t.applied && !t.refs);
        mem.fail=0;mem.revoke_current=NULL;
        if(mode==0)CHECK(nx_text_document_register_with_clipboard(ctx,&w,&s,apply,&m,&options,&out)==NX_OK);
        CHECK(nx_end_frame(ctx)==NX_OK && clipboard_frame(ctx,&m,&c,&out)==0 && !m.calls && c.reads==2);
        nx_context_destroy(ctx);
    }
    return 0;
}
static int clipboard_contract(void) {
    CHECK(clipboard_ordinary()==0 && clipboard_failures()==0 && clipboard_policy()==0 && clipboard_storage()==0);return 0;
}
