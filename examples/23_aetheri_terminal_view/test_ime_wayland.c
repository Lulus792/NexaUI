#include "wayland_v3_fixture.h"
#include "ime_adapter.h"

int main(void) {
    test_server server={0};char runtime[]="/tmp/nexaui-terminal-ime-XXXXXX";
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
    CHECK(nx_window_set_ime_rect(window,(nx_rect){12,34,2,18})==NX_OK);
    CHECK(nx_window_set_ime_enabled(window,1)==NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    nx_event event;int available=0,preedit=0;
    do {
        CHECK(nx_window_next_event(window,&event,&available)==NX_OK);
        if(available && event.type==NX_EVENT_IME_PREEDIT_CHANGED)preedit=1;
    } while(available);
    CHECK(preedit);
    char draft[32];size_t length=0;int32_t begin=-1,end=-1;
    CHECK(nx_window_get_ime_preedit(window,draft,sizeof(draft),&length,&begin,&end)==NX_OK);
    CHECK(length==4 && !memcmp(draft,"na\xc3\xaf",4) && begin==2 && end==2);
    terminal_ime_adapter ime={0};terminal_ime_guard guard={41,7,1};
    CHECK(terminal_ime_bind(&ime,guard)==NX_OK);
    CHECK(terminal_ime_preedit(&ime,guard,draft,length,begin,end)==NX_OK);
    CHECK(ime.composition.size==4 && ime.composition.count==3 &&
          ime.composition.cursor==2 && ime.composition.anchor==2);
    guard.focused=0;
    CHECK(terminal_ime_preedit(&ime,guard,draft,length,begin,end)==NX_ERROR_STATE && !ime.composition.size);
    terminal_ime_reset(&ime);nx_window_destroy(window);
    wl_display_terminate(server.display);CHECK(pthread_join(thread,NULL)==0);
    wl_display_destroy_clients(server.display);wl_display_destroy(server.display);
    CHECK(unlink(socket_path)==0 || access(socket_path,F_OK)!=0);CHECK(rmdir(runtime)==0);
    puts("Controlled Wayland preedit reached the original Aetheri composition model");
    return 0;
}
