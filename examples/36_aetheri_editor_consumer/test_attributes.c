#include "attributes.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"editor attributes:%d: %s\n",__LINE__,#x);return 1;}} while(0)
static const char *value(const nx_text_document_attribute *attrs,size_t count,const char *name) {
    for(size_t i=0;i<count;++i)if(attrs[i].name.length==strlen(name) && !memcmp(attrs[i].name.data,name,strlen(name)))return attrs[i].value.data;
    return NULL;
}
static int contract(const nx_font *font) {
    const char text[]="A\xc3\xa9\xf0\x9f\x8c\x8d\r\nZ";struct editor_attributes s={0};
    struct editor_attribute_desc d={.font=font,.text={text,sizeof(text)-1},.pixels_per_em=14,.logical_dpi=96,
        .foreground={1,1,1,1},.background={0,0,0,1},.selected={1,1,0,1},.selection_start=1,.selection_end=7,.selected_text=true,.contrast=true,.editable=true};
    nx_text_document_box box={0,1,{1,2,3,4}};nx_text_document_presentation p={.boxes=&box,.box_count=1};
    CHECK(editor_attributes_project(&s,&d,&p)==NX_OK && p.boxes==&box && p.box_count==1 && p.default_count==(font?8u:4u));
    CHECK(!strcmp(value(p.defaults,p.default_count,"size"),"10.5") && !strcmp(value(p.defaults,p.default_count,"fg-color"),"65535,65535,65535") && !strcmp(value(p.defaults,p.default_count,"bg-color"),"0,0,0") && !strcmp(value(p.defaults,p.default_count,"editable"),"true"));
    CHECK(p.run_count==1 && p.runs[0].start_byte==1 && p.runs[0].end_byte==7 && !strcmp(value(p.runs[0].attributes,2,"bg-color"),"65535,65535,0") && !strcmp(value(p.runs[0].attributes,2,"fg-color"),"0,0,0"));
    if(font) {CHECK(value(p.defaults,p.default_count,"family-name") && !strcmp(value(p.defaults,p.default_count,"weight"),"400") && !strcmp(value(p.defaults,p.default_count,"style"),"normal"));}
    else CHECK(!value(p.defaults,p.default_count,"family-name") && s.family_result==NX_ERROR_UNSUPPORTED && s.style_result==NX_ERROR_UNSUPPORTED);
    d.pixels_per_em=21;d.contrast=false;d.editable=false;d.foreground=(nx_color){.5f,0,1,1};
    CHECK(editor_attributes_project(&s,&d,&p)==NX_OK && !strcmp(value(p.defaults,p.default_count,"size"),"15.75") && !strcmp(value(p.defaults,p.default_count,"editable"),"false") && !strcmp(value(p.runs[0].attributes,2,"fg-color"),"32768,0,65535"));
    d.logical_dpi=144;CHECK(editor_attributes_project(&s,&d,&p)==NX_OK && !strcmp(value(p.defaults,p.default_count,"size"),"10.5"));
    d.selected_text=false;CHECK(editor_attributes_project(&s,&d,&p)==NX_OK && !p.run_count && !p.runs);
    d.font=NULL;CHECK(editor_attributes_project(&s,&d,&p)==NX_OK && p.default_count==4 && !value(p.defaults,p.default_count,"family-name"));
    d.selection_start=2;CHECK(editor_attributes_project(&s,&d,&p)==NX_ERROR_ARGUMENT && !p.default_count && !p.run_count && p.box_count==1 && p.boxes==&box);
    d.selection_start=1;d.pixels_per_em=NAN;CHECK(editor_attributes_project(&s,&d,&p)==NX_ERROR_ARGUMENT && !p.defaults);
    d.pixels_per_em=14;d.foreground.a=.5f;CHECK(editor_attributes_project(&s,&d,&p)==NX_ERROR_ARGUMENT && !p.defaults);
    d.foreground.a=1;d.logical_dpi=0;CHECK(editor_attributes_project(&s,&d,&p)==NX_ERROR_ARGUMENT && !p.defaults);
    return 0;
}
static int unknown_font(void) {
    uint8_t bytes[48]={0};bytes[1]=1;bytes[5]=1;memcpy(bytes+12,"name",4);bytes[23]=28;bytes[27]=20;
    bytes[31]=1;bytes[33]=18;bytes[37]=4;bytes[41]=1;bytes[43]=2;bytes[46]=0xd8;
    nx_font *font=NULL;CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font)==NX_OK);
    struct editor_attributes s={0};nx_text_document_presentation p={0};
    struct editor_attribute_desc d={.font=font,.pixels_per_em=14,.logical_dpi=96,.foreground={1,1,1,1},.background={0,0,0,1},.selected={1,1,0,1}};
    nx_result result=editor_attributes_project(&s,&d,&p);nx_font_close(font);
    CHECK(result==NX_OK && p.default_count==4 && s.family_result==NX_ERROR_ARGUMENT && s.style_result==NX_ERROR_UNSUPPORTED && !value(p.defaults,p.default_count,"family-name"));
    return 0;
}
int main(int argc,char **argv) {
    CHECK(argc==1 || argc==2);CHECK(!contract(NULL) && !unknown_font());
    if(argc==2) {
        FILE *file=fopen(argv[1],"rb");CHECK(file && !fseek(file,0,SEEK_END));long length=ftell(file);CHECK(length>0 && length<20000000 && !fseek(file,0,SEEK_SET));
        uint8_t *bytes=malloc((size_t)length);CHECK(bytes && fread(bytes,1,(size_t)length,file)==(size_t)length && !fclose(file));nx_font *font=NULL;
        CHECK(nx_font_open(bytes,(size_t)length,NULL,&font)==NX_OK);int failed=contract(font);nx_font_close(font);free(bytes);CHECK(!failed);
    }
    puts("Editor attributes: logical point units,16-bit color, selection overrides, style/font refresh, edit policy and atomic omission with retained geometry passed.");return 0;
}
