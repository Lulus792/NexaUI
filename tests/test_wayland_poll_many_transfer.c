#define _XOPEN_SOURCE 700
#include "wayland_v3_fixture.h"
#include "nexaui/wayland_clipboard.h"
#include <errno.h>
static size_t queue_write_limit;
static int queue_write_error;
static ssize_t test_queue_write(int fd, const void *bytes, size_t length) {
    if(queue_write_error){errno=queue_write_error;queue_write_error=0;return -1;}
    if(queue_write_limit && length>queue_write_limit)length=queue_write_limit;
    return write(fd,bytes,length);
}
#define write test_queue_write
#include "../examples/23_aetheri_terminal_view/pty_queue.h"
#undef write
#include <fcntl.h>
#include <sys/socket.h>
#include <poll.h>
#include <termios.h>
#include <time.h>

static double now_seconds(void) {
    struct timespec t;CHECK(clock_gettime(CLOCK_MONOTONIC,&t)==0);
    return t.tv_sec+t.tv_nsec/1e9;
}
typedef struct drain_args {int fd;size_t bytes;int error;} drain_args;
static void *delayed_drain(void *user) {
    drain_args *a=user;struct timespec delay={0,150000000};
    while(nanosleep(&delay,&delay)<0 && errno==EINTR){}
    char buffer[4096];ssize_t n;
    do {n=read(a->fd,buffer,sizeof(buffer));if(n>0)a->bytes+=(size_t)n;}
    while(n>0 || (n<0 && errno==EINTR));
    if(n>=0 || (errno!=EAGAIN && errno!=EWOULDBLOCK))a->error=1;
    return NULL;
}
static const char *start_server(test_server *s,pthread_t *thread) {
    s->clipboard_auto_focus=1;s->display=wl_display_create();CHECK(s->display);
    CHECK(wl_global_create(s->display,&wl_compositor_interface,1,s,bind_compositor));
    CHECK(wl_global_create(s->display,&wl_shm_interface,1,s,bind_shm));
    CHECK(wl_global_create(s->display,&wl_seat_interface,5,s,bind_seat));
    CHECK(wl_global_create(s->display,&wl_data_device_manager_interface,3,s,bind_data_manager));
    CHECK(wl_global_create(s->display,&xdg_wm_base_interface,1,s,bind_wm));
    CHECK(wl_global_create(s->display,&zwp_text_input_manager_v3_interface,1,s,bind_manager));
    const char *name=wl_display_add_socket_auto(s->display);CHECK(name);
    CHECK(!pthread_create(thread,NULL,run_server,s));return name;
}
static void drain(nx_window *w) {
    nx_event event;int available;
    do {CHECK(nx_window_next_event(w,&event,&available)==NX_OK);} while(available);
}

typedef struct pty_reader {
    int fd, error;
    size_t filler, received;
    const char *payload;
    size_t payload_length;
} pty_reader;
static void *read_pty(void *user) {
    pty_reader *r=user;struct timespec delay={0,150000000};
    while(nanosleep(&delay,&delay)<0 && errno==EINTR){}
    double deadline=now_seconds()+5;
    while(r->received<r->filler+r->payload_length && now_seconds()<deadline) {
        struct pollfd p={r->fd,POLLIN,0};int ready=poll(&p,1,100);
        if(ready<0 && errno==EINTR)continue;
        if(ready<0 || (p.revents&(POLLERR|POLLNVAL|POLLHUP))){r->error=1;return NULL;}
        if(!ready)continue;
        char bytes[2048];ssize_t n=read(r->fd,bytes,sizeof(bytes));
        if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR))continue;
        if(n<=0){r->error=1;return NULL;}
        for(ssize_t i=0;i<n;++i) {
            size_t index=r->received++;
            char expected=index<r->filler?'F':
                index-r->filler<r->payload_length?r->payload[index-r->filler]:0;
            if(index>=r->filler+r->payload_length || bytes[i]!=expected)r->error=1;
        }
        if(r->error)return NULL;
    }
    if(r->received!=r->filler+r->payload_length)r->error=1;
    return NULL;
}
static void check_pty_backpressure(nx_window **windows) {
    int master=posix_openpt(O_RDWR|O_NOCTTY|O_NONBLOCK);
    CHECK(master>=0 && !grantpt(master) && !unlockpt(master));
    char *name=ptsname(master);CHECK(name);
    int slave=open(name,O_RDWR|O_NOCTTY|O_NONBLOCK);CHECK(slave>=0);
    struct termios attrs;CHECK(!tcgetattr(slave,&attrs));
    attrs.c_iflag=0;attrs.c_oflag=0;attrs.c_lflag=0;
    attrs.c_cflag=(attrs.c_cflag&~(CSIZE|PARENB))|CS8|CREAD;
    attrs.c_cc[VMIN]=1;attrs.c_cc[VTIME]=0;CHECK(!tcsetattr(slave,TCSANOW,&attrs));
    char filler[4096];memset(filler,'F',sizeof(filler));size_t total=0;double deadline=now_seconds()+3;
    uint32_t flags=0;
    for(;;) {
        ssize_t n=write(master,filler,sizeof(filler));
        if(n>0){total+=(size_t)n;CHECK(total<4*1024*1024);continue;}
        if(n<0 && errno==EINTR)continue;
        CHECK(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK));
        CHECK(nx_window_poll_many_with_fd_events(windows,2,0,master,NX_WINDOW_FD_WRITABLE,&flags)==NX_OK);
        if(!(flags&NX_WINDOW_FD_WRITABLE))break;
        CHECK(now_seconds()<deadline);
    }
    CHECK(total>0);
    terminal_pty_queue q={0};
    const char first[]="first-commit/",second[]="second-commit";
    const char expected[]="first-commit/second-commit";
    CHECK(terminal_pty_queue_push(&q,first,sizeof(first)-1));
    CHECK(terminal_pty_queue_push(&q,second,sizeof(second)-1));
    CHECK(q.length==sizeof(expected)-1);
    CHECK(!terminal_pty_queue_push(&q,filler,sizeof(filler)) && q.length==sizeof(expected)-1);
    queue_write_error=EINTR;CHECK(terminal_pty_queue_write(&q,master)==0 && q.length==sizeof(expected)-1);
    queue_write_error=EAGAIN;CHECK(terminal_pty_queue_write(&q,master)==0 && q.length==sizeof(expected)-1);
    queue_write_error=EIO;CHECK(terminal_pty_queue_write(&q,master)==-1 && q.length==sizeof(expected)-1);
    CHECK(terminal_pty_queue_write(&q,master)==0 && q.length==sizeof(expected)-1);
    pty_reader reader={.fd=slave,.filler=total,.payload=expected,.payload_length=sizeof(expected)-1};
    pthread_t thread;CHECK(!pthread_create(&thread,NULL,read_pty,&reader));
    double start=now_seconds();queue_write_limit=3;unsigned writes=0;
    while(q.length && now_seconds()-start<5) {
        CHECK(nx_window_poll_many_with_fd_events(windows,2,1000,master,NX_WINDOW_FD_WRITABLE,&flags)==NX_OK);
        drain(windows[0]);drain(windows[1]);
        if(flags&NX_WINDOW_FD_WRITABLE){int result=terminal_pty_queue_write(&q,master);CHECK(result>=0);writes+=(unsigned)result;}
    }
    CHECK(!pthread_join(thread,NULL));
    queue_write_limit=0;
    CHECK(!q.length && writes>1 && !reader.error && reader.received==total+sizeof(expected)-1);
    CHECK(now_seconds()-start>=.08 && now_seconds()-start<5);
    CHECK(terminal_pty_queue_push(&q,first,sizeof(first)-1));terminal_pty_queue_clear(&q);
    CHECK(!q.length && !q.offset);
    for(size_t i=0;i<sizeof(q.bytes);++i)CHECK(!q.bytes[i]);
    int pipe_fds[2];CHECK(!pipe(pipe_fds));
    CHECK(terminal_pty_queue_push(&q,filler,sizeof(filler)));
    queue_write_limit=7;CHECK(terminal_pty_queue_write(&q,pipe_fds[1])==1);queue_write_limit=0;
    CHECK(q.offset==7 && q.length==sizeof(filler)-7);
    CHECK(terminal_pty_queue_push(&q,"1234567",7));
    CHECK(q.offset==0 && q.length==sizeof(filler));
    CHECK(!memcmp(q.bytes+sizeof(filler)-7,"1234567",7));
    terminal_pty_queue_clear(&q);
    CHECK(!close(pipe_fds[0]) && !close(pipe_fds[1]));
    CHECK(!close(slave) && !close(master));
}
int main(void) {
    char runtime[]="/tmp/nexaui-many-transfer-XXXXXX";CHECK(mkdtemp(runtime));
    CHECK(!setenv("XDG_RUNTIME_DIR",runtime,1) && !setenv("NEXAUI_PLATFORM","wayland",1));
    unsetenv("WAYLAND_SOCKET");
    test_server servers[2]={{0}};pthread_t threads[2];nx_window *windows[2]={NULL,NULL};
    nx_window_desc desc={"poll many transfer",240,120,NX_WINDOW_HIDDEN,{0}};
    for(unsigned i=0;i<2;++i) {
        const char *name=start_server(&servers[i],&threads[i]);
        CHECK(!setenv("WAYLAND_DISPLAY",name,1));
        CHECK(nx_window_create(&desc,&windows[i])==NX_OK);
    }
    test_server *peer=&servers[1];atomic_store(&peer->offer_stall,1);atomic_store(&peer->send_offer,1);
    wl_surface_commit(nx_window_wayland_surface(windows[1]));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(windows[1]))>=0);
    uint64_t token=0;double start=now_seconds();
    CHECK(nx_wayland_clipboard_read_begin(windows[1],1024,200,0,&token)==NX_OK && token);
    uint32_t flags=0;int ready=0;char *bytes=NULL;size_t length=0;nx_result result=NX_OK;
    for(unsigned i=0;i<30 && !ready;++i) {
        CHECK(nx_window_poll_many(windows,2,2000,-1,&flags)==NX_OK && !flags);
        drain(windows[0]);drain(windows[1]);
        result=nx_wayland_clipboard_read_take(windows[1],token,&bytes,&length,&ready);
    }
    double elapsed=now_seconds()-start;
    CHECK(ready && result==NX_ERROR_PLATFORM && !bytes && !length && elapsed>=.1 && elapsed<1.5);
    CHECK(atomic_load(&peer->offer_reads)==1 && atomic_load(&peer->stalled_fd)>0);
    CHECK(close(atomic_exchange(&peer->stalled_fd,0)-1)==0);
    CHECK(nx_window_poll_many(&windows[0],1,0,-1,&flags)==NX_OK);
    int pair[2];CHECK(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
    CHECK(fcntl(pair[0],F_SETFL,fcntl(pair[0],F_GETFL)|O_NONBLOCK)==0);
    CHECK(fcntl(pair[1],F_SETFL,fcntl(pair[1],F_GETFL)|O_NONBLOCK)==0);
    flags=99;CHECK(nx_window_poll_many_with_fd_events(windows,2,0,pair[0],0,&flags)==NX_ERROR_ARGUMENT && !flags);
    flags=99;CHECK(nx_window_poll_many_with_fd_events(windows,2,0,-1,NX_WINDOW_FD_WRITABLE,&flags)==NX_ERROR_ARGUMENT && !flags);
    flags=99;CHECK(nx_window_poll_with_fd_events(windows[0],0,pair[0],NX_WINDOW_FD_ERROR,&flags)==NX_ERROR_ARGUMENT && !flags);
    char buffer[4096]={0};ssize_t n;
    do {n=write(pair[0],buffer,sizeof(buffer));} while(n>0 || (n<0 && errno==EINTR));
    CHECK(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK));
    CHECK(nx_window_poll_many_with_fd_events(windows,2,0,pair[0],NX_WINDOW_FD_WRITABLE,&flags)==NX_OK && !(flags&NX_WINDOW_FD_WRITABLE));
    CHECK(nx_window_poll_with_fd_events(windows[0],0,pair[0],NX_WINDOW_FD_WRITABLE,&flags)==NX_OK && !(flags&NX_WINDOW_FD_WRITABLE));
    drain_args args={.fd=pair[1]};pthread_t drain_thread;
    CHECK(!pthread_create(&drain_thread,NULL,delayed_drain,&args));double blocked_start=now_seconds();
    do {
        CHECK(nx_window_poll_many_with_fd_events(windows,2,1000,pair[0],NX_WINDOW_FD_WRITABLE,&flags)==NX_OK);
        drain(windows[0]);drain(windows[1]);
    } while(!(flags&NX_WINDOW_FD_WRITABLE) && now_seconds()-blocked_start<2);
    double blocked_elapsed=now_seconds()-blocked_start;
    CHECK(!pthread_join(drain_thread,NULL) && !args.error && args.bytes>0);
    CHECK((flags&NX_WINDOW_FD_WRITABLE) && blocked_elapsed>=.08 && blocked_elapsed<1.5);
    CHECK(nx_window_poll_many(windows,2,0,-1,&flags)==NX_OK);
    CHECK(nx_window_poll_with_fd_events(windows[0],0,pair[0],NX_WINDOW_FD_WRITABLE,&flags)==NX_OK && (flags&NX_WINDOW_FD_WRITABLE));
    CHECK(nx_window_poll_with_fd_events(windows[0],0,pair[0],NX_WINDOW_FD_READABLE,&flags)==NX_OK && !(flags&NX_WINDOW_FD_WRITABLE));
    CHECK(write(pair[1],"q",1)==1);
    CHECK(nx_window_poll_many_with_fd_events(windows,2,0,pair[0],NX_WINDOW_FD_READABLE|NX_WINDOW_FD_WRITABLE,&flags)==NX_OK &&
          (flags&(NX_WINDOW_FD_READABLE|NX_WINDOW_FD_WRITABLE))==(NX_WINDOW_FD_READABLE|NX_WINDOW_FD_WRITABLE));
    CHECK(nx_window_poll_with_fd(windows[0],0,pair[0],&flags)==NX_OK && (flags&NX_WINDOW_FD_READABLE) && !(flags&NX_WINDOW_FD_WRITABLE));
    int invalid=dup(pair[0]);CHECK(invalid>=0 && close(invalid)==0);
    CHECK(nx_window_poll_many_with_fd_events(windows,2,0,invalid,NX_WINDOW_FD_WRITABLE,&flags)==NX_OK && (flags&NX_WINDOW_FD_ERROR));
    CHECK(read(pair[0],buffer,1)==1 && buffer[0]=='q');CHECK(close(pair[1])==0);
    CHECK(nx_window_poll_many_with_fd_events(windows,2,0,pair[0],NX_WINDOW_FD_WRITABLE,&flags)==NX_OK && (flags&NX_WINDOW_FD_HANGUP));
    CHECK(close(pair[0])==0);
    check_pty_backpressure(windows);
    for(unsigned i=0;i<2;++i)nx_window_destroy(windows[i]);
    for(unsigned i=0;i<2;++i) {
        wl_display_terminate(servers[i].display);CHECK(!pthread_join(threads[i],NULL));
        wl_display_destroy_clients(servers[i].display);wl_display_destroy(servers[i].display);
    }
    CHECK(!rmdir(runtime));puts("second window's stalled transfer deadline woke group");return 0;
}
