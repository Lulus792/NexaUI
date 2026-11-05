#include "text_wrap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(e) do {if(!(e)){fprintf(stderr,"wrap:%d: %s\n",__LINE__,#e);goto fail;}}while(0)
#define COMBINED_E "e\xcc\x81"
static int check_layout(nx_font_collection *fonts,const char *text,double width,int require_break) {
    size_t length=strlen(text), source=0; nx_text_layout *layout=NULL; nx_utf8_span result;
    if(!nx_example_small_text_layout(fonts,text,length,28,width,&layout)) {fputs("layout creation failed\n",stderr);goto fail;}
    if(nx_text_layout_width(layout)>width || nx_text_layout_text(layout,&result)!=NX_OK) goto fail;
    unsigned breaks=0;
    for(size_t i=0;i<result.length;++i) {
        if(result.data[i]=='\n') {
            size_t boundary=0,next=0;
            while(boundary<source && nx_grapheme_next(text,length,boundary,&next)==NX_OK) boundary=next;
            if(!source || source==length || boundary!=source) {fprintf(stderr,"bad break %zu\n",source);goto fail;}
            ++breaks; continue;
        }
        if(source>=length || result.data[i]!=text[source++]) {fprintf(stderr,"byte mismatch at %zu\n",source);goto fail;}
    }
    if(source!=length || (require_break && !breaks)) {fprintf(stderr,"bytes %zu/%zu, breaks %u\n",source,length,breaks);goto fail;}
    nx_text_layout_destroy(layout);return 1;
fail:
    nx_text_layout_destroy(layout);return 0;
}
int main(int argc,char **argv) {
    FILE *file=NULL; unsigned char *bytes=NULL; nx_font *font=NULL; nx_font_collection *fonts=NULL;
    CHECK(argc==2 && (file=fopen(argv[1],"rb")) && !fseek(file,0,SEEK_END));
    long size=ftell(file); CHECK(size>0 && size<64*1024*1024 && !fseek(file,0,SEEK_SET));
    bytes=malloc((size_t)size); CHECK(bytes && fread(bytes,1,(size_t)size,file)==(size_t)size);
    CHECK(!fclose(file));file=NULL;
    CHECK(nx_font_open(bytes,(size_t)size,NULL,&font)==NX_OK &&
          nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    const char *samples[]={
        "", "A",
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz",
        "officeofficeofficeofficeofficeofficeofficeofficeofficeofficeoffice",
        "longwordwithoutspacesand another longwordwithoutspacesandmore",
        COMBINED_E COMBINED_E COMBINED_E COMBINED_E COMBINED_E COMBINED_E COMBINED_E COMBINED_E
        COMBINED_E COMBINED_E COMBINED_E COMBINED_E COMBINED_E COMBINED_E COMBINED_E,
        "\xd7\x90\xd7\x91\xd7\x92\xd7\x93\xd7\x94\xd7\x95\xd7\x96\xd7\x97\xd7\x98\xd7\x99\xd7\x9a\xd7\x9b\xd7\x9c\xd7\x9d\xd7\x9e\xd7\x9f\xd7\xa0\xd7\xa1\xd7\xa2\xd7\xa3\xd7\xa4\xd7\xa5\xd7\xa6\xd7\xa7\xd7\xa8\xd7\xa9\xd7\xaa"};
    const double widths[]={48,64,80,96,160};
    for(size_t i=0;i<sizeof(samples)/sizeof(samples[0]);++i)
        for(size_t w=0;w<sizeof(widths)/sizeof(widths[0]);++w) {
            if(!check_layout(fonts,samples[i],widths[w],i==2 || i==3)) {
                fprintf(stderr,"sample %zu, width %.0f\n",i,widths[w]);goto fail;
            }
        }
    nx_text_layout *impossible=NULL;
    CHECK(!nx_example_small_text_layout(fonts,"W",1,28,1,&impossible) && !impossible);
    nx_font_collection_destroy(fonts);nx_font_close(font);free(bytes);
    puts("Emergency wraps preserve bytes and graphemes within measured widths");return 0;
fail:
    if(file)fclose(file);
    if(fonts)nx_font_collection_destroy(fonts);
    if(font)nx_font_close(font);
    free(bytes);return 1;
}
