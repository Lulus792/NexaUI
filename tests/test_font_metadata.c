#include "nexaui/nexaui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"font metadata:%d: %s\n",__LINE__,#x);return 1;}} while(0)
static void u16(uint8_t *p,unsigned n) {p[0]=(uint8_t)(n>>8u);p[1]=(uint8_t)n;}
static void u32(uint8_t *p,uint32_t n) {p[0]=(uint8_t)(n>>24u);p[1]=(uint8_t)(n>>16u);p[2]=(uint8_t)(n>>8u);p[3]=(uint8_t)n;}
static nx_font *face(const char *tag,const uint8_t *table,size_t length) {
    /* The backing bytes survive each font until its caller closes it. */
    static uint8_t bytes[100000];nx_font *font=NULL;
    if(length>sizeof(bytes)-28u)return NULL;
    memset(bytes,0,28);u32(bytes,0x00010000u);u16(bytes+4,1);memcpy(bytes+12,tag,4);u32(bytes+20,28);u32(bytes+24,(uint32_t)length);memcpy(bytes+28,table,length);
    if(nx_font_open(bytes,length+28u,NULL,&font)!=NX_OK)return NULL;
    return font;
}
static void record(uint8_t *p,unsigned platform,unsigned encoding,unsigned language,unsigned id,unsigned length,unsigned offset) {
    u16(p,platform);u16(p+2,encoding);u16(p+4,language);u16(p+6,id);u16(p+8,length);u16(p+10,offset);
}
static int names(void) {
    uint8_t table[256]={0};u16(table+2,3);u16(table+4,42);
    record(table+6,0,4,0,16,2,0);record(table+18,3,1,0x0409,16,2,2);record(table+30,3,1,0x0409,1,2,4);
    table[43]='U';table[45]='E';table[47]='L';nx_font *f=face("name",table,48);CHECK(f);
    char out[32];size_t length=99;
    CHECK(nx_font_name_utf8(f,16,NULL,0,&length)==NX_OK && length==1);
    memset(out,'X',sizeof(out));CHECK(nx_font_name_utf8(f,16,out,1,&length)==NX_ERROR_LIMIT && length==1 && out[0]=='X');
    CHECK(nx_font_name_utf8(f,16,out,2,&length)==NX_OK && length==1 && !strcmp(out,"E") && out[2]=='X');
    CHECK(nx_font_name_utf8(f,1,out,sizeof(out),&length)==NX_OK && !strcmp(out,"L"));
    CHECK(nx_font_name_utf8(f,17,out,sizeof(out),&length)==NX_ERROR_UNSUPPORTED && !length);
    CHECK(nx_font_name_utf8(f,1,NULL,1,&length)==NX_ERROR_ARGUMENT && !length);
    CHECK(nx_font_name_utf8(NULL,1,out,sizeof(out),&length)==NX_ERROR_ARGUMENT && !length);
    CHECK(nx_font_name_utf8(f,1,out,sizeof(out),NULL)==NX_ERROR_ARGUMENT);nx_font_close(f);
    /* Rank is independent of record order; a missing English record uses Unicode0. */
    uint8_t saved[12];memcpy(saved,table+6,12);memcpy(table+6,table+18,12);memcpy(table+18,saved,12);
    f=face("name",table,48);CHECK(f && nx_font_name_utf8(f,16,out,sizeof(out),&length)==NX_OK && !strcmp(out,"E"));nx_font_close(f);
    u16(table+8,2); /* Unsupported Windows codepage2 does not outrank Unicode. */
    f=face("name",table,48);CHECK(f && nx_font_name_utf8(f,16,out,sizeof(out),&length)==NX_OK && !strcmp(out,"U"));nx_font_close(f);
    const uint8_t unicode[]={0x03,0xa9,0x6c,0x49,0xd8,0x3c,0xdf,0x0d};
    memset(table,0,sizeof(table));u16(table+2,1);u16(table+4,18);record(table+6,0,4,0,1,sizeof(unicode),0);memcpy(table+18,unicode,sizeof(unicode));
    f=face("name",table,26);CHECK(f);
    for(size_t cap=0;cap<=9;++cap) {memset(out,'X',sizeof(out));CHECK(nx_font_name_utf8(f,1,out,cap,&length)==NX_ERROR_LIMIT && length==9);for(size_t i=0;i<sizeof(out);++i)CHECK(out[i]=='X');}
    CHECK(nx_font_name_utf8(f,1,out,10,&length)==NX_OK && length==9 && !strcmp(out,"\xce\xa9\xe6\xb1\x89\xf0\x9f\x8c\x8d"));nx_font_close(f);
    /* Version1 language tags are structurally bounded, not locale negotiation. */
    memset(table,0,sizeof(table));u16(table,1);u16(table+2,1);u16(table+4,24);record(table+6,0,4,0x8000,1,2,0);u16(table+18,1);u16(table+20,4);u16(table+22,2);table[25]='N';table[27]='e';table[29]='n';
    f=face("name",table,30);CHECK(f && nx_font_name_utf8(f,1,out,sizeof(out),&length)==NX_OK && !strcmp(out,"N"));nx_font_close(f);
    u16(table+10,0x8001);f=face("name",table,30);CHECK(f && nx_font_name_utf8(f,1,out,sizeof(out),&length)==NX_ERROR_UNSUPPORTED && !length);nx_font_close(f);
    u16(table+10,0x8000);u16(table+22,100);f=face("name",table,30);CHECK(f && nx_font_name_utf8(f,1,out,sizeof(out),&length)==NX_ERROR_ARGUMENT && !length);nx_font_close(f);
    return 0;
}
static int malformed(void) {
    uint8_t t[64]={0};char out[32];size_t n;
    u16(t+2,1);u16(t+4,18);record(t+6,3,10,0x0409,1,4,0);t[19]='A';t[21]='B';
    for(size_t length=0;length<22;++length) {nx_font *f=face("name",t,length);CHECK(f);memset(out,'X',sizeof(out));n=99;CHECK(nx_font_name_utf8(f,1,out,sizeof(out),&n)==NX_ERROR_ARGUMENT && !n && out[0]=='X');nx_font_close(f);}
    const uint8_t bad[][4]={{0xd8,0x00,0,0x41},{0xdc,0x00,0,0x41},{0,0,0,0x41},{0,0x41,0xd8,0x00}};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i) {memcpy(t+18,bad[i],4);nx_font *f=face("name",t,22);CHECK(f);memset(out,'X',sizeof(out));CHECK(nx_font_name_utf8(f,1,out,sizeof(out),&n)==NX_ERROR_ARGUMENT && !n);for(size_t j=0;j<sizeof(out);++j)CHECK(out[j]=='X');nx_font_close(f);}
    t[18]=0;t[19]='A';t[20]=0;t[21]='B';u16(t+14,3);nx_font *f=face("name",t,22);CHECK(f && nx_font_name_utf8(f,1,out,sizeof(out),&n)==NX_ERROR_ARGUMENT && !n);nx_font_close(f);
    u16(t+14,4);u16(t+4,17);f=face("name",t,22);CHECK(f && nx_font_name_utf8(f,1,out,sizeof(out),&n)==NX_ERROR_ARGUMENT && !n);nx_font_close(f);
    u16(t+4,18);u16(t,2);f=face("name",t,22);CHECK(f && nx_font_name_utf8(f,1,out,sizeof(out),&n)==NX_ERROR_UNSUPPORTED && !n);nx_font_close(f);
    u16(t,0);u16(t+2,4097);f=face("name",t,22);CHECK(f && nx_font_name_utf8(f,1,out,sizeof(out),&n)==NX_ERROR_LIMIT && !n);nx_font_close(f);
    f=face("xxxx",t,22);CHECK(f && nx_font_name_utf8(f,1,out,sizeof(out),&n)==NX_ERROR_UNSUPPORTED && !n);nx_font_close(f);
    uint8_t *large=calloc(1,49158);CHECK(large);u16(large+2,4096);u16(large+4,49158);
    for(unsigned i=0;i<4096;++i)record(large+6u+(size_t)i*12u,0,4,0,1,0,0);
    f=face("name",large,49158);CHECK(f && nx_font_name_utf8(f,1,out,sizeof(out),&n)==NX_OK && !n && !out[0]);nx_font_close(f);free(large);
    large=calloc(1,8212);CHECK(large);u16(large+2,1);u16(large+4,18);record(large+6,0,4,0,1,8192,0);for(size_t i=0;i<4097;++i)large[19+i*2]='A';
    f=face("name",large,8212);CHECK(f && nx_font_name_utf8(f,1,NULL,0,&n)==NX_OK && n==4096);nx_font_close(f);
    u16(large+14,8194);f=face("name",large,8212);CHECK(f && nx_font_name_utf8(f,1,out,sizeof(out),&n)==NX_ERROR_LIMIT && !n);nx_font_close(f);free(large);
    return 0;
}
static int styles(void) {
    uint8_t table[100]={0};const size_t lengths[]={68,86,96,96,96,100};nx_font_style style;
    CHECK(nx_font_get_style(NULL,&style)==NX_ERROR_ARGUMENT && !style.weight_class && !style.width_class);
    CHECK(nx_font_get_style(NULL,NULL)==NX_ERROR_ARGUMENT);
    for(unsigned version=0;version<=5;++version) {
        u16(table,version);u16(table+4,650);u16(table+6,3);u16(table+62,513);
        for(size_t length=0;length<lengths[version];++length) {nx_font *f=face("OS/2",table,length);CHECK(f && nx_font_get_style(f,&style)==NX_ERROR_ARGUMENT && !style.weight_class && !style.width_class);nx_font_close(f);}
        nx_font *f=face("OS/2",table,lengths[version]);CHECK(f && nx_font_get_style(f,&style)==NX_OK && style.weight_class==650 && style.width_class==3 && style.slant==(version>=4?NX_FONT_SLANT_OBLIQUE:NX_FONT_SLANT_ITALIC));nx_font_close(f);
    }
    for(unsigned k=0;k<4;++k) {u16(table+4,k<2?(k?1001:0):400);u16(table+6,k>=2?(k==2?0:10):5);nx_font *f=face("OS/2",table,100);CHECK(f && nx_font_get_style(f,&style)==NX_ERROR_ARGUMENT && !style.weight_class);nx_font_close(f);}
    u16(table+4,1000);u16(table+6,9);u16(table+62,0);nx_font *f=face("OS/2",table,100);CHECK(f && nx_font_get_style(f,&style)==NX_OK && style.slant==NX_FONT_SLANT_UPRIGHT && style.weight_class==1000 && style.width_class==9);nx_font_close(f);
    u16(table,6);f=face("OS/2",table,100);CHECK(f && nx_font_get_style(f,&style)==NX_ERROR_UNSUPPORTED && !style.weight_class);nx_font_close(f);
    f=face("xxxx",table,100);CHECK(f && nx_font_get_style(f,&style)==NX_ERROR_UNSUPPORTED && !style.weight_class);nx_font_close(f);return 0;
}
static int mutations(void) {
    uint8_t t[100]={0};u16(t+2,1);u16(t+4,18);record(t+6,0,4,0,1,4,0);t[19]='A';t[21]='B';
    for(size_t at=0;at<sizeof(t);++at)for(unsigned bit=0;bit<8;++bit) {
        t[at]^=(uint8_t)(1u<<bit);nx_font *font=face("name",t,sizeof(t));CHECK(font);char out[32];memset(out,'X',sizeof(out));size_t length=99;
        nx_result result=nx_font_name_utf8(font,1,out,sizeof(out),&length);
        CHECK(result==NX_OK || result==NX_ERROR_ARGUMENT || result==NX_ERROR_LIMIT || result==NX_ERROR_UNSUPPORTED);
        if(result!=NX_OK)for(size_t i=0;i<sizeof(out);++i)CHECK(out[i]=='X');
        else CHECK(length<sizeof(out) && !out[length]);
        nx_font_close(font);t[at]^=(uint8_t)(1u<<bit);
    }
    return 0;
}
static int real_font(char **argv) {
    FILE *file=fopen(argv[1],"rb");CHECK(file && !fseek(file,0,SEEK_END));long size=ftell(file);CHECK(size>0 && size<20000000 && !fseek(file,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)size);CHECK(bytes && fread(bytes,1,(size_t)size,file)==(size_t)size && !fclose(file));nx_font *font=NULL;CHECK(nx_font_open(bytes,(size_t)size,NULL,&font)==NX_OK);
    char name[4097];size_t count=0;nx_result result=nx_font_name_utf8(font,16,name,sizeof(name),&count);if(result==NX_ERROR_UNSUPPORTED)result=nx_font_name_utf8(font,1,name,sizeof(name),&count);
    nx_font_style style;CHECK(result==NX_OK && !strcmp(name,argv[2]) && nx_font_get_style(font,&style)==NX_OK);
    CHECK(style.weight_class==(unsigned)strtoul(argv[3],NULL,10) && (unsigned)style.slant==(unsigned)strtoul(argv[4],NULL,10));
    printf("Real font: family=%s weight=%u width=%u slant=%u\n",name,(unsigned)style.weight_class,(unsigned)style.width_class,(unsigned)style.slant);nx_font_close(font);free(bytes);return 0;
}
int main(int argc,char **argv) {
    CHECK(argc==1 || argc==5);CHECK(!names() && !malformed() && !styles() && !mutations());if(argc==5)CHECK(!real_font(argv));
    puts("Font metadata: exact UTF-8, deterministic name selection, bounds, atomic errors, legacy/versioned OS/2 and explicit unknown passed.");return 0;
}
