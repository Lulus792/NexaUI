#include "wayland_v3_fixture.h"
#include "text_value.h"
#include "accessibility.h"
#include "nexaui/wayland_clipboard.h"
typedef struct validation_probe {unsigned calls;int reject;size_t start,end;} validation_probe;
typedef struct lease {unsigned refs,calls;int applied;} lease;
static int validate(nx_utf8_span old,size_t start,size_t end,nx_utf8_span inserted,void *user) {
    validation_probe *p=user;++p->calls;p->start=start;p->end=end;
    CHECK(old.length==3 && !memcmp(old.data,"abc",3));
    CHECK(inserted.length==17 && !memcmp(inserted.data,"first second\tline",17));
    return !p->reject;
}
static void retain(void *user){++((lease *)user)->refs;}
static void release(void *user){lease *p=user;CHECK(p->refs);--p->refs;}
static int current(void *user,uint64_t token,uint64_t identity,uint64_t id,nx_text_value_target target){
    (void)user;(void)token;(void)identity;(void)id;(void)target;return 1;
}
static int defer(void *user,uint32_t *remaining){(void)user;*remaining=5000;return 1;}
static void complete(void *user,int applied){lease *p=user;++p->calls;p->applied=applied;}
static void command(nx_context *ctx,nx_text_command command) {
    nx_event e={0};e.type=NX_EVENT_TEXT_COMMAND;e.text_command=command;CHECK(nx_context_queue_event(ctx,&e)==NX_OK);
}
static void frame(nx_context *ctx,nx_text_field *field,nx_text_field_desc *desc) {
    nx_interaction_state state;int valid=0,submitted=0;
    CHECK(nx_begin_frame(ctx,240,120,0)==NX_OK);
    CHECK(nx_text_field_widget(ctx,desc,field,&state,&valid,&submitted)==NX_OK && valid);
    CHECK(nx_end_frame(ctx)==NX_OK);
}
static void has(nx_text_field *field,const char *expected) {
    nx_utf8_span text;CHECK(nx_text_field_text(field,&text)==NX_OK && text.length==strlen(expected) && !memcmp(text.data,expected,text.length));
}
static void pump(nx_window *window,nx_context *ctx,nx_text_field *field,nx_text_field_desc *desc) {
    size_t routed=0;CHECK(nx_window_poll(window,100)==NX_OK && nx_window_route_events(window,ctx,&routed)==NX_OK);
    frame(ctx,field,desc);
}
static void offer(test_server *server,nx_window *window) {
    atomic_store(&server->send_offer,1);wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
}
static void paste(nx_context *ctx,unsigned route,lease *ticket) {
    if(route==0){nx_event e={0};e.type=NX_EVENT_KEY_DOWN;e.key=NX_KEY_V;e.modifiers=NX_MOD_CONTROL;CHECK(nx_context_queue_event(ctx,&e)==NX_OK);}
    else if(route==1)command(ctx,NX_TEXT_COMMAND_PASTE);
    else {nx_text_value_guard guard={0};guard.user=ticket;guard.token=1;guard.retain=retain;guard.release=release;guard.current=current;guard.defer_paste=defer;
        CHECK(nx_semantics_request_text_paste_with_completion(ctx,81,2,&guard,complete)==NX_OK);}
}
static void close_writer(test_server *server){int fd=atomic_exchange(&server->stalled_fd,0);CHECK(fd>0 && !close(fd-1));}
static void routes(test_server *server,nx_window *window,nx_font_collection *fonts,unsigned route) {
    nx_context *ctx=NULL;nx_text_field *field=NULL;lease ticket={0};validation_probe a={0,1,0,0},b={0,1,0,0};
    CHECK(nx_context_create(NULL,&ctx)==NX_OK && nx_text_field_create("ab",2,4096,&field)==NX_OK);
    nx_text_field_desc desc={0};desc.id=81;desc.bounds=(nx_rect){0,0,220,28};desc.name=(nx_utf8_span){"Name",4};
    desc.fonts=fonts;desc.pixels_per_em=16;desc.glyph_cache_bytes=4096;
    CHECK(nx_text_clipboard_for_window(window,&desc.clipboard)==NX_OK);
    CHECK(nx_text_field_set_paste_validator(field,validate,&a)==NX_OK);
    size_t routed=0;CHECK(nx_window_route_events(window,ctx,&routed)==NX_OK);frame(ctx,field,&desc);CHECK(nx_focus_set(ctx,81)==NX_OK);
    nx_event e={0};e.type=NX_EVENT_TEXT;e.scalar='c';CHECK(nx_context_queue_event(ctx,&e)==NX_OK);frame(ctx,field,&desc);has(field,"abc");
    CHECK(nx_text_field_set_selection(field,2,1)==NX_OK);frame(ctx,field,&desc);
    offer(server,window);paste(ctx,route,&ticket);frame(ctx,field,&desc);CHECK(!a.calls);has(field,"abc");
    for(unsigned i=0;i<30 && !a.calls;++i)pump(window,ctx,field,&desc);
    CHECK(a.calls==1 && a.start==(route==2?2u:1u) && a.end==2);has(field,"abc");
    size_t anchor=0,cursor=0;CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && anchor==2 && cursor==1);
    if(route==2)CHECK(ticket.calls==1 && !ticket.refs && !ticket.applied);
    command(ctx,NX_TEXT_COMMAND_UNDO);frame(ctx,field,&desc);has(field,"ab");
    command(ctx,NX_TEXT_COMMAND_REDO);frame(ctx,field,&desc);has(field,"abc");
    CHECK(nx_text_field_set_selection(field,2,1)==NX_OK);frame(ctx,field,&desc);
    a.reject=0;offer(server,window);paste(ctx,route,&ticket);frame(ctx,field,&desc);CHECK(a.calls==1);
    for(unsigned i=0;i<30 && a.calls<2;++i)pump(window,ctx,field,&desc);
    CHECK(a.calls==2);has(field,route==2?"abfirst second\tlinec":"afirst second\tlinec");
    if(route==2)CHECK(ticket.calls==2 && !ticket.refs && ticket.applied);
    command(ctx,NX_TEXT_COMMAND_UNDO);frame(ctx,field,&desc);has(field,"abc");
    CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK && anchor==2 && cursor==1);
    /* Partial data has arrived but EOF is held by the independent server FD. */
    atomic_store(&server->offer_stall,1);offer(server,window);paste(ctx,route,&ticket);frame(ctx,field,&desc);
    for(unsigned i=0;i<30 && !atomic_load(&server->stalled_fd);++i)pump(window,ctx,field,&desc);
    CHECK(atomic_load(&server->stalled_fd) && a.calls==2);
    CHECK(nx_text_field_set_paste_validator(field,validate,&b)==NX_OK && nx_text_field_set_paste_validator(field,validate,&a)==NX_OK);
    frame(ctx,field,&desc);close_writer(server);pump(window,ctx,field,&desc);has(field,"abc");CHECK(a.calls==2 && !b.calls);
    if(route==2)CHECK(ticket.calls==3 && !ticket.refs && !ticket.applied);
    /* Identical reconfiguration preserves an otherwise current pending paste. */
    offer(server,window);paste(ctx,route,&ticket);frame(ctx,field,&desc);
    for(unsigned i=0;i<30 && !atomic_load(&server->stalled_fd);++i)pump(window,ctx,field,&desc);
    CHECK(atomic_load(&server->stalled_fd));CHECK(nx_text_field_set_paste_validator(field,validate,&a)==NX_OK);
    close_writer(server);
    for(unsigned i=0;i<30 && a.calls<3;++i)pump(window,ctx,field,&desc);
    CHECK(a.calls==3);has(field,route==2?"abfirst second\tlinec":"afirst second\tlinec");
    CHECK(!ticket.refs);atomic_store(&server->offer_stall,0);
    nx_context_destroy(ctx);nx_text_field_destroy(field);
    atomic_store(&server->offer_stall,1);offer(server,window);
    uint64_t token=0;CHECK(nx_wayland_clipboard_read_begin(window,1024,1000,0,&token)==NX_OK && token);
    for(unsigned i=0;i<30 && !atomic_load(&server->stalled_fd);++i)CHECK(nx_window_poll(window,100)==NX_OK);
    CHECK(atomic_load(&server->stalled_fd));CHECK(nx_wayland_clipboard_read_cancel(window,token)==NX_OK);
    close_writer(server);atomic_store(&server->offer_stall,0);
}
int main(void) {
    test_server server={0};server.clipboard_auto_focus=1;atomic_store(&server.offer_multiline,1);
    char runtime[]="/tmp/nexaui-paste-policy-XXXXXX";CHECK(mkdtemp(runtime));CHECK(setenv("XDG_RUNTIME_DIR",runtime,1)==0);
    CHECK(setenv("NEXAUI_PLATFORM","wayland",1)==0);unsetenv("DISPLAY");unsetenv("WAYLAND_SOCKET");
    server.display=wl_display_create();CHECK(server.display);
    CHECK(wl_global_create(server.display,&wl_compositor_interface,1,&server,bind_compositor));
    CHECK(wl_global_create(server.display,&wl_shm_interface,1,&server,bind_shm));
    server.seat_global=wl_global_create(server.display,&wl_seat_interface,5,&server,bind_seat);CHECK(server.seat_global);
    CHECK(wl_global_create(server.display,&wl_data_device_manager_interface,3,&server,bind_data_manager));
    CHECK(wl_global_create(server.display,&xdg_wm_base_interface,1,&server,bind_wm));
    CHECK(wl_global_create(server.display,&zwp_text_input_manager_v3_interface,1,&server,bind_manager));
    const char *name=wl_display_add_socket_auto(server.display);CHECK(name);char path[256];CHECK(snprintf(path,sizeof(path),"%s/%s",runtime,name)>0);
    CHECK(setenv("WAYLAND_DISPLAY",name,1)==0);pthread_t thread;CHECK(!pthread_create(&thread,NULL,run_server,&server));
    window_heap heap={0};nx_window_desc desc={"paste validation",240,120,NX_WINDOW_HIDDEN,{&heap,window_allocate,window_free}};
    nx_window *window=NULL;CHECK(nx_window_create(&desc,&window)==NX_OK);
    uint8_t bytes[512];font_fixture(bytes);nx_font *font=NULL;nx_font_collection *fonts=NULL;
    CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font)==NX_OK && nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    for(unsigned route=0;route<3;++route)routes(&server,window,fonts,route);
    nx_font_collection_destroy(fonts);nx_font_close(font);nx_window_destroy(window);CHECK(!heap.live);
    wl_display_terminate(server.display);CHECK(!pthread_join(thread,NULL));wl_display_destroy(server.display);
    CHECK(unlink(path)==0 || access(path,F_OK)!=0);CHECK(!rmdir(runtime));
    puts("Native paste admission:3 async routes, rejection history, policy-change cancellation, no-op freshness and balanced lifetimes passed");return 0;
}
