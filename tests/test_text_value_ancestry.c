#include "nexaui/nexaui.h"
#include "fixture_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

static int frame(nx_context *ctx,nx_font_collection *fonts,nx_text_field *field,int kind) {
    CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK);
    uint64_t ids[]={100,200,kind==5?111u:110u,120};
    uint64_t parents[]={0,0,kind==3?200u:100u,kind==4?200u:ids[2]};
    for (size_t i=0;i<4;++i) {
        nx_semantic_desc a={0}; a.id=ids[i];a.parent_id=parents[i];
        a.role=((kind==1 && i==0)||(kind==2 && i==3))?NX_ROLE_CUSTOM:NX_ROLE_PANEL;
        a.name=(nx_utf8_span){"Ancestor",8};a.bounds=(nx_rect){0,0,300,160};
        CHECK(nx_semantics_add(ctx,&a,NULL)==NX_OK);
    }
    nx_text_field_desc d={0};d.id=1;d.parent_id=120;d.name=(nx_utf8_span){"Name",4};
    d.bounds=(nx_rect){0,0,160,28};d.fonts=fonts;d.pixels_per_em=16;d.glyph_cache_bytes=65536;
    nx_interaction_state state;int valid,submitted;
    CHECK(nx_text_field_widget(ctx,&d,field,&state,&valid,&submitted)==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK);return 0;
}
static int scenario(nx_font_collection *fonts,int kind) {
    nx_context *ctx=NULL;nx_text_field *field=NULL;nx_utf8_span actual;
    CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    CHECK(nx_text_field_create("old",3,4096,&field)==NX_OK);
    CHECK(frame(ctx,fonts,field,0)==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"new",3})==NX_OK);
    CHECK(frame(ctx,fonts,field,kind)==0);
    CHECK(nx_text_field_text(field,&actual)==NX_OK && actual.length==3);
    const char *expected=kind?"old":"new";
    if (memcmp(actual.data,expected,3)) {
        fprintf(stderr,"ancestor scenario %d: expected %s, got %.*s\n",kind,expected,(int)actual.length,actual.data);
        return 1;
    }
    if (kind) {
        CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"yes",3})==NX_OK);
        CHECK(frame(ctx,fonts,field,kind)==0);
        CHECK(nx_text_field_text(field,&actual)==NX_OK && actual.length==3 && !memcmp(actual.data,"yes",3));
    }
    nx_text_field_destroy(field);nx_context_destroy(ctx);return 0;
}
static int deep_frame(nx_context *ctx,nx_font_collection *fonts,nx_text_field *field,int changed) {
    CHECK(nx_begin_frame(ctx,300,160,0)==NX_OK);
    for (uint32_t i=0;i<NX_SEMANTIC_MAX_DEPTH-1u;++i) {
        nx_semantic_desc a={0};a.id=100u+i;a.parent_id=i?a.id-1u:0;
        a.role=changed && !i?NX_ROLE_CUSTOM:NX_ROLE_PANEL;
        a.name=(nx_utf8_span){"Ancestor",8};a.bounds=(nx_rect){0,0,300,160};
        CHECK(nx_semantics_add(ctx,&a,NULL)==NX_OK);
    }
    nx_text_field_desc d={0};d.id=1;d.parent_id=100u+NX_SEMANTIC_MAX_DEPTH-2u;
    d.name=(nx_utf8_span){"Name",4};d.bounds=(nx_rect){0,0,160,28};
    d.fonts=fonts;d.pixels_per_em=16;d.glyph_cache_bytes=65536;
    nx_interaction_state state;int valid,submitted;
    CHECK(nx_text_field_widget(ctx,&d,field,&state,&valid,&submitted)==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK);return 0;
}
static int deep_scenario(nx_font_collection *fonts,int changed) {
    nx_context *ctx=NULL;nx_text_field *field=NULL;nx_utf8_span actual;
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && nx_text_field_create("old",3,4096,&field)==NX_OK);
    CHECK(deep_frame(ctx,fonts,field,0)==0);
    CHECK(nx_semantics_request_text_value(ctx,1,(nx_utf8_span){"new",3})==NX_OK);
    CHECK(deep_frame(ctx,fonts,field,changed)==0);
    CHECK(nx_text_field_text(field,&actual)==NX_OK && actual.length==3 &&
          !memcmp(actual.data,changed?"old":"new",3));
    nx_text_field_destroy(field);nx_context_destroy(ctx);return 0;
}
int main(int argc,char **argv) {
    uint8_t bytes[512];font_fixture(bytes);nx_font *font=NULL;nx_font_collection *fonts=NULL;
    CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    if (argc==2) {
        char *end=NULL; long kind=strtol(argv[1],&end,10);
        CHECK(end && !*end && kind>=0 && kind<=5);
        CHECK(scenario(fonts,(int)kind)==0);
    } else {
        CHECK(argc==1);
        for(int kind=0;kind<=5;++kind) CHECK(scenario(fonts,kind)==0);
        CHECK(deep_scenario(fonts,0)==0 && deep_scenario(fonts,1)==0);
    }
    nx_font_collection_destroy(fonts);nx_font_close(font);
    puts("Text value ancestry revalidation passed.");return 0;
}
