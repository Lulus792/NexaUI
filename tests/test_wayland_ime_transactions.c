#include "wayland_v3_fixture.h"

static void commit_state(test_server *server, struct wl_resource *resource) {
    (void)resource;
    if (server->enabled) atomic_fetch_add(&server->enable_commits,1);
}

static void produce(test_server *server, struct wl_client *client) {
    (void)client;
    int test=atomic_exchange(&server->numeric_commit,0);
    if (!test) return;
    struct wl_resource *input=server->text_input;
    CHECK(input && server->enabled);
    if (test==8) {
        zwp_text_input_v3_send_preedit_string(input,"Gr\xc3\xbc\xc3\x9f" "e",2,4);
    } else {
        if (test!=5 && test!=7) zwp_text_input_v3_send_delete_surrounding_text(input,test==11?3:1,0);
        if (test!=4) {
            const char *text=test==2?"\x1b\n\x7f":test==3 || test==5?"":
                test==6?"a\nb":test==12?"\xc0":test==13?NULL:"x\xc3\xa9";
            zwp_text_input_v3_send_commit_string(input,text);
        }
    }
    zwp_text_input_v3_send_done(input,server->commit_serial-(test==10?1u:0u));
    if (test==7) {
        zwp_text_input_v3_send_commit_string(input,"!");
        zwp_text_input_v3_send_done(input,server->commit_serial);
    }
}

static void *foreign_thread(void *window) {
    CHECK(nx_window_set_ime_transaction_events(window,1)==NX_ERROR_STATE);
    return NULL;
}

static nx_event take(nx_window *window,nx_event_type type) {
    nx_event event;int available=0;
    CHECK(nx_window_next_event(window,&event,&available)==NX_OK && available && event.type==type);
    return event;
}

static void boundary(nx_window *window,nx_event_type type,uint32_t flags) {
    nx_event event=take(window,type);
    if (event.ime_target!=81 || event.ime_revision!=7 || event.ime_before!=flags)
        fprintf(stderr,"boundary type=%d target=%llu revision=%llu flags=%u expected=%u\n",
            (int)type,(unsigned long long)event.ime_target,(unsigned long long)event.ime_revision,event.ime_before,flags);
    CHECK(event.ime_target==81 && event.ime_revision==7 && event.ime_before==flags);
}

int main(int argc,char **argv) {
    int legacy=argc==2 && !strcmp(argv[1],"--legacy");
    CHECK(argc==1 || legacy);
    test_server server={0};server.text_committed=commit_state;server.surface_committed=produce;
    char runtime[]="/tmp/nexaui-ime-transactions-XXXXXX";
    CHECK(mkdtemp(runtime) && setenv("XDG_RUNTIME_DIR",runtime,1)==0 && setenv("NEXAUI_PLATFORM","wayland",1)==0);
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
    window_heap heap={0};nx_window *window=NULL;
    nx_window_desc desc={0};desc.width=240;desc.height=120;desc.flags=NX_WINDOW_HIDDEN;
    desc.allocator=(nx_allocator){&heap,window_allocate,window_free};
    CHECK(nx_window_create(&desc,&window)==NX_OK && window);
    CHECK(nx_window_set_ime_transaction_events(NULL,1)==NX_ERROR_ARGUMENT);
    CHECK(nx_window_set_ime_transaction_events(window,2)==NX_ERROR_ARGUMENT);
    if (!legacy) {
        CHECK(nx_window_set_ime_transaction_events(window,1)==NX_OK);
        CHECK(nx_window_set_ime_transaction_events(window,0)==NX_OK);
        CHECK(nx_window_set_ime_transaction_events(window,1)==NX_OK);
    }
    CHECK(nx_window_set_ime_surrounding_text(window,81,7,"ab\xc3\xa9" "cd",6,6,6)==NX_OK);
    CHECK(nx_window_set_ime_enabled(window,1)==NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
    CHECK(nx_window_set_ime_transaction_events(window,!legacy)==NX_OK);
    CHECK(nx_window_set_ime_transaction_events(window,legacy)==NX_ERROR_STATE);
    pthread_t foreign;CHECK(pthread_create(&foreign,NULL,foreign_thread,window)==0 && pthread_join(foreign,NULL)==0);
    nx_context *context=NULL;CHECK(nx_context_create(NULL,&context)==NX_OK);
    nx_event invalid={0};invalid.type=NX_EVENT_IME_TRANSACTION_END;
    CHECK(nx_context_queue_event(context,&invalid)==NX_ERROR_ARGUMENT);
    invalid.ime_before=NX_IME_TRANSACTION_COMMIT;invalid.ime_target=81;
    CHECK(nx_context_queue_event(context,&invalid)==NX_ERROR_ARGUMENT);
    invalid.ime_revision=7;invalid.ime_before=8;
    CHECK(nx_context_queue_event(context,&invalid)==NX_ERROR_ARGUMENT);
    invalid.ime_before=NX_IME_TRANSACTION_COMMIT;
    CHECK(nx_context_queue_event(context,&invalid)==NX_OK && nx_begin_frame(context,1,1,0)==NX_OK && nx_end_frame(context)==NX_OK);
    nx_context_destroy(context);
    for (int test=1;test<=13;++test) {
        printf("v3 transaction case %d legacy=%d\n",test,legacy);fflush(stdout);
        atomic_store(&server.numeric_commit,test);
        wl_surface_commit(nx_window_wayland_surface(window));
        CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
        if (test==8) {
            nx_event event=take(window,NX_EVENT_IME_PREEDIT_CHANGED);
            CHECK(event.ime_target==81 && event.ime_revision==7);
        } else if (test!=10 && test!=11) {
            uint32_t flags=(test==4?0u:NX_IME_TRANSACTION_COMMIT) |
                (test==5 || test==7?0u:NX_IME_TRANSACTION_DELETE) |
                (test==2 || test==6 || test==12?NX_IME_TRANSACTION_REJECTED:0u);
            if (!legacy) boundary(window,NX_EVENT_IME_TRANSACTION_BEGIN,flags);
            if (test!=5 && test!=7) {
                nx_event event=take(window,NX_EVENT_IME_DELETE_SURROUNDING);
                CHECK(event.ime_target==81 && event.ime_revision==7 && event.ime_before==1 && !event.ime_after);
            }
            if (test==1 || test==6 || test==7 || test==9) {
                const uint32_t scalars[]={test==6?'a':'x',test==6?'b':0xe9};
                for (size_t i=0;i<2;++i) {
                    nx_event event=take(window,NX_EVENT_TEXT);
                    CHECK(event.ime_target==81 && event.ime_revision==7 && event.scalar==scalars[i]);
                    CHECK(event.ime_before==((i?NX_IME_COMMIT_END:NX_IME_COMMIT_START) |
                        (test==6?NX_IME_COMMIT_FILTERED:0u)));
                }
            }
            if (test==9) (void)take(window,NX_EVENT_IME_PREEDIT_CHANGED);
            if (!legacy) boundary(window,NX_EVENT_IME_TRANSACTION_END,flags);
            if (test==7) {
                if (!legacy) boundary(window,NX_EVENT_IME_TRANSACTION_BEGIN,NX_IME_TRANSACTION_COMMIT);
                nx_event event=take(window,NX_EVENT_TEXT);CHECK(event.scalar=='!' && event.ime_before==(NX_IME_COMMIT_START|NX_IME_COMMIT_END));
                if (!legacy) boundary(window,NX_EVENT_IME_TRANSACTION_END,NX_IME_TRANSACTION_COMMIT);
            }
        }
        nx_event event;int available=0;CHECK(nx_window_next_event(window,&event,&available)==NX_OK && !available);
    }
    CHECK(nx_window_set_ime_enabled(window,0)==NX_OK);
    CHECK(nx_window_set_ime_transaction_events(window,legacy)==NX_ERROR_STATE);
    nx_window_destroy(window);CHECK(!heap.live);
    wl_display_terminate(server.display);CHECK(pthread_join(thread,NULL)==0);wl_display_destroy(server.display);
    CHECK(unlink(socket_path)==0 || access(socket_path,F_OK)!=0);CHECK(rmdir(runtime)==0);
    puts(legacy?"Legacy IME stream:13 wire transactions passed":"Opt-in IME boundaries:13 wire transactions passed");
    return 0;
}
