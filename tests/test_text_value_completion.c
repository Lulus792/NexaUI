#include "nexaui/nexaui.h"
#include "text_value.h"
#include "fixture_font.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

typedef struct ticket { int refs, calls, applied, current; } ticket;
typedef struct clipboard { ticket *ticket; nx_text_field *field; int mode, writes, reads; char data[64]; } clipboard;
static nx_result write_clipboard(void *user,const char *bytes,size_t length) {
    clipboard *c=user;if(length>=sizeof(c->data))return NX_ERROR_LIMIT;
    memcpy(c->data,bytes,length);c->data[length]=0;++c->writes;
    if(c->mode==1)c->ticket->current=0;
    if(c->mode==2 && nx_text_field_set_text(c->field,"replaced",8)!=NX_OK)return NX_ERROR_STATE;
    return c->mode==3?NX_ERROR_STATE:NX_OK;
}
static nx_result read_clipboard(void *user,char *bytes,size_t capacity,size_t *out) {
    clipboard *c=user;++c->reads;size_t n=strlen(c->data);*out=n;
    if(c->mode==7) {*out=NX_TEXT_VALUE_MAX_BYTES+1u;return NX_OK;}
    if((c->mode==4 && !bytes) || (c->mode==5 && bytes))
        if(nx_text_field_set_text(c->field,"replaced",8)!=NX_OK)return NX_ERROR_STATE;
    if(c->mode==6 && bytes)c->ticket->current=0;
    if(!bytes)return NX_OK;
    if(capacity<=n)return NX_ERROR_LIMIT;
    memcpy(bytes,c->data,n+1u);return NX_OK;
}
static void retain(void *p) { ++((ticket *)p)->refs; }
static void release(void *p) { --((ticket *)p)->refs; }
static int current(void *p, uint64_t token, uint64_t context, uint64_t id, nx_text_value_target target) {
    (void)token; (void)context; (void)id; (void)target;
    return ((ticket *)p)->current;
}
static void complete(void *p, int applied) {
    ticket *t = p;
    ++t->calls; t->applied = applied;
    /* The ticket must remain retained while completion records its result. */
    if (t->refs != 1) t->calls = -100;
}
static int frame(nx_context *ctx, nx_text_field *field, nx_text_field_desc *desc, int submit) {
    nx_interaction_state state; int valid, submitted;
    CHECK(nx_begin_frame(ctx, 300, 160, 0) == NX_OK);
    if (submit) CHECK(nx_text_field_widget(ctx, desc, field, &state, &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    return 0;
}
static int has_text(nx_text_field *field, const char *expected) {
    nx_utf8_span text;
    CHECK(nx_text_field_text(field, &text) == NX_OK);
    CHECK(text.length == strlen(expected) && !memcmp(text.data, expected, text.length));
    return 0;
}
int main(void) {
    uint8_t bytes[512]; font_fixture(bytes);
    nx_font *font = NULL; nx_font_collection *fonts = NULL;
    nx_context *ctx = NULL; nx_text_field *field = NULL;
    nx_text_field_desc d = {0}; ticket t = {0};
    nx_text_value_guard guard = {&t, 1, retain, release, current};
    CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && nx_text_field_create("old",3,4096,&field)==NX_OK);
    d.id=1; d.bounds=(nx_rect){0,0,160,28}; d.name=(nx_utf8_span){"Value",5};
    d.fonts=fonts; d.pixels_per_em=16; d.glyph_cache_bytes=65536;
    t.current=1; CHECK(frame(ctx,field,&d,1)==0);

    CHECK(nx_semantics_request_text_value_with_completion(ctx,1,(nx_utf8_span){"new",3},&guard,complete)==NX_OK);
    CHECK(t.refs==1 && t.calls==0 && has_text(field,"old")==0);
    CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK && t.calls==0);
    { nx_interaction_state state; int valid, submitted;
      CHECK(nx_text_field_widget(ctx,&d,field,&state,&valid,&submitted)==NX_OK); }
    CHECK(has_text(field,"new")==0 && t.calls==1 && t.applied==1 && t.refs==0);
    CHECK(nx_end_frame(ctx)==NX_OK);
    CHECK(t.refs==0 && t.calls==1 && t.applied==1);
    t.calls=0;
    CHECK(nx_semantics_request_text_value_with_completion(ctx,1,(nx_utf8_span){"new",3},&guard,complete)==NX_OK);
    CHECK(frame(ctx,field,&d,1)==0 && t.calls==1 && t.applied==1 && t.refs==0);
    t.calls=0;
    CHECK(nx_semantics_request_text_value_with_completion(ctx,1,(nx_utf8_span){"stale",5},&guard,complete)==NX_OK);
    t.current=0;
    CHECK(frame(ctx,field,&d,1)==0 && has_text(field,"new")==0 && t.calls==1 && t.applied==0 && t.refs==0);
    t.current=1; t.calls=0;
    CHECK(nx_semantics_request_text_value_with_completion(ctx,1,(nx_utf8_span){"omitted",7},&guard,complete)==NX_OK);
    CHECK(frame(ctx,field,&d,0)==0 && t.calls==1 && t.applied==0 && t.refs==0);
    CHECK(frame(ctx,field,&d,1)==0); t.calls=0;
    CHECK(nx_semantics_request_text_value_with_completion(ctx,1,(nx_utf8_span){"disabled",8},&guard,complete)==NX_OK);
    d.disabled=1;
    CHECK(frame(ctx,field,&d,1)==0 && t.calls==1 && t.applied==0 && t.refs==0);
    t.calls=0;
    CHECK(nx_semantics_request_text_value_with_completion(ctx,1,(nx_utf8_span){"rejected",8},&guard,complete)==NX_ERROR_STATE);
    CHECK(t.calls==0 && t.refs==0);
    d.disabled=0; CHECK(frame(ctx,field,&d,1)==0);
    clipboard clip={0};clip.ticket=&t;clip.field=field;
    d.clipboard=(nx_text_clipboard){&clip,read_clipboard,write_clipboard};
    CHECK(nx_text_field_set_text(field,"A\xc3\xa9Z",4)==NX_OK && frame(ctx,field,&d,1)==0);
    t.calls=0;CHECK(nx_semantics_request_text_clipboard_with_completion(ctx,1,1,3,0,&guard,complete)==NX_OK);
    CHECK(frame(ctx,field,&d,1)==0 && t.calls==1 && t.applied==1 && !strcmp(clip.data,"\xc3\xa9") && has_text(field,"A\xc3\xa9Z")==0);
    t.calls=0;CHECK(nx_semantics_request_text_clipboard_with_completion(ctx,1,1,3,1,&guard,complete)==NX_OK);
    CHECK(frame(ctx,field,&d,1)==0 && t.calls==1 && t.applied==1 && has_text(field,"AZ")==0);
    t.calls=0;clip.mode=3;CHECK(nx_semantics_request_text_clipboard_with_completion(ctx,1,0,1,1,&guard,complete)==NX_OK);
    CHECK(frame(ctx,field,&d,1)==0 && t.calls==1 && t.applied==0 && has_text(field,"AZ")==0);
    t.calls=0;clip.mode=1;CHECK(nx_semantics_request_text_clipboard_with_completion(ctx,1,0,1,1,&guard,complete)==NX_OK);
    CHECK(frame(ctx,field,&d,1)==0 && t.calls==1 && t.applied==0 && has_text(field,"AZ")==0);
    t.current=1;t.calls=0;clip.mode=2;
    CHECK(nx_semantics_request_text_clipboard_with_completion(ctx,1,0,1,1,&guard,complete)==NX_OK);
    CHECK(frame(ctx,field,&d,1)==0 && t.calls==1 && t.applied==0 && has_text(field,"replaced")==0);
    t.calls=0;clip.mode=0;strcpy(clip.data,"X\nY");
    CHECK(nx_text_field_set_text(field,"abc",3)==NX_OK && frame(ctx,field,&d,1)==0);
    CHECK(nx_semantics_request_text_paste_with_completion(ctx,1,1,&guard,complete)==NX_OK);
    CHECK(frame(ctx,field,&d,1)==0 && t.calls==1 && t.applied==1 && has_text(field,"aX Ybc")==0);
    for(int mode=4;mode<=6;++mode) {
        clip.mode=mode;t.current=1;t.calls=0;
        CHECK(nx_text_field_set_text(field,"abc",3)==NX_OK && frame(ctx,field,&d,1)==0);
        CHECK(nx_semantics_request_text_paste_with_completion(ctx,1,1,&guard,complete)==NX_OK);
        CHECK(frame(ctx,field,&d,1)==0 && t.calls==1 && t.applied==0 &&
              has_text(field,mode==6?"abc":"replaced")==0);
    }
    for(int mode=7;mode<=8;++mode) {
        clip.mode=mode;t.current=1;t.calls=0;
        if(mode==8){clip.data[0]=(char)0xc0;clip.data[1]=0;}
        CHECK(nx_text_field_set_text(field,"abc",3)==NX_OK && frame(ctx,field,&d,1)==0);
        CHECK(nx_semantics_request_text_paste_with_completion(ctx,1,1,&guard,complete)==NX_OK);
        CHECK(frame(ctx,field,&d,1)==0 && t.calls==1 && t.applied==0 && has_text(field,"abc")==0);
    }
    t.current=1;t.calls=0;clip.mode=0;
    CHECK(nx_semantics_request_text_value_with_completion(ctx,1,(nx_utf8_span){"destroy",7},&guard,complete)==NX_OK);
    nx_context_destroy(ctx);
    CHECK(t.calls==1 && t.applied==0 && t.refs==0);
    nx_text_field_destroy(field); nx_font_collection_destroy(fonts); nx_font_close(font);
    puts("text value completion and cancellation passed"); return 0;
}
