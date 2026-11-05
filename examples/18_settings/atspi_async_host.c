#include "../../tests/wayland_v3_fixture.h"
#include "scene.h"
#include <errno.h>
#include <fcntl.h>
#include <time.h>

static unsigned char *font_read(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    long n=-1;unsigned char *bytes=NULL;
    if(fseek(f,0,SEEK_END)==0 && (n=ftell(f))>0 && n<64*1024*1024 &&
       fseek(f,0,SEEK_SET)==0 && (bytes=malloc((size_t)n)) && fread(bytes,1,(size_t)n,f)==(size_t)n)*size=(size_t)n;
    else {free(bytes);bytes=NULL;}fclose(f);return bytes;
}
static int value_is(nx_context *ctx,const char *value) {
    uint32_t index;nx_semantic_view view;
    return nx_semantics_find(ctx,3,&index)==NX_OK && nx_semantics_get(ctx,index,&view)==NX_OK &&
        view.value.length==strlen(value) && !memcmp(view.value.data,value,view.value.length);
}
int main(int argc,char **argv) {
    CHECK(argc==3 && (!strcmp(argv[2],"input") || !strcmp(argv[2],"timeout") || !strcmp(argv[2],"complete")));
    int input=!strcmp(argv[2],"input"),complete=!strcmp(argv[2],"complete");
    size_t size=0;unsigned char *font=font_read(argv[1],&size);CHECK(font);
    test_server server={0};server.width=640;server.height=360;server.clipboard_auto_focus=1;
    char runtime[]="/tmp/nexaui-ax-async-XXXXXX";CHECK(mkdtemp(runtime));
    CHECK(setenv("XDG_RUNTIME_DIR",runtime,1)==0 && setenv("NEXAUI_PLATFORM","wayland",1)==0);unsetenv("WAYLAND_SOCKET");
    server.display=wl_display_create();CHECK(server.display);
    CHECK(wl_global_create(server.display,&wl_compositor_interface,1,&server,bind_compositor));
    CHECK(wl_global_create(server.display,&wl_shm_interface,1,&server,bind_shm));
    CHECK(wl_global_create(server.display,&wl_seat_interface,5,&server,bind_seat));
    CHECK(wl_global_create(server.display,&wl_data_device_manager_interface,3,&server,bind_data_manager));
    CHECK(wl_global_create(server.display,&xdg_wm_base_interface,1,&server,bind_wm));
    CHECK(wl_global_create(server.display,&zwp_text_input_manager_v3_interface,1,&server,bind_manager));
    const char *name=wl_display_add_socket_auto(server.display);CHECK(name);
    char path[256];CHECK(snprintf(path,sizeof(path),"%s/%s",runtime,name)>0 && setenv("WAYLAND_DISPLAY",name,1)==0);
    pthread_t thread;CHECK(pthread_create(&thread,NULL,run_server,&server)==0);
    nx_window_desc desc={"AT-SPI async host",640,360,NX_WINDOW_HIDDEN,{0}};nx_window *window=NULL;
    CHECK(nx_window_create(&desc,&window)==NX_OK);settings_scene *scene=NULL;CHECK(settings_create(font,size,window,&scene));
    nx_context *ctx=settings_context(scene);CHECK(settings_frame(scene,640,360));
    CHECK(nx_focus_set(ctx,3)==NX_OK && settings_frame(scene,640,360));
    atomic_store(&server.offer_stall,1);atomic_store(&server.send_offer,1);
    wl_surface_commit(nx_window_wayland_surface(window));CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    size_t initial_events=0;CHECK(nx_window_route_events(window,ctx,&initial_events)==NX_OK && initial_events>0);
    CHECK(settings_frame(scene,640,360) && value_is(ctx,"Aethericaf\xc3\xa9"));
    nx_result published=nx_window_publish_semantics(window,ctx);CHECK(published==NX_OK);
    int flags=fcntl(STDIN_FILENO,F_GETFL);CHECK(flags>=0 && fcntl(STDIN_FILENO,F_SETFL,flags|O_NONBLOCK)==0);
    int changed=0,done=0,released=0;unsigned ticks=0;
    for(unsigned i=0;i<1000 && !done;++i) {
        size_t routed=0;CHECK(nx_window_poll(window,20)==NX_OK && nx_window_route_events(window,ctx,&routed)==NX_OK);
        if(atomic_load(&server.offer_reads)) {
            CHECK(write(STDOUT_FILENO,".",1)==1);
            ++ticks;
            if(input && !changed) {
                nx_event event={0};event.type=NX_EVENT_TEXT;event.scalar='X';
                CHECK(nx_context_queue_event(ctx,&event)==NX_OK);++routed;changed=1;
            }
            if(complete && ticks<=5)++routed; /* publish unchanged intervening frames */
            if(complete && ticks==6) {
                int fd=atomic_exchange(&server.stalled_fd,0);CHECK(fd>0 && close(fd-1)==0);released=1;
            }
        }
        if(routed) {CHECK(settings_frame(scene,640,360));CHECK(nx_window_publish_semantics(window,ctx)==NX_OK);}
        char command=0;ssize_t got=read(STDIN_FILENO,&command,1);
        if(got==1) {CHECK(command=='Q');done=1;}
        else CHECK(got<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR));
    }
    CHECK(done && atomic_load(&server.offer_reads)==1);
    CHECK(value_is(ctx,input?"Aethericaf\xc3\xa9X":complete?"OnceAethericaf\xc3\xa9":"Aethericaf\xc3\xa9"));
    int held=atomic_exchange(&server.stalled_fd,0);
    if(complete)CHECK(released && !held);else CHECK(held>0 && close(held-1)==0);
    settings_destroy(scene);nx_window_destroy(window);free(font);
    wl_display_terminate(server.display);CHECK(pthread_join(thread,NULL)==0);wl_display_destroy(server.display);
    CHECK(unlink(path)==0 || access(path,F_OK)!=0);CHECK(rmdir(runtime)==0);return 0;
}
