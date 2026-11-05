#include "ime_overlay.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}} while(0)
static int width(uint32_t c){return c==0x754c?2:c==0x301?0:1;}
static nx_result mark(void *user,uint32_t base,uint32_t scalar,unsigned index,float *x,float *y) {
    unsigned *calls=user;if(base!='e' || scalar!=0x301 || index)return NX_ERROR_ARGUMENT;
    ++*calls;*x=1;*y=-2;return NX_OK;
}
static unsigned char *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);if(n<=0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}fclose(f);*size=(size_t)n;return p;
}
static int render(aetheri_terminal_view *view,unsigned char *pixels) {
    const nx_command *commands=NULL;size_t count=0;
    return nx_frame_commands(aetheri_terminal_view_context(view),&commands,&count)==NX_OK && count &&
        nx_headless_render(commands,count,pixels,200,40,800,(nx_color){0,0,0,1})==NX_OK;
}
int main(int argc,char **argv) {
    CHECK(argc==2 || argc==3);size_t size=0;unsigned char *font_bytes=read_file(argv[1],&size);CHECK(font_bytes);
    nx_font *font=NULL;CHECK(nx_font_open(font_bytes,size,NULL,&font)==NX_OK);
    unsigned char *fallback_bytes=NULL;nx_font *fallback_font=NULL;nx_font_collection *fallback=NULL;
    if(argc==3) {
        size_t fallback_size=0;fallback_bytes=read_file(argv[2],&fallback_size);CHECK(fallback_bytes);
        CHECK(nx_font_open(fallback_bytes,fallback_size,NULL,&fallback_font)==NX_OK);
        CHECK(nx_font_collection_create(NULL,&fallback)==NX_OK && nx_font_collection_add(fallback,fallback_font)==NX_OK);
        uint16_t primary_glyph=0,fallback_glyph=0;
        CHECK(nx_font_glyph_index(font,0x754c,&primary_glyph)==NX_OK && !primary_glyph);
        CHECK(nx_font_glyph_index(fallback_font,0x754c,&fallback_glyph)==NX_OK && fallback_glyph);
    }
    struct ae_terminal *model=malloc(sizeof(*model));CHECK(model);ae_terminal_init(model,20,2,width);
    ae_terminal_feed(model,"base",4);unsigned mark_calls=0;
    aetheri_terminal_view_desc desc={.model=model,.regular=font,.bold=font,.text_identity=77,
        .cell_width=10,.cell_height=20,.baseline=15,.pixels_per_em=14,.mark_position=mark,.mark_user=&mark_calls,
        .regular_fallback=fallback,.bold_fallback=fallback};
    aetheri_terminal_view *view=NULL;CHECK(aetheri_terminal_view_create(&desc,&view)==NX_OK);
    terminal_ime_adapter ime={0};terminal_ime_guard guard={77,1,1};CHECK(terminal_ime_bind(&ime,guard)==NX_OK);
    terminal_ime_overlay overlay={.ime=&ime,.current=guard,.semantic_identity=78};
    unsigned char base[200*40*4],paint[200*40*4];
    CHECK(aetheri_terminal_view_frame(view,1,1)==NX_OK && render(view,base));
    nx_utf8_span before;size_t anchor,cursor;int has_caret;
    CHECK(aetheri_terminal_view_text(view,1,&before,&anchor,&cursor,&has_caret)==NX_OK && before.length<512);
    char original[512];size_t original_length=before.length;memcpy(original,before.data,before.length);
    const char draft[]="A\xe7\x95\x8c" "e\xcc\x81";
    CHECK(terminal_ime_preedit(&ime,guard,draft,sizeof(draft)-1,1,4)==NX_OK);
    nx_rect caret;CHECK(terminal_ime_overlay_caret(&overlay,&desc,&caret)==NX_OK && caret.x==50 && caret.y==0);
    aetheri_terminal_view_desc huge=desc;huge.cell_width=FLT_MAX;
    CHECK(terminal_ime_overlay_caret(&overlay,&huge,&caret)==NX_ERROR_LIMIT && caret.width==0);
    CHECK(aetheri_terminal_view_frame_with_overlay(view,1,1,terminal_ime_overlay_draw,&overlay)==NX_OK && render(view,paint));
    const char *image_path=getenv("NEXAUI_IME_OVERLAY_PPM");
    if(image_path) {
        FILE *image=fopen(image_path,"wb");CHECK(image && fprintf(image,"P6\n200 40\n255\n")>0);
        for(size_t i=0;i<200u*40u;++i)CHECK(fwrite(paint+i*4,1,3,image)==3);
        CHECK(fclose(image)==0);
    }
    CHECK(mark_calls==1 && memcmp(base,paint,sizeof(base))!=0);
    if(fallback) {
        const nx_font *selected=NULL;uint16_t glyph=0;
        CHECK(aetheri_terminal_resolve_glyph(&desc,0,0x754c,&selected,&glyph)==NX_OK && selected==fallback_font && glyph);
        CHECK(aetheri_terminal_resolve_glyph(&desc,0,'A',&selected,&glyph)==NX_OK && selected==font && glyph);
        CHECK(aetheri_terminal_resolve_glyph(&desc,1,0x754c,&selected,&glyph)==NX_OK && selected==fallback_font && glyph);
        aetheri_terminal_view_desc no_bold=desc;no_bold.bold=NULL;
        CHECK(aetheri_terminal_resolve_glyph(&no_bold,1,0x754c,&selected,&glyph)==NX_OK && selected==fallback_font && glyph);
        CHECK(aetheri_terminal_resolve_glyph(&desc,0,0x10ffff,&selected,&glyph)==NX_ERROR_UNSUPPORTED && !selected && !glyph);
        aetheri_terminal_view_desc no_fallback=desc;no_fallback.regular_fallback=NULL;no_fallback.bold_fallback=NULL;
        aetheri_terminal_view *legacy=NULL;unsigned char legacy_pixels[200*40*4];
        CHECK(aetheri_terminal_view_create(&no_fallback,&legacy)==NX_OK);
        CHECK(aetheri_terminal_view_frame_with_overlay(legacy,1,1,terminal_ime_overlay_draw,&overlay)==NX_OK && render(legacy,legacy_pixels));
        CHECK(memcmp(legacy_pixels,paint,sizeof(paint))!=0);
        aetheri_terminal_view_destroy(legacy);
    }
    CHECK(memcmp(paint+(1*200+65)*4,paint+(1*200+45)*4,4)!=0);
    /* The background after the four visible cells and caret is untouched. */
    CHECK(!memcmp(base+(5*200+160)*4,paint+(5*200+160)*4,4));
    nx_utf8_span after;CHECK(aetheri_terminal_view_text(view,1,&after,&anchor,&cursor,&has_caret)==NX_OK &&
        after.length==original_length && !memcmp(after.data,original,original_length));
    nx_semantic_view semantic;uint32_t index=0;nx_context *ctx=aetheri_terminal_view_context(view);
    CHECK(nx_semantics_find(ctx,2,&index)==NX_OK && nx_semantics_get(ctx,index,&semantic)==NX_OK &&
        semantic.value.length==original_length && !memcmp(semantic.value.data,original,original_length));
    CHECK(nx_semantics_find(ctx,3,&index)==NX_OK && nx_semantics_get(ctx,index,&semantic)==NX_OK &&
        semantic.state==NX_SEM_READONLY && semantic.value.length==sizeof(draft)-1 &&
        !memcmp(semantic.value.data,draft,sizeof(draft)-1));
    overlay.semantic_name=(nx_utf8_span){"Eingabeentwurf",14};
    CHECK(aetheri_terminal_view_frame_with_overlay(view,1,1,terminal_ime_overlay_draw,&overlay)==NX_OK);
    CHECK(nx_semantics_find(ctx,3,&index)==NX_OK && nx_semantics_get(ctx,index,&semantic)==NX_OK &&
          semantic.name.length==14 && !memcmp(semantic.name.data,"Eingabeentwurf",14));
    overlay.semantic_name=(nx_utf8_span){0};overlay.semantic_identity=0;
    CHECK(aetheri_terminal_view_frame_with_overlay(view,1,1,terminal_ime_overlay_draw,&overlay)==NX_OK &&
          nx_semantics_find(ctx,3,&index)==NX_ERROR_ARGUMENT);
    overlay.semantic_identity=77;
    CHECK(aetheri_terminal_view_frame_with_overlay(view,1,1,terminal_ime_overlay_draw,&overlay)==NX_ERROR_ARGUMENT);
    overlay.semantic_identity=78;
    aetheri_terminal_view_desc without_marks=desc;without_marks.mark_position=NULL;
    aetheri_terminal_view *unsupported=NULL;
    CHECK(aetheri_terminal_view_create(&without_marks,&unsupported)==NX_OK);
    CHECK(aetheri_terminal_view_frame_with_overlay(unsupported,1,1,terminal_ime_overlay_draw,&overlay)==NX_ERROR_UNSUPPORTED);
    aetheri_terminal_view_destroy(unsupported);
    if(fallback) {
        const char missing[]="\xf4\x8f\xbf\xbf";
        CHECK(terminal_ime_preedit(&ime,guard,missing,4,0,0)==NX_OK);
        CHECK(aetheri_terminal_view_frame_with_overlay(view,1,1,terminal_ime_overlay_draw,&overlay)==NX_ERROR_UNSUPPORTED);
        CHECK(terminal_ime_preedit(&ime,guard,draft,sizeof(draft)-1,1,4)==NX_OK);
    }
    CHECK(terminal_ime_preedit(&ime,guard,draft,sizeof(draft)-1,-1,-1)==NX_OK);
    CHECK(terminal_ime_overlay_caret(&overlay,&desc,&caret)==NX_OK && caret.x==80);
    CHECK(aetheri_terminal_view_frame_with_overlay(view,1,1.5f,terminal_ime_overlay_draw,&overlay)==NX_OK && render(view,paint));
    /* A long draft follows its cursor, and near the edge uses the whole row. */
    char long_draft[40];memset(long_draft,'x',sizeof(long_draft));
    CHECK(terminal_ime_preedit(&ime,guard,long_draft,sizeof(long_draft),40,40)==NX_OK);
    model->x=19;model->sequence++;
    CHECK(terminal_ime_overlay_caret(&overlay,&desc,&caret)==NX_OK && caret.x==190);
    CHECK(aetheri_terminal_view_frame_with_overlay(view,2,2,terminal_ime_overlay_draw,&overlay)==NX_OK && render(view,paint));
    CHECK(nx_semantics_find(ctx,3,&index)==NX_OK && nx_semantics_get(ctx,index,&semantic)==NX_OK &&
          semantic.value.length==sizeof(long_draft) && !memcmp(semantic.value.data,long_draft,sizeof(long_draft)));
    CHECK(terminal_ime_preedit(&ime,guard,NULL,0,0,0)==NX_OK);
    CHECK(aetheri_terminal_view_frame_with_overlay(view,2,1,terminal_ime_overlay_draw,&overlay)==NX_OK &&
          nx_semantics_find(ctx,3,&index)==NX_ERROR_ARGUMENT);
    CHECK(terminal_ime_preedit(&ime,guard,long_draft,sizeof(long_draft),40,40)==NX_OK);
    uint64_t last_revision=2;
    if(fallback) {
        ae_terminal_init(model,20,2,width);
        const char text[]="A\xe7\x95\x8c\x1b[1m\xe7\x95\x8c";
        ae_terminal_feed(model,text,sizeof(text)-1);
        CHECK(aetheri_terminal_view_frame(view,3,1)==NX_OK);
        const nx_cell_view *cells=NULL;size_t rows=0,columns=0;
        CHECK(aetheri_terminal_view_cells(view,3,&cells,&rows,&columns)==NX_OK && rows==2 && columns==20);
        CHECK(cells[0].glyphs[0].font==font && cells[1].glyphs[0].font==fallback_font &&
              cells[1].span==2 && cells[3].glyphs[0].font==fallback_font && cells[3].span==2);
        last_revision=3;
    }
    overlay.current.epoch++;
    CHECK(terminal_ime_overlay_caret(&overlay,&desc,&caret)==NX_ERROR_STATE);
    CHECK(aetheri_terminal_view_frame_with_overlay(view,last_revision,1,terminal_ime_overlay_draw,&overlay)==NX_ERROR_STATE);
    overlay.current=guard;terminal_ime_reset(&ime);
    CHECK(aetheri_terminal_view_frame_with_overlay(view,last_revision,1,terminal_ime_overlay_draw,&overlay)==NX_ERROR_STATE);
    aetheri_terminal_view_destroy(view);free(model);
    if(fallback)nx_font_collection_destroy(fallback);
    if(fallback_font)nx_font_close(fallback_font);
    free(fallback_bytes);
    nx_font_close(font);free(font_bytes);
    puts("Terminal preedit overlay, caret, visible range and unchanged semantic text passed");return 0;
}
