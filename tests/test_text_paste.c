#include "nexaui/nexaui.h"
#include "fixture_font.h"
#if defined(__linux__)
#include "text_value.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"paste %d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct probe {
    const char *input;size_t length;unsigned calls;
    char before[256],insert[256];size_t start,end;int reject;
} probe;
static nx_result read_clip(void *p,char *bytes,size_t capacity,size_t *length) {
    probe *q=p;*length=q->length;if(!bytes)return NX_OK;
    if(capacity<=q->length)return NX_ERROR_LIMIT;
    memcpy(bytes,q->input,q->length);bytes[q->length]=0;return NX_OK;
}
static int admit(nx_utf8_span current,size_t start,size_t end,nx_utf8_span insertion,void *p) {
    probe *q=p;++q->calls;CHECK(current.length<sizeof(q->before) && insertion.length<sizeof(q->insert));
    memcpy(q->before,current.data,current.length);q->before[current.length]=0;
    if(insertion.length)memcpy(q->insert,insertion.data,insertion.length);
    q->insert[insertion.length]=0;q->start=start;q->end=end;
    return !q->reject;
}
static void frame(nx_context *c,nx_text_field *f,nx_text_field_desc *d) {
    nx_interaction_state state;int valid=0,submitted=0;
    CHECK(nx_begin_frame(c,320,120,0)==NX_OK);
    CHECK(nx_text_field_widget(c,d,f,&state,&valid,&submitted)==NX_OK);
    CHECK(nx_end_frame(c)==NX_OK);
}
static void text(nx_text_field *f,const char *s) {
    nx_utf8_span value;CHECK(nx_text_field_text(f,&value)==NX_OK);
    CHECK(value.length==strlen(s) && !memcmp(value.data,s,value.length));
}
static void selection(nx_text_field *f,size_t a,size_t c) {
    size_t aa=0,cc=0;CHECK(nx_text_field_selection(f,&aa,&cc)==NX_OK && aa==a && cc==c);
}
static void command(nx_context *c,nx_text_command action) {
    nx_event e={0};e.type=NX_EVENT_TEXT_COMMAND;e.text_command=action;CHECK(nx_context_queue_event(c,&e)==NX_OK);
}
#if defined(__linux__)
static unsigned refs,completions;static int last_applied;
static void retain(void *p){(void)p;++refs;}
static void release(void *p){(void)p;CHECK(refs);--refs;}
static int current(void *p,uint64_t token,uint64_t identity,uint64_t id,nx_text_value_target target){
    (void)p;(void)token;(void)identity;(void)id;(void)target;return 1;
}
static void complete(void *p,int applied){(void)p;++completions;last_applied=applied;}
#endif
static void paste(nx_context *c,unsigned route) {
    if(route==0){nx_event e={0};e.type=NX_EVENT_KEY_DOWN;e.key=NX_KEY_V;e.modifiers=NX_MOD_CONTROL;CHECK(nx_context_queue_event(c,&e)==NX_OK);}
    else if(route==1)command(c,NX_TEXT_COMMAND_PASTE);
#if defined(__linux__)
    else {nx_text_value_guard guard={0};guard.retain=retain;guard.release=release;guard.current=current;
        CHECK(nx_semantics_request_text_paste_with_completion(c,1,2,&guard,complete)==NX_OK);}
#endif
}
static void routes(nx_font_collection *fonts,unsigned route,int multiline) {
    nx_context *c=NULL;nx_text_field *f=NULL;probe p={"bad/",4,0,{0},{0},0,0,1};
    CHECK(nx_context_create(NULL,&c)==NX_OK && nx_text_field_create("ab",2,4096,&f)==NX_OK);
    nx_text_field_desc d={0};d.id=1;d.bounds=(nx_rect){0,0,260,90};d.name=(nx_utf8_span){"Name",4};
    d.fonts=fonts;d.pixels_per_em=16;d.glyph_cache_bytes=65536;d.multiline=multiline;
    d.clipboard=(nx_text_clipboard){&p,read_clip,NULL};frame(c,f,&d);CHECK(nx_focus_set(c,1)==NX_OK);
    nx_event e={0};e.type=NX_EVENT_TEXT;e.scalar='c';CHECK(nx_context_queue_event(c,&e)==NX_OK);frame(c,f,&d);text(f,"abc");
    CHECK(nx_text_field_set_selection(f,2,1)==NX_OK && nx_text_field_set_paste_validator(f,admit,&p)==NX_OK);frame(c,f,&d);
    paste(c,route);frame(c,f,&d);CHECK(p.calls==1 && !strcmp(p.before,"abc") && !strcmp(p.insert,"bad/"));
#if defined(__linux__)
    if(route==2)CHECK(!refs && completions && !last_applied);
#endif
    CHECK(p.start==(route==2?2u:1u) && p.end==2);text(f,"abc");selection(f,2,1);
    command(c,NX_TEXT_COMMAND_UNDO);frame(c,f,&d);text(f,"ab");
    /* Rejection also preserves a redo branch. */
    if(route!=2){paste(c,route);frame(c,f,&d);text(f,"ab");}
    command(c,NX_TEXT_COMMAND_REDO);frame(c,f,&d);text(f,"abc");
    CHECK(nx_text_field_set_selection(f,2,1)==NX_OK);frame(c,f,&d);
    p.reject=0;p.input="X\r\nY";p.length=4;paste(c,route);frame(c,f,&d);
#if defined(__linux__)
    if(route==2)CHECK(!refs && last_applied);
#endif
    CHECK(!strcmp(p.insert,multiline?"X\r\nY":"X Y"));
    text(f,route==2?(multiline?"abX\r\nYc":"abX Yc"):(multiline?"aX\r\nYc":"aX Yc"));
    command(c,NX_TEXT_COMMAND_UNDO);frame(c,f,&d);text(f,"abc");selection(f,2,1);
    CHECK(nx_text_field_set_paste_validator(f,NULL,&p)==NX_ERROR_ARGUMENT);
    CHECK(nx_text_field_set_paste_validator(NULL,admit,&p)==NX_ERROR_ARGUMENT);
    p.input="\xc3\xa9";p.length=2;paste(c,route);frame(c,f,&d);text(f,route==2?"ab\xc3\xa9" "c":"a\xc3\xa9" "c");
    command(c,NX_TEXT_COMMAND_UNDO);frame(c,f,&d);text(f,"abc");
    unsigned before=p.calls;p.input="\xc3";p.length=1;paste(c,route);frame(c,f,&d);text(f,"abc");CHECK(p.calls==before);
    p.input="a\0b";p.length=3;paste(c,route);frame(c,f,&d);text(f,"abc");CHECK(p.calls==before);
    CHECK(nx_text_field_set_paste_validator(f,NULL,NULL)==NX_OK);p.input="ok";p.length=2;
    frame(c,f,&d);paste(c,route);frame(c,f,&d);CHECK(p.calls==before);
    nx_text_field_destroy(f);nx_context_destroy(c);
}
#if defined(__linux__)
static void policy_freshness(nx_font_collection *fonts) {
    nx_context *c=NULL;nx_text_field *f=NULL;probe a={"x",1,0,{0},{0},0,0,0},b=a;
    CHECK(nx_context_create(NULL,&c)==NX_OK && nx_text_field_create("abc",3,4096,&f)==NX_OK);
    nx_text_field_desc d={0};d.id=1;d.bounds=(nx_rect){0,0,260,90};d.name=(nx_utf8_span){"Name",4};
    d.fonts=fonts;d.pixels_per_em=16;d.glyph_cache_bytes=65536;d.clipboard=(nx_text_clipboard){&a,read_clip,NULL};
    CHECK(nx_text_field_set_paste_validator(f,admit,&a)==NX_OK);frame(c,f,&d);
    paste(c,2);CHECK(nx_text_field_set_paste_validator(f,admit,&b)==NX_OK);frame(c,f,&d);text(f,"abc");CHECK(!a.calls && !b.calls);
    paste(c,2);CHECK(nx_text_field_set_paste_validator(f,admit,&b)==NX_OK);frame(c,f,&d);text(f,"abxc");CHECK(b.calls==1);
    paste(c,2);CHECK(nx_text_field_set_paste_validator(f,NULL,NULL)==NX_OK && nx_text_field_set_paste_validator(f,admit,&b)==NX_OK);
    frame(c,f,&d);text(f,"abxc");CHECK(b.calls==1);
    nx_text_field_destroy(f);nx_context_destroy(c);
}
#endif
int main(void) {
    uint8_t bytes[512];font_fixture(bytes);nx_font *font=NULL;nx_font_collection *fonts=NULL;
    CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    unsigned count=2;
#if defined(__linux__)
    count=3;policy_freshness(fonts);
#endif
    for(unsigned route=0;route<count;++route)for(int multiline=0;multiline<2;++multiline)routes(fonts,route,multiline);
    nx_font_collection_destroy(fonts);nx_font_close(font);
    puts("Paste admission:normalized whole-splice proposals, rejection/undo/redo/selection and policy freshness passed");return 0;
}
