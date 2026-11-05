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
    CHECK(argc==2);size_t size=0;unsigned char *font_bytes=read_file(argv[1],&size);CHECK(font_bytes);
    nx_font *font=NULL;CHECK(nx_font_open(font_bytes,size,NULL,&font)==NX_OK);
    struct ae_terminal *model=malloc(sizeof(*model));CHECK(model);ae_terminal_init(model,20,2,width);
    ae_terminal_feed(model,"base",4);unsigned mark_calls=0;
    aetheri_terminal_view_desc desc={.model=model,.regular=font,.bold=font,.text_identity=77,
        .cell_width=10,.cell_height=20,.baseline=15,.pixels_per_em=14,.mark_position=mark,.mark_user=&mark_calls};
    aetheri_terminal_view *view=NULL;CHECK(aetheri_terminal_view_create(&desc,&view)==NX_OK);
    terminal_ime_adapter ime={0};terminal_ime_guard guard={77,1,1};CHECK(terminal_ime_bind(&ime,guard)==NX_OK);
    terminal_ime_overlay overlay={&ime,guard};
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
    CHECK(memcmp(paint+(1*200+65)*4,paint+(1*200+45)*4,4)!=0);
    /* The background after the four visible cells and caret is untouched. */
    CHECK(!memcmp(base+(5*200+160)*4,paint+(5*200+160)*4,4));
    nx_utf8_span after;CHECK(aetheri_terminal_view_text(view,1,&after,&anchor,&cursor,&has_caret)==NX_OK &&
        after.length==original_length && !memcmp(after.data,original,original_length));
    nx_semantic_view semantic;uint32_t index=0;nx_context *ctx=aetheri_terminal_view_context(view);
    CHECK(nx_semantics_find(ctx,2,&index)==NX_OK && nx_semantics_get(ctx,index,&semantic)==NX_OK &&
        semantic.value.length==original_length && !memcmp(semantic.value.data,original,original_length));
    aetheri_terminal_view_desc without_marks=desc;without_marks.mark_position=NULL;
    aetheri_terminal_view *unsupported=NULL;
    CHECK(aetheri_terminal_view_create(&without_marks,&unsupported)==NX_OK);
    CHECK(aetheri_terminal_view_frame_with_overlay(unsupported,1,1,terminal_ime_overlay_draw,&overlay)==NX_ERROR_UNSUPPORTED);
    aetheri_terminal_view_destroy(unsupported);
    CHECK(terminal_ime_preedit(&ime,guard,draft,sizeof(draft)-1,-1,-1)==NX_OK);
    CHECK(terminal_ime_overlay_caret(&overlay,&desc,&caret)==NX_OK && caret.x==80);
    CHECK(aetheri_terminal_view_frame_with_overlay(view,1,1.5f,terminal_ime_overlay_draw,&overlay)==NX_OK && render(view,paint));
    /* A long draft follows its cursor, and near the edge uses the whole row. */
    char long_draft[40];memset(long_draft,'x',sizeof(long_draft));
    CHECK(terminal_ime_preedit(&ime,guard,long_draft,sizeof(long_draft),40,40)==NX_OK);
    model->x=19;model->sequence++;
    CHECK(terminal_ime_overlay_caret(&overlay,&desc,&caret)==NX_OK && caret.x==190);
    CHECK(aetheri_terminal_view_frame_with_overlay(view,2,2,terminal_ime_overlay_draw,&overlay)==NX_OK && render(view,paint));
    overlay.current.epoch++;
    CHECK(terminal_ime_overlay_caret(&overlay,&desc,&caret)==NX_ERROR_STATE);
    CHECK(aetheri_terminal_view_frame_with_overlay(view,2,1,terminal_ime_overlay_draw,&overlay)==NX_ERROR_STATE);
    overlay.current=guard;terminal_ime_reset(&ime);
    CHECK(aetheri_terminal_view_frame_with_overlay(view,2,1,terminal_ime_overlay_draw,&overlay)==NX_ERROR_STATE);
    aetheri_terminal_view_destroy(view);free(model);nx_font_close(font);free(font_bytes);
    puts("Terminal preedit overlay, caret, visible range and unchanged semantic text passed");return 0;
}
