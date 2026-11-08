#include "nexaui/nexaui.h"
#include "fixture_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);}} while(0)
typedef struct heap {size_t calls,fail,live;} heap;
static void *allocate(void *user,size_t bytes) {heap *h=user;if(++h->calls==h->fail)return NULL;void *p=malloc(bytes);if(p)++h->live;return p;}
static void release(void *user,void *p,size_t bytes) {(void)bytes;heap *h=user;if(p){CHECK(h->live);--h->live;free(p);}}
typedef struct fixture {heap memory;nx_context *ctx;nx_text_field *field;nx_text_field_desc desc;} fixture;
static nx_result widget(fixture *f) {nx_interaction_state state;int valid,submitted;return nx_text_field_widget(f->ctx,&f->desc,f->field,&state,&valid,&submitted);}
static void frame(fixture *f) {CHECK(nx_begin_frame(f->ctx,240,80,0)==NX_OK);CHECK(widget(f)==NX_OK);CHECK(nx_end_frame(f->ctx)==NX_OK);}
static void queue(fixture *f,nx_event e) {CHECK(nx_context_queue_event(f->ctx,&e)==NX_OK);}
static void command(fixture *f,uint32_t action) {queue(f,(nx_event){.type=NX_EVENT_TEXT_COMMAND,.text_command=action});frame(f);}
static void expect(fixture *f,const char *text,size_t anchor,size_t cursor) {
    nx_utf8_span value;size_t a,c;CHECK(nx_text_field_text(f->field,&value)==NX_OK);
    CHECK(value.length==strlen(text) && !memcmp(value.data,text,value.length));
    CHECK(nx_text_field_selection(f->field,&a,&c)==NX_OK && a==anchor && c==cursor);
}
static uint64_t revision(fixture *f) {uint64_t r=0;CHECK(nx_text_field_host_ime_revision(f->ctx,1,f->field,&r)==NX_OK && r);return r;}
static void init(fixture *f,nx_font_collection *fonts) {
    memset(f,0,sizeof(*f));nx_context_desc context={0};context.allocator=(nx_allocator){&f->memory,allocate,release};
    CHECK(nx_context_create(&context,&f->ctx)==NX_OK && nx_text_field_create("ABCD",4,4096,&f->field)==NX_OK);
    CHECK(nx_text_field_set_host_ime(f->field,1)==NX_OK);
    f->desc=(nx_text_field_desc){.id=1,.name={"Atomic input",12},.bounds={0,0,220,40},.fonts=fonts,.pixels_per_em=16,.glyph_cache_bytes=65536};
    queue(f,(nx_event){.type=NX_EVENT_FOCUS,.focused=1});frame(f);CHECK(nx_focus_set(f->ctx,1)==NX_OK);frame(f);
    CHECK(nx_text_field_set_selection(f->field,4,4)==NX_OK);frame(f);
    queue(f,(nx_event){.type=NX_EVENT_TEXT,.scalar='!'});frame(f);
    CHECK(nx_text_field_set_selection(f->field,4,1)==NX_OK);frame(f);expect(f,"ABCD!",4,1);
}
static void destroy(fixture *f) {nx_text_field_destroy(f->field);nx_context_destroy(f->ctx);CHECK(!f->memory.live);}
static size_t packet(nx_event *e,uint64_t rev,uint32_t flags,int deletion,const uint32_t *text,size_t count) {
    size_t n=0;e[n++]=(nx_event){.type=NX_EVENT_IME_TRANSACTION_BEGIN,.ime_target=1,.ime_revision=rev,.ime_before=flags};
    if(deletion)e[n++]=(nx_event){.type=deletion==1?NX_EVENT_IME_DELETE_SURROUNDING:NX_EVENT_IME_DELETE_RANGE,
        .ime_target=1,.ime_revision=rev,.ime_before=1,.ime_after=1,.ime_index=2,.ime_length=1};
    for(size_t i=0;i<count;++i)e[n++]=(nx_event){.type=NX_EVENT_TEXT,.scalar=text[i],.ime_target=1,.ime_revision=rev,
        .ime_before=(i?0u:NX_IME_COMMIT_START)|(i+1u==count?NX_IME_COMMIT_END:0u)};
    e[n++]=(nx_event){.type=NX_EVENT_IME_TRANSACTION_END,.ime_target=1,.ime_revision=rev,.ime_before=flags};return n;
}
static void all(fixture *f,const nx_event *e,size_t n) {for(size_t i=0;i<n;++i)queue(f,e[i]);}
static void ordinary(nx_font_collection *fonts) {
    const uint32_t unicode[]={0xe9,0x3a9},same[]={'B','C','D'};
    for(int test=0;test<6;++test) {
        fixture f;init(&f,fonts);nx_event e[12];uint32_t flags=test==2?NX_IME_TRANSACTION_DELETE:NX_IME_TRANSACTION_COMMIT;
        int deletion=test==0 || test==2?1:test==3?2:0;if(deletion)flags|=NX_IME_TRANSACTION_DELETE;
        const uint32_t *text=test==4?same:unicode;size_t count=test==1 || test==2 || test==5?0:test==4?3:2;
        if(test==5){CHECK(nx_text_field_set_selection(f.field,5,5)==NX_OK);frame(&f);}
        size_t n=packet(e,revision(&f),flags,deletion,text,count);all(&f,e,n);frame(&f);
        if(test==0)expect(&f,"\xc3\xa9\xce\xa9",4,4);
        if(test==1)expect(&f,"A!",1,1);
        if(test==2)expect(&f,"",0,0);
        if(test==3)expect(&f,"ABC\xc3\xa9\xce\xa9!",7,7);
        if(test==4)expect(&f,"ABCD!",4,4);
        if(test==5)expect(&f,"ABCD!",5,5);
        command(&f,NX_TEXT_COMMAND_UNDO);
        if(test==5)expect(&f,"ABCD",4,4);else {
            expect(&f,"ABCD!",4,1);command(&f,NX_TEXT_COMMAND_UNDO);expect(&f,"ABCD",4,4);
            command(&f,NX_TEXT_COMMAND_REDO);command(&f,NX_TEXT_COMMAND_REDO);
            if(test==0)expect(&f,"\xc3\xa9\xce\xa9",4,4);
            if(test==4)expect(&f,"ABCD!",4,4);
        }
        destroy(&f);
    }
}
static void rejected(nx_font_collection *fonts) {
    for(int test=0;test<9;++test) {
        fixture f;init(&f,fonts);nx_event e[16];uint32_t text[]={'x','y'};uint64_t token=revision(&f);
        size_t n=packet(e,token,NX_IME_TRANSACTION_COMMIT|NX_IME_TRANSACTION_DELETE,1,text,2);
        if(test==0)e[0].ime_before=e[n-1].ime_before|=NX_IME_TRANSACTION_REJECTED;
        if(test==1)e[2].ime_before|=NX_IME_COMMIT_FILTERED;
        if(test==2)e[2].ime_target=2;
        if(test==3)e[3].ime_before=0;
        if(test==4)e[2].ime_before=0;
        if(test==5){memmove(e+3,e+2,(n-2)*sizeof(*e));e[2]=e[1];++n;}
        if(test==6){CHECK(nx_text_field_set_text(f.field,"A\xc3\xa9" "B",4)==NX_OK);CHECK(nx_text_field_set_selection(f.field,4,4)==NX_OK);frame(&f);n=packet(e,revision(&f),NX_IME_TRANSACTION_COMMIT|NX_IME_TRANSACTION_DELETE,1,text,2);e[1].ime_before=2;e[1].ime_after=0;}
        if(test==7){for(size_t i=0;i<n;++i)e[i].ime_revision=token+1u;}
        if(test==8){CHECK(nx_text_field_set_text(f.field,"replacement",11)==NX_OK);frame(&f);}
        all(&f,e,n);frame(&f);
        if(test==6)expect(&f,"A\xc3\xa9" "B",4,4);
        else if(test==8)expect(&f,"replacement",11,11);
        else {expect(&f,"ABCD!",4,1);command(&f,NX_TEXT_COMMAND_UNDO);expect(&f,"ABCD",4,4);}
        destroy(&f);
    }
}
static void framing(nx_font_collection *fonts) {
    for(int test=0;test<5;++test) {
        fixture f;init(&f,fonts);nx_event e[16];uint32_t text[]={'x','y'};
        size_t n=packet(e,revision(&f),NX_IME_TRANSACTION_COMMIT|NX_IME_TRANSACTION_DELETE,1,text,2);
        if(test==0){all(&f,e,3);frame(&f);expect(&f,"ABCD!",4,1);all(&f,e+3,n-3);}
        if(test==1){nx_event wrong=e[n-1];++wrong.ime_revision;all(&f,e,n-1);queue(&f,wrong);frame(&f);expect(&f,"ABCD!",4,1);queue(&f,e[3]);queue(&f,e[n-1]);}
        if(test==2){queue(&f,e[0]);all(&f,e,n);queue(&f,e[n-1]);}
        if(test==3){queue(&f,e[0]);queue(&f,e[1]);queue(&f,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_BACKSPACE});all(&f,e+2,n-2);}
        if(test==4){all(&f,e,2);queue(&f,(nx_event){.type=NX_EVENT_FOCUS,.focused=0});queue(&f,(nx_event){.type=NX_EVENT_FOCUS,.focused=1});all(&f,e+2,n-2);}
        frame(&f);
        if(test==3){expect(&f,"A!",1,1);command(&f,NX_TEXT_COMMAND_UNDO);expect(&f,"ABCD!",4,1);}
        else expect(&f,"ABCD!",4,1);
        n=packet(e,revision(&f),NX_IME_TRANSACTION_COMMIT|NX_IME_TRANSACTION_DELETE,1,text,2);all(&f,e,n);frame(&f);expect(&f,"xy",2,2);
        destroy(&f);
    }
}
static void sequence_and_policy(nx_font_collection *fonts) {
    fixture f;init(&f,fonts);CHECK(nx_text_field_set_selection(f.field,5,5)==NX_OK);frame(&f);
    uint64_t token=revision(&f);nx_event e[12];uint32_t x='x',y='y';size_t n=packet(e,token,NX_IME_TRANSACTION_COMMIT,0,&x,1);all(&f,e,n);
    n=packet(e,token,NX_IME_TRANSACTION_COMMIT,0,&y,1);all(&f,e,n);frame(&f);expect(&f,"ABCD!xy",7,7);
    all(&f,e,n);frame(&f);expect(&f,"ABCD!xy",7,7);
    command(&f,NX_TEXT_COMMAND_UNDO);expect(&f,"ABCD!x",6,6);command(&f,NX_TEXT_COMMAND_UNDO);expect(&f,"ABCD!",5,5);
    for(int mode=0;mode<2;++mode){
        token=revision(&f);n=packet(e,token,NX_IME_TRANSACTION_COMMIT,0,&x,1);
        if(mode==0)CHECK(nx_text_field_set_read_only(f.field,1)==NX_OK);else f.desc.disabled=1;
        all(&f,e,n);frame(&f);expect(&f,"ABCD!",5,5);
        if(mode==0)CHECK(nx_text_field_set_read_only(f.field,0)==NX_OK);else f.desc.disabled=0;
        frame(&f);CHECK(nx_focus_set(f.ctx,1)==NX_OK);frame(&f);all(&f,e,n);frame(&f);expect(&f,"ABCD!",5,5);
    }
    destroy(&f);
}
static void combining_and_multiline(nx_font_collection *fonts) {
    fixture f;init(&f,fonts);nx_event e[12];uint32_t marks[]={'e',0x301};
    CHECK(nx_text_field_set_text(f.field,"Ae\xcc\x81" "B",5)==NX_OK && nx_text_field_set_selection(f.field,4,1)==NX_OK);frame(&f);
    size_t n=packet(e,revision(&f),NX_IME_TRANSACTION_COMMIT,0,marks,2);all(&f,e,n);frame(&f);expect(&f,"Ae\xcc\x81" "B",4,4);
    command(&f,NX_TEXT_COMMAND_UNDO);expect(&f,"Ae\xcc\x81" "B",4,1);
    f.desc.multiline=1;CHECK(nx_text_field_set_text(f.field,"A\nBC\nD",6)==NX_OK && nx_text_field_set_selection(f.field,5,1)==NX_OK);frame(&f);
    n=packet(e,revision(&f),NX_IME_TRANSACTION_COMMIT,0,NULL,0);all(&f,e,n);frame(&f);expect(&f,"AD",1,1);
    command(&f,NX_TEXT_COMMAND_UNDO);expect(&f,"A\nBC\nD",5,1);destroy(&f);
}
static void paint_retry(nx_font_collection *fonts) {
    int observed=0;
    for(size_t offset=1;offset<=32 && !observed;++offset) {
        fixture f;init(&f,fonts);nx_event e[12];uint32_t x='x';size_t n=packet(e,revision(&f),NX_IME_TRANSACTION_COMMIT,0,&x,1);all(&f,e,n);
        CHECK(nx_begin_frame(f.ctx,240,80,0)==NX_OK);f.memory.fail=f.memory.calls+offset;
        nx_result result=widget(&f);f.memory.fail=0;
        nx_utf8_span value;CHECK(nx_text_field_text(f.field,&value)==NX_OK);
        if(result==NX_ERROR_MEMORY && value.length==3 && !memcmp(value.data,"Ax!",3)) {
            CHECK(widget(&f)==NX_OK);CHECK(nx_end_frame(f.ctx)==NX_OK);expect(&f,"Ax!",2,2);
            command(&f,NX_TEXT_COMMAND_UNDO);expect(&f,"ABCD!",4,1);observed=1;
        } else {CHECK(result==NX_OK || result==NX_ERROR_MEMORY);CHECK(nx_end_frame(f.ctx)==NX_OK);}
        destroy(&f);
    }
    CHECK(observed);
}
int main(void) {
    uint8_t bytes[512];font_fixture(bytes);nx_font *font=NULL;nx_font_collection *fonts=NULL;
    CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font)==NX_OK && nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    ordinary(fonts);rejected(fonts);framing(fonts);sequence_and_policy(fonts);combining_and_multiline(fonts);paint_retry(fonts);
    nx_font_collection_destroy(fonts);nx_font_close(font);
    puts("Atomic IME fields: Unicode/v1/v3/empty/identical/undo/rejection/framing/policy/retry passed");return 0;
}
