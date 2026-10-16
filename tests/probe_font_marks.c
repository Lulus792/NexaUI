/* Optional real-font diagnostic. No system font is required by CTest. */
#include "nexaui/nexaui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv) {
    if (argc!=2) { fprintf(stderr,"usage: nexaui_font_mark_probe FONT_FILE\n"); return 2; }
    int status=1; FILE *fp=fopen(argv[1],"rb"); uint8_t *bytes=NULL;
    nx_font *font=NULL; nx_font_collection *fonts=NULL;
    if (!fp || fseek(fp,0,SEEK_END)) goto done;
    long length=ftell(fp);
    if (length<=0 || length>64L*1024L*1024L || fseek(fp,0,SEEK_SET)) goto done;
    bytes=malloc((size_t)length);
    if (!bytes || fread(bytes,1,(size_t)length,fp)!=(size_t)length) goto done;
    if (nx_font_open(bytes,(size_t)length,NULL,&font)!=NX_OK ||
        nx_font_collection_create(NULL,&fonts)!=NX_OK ||
        nx_font_collection_add(fonts,font)!=NX_OK) goto done;
    const char *samples[]={"A\xcc\x81","A\xcc\x81\xcc\x82","f\xcc\x81i\xcc\x82","\xd7\x90\xd6\xb0"};
    status=0;
    for (unsigned sample=0;sample<4;++sample) {
        nx_shaped_glyph glyphs[16]; nx_shape_run runs[16]; size_t count=0,run_count=0; uint8_t level=0;
        nx_result result=nx_shape_text(fonts,samples[sample],strlen(samples[sample]),
            sample==3?NX_BIDI_RTL:NX_BIDI_LTR,32,glyphs,16,runs,16,&count,&run_count,&level);
        printf("sample,%u,result,%d,count,%zu\n",sample,(int)result,count);
        if (result!=NX_OK) status=1;
        for (size_t i=0;i<count;++i)
            printf("glyph,%u,%zu,%u,%.12g,%.12g,%.12g\n",sample,i,
                (unsigned)glyphs[i].glyph_index,glyphs[i].advance_x,glyphs[i].offset_x,glyphs[i].offset_y);
    }
done:
    nx_font_collection_destroy(fonts); nx_font_close(font); free(bytes);
    if (fp && fclose(fp)) status=1;
    return status;
}
