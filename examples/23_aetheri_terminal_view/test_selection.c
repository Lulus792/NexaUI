#include "view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}} while(0)
static int width(uint32_t c){return c==0x754c?2:c==0x301?0:1;}
static nx_result mark(void *u,uint32_t b,uint32_t m,unsigned i,float *x,float *y){
    (void)u;(void)b;(void)m;(void)i;*x=0;*y=0;return NX_OK;
}
static unsigned char *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);if(n<=0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}fclose(f);*size=(size_t)n;return p;
}
/* Events traverse NexaUI's real frame queue, then the owner routes them. */
static int dispatch(aetheri_terminal_view *view,uint64_t *revision,nx_event event,int want_handled) {
    nx_context *context=aetheri_terminal_view_context(view);
    CHECK(nx_context_queue_event(context,&event)==NX_OK);
    CHECK(aetheri_terminal_view_frame(view,*revision,1)==NX_OK);
    const nx_event *events=NULL;size_t count=0;
    CHECK(nx_frame_events(context,&events,&count)==NX_OK && count==1);
    nx_event delivered=events[0];int handled=0,changed=0;
    CHECK(aetheri_terminal_view_selection_event(view,*revision,&delivered,&handled,&changed)==NX_OK);
    CHECK(handled==want_handled && (!changed || handled));
    if(changed) {
        const nx_cell_view *cells=NULL;size_t rows=0,cols=0;
        CHECK(aetheri_terminal_view_cells(view,*revision,&cells,&rows,&cols)==NX_ERROR_STATE);
        ++*revision;CHECK(aetheri_terminal_view_frame(view,*revision,1)==NX_OK);
    }
    return 0;
}
static int drag_batch(aetheri_terminal_view *view,uint64_t *revision) {
    nx_event events[3]={{.type=NX_EVENT_POINTER_DOWN,.button=NX_BUTTON_LEFT,.x=12,.y=5},
        {.type=NX_EVENT_POINTER_MOVE,.x=48,.y=5},
        {.type=NX_EVENT_POINTER_UP,.button=NX_BUTTON_LEFT,.x=48,.y=5}};
    nx_context *context=aetheri_terminal_view_context(view);
    for(size_t i=0;i<3;++i)CHECK(nx_context_queue_event(context,&events[i])==NX_OK);
    CHECK(aetheri_terminal_view_frame(view,*revision,1)==NX_OK);
    const nx_event *pending=NULL;size_t count=0;
    CHECK(nx_frame_events(context,&pending,&count)==NX_OK && count==3);
    memcpy(events,pending,sizeof(events));
    for(size_t i=0;i<3;++i) {
        int handled=0,changed=0;
        CHECK(aetheri_terminal_view_selection_event(view,*revision,&events[i],&handled,&changed)==NX_OK && handled && changed);
        ++*revision;CHECK(aetheri_terminal_view_frame(view,*revision,1)==NX_OK);
    }
    return 0;
}
int main(int argc,char **argv) {
    CHECK(argc==2);size_t size=0;unsigned char *bytes=read_file(argv[1],&size);CHECK(bytes);
    nx_font *font=NULL;CHECK(nx_font_open(bytes,size,NULL,&font)==NX_OK);
    struct ae_terminal *model=malloc(sizeof(*model));CHECK(model);ae_terminal_init(model,8,3,width);
    const char input[]="A\xe7\x95\x8c" "e\xcc\x81";ae_terminal_feed(model,input,sizeof(input)-1);
    aetheri_terminal_view_desc d={.model=model,.regular=font,.bold=font,.text_identity=123,
        .cell_width=12,.cell_height=22,.baseline=17,.pixels_per_em=16,.mark_position=mark};
    aetheri_terminal_view *view=NULL;uint64_t rev=1;
    CHECK(aetheri_terminal_view_create(&d,&view)==NX_OK && aetheri_terminal_view_frame(view,rev,1)==NX_OK);
    CHECK(!drag_batch(view,&rev));nx_event event;
    char copy[32];CHECK(ae_terminal_selection_size(model)==7 &&
        ae_terminal_selection_copy(model,copy,sizeof(copy)) && !strcmp(copy,"\xe7\x95\x8c" "e\xcc\x81"));
    nx_utf8_span text;size_t anchor=0,cursor=0;int caret=0;
    CHECK(aetheri_terminal_view_text(view,rev,&text,&anchor,&cursor,&caret)==NX_OK && caret && anchor==1 && cursor==7);
    event=(nx_event){.type=NX_EVENT_POINTER_DOWN,.button=NX_BUTTON_LEFT,.x=48,.y=5};
    CHECK(!dispatch(view,&rev,event,1));event=(nx_event){.type=NX_EVENT_POINTER_MOVE,.x=12,.y=5};
    CHECK(!dispatch(view,&rev,event,1));
    CHECK(aetheri_terminal_view_text(view,rev,&text,&anchor,&cursor,&caret)==NX_OK && anchor==7 && cursor==1);
    event=(nx_event){.type=NX_EVENT_POINTER_CANCEL};CHECK(!dispatch(view,&rev,event,0));
    event=(nx_event){.type=NX_EVENT_POINTER_MOVE,.x=90,.y=5};CHECK(!dispatch(view,&rev,event,0));
    CHECK(aetheri_terminal_view_text(view,rev,&text,&anchor,&cursor,&caret)==NX_OK && anchor==7 && cursor==1);
    event=(nx_event){.type=NX_EVENT_POINTER_DOWN,.button=NX_BUTTON_LEFT,.x=12,.y=5};
    CHECK(!dispatch(view,&rev,event,1));event=(nx_event){.type=NX_EVENT_FOCUS,.focused=0};
    CHECK(!dispatch(view,&rev,event,0));event=(nx_event){.type=NX_EVENT_POINTER_MOVE,.x=90,.y=5};
    CHECK(!dispatch(view,&rev,event,0));
    event=(nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_ESCAPE};CHECK(!dispatch(view,&rev,event,1));
    event=(nx_event){.type=NX_EVENT_KEY_DOWN_CONSUMED,.key=NX_KEY_A,.modifiers=NX_MOD_CONTROL|NX_MOD_SHIFT};
    CHECK(!dispatch(view,&rev,event,0));CHECK(!model->selection.count);
    event.type=NX_EVENT_KEY_DOWN;CHECK(!dispatch(view,&rev,event,1));
    CHECK(ae_terminal_selection_size(model)==8 && ae_terminal_selection_copy(model,copy,sizeof(copy)) && !strcmp(copy,input));
    event=(nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_LEFT,.modifiers=NX_MOD_CONTROL|NX_MOD_SHIFT};
    CHECK(!dispatch(view,&rev,event,1));
    CHECK(ae_terminal_selection_copy(model,copy,sizeof(copy)) && !strcmp(copy,"A\xe7\x95\x8c"));
    event=(nx_event){.type=NX_EVENT_POINTER_DOWN,.button=NX_BUTTON_LEFT,.x=12,.y=5};
    CHECK(!dispatch(view,&rev,event,1));ae_terminal_feed(model,"X",1);
    event=(nx_event){.type=NX_EVENT_POINTER_MOVE,.x=48,.y=5};int handled=1,changed=1;
    CHECK(aetheri_terminal_view_selection_event(view,rev,&event,&handled,&changed)==NX_ERROR_STATE && !handled && !changed);
    ++rev;CHECK(aetheri_terminal_view_frame(view,rev,1)==NX_OK);
    CHECK(!dispatch(view,&rev,event,0));
    aetheri_terminal_view_destroy(view);free(model);nx_font_close(font);free(bytes);return 0;
}
