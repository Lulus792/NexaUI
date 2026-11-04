#include "nexaui/nexaui.h"
#include "text_value.h"
#include "fixture_font.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

typedef struct ticket { int refs, calls, applied, current; } ticket;
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
    CHECK(nx_semantics_request_text_value_with_completion(ctx,1,(nx_utf8_span){"destroy",7},&guard,complete)==NX_OK);
    nx_context_destroy(ctx);
    CHECK(t.calls==1 && t.applied==0 && t.refs==0);
    nx_text_field_destroy(field); nx_font_collection_destroy(fonts); nx_font_close(font);
    puts("text value completion and cancellation passed"); return 0;
}
