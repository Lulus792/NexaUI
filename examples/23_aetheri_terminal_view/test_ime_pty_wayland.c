#include "wayland_v3_fixture.h"
#include "ime_adapter.h"
#include "pty.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <time.h>

/* This fixture owns its shell. It is not an application event loop. */
#undef CHECK
#define CHECK(x) do {if(!(x)){fprintf(stderr,"ime-pty:%d: %s (errno %d)\n",__LINE__,#x,errno);goto cleanup;}} while(0)
static double now_seconds(void) {struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;return t.tv_sec+t.tv_nsec/1e9;}
static int width(uint32_t c){(void)c;return 1;}
static int send_bytes(int fd,const char *text,size_t length) {
    size_t at=0;double until=now_seconds()+5;
    while(at<length && now_seconds()<until) {
        ssize_t n=write(fd,text+at,length-at);
        if(n>0){at+=(size_t)n;continue;}
        if(n<0 && errno==EINTR)continue;
        if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) {
            struct pollfd p={fd,POLLOUT,0};if(poll(&p,1,100)<0 && errno!=EINTR)return 0;continue;
        }
        return 0;
    }
    return at==length;
}
static int has_text(const struct ae_terminal *model,const char *text) {
    size_t length=strlen(text);
    for(unsigned y=0;y<model->rows;++y){const struct ae_term_cell *row=ae_terminal_row(model,y);if(!row)continue;
        for(unsigned x=0;x+length<=model->cols;++x){size_t i=0;
            while(i<length && row[x+i].code==(unsigned char)text[i])++i;
            if(i==length)return 1;}}
    return 0;
}
static int pump(struct ae_terminal_pty *pty,struct ae_terminal *model,const char *text,int finish) {
    double until=now_seconds()+10;
    while(now_seconds()<until) {
        struct pollfd p={pty->master,POLLIN|POLLHUP,0};int ready=poll(&p,1,100);
        if(ready<0 && errno!=EINTR)return 0;
        if(ready>0 && !pty->eof)for(;;){char bytes[4096];ssize_t n=ae_terminal_pty_read(pty,bytes,sizeof(bytes));
            if(n>0){if(model)ae_terminal_feed(model,bytes,(size_t)n);continue;}
            if(n==0 || (n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)))break;
            if(n<0 && errno==EINTR)continue;
            return 0;
        }
        if(ae_terminal_pty_observe(pty)<0)return 0;
        if(text && model && has_text(model,text))return 1;
        if(finish && pty->eof && pty->ended)return 1;
        if(!finish && pty->ended)return 0;
    }
    return 0;
}
int main(int argc,char **argv) {
    int okay=0,source=-1,started=0,has_runtime=0,accepted=0,rejected=0;
    struct ae_terminal_pty pty={.master=-1,.pin=-1,.pid=-1};
    struct ae_terminal *model=NULL;test_server server={0};nx_window *window=NULL;
    char runtime[]="/tmp/nexaui-ime-pty-XXXXXX",socket_path[256]={0};pthread_t thread;
    terminal_ime_adapter ime={0};terminal_ime_guard guard={41,7,1};
    CHECK(argc==3 && getuid()!=0 && setenv("TERM","dumb",1)==0);
    int filtered=!strcmp(argv[2],"filtered"),stale=!strcmp(argv[2],"stale");
    CHECK(filtered || stale || !strcmp(argv[2],"valid"));
    source=open(argv[1],O_RDONLY|O_CLOEXEC);CHECK(source>=3);
    /* Aetheri's process owner starts the PTY before any UI/server threads. */
    CHECK(ae_terminal_pty_open(&pty,source,80,12)==0);close(source);source=-1;
    model=malloc(sizeof(*model));CHECK(model);ae_terminal_init(model,80,12,width);
    atomic_store(&server.commit_pty_test,filtered?2:1);
    CHECK(mkdtemp(runtime));has_runtime=1;
    CHECK(setenv("XDG_RUNTIME_DIR",runtime,1)==0 && setenv("NEXAUI_PLATFORM","wayland",1)==0);
    server.display=wl_display_create();CHECK(server.display);
    CHECK(wl_global_create(server.display,&wl_compositor_interface,1,&server,bind_compositor));
    CHECK(wl_global_create(server.display,&wl_shm_interface,1,&server,bind_shm));
    CHECK(wl_global_create(server.display,&wl_seat_interface,5,&server,bind_seat));
    CHECK(wl_global_create(server.display,&wl_data_device_manager_interface,3,&server,bind_data_manager));
    CHECK(wl_global_create(server.display,&xdg_wm_base_interface,1,&server,bind_wm));
    CHECK(wl_global_create(server.display,&zwp_text_input_manager_v3_interface,1,&server,bind_manager));
    const char *name=wl_display_add_socket_auto(server.display);CHECK(name);
    CHECK(snprintf(socket_path,sizeof(socket_path),"%s/%s",runtime,name)>0 && setenv("WAYLAND_DISPLAY",name,1)==0);
    CHECK(pthread_create(&thread,NULL,run_server,&server)==0);started=1;
    nx_window_desc desc={0};desc.width=240;desc.height=120;desc.flags=NX_WINDOW_HIDDEN;
    CHECK(nx_window_create(&desc,&window)==NX_OK && terminal_ime_bind(&ime,guard)==NX_OK);
    CHECK(nx_window_set_ime_surrounding_text(window,41,7,"",0,0,0)==NX_OK);
    CHECK(nx_window_set_ime_enabled(window,1)==NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    if(stale){guard.epoch=8;CHECK(terminal_ime_bind(&ime,guard)==NX_OK);
        CHECK(nx_window_set_ime_surrounding_text(window,41,8,"",0,0,0)==NX_OK);}
    nx_event event;int available=0;
    do {
        CHECK(nx_window_next_event(window,&event,&available)==NX_OK);
        if(available && event.type==NX_EVENT_TEXT) {
            int ready=0;nx_utf8_span bytes={0};
            nx_result r=terminal_ime_commit_event(&ime,guard,&event,&ready,&bytes);
            if(filtered || stale){CHECK(r==NX_ERROR_STATE && !ready && !bytes.data);++rejected;}
            else {CHECK(r==NX_OK);if(ready){CHECK(send_bytes(pty.master,bytes.data,bytes.length));++accepted;}}
        }
    } while(available);
    if(filtered || stale) {
        CHECK(!accepted && rejected>1);
        const char *probe="printf '%s%s\\n' 'SAFE_' 'PROBE'\n";
        CHECK(send_bytes(pty.master,probe,strlen(probe)) && pump(&pty,model,"SAFE_PROBE",0));
        CHECK(!has_text(model,"IME_MARK"));
    } else {
        CHECK(accepted==1 && !rejected);
        CHECK(send_bytes(pty.master,"\n",1) && pump(&pty,model,"IME_MARK",0));
    }
    CHECK(send_bytes(pty.master,"exit 0\n",7) && pump(&pty,model,NULL,1) && pty.status==0);
    CHECK(ae_terminal_pty_close(&pty)==0);okay=1;
cleanup:
    terminal_ime_reset(&ime);if(window)nx_window_destroy(window);
    if(started){wl_display_terminate(server.display);(void)pthread_join(thread,NULL);}
    if(server.display){wl_display_destroy_clients(server.display);wl_display_destroy(server.display);}
    if(socket_path[0])(void)unlink(socket_path);
    if(has_runtime)(void)rmdir(runtime);
    if(!okay && pty.pid>0 && !pty.ended)(void)ae_terminal_pty_hangup(&pty);
    if(!okay && pty.pid>0 && (!pty.ended || !pty.eof)) {
        if(!pump(&pty,model,NULL,1) && !pty.ended){(void)kill(pty.pid,SIGKILL);(void)pump(&pty,model,NULL,1);}
    }
    if(pty.pid>0 && pty.ended && pty.eof)(void)ae_terminal_pty_close(&pty);
    if(source>=0)close(source);free(model);
    if(okay)printf("Controlled Wayland IME to original PTY: %s passed\n",argv[2]);
    return okay?0:1;
}
