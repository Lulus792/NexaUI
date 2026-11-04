#include "nexaui/nexaui.h"
#include "text_value.h"
#include "fixture_font.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

static int submit(nx_context *ctx, nx_font_collection *fonts,
                  nx_text_field *ordinary, nx_text_field *secret) {
    nx_text_field_desc d = {0}; nx_interaction_state state; int valid, submitted;
    d.id=1; d.bounds=(nx_rect){0,0,140,28}; d.name=(nx_utf8_span){"Name",4};
    d.fonts=fonts; d.pixels_per_em=16; d.glyph_cache_bytes=65536;
    CHECK(nx_text_field_widget(ctx,&d,ordinary,&state,&valid,&submitted)==NX_OK);
    d.id=2; d.bounds.y=32; d.name=(nx_utf8_span){"Secret",6}; d.password=1;
    CHECK(nx_text_field_widget(ctx,&d,secret,&state,&valid,&submitted)==NX_OK);
    nx_semantic_desc fake={0}; fake.id=3; fake.role=NX_ROLE_TEXT_FIELD;
    fake.bounds=(nx_rect){0,64,140,28}; fake.name=(nx_utf8_span){"Custom",6};
    fake.value=(nx_utf8_span){"text",4};
    CHECK(nx_semantics_add(ctx,&fake,NULL)==NX_OK);
    return 0;
}
static int frame(nx_context *ctx, nx_font_collection *fonts,
                 nx_text_field *ordinary, nx_text_field *secret) {
    CHECK(nx_begin_frame(ctx,200,120,0)==NX_OK);
    CHECK(!nx_context_text_selection_snapshot(ctx,1).present);
    CHECK(submit(ctx,fonts,ordinary,secret)==0);
    CHECK(nx_end_frame(ctx)==NX_OK);
    return 0;
}
int main(void) {
    uint8_t bytes[512]; font_fixture(bytes);
    nx_font *font=NULL; nx_font_collection *fonts=NULL; nx_context *ctx=NULL;
    nx_text_field *ordinary=NULL, *secret=NULL;
    const char initial[]="A\xc3\xa9\xf0\x9f\x8c\x8d";
    CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    CHECK(nx_text_field_create(initial,sizeof(initial)-1,4096,&ordinary)==NX_OK);
    CHECK(nx_text_field_create_password("secret",6,&secret)==NX_OK);
    CHECK(!nx_context_text_selection_snapshot(NULL,1).present);
    CHECK(frame(ctx,fonts,ordinary,secret)==0);
    CHECK(nx_focus_set(ctx,1)==NX_OK);
    nx_event event={0}; event.type=NX_EVENT_TEXT_COMMAND;
    event.text_command=NX_TEXT_COMMAND_DOCUMENT_END;
    CHECK(nx_context_queue_event(ctx,&event)==NX_OK);
    event.text_command=NX_TEXT_COMMAND_MOVE_BACKWARD|NX_TEXT_COMMAND_EXTEND_SELECTION;
    CHECK(nx_context_queue_event(ctx,&event)==NX_OK && frame(ctx,fonts,ordinary,secret)==0);
    size_t anchor=0,cursor=0;
    CHECK(nx_text_field_selection(ordinary,&anchor,&cursor)==NX_OK);
    CHECK(anchor==7 && cursor==3); /* Scalar at 3 occupies four UTF-8 bytes. */
    nx_text_selection_snapshot old=nx_context_text_selection_snapshot(ctx,1);
    CHECK(old.present && old.anchor==anchor && old.cursor==cursor);
    uint32_t index=0; nx_semantic_view value;
    CHECK(nx_semantics_find(ctx,1,&index)==NX_OK && nx_semantics_get(ctx,index,&value)==NX_OK &&
          value.value.length==sizeof(initial)-1 && !memcmp(value.value.data,initial,sizeof(initial)-1));
    CHECK(!nx_context_text_selection_snapshot(ctx,2).present); /* Never publish secret offsets. */
    CHECK(!nx_context_text_selection_snapshot(ctx,3).present); /* A custom semantic role is not an editor. */
    CHECK(!nx_context_text_selection_snapshot(ctx,99).present);
    CHECK(nx_text_field_set_text(ordinary,"changed",7)==NX_OK);
    nx_text_selection_snapshot retained=nx_context_text_selection_snapshot(ctx,1);
    CHECK(retained.present && retained.anchor==old.anchor && retained.cursor==old.cursor);
    CHECK(nx_semantics_get(ctx,index,&value)==NX_OK && value.value.length==sizeof(initial)-1 &&
          !memcmp(value.value.data,initial,sizeof(initial)-1));
    CHECK(frame(ctx,fonts,ordinary,secret)==0 && nx_focus_set(ctx,1)==NX_OK);
    event=(nx_event){0}; event.type=NX_EVENT_TEXT_COMMAND; event.text_command=NX_TEXT_COMMAND_SELECT_ALL;
    CHECK(nx_context_queue_event(ctx,&event)==NX_OK && frame(ctx,fonts,ordinary,secret)==0);
    nx_text_selection_snapshot selected=nx_context_text_selection_snapshot(ctx,1);
    CHECK(selected.present && selected.anchor==0 && selected.cursor==7);
    CHECK(nx_semantics_find(ctx,1,&index)==NX_OK && nx_semantics_get(ctx,index,&value)==NX_OK &&
          value.value.length==7 && !memcmp(value.value.data,"changed",7));
    nx_text_field_destroy(ordinary); ordinary=NULL;
    retained=nx_context_text_selection_snapshot(ctx,1);
    CHECK(retained.present && retained.anchor==selected.anchor && retained.cursor==selected.cursor);
    nx_text_field_destroy(secret); nx_context_destroy(ctx);
    nx_font_collection_destroy(fonts); nx_font_close(font);
    puts("Owned semantic text selection snapshots passed.");
    return 0;
}
