#include "view.h"
#include <nexaui/software.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Private-test consumer: application model owns selection and copy content. */
static int width(uint32_t c){return c==0x754c?2:c==0x301?0:1;}
static nx_result mark(void *u,uint32_t b,uint32_t m,unsigned i,float *x,float *y) {
    (void)u;(void)b;(void)m;(void)i;*x=0;*y=0;return NX_OK;
}
static unsigned char *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);if(n<=0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}fclose(f);*size=(size_t)n;return p;
}
int main(int argc,char **argv) {
    int okay=0,shown=0,dirty=1,done=0,focused=0;uint64_t revision=1;size_t size=0;
    unsigned char *bytes=NULL;nx_font *font=NULL;nx_window *window=NULL;nx_software_view *renderer=NULL;
    struct ae_terminal *model=NULL;aetheri_terminal_view *view=NULL;
    if(argc!=2 || !(bytes=read_file(argv[1],&size)) || nx_font_open(bytes,size,NULL,&font)!=NX_OK ||
       !(model=malloc(sizeof(*model))))goto cleanup;
    ae_terminal_init(model,16,3,width);
    const char content[]="A\xe7\x95\x8c" "e\xcc\x81\r\nSecond";
    ae_terminal_feed(model,content,sizeof(content)-1);
    aetheri_terminal_view_desc vd={.model=model,.regular=font,.bold=font,.text_identity=123,
        .cell_width=12,.cell_height=22,.baseline=17,.pixels_per_em=16,.mark_position=mark};
    nx_window_desc wd={"NexaUI terminal copy test",640,360,0,{0}};
    if(aetheri_terminal_view_create(&vd,&view)!=NX_OK || aetheri_terminal_view_frame(view,revision,1)!=NX_OK ||
       nx_window_create(&wd,&window)!=NX_OK || nx_software_view_open(window,&renderer)!=NX_OK)goto cleanup;
    okay=1;
    while(okay && !done) {
        if(nx_window_poll(window,50)!=NX_OK){okay=0;break;}
        nx_window_info info;if(nx_window_get_info(window,&info)!=NX_OK){okay=0;break;}
        if(info.should_close)break;
        nx_event event;int available=0;
        do {
            if(nx_window_next_event(window,&event,&available)!=NX_OK){okay=0;break;}
            if(!available)break;
            if(event.type==NX_EVENT_FOCUS)focused=!!event.focused;
            if(event.type==NX_EVENT_RESIZE)dirty=1;
            if(event.type==NX_EVENT_KEY_DOWN && event.key==NX_KEY_ESCAPE && !model->selection.count){done=1;break;}
            if(event.type==NX_EVENT_KEY_DOWN && event.key==NX_KEY_C && !event.repeat && focused &&
               (event.modifiers&(NX_MOD_CONTROL|NX_MOD_SHIFT|NX_MOD_ALT|NX_MOD_SUPER))==(NX_MOD_CONTROL|NX_MOD_SHIFT)) {
                size_t needed=ae_terminal_selection_size(model);
                if(needed<2){puts("NATIVE_COPY_EMPTY");fflush(stdout);continue;}
                char *text=malloc(needed);
                if(!text || !ae_terminal_selection_copy(model,text,needed) ||
                   nx_window_clipboard_set_text(window,text,needed-1)!=NX_OK){free(text);okay=0;break;}
                free(text);puts("NATIVE_COPY_PUBLISHED");fflush(stdout);continue;
            }
            int handled=0,changed=0;
            if(aetheri_terminal_view_selection_event(view,revision,&event,&handled,&changed)!=NX_OK){okay=0;break;}
            if(changed){++revision;if(aetheri_terminal_view_frame(view,revision,info.scale)!=NX_OK){okay=0;break;}
                dirty=1;if(event.type==NX_EVENT_KEY_DOWN && event.key==NX_KEY_A){puts("NATIVE_COPY_SELECTION");fflush(stdout);}
                if(event.type==NX_EVENT_KEY_DOWN && event.key==NX_KEY_ESCAPE){puts("NATIVE_COPY_CLEARED");fflush(stdout);}}
        } while(available);
        if(!okay || done)break;
        if(!dirty)continue;
        nx_context *context=aetheri_terminal_view_context(view);const nx_command *commands=NULL;size_t count=0;
        nx_software_view_outcome outcome;
        if(nx_frame_commands(context,&commands,&count)!=NX_OK ||
           nx_software_view_render(renderer,commands,count,(nx_color){0,0,0,1},&outcome)!=NX_OK){okay=0;break;}
        nx_result ax=nx_window_publish_semantics(window,context);
        if(ax!=NX_OK && ax!=NX_ERROR_UNSUPPORTED){okay=0;break;}
        dirty=outcome!=NX_SOFTWARE_VIEW_READY;
        if(outcome==NX_SOFTWARE_VIEW_READY && !shown){puts("NATIVE_COPY_READY");fflush(stdout);shown=1;}
    }
cleanup:
    if(renderer && nx_software_view_close(&renderer)!=NX_OK)okay=0;
    if(window)nx_window_destroy(window);
    if(view)aetheri_terminal_view_destroy(view);
    free(model);if(font)nx_font_close(font);free(bytes);
    if(!okay)fputs("Native copy test consumer failed\n",stderr);
    return okay && done?0:1;
}
