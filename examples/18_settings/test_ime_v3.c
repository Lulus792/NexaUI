#include "../../tests/wayland_v3_fixture.h"
#include "scene.h"
#include "text_value.h"

typedef struct paste_ticket { int refs,calls,applied; } paste_ticket;
static void paste_retain(void *p){++((paste_ticket *)p)->refs;}
static void paste_release(void *p){--((paste_ticket *)p)->refs;}
static int paste_current(void *p,uint64_t token,uint64_t context,uint64_t id,nx_text_value_target target) {
    (void)p;(void)token;(void)context;(void)id;(void)target;return 1;
}
static void paste_complete(void *p,int applied){paste_ticket *t=p;++t->calls;t->applied=applied;}

static unsigned char *read_font(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb"); if(!f)return NULL;
    long n=-1; unsigned char *data=NULL;
    if(fseek(f,0,SEEK_END)==0 && (n=ftell(f))>0 && n<64*1024*1024 &&
       fseek(f,0,SEEK_SET)==0 && (data=malloc((size_t)n)) &&
       fread(data,1,(size_t)n,f)==(size_t)n) *size=(size_t)n;
    else {free(data);data=NULL;}
    if(fclose(f)!=0){free(data);return NULL;} return data;
}
static int value_is(nx_context *ctx,const char *expected) {
    uint32_t index; nx_semantic_view view;
    return nx_semantics_find(ctx,3,&index)==NX_OK && nx_semantics_get(ctx,index,&view)==NX_OK &&
        view.value.length==strlen(expected) && !memcmp(view.value.data,expected,view.value.length);
}
int main(int argc,char **argv) {
    CHECK(argc==2 || (argc==3 && (!strcmp(argv[2],"--external") || !strcmp(argv[2],"--same") ||
          !strcmp(argv[2],"--clipboard-once") || !strcmp(argv[2],"--keyboard-clipboard-once"))));
    int external=argc==3 && !strcmp(argv[2],"--external");
    int same=argc==3 && !strcmp(argv[2],"--same");
    int once=argc==3 && !strcmp(argv[2],"--clipboard-once");
    int keyboard_once=argc==3 && !strcmp(argv[2],"--keyboard-clipboard-once");
    size_t length=0; unsigned char *font=read_font(argv[1],&length); CHECK(font);
    test_server server={0}; server.width=640; server.height=360;
    char runtime[]="/tmp/nexaui-settings-ime-XXXXXX";
    CHECK(mkdtemp(runtime)); CHECK(setenv("XDG_RUNTIME_DIR",runtime,1)==0);
    CHECK(setenv("NEXAUI_PLATFORM","wayland",1)==0); unsetenv("WAYLAND_SOCKET");
    server.display=wl_display_create(); CHECK(server.display);
    CHECK(wl_global_create(server.display,&wl_compositor_interface,1,&server,bind_compositor));
    CHECK(wl_global_create(server.display,&wl_shm_interface,1,&server,bind_shm));
    CHECK(wl_global_create(server.display,&wl_seat_interface,5,&server,bind_seat));
    CHECK(wl_global_create(server.display,&wl_data_device_manager_interface,3,&server,bind_data_manager));
    CHECK(wl_global_create(server.display,&xdg_wm_base_interface,1,&server,bind_wm));
    CHECK(wl_global_create(server.display,&zwp_text_input_manager_v3_interface,1,&server,bind_manager));
    const char *socket_name=wl_display_add_socket_auto(server.display); CHECK(socket_name);
    char socket_path[256]; CHECK(snprintf(socket_path,sizeof(socket_path),"%s/%s",runtime,socket_name)>0);
    CHECK(setenv("WAYLAND_DISPLAY",socket_name,1)==0);
    pthread_t thread; CHECK(pthread_create(&thread,NULL,run_server,&server)==0);
    nx_window *window=NULL; nx_window_desc desc={"Settings IME test",640,360,NX_WINDOW_HIDDEN,{0}};
    CHECK(nx_window_create(&desc,&window)==NX_OK && window);
    settings_scene *scene=NULL; CHECK(settings_create(font,length,window,&scene));
    nx_context *ctx=settings_context(scene);
    CHECK(settings_frame(scene,640,360) && value_is(ctx,"Aetheri"));
    CHECK(nx_focus_set(ctx,3)==NX_OK && settings_frame(scene,640,360));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    CHECK(atomic_load(&server.enable_commits)>0 && value_is(ctx,"Aetheri"));
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    char preedit[8]={0}; size_t preedit_length=0; int32_t begin=0,end=0;
    CHECK(nx_window_get_ime_preedit(window,preedit,sizeof(preedit),&preedit_length,&begin,&end)==NX_OK &&
          preedit_length==4 && !memcmp(preedit,"na\xc3\xaf",4) && begin==2 && end==2);
    int enabled_before=atomic_load(&server.enable_commits);
    if(external) CHECK(nx_semantics_request_text_value(ctx,3,(nx_utf8_span){"External",8})==NX_OK);
    if(same) CHECK(nx_semantics_request_text_value(ctx,3,(nx_utf8_span){"Aetheri",7})==NX_OK);
    size_t routed=0; CHECK(nx_window_route_events(window,ctx,&routed)==NX_OK && routed>0);
    CHECK(settings_frame(scene,640,360));
    const char *expected=external?"External":"Aethericaf\xc3\xa9";
    CHECK(value_is(ctx,expected));
    if(external) CHECK(nx_window_get_ime_preedit(window,preedit,sizeof(preedit),&preedit_length,&begin,&end)==NX_OK &&
                       preedit_length==0);
    if(same) CHECK(nx_window_get_ime_preedit(window,preedit,sizeof(preedit),&preedit_length,&begin,&end)==NX_OK &&
                   preedit_length==4 && !memcmp(preedit,"na\xc3\xaf",4));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    if(external) CHECK(atomic_load(&server.disable_commits)>0 &&
                       atomic_load(&server.enable_commits)>enabled_before);
    if(same) CHECK(atomic_load(&server.disable_commits)==0);
    CHECK(settings_frame(scene,640,360) && value_is(ctx,expected));
    nx_utf8_span applied; int reduced_motion;
    CHECK(settings_applied(scene,&applied,&reduced_motion) && applied.length==7 &&
          !memcmp(applied.data,"Aetheri",7) && !reduced_motion);
    CHECK(nx_focus_set(ctx,4)==NX_OK && settings_frame(scene,640,360));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    CHECK(nx_window_get_ime_preedit(window,preedit,sizeof(preedit),&preedit_length,&begin,&end)==NX_OK &&
          preedit_length==0 && value_is(ctx,expected));
    if(once || keyboard_once) {
        CHECK(nx_focus_set(ctx,3)==NX_OK && settings_frame(scene,640,360));
        atomic_store(&server.offer_once,1);atomic_store(&server.send_offer,1);
        wl_surface_commit(nx_window_wayland_surface(window));
        CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
        paste_ticket ticket={0};nx_text_value_guard guard={&ticket,1,paste_retain,paste_release,paste_current};
        if(once)CHECK(nx_semantics_request_text_paste_with_completion(ctx,3,0,&guard,paste_complete)==NX_OK);
        else {nx_event event={0};event.type=NX_EVENT_KEY_DOWN;event.key=NX_KEY_V;event.modifiers=NX_MOD_CONTROL;
              CHECK(nx_context_queue_event(ctx,&event)==NX_OK);}
        CHECK(settings_frame(scene,640,360));
        if(once)CHECK(ticket.calls==1 && ticket.applied==1 && ticket.refs==0);
        fprintf(stderr,"one-shot clipboard: reads=%d expected-value=%d\n",
            atomic_load(&server.offer_reads),value_is(ctx,once?"OnceAethericaf\xc3\xa9":"Aethericaf\xc3\xa9Once"));
        CHECK(value_is(ctx,once?"OnceAethericaf\xc3\xa9":"Aethericaf\xc3\xa9Once") && atomic_load(&server.offer_reads)==1);
    }
    settings_destroy(scene); nx_window_destroy(window); free(font);
    wl_display_terminate(server.display); CHECK(pthread_join(thread,NULL)==0);
    wl_display_destroy(server.display);
    CHECK(unlink(socket_path)==0 || access(socket_path,F_OK)!=0); CHECK(rmdir(runtime)==0);
    puts("Settings text-input-v3 commit reached the focused scene exactly once."); return 0;
}
