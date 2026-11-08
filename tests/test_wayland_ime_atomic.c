#include "wayland_v3_fixture.h"

static void committed(test_server *server,struct wl_resource *input) {
    (void)input;if(server->enabled)atomic_fetch_add(&server->enable_commits,1);
}
static void produce(test_server *server,struct wl_client *client) {
    (void)client;int mode=atomic_exchange(&server->numeric_commit,0);if(!mode)return;
    struct wl_resource *input=server->text_input;CHECK(input && server->enabled);
    if(mode!=4 && mode!=5)zwp_text_input_v3_send_delete_surrounding_text(input,1,0);
    zwp_text_input_v3_send_commit_string(input,mode==1?"x\xc3\xa9":mode==2?"\n\x1b":mode==3 || mode==4?"":mode==6?"67":mode==7?"!":"x");
    zwp_text_input_v3_send_done(input,server->commit_serial);
    if(mode==5){zwp_text_input_v3_send_commit_string(input,"y");zwp_text_input_v3_send_done(input,server->commit_serial);}
}
static void text_frame(nx_context *ctx,const nx_text_field_desc *desc,nx_text_field *field) {
    nx_interaction_state state;int valid,submitted;CHECK(nx_begin_frame(ctx,240,120,0)==NX_OK);
    CHECK(nx_text_field_widget(ctx,desc,field,&state,&valid,&submitted)==NX_OK && valid);CHECK(nx_end_frame(ctx)==NX_OK);
}
static void expect_text(nx_text_field *field,const char *wanted,size_t anchor,size_t cursor) {
    nx_utf8_span text;size_t a,c;CHECK(nx_text_field_text(field,&text)==NX_OK && text.length==strlen(wanted) && !memcmp(text.data,wanted,text.length));
    CHECK(nx_text_field_selection(field,&a,&c)==NX_OK && a==anchor && c==cursor);
}
static void send(test_server *server,nx_window *window,nx_context *ctx,int mode) {
    atomic_store(&server->numeric_commit,mode);wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    size_t routed=0;CHECK(nx_window_route_events(window,ctx,&routed)==NX_OK && routed>=2);
}
static void undo(nx_context *ctx) {nx_event e={.type=NX_EVENT_TEXT_COMMAND,.text_command=NX_TEXT_COMMAND_UNDO};CHECK(nx_context_queue_event(ctx,&e)==NX_OK);}
static void number_frame(nx_context *ctx,const nx_numeric_field_desc *desc,nx_numeric_field *field,int wanted) {
    nx_interaction_state state;int valid=-1;CHECK(nx_begin_frame(ctx,240,120,0)==NX_OK);
    CHECK(nx_numeric_field_widget(ctx,desc,field,&state,&valid)==NX_OK && valid==wanted);CHECK(nx_end_frame(ctx)==NX_OK);
}
int main(void) {
    test_server server={0};server.text_committed=committed;server.surface_committed=produce;
    char runtime[]="/tmp/nexaui-native-ime-atomic-XXXXXX";CHECK(mkdtemp(runtime));
    CHECK(setenv("XDG_RUNTIME_DIR",runtime,1)==0 && setenv("NEXAUI_PLATFORM","wayland",1)==0);
    server.display=wl_display_create();CHECK(server.display);
    CHECK(wl_global_create(server.display,&wl_compositor_interface,1,&server,bind_compositor));
    CHECK(wl_global_create(server.display,&wl_shm_interface,1,&server,bind_shm));
    CHECK(wl_global_create(server.display,&wl_seat_interface,5,&server,bind_seat));
    CHECK(wl_global_create(server.display,&wl_data_device_manager_interface,3,&server,bind_data_manager));
    CHECK(wl_global_create(server.display,&xdg_wm_base_interface,1,&server,bind_wm));
    CHECK(wl_global_create(server.display,&zwp_text_input_manager_v3_interface,1,&server,bind_manager));
    const char *socket_name=wl_display_add_socket_auto(server.display);CHECK(socket_name);
    char socket_path[256];CHECK(snprintf(socket_path,sizeof(socket_path),"%s/%s",runtime,socket_name)>0);
    CHECK(setenv("WAYLAND_DISPLAY",socket_name,1)==0);pthread_t thread;CHECK(pthread_create(&thread,NULL,run_server,&server)==0);
    window_heap heap={0};nx_window_desc window_desc={0};window_desc.width=240;window_desc.height=120;window_desc.flags=NX_WINDOW_HIDDEN;
    window_desc.allocator=(nx_allocator){&heap,window_allocate,window_free};nx_window *window=NULL;
    CHECK(nx_window_create(&window_desc,&window)==NX_OK && nx_window_set_ime_transaction_events(window,1)==NX_OK);
    uint8_t bytes[512];font_fixture(bytes);nx_font *font=NULL;nx_font_collection *fonts=NULL;
    CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font)==NX_OK && nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    nx_context *ctx=NULL;nx_text_field *field=NULL;CHECK(nx_context_create(NULL,&ctx)==NX_OK && nx_text_field_create("ABCDE",5,4096,&field)==NX_OK);
    nx_text_field_desc desc={0};desc.id=81;desc.name=(nx_utf8_span){"Native atomic",13};desc.bounds=(nx_rect){0,0,220,40};desc.fonts=fonts;desc.pixels_per_em=16;desc.glyph_cache_bytes=65536;desc.ime_window=window;
    nx_event focus={.type=NX_EVENT_FOCUS,.focused=1};CHECK(nx_context_queue_event(ctx,&focus)==NX_OK);text_frame(ctx,&desc,field);
    CHECK(nx_focus_set(ctx,81)==NX_OK && nx_text_field_set_selection(field,5,5)==NX_OK);text_frame(ctx,&desc,field);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    send(&server,window,ctx,1);text_frame(ctx,&desc,field);expect_text(field,"ABCDx\xc3\xa9",7,7);
    undo(ctx);text_frame(ctx,&desc,field);expect_text(field,"ABCDE",5,5);
    for(int mode=2;mode<=5;++mode) {
        CHECK(nx_text_field_set_text(field,"ABCDE",5)==NX_OK && nx_text_field_set_selection(field,mode==4?4:5,mode==4?2:5)==NX_OK);
        text_frame(ctx,&desc,field);CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
        send(&server,window,ctx,mode);text_frame(ctx,&desc,field);
        if(mode==2)expect_text(field,"ABCDE",5,5);
        if(mode==3)expect_text(field,"ABCD",4,4);
        if(mode==4)expect_text(field,"ABE",2,2);
        if(mode==5){expect_text(field,"ABCDExy",7,7);undo(ctx);text_frame(ctx,&desc,field);expect_text(field,"ABCDEx",6,6);}
        undo(ctx);text_frame(ctx,&desc,field);expect_text(field,"ABCDE",mode==4?4:5,mode==4?2:5);
    }
    nx_text_field_destroy(field);nx_context_destroy(ctx);CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    nx_numeric_field *number=NULL;CHECK(nx_numeric_field_create(1234,4096,&number)==NX_OK && nx_numeric_field_set_ime_window(number,window)==NX_OK && nx_context_create(NULL,&ctx)==NX_OK);
    nx_numeric_field_desc numeric={0};numeric.id=181;numeric.name=(nx_utf8_span){"Number",6};numeric.bounds=desc.bounds;numeric.fonts=fonts;numeric.pixels_per_em=16;numeric.glyph_cache_bytes=65536;numeric.minimum=0;numeric.maximum=100000;
    CHECK(nx_context_queue_event(ctx,&focus)==NX_OK);number_frame(ctx,&numeric,number,1);CHECK(nx_focus_set(ctx,181)==NX_OK);number_frame(ctx,&numeric,number,1);
    for(int mode=6;mode<=7;++mode){
        CHECK(nx_numeric_field_set_value(number,1234)==NX_OK);number_frame(ctx,&numeric,number,1);CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
        send(&server,window,ctx,mode);number_frame(ctx,&numeric,number,mode==6);
        nx_utf8_span draft;double value=0;CHECK(nx_numeric_field_text(number,&draft)==NX_OK);
        CHECK(draft.length==(mode==6?5u:4u) && !memcmp(draft.data,mode==6?"12367":"123!",draft.length));
        CHECK(nx_numeric_field_value(number,&value)==NX_OK && value==1234);
        int accepted=-1;CHECK(nx_numeric_field_commit(number,numeric.minimum,numeric.maximum,&accepted)==NX_OK && accepted==(mode==6));
        CHECK(nx_numeric_field_value(number,&value)==NX_OK && value==(mode==6?12367:1234));
        undo(ctx);number_frame(ctx,&numeric,number,1);CHECK(nx_numeric_field_text(number,&draft)==NX_OK && draft.length==4 && !memcmp(draft.data,"1234",4));
    }
    nx_numeric_field_destroy(number);nx_context_destroy(ctx);CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && nx_text_field_create_password("AB",2,&field)==NX_OK);desc.password=1;
    CHECK(nx_context_queue_event(ctx,&focus)==NX_OK);text_frame(ctx,&desc,field);CHECK(nx_focus_set(ctx,81)==NX_OK && nx_text_field_set_selection(field,2,2)==NX_OK);text_frame(ctx,&desc,field);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0 && atomic_load(&server.password_content)>0);
    CHECK(atomic_load(&server.enable_commits)>0 && !server.surrounding_length);
    send(&server,window,ctx,5);text_frame(ctx,&desc,field);expect_text(field,"ABxy",4,4);
    uint32_t index=0;nx_semantic_view semantics;CHECK(nx_semantics_find(ctx,81,&index)==NX_OK && nx_semantics_get(ctx,index,&semantics)==NX_OK && !semantics.value.length && (semantics.state&NX_SEM_PROTECTED));
    undo(ctx);text_frame(ctx,&desc,field);expect_text(field,"ABxy",4,4);
    nx_text_field_destroy(field);nx_context_destroy(ctx);nx_font_collection_destroy(fonts);nx_font_close(font);
    nx_window_destroy(window);CHECK(!heap.live);wl_display_terminate(server.display);CHECK(pthread_join(thread,NULL)==0);wl_display_destroy(server.display);
    CHECK(unlink(socket_path)==0 || access(socket_path,F_OK)!=0);CHECK(rmdir(runtime)==0);
    puts("Native atomic IME:8 text/numeric/protected tasks, original selections and whole undo passed");return 0;
}
