#include "presentation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"editor presentation %d: %s\n",__LINE__,#x);return 1;}} while(0)
static bool fail_realloc;
static size_t shared_groups;
void *__real_realloc(void *,size_t);
void *__wrap_realloc(void *p,size_t n) {return fail_realloc?NULL:__real_realloc(p,n);}
static int near(float a,float b) {return fabsf(a-b)<.002f;}
static int sample(nx_font_collection *fonts,const char *text,double em,nx_point origin,nx_rect clip) {
    nx_text_layout *layout=NULL;CHECK(nx_text_layout_create(fonts,text,strlen(text),NX_BIDI_LTR,em,10000000,&layout)==NX_OK);
    struct editor_presentation p={0};nx_text_document_presentation d;
    CHECK(editor_presentation_row(&p,layout,0,0,origin,clip)==NX_OK && editor_presentation_finish(&p,&d)==NX_OK);
    size_t characters=0,carets=0;
    for(size_t i=0;i<d.box_count;++i) {
        const nx_text_document_box *b=&d.boxes[i];
        CHECK(b->bounds.x>=clip.x && b->bounds.y>=clip.y && b->bounds.x+b->bounds.width<=clip.x+clip.width+.002f &&
            b->bounds.y+b->bounds.height<=clip.y+clip.height+.002f);
        if(b->start_byte==b->end_byte) {
            ++carets;bool found=false;
            for(size_t c=0;c<nx_text_layout_caret_count(layout);++c) {
                nx_text_caret caret;CHECK(nx_text_layout_caret_at(layout,c,&caret)==NX_OK);
                if(caret.byte_offset!=b->start_byte)continue;
                CHECK(near(b->bounds.x,fmaxf(clip.x,origin.x+(float)caret.x)) && near(b->bounds.y,fmaxf(clip.y,origin.y+(float)caret.top)));
                found=true;break;
            }
            CHECK(found);continue;
        }
        ++characters;CHECK(nx_text_layout_is_grapheme_boundary(layout,b->start_byte) && nx_text_layout_is_grapheme_boundary(layout,b->end_byte));
        size_t parts=0;for(size_t k=b->start_byte+1;k<=b->end_byte;++k)if(nx_text_layout_is_grapheme_boundary(layout,k))++parts;
        if(parts>1)++shared_groups;
        size_t count=0;nx_result r=nx_text_layout_selection(layout,b->start_byte,b->end_byte,NULL,0,&count);
        CHECK(r==NX_OK || r==NX_ERROR_LIMIT);
        if(!count) {CHECK(b->bounds.width==0);continue;}
        nx_text_selection_rect *rects=malloc(count*sizeof(*rects));CHECK(rects && nx_text_layout_selection(layout,b->start_byte,b->end_byte,rects,count,&count)==NX_OK);
        double left=INFINITY,top=INFINITY,right=-INFINITY,bottom=-INFINITY;
        for(size_t j=0;j<count;++j) {
            double x=fmax((double)clip.x,origin.x+rects[j].x),y=fmax((double)clip.y,origin.y+rects[j].y);
            double end=fmin((double)clip.x+clip.width,origin.x+rects[j].x+rects[j].width);
            double low=fmin((double)clip.y+clip.height,origin.y+rects[j].y+rects[j].height);
            if(end<=x || low<=y)continue;
            left=fmin(left,x);top=fmin(top,y);right=fmax(right,end);bottom=fmax(bottom,low);
        }
        free(rects);CHECK(isfinite(left) && near(b->bounds.x,(float)left) && near(b->bounds.y,(float)top) &&
            near(b->bounds.width,(float)(right-left)) && near(b->bounds.height,(float)(bottom-top)));
    }
    CHECK(characters && carets);
    /* The box-only oracle above must not accidentally accept omitted visible
       graphemes. Independently query each selection, including ligature parts. */
    for(size_t first=0;first<strlen(text);) {
        size_t end=0,count=0;CHECK(nx_grapheme_next(text,strlen(text),first,&end)==NX_OK && end>first);
        nx_result r=nx_text_layout_selection(layout,first,end,NULL,0,&count);CHECK(r==NX_OK || r==NX_ERROR_LIMIT);
        nx_text_selection_rect *rects=count?malloc(count*sizeof(*rects)):NULL;CHECK(!count || rects);
        CHECK(nx_text_layout_selection(layout,first,end,rects,count,&count)==NX_OK);
        for(size_t j=0;j<count;++j) {
            float left=(float)fmax(clip.x,origin.x+rects[j].x),top=(float)fmax(clip.y,origin.y+rects[j].y);
            float right=(float)fmin((double)clip.x+clip.width,origin.x+rects[j].x+rects[j].width);
            float bottom=(float)fmin((double)clip.y+clip.height,origin.y+rects[j].y+rects[j].height);
            if(right<=left || bottom<=top)continue;
            bool covered=false;
            for(size_t k=0;k<d.box_count;++k) {const nx_text_document_box *b=&d.boxes[k];
                if(b->start_byte<=first && b->end_byte>=end && b->start_byte<b->end_byte &&
                   b->bounds.x<=left+.002f && b->bounds.y<=top+.002f && b->bounds.x+b->bounds.width>=right-.002f &&
                   b->bounds.y+b->bounds.height>=bottom-.002f)covered=true;
            }
            CHECK(covered);
        }
        free(rects);first=end;
    }
    nx_text_document_box *copy=malloc(d.box_count*sizeof(*copy));CHECK(copy);size_t n=d.box_count;memcpy(copy,d.boxes,n*sizeof(*copy));
    editor_presentation_reset(&p);CHECK(editor_presentation_row(&p,layout,0,0,origin,clip)==NX_OK && editor_presentation_finish(&p,&d)==NX_OK && d.box_count==n);
    for(size_t i=0;i<n;++i)CHECK(copy[i].start_byte==d.boxes[i].start_byte && copy[i].end_byte==d.boxes[i].end_byte &&
        near(copy[i].bounds.x,d.boxes[i].bounds.x) && near(copy[i].bounds.width,d.boxes[i].bounds.width));
    free(copy);editor_presentation_close(&p);nx_text_layout_destroy(layout);return 0;
}
static int limits(nx_font_collection *fonts) {
    nx_text_layout *layout=NULL;CHECK(nx_text_layout_create(fonts,"Wii",3,NX_BIDI_LTR,20,10000,&layout)==NX_OK);
    nx_text_glyph w,i;CHECK(nx_text_layout_glyph_at(layout,0,&w)==NX_OK && nx_text_layout_glyph_at(layout,1,&i)==NX_OK && w.shape.advance_x>i.shape.advance_x);
    nx_rect clip={0,0,1000,1000};struct editor_presentation p={0};nx_text_document_presentation d;
    fail_realloc=true;CHECK(editor_presentation_row(&p,layout,0,0,(nx_point){0},clip)==NX_ERROR_MEMORY);fail_realloc=false;
    CHECK(!p.count && editor_presentation_finish(&p,&d)==NX_ERROR_MEMORY && !d.box_count);
    editor_presentation_reset(&p);p.limit=1;CHECK(editor_presentation_row(&p,layout,0,0,(nx_point){0},clip)==NX_ERROR_LIMIT && !p.count);
    CHECK(editor_presentation_finish(&p,&d)==NX_ERROR_LIMIT && !d.box_count);
    editor_presentation_reset(&p);p.limit=0;CHECK(editor_presentation_row(&p,layout,0,2,(nx_point){0},clip)==NX_OK);
    CHECK(editor_presentation_caret(&p,1,(nx_rect){50,5,1.5f,15},clip)==NX_OK && editor_presentation_finish(&p,&d)==NX_OK);
    bool changed=false,ending=false;
    for(size_t k=0;k<d.box_count;++k) {const nx_text_document_box *b=&d.boxes[k];if(b->start_byte==1 && b->end_byte==1) {CHECK(b->bounds.x==50);changed=true;}if(b->start_byte==3 && b->end_byte==5) {CHECK(b->bounds.width==0);ending=true;}}
    CHECK(changed && ending);editor_presentation_reset(&p);
    CHECK(editor_presentation_row(&p,layout,0,0,(nx_point){-1000,0},clip)==NX_OK && editor_presentation_finish(&p,&d)==NX_OK && !d.box_count);
    editor_presentation_close(&p);nx_text_layout_destroy(layout);return 0;
}
int main(int argc,char **argv) {
    CHECK(argc==2 || (argc==3 && !strcmp(argv[2],"--require-ligature")));FILE *file=fopen(argv[1],"rb");CHECK(file && !fseek(file,0,SEEK_END));long length=ftell(file);CHECK(length>0 && length<20000000 && !fseek(file,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)length);CHECK(bytes && fread(bytes,1,(size_t)length,file)==(size_t)length && !fclose(file));
    nx_font *font=NULL;nx_font_collection *fonts=NULL;CHECK(nx_font_open(bytes,(size_t)length,NULL,&font)==NX_OK && nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    const char *cases[]={"Wii office", ("a\xcc\x81" " b\xc3\xa9"), "abc \xd7\x90\xd7\x91\xd7\x92 def"};
    for(size_t c=0;c<sizeof(cases)/sizeof(*cases);++c)for(unsigned scale=1;scale<=2;++scale) {
        CHECK(!sample(fonts,cases[c],20.0*scale,(nx_point){20,10},(nx_rect){0,0,1000,1000}));
        CHECK(!sample(fonts,cases[c],20.0*scale,(nx_point){-9.25f,5.5f},(nx_rect){5,9,100,30}));
    }
    CHECK(!limits(fonts));if(argc==3)CHECK(shared_groups>0);
    printf("Real shared ligature groups checked: %zu\n",shared_groups);
    nx_font_collection_destroy(fonts);nx_font_close(font);free(bytes);
    puts("Editor presentation: real proportional/combining/bidi selection oracle, scroll/clip/scale, first caret, CRLF, reuse and atomic budget/allocation failure passed.");return 0;
}
