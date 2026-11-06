/* Controlled presentation example: no backend dispatch or privileged operation. */
#include <nexaui/nexaui.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv) {
    int okay=0; FILE *file=NULL; unsigned char *font_bytes=NULL,*pixels=NULL;
    nx_font *font=NULL; nx_font_collection *fonts=NULL; nx_context *ctx=NULL;
    nx_text_layout *layouts[3]={0};
    const char *labels[]={"Observed progress: 40%","Working: total progress unavailable","Result unknown: reconnect to the owner"};
    const char *explanation="Observed active operation; no completion estimate";
    if(argc!=3 || !(file=fopen(argv[1],"rb")))goto done;
    if(fseek(file,0,SEEK_END))goto done;
    long length=ftell(file);
    if(length<=0 || length>64000000 || fseek(file,0,SEEK_SET))goto done;
    font_bytes=malloc((size_t)length); if(!font_bytes)goto done;
    if(fread(font_bytes,1,(size_t)length,file)!=(size_t)length)goto done;
    fclose(file);file=NULL;
    if(nx_font_open(font_bytes,(size_t)length,NULL,&font)!=NX_OK ||
       nx_font_collection_create(NULL,&fonts)!=NX_OK || nx_font_collection_add(fonts,font)!=NX_OK ||
       nx_context_create(NULL,&ctx)!=NX_OK || nx_begin_frame(ctx,480,180,0)!=NX_OK)goto done;
    for(unsigned i=0;i<3;++i) {
        if(nx_text_layout_create(fonts,labels[i],strlen(labels[i]),NX_BIDI_LTR,16,450,&layouts[i])!=NX_OK ||
           nx_draw_text_layout(ctx,layouts[i],(nx_point){12,10+55*(float)i},(nx_color){1,1,1,1},65536)!=NX_OK)goto done;
    }
    nx_progress_desc progress={1,0,{12,38,440,8},{labels[0],strlen(labels[0])},.4};
    nx_activity_desc activity={2,0,{12,93,440,8},{labels[1],strlen(labels[1])},
        {explanation,strlen(explanation)},.5};
    nx_semantic_desc unknown={3,0,NX_ROLE_STATUS,{12,120,440,40},
        {labels[2],strlen(labels[2])},{0},{0},NX_SEM_READONLY,0};
    if(nx_progress(ctx,&progress)!=NX_OK || nx_activity(ctx,&activity)!=NX_OK ||
       nx_semantics_add(ctx,&unknown,NULL)!=NX_OK || nx_end_frame(ctx)!=NX_OK)goto done;
    uint32_t index; nx_semantic_view view;
    if(nx_semantics_find(ctx,2,&index)!=NX_OK || nx_semantics_get(ctx,index,&view)!=NX_OK ||
       view.value.length || !(view.state&NX_SEM_BUSY))goto done;
    if(nx_semantics_find(ctx,3,&index)!=NX_OK || nx_semantics_get(ctx,index,&view)!=NX_OK ||
       view.value.length || (view.state&NX_SEM_BUSY))goto done;
    const nx_command *commands=NULL;size_t count=0;
    pixels=malloc(480*180*4);
    if(!pixels || nx_frame_commands(ctx,&commands,&count)!=NX_OK ||
       nx_headless_render(commands,count,pixels,480,180,480*4,(nx_color){.04f,.05f,.07f,1})!=NX_OK ||
       !(file=fopen(argv[2],"wb")))goto done;
    if(fprintf(file,"P6\n480 180\n255\n")<0)goto done;
    for(size_t i=0;i<480*180;++i)if(fwrite(pixels+4*i,1,3,file)!=3)goto done;
    if(fclose(file)){file=NULL;goto done;}file=NULL;okay=1;
done:
    if(file)fclose(file);
    for(unsigned i=0;i<3;++i)if(layouts[i])nx_text_layout_destroy(layouts[i]);
    if(ctx)nx_context_destroy(ctx);
    if(fonts)nx_font_collection_destroy(fonts);
    if(font)nx_font_close(font);
    free(font_bytes);free(pixels);
    if(okay)puts("Observed fraction, unquantified activity and unknown result stay distinct");
    return okay?0:1;
}
