#include "wayland_v3_fixture.h"
#include "ime_adapter.h"
#include "ime_overlay.h"
#include <math.h>

static int width(uint32_t scalar){(void)scalar;return 1;}
static unsigned char *read_font(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);if(n<=0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}fclose(f);*size=(size_t)n;return p;
}
static void candidate(nx_window *window,test_server *server,terminal_ime_adapter *ime,
    terminal_ime_guard guard,const char *font_path) {
    size_t size=0;unsigned char *bytes=read_font(font_path,&size);CHECK(bytes);
    nx_font *font=NULL;CHECK(nx_font_open(bytes,size,NULL,&font)==NX_OK);
    struct ae_terminal *model=malloc(sizeof(*model));CHECK(model);ae_terminal_init(model,24,3,width);
    ae_terminal_feed(model,"base",4);
    aetheri_terminal_view_desc d={.model=model,.regular=font,.bold=font,.text_identity=41,
        .cell_width=10,.cell_height=20,.baseline=15,.pixels_per_em=14};
    aetheri_terminal_view *view=NULL;CHECK(aetheri_terminal_view_create(&d,&view)==NX_OK);
    terminal_ime_overlay overlay={.ime=ime,.current=guard};nx_point origin={7.25f,36.5f};
    CHECK(aetheri_terminal_view_frame_with_overlay(view,1,1,terminal_ime_overlay_draw,&overlay)==NX_OK);
    CHECK(terminal_ime_overlay_publish_caret(&overlay,&d,window,origin)==NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    CHECK(atomic_load(&server->caret_x)==67 && atomic_load(&server->caret_y)==37 &&
          atomic_load(&server->caret_width)==10 && atomic_load(&server->caret_height)==20);
    CHECK(terminal_ime_preedit(ime,guard,NULL,0,0,0)==NX_OK);
    CHECK(aetheri_terminal_view_frame_with_overlay(view,1,1.5f,terminal_ime_overlay_draw,&overlay)==NX_OK);
    CHECK(terminal_ime_overlay_publish_caret(&overlay,&d,window,origin)==NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    CHECK(atomic_load(&server->caret_x)==47 && atomic_load(&server->caret_y)==37);
    CHECK(terminal_ime_preedit(ime,guard,"na\xc3\xaf",4,2,2)==NX_OK);
    ae_terminal_feed(model,"X",1);
    CHECK(aetheri_terminal_view_frame_with_overlay(view,2,2,terminal_ime_overlay_draw,&overlay)==NX_OK);
    CHECK(terminal_ime_overlay_publish_caret(&overlay,&d,window,origin)==NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    CHECK(atomic_load(&server->caret_x)==77 && atomic_load(&server->caret_y)==37);
    CHECK(ae_terminal_resize(model,10,2));
    CHECK(aetheri_terminal_view_frame_with_overlay(view,3,2,terminal_ime_overlay_draw,&overlay)==NX_OK);
    CHECK(terminal_ime_overlay_publish_caret(&overlay,&d,window,origin)==NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    CHECK(atomic_load(&server->caret_x)==27 && atomic_load(&server->caret_y)==37 &&
          atomic_load(&server->caret_width)==10 && atomic_load(&server->caret_height)==20);
    nx_point negative={-1.75f,-2.5f};
    CHECK(terminal_ime_overlay_publish_caret(&overlay,&d,window,negative)==NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    CHECK(atomic_load(&server->caret_x)==18 && atomic_load(&server->caret_y)==-3);
    int old_count=atomic_load(&server->caret_updates);overlay.current.epoch++;
    CHECK(terminal_ime_overlay_publish_caret(&overlay,&d,window,origin)==NX_ERROR_STATE);
    overlay.current=guard;
    CHECK(terminal_ime_overlay_publish_caret(&overlay,&d,window,(nx_point){INFINITY,0})==NX_ERROR_ARGUMENT);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    CHECK(atomic_load(&server->caret_updates)==old_count);
    aetheri_terminal_view_destroy(view);free(model);nx_font_close(font);free(bytes);
}

int main(int argc,char **argv) {
    test_server server={0};char runtime[]="/tmp/nexaui-terminal-ime-XXXXXX";
    int filtered=argc==2 && !strcmp(argv[1],"--filtered");
    int with_candidate=argc==3 && !strcmp(argv[1],"--candidate");CHECK(argc==1 || filtered || with_candidate);
    atomic_store(&server.commit_control_test,filtered);
    CHECK(mkdtemp(runtime));CHECK(setenv("XDG_RUNTIME_DIR",runtime,1)==0);
    CHECK(setenv("NEXAUI_PLATFORM","wayland",1)==0);
    server.display=wl_display_create();CHECK(server.display);
    CHECK(wl_global_create(server.display,&wl_compositor_interface,1,&server,bind_compositor));
    CHECK(wl_global_create(server.display,&wl_shm_interface,1,&server,bind_shm));
    CHECK(wl_global_create(server.display,&wl_seat_interface,5,&server,bind_seat));
    CHECK(wl_global_create(server.display,&wl_data_device_manager_interface,3,&server,bind_data_manager));
    CHECK(wl_global_create(server.display,&xdg_wm_base_interface,1,&server,bind_wm));
    CHECK(wl_global_create(server.display,&zwp_text_input_manager_v3_interface,1,&server,bind_manager));
    const char *socket_name=wl_display_add_socket_auto(server.display);CHECK(socket_name);
    char socket_path[256];CHECK(snprintf(socket_path,sizeof(socket_path),"%s/%s",runtime,socket_name)>0);
    CHECK(setenv("WAYLAND_DISPLAY",socket_name,1)==0);
    pthread_t thread;CHECK(pthread_create(&thread,NULL,run_server,&server)==0);
    nx_window_desc desc={0};desc.width=240;desc.height=120;desc.flags=NX_WINDOW_HIDDEN;
    nx_window *window=NULL;CHECK(nx_window_create(&desc,&window)==NX_OK);
    terminal_ime_adapter ime={0};terminal_ime_guard guard={41,7,1};
    CHECK(terminal_ime_bind(&ime,guard)==NX_OK);
    CHECK(nx_window_set_ime_surrounding_text(window,guard.identity,guard.epoch,"",0,0,0)==NX_OK);
    CHECK(nx_window_set_ime_rect(window,(nx_rect){12,34,2,18})==NX_OK);
    CHECK(nx_window_set_ime_enabled(window,1)==NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    nx_event event,old_preedit={0};int available=0,preedit=0,committed=0;
    do {
        CHECK(nx_window_next_event(window,&event,&available)==NX_OK);
        if(available && event.type==NX_EVENT_IME_PREEDIT_CHANGED){preedit=1;old_preedit=event;}
        if(available && event.type==NX_EVENT_TEXT) {
            int ready=0;nx_utf8_span bytes={0};
            nx_result result=terminal_ime_commit_event(&ime,guard,&event,&ready,&bytes);
            CHECK(result==(filtered?NX_ERROR_STATE:NX_OK));
            if(ready){CHECK(bytes.length==5 && !memcmp(bytes.data,"caf\xc3\xa9",5));++committed;}
        }
    } while(available);
    CHECK(preedit && committed==(filtered?0:1));
    char draft[32];size_t length=0;int32_t begin=-1,end=-1;
    CHECK(old_preedit.ime_target==guard.identity && old_preedit.ime_revision==guard.epoch);
    CHECK(nx_window_get_ime_preedit_for(window,guard.identity,guard.epoch,draft,sizeof(draft),&length,&begin,&end)==NX_OK);
    CHECK(length==4 && !memcmp(draft,"na\xc3\xaf",4) && begin==2 && end==2);
    length=99;begin=end=99;
    CHECK(nx_window_get_ime_preedit_for(window,41,7,draft,4,&length,&begin,&end)==NX_ERROR_LIMIT &&
          !length && !begin && !end);
    CHECK(nx_window_get_ime_preedit_for(window,41,7,NULL,0,&length,&begin,&end)==NX_OK && length==4);
    CHECK(terminal_ime_native_preedit(&ime,guard,window,&old_preedit)==NX_OK);
    CHECK(ime.composition.size==4 && ime.composition.count==3 &&
          ime.composition.cursor==2 && ime.composition.anchor==2);
    if(with_candidate)candidate(window,&server,&ime,guard,argv[2]);
    CHECK(nx_window_set_ime_surrounding_text(window,41,8,"",0,0,0)==NX_OK);
    length=123;begin=end=123;
    CHECK(nx_window_get_ime_preedit_for(window,41,7,draft,sizeof(draft),&length,&begin,&end)==NX_ERROR_STATE &&
          !length && !begin && !end);
    guard.epoch=8;CHECK(terminal_ime_bind(&ime,guard)==NX_OK);
    CHECK(terminal_ime_native_preedit(&ime,guard,window,&old_preedit)==NX_ERROR_STATE && !ime.composition.size);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    preedit=0;
    do {
        CHECK(nx_window_next_event(window,&event,&available)==NX_OK);
        if(available && event.type==NX_EVENT_IME_PREEDIT_CHANGED && event.ime_revision==8) {
            CHECK(terminal_ime_native_preedit(&ime,guard,window,&event)==NX_OK);preedit=1;
        }
    } while(available);
    CHECK(preedit && ime.composition.size==4);
    CHECK(terminal_ime_native_preedit(&ime,guard,window,&old_preedit)==NX_ERROR_STATE && ime.composition.size==4);
    guard.focused=0;
    CHECK(terminal_ime_preedit(&ime,guard,draft,length,begin,end)==NX_ERROR_STATE && !ime.composition.size);
    terminal_ime_reset(&ime);nx_window_destroy(window);
    wl_display_terminate(server.display);CHECK(pthread_join(thread,NULL)==0);
    wl_display_destroy_clients(server.display);wl_display_destroy(server.display);
    CHECK(unlink(socket_path)==0 || access(socket_path,F_OK)!=0);CHECK(rmdir(runtime)==0);
    puts(with_candidate?"Controlled Wayland candidate geometry and preedit passed":
         "Controlled Wayland preedit and whole commit reached original Aetheri models");
    return 0;
}
