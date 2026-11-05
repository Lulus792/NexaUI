#include "paste_flow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}} while(0)
typedef struct fake {
    uint64_t token;const char *source;size_t length;int pending,cancels,releases,sensitive;
    nx_result result;
} fake;
static nx_result begin(void *u,size_t limit,uint32_t timeout,int sensitive,uint64_t *token) {
    fake *f=u;if(limit!=AE_TERM_PASTE_BYTES || !timeout || !sensitive)return NX_ERROR_ARGUMENT;
    f->sensitive=sensitive;*token=++f->token;return NX_OK;
}
static nx_result take(void *u,uint64_t token,char **bytes,size_t *length,int *ready) {
    fake *f=u;*bytes=NULL;*length=0;*ready=0;if(token!=f->token)return NX_ERROR_STATE;
    if(f->pending)return NX_OK;
    *ready=1;if(f->result!=NX_OK)return f->result;
    if(f->length){*bytes=malloc(f->length);if(!*bytes)return NX_ERROR_MEMORY;memcpy(*bytes,f->source,f->length);}
    *length=f->length;return NX_OK;
}
static nx_result cancel(void *u,uint64_t token){fake *f=u;if(token!=f->token)return NX_ERROR_STATE;++f->cancels;return NX_OK;}
static void release(void *u,char *bytes,size_t length){fake *f=u;++f->releases;if(bytes){memset(bytes,0,length);free(bytes);}}
static int one_width(uint32_t scalar){(void)scalar;return 1;}
int main(void) {
    fake peer={0};terminal_paste_transport t={&peer,begin,take,cancel,release};terminal_paste_flow f;
    terminal_paste_guard g={11,1,1,1};CHECK(terminal_paste_init(&f,t)==NX_OK);
    peer.source="first\r\nsecond\tline";peer.length=strlen(peer.source);peer.pending=1;
    CHECK(terminal_paste_begin(&f,g,1000)==NX_OK && peer.sensitive);
    CHECK(terminal_paste_begin(&f,g,1000)==NX_ERROR_STATE);
    int ready=1;enum ae_term_paste_result validation;
    CHECK(terminal_paste_poll(&f,g,&ready,&validation)==NX_OK && !ready);
    peer.pending=0;CHECK(terminal_paste_poll(&f,g,&ready,&validation)==NX_OK && ready && validation==AE_TERM_PASTE_OK);
    CHECK(peer.releases==1 && f.paste.review && f.paste.lines==2 && !strcmp(f.paste.text,"first\nsecond\tline"));
    char line[AE_TERM_PREVIEW_BYTES];size_t at=0;unsigned rows=0;
    while(at<f.paste.size){size_t next=ae_terminal_paste_preview(&f.paste,at,16,one_width,line);
        CHECK(next>at && line[0]);if(!rows)CHECK(strstr(line,"\\n"));at=next;++rows;}
    CHECK(at==f.paste.size && rows>=2);
    nx_utf8_span output={0};
    CHECK(terminal_paste_confirm(&f,g,0,0,&output)==NX_ERROR_ARGUMENT && !output.data);
    CHECK(terminal_paste_confirm(&f,g,1,1,&output)==NX_ERROR_ARGUMENT && !output.data);
    CHECK(terminal_paste_confirm(&f,g,1,0,&output)==NX_OK && output.length==17 && !memcmp(output.data,"first\nsecond\tline",17));
    CHECK(terminal_paste_confirm(&f,g,1,0,&output)==NX_ERROR_STATE);
    terminal_paste_reset(&f);CHECK(!f.paste.size && !f.state);
    peer.source="plain";peer.length=5;CHECK(terminal_paste_begin(&f,g,1000)==NX_OK);
    CHECK(terminal_paste_poll(&f,g,&ready,&validation)==NX_OK && ready && !f.paste.review);
    CHECK(terminal_paste_confirm(&f,g,0,0,&output)==NX_OK && output.length==5);
    terminal_paste_reset(&f);
    peer.source="bad\x1b";peer.length=4;CHECK(terminal_paste_begin(&f,g,1000)==NX_OK);
    CHECK(terminal_paste_poll(&f,g,&ready,&validation)==NX_ERROR_ARGUMENT && ready && validation==AE_TERM_PASTE_CONTROL && !f.state);
    peer.pending=1;CHECK(terminal_paste_begin(&f,g,1000)==NX_OK);
    terminal_paste_guard changed=g;changed.focus_epoch++;
    CHECK(terminal_paste_poll(&f,changed,&ready,&validation)==NX_ERROR_STATE && !ready && peer.cancels==1 && !f.state);
    peer.pending=0;peer.source="ok";peer.length=2;CHECK(terminal_paste_begin(&f,g,1000)==NX_OK);
    CHECK(terminal_paste_poll(&f,g,&ready,&validation)==NX_OK && ready);
    changed=g;changed.revision++;
    CHECK(terminal_paste_confirm(&f,changed,1,0,&output)==NX_ERROR_STATE && !output.data && !f.state);
    CHECK(terminal_paste_begin(&f,g,1000)==NX_OK);terminal_paste_reset(&f);terminal_paste_reset(&f);
    CHECK(peer.cancels==2);
    char full[AE_TERM_PASTE_BYTES];memset(full,'x',sizeof(full));full[2048]='\n';
    peer.source=full;peer.length=sizeof(full);CHECK(terminal_paste_begin(&f,g,1000)==NX_OK);
    CHECK(terminal_paste_poll(&f,g,&ready,&validation)==NX_OK && ready && f.paste.review && f.paste.size==sizeof(full));
    at=0;rows=0;while(at<f.paste.size){size_t next=ae_terminal_paste_preview(&f.paste,at,16,one_width,line);
        CHECK(next>at);at=next;++rows;}
    CHECK(at==sizeof(full) && rows>200);
    CHECK(terminal_paste_confirm(&f,g,1,0,&output)==NX_OK && output.length==sizeof(full) && !memcmp(output.data,full,sizeof(full)));
    terminal_paste_reset(&f);
    puts("App-owned paste validation, preview, stale guard and cancellation passed");return 0;
}
