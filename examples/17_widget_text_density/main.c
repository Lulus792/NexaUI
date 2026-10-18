#include <nexaui/nexaui.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TRY(x) do{nx_result r=(x);if(r!=NX_OK){fprintf(stderr,"line %d: result %d\n",__LINE__,r);goto done;}}while(0)
int main(int argc,char **argv) {
    if(argc!=3){fputs("usage: nexaui_widget_text_density FONT.ttf OUTPUT.ppm\n",stderr);return 2;}
    int status=1;FILE *input=fopen(argv[1],"rb"),*output=NULL;long length=-1;
    uint8_t *bytes=NULL,*pixels=NULL;nx_font *font=NULL;nx_font_collection *fonts=NULL;
    nx_context *ui[2]={0};nx_text_field *fields[2][2]={{0}};nx_numeric_field *numbers[2]={0};
    nx_text_layout *titles[2]={0};nx_command *combined=NULL;size_t total=0;
    if(input&&!fseek(input,0,SEEK_END))length=ftell(input);
    if(length<=0||length>64*1024*1024||fseek(input,0,SEEK_SET))goto done;
    bytes=malloc((size_t)length);if(!bytes||fread(bytes,1,(size_t)length,input)!=(size_t)length)goto done;
    TRY(nx_font_open(bytes,(size_t)length,NULL,&font));TRY(nx_font_collection_create(NULL,&fonts));TRY(nx_font_collection_add(fonts,font));
    const char *sample=u8"Größe · Übertragung 47%";
    for(unsigned panel=0;panel<2;++panel) {
        TRY(nx_context_create(NULL,&ui[panel]));TRY(nx_context_set_text_raster_scale(ui[panel],(float)(panel+1)));
        TRY(nx_text_field_create(sample,strlen(sample),4096,&fields[panel][0]));
        TRY(nx_text_field_create_password("sample password",15,&fields[panel][1]));
        TRY(nx_numeric_field_create(237.5,4096,&numbers[panel]));
        const char *title=panel?"2x widget masks at 2x output":"1x widget masks at 2x output";
        TRY(nx_text_layout_create(fonts,title,strlen(title),NX_BIDI_LTR,13,236,&titles[panel]));
        nx_theme theme;TRY(nx_theme_preset(NX_THEME_MIDNIGHT,&theme));TRY(nx_context_set_theme(ui[panel],&theme));
        nx_text_field_desc d={0};d.id=1;d.bounds=(nx_rect){10,38,236,38};d.name=(nx_utf8_span){"Transfer",8};
        d.fonts=fonts;d.pixels_per_em=18.5;d.glyph_cache_bytes=2u*1024u*1024u;
        nx_interaction_state interaction;int valid,submitted;
        TRY(nx_begin_frame(ui[panel],256,180,0));TRY(nx_text_field_widget(ui[panel],&d,fields[panel][0],&interaction,&valid,&submitted));TRY(nx_end_frame(ui[panel]));
        TRY(nx_focus_set(ui[panel],1));nx_event select={0};select.type=NX_EVENT_TEXT_COMMAND;select.text_command=NX_TEXT_COMMAND_SELECT_ALL;
        TRY(nx_context_queue_event(ui[panel],&select));TRY(nx_begin_frame(ui[panel],256,180,0));
        TRY(nx_draw_text_layout_scaled(ui[panel],titles[panel],(nx_point){10,10},(nx_color){.3f,.8f,1,1},2,2u*1024u*1024u));
        TRY(nx_text_field_widget(ui[panel],&d,fields[panel][0],&interaction,&valid,&submitted));
        d.id=2;d.name=(nx_utf8_span){"Password",8};d.bounds.y=86;d.password=1;
        TRY(nx_text_field_widget(ui[panel],&d,fields[panel][1],&interaction,&valid,&submitted));
        nx_numeric_field_desc n={0};n.id=3;n.name=(nx_utf8_span){"Storage",7};n.bounds=(nx_rect){10,134,236,36};
        n.fonts=fonts;n.pixels_per_em=18.5;n.glyph_cache_bytes=2u*1024u*1024u;n.maximum=1000;
        TRY(nx_numeric_field_widget(ui[panel],&n,numbers[panel],&interaction,&valid));TRY(nx_end_frame(ui[panel]));
        const nx_command *commands;size_t count;TRY(nx_frame_commands(ui[panel],&commands,&count));
        nx_command *grown=realloc(combined,(total+count)*sizeof(*combined));if(!grown)goto done;combined=grown;
        unsigned masks=0;size_t mask_pixels=0;
        for(size_t i=0;i<count;++i) {
            nx_command c=commands[i];masks+=c.type==NX_COMMAND_BITMAP;
            if(c.type==NX_COMMAND_BITMAP)mask_pixels+=(size_t)c.bitmap.width*c.bitmap.height;
            c.rect.x=(c.rect.x+panel*256.f)*2;c.rect.y*=2;c.rect.width*=2;c.rect.height*=2;c.thickness*=2;c.radius*=2;
            for(unsigned p=0;p<3;++p){c.points[p].x=(c.points[p].x+panel*256.f)*2;c.points[p].y*=2;}
            combined[total+i]=c;
        }total+=count;printf("panel %u: %u mask commands, %zu referenced mask pixels, logical ppem 18.5\n",panel+1,masks,mask_pixels);
    }
    /* All direct masks and semantic strings are owned by their contexts now. */
    for(unsigned p=0;p<2;++p){nx_text_layout_destroy(titles[p]);titles[p]=NULL;}
    nx_font_collection_destroy(fonts);fonts=NULL;nx_font_close(font);font=NULL;memset(bytes,0,(size_t)length);free(bytes);bytes=NULL;
    pixels=malloc(1024u*360u*4u);if(!pixels)goto done;
    TRY(nx_headless_render(combined,total,pixels,1024,360,4096,(nx_color){.015f,.035f,.07f,1}));
    output=fopen(argv[2],"wb");if(!output||fprintf(output,"P6\n1024 360\n255\n")<0)goto done;
    for(size_t i=0;i<1024u*360u;++i)if(fwrite(pixels+4*i,1,3,output)!=3)goto done;
    status=0;
done:
    if(output&&fclose(output))status=1;if(input)fclose(input);
    for(unsigned p=0;p<2;++p){for(unsigned f=0;f<2;++f)nx_text_field_destroy(fields[p][f]);nx_numeric_field_destroy(numbers[p]);nx_text_layout_destroy(titles[p]);nx_context_destroy(ui[p]);}
    nx_font_collection_destroy(fonts);nx_font_close(font);free(bytes);free(combined);free(pixels);return status;
}
