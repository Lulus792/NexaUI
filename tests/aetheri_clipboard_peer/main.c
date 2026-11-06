#include "nexaui/nexaui.h"
#include "nexaui/software.h"
#include "nexaui/wayland_clipboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

/* Each instance owns exactly one window and its own Wayland connection. */
int main(void) {
    static const char sample[]="Gr\xc3\xbc\xc3\x9f" "e \xf0\x9f\x8c\x8d";
    static const char foreign[]="Fremd \xf0\x9f\x8c\x8d";
    nx_window_desc desc={"NexaUI clipboard peer",360,240,0,{0}};
    nx_window *window=NULL; nx_software_view *view=NULL;
    CHECK(nx_window_create(&desc,&window)==NX_OK);
    CHECK(nx_software_view_open(window,&view)==NX_OK);
    uint64_t token=0; int ready=0,done=0,pending_reported=0,expect_foreign=0;
    int expect_interrupted=getenv("NEXAUI_TEST_CLIPBOARD_INTERRUPTED")!=NULL;
    while(!done) {
        uint32_t events=0;
        CHECK(nx_window_poll_with_fd(window,1000,STDIN_FILENO,&events)==NX_OK);
        nx_window_info info; CHECK(nx_window_get_info(window,&info)==NX_OK && !info.should_close);
        if(!ready) {
            nx_software_view_outcome outcome;
            CHECK(nx_software_view_render(view,NULL,0,(nx_color){0.2f,0.3f,0.4f,1},&outcome)==NX_OK);
            if(outcome==NX_SOFTWARE_VIEW_READY) {ready=1;puts("PEER_READY");}
        }
        nx_event event;int available=0;
        do {
            CHECK(nx_window_next_event(window,&event,&available)==NX_OK);
            if(available && event.type==NX_EVENT_FOCUS) printf("FOCUS %d\n",event.focused);
            if(available && event.type==NX_EVENT_TEXT) printf("TEXT %u\n",event.scalar);
        } while(available);
        if(events&NX_WINDOW_FD_READABLE) {
            char command=0; CHECK(read(STDIN_FILENO,&command,1)==1);
            if(command=='C') {
                CHECK(!token && nx_window_clipboard_set_text(window,sample,sizeof(sample)-1)==NX_OK);
                puts("CLIPBOARD_SET");
            } else if(command=='P' || command=='F') {
                CHECK(!token && nx_wayland_clipboard_read_begin(window,1024,3000,0,&token)==NX_OK && token);
                expect_foreign=command=='F';
                puts("CLIPBOARD_READ");
            } else if(command=='L') {
                uint64_t stale=0;
                CHECK(!token && nx_wayland_clipboard_read_begin(window,1024,3000,0,&stale)==NX_ERROR_STATE && !stale);
                puts("CLIPBOARD_LOST");
            } else if(command=='Q') {CHECK(!token);done=1;}
            else CHECK(0);
        } else CHECK(!(events&(NX_WINDOW_FD_HANGUP|NX_WINDOW_FD_ERROR)));
        if(token) {
            char *bytes=NULL;size_t length=0;int complete=0;
            nx_result result=nx_wayland_clipboard_read_take(window,token,&bytes,&length,&complete);
            if(expect_interrupted && (result==NX_ERROR_STATE || (complete && result==NX_ERROR_PLATFORM))) {
                CHECK(!bytes && !length);token=0;puts("CLIPBOARD_ABORTED");fflush(stdout);continue;
            }
            CHECK(result==NX_OK);
            if(complete) {
                if(expect_interrupted) {CHECK(length==0);puts("CLIPBOARD_EOF_EMPTY");}
                else {
                    const char *expected=expect_foreign?foreign:sample;
                    size_t expected_length=expect_foreign?sizeof(foreign)-1:sizeof(sample)-1;
                    CHECK(length==expected_length && !memcmp(bytes,expected,length));puts("CLIPBOARD_MATCH");
                }
                nx_wayland_clipboard_read_release(bytes,length);token=0;
            } else {CHECK(!bytes && !length);if(!pending_reported){puts("CLIPBOARD_PENDING");pending_reported=1;}}
        }
        fflush(stdout);
    }
    CHECK(nx_software_view_close(&view)==NX_OK);
    nx_window_destroy(window);puts("PEER_CLOSED");return 0;
}
