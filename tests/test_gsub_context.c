#include "nexaui/nexaui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

typedef struct fixture { uint8_t bytes[16384]; } fixture;
typedef struct action { uint16_t position,lookup; } action;
static void p16(uint8_t *p,unsigned x) { p[0]=(uint8_t)(x>>8); p[1]=(uint8_t)x; }
static void p32(uint8_t *p,uint32_t x) { p[0]=(uint8_t)(x>>24); p[1]=(uint8_t)(x>>16); p[2]=(uint8_t)(x>>8); p[3]=(uint8_t)x; }
static void record(uint8_t *p,uint32_t tag,unsigned offset,unsigned length) { p32(p,tag); p32(p+8,offset); p32(p+12,length); }
static uint8_t *table(fixture *f) { return f->bytes+1024; }
static void init(fixture *f) {
    memset(f,0,sizeof(*f)); uint8_t *b=f->bytes;
    p32(b,0x10000); p16(b+4,7);
    record(b+12,NX_SFNT_TAG('h','e','a','d'),256,54);
    record(b+28,NX_SFNT_TAG('m','a','x','p'),320,32);
    record(b+44,NX_SFNT_TAG('h','h','e','a'),368,36);
    record(b+60,NX_SFNT_TAG('h','m','t','x'),416,64);
    record(b+76,NX_SFNT_TAG('c','m','a','p'),512,112);
    record(b+92,NX_SFNT_TAG('G','S','U','B'),1024,8192);
    record(b+108,NX_SFNT_TAG('G','D','E','F'),12288,64);
    p32(b+256,0x10000); p32(b+268,0x5f0f3cf5); p16(b+274,1000);
    p32(b+320,0x10000); p16(b+324,16);
    p32(b+368,0x10000); p16(b+372,800); p16(b+374,(uint16_t)-200); p16(b+402,16);
    for (unsigned i=0;i<16;++i) p16(b+416+4*i,600);
    uint8_t *c=b+512; p16(c+2,1); p16(c+4,3); p16(c+6,10); p32(c+8,12); c+=12;
    p16(c,12); p32(c+4,100); p32(c+12,7);
    for (unsigned i=0;i<7;++i) { uint32_t scalar=i<6?'A'+i:0x301; p32(c+16+12*i,scalar); p32(c+20+12*i,scalar); p32(c+24+12*i,i+1); }
    uint8_t *g=table(f); p32(g,0x10000); p16(g+4,10); p16(g+6,48); p16(g+8,128);
    p16(g+10,1); p32(g+12,NX_SFNT_TAG('D','F','L','T')); p16(g+16,8);
    p16(g+18,4); p16(g+24,0xffff); p16(g+26,1); p16(g+28,0);
    p16(g+48,1); p32(g+50,NX_SFNT_TAG('c','c','m','p')); p16(g+54,16);
    p16(g+66,1); p16(g+68,0);
    p16(g+128,8); for (unsigned i=0;i<8;++i) p16(g+130+2*i,256+512*i-128);
    uint8_t *d=b+12288; p32(d,0x10000); p16(d+4,12); p16(d+12,2); p16(d+14,3);
    p16(d+16,1); p16(d+18,6); p16(d+20,1);
    p16(d+22,7); p16(d+24,7); p16(d+26,3);
    p16(d+28,8); p16(d+30,15); p16(d+32,2);
}
static uint8_t *lookup(fixture *f,unsigned index,unsigned type,unsigned flags) {
    uint8_t *l=table(f)+256+512*index; memset(l,0,512);
    p16(l,type); p16(l+2,flags); p16(l+4,1); p16(l+6,8); return l+8;
}
static void coverage(uint8_t *p,unsigned glyph) { p16(p,1); p16(p+2,1); p16(p+4,glyph); }
static void single(fixture *f,unsigned index,unsigned from,unsigned to) {
    uint8_t *s=lookup(f,index,1,0); p16(s,2); p16(s+2,8); p16(s+4,1); p16(s+6,to); coverage(s+8,from);
}
static void ligature(fixture *f,unsigned index,unsigned first,unsigned second,unsigned to) {
    uint8_t *s=lookup(f,index,4,8); p16(s,1); p16(s+2,8); p16(s+4,1); p16(s+6,14);
    coverage(s+8,first); p16(s+14,1); p16(s+16,4); p16(s+18,to); p16(s+20,2); p16(s+22,second);
}
static void extension(fixture *f,unsigned index) {
    uint8_t *l=table(f)+256+512*index; unsigned type=((unsigned)l[0]<<8)|l[1];
    memmove(l+16,l+8,496); p16(l,7); p16(l+8,1); p16(l+10,type); p32(l+12,8);
}
static unsigned class_value(unsigned kind,unsigned glyph) {
    return kind==1?glyph-1:kind==0?(glyph+3)%7:(glyph+5)%7;
}
static size_t classes(uint8_t *s,size_t at,unsigned kind) {
    if (kind==0) {
        p16(s+at,2); p16(s+at+2,7);
        for (unsigned i=1;i<=7;++i) { p16(s+at+4+6*(i-1),i); p16(s+at+6+6*(i-1),i); p16(s+at+8+6*(i-1),class_value(kind,i)); }
        return at+46;
    }
    /* Input A is deliberately implicit class zero. */
    unsigned first=kind==1?2:1,count=kind==1?6:7;
    p16(s+at,1); p16(s+at+2,first); p16(s+at+4,count);
    for (unsigned i=0;i<count;++i) p16(s+at+6+2*i,class_value(kind,first+i));
    return at+6+2*count;
}
static void pattern(fixture *f,unsigned index,unsigned type,unsigned format,unsigned flags,
    const unsigned *back,unsigned bn,const unsigned *input,unsigned in,
    const unsigned *ahead,unsigned an,const action *actions,unsigned count) {
    uint8_t *s=lookup(f,index,type,flags); p16(s,format);
    size_t cursor,coverage_fields[192],coverage_glyphs[192],cn=0;
    if (format==3) cursor=2;
    else {
        size_t header=format==1?6u:type==5?8u:12u;
        unsigned set=format==1?0:class_value(1,input[0]),sets=set+1;
        p16(s+header-2,sets); size_t rule_set=header+2*sets;
        p16(s+header+2*set,(unsigned)rule_set); p16(s+rule_set,1); p16(s+rule_set+2,4);
        cursor=rule_set+4;
    }
    if (type==6) {
        p16(s+cursor,bn); cursor+=2;
        for (unsigned i=0;i<bn;++i) {
            if (format==3) { coverage_fields[cn]=cursor; coverage_glyphs[cn++]=back[i]; }
            else p16(s+cursor,format==2?class_value(0,back[i]):back[i]);
            cursor+=2;
        }
    }
    p16(s+cursor,in); cursor+=2;
    if (type==5) { p16(s+cursor,count); cursor+=2; }
    for (unsigned i=format==3?0:1;i<in;++i) {
        if (format==3) { coverage_fields[cn]=cursor; coverage_glyphs[cn++]=input[i]; }
        else p16(s+cursor,format==2?class_value(1,input[i]):input[i]);
        cursor+=2;
    }
    if (type==6) {
        p16(s+cursor,an); cursor+=2;
        for (unsigned i=0;i<an;++i) {
            if (format==3) { coverage_fields[cn]=cursor; coverage_glyphs[cn++]=ahead[i]; }
            else p16(s+cursor,format==2?class_value(2,ahead[i]):ahead[i]);
            cursor+=2;
        }
        p16(s+cursor,count); cursor+=2;
    }
    for (unsigned i=0;i<count;++i) { p16(s+cursor,actions[i].position); p16(s+cursor+2,actions[i].lookup); cursor+=4; }
    if (format==3) {
        for (size_t i=0;i<cn;++i) { p16(s+coverage_fields[i],(unsigned)cursor); coverage(s+cursor,(unsigned)coverage_glyphs[i]); cursor+=6; }
    } else {
        p16(s+2,(unsigned)cursor); coverage(s+cursor,input[0]); cursor+=6;
        if (format==2) for (unsigned k=0;k<3;++k) {
            if (type==5 && k!=1) continue;
            p16(s+(type==5?4u:4u+2*k),(unsigned)cursor); cursor=classes(s,cursor,k);
        }
    }
}
static nx_result shape(fixture *f,const char *text,nx_shaped_glyph *glyphs,size_t capacity,size_t *count) {
    nx_font *font=NULL; nx_font_collection *fonts=NULL; nx_shape_run runs[32]; size_t run_count=0; uint8_t paragraph=0;
    nx_result r=nx_font_open(f->bytes,sizeof(f->bytes),NULL,&font);
    if (r==NX_OK) r=nx_font_collection_create(NULL,&fonts);
    if (r==NX_OK) r=nx_font_collection_add(fonts,font);
    if (r==NX_OK) r=nx_shape_text(fonts,text,strlen(text),NX_BIDI_LTR,20,glyphs,capacity,runs,32,count,&run_count,&paragraph);
    nx_font_collection_destroy(fonts); nx_font_close(font); return r;
}
static int expect(fixture *f,const char *text,const unsigned *ids,size_t expected) {
    nx_shaped_glyph glyphs[64]; size_t count=0;
    CHECK(shape(f,text,glyphs,64,&count)==NX_OK && count==expected);
    for (size_t i=0;i<count;++i) CHECK(glyphs[i].glyph_index==ids[i]); return 0;
}
static int test_formats(void) {
    for (unsigned type=5;type<=6;++type) for (unsigned format=1;format<=3;++format) {
        fixture f; init(&f); unsigned input[]={2,3},back[]={1,6},ahead[]={4,5};
        action actions[]={{1,1},{0,2}};
        pattern(&f,0,type,format,0,back,type==6?2:0,input,2,ahead,type==6?2:0,actions,2);
        single(&f,1,3,6); single(&f,2,2,5);
        if (type==5) { const unsigned ids[]={1,5,6,4,5}; CHECK(!expect(&f,"ABCDE",ids,5)); }
        else {
            const unsigned yes[]={6,1,5,6,4,5},no[]={1,6,2,3,4,5};
            CHECK(!expect(&f,"FABCDE",yes,6)); CHECK(!expect(&f,"AFBCDE",no,6));
            const unsigned look_no[]={6,1,2,3,5,4}; CHECK(!expect(&f,"FABCED",look_no,6));
        }
    }
    return 0;
}
static int test_nested_and_contraction(void) {
    for (unsigned format=1;format<=3;++format) {
        fixture f; init(&f); unsigned input[]={1,2,3}; action actions[]={{0,1},{1,2}};
        pattern(&f,0,5,format,8,NULL,0,input,3,NULL,0,actions,2);
        ligature(&f,1,1,2,8); single(&f,2,3,6);
        const unsigned ids[]={8,6}; CHECK(!expect(&f,"ABC",ids,2));
        nx_shaped_glyph glyphs[8]; size_t count=0;
        CHECK(shape(&f,"ABC",glyphs,8,&count)==NX_OK && glyphs[0].byte_start==0 && glyphs[0].byte_end==2 && glyphs[1].byte_start==2);
        action outer[]={{0,1}},inner[]={{0,2}}; unsigned two[]={1,2},one[]={1},look[]={2};
        pattern(&f,0,5,format,0,NULL,0,two,2,NULL,0,outer,1);
        pattern(&f,1,6,3,0,NULL,0,one,1,look,1,inner,1); single(&f,2,1,5);
        const unsigned nested[]={5,2}; CHECK(!expect(&f,"AB",nested,2));
    }
    fixture f; init(&f); unsigned one[]={1}; action actions[]={{0,1}};
    pattern(&f,0,5,3,0,NULL,0,one,1,NULL,0,actions,1); ligature(&f,1,1,2,8);
    const unsigned fence[]={8,3}; CHECK(!expect(&f,"ABC",fence,2));
    return 0;
}
static int test_filter_and_progress(void) {
    fixture f; init(&f); unsigned input[]={1,2}; action actions[]={{0,1}};
    pattern(&f,0,5,3,8,NULL,0,input,2,NULL,0,actions,1); ligature(&f,1,1,2,8);
    const unsigned ids[]={8,7,7}; CHECK(!expect(&f,"A\xcc\x81" "B\xcc\x81",ids,3));
    unsigned mark[]={7}; pattern(&f,0,5,3,8,NULL,0,mark,1,NULL,0,actions,1); single(&f,1,7,6);
    const unsigned first_mark[]={6}; CHECK(!expect(&f,"\xcc\x81",first_mark,1));
    unsigned pair[]={1,2}; action second[]={{1,1}};
    pattern(&f,0,5,3,0,NULL,0,pair,2,NULL,0,second,1); single(&f,1,2,1);
    const unsigned skip[]={1,1,2}; CHECK(!expect(&f,"ABB",skip,3));
    unsigned b[]={2},a[]={1}; pattern(&f,0,6,3,0,a,1,b,1,b,1,actions,1);
    const unsigned look[]={1,1,1,2}; CHECK(!expect(&f,"ABBB",look,4));
    return 0;
}
static int test_errors(void) {
    fixture f; init(&f); unsigned input[]={1}; action cycle[]={{0,0}};
    pattern(&f,0,5,3,0,NULL,0,input,1,NULL,0,cycle,1);
    nx_shaped_glyph glyphs[8],before[8]; memset(glyphs,0xa5,sizeof(glyphs)); memcpy(before,glyphs,sizeof(glyphs));
    size_t count=77; CHECK(shape(&f,"A",glyphs,8,&count)==NX_ERROR_LIMIT && !memcmp(glyphs,before,sizeof(glyphs)) && count==77);
    action invalid[]={{1,1}}; pattern(&f,0,5,3,0,NULL,0,input,1,NULL,0,invalid,1); single(&f,1,1,5);
    CHECK(shape(&f,"A",glyphs,8,&count)==NX_ERROR_ARGUMENT && !memcmp(glyphs,before,sizeof(glyphs)));
    invalid[0]=(action){0,8}; pattern(&f,0,5,3,0,NULL,0,input,1,NULL,0,invalid,1);
    CHECK(shape(&f,"A",glyphs,8,&count)==NX_ERROR_ARGUMENT);
    for (unsigned type=5;type<=6;++type) for (unsigned format=1;format<=3;++format) {
        unsigned pair[]={1,2}; action actions[]={{0,1}};
        pattern(&f,0,type,format,0,NULL,0,pair,2,NULL,0,actions,1); single(&f,1,1,5);
        for (unsigned length=0;length<790;++length) {
            p32(f.bytes+104,length); count=77;
            CHECK(shape(&f,"AB",glyphs,8,&count)==NX_ERROR_ARGUMENT && count==77 && !memcmp(glyphs,before,sizeof(glyphs)));
        }
        p32(f.bytes+104,8192);
        extension(&f,0); extension(&f,1);
        const unsigned ids[]={5,2}; CHECK(!expect(&f,"AB",ids,2));
    }
    return 0;
}
static int test_first_match_and_unary(void) {
    fixture f; init(&f); unsigned input[]={1}; action actions[]={{0,1}};
    pattern(&f,0,5,3,0,NULL,0,input,1,NULL,0,NULL,0);
    /* A second matching subtable must not bypass a zero-action ignore rule. */
    uint8_t *l=table(&f)+256,*s=l+128;
    /* Move the first pattern to make room for the second offset. */
    memmove(l+16,l+8,14); p16(l+4,2); p16(l+6,16); p16(l+8,128);
    p16(s,3); p16(s+2,1); p16(s+4,1); p16(s+6,12); p16(s+8,0); p16(s+10,1); coverage(s+12,1);
    single(&f,1,1,5); const unsigned unchanged[]={1}; CHECK(!expect(&f,"A",unchanged,1));
    init(&f); unsigned mark[]={7}; pattern(&f,0,5,3,8,NULL,0,mark,1,NULL,0,actions,1);
    ligature(&f,1,7,7,6); p16(table(&f)+256+512+8+20,1);
    nx_shaped_glyph glyphs[8]; size_t count=0;
    CHECK(shape(&f,"\xcc\x81",glyphs,8,&count)==NX_OK && count==1 && glyphs[0].glyph_index==6 && glyphs[0].advance_x==0);
    return 0;
}
static int test_large(void) {
    fixture f; init(&f); unsigned input[]={1,2}; action actions[]={{0,1}};
    pattern(&f,0,5,3,8,NULL,0,input,2,NULL,0,actions,1); ligature(&f,1,1,2,8);
    size_t n=10000; char *text=malloc(n*2+1); nx_shaped_glyph *glyphs=malloc(n*sizeof(*glyphs));
    CHECK(text && glyphs); for (size_t i=0;i<n;++i) { text[2*i]='A'; text[2*i+1]='B'; } text[n*2]=0;
    size_t count=0; CHECK(shape(&f,text,glyphs,n,&count)==NX_OK && count==n);
    for (size_t i=0;i<n;++i) CHECK(glyphs[i].glyph_index==8 && glyphs[i].byte_start==2*i && glyphs[i].byte_end==2*i+2);
    free(glyphs); free(text); return 0;
}
static int dump_fixtures(const char *directory) {
    for (unsigned type=5;type<=6;++type) for (unsigned format=1;format<=3;++format) {
        fixture f; init(&f); unsigned input[]={2,3},back[]={1,6},ahead[]={4,5}; action actions[]={{1,1},{0,2}};
        pattern(&f,0,type,format,0,back,type==6?2:0,input,2,ahead,type==6?2:0,actions,2);
        single(&f,1,3,6); single(&f,2,2,5); p16(table(&f)+128,3);
        char path[1024]; int n=snprintf(path,sizeof(path),"%s/context-%u-%u.ttf",directory,type,format);
        CHECK(n>0 && (size_t)n<sizeof(path)); FILE *out=fopen(path,"wb"); CHECK(out);
        CHECK(fwrite(f.bytes,1,sizeof(f.bytes),out)==sizeof(f.bytes) && fclose(out)==0);
        n=snprintf(path,sizeof(path),"%s/context-%u-%u.bin",directory,type,format);
        CHECK(n>0 && (size_t)n<sizeof(path)); out=fopen(path,"wb"); CHECK(out);
        CHECK(fwrite(table(&f),1,1302,out)==1302 && fclose(out)==0);
    }
    return 0;
}
int main(int argc,char **argv) {
    if (test_formats() || test_nested_and_contraction() || test_filter_and_progress() || test_errors() ||
        test_first_match_and_unary() || test_large()) return 1;
    if (argc==2 && dump_fixtures(argv[1])) return 1;
    puts("contextual GSUB formats, chaining, nested edits and bounds passed"); return 0;
}
