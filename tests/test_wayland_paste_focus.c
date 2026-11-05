#include "wayland_v3_fixture.h"
#include "text_value.h"

typedef struct close_heap { nx_window *window; size_t live, frees; int close_on_free; } close_heap;
static void *heap_allocate(void *user,size_t size) {
    close_heap *heap=user;void *p=malloc(size);if(p)++heap->live;return p;
}
static void heap_free(void *user,void *pointer,size_t size) {
    (void)size;close_heap *heap=user;CHECK(heap->live);--heap->live;++heap->frees;
    if(heap->close_on_free) {heap->close_on_free=0;nx_window_destroy(heap->window);}
    free(pointer);
}

static int frame(nx_context *ctx, nx_text_field *first, nx_text_field *second,
                 nx_font_collection *fonts, nx_window *window) {
    nx_text_field_desc desc={0}; desc.bounds=(nx_rect){0,0,120,28};
    desc.name=(nx_utf8_span){"Field",5}; desc.fonts=fonts; desc.pixels_per_em=16;
    desc.glyph_cache_bytes=4096;
    if(nx_text_clipboard_for_window(window,&desc.clipboard)!=NX_OK || nx_begin_frame(ctx,200,100,0)!=NX_OK) return 0;
    nx_interaction_state state; int valid=0,submitted=0;
    desc.id=81;
    if(nx_text_field_widget(ctx,&desc,first,&state,&valid,&submitted)!=NX_OK) return 0;
    desc.id=82; desc.bounds.y=40;
    if(nx_text_field_widget(ctx,&desc,second,&state,&valid,&submitted)!=NX_OK) return 0;
    return nx_end_frame(ctx)==NX_OK;
}
static int text_is(nx_text_field *field,const char *value) {
    nx_utf8_span text;return nx_text_field_text(field,&text)==NX_OK &&
        text.length==strlen(value) && !memcmp(text.data,value,text.length);
}
static int paste(nx_context *ctx) {
    nx_event event={0};event.type=NX_EVENT_KEY_DOWN;event.key=NX_KEY_V;event.modifiers=NX_MOD_CONTROL;
    return nx_context_queue_event(ctx,&event)==NX_OK;
}
int main(void) {
    test_server server={0};char runtime[]="/tmp/nexaui-paste-focus-XXXXXX";
    server.clipboard_auto_focus=1;
    CHECK(mkdtemp(runtime) && setenv("XDG_RUNTIME_DIR",runtime,1)==0 && setenv("NEXAUI_PLATFORM","wayland",1)==0);
    unsetenv("WAYLAND_SOCKET"); server.display=wl_display_create();CHECK(server.display);
    CHECK(wl_global_create(server.display,&wl_compositor_interface,1,&server,bind_compositor));
    CHECK(wl_global_create(server.display,&wl_shm_interface,1,&server,bind_shm));
    CHECK(wl_global_create(server.display,&wl_seat_interface,5,&server,bind_seat));
    CHECK(wl_global_create(server.display,&wl_data_device_manager_interface,3,&server,bind_data_manager));
    CHECK(wl_global_create(server.display,&xdg_wm_base_interface,1,&server,bind_wm));
    const char *name=wl_display_add_socket_auto(server.display);CHECK(name);
    char path[256];CHECK(snprintf(path,sizeof(path),"%s/%s",runtime,name)>0 && setenv("WAYLAND_DISPLAY",name,1)==0);
    pthread_t thread;CHECK(pthread_create(&thread,NULL,run_server,&server)==0);
    close_heap heap={0};
    nx_window_desc desc={"focus paste",240,120,NX_WINDOW_HIDDEN,{&heap,heap_allocate,heap_free}};nx_window *window=NULL;
    CHECK(nx_window_create(&desc,&window)==NX_OK);
    heap.window=window;
    uint8_t font_bytes[512];font_fixture(font_bytes);nx_font *font=NULL;nx_font_collection *fonts=NULL;
    nx_text_field *first=NULL,*second=NULL;nx_context *ctx=NULL;
    CHECK(nx_font_open(font_bytes,sizeof(font_bytes),NULL,&font)==NX_OK && nx_font_collection_create(NULL,&fonts)==NX_OK);
    CHECK(nx_font_collection_add(fonts,font)==NX_OK && nx_text_field_create("a",1,1024,&first)==NX_OK);
    CHECK(nx_text_field_create("b",1,1024,&second)==NX_OK && nx_context_create(NULL,&ctx)==NX_OK);
    CHECK(frame(ctx,first,second,fonts,window) && nx_focus_set(ctx,82)==NX_OK && frame(ctx,first,second,fonts,window));
    atomic_store(&server.offer_stall,1);atomic_store(&server.send_offer,1);
    wl_surface_commit(nx_window_wayland_surface(window));CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    CHECK(paste(ctx) && frame(ctx,first,second,fonts,window));
    for(unsigned i=0;i<20 && !atomic_load(&server.stalled_fd);++i)CHECK(nx_window_poll(window,100)==NX_OK);
    int old=atomic_exchange(&server.stalled_fd,0);CHECK(old>0);
    /* Field 81 is submitted first, before field 82 can cancel its old request. */
    CHECK(nx_focus_set(ctx,81)==NX_OK && paste(ctx) && frame(ctx,first,second,fonts,window));
    for(unsigned i=0;i<20 && !atomic_load(&server.stalled_fd);++i)CHECK(nx_window_poll(window,100)==NX_OK);
    int fresh=atomic_exchange(&server.stalled_fd,0);CHECK(fresh>0 && close(fresh-1)==0);
    for(unsigned i=0;i<30 && !text_is(first,"aOnce");++i) {
        size_t routed=0;CHECK(nx_window_poll(window,100)==NX_OK && nx_window_route_events(window,ctx,&routed)==NX_OK);
        if(routed)CHECK(frame(ctx,first,second,fonts,window));
    }
    CHECK(text_is(first,"aOnce") && text_is(second,"b") && atomic_load(&server.offer_reads)==2);
    CHECK(close(old-1)==0);nx_text_field_destroy(first);nx_text_field_destroy(second);
    nx_context_destroy(ctx);nx_font_collection_destroy(fonts);nx_font_close(font);
    /* Retiring an obsolete offer is deferred out of the caller's frame. A
       reentrant window close from its allocator must be safe during poll. */
    atomic_store(&server.send_offer,1);wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    uint64_t token=0;CHECK(nx_linux_window_clipboard_async_start(window,1024,2000,0,&token)==NX_OK);
    for(unsigned i=0;i<20 && !atomic_load(&server.stalled_fd);++i)CHECK(nx_window_poll(window,100)==NX_OK);
    int pending=atomic_exchange(&server.stalled_fd,0);CHECK(pending>0);
    atomic_store(&server.send_offer,1);wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    size_t frees=heap.frees;
    CHECK(nx_linux_window_clipboard_async_cancel(window,token)==NX_OK && heap.frees==frees);
    heap.close_on_free=1;
    CHECK(nx_window_poll(window,0)==NX_ERROR_STATE && !heap.close_on_free && heap.live==0);
    CHECK(close(pending-1)==0);
    wl_display_terminate(server.display);CHECK(pthread_join(thread,NULL)==0);wl_display_destroy(server.display);
    CHECK(unlink(path)==0 || access(path,F_OK)!=0);CHECK(rmdir(runtime)==0);
    puts("Focus replacement supersedes the previous pending paste");return 0;
}
