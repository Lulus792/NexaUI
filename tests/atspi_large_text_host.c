#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include "fixture_font.h"
#include "accessibility.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct fixture {
    nx_window *window; nx_context *context; nx_font *font;
    nx_font_collection *fonts; nx_text_field *field; nx_numeric_field *number;
    int disabled, omitted, apply_count, apply_valid, apply_disabled;
    int clipboard_mode;
    char copied[64];
} fixture;
static nx_result clipboard_write(void *user,const char *bytes,size_t length) {
    fixture *f=user;
    if(length>=sizeof(f->copied) || f->clipboard_mode==1)return NX_ERROR_STATE;
    memcpy(f->copied,bytes,length);f->copied[length]=0;
    if(f->clipboard_mode==2 && nx_text_field_set_text(f->field,"replacement",11)!=NX_OK)return NX_ERROR_STATE;
    if(f->clipboard_mode==3 && nx_window_publish_semantics(f->window,NULL)!=NX_OK)return NX_ERROR_STATE;
    return NX_OK;
}
static nx_result clipboard_read(void *user,char *bytes,size_t capacity,size_t *out) {
    fixture *f=user;size_t length=strlen(f->copied);*out=length;
    if(f->clipboard_mode==1)return NX_ERROR_STATE;
    if(!bytes)return NX_OK;
    if(capacity<=length)return NX_ERROR_LIMIT;
    memcpy(bytes,f->copied,length+1u);return NX_OK;
}
static int draw(fixture *f) {
    nx_semantic_desc root={0}; root.id=1; root.role=NX_ROLE_WINDOW;
    root.name=(nx_utf8_span){"Large text fixture",18}; root.bounds=(nx_rect){0,0,320,120};
    nx_text_field_desc d={0}; d.id=2; d.parent_id=1; d.name=(nx_utf8_span){"Large text field",16};
    d.bounds=(nx_rect){8,8,300,32}; d.fonts=f->fonts; d.pixels_per_em=16; d.glyph_cache_bytes=65536;
    d.disabled=f->disabled;
    d.clipboard=(nx_text_clipboard){f,clipboard_read,clipboard_write};
    nx_numeric_field_desc number={0}; number.id=3; number.parent_id=1;
    number.name=(nx_utf8_span){"Numeric field",13}; number.bounds=(nx_rect){8,48,300,32};
    number.fonts=f->fonts; number.pixels_per_em=16; number.glyph_cache_bytes=65536;
    number.minimum=-10000; number.maximum=10000;
    nx_control_desc apply={0}; apply.id=4; apply.parent_id=1;
    apply.name=(nx_utf8_span){"Apply number",12}; apply.bounds=(nx_rect){8,84,120,28};
    apply.disabled=f->apply_disabled;
    nx_interaction_state state; int valid,submitted;
    if(nx_begin_frame(f->context,320,120,0)!=NX_OK ||
       nx_semantics_add(f->context,&root,NULL)!=NX_OK ||
       (!f->omitted && nx_text_field_widget(f->context,&d,f->field,&state,&valid,&submitted)!=NX_OK) ||
       nx_numeric_field_widget(f->context,&number,f->number,&state,&valid)!=NX_OK ||
       nx_button(f->context,&apply,&state)!=NX_OK) return 0;
    int clicked=state.clicked;
    if(nx_end_frame(f->context)!=NX_OK)return 0;
    if(clicked) {
        ++f->apply_count;
        if(nx_numeric_field_commit(f->number,number.minimum,number.maximum,&f->apply_valid)!=NX_OK)return 0;
    }
    return 1;
}
static int frame(fixture *f) {return draw(f) && nx_window_publish_semantics(f->window,f->context)==NX_OK;}
int main(void) {
    fixture f={0}; uint8_t font_bytes[512]; font_fixture(font_bytes);
    nx_window_desc desc={"Large text fixture",320,120,0,{0}};
    int okay=0; char *large=NULL;
    const size_t side=65534u, length=2u*side+6u;
    large=malloc(length+1u); if (!large) goto done;
    large[0]='A'; memset(large+1,'x',side); memcpy(large+1+side,"\xf0\x9f\x8c\x8d",4);
    memset(large+1+side+4,'x',side); large[length-1]='Z'; large[length]=0;
    if (nx_window_create(&desc,&f.window)!=NX_OK || nx_context_create(NULL,&f.context)!=NX_OK ||
        nx_font_open(font_bytes,sizeof(font_bytes),NULL,&f.font)!=NX_OK ||
        nx_font_collection_create(NULL,&f.fonts)!=NX_OK || nx_font_collection_add(f.fonts,f.font)!=NX_OK ||
        nx_text_field_create("AZ",2,65536,&f.field)!=NX_OK ||
        nx_numeric_field_create(1234,1024,&f.number)!=NX_OK || !frame(&f)) goto done;
    setvbuf(stdout,NULL,_IOLBF,0); puts("READY");
    if (fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL)|O_NONBLOCK)<0) goto done;
    char input[64]; size_t used=0; int paused=0;
    for (;;) {
        if (nx_window_poll(f.window,20)!=NX_OK) goto done;
        size_t routed=0;
        if (!paused && (nx_window_route_events(f.window,f.context,&routed)!=NX_OK || (routed && !frame(&f)))) goto done;
        ssize_t n=read(STDIN_FILENO,input+used,sizeof(input)-used-1u);
        if (!n) goto done;
        if (n<0 && errno!=EAGAIN && errno!=EINTR) goto done;
        if (n>0) used+=(size_t)n;
        input[used]=0; char *end=memchr(input,'\n',used); if (!end) continue;
        *end=0;
        if (!strcmp(input,"quit")) { okay=1; break; }
        if (!strcmp(input,"numeric-invalid")) {
            double value=0; nx_utf8_span draft;
            if(nx_numeric_field_value(f.number,&value)!=NX_OK || value!=1234 ||
               nx_numeric_field_text(f.number,&draft)!=NX_OK || draft.length!=3 || memcmp(draft.data,"bad",3))goto done;
            puts("ACK numeric-invalid");
        } else if (!strcmp(input,"numeric-apply-disable")) {
            f.apply_disabled=1;if(!frame(&f))goto done;
            puts("ACK numeric-apply-disable");
        } else if (!strcmp(input,"numeric-apply-enable")) {
            f.apply_disabled=0;if(!frame(&f))goto done;
            puts("ACK numeric-apply-enable");
        } else if (!strcmp(input,"numeric-apply-invalid")) {
            double value=0;
            if(f.apply_count!=1 || f.apply_valid || nx_numeric_field_value(f.number,&value)!=NX_OK || value!=1234)goto done;
            puts("ACK numeric-apply-invalid");
        } else if (!strcmp(input,"numeric-apply-valid")) {
            double value=0;
            if(f.apply_count!=2 || !f.apply_valid || nx_numeric_field_value(f.number,&value)!=NX_OK || value!=42)goto done;
            puts("ACK numeric-apply-valid");
        } else if (!strcmp(input,"numeric-enter")) {
            nx_event event={0};event.type=NX_EVENT_TEXT_COMMAND;event.text_command=NX_TEXT_COMMAND_NEWLINE;
            double value=0;
            if(nx_focus_set(f.context,3)!=NX_OK || nx_context_queue_event(f.context,&event)!=NX_OK || !frame(&f) ||
               nx_numeric_field_value(f.number,&value)!=NX_OK || value!=42)goto done;
            puts("ACK numeric-enter");
        } else if (!strcmp(input,"large")) {
            if (nx_text_field_set_text(f.field,large,length)!=NX_OK || !frame(&f)) goto done;
            puts("ACK large");
        } else if (!strcmp(input,"short")) {
            if (nx_text_field_set_text(f.field,"AZ",2)!=NX_OK || !frame(&f)) goto done;
            puts("ACK short");
        } else if (!strcmp(input,"undo")) {
            nx_event event={0};event.type=NX_EVENT_TEXT_COMMAND;event.text_command=NX_TEXT_COMMAND_UNDO;
            if(nx_focus_set(f.context,2)!=NX_OK || nx_context_queue_event(f.context,&event)!=NX_OK || !frame(&f))goto done;
            puts("ACK undo");
        } else if (!strcmp(input,"local")) {
            nx_event event={0};event.type=NX_EVENT_TEXT;event.scalar='Q';
            if(nx_focus_set(f.context,2)!=NX_OK || nx_context_queue_event(f.context,&event)!=NX_OK || !frame(&f))goto done;
            puts("ACK local");
        } else if (!strcmp(input,"clip-check")) {
            if(strcmp(f.copied,"\xc3\xa9\xf0\x9f\x8c\x8d"))goto done;
            puts("ACK clip-check");
        } else if (!strcmp(input,"clipfail")) {f.clipboard_mode=1;puts("ACK clipfail");}
        else if (!strcmp(input,"clipreplace")) {f.clipboard_mode=2;puts("ACK clipreplace");}
        else if (!strcmp(input,"clipdetach")) {f.clipboard_mode=3;puts("ACK clipdetach");}
        else if (!strcmp(input,"clipok")) {f.clipboard_mode=0;puts("ACK clipok");}
        else if (!strcmp(input,"pause")) {paused=1;puts("ACK pause");}
        else if (!strcmp(input,"resume")) {paused=0;puts("ACK resume");}
        else if (!strcmp(input,"disable")) {f.disabled=1;puts("ACK disable");}
        else if (!strcmp(input,"enable")) {f.disabled=0;puts("ACK enable");}
        else if (!strcmp(input,"omit")) {f.omitted=1;puts("ACK omit");}
        else if (!strcmp(input,"show")) {f.omitted=0;puts("ACK show");}
        else if (!strcmp(input,"readonly")) {
            if(nx_text_field_set_read_only(f.field,1)!=NX_OK)goto done;
            puts("ACK readonly");
        } else if (!strcmp(input,"writable")) {
            if(nx_text_field_set_read_only(f.field,0)!=NX_OK)goto done;
            puts("ACK writable");
        }
        else if (!strcmp(input,"route")) {
            if(nx_window_route_events(f.window,f.context,&routed)!=NX_OK)goto done;
            puts("ACK route");
        }
        else if (!strcmp(input,"disconnect")) {
            if(nx_window_linux_ax_test_disconnect(f.window)!=NX_OK || nx_window_poll(f.window,0)!=NX_OK)goto done;
            puts("ACK disconnect");
        } else if (!strcmp(input,"offline-step")) {
            if(!draw(&f))goto done;
            puts("ACK offline-step");
        } else if (!strcmp(input,"rebind")) {
            nx_context *old=f.context,*fresh=NULL;
            if(nx_context_create(NULL,&fresh)!=NX_OK)goto done;
            f.context=fresh;
            if(!frame(&f))goto done;
            f.context=old;
            if(!draw(&f))goto done;
            f.context=fresh;nx_context_destroy(old);
            if(!frame(&f))goto done;
            puts("ACK rebind");
        } else if (!strcmp(input,"reconnect")) {
            nx_result result=NX_ERROR_UNSUPPORTED;
            for(unsigned i=0;i<200 && result==NX_ERROR_UNSUPPORTED;++i) {
                if(nx_window_poll(f.window,20)!=NX_OK)goto done;
                result=nx_window_publish_semantics(f.window,f.context);
            }
            if(result!=NX_OK)goto done;
            puts("ACK reconnect");
        }
        else if (!strcmp(input,"step")) {
            if(nx_window_route_events(f.window,f.context,&routed)!=NX_OK || !frame(&f))goto done;
            puts("ACK step");
        } else if (!strcmp(input,"detach")) {
            if(nx_window_publish_semantics(f.window,NULL)!=NX_OK)goto done;
            puts("ACK detach");
        } else goto done;
        size_t consumed=(size_t)(end-input)+1u; used-=consumed; memmove(input,input+consumed,used);
    }
done:
    if (f.window) nx_window_publish_semantics(f.window,NULL);
    nx_numeric_field_destroy(f.number); nx_text_field_destroy(f.field); nx_context_destroy(f.context);
    nx_font_collection_destroy(f.fonts); nx_font_close(f.font); nx_window_destroy(f.window);
    free(large); return okay?0:1;
}
