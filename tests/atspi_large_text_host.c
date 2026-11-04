#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include "fixture_font.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct fixture {
    nx_window *window; nx_context *context; nx_font *font;
    nx_font_collection *fonts; nx_text_field *field;
} fixture;
static int frame(fixture *f) {
    nx_semantic_desc root={0}; root.id=1; root.role=NX_ROLE_WINDOW;
    root.name=(nx_utf8_span){"Large text fixture",18}; root.bounds=(nx_rect){0,0,320,120};
    nx_text_field_desc d={0}; d.id=2; d.parent_id=1; d.name=(nx_utf8_span){"Large text field",16};
    d.bounds=(nx_rect){8,8,300,32}; d.fonts=f->fonts; d.pixels_per_em=16; d.glyph_cache_bytes=65536;
    nx_interaction_state state; int valid,submitted;
    return nx_begin_frame(f->context,320,120,0)==NX_OK &&
        nx_semantics_add(f->context,&root,NULL)==NX_OK &&
        nx_text_field_widget(f->context,&d,f->field,&state,&valid,&submitted)==NX_OK &&
        nx_end_frame(f->context)==NX_OK && nx_window_publish_semantics(f->window,f->context)==NX_OK;
}
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
        nx_text_field_create("AZ",2,0,&f.field)!=NX_OK || !frame(&f)) goto done;
    setvbuf(stdout,NULL,_IOLBF,0); puts("READY");
    if (fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL)|O_NONBLOCK)<0) goto done;
    char input[64]; size_t used=0;
    for (;;) {
        if (nx_window_poll(f.window,20)!=NX_OK) goto done;
        size_t routed; if (nx_window_route_events(f.window,f.context,&routed)!=NX_OK) goto done;
        ssize_t n=read(STDIN_FILENO,input+used,sizeof(input)-used-1u);
        if (!n) goto done;
        if (n<0 && errno!=EAGAIN && errno!=EINTR) goto done;
        if (n>0) used+=(size_t)n;
        input[used]=0; char *end=memchr(input,'\n',used); if (!end) continue;
        *end=0;
        if (!strcmp(input,"quit")) { okay=1; break; }
        if (!strcmp(input,"large")) {
            if (nx_text_field_set_text(f.field,large,length)!=NX_OK || !frame(&f)) goto done;
            puts("ACK large");
        } else if (!strcmp(input,"short")) {
            if (nx_text_field_set_text(f.field,"AZ",2)!=NX_OK || !frame(&f)) goto done;
            puts("ACK short");
        } else goto done;
        size_t consumed=(size_t)(end-input)+1u; used-=consumed; memmove(input,input+consumed,used);
    }
done:
    if (f.window) nx_window_publish_semantics(f.window,NULL);
    nx_text_field_destroy(f.field); nx_context_destroy(f.context);
    nx_font_collection_destroy(f.fonts); nx_font_close(f.font); nx_window_destroy(f.window);
    free(large); return okay?0:1;
}
