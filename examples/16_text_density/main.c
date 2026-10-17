#include <nexaui/nexaui.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TRY(x) do{nx_result r=(x);if(r!=NX_OK){fprintf(stderr,"line %d: result %d\n",__LINE__,r);goto done;}}while(0)
int main(int argc,char **argv){
    if(argc!=3){fprintf(stderr,"usage: nexaui_text_density FONT.ttf OUTPUT.ppm\n");return 2;}
    int status=1;FILE *input=fopen(argv[1],"rb"),*output=NULL;
    uint8_t *font_bytes=NULL,*pixels=NULL;nx_font *font=NULL;nx_font_collection *fonts=NULL;
    nx_text_layout *layout=NULL,*titles[2]={0};nx_text_stamp *stamps[2]={0};nx_context *ui=NULL;
    nx_command *scaled=NULL;long length=-1;
    if(input&&!fseek(input,0,SEEK_END))length=ftell(input);
    if(length<=0||length>64*1024*1024||fseek(input,0,SEEK_SET))goto done;
    font_bytes=malloc((size_t)length);
    if(!font_bytes||fread(font_bytes,1,(size_t)length,input)!=(size_t)length)goto done;
    TRY(nx_font_open(font_bytes,(size_t)length,NULL,&font));
    TRY(nx_font_collection_create(NULL,&fonts));TRY(nx_font_collection_add(fonts,font));
    const char *sample=u8"NexaCloud\nGröße · Übertragung 47%\nWLAN verbunden";
    TRY(nx_text_layout_create(fonts,sample,strlen(sample),NX_BIDI_LTR,18.5,230,&layout));
    const char *labels[]={"1x mask displayed at 2x","2x mask displayed at 2x"};
    for(unsigned i=0;i<2;++i){
        TRY(nx_text_stamp_create_scaled(layout,(float)(i+1),2u*1024u*1024u,NULL,&stamps[i]));
        TRY(nx_text_layout_create(fonts,labels[i],strlen(labels[i]),NX_BIDI_LTR,13,230,&titles[i]));
        nx_bitmap_view view;nx_point offset;nx_rect bounds;
        TRY(nx_text_stamp_view(stamps[i],&view,&offset));TRY(nx_text_stamp_bounds(stamps[i],&bounds));
        printf("density %u: physical %ux%u, logical ink %.6fx%.6f; layout %.6fx%.6f, %zu lines\n",
            i+1,view.width,view.height,bounds.width,bounds.height,
            nx_text_layout_width(layout),nx_text_layout_height(layout),nx_text_layout_line_count(layout));
    }
    TRY(nx_context_create(NULL,&ui));TRY(nx_begin_frame(ui,512,160,0));
    for(unsigned i=0;i<2;++i){float x=12+i*256.f;
        TRY(nx_draw_text_layout_scaled(ui,titles[i],(nx_point){x,10},(nx_color){.3f,.8f,1,1},2,2u*1024u*1024u));
        TRY(nx_push_clip(ui,(nx_rect){x,36,236,118}));
        TRY(nx_draw_text_stamp(ui,stamps[i],(nx_point){x,40},(nx_color){.92f,.96f,1,1}));
        TRY(nx_pop_clip(ui));
    }TRY(nx_end_frame(ui));
    /* Independent cached pixels and frame-owned direct masks survive sources. */
    nx_text_layout_destroy(layout);layout=NULL;
    for(unsigned i=0;i<2;++i){nx_text_layout_destroy(titles[i]);titles[i]=NULL;}
    nx_font_collection_destroy(fonts);fonts=NULL;nx_font_close(font);font=NULL;
    memset(font_bytes,0,(size_t)length);free(font_bytes);font_bytes=NULL;
    const nx_command *commands;size_t count;TRY(nx_frame_commands(ui,&commands,&count));
    scaled=malloc(count*sizeof(*scaled));pixels=malloc(1024u*320u*4u);if(!scaled||!pixels)goto done;
    for(size_t i=0;i<count;++i){scaled[i]=commands[i];scaled[i].rect.x*=2;scaled[i].rect.y*=2;
        scaled[i].rect.width*=2;scaled[i].rect.height*=2;}
    TRY(nx_headless_render(scaled,count,pixels,1024,320,4096,(nx_color){.015f,.035f,.07f,1}));
    output=fopen(argv[2],"wb");if(!output||fprintf(output,"P6\n1024 320\n255\n")<0)goto done;
    for(size_t i=0;i<1024u*320u;++i)if(fwrite(pixels+4*i,1,3,output)!=3)goto done;
    status=0;
done:
    if(output&&fclose(output))status=1;
    if(input)fclose(input);
    nx_context_destroy(ui);for(unsigned i=0;i<2;++i){nx_text_stamp_destroy(stamps[i]);nx_text_layout_destroy(titles[i]);}
    nx_text_layout_destroy(layout);nx_font_collection_destroy(fonts);nx_font_close(font);
    free(font_bytes);free(pixels);free(scaled);return status;
}
