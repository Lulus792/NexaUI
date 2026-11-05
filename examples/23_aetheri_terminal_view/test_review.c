#include "paste_review.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}} while(0)
static int width(uint32_t c){(void)c;return 1;}
static int capture(const char *path,const unsigned char *pixels) {
    if(!path)return 1;
    FILE *f=fopen(path,"wb");if(!f)return 0;
    unsigned char header[54]={0};unsigned total=54+640u*360u*3u;
    header[0]='B';header[1]='M';for(unsigned i=0;i<4;++i)header[2+i]=(unsigned char)(total>>(8*i));
    header[10]=54;header[14]=40;header[18]=128;header[19]=2;header[22]=104;header[23]=1;
    header[26]=1;header[28]=24;
    if(fwrite(header,1,sizeof(header),f)!=sizeof(header)){fclose(f);return 0;}
    for(int y=359;y>=0;--y)for(unsigned x=0;x<640;++x){const unsigned char *p=pixels+((size_t)y*640+x)*4;
        unsigned char bgr[3]={p[2],p[1],p[0]};if(fwrite(bgr,1,3,f)!=3){fclose(f);return 0;}}
    return fclose(f)==0;
}
static unsigned char *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);if(n<=0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}fclose(f);*size=(size_t)n;return p;
}
static int key(nx_context *ctx,nx_key k,int repeat) {
    nx_event event={.type=NX_EVENT_KEY_DOWN,.key=k,.repeat=repeat};
    if(nx_context_queue_event(ctx,&event)!=NX_OK)return 0;
    event.type=NX_EVENT_KEY_UP;event.repeat=0;return nx_context_queue_event(ctx,&event)==NX_OK;
}
static int frame(terminal_paste_review *r,terminal_paste_guard guard,enum terminal_review_result want) {
    enum terminal_review_result result;nx_utf8_span bytes;
    CHECK(terminal_paste_review_frame(r,guard,&result,&bytes)==NX_OK && result==want);return 0;
}
int main(int argc,char **argv) {
    CHECK(argc==2);size_t size=0;unsigned char *font_bytes=read_file(argv[1],&size);CHECK(font_bytes);
    nx_font *font=NULL;nx_font_collection *fonts=NULL;CHECK(nx_font_open(font_bytes,size,NULL,&font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    terminal_paste_guard guard={11,1,1,1};terminal_paste_flow flow={0};flow.guard=guard;
    char input[4096];memset(input,'x',sizeof(input));for(size_t i=31;i<sizeof(input);i+=32)input[i]='\n';
    CHECK(ae_terminal_paste_set(&flow.paste,input,sizeof(input))==AE_TERM_PASTE_OK && flow.paste.review);flow.state=2;
    terminal_paste_review *review=NULL;CHECK(terminal_paste_review_create(&flow,fonts,width,&review)==NX_OK);
    nx_context *ctx=terminal_paste_review_context(review);CHECK(!frame(review,guard,TERMINAL_REVIEW_OPEN));
    CHECK(nx_focus_id(ctx)==10);uint32_t index=0;nx_semantic_view view;
    CHECK(nx_semantics_find(ctx,3,&index)==NX_OK && nx_semantics_get(ctx,index,&view)==NX_OK && view.role==NX_ROLE_DIALOG);
    CHECK(nx_semantics_find(ctx,100,&index)==NX_OK && nx_semantics_get(ctx,index,&view)==NX_OK && view.value.length==16);
    const nx_command *commands=NULL;size_t count=0;CHECK(nx_frame_commands(ctx,&commands,&count)==NX_OK && count>5);
    unsigned char *pixels=malloc(640u*360u*4u);CHECK(pixels);
    CHECK(nx_headless_render(commands,count,pixels,640,360,640u*4u,(nx_color){0,0,0,1})==NX_OK);
    CHECK(capture(getenv("NEXAUI_REVIEW_BMP"),pixels));free(pixels);
    CHECK(key(ctx,NX_KEY_END,0) && !frame(review,guard,TERMINAL_REVIEW_OPEN));
    CHECK(nx_semantics_find(ctx,5,&index)==NX_OK && nx_semantics_get(ctx,index,&view)==NX_OK &&
          view.value.length>10 && memcmp(view.value.data,"Rows 1-",7));
    CHECK(nx_semantics_find(ctx,12,&index)==NX_OK && nx_semantics_get(ctx,index,&view)==NX_OK && (view.state&NX_SEM_DISABLED));
    CHECK(key(ctx,NX_KEY_HOME,0) && !frame(review,guard,TERMINAL_REVIEW_OPEN));
    CHECK(nx_semantics_find(ctx,11,&index)==NX_OK && nx_semantics_get(ctx,index,&view)==NX_OK && (view.state&NX_SEM_DISABLED));
    CHECK(nx_semantics_request_action(ctx,12,NX_ACTION_PRESS)==NX_OK && !frame(review,guard,TERMINAL_REVIEW_OPEN));
    CHECK(!frame(review,guard,TERMINAL_REVIEW_OPEN));
    CHECK(nx_semantics_find(ctx,5,&index)==NX_OK && nx_semantics_get(ctx,index,&view)==NX_OK &&
          view.value.length>10 && memcmp(view.value.data,"Rows 1-",7));
    CHECK(key(ctx,NX_KEY_ENTER,1) && !frame(review,guard,TERMINAL_REVIEW_OPEN));
    CHECK(key(ctx,NX_KEY_ESCAPE,0) && !frame(review,guard,TERMINAL_REVIEW_CANCELLED));
    CHECK(!flow.state && !flow.paste.size);terminal_paste_review_destroy(review);
    flow.guard=guard;CHECK(ae_terminal_paste_set(&flow.paste,"a\nb",3)==AE_TERM_PASTE_OK);flow.state=2;
    CHECK(terminal_paste_review_create(&flow,fonts,width,&review)==NX_OK);ctx=terminal_paste_review_context(review);
    CHECK(!frame(review,guard,TERMINAL_REVIEW_OPEN) && nx_focus_set(ctx,13)==NX_OK);
    CHECK(key(ctx,NX_KEY_ENTER,1) && !frame(review,guard,TERMINAL_REVIEW_OPEN));
    CHECK(key(ctx,NX_KEY_ENTER,0));enum terminal_review_result result;nx_utf8_span confirmed;
    CHECK(terminal_paste_review_frame(review,guard,&result,&confirmed)==NX_OK && result==TERMINAL_REVIEW_CONFIRMED &&
          confirmed.length==3 && !memcmp(confirmed.data,"a\nb",3));
    CHECK(terminal_paste_review_frame(review,guard,&result,&confirmed)==NX_ERROR_STATE);
    terminal_paste_review_destroy(review);terminal_paste_reset(&flow);
    flow.guard=guard;CHECK(ae_terminal_paste_set(&flow.paste,"a\nb",3)==AE_TERM_PASTE_OK);flow.state=2;
    CHECK(terminal_paste_review_create(&flow,fonts,width,&review)==NX_OK && !frame(review,guard,TERMINAL_REVIEW_OPEN));
    ++guard.focus_epoch;CHECK(!frame(review,guard,TERMINAL_REVIEW_CANCELLED) && !flow.state);
    terminal_paste_review_destroy(review);
    flow.guard=guard;CHECK(ae_terminal_paste_set(&flow.paste,"a\nb",3)==AE_TERM_PASTE_OK);flow.state=2;
    CHECK(terminal_paste_review_create(&flow,fonts,width,&review)==NX_OK);
    terminal_paste_review_destroy(review);CHECK(!flow.state && !flow.paste.size);
    nx_font_collection_destroy(fonts);nx_font_close(font);free(font_bytes);
    puts("Paste review navigation, modal focus and guarded decisions passed");return 0;
}
