#define _POSIX_C_SOURCE 200809L
/* Optional bounded example benchmark: no presentation or native input. */
#include "view.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_ms(void) {
    struct timespec t;
    return clock_gettime(CLOCK_MONOTONIC,&t)==0 ? t.tv_sec*1000.0+t.tv_nsec/1000000.0 : -1;
}
int main(int argc,char **argv) {
    FILE *file=NULL; unsigned char *font=NULL; aetheri_view *view=NULL; int ok=0;
    if(argc!=2 || !(file=fopen(argv[1],"rb")) || fseek(file,0,SEEK_END)) goto done;
    long size=ftell(file); if(size<=0 || size>64*1024*1024 || fseek(file,0,SEEK_SET)) goto done;
    font=malloc((size_t)size); if(!font || fread(font,1,(size_t)size,file)!=(size_t)size) goto done;
    if(fclose(file)) {file=NULL;goto done;} file=NULL;
    struct ae_settings model; ae_settings_init(&model); model.german=true; model.text_percent=200;
    struct ae_settings_display displays[AE_SETTINGS_DISPLAYS]={0};
    for(size_t i=0;i<AE_SETTINGS_DISPLAYS;++i) {
        displays[i].id=(uint32_t)i+1; displays[i].scale=1;
        for(size_t j=0;j<120;++j) displays[i].name[j]=(char)('a'+(j+i)%26);
    }
    if(!ae_settings_snapshot(&model,displays,AE_SETTINGS_DISPLAYS) ||
       !aetheri_view_create(&model,font,(size_t)size,&view)) goto done;
    for(unsigned i=0;i<6;++i) {
        int changed=0; double start=now_ms();
        if(start<0 || !aetheri_view_frame(view,128,150,&changed) || changed) goto done;
        double end=now_ms(); if(end<start) goto done;
        uint32_t index; nx_semantic_view node;
        if(nx_semantics_find(aetheri_view_context(view),UINT64_C(0x100000010),&index)!=NX_OK ||
           nx_semantics_get(aetheri_view_context(view),index,&node)!=NX_OK || node.name.length!=120) goto done;
        printf("%s %.3f ms\n",i?"warm":"cold",end-start);
    }
    ok=1;
done:
    if(file)fclose(file);
    aetheri_view_destroy(view);free(font);
    if(!ok)fputs("Settings benchmark failed\n",stderr);
    return ok?0:1;
}
