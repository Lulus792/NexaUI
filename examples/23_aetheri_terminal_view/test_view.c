#include "view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}} while(0)
static int cell_width(uint32_t c){return c==0x754c?2:c==0x301?0:1;}
static nx_result mark(void *user,uint32_t base,uint32_t scalar,unsigned index,float *x,float *y) {
    unsigned *calls=user;CHECK(base=='e' && scalar==0x301 && index==0);++*calls;*x=1.5f;*y=-2;return NX_OK;
}
static unsigned char *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);if(n<=0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}fclose(f);*size=(size_t)n;return p;
}
int main(int argc,char **argv) {
    CHECK(argc==2);size_t length=0;unsigned char *bytes=read_file(argv[1],&length);CHECK(bytes);
    nx_font *font=NULL;CHECK(nx_font_open(bytes,length,NULL,&font)==NX_OK);
    struct ae_terminal *model=malloc(sizeof(*model));CHECK(model);ae_terminal_init(model,8,3,cell_width);
    const char input[]="\x1b[31;44;4mA\x1b[0m\xe7\x95\x8c" "e\xcc\x81";
    ae_terminal_feed(model,input,sizeof(input)-1);
    unsigned mark_calls=0;aetheri_terminal_view *view=NULL;
    aetheri_terminal_view_desc desc={.model=model,.regular=font,.bold=font,.text_identity=123,
        .cell_width=12,.cell_height=22,.baseline=17,.pixels_per_em=16,.mark_position=mark,.mark_user=&mark_calls};
    CHECK(aetheri_terminal_view_create(&desc,&view)==NX_OK);
    CHECK(aetheri_terminal_view_frame(view,1,1)==NX_OK && mark_calls==1);
    const nx_cell_view *cells=NULL;size_t rows=0,cols=0;
    CHECK(aetheri_terminal_view_cells(view,1,&cells,&rows,&cols)==NX_OK && rows==3 && cols==8);
    CHECK(cells[0].flags&NX_CELL_UNDERLINE);CHECK(cells[1].span==2 && cells[2].span==0);
    CHECK(cells[3].glyph_count==2 && cells[3].glyphs[1].offset_x==1.5f && cells[3].glyphs[1].offset_y==-2);
    size_t row=9,col=9;CHECK(aetheri_terminal_view_hit(view,1,25,5,&row,&col)==NX_OK && row==0 && col==1);
    nx_utf8_span text;size_t anchor=0,cursor=0;int has_caret=0;
    CHECK(aetheri_terminal_view_text(view,1,&text,&anchor,&cursor,&has_caret)==NX_OK && has_caret && text.length>=7 &&
          !memcmp(text.data,"A\xe7\x95\x8c" "e\xcc\x81",7));
    nx_semantic_view sem;uint32_t index=0;nx_context *ctx=aetheri_terminal_view_context(view);
    CHECK(nx_semantics_find(ctx,2,&index)==NX_OK && nx_semantics_get(ctx,index,&sem)==NX_OK &&
          sem.value.length==text.length && !memcmp(sem.value.data,text.data,text.length));
    const nx_command *commands=NULL;size_t count=0;CHECK(nx_frame_commands(ctx,&commands,&count)==NX_OK && count);
    unsigned char pixels[96*66*4];CHECK(nx_headless_render(commands,count,pixels,96,66,96*4,(nx_color){0,0,0,1})==NX_OK);
    CHECK(ae_terminal_select_begin(model,0,1) && ae_terminal_select_extend(model,0,4));
    CHECK(aetheri_terminal_view_hit(view,1,25,5,&row,&col)==NX_ERROR_STATE);
    CHECK(aetheri_terminal_view_frame(view,1,1)==NX_ERROR_STATE);
    CHECK(aetheri_terminal_view_frame(view,2,1)==NX_OK);
    CHECK(aetheri_terminal_view_text(view,2,&text,&anchor,&cursor,&has_caret)==NX_OK && has_caret && anchor==1 && cursor==7);
    CHECK(aetheri_terminal_view_cells(view,2,&cells,&rows,&cols)==NX_OK &&
          cells[1].background.b==cells[2].background.b && cells[1].background.b==120.f/255.f &&
          cells[0].background.b==232.f/255.f);
    size_t selected=ae_terminal_selection_size(model);char copy[32];
    CHECK(selected==7 && ae_terminal_selection_copy(model,copy,sizeof(copy)) && !strcmp(copy,"\xe7\x95\x8c" "e\xcc\x81"));
    CHECK(ae_terminal_select_begin(model,0,4) && ae_terminal_select_extend(model,0,1));
    CHECK(aetheri_terminal_view_frame(view,3,1)==NX_OK);
    CHECK(aetheri_terminal_view_text(view,3,&text,&anchor,&cursor,&has_caret)==NX_OK && has_caret && anchor==7 && cursor==1);
    aetheri_terminal_view *unpositioned=NULL;desc.mark_position=NULL;
    CHECK(aetheri_terminal_view_create(&desc,&unpositioned)==NX_OK);
    CHECK(aetheri_terminal_view_frame(unpositioned,1,1)==NX_ERROR_UNSUPPORTED);
    aetheri_terminal_view_destroy(unpositioned);
    ae_terminal_select_clear(model);
    CHECK(aetheri_terminal_view_text(view,3,&text,&anchor,&cursor,&has_caret)==NX_ERROR_STATE);
    CHECK(aetheri_terminal_view_frame(view,3,1)==NX_ERROR_STATE);
    CHECK(aetheri_terminal_view_frame(view,4,1)==NX_OK);
    CHECK(ae_terminal_resize(model,6,2));CHECK(aetheri_terminal_view_frame(view,5,1.5f)==NX_OK);
    CHECK(aetheri_terminal_view_cells(view,5,&cells,&rows,&cols)==NX_OK && rows==2 && cols==6);
    ae_terminal_feed(model,"\r\n1\r\n2\r\n3",9);CHECK(model->history_count>0);
    ae_terminal_scroll(model,1);CHECK(aetheri_terminal_view_frame(view,6,1)==NX_OK);
    CHECK(aetheri_terminal_view_text(view,6,&text,&anchor,&cursor,&has_caret)==NX_OK && !has_caret && text.length>0 &&
          (unsigned char)text.data[0]==ae_terminal_row(model,0)[0].code);
    ae_terminal_feed(model,"\x1b[?1049hALT",11);CHECK(model->alt);
    CHECK(aetheri_terminal_view_frame(view,7,1)==NX_OK);
    CHECK(aetheri_terminal_view_text(view,7,&text,&anchor,&cursor,&has_caret)==NX_OK && has_caret && !memcmp(text.data,"ALT",3));
    ae_terminal_feed(model,"\x1b[?1049l",8);CHECK(!model->alt);
    CHECK(aetheri_terminal_view_frame(view,8,1)==NX_OK);
    aetheri_terminal_view *no_bold=NULL;desc.bold=NULL;desc.mark_position=mark;
    CHECK(aetheri_terminal_view_create(&desc,&no_bold)==NX_OK);
    ae_terminal_feed(model,"\x1b[1mB",5);
    CHECK(aetheri_terminal_view_frame(no_bold,1,1)==NX_ERROR_UNSUPPORTED);
    aetheri_terminal_view_destroy(no_bold);
    ae_terminal_init(model,240,100,cell_width);
    size_t many=240u*100u*4u;char *large=malloc(many);CHECK(large);
    for(size_t i=0;i<many;i+=4)memcpy(large+i,"\xf0\x9f\x98\x80",4);
    ae_terminal_feed(model,large,many);free(large);
    CHECK(aetheri_terminal_view_frame(view,9,1)==NX_OK);
    CHECK(aetheri_terminal_view_text(view,9,&text,&anchor,&cursor,&has_caret)==NX_OK && text.length>65536);
    CHECK(aetheri_terminal_view_cells(view,9,&cells,&rows,&cols)==NX_OK && rows==100 && cols==240);
    CHECK(nx_frame_commands(ctx,&commands,&count)==NX_OK && count);
    size_t stride=2880u*4u;unsigned char *large_pixels=malloc(stride*2200u);CHECK(large_pixels);
    nx_result rendered=nx_headless_render(commands,count,large_pixels,2880,2200,stride,(nx_color){0,0,0,1});
    free(large_pixels);CHECK(rendered==NX_OK);
    CHECK(aetheri_terminal_view_frame(view,8,1)==NX_ERROR_STATE);
    aetheri_terminal_view_destroy(view);free(model);nx_font_close(font);free(bytes);return 0;
}
