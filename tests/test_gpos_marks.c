#include "nexaui/nexaui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
typedef struct fixture { uint8_t bytes[8192]; size_t used; } fixture;
static void p16(uint8_t *p,unsigned v) { p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static void p32(uint8_t *p,uint32_t v) { p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v; }
static void record(uint8_t *p,uint32_t tag,unsigned offset,unsigned size) { p32(p,tag); p32(p+8,offset); p32(p+12,size); }
static void init(fixture *f) {
    memset(f,0,sizeof(*f)); uint8_t *b=f->bytes;
    p32(b,0x10000); p16(b+4,8);
    record(b+12,NX_SFNT_TAG('h','e','a','d'),256,54);
    record(b+28,NX_SFNT_TAG('m','a','x','p'),320,32);
    record(b+44,NX_SFNT_TAG('h','h','e','a'),368,36);
    record(b+60,NX_SFNT_TAG('h','m','t','x'),416,64);
    record(b+76,NX_SFNT_TAG('c','m','a','p'),512,172);
    record(b+92,NX_SFNT_TAG('G','P','O','S'),1024,2048);
    record(b+108,NX_SFNT_TAG('X','S','U','B'),3072,0);
    record(b+124,NX_SFNT_TAG('X','D','E','F'),4096,0);
    p32(b+256,0x10000); p32(b+268,0x5f0f3cf5); p16(b+274,1000);
    p32(b+320,0x10000); p16(b+324,16);
    p32(b+368,0x10000); p16(b+372,900); p16(b+374,(unsigned)(uint16_t)-200); p16(b+402,16);
    for (unsigned i=0;i<16;++i) p16(b+416+4*i,(i==7 || i==8 || i==10)?200:600);
    uint8_t *c=b+512; p16(c+2,1); p16(c+4,3); p16(c+6,10); p32(c+8,12);
    c+=12; p16(c,12); p32(c+4,160); p32(c+12,12);
    const uint32_t chars[]={32,'A','B','f','i',0x301,0x302,0x307,0x5d0,0x915,0x93e,0xfb01,0xfffd};
    const unsigned gids[]={1,2,3,4,5,7,8,10,9,2,11,6,0};
    const unsigned mapping_count=(unsigned)(sizeof(chars)/sizeof(*chars));
    p32(b+88,12u+16u+mapping_count*12u); p32(c+4,16u+mapping_count*12u); p32(c+12,mapping_count);
    for (unsigned i=0;i<mapping_count;++i) { p32(c+16+12*i,chars[i]); p32(c+20+12*i,chars[i]); p32(c+24+12*i,gids[i]); }
    p16(b+416+4u*6u,900); p16(b+416+4u*13u,1200);
    uint8_t *g=b+1024;
    p32(g,0x10000); p16(g+4,10); p16(g+6,48); p16(g+8,128);
    p16(g+10,1); p32(g+12,NX_SFNT_TAG('D','F','L','T')); p16(g+16,8);
    p16(g+18,4); p16(g+24,0xffff); p16(g+26,1); p16(g+28,0);
    p16(g+48,1); p32(g+50,NX_SFNT_TAG('m','a','r','k')); p16(g+54,16);
    p16(g+66,1); p16(g+68,0);
    p16(g+128,1); p16(g+130,64);
    f->used=160;
}
static size_t anchor(uint8_t *g,size_t at,int x,int y) {
    p16(g+at,1); p16(g+at+2,(unsigned)(uint16_t)x); p16(g+at+4,(unsigned)(uint16_t)y); return at+6;
}
/* One mark-to-base lookup. The expected absolute attachment is (400,700),
   independent of glyph advances and the fallback's centering heuristic. */
static void base(fixture *f) {
    uint8_t *g=f->bytes+1024; size_t l=192,s=200;
    p16(g+l,4); p16(g+l+4,1); p16(g+l+6,8);
    p16(g+s,1); p16(g+s+2,12); p16(g+s+4,18); p16(g+s+6,1); p16(g+s+8,26); p16(g+s+10,38);
    p16(g+s+12,1); p16(g+s+14,1); p16(g+s+16,7);
    p16(g+s+18,1); p16(g+s+20,2); p16(g+s+22,2); p16(g+s+24,9);
    p16(g+s+26,1); p16(g+s+30,6); anchor(g,s+32,100,0);
    p16(g+s+38,2); p16(g+s+40,6); p16(g+s+42,12);
    anchor(g,s+44,400,700); f->used=anchor(g,s+50,400,700);
}
static size_t lookup(fixture *f,unsigned type,unsigned flags) {
    uint8_t *g=f->bytes+1024;
    unsigned count=(unsigned)((g[128]<<8)|g[129]);
    size_t at=(f->used+1u)&~(size_t)1u;
    p16(g+130+2u*count,(unsigned)(at-128u)); p16(g+128,count+1u);
    p16(g+66,count+1u); p16(g+68+2u*count,count);
    p16(g+at,type); p16(g+at+2,flags); p16(g+at+4,1);
    unsigned header=(flags&16u)?10u:8u;
    p16(g+at+6,header); f->used=at+header; return at;
}
static void mkmk(fixture *f,unsigned flags,unsigned set) {
    size_t l=lookup(f,6,flags),s=f->used; uint8_t *g=f->bytes+1024;
    if (flags&16u) p16(g+l+8,set);
    p16(g+s,1); p16(g+s+2,12); p16(g+s+4,18); p16(g+s+6,1); p16(g+s+8,24); p16(g+s+10,36);
    p16(g+s+12,1); p16(g+s+14,1); p16(g+s+16,8);
    p16(g+s+18,1); p16(g+s+20,1); p16(g+s+22,7);
    p16(g+s+24,1); p16(g+s+28,6); anchor(g,s+30,80,0);
    p16(g+s+36,1); p16(g+s+38,4); f->used=anchor(g,s+40,100,250);
}
static void single(fixture *f,unsigned glyph,int x,int y) {
    lookup(f,1,0); size_t s=f->used; uint8_t *g=f->bytes+1024;
    p16(g+s,1); p16(g+s+2,10); p16(g+s+4,3); p16(g+s+6,(unsigned)(uint16_t)x); p16(g+s+8,(unsigned)(uint16_t)y);
    p16(g+s+10,1); p16(g+s+12,1); p16(g+s+14,glyph); f->used=s+16;
}
static void gdef(fixture *f) {
    record(f->bytes+124,NX_SFNT_TAG('G','D','E','F'),4096,114);
    uint8_t *g=f->bytes+4096;
    p32(g,0x10002); p16(g+4,14); p16(g+10,52); p16(g+12,90);
    p16(g+14,1); p16(g+18,16);
    for (unsigned i=1;i<16;++i) p16(g+20+2*i,1);
    const unsigned ligatures[]={6,12,13},marks[]={7,8,10,11};
    for (unsigned i=0;i<3;++i) p16(g+20+2*ligatures[i],2);
    for (unsigned i=0;i<4;++i) p16(g+20+2*marks[i],3);
    p16(g+52,1); p16(g+56,16); p16(g+58+2*7,1); p16(g+58+2*8,2); p16(g+58+2*10,1);
    p16(g+90,1); p16(g+92,2); p32(g+94,12); p32(g+98,18);
    p16(g+102,1); p16(g+104,1); p16(g+106,7);
    p16(g+108,1); p16(g+110,1); p16(g+112,8);
}
static void pair(fixture *f) {
    lookup(f,2,8); uint8_t *g=f->bytes+1024; size_t s=f->used;
    p16(g+s,1); p16(g+s+2,18); p16(g+s+4,4); p16(g+s+8,1); p16(g+s+10,12);
    p16(g+s+12,1); p16(g+s+14,3); p16(g+s+16,(unsigned)(uint16_t)-50);
    p16(g+s+18,1); p16(g+s+20,1); p16(g+s+22,2); f->used=s+24;
}
static void ligature_position(fixture *f) {
    lookup(f,5,0); uint8_t *g=f->bytes+1024; size_t s=f->used;
    p16(g+s,1); p16(g+s+2,12); p16(g+s+4,20); p16(g+s+6,1); p16(g+s+8,28); p16(g+s+10,50);
    p16(g+s+12,1); p16(g+s+14,2); p16(g+s+16,7); p16(g+s+18,8);
    p16(g+s+20,1); p16(g+s+22,2); p16(g+s+24,6); p16(g+s+26,13);
    p16(g+s+28,2); p16(g+s+32,10); p16(g+s+36,16);
    anchor(g,s+38,100,0); anchor(g,s+44,80,0);
    size_t a=s+50; p16(g+a,2); p16(g+a+2,6); p16(g+a+4,24);
    size_t l=a+6; p16(g+l,2); p16(g+l+2,6); p16(g+l+4,12);
    anchor(g,l+6,150,700); anchor(g,l+12,450,700);
    l=a+24; p16(g+l,3); p16(g+l+2,8); p16(g+l+4,14); p16(g+l+6,20);
    anchor(g,l+8,150,700); anchor(g,l+14,450,700); f->used=anchor(g,l+20,750,700);
}
static void ligature_substitution(fixture *f,int nested) {
    uint8_t *g=f->bytes+3072;
    record(f->bytes+108,NX_SFNT_TAG('G','S','U','B'),3072,400);
    p32(g,0x10000); p16(g+4,10); p16(g+6,48); p16(g+8,128);
    p16(g+10,1); p32(g+12,NX_SFNT_TAG('D','F','L','T')); p16(g+16,8);
    p16(g+18,4); p16(g+24,0xffff); p16(g+26,1); p16(g+28,0);
    p16(g+48,1); p32(g+50,NX_SFNT_TAG('l','i','g','a')); p16(g+54,16);
    p16(g+66,nested?2:1); p16(g+68,0); p16(g+70,1);
    p16(g+128,nested?2:1); p16(g+130,64); p16(g+132,172);
    size_t l=192,s=200; p16(g+l,4); p16(g+l+2,8); p16(g+l+4,1); p16(g+l+6,8);
    p16(g+s,1); p16(g+s+2,26); p16(g+s+4,1); p16(g+s+6,8);
    p16(g+s+8,2); p16(g+s+10,6); p16(g+s+12,12);
    p16(g+s+14,12); p16(g+s+16,2); p16(g+s+18,4); /* ff */
    p16(g+s+20,6); p16(g+s+22,2); p16(g+s+24,5); /* fi */
    p16(g+s+26,1); p16(g+s+28,1); p16(g+s+30,4);
    if (nested) {
        l=300; s=308; p16(g+l,4); p16(g+l+2,8); p16(g+l+4,1); p16(g+l+6,8);
        p16(g+s,1); p16(g+s+2,18); p16(g+s+4,1); p16(g+s+6,8);
        p16(g+s+8,1); p16(g+s+10,4); p16(g+s+12,13); p16(g+s+14,2); p16(g+s+16,5);
        p16(g+s+18,1); p16(g+s+20,1); p16(g+s+22,12);
    }
}
static nx_result shape(fixture *f,const char *text,nx_bidi_direction direction,double ppem,
                        nx_shaped_glyph *glyphs,size_t capacity,size_t *count) {
    nx_font *font=NULL; nx_font_collection *fonts=NULL;
    nx_result result=nx_font_open(f->bytes,sizeof(f->bytes),NULL,&font);
    if (result==NX_OK) result=nx_font_collection_create(NULL,&fonts);
    if (result==NX_OK) result=nx_font_collection_add(fonts,font);
    nx_shape_run runs[32]; size_t run_count=0; uint8_t level=0;
    if (result==NX_OK) result=nx_shape_text(fonts,text,strlen(text),direction,ppem,glyphs,capacity,runs,32,count,&run_count,&level);
    nx_font_collection_destroy(fonts); nx_font_close(font); return result;
}
static int close_to(double a,double b) { return fabs(a-b)<0.0000001; }
static int table_edges(void) {
    fixture f; nx_shaped_glyph glyphs[16],unchanged[16]; size_t count=0;
    init(&f); base(&f);
    /* Coverage format 2, with the same single mark, lives after the anchors. */
    uint8_t *g=f.bytes+1024; size_t cov=f.used;
    p16(g+202,(unsigned)(cov-200)); p16(g+cov,2); p16(g+cov+2,1);
    p16(g+cov+4,7); p16(g+cov+6,7); p16(g+cov+8,0); f.used=cov+10;
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK);
    CHECK(close_to(glyphs[1].offset_y,14));
    p16(g+cov+8,1); CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_ERROR_ARGUMENT);
    init(&f); base(&f); gdef(&f); mkmk(&f,0x100,0);
    /* GDEF class format 2: base A and the mark range. */
    uint8_t *d=f.bytes+4096; p16(d+14,2); p16(d+16,2);
    p16(d+18,2); p16(d+20,2); p16(d+22,1);
    p16(d+24,7); p16(d+26,8); p16(d+28,3);
    CHECK(shape(&f,"A\xcc\x81\xcc\x82",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK);
    CHECK(close_to(glyphs[2].offset_y,19));
    p16(d+24,2); CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_ERROR_ARGUMENT);
    init(&f); base(&f);
    /* Move base anchor to allow the longer format 2/3 records. */
    p16(g+240,100); anchor(g,338,400,700); p16(g+338,2); p16(g+344,65535);
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && close_to(glyphs[1].offset_y,14));
    p16(g+338,3); p16(g+344,10); p16(g+346,18);
    p16(g+348,20); p16(g+350,20); p16(g+352,1); p16(g+354,0x4000); /* +1 pixel */
    p16(g+356,20); p16(g+358,20); p16(g+360,1); p16(g+362,0xc000); /* -1 pixel */
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK);
    CHECK(close_to(glyphs[1].offset_x,-5) && close_to(glyphs[1].offset_y,13));
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20.5,glyphs,16,&count)==NX_OK);
    CHECK(close_to(glyphs[1].offset_x,-6.15) && close_to(glyphs[1].offset_y,14.35));
    p16(g+346,0); p16(g+352,2); p16(g+354,0x8000); /* Signed 4-bit -8. */
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && close_to(glyphs[1].offset_x,-14));
    p16(g+352,3); /* Signed 8-bit -128. */
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && close_to(glyphs[1].offset_x,-134));
    p16(g+348,16); p16(g+350,32); p16(g+352,1); p16(g+358,0xc000);
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,32,glyphs,16,&count)==NX_OK && close_to(glyphs[1].offset_x,-10.6));
    p16(g+348,20); p16(g+350,20); p16(g+346,18);
    p16(g+356,20); p16(g+358,20);
    p16(g+352,0x8000); p16(g+360,0x8000);
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && close_to(glyphs[1].offset_y,14));
    p16(g+352,1); p16(g+348,21);
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_ERROR_ARGUMENT);
    init(&f); base(&f); p16(g+240,0); /* Optional absent parent anchor. */
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && close_to(glyphs[1].offset_y,0));
    p16(g+230,0); /* A source mark anchor is required. */
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_ERROR_ARGUMENT);
    init(&f); base(&f);
    p16(g+192,9); p16(g+198,200); /* Extension subtable at 392. */
    p16(g+392,1); p16(g+394,4); p32(g+396,8);
    memcpy(g+400,g+200,56);
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && close_to(glyphs[1].offset_y,14));
    p16(g+394,9); CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_ERROR_ARGUMENT);
    p16(g+394,4); p32(g+396,UINT32_MAX);
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_ERROR_ARGUMENT);
    init(&f); base(&f); gdef(&f); ligature_position(&f); ligature_substitution(&f,0);
    uint8_t *s=f.bytes+3072;
    p16(s+192,7); p16(s+198,200); p16(s+392,1); p16(s+394,4); p32(s+396,8);
    memcpy(s+400,s+200,32); p32(f.bytes+120,432);
    CHECK(shape(&f,"f\xcc\x81i",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && count==2 && glyphs[0].glyph_index==6);
    CHECK(close_to(glyphs[1].offset_x,-17));
    init(&f); base(&f); gdef(&f); ligature_substitution(&f,0);
    /* Real fonts also use one-component Ligature tables as substitutions.
       A mark stays nonspacing and keeps its attachment provenance. */
    s=f.bytes+3072; p16(s+208,1); p16(s+214,10); p16(s+216,1); p16(s+230,7);
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && count==2);
    CHECK(glyphs[1].glyph_index==10 && glyphs[1].advance_x==0);
    p16(s+216,0);
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_ERROR_ARGUMENT);
    init(&f); base(&f);
    for (size_t n=0;n<f.used;++n) {
        p32(f.bytes+104,(uint32_t)n); memset(glyphs,0x5a,sizeof(glyphs)); memcpy(unchanged,glyphs,sizeof(glyphs));
        CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)!=NX_OK);
        CHECK(memcmp(glyphs,unchanged,sizeof(glyphs))==0);
    }
    return 0;
}
static int stress(void) {
    fixture f; size_t count=0,n=10000;
    char *text=malloc(n*6+1); nx_shaped_glyph *glyphs=calloc(n*4,sizeof(*glyphs));
    CHECK(text && glyphs);
    init(&f); base(&f); gdef(&f); ligature_position(&f); ligature_substitution(&f,0);
    for (size_t i=0;i<n;++i) memcpy(text+6*i,"f\xcc\x81i\xcc\x82",6);
    text[n*6]=0;
    CHECK(shape(&f,text,NX_BIDI_LTR,20,glyphs,n*4,&count)==NX_OK && count==n*3);
    for (size_t i=0;i<n;++i) CHECK(glyphs[3*i].glyph_index==6 && close_to(glyphs[3*i+1].offset_x,-17) && close_to(glyphs[3*i+2].offset_x,-10.6));
    init(&f); base(&f); mkmk(&f,0,0); text[0]='A';
    p16(f.bytes+1024+280,7); /* Repeated source/parent glyph makes a deep attachment chain. */
    for (size_t i=0;i<n;++i) memcpy(text+1+2*i,"\xcc\x81",2);
    text[1+2*n]=0;
    CHECK(shape(&f,text,NX_BIDI_LTR,20,glyphs,n*4,&count)==NX_OK && count==n+1);
    CHECK(fabs(glyphs[n].offset_x-(-6+.4*(double)(n-1)))<0.00001 && close_to(glyphs[n].offset_y,14+5*(double)(n-1)));
    free(text); free(glyphs); return 0;
}
static void outlines(fixture *f) {
    uint8_t *b=f->bytes; p16(b+4,10); p16(b+306,1);
    record(b+140,NX_SFNT_TAG('l','o','c','a'),4608,68);
    record(b+156,NX_SFNT_TAG('g','l','y','f'),5120,16*34);
    for (unsigned i=0;i<=16;++i) p32(b+4608+4*i,34*i);
    for (unsigned i=0;i<16;++i) {
        uint8_t *g=b+5120+34*i; int w=(i==7 || i==8)?200:600,h=(i==7 || i==8)?150:500;
        p16(g,1); p16(g+6,(unsigned)w); p16(g+8,(unsigned)h); p16(g+10,3);
        for (unsigned p=0;p<4;++p) g[14+p]=1; /* Four on-curve points, signed 16-bit deltas. */
        p16(g+20,(unsigned)w); p16(g+24,(unsigned)(uint16_t)-w);
        p16(g+30,(unsigned)h);
    }
}
static int render(const char *output) {
    fixture f; init(&f); base(&f); mkmk(&f,0,0); outlines(&f);
    nx_font *font=NULL; nx_font_collection *fonts=NULL; nx_text_layout *layout=NULL; nx_context *ctx=NULL;
    CHECK(nx_font_open(f.bytes,sizeof(f.bytes),NULL,&font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    CHECK(nx_text_layout_create(fonts,"A\xcc\x81\xcc\x82",5,NX_BIDI_LTR,64,200,&layout)==NX_OK);
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && nx_begin_frame(ctx,128,96,0)==NX_OK);
    CHECK(nx_draw_text_layout(ctx,layout,(nx_point){10,20},(nx_color){1,1,1,1},65536)==NX_OK);
    CHECK(nx_end_frame(ctx)==NX_OK);
    const nx_command *commands=NULL; size_t command_count=0; uint8_t pixels[128*96*4];
    CHECK(nx_frame_commands(ctx,&commands,&command_count)==NX_OK);
    CHECK(nx_headless_render(commands,command_count,pixels,128,96,128*4,(nx_color){0,0,0,1})==NX_OK);
    CHECK(pixels[(30*128+32)*4]>240); /* First accent at the base anchor. */
    CHECK(pixels[(10*128+33)*4]>240); /* Second accent stacked above the first. */
    CHECK(pixels[(40*128+32)*4]==0); /* Gap between accents and base. */
    CHECK(pixels[(60*128+20)*4]>240); /* Base outline passed through the same rasterizer. */
    if (output) {
        FILE *fp=fopen(output,"wb"); CHECK(fp);
        CHECK(fprintf(fp,"P6\n128 96\n255\n")>0);
        for (size_t i=0;i<128*96;++i) CHECK(fwrite(pixels+4*i,1,3,fp)==3);
        CHECK(fclose(fp)==0);
    }
    nx_context_destroy(ctx); nx_text_layout_destroy(layout); nx_font_collection_destroy(fonts); nx_font_close(font);
    return 0;
}
static int seed(const fixture *f,const char *directory,const char *name,unsigned dlen,unsigned slen) {
    char path[1024]; int length=snprintf(path,sizeof(path),"%s/%s.bin",directory,name);
    CHECK(length>0 && (size_t)length<sizeof(path));
    uint8_t header[16]={'N','X','P','2'};
    p32(header+4,(uint32_t)f->used); p32(header+8,dlen); p32(header+12,slen);
    FILE *fp=fopen(path,"wb"); CHECK(fp);
    CHECK(fwrite(header,1,16,fp)==16 && fwrite(f->bytes+1024,1,f->used,fp)==f->used);
    CHECK(fwrite(f->bytes+4096,1,dlen,fp)==dlen && fwrite(f->bytes+3072,1,slen,fp)==slen);
    CHECK(fclose(fp)==0); return 0;
}
static int seeds(const char *directory) {
    fixture f;
    init(&f); base(&f); CHECK(seed(&f,directory,"mark-base",0,0)==0);
    mkmk(&f,0,0); CHECK(seed(&f,directory,"mark-stack",0,0)==0);
    init(&f); base(&f); gdef(&f); mkmk(&f,0x210,0); pair(&f);
    CHECK(seed(&f,directory,"mark-filter-pair",114,0)==0);
    init(&f); base(&f); gdef(&f); ligature_position(&f); ligature_substitution(&f,1); mkmk(&f,0,0);
    CHECK(seed(&f,directory,"mark-nested-ligature",114,400)==0);
    init(&f); base(&f); gdef(&f); ligature_substitution(&f,0);
    uint8_t *s=f.bytes+3072; p16(s+208,1); p16(s+214,10); p16(s+216,1); p16(s+230,7);
    CHECK(seed(&f,directory,"mark-unary-substitution",114,400)==0);
    init(&f); base(&f); uint8_t *g=f.bytes+1024;
    p16(g+240,100); anchor(g,338,400,700); p16(g+338,3); p16(g+344,10); p16(g+346,18);
    p16(g+348,20); p16(g+350,20); p16(g+352,1); p16(g+354,0x4000);
    p16(g+356,20); p16(g+358,20); p16(g+360,1); p16(g+362,0xc000); f.used=364;
    CHECK(seed(&f,directory,"mark-device",0,0)==0);
    init(&f); base(&f); p16(g+192,9); p16(g+198,200);
    p16(g+392,1); p16(g+394,4); p32(g+396,8); memcpy(g+400,g+200,56); f.used=456;
    CHECK(seed(&f,directory,"mark-extension",0,0)==0);
    return 0;
}
int main(int argc,char **argv) {
    fixture f; nx_shaped_glyph glyphs[16]; size_t count=0;
    init(&f); base(&f);
    CHECK(shape(&f,"A\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && count==2);
    CHECK(close_to(glyphs[1].offset_x,-6) && close_to(glyphs[1].offset_y,14));
    CHECK(glyphs[0].advance_x==12 && glyphs[1].advance_x==0);
    CHECK(shape(&f,"\xd7\x90\xcc\x81",NX_BIDI_RTL,20,glyphs,16,&count)==NX_OK && count==2);
    CHECK(close_to(glyphs[1].offset_x,6) && close_to(glyphs[1].offset_y,14));
    CHECK(shape(&f,"AB\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && count==3);
    CHECK(close_to(glyphs[2].offset_y,0)); /* An uncovered base blocks attachment to A. */
    p16(f.bytes+1024+216,11);
    CHECK(shape(&f,"\xe0\xa4\x95\xe0\xa4\xbe",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && count==2);
    CHECK(glyphs[1].advance_x==12 && close_to(glyphs[1].offset_x,-6) && close_to(glyphs[1].offset_y,14));
    init(&f); base(&f); mkmk(&f,0,0);
    CHECK(shape(&f,"A\xcc\x81\xcc\x82",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && count==3);
    CHECK(close_to(glyphs[2].offset_x,-5.6) && close_to(glyphs[2].offset_y,19));
    CHECK(shape(&f,"A\xcc\x81\xcc\x87\xcc\x82",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && count==4);
    CHECK(close_to(glyphs[3].offset_y,0)); /* The preceding uncovered mark blocks mkmk. */
    single(&f,2,50,30);
    CHECK(shape(&f,"A\xcc\x81\xcc\x82",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK);
    CHECK(close_to(glyphs[1].offset_x,-5) && close_to(glyphs[1].offset_y,14.6));
    CHECK(close_to(glyphs[2].offset_x,-4.6) && close_to(glyphs[2].offset_y,19.6));
    init(&f); base(&f); mkmk(&f,0,0); gdef(&f); pair(&f);
    CHECK(shape(&f,"A\xcc\x81\xcc\x82" "B",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && count==4);
    CHECK(close_to(glyphs[0].advance_x,11) && close_to(glyphs[1].offset_x,-5));
    CHECK(close_to(glyphs[2].offset_x,-4.6) && close_to(glyphs[2].offset_y,19));
    init(&f); base(&f); gdef(&f); ligature_position(&f); ligature_substitution(&f,0); mkmk(&f,0,0);
    CHECK(shape(&f,"f\xcc\x81i\xcc\x82",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && count==3);
    CHECK(glyphs[0].glyph_index==6 && glyphs[0].advance_x==18);
    CHECK(close_to(glyphs[1].offset_x,-17) && close_to(glyphs[1].offset_y,14));
    CHECK(close_to(glyphs[2].offset_x,-10.6) && close_to(glyphs[2].offset_y,14));
    CHECK(glyphs[1].cluster_byte_start==0 && glyphs[2].cluster_byte_start==0);
    CHECK(shape(&f,"fi\xcc\x81\xcc\x82",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && count==3);
    CHECK(close_to(glyphs[1].offset_x,-11) && close_to(glyphs[2].offset_x,-10.6) && close_to(glyphs[2].offset_y,19));
    init(&f); base(&f); gdef(&f); ligature_position(&f); ligature_substitution(&f,1); mkmk(&f,0,0);
    CHECK(shape(&f,"ff\xcc\x81i\xcc\x82",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && count==3);
    CHECK(glyphs[0].glyph_index==13 && glyphs[0].advance_x==24);
    CHECK(close_to(glyphs[1].offset_x,-17) && close_to(glyphs[2].offset_x,-10.6) && close_to(glyphs[2].offset_y,14));
    CHECK(shape(&f,"\xef\xac\x81\xcc\x81",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK && count==2);
    CHECK(close_to(glyphs[1].offset_x,-11) && close_to(glyphs[1].offset_y,14));
    init(&f); base(&f); gdef(&f); mkmk(&f,0x200,0);
    CHECK(shape(&f,"A\xcc\x81\xcc\x82",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK);
    CHECK(close_to(glyphs[2].offset_y,0));
    init(&f); base(&f); gdef(&f); mkmk(&f,0x210,0);
    CHECK(shape(&f,"A\xcc\x81\xcc\x82",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK);
    CHECK(close_to(glyphs[2].offset_y,19));
    init(&f); base(&f); gdef(&f); mkmk(&f,0x218,65535);
    CHECK(shape(&f,"A\xcc\x81\xcc\x82",NX_BIDI_LTR,20,glyphs,16,&count)==NX_OK);
    CHECK(close_to(glyphs[2].offset_y,0));
    init(&f); base(&f); mkmk(&f,8,0);
    memset(glyphs,0x5a,sizeof(glyphs)); nx_shaped_glyph unchanged[16]; memcpy(unchanged,glyphs,sizeof(glyphs));
    CHECK(shape(&f,"A\xcc\x81\xcc\x82",NX_BIDI_LTR,20,glyphs,16,&count)==NX_ERROR_ARGUMENT);
    CHECK(memcmp(glyphs,unchanged,sizeof(glyphs))==0);
    CHECK(table_edges()==0);
    CHECK(stress()==0);
    CHECK(render(argc==2?argv[1]:NULL)==0);
    if (argc==3 && strcmp(argv[1],"--write-seeds")==0) CHECK(seeds(argv[2])==0);
    puts("GPOS mark attachment tests passed"); return 0;
}
