/* Controlled Wayland server; actual installed NexaUI asynchronous transport. */
#include "wayland_v3_fixture.h"
#include "paste_review.h"

static void publish(test_server *server,nx_window *window) {
    atomic_store(&server->send_offer,1);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
}
static int width(uint32_t c){(void)c;return 1;}
static unsigned char *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);if(n<=0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}fclose(f);*size=(size_t)n;return p;
}
int main(int argc,char **argv) {
    CHECK(argc==2);size_t font_size=0;unsigned char *font_bytes=read_file(argv[1],&font_size);CHECK(font_bytes);
    nx_font *font=NULL;nx_font_collection *fonts=NULL;CHECK(nx_font_open(font_bytes,font_size,NULL,&font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    test_server server={0};char runtime[]="/tmp/nexaui-terminal-clipboard-XXXXXX";
    server.clipboard_auto_focus=1;CHECK(mkdtemp(runtime) && setenv("XDG_RUNTIME_DIR",runtime,1)==0);
    server.display=wl_display_create();CHECK(server.display);
    CHECK(wl_global_create(server.display,&wl_compositor_interface,1,&server,bind_compositor));
    CHECK(wl_global_create(server.display,&wl_shm_interface,1,&server,bind_shm));
    CHECK(wl_global_create(server.display,&wl_seat_interface,5,&server,bind_seat));
    CHECK(wl_global_create(server.display,&wl_data_device_manager_interface,3,&server,bind_data_manager));
    CHECK(wl_global_create(server.display,&xdg_wm_base_interface,1,&server,bind_wm));
    CHECK(wl_global_create(server.display,&zwp_text_input_manager_v3_interface,1,&server,bind_manager));
    const char *name=wl_display_add_socket_auto(server.display);CHECK(name);
    char path[256];CHECK(snprintf(path,sizeof(path),"%s/%s",runtime,name)>0);
    CHECK(setenv("WAYLAND_DISPLAY",name,1)==0);unsetenv("WAYLAND_SOCKET");
    pthread_t thread;CHECK(pthread_create(&thread,NULL,run_server,&server)==0);
    nx_window_desc desc={"terminal paste",240,120,NX_WINDOW_HIDDEN,{0}};
    nx_window *window=NULL;CHECK(nx_window_create(&desc,&window)==NX_OK);
    terminal_paste_flow flow;CHECK(terminal_paste_init(&flow,terminal_paste_wayland_transport(window))==NX_OK);
    terminal_paste_guard guard={11,1,1,1};atomic_store(&server.offer_multiline,1);publish(&server,window);
    CHECK(terminal_paste_begin(&flow,guard,2000)==NX_OK);int ready=0;enum ae_term_paste_result validation;
    for(unsigned i=0;i<30 && !ready;++i) {
        CHECK(nx_window_poll(window,100)==NX_OK);
        CHECK(terminal_paste_poll(&flow,guard,&ready,&validation)==NX_OK);
    }
    CHECK(ready && validation==AE_TERM_PASTE_OK && flow.paste.review && flow.paste.lines==2 &&
          !strcmp(flow.paste.text,"first\nsecond\tline"));
    nx_utf8_span output;CHECK(terminal_paste_confirm(&flow,guard,0,0,&output)==NX_ERROR_ARGUMENT);
    terminal_paste_review *review=NULL;CHECK(terminal_paste_review_create(&flow,fonts,width,&review)==NX_OK);
    enum terminal_review_result result;CHECK(terminal_paste_review_frame(review,guard,&result,&output)==NX_OK && result==TERMINAL_REVIEW_OPEN);
    nx_context *context=terminal_paste_review_context(review);CHECK(nx_focus_id(context)==10 && nx_focus_set(context,13)==NX_OK);
    nx_event event={.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_ENTER};CHECK(nx_context_queue_event(context,&event)==NX_OK);
    event.type=NX_EVENT_KEY_UP;CHECK(nx_context_queue_event(context,&event)==NX_OK);
    CHECK(terminal_paste_review_frame(review,guard,&result,&output)==NX_OK && result==TERMINAL_REVIEW_CONFIRMED &&
          output.length==17 && !memcmp(output.data,"first\nsecond\tline",17));
    terminal_paste_review_destroy(review);
    terminal_paste_reset(&flow);
    atomic_store(&server.offer_stall,1);publish(&server,window);
    CHECK(terminal_paste_begin(&flow,guard,2000)==NX_OK);
    for(unsigned i=0;i<20 && !atomic_load(&server.stalled_fd);++i)CHECK(nx_window_poll(window,100)==NX_OK);
    CHECK(atomic_load(&server.stalled_fd));terminal_paste_guard changed=guard;changed.focus_epoch++;
    CHECK(terminal_paste_poll(&flow,changed,&ready,&validation)==NX_ERROR_STATE && !flow.state && !flow.paste.size);
    int fd=atomic_exchange(&server.stalled_fd,0);CHECK(fd>0 && close(fd-1)==0);
    atomic_store(&server.offer_stall,0);publish(&server,window);ready=0;
    CHECK(terminal_paste_begin(&flow,guard,2000)==NX_OK);
    for(unsigned i=0;i<30 && !ready;++i){CHECK(nx_window_poll(window,100)==NX_OK);
        CHECK(terminal_paste_poll(&flow,guard,&ready,&validation)==NX_OK);}
    CHECK(ready && terminal_paste_review_create(&flow,fonts,width,&review)==NX_OK);
    CHECK(terminal_paste_review_frame(review,guard,&result,&output)==NX_OK && result==TERMINAL_REVIEW_OPEN);
    context=terminal_paste_review_context(review);event=(nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_ESCAPE};
    CHECK(nx_context_queue_event(context,&event)==NX_OK);
    CHECK(terminal_paste_review_frame(review,guard,&result,&output)==NX_OK && result==TERMINAL_REVIEW_CANCELLED && !output.data && !flow.state);
    terminal_paste_review_destroy(review);terminal_paste_reset(&flow);nx_window_destroy(window);
    wl_display_terminate(server.display);CHECK(pthread_join(thread,NULL)==0);wl_display_destroy(server.display);
    CHECK(unlink(path)==0 || access(path,F_OK)!=0);CHECK(rmdir(runtime)==0);
    nx_font_collection_destroy(fonts);nx_font_close(font);free(font_bytes);
    puts("Controlled Wayland async terminal paste and stale focus passed");return 0;
}
