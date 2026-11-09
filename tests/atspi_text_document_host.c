#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct fixture {
    nx_window *window;nx_context *context;
    char *bytes;size_t length,anchor,cursor;
    uint64_t identity,revision;
    int readonly,disabled,omitted,fail;
    unsigned applied;
} fixture;
static nx_text_document_state state(fixture *f) {
    return (nx_text_document_state){{f->bytes,f->length},f->identity,f->revision,
        f->anchor,f->cursor,NX_TEXT_DOCUMENT_SELECT|(f->readonly?0u:NX_TEXT_DOCUMENT_REPLACE),1};
}
static nx_result edit(void *user,const nx_text_document_request *r,nx_text_document_state *out) {
    fixture *f=user;
    if(f->fail) {f->fail=0;return NX_ERROR_MEMORY;}
    if(r->identity!=f->identity || r->revision!=f->revision || r->start_byte>r->end_byte || r->end_byte>f->length)return NX_ERROR_STATE;
    if(r->operation==NX_TEXT_DOCUMENT_SET_SELECTION) {
        f->anchor=r->start_byte;f->cursor=r->end_byte;
    } else if(r->operation==NX_TEXT_DOCUMENT_REPLACE_RANGE) {
        size_t next=f->length-(r->end_byte-r->start_byte)+r->value.length;
        if(next>NX_TEXT_DOCUMENT_MAX_BYTES)return NX_ERROR_LIMIT;
        char *bytes=malloc(next+1u);if(!bytes)return NX_ERROR_MEMORY;
        if(r->start_byte)memcpy(bytes,f->bytes,r->start_byte);
        if(r->value.length)memcpy(bytes+r->start_byte,r->value.data,r->value.length);
        if(f->length>r->end_byte)memcpy(bytes+r->start_byte+r->value.length,f->bytes+r->end_byte,f->length-r->end_byte);
        bytes[next]=0;free(f->bytes);f->bytes=bytes;f->length=next;
        f->anchor=f->cursor=r->start_byte+r->value.length;
    } else return NX_ERROR_UNSUPPORTED;
    ++f->revision;++f->applied;*out=state(f);return NX_OK;
}
static int frame(fixture *f) {
    nx_semantic_desc root={0};root.id=1;root.role=NX_ROLE_WINDOW;
    root.name=(nx_utf8_span){"External document fixture",25};root.bounds=(nx_rect){0,0,320,160};
    root.state=f->disabled?NX_SEM_DISABLED:0;
    nx_widget_desc w={0};w.semantic.id=2;w.semantic.parent_id=1;w.semantic.role=NX_ROLE_TEXT_FIELD;
    w.semantic.name=(nx_utf8_span){"External document",17};w.semantic.bounds=(nx_rect){8,8,300,140};
    w.semantic.state=f->readonly?NX_SEM_READONLY:0;w.semantic.actions=NX_ACTION_FOCUS|(f->readonly?0u:NX_ACTION_SET_VALUE);
    w.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE;nx_text_document_state s=state(f);nx_text_document_result result;
    return nx_begin_frame(f->context,320,160,0)==NX_OK && nx_semantics_add(f->context,&root,NULL)==NX_OK &&
        (f->omitted || nx_text_document_register(f->context,&w,&s,edit,f,&result)==NX_OK) &&
        nx_end_frame(f->context)==NX_OK && nx_window_publish_semantics(f->window,f->context)==NX_OK;
}
static int replace_local(fixture *f,const char *bytes,size_t length,int fresh) {
    char *copy=malloc(length+1u);if(!copy)return 0;
    if(length)memcpy(copy,bytes,length);
    copy[length]=0;free(f->bytes);f->bytes=copy;f->length=length;f->anchor=f->cursor=length;
    if(fresh){++f->identity;f->revision=1;}else ++f->revision;
    return 1;
}
int main(void) {
    fixture f={0};f.identity=41;f.revision=1;int okay=0,paused=0;setvbuf(stdout,NULL,_IOLBF,0);
    nx_window_desc d={"External document fixture",320,160,0,{0}};
    if(!replace_local(&f,"A\xc3\xa9\xf0\x9f\x8c\x8dZ",8,0) || nx_window_create(&d,&f.window)!=NX_OK ||
       nx_context_create(NULL,&f.context)!=NX_OK || !frame(&f))goto done;
    if(fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL)|O_NONBLOCK)<0)goto done;
    puts("READY");char input[80];size_t used=0;
    for(;;) {
        if(nx_window_poll(f.window,20)!=NX_OK)goto done;
        size_t routed=0;
        if(!paused && (nx_window_route_events(f.window,f.context,&routed)!=NX_OK || (routed && !frame(&f))))goto done;
        ssize_t n=read(STDIN_FILENO,input+used,sizeof(input)-used-1u);
        if(!n || (n<0 && errno!=EAGAIN && errno!=EINTR))goto done;
        if(n>0)used+=(size_t)n;
        input[used]=0;char *end=memchr(input,'\n',used);if(!end)continue;
        *end=0;int publish=1;
        if(!strcmp(input,"quit")){okay=1;break;}
        if(!strcmp(input,"pause")){paused=1;publish=0;}
        else if(!strcmp(input,"resume")){paused=0;publish=0;}
        else if(!strcmp(input,"local")){if(!replace_local(&f,"local",5,0))goto done;}
        else if(!strcmp(input,"readonly")){f.readonly=1;++f.revision;}
        else if(!strcmp(input,"writable")){f.readonly=0;++f.revision;}
        else if(!strcmp(input,"disable")){f.disabled=1;++f.revision;}
        else if(!strcmp(input,"enable")){f.disabled=0;++f.revision;}
        else if(!strcmp(input,"omit")){f.omitted=1;++f.revision;}
        else if(!strcmp(input,"restore")){f.omitted=0;++f.revision;}
        else if(!strcmp(input,"fail")){f.fail=1;publish=0;}
        else if(!strcmp(input,"large")) {
            char *large=malloc(NX_TEXT_DOCUMENT_MAX_BYTES);if(!large)goto done;
            memset(large,'x',NX_TEXT_DOCUMENT_MAX_BYTES);large[0]='A';large[NX_TEXT_DOCUMENT_MAX_BYTES-1u]='Z';
            int copied=replace_local(&f,large,NX_TEXT_DOCUMENT_MAX_BYTES,1);free(large);if(!copied)goto done;
            f.readonly=1;
        } else if(!strcmp(input,"detach")) {
            if(nx_window_publish_semantics(f.window,NULL)!=NX_OK)goto done;
            paused=1;publish=0;
        } else goto done;
        if(publish && !frame(&f))goto done;
        printf("ACK %s\n",input);size_t consumed=(size_t)(end-input)+1u;used-=consumed;memmove(input,input+consumed,used);
    }
done:
    if(f.window)nx_window_publish_semantics(f.window,NULL);
    nx_context_destroy(f.context);nx_window_destroy(f.window);free(f.bytes);
    if(!okay)fputs("External document host failed\n",stderr);
    return okay?0:1;
}
