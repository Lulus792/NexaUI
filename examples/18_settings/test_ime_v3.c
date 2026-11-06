#include "../../tests/wayland_v3_fixture.h"
#include "scene.h"
#include "text_value.h"
#include <time.h>

static nx_result fixture_poll(nx_window *window,uint32_t timeout) {
    if(getenv("NEXAUI_TEST_POLL_MANY")) {
        uint32_t events=0;
        return nx_window_poll_many(&window,1,timeout,-1,&events);
    }
    return nx_window_poll(window,timeout);
}

static int wait_stalled_offer(test_server *server,nx_window *window) {
    /* The old 20 * 100ms setup loop could finish immediately while native
       events remained queued. Keep its two-second setup budget in real time
       and let the fixture producer run, while still pumping the tested API. */
    struct timespec start,now;
    if(clock_gettime(CLOCK_MONOTONIC,&start)!=0)return 0;
    for(;;) {
        if(clock_gettime(CLOCK_MONOTONIC,&now)!=0)return 0;
        double elapsed=(double)(now.tv_sec-start.tv_sec)+(now.tv_nsec-start.tv_nsec)/1e9;
        if(elapsed>=2.0)return 0;
        if(atomic_load(&server->stalled_fd))return 1;
        if(fixture_poll(window,0)!=NX_OK)return 0;
        struct timespec yield={0,1000000};
        while(nanosleep(&yield,&yield)!=0)if(errno!=EINTR)return 0;
    }
}

typedef struct paste_ticket { int refs,calls,applied,deferred,revoked; uint32_t timeout; } paste_ticket;
typedef struct delayed_allocator { int armed,delayed; } delayed_allocator;
static void *delayed_allocate(void *p,size_t size) {
    delayed_allocator *a=p;
    if(a->armed && size==8192u) {
        a->armed=0;a->delayed=1;
        struct timespec wait={5,200000000};
        while(nanosleep(&wait,&wait)!=0) {}
    }
    return malloc(size);
}
static void delayed_deallocate(void *p,void *memory,size_t size) {(void)p;(void)size;free(memory);}
static void paste_retain(void *p){++((paste_ticket *)p)->refs;}
static void paste_release(void *p){--((paste_ticket *)p)->refs;}
static int paste_current(void *p,uint64_t token,uint64_t context,uint64_t id,nx_text_value_target target) {
    (void)token;(void)context;(void)id;(void)target;return !((paste_ticket *)p)->revoked;
}
static void paste_complete(void *p,int applied){paste_ticket *t=p;++t->calls;t->applied=applied;}
static int paste_defer(void *p,uint32_t *remaining_ms){paste_ticket *t=p;if(t->revoked)return 0;*remaining_ms=t->timeout?t->timeout:2000;t->deferred=1;return 1;}

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
          !strcmp(argv[2],"--clipboard-once") || !strcmp(argv[2],"--keyboard-clipboard-once") ||
          !strcmp(argv[2],"--clipboard-stall") || !strcmp(argv[2],"--keyboard-clipboard-stall") ||
          !strcmp(argv[2],"--clipboard-slow-allocator") || !strcmp(argv[2],"--keyboard-clipboard-focus") ||
          !strcmp(argv[2],"--keyboard-clipboard-model") || !strcmp(argv[2],"--keyboard-clipboard-selection") ||
          !strcmp(argv[2],"--keyboard-clipboard-timeout") || !strcmp(argv[2],"--keyboard-clipboard-command") ||
          !strcmp(argv[2],"--keyboard-clipboard-abandoned") || !strcmp(argv[2],"--keyboard-clipboard-ime") ||
          !strcmp(argv[2],"--keyboard-clipboard-revert") || !strcmp(argv[2],"--keyboard-clipboard-repeat") ||
          !strcmp(argv[2],"--keyboard-clipboard-window-focus") || !strcmp(argv[2],"--ax-async-once") ||
          !strcmp(argv[2],"--ax-async-stall") || !strcmp(argv[2],"--ax-async-short") || !strcmp(argv[2],"--ax-async-input") ||
          !strcmp(argv[2],"--ax-async-model") || !strcmp(argv[2],"--ax-async-detach") ||
          !strcmp(argv[2],"--ax-async-omit"))));
    int external=argc==3 && !strcmp(argv[2],"--external");
    int same=argc==3 && !strcmp(argv[2],"--same");
    int once=argc==3 && !strcmp(argv[2],"--clipboard-once");
    int keyboard_once=argc==3 && !strcmp(argv[2],"--keyboard-clipboard-once");
    int stall=argc==3 && !strcmp(argv[2],"--clipboard-stall");
    int keyboard_stall=argc==3 && !strcmp(argv[2],"--keyboard-clipboard-stall");
    int keyboard_focus=argc==3 && !strcmp(argv[2],"--keyboard-clipboard-focus");
    int keyboard_model=argc==3 && !strcmp(argv[2],"--keyboard-clipboard-model");
    int keyboard_selection=argc==3 && !strcmp(argv[2],"--keyboard-clipboard-selection");
    int keyboard_timeout=argc==3 && !strcmp(argv[2],"--keyboard-clipboard-timeout");
    int keyboard_command=argc==3 && !strcmp(argv[2],"--keyboard-clipboard-command");
    int keyboard_abandoned=argc==3 && !strcmp(argv[2],"--keyboard-clipboard-abandoned");
    int keyboard_ime=argc==3 && !strcmp(argv[2],"--keyboard-clipboard-ime");
    int keyboard_revert=argc==3 && !strcmp(argv[2],"--keyboard-clipboard-revert");
    int keyboard_repeat=argc==3 && !strcmp(argv[2],"--keyboard-clipboard-repeat");
    int keyboard_window_focus=argc==3 && !strcmp(argv[2],"--keyboard-clipboard-window-focus");
    int ax_once=argc==3 && !strcmp(argv[2],"--ax-async-once");
    int ax_stall=argc==3 && !strcmp(argv[2],"--ax-async-stall");
    int ax_short=argc==3 && !strcmp(argv[2],"--ax-async-short");
    int ax_input=argc==3 && !strcmp(argv[2],"--ax-async-input");
    int ax_model=argc==3 && !strcmp(argv[2],"--ax-async-model");
    int ax_detach=argc==3 && !strcmp(argv[2],"--ax-async-detach");
    int ax_omit=argc==3 && !strcmp(argv[2],"--ax-async-omit");
    int ax_async=ax_once || ax_stall || ax_short || ax_input || ax_model || ax_detach || ax_omit;
    int slow_allocator=argc==3 && !strcmp(argv[2],"--clipboard-slow-allocator");
    size_t length=0; unsigned char *font=read_font(argv[1],&length); CHECK(font);
    test_server server={0}; server.width=640; server.height=360; server.clipboard_auto_focus=1;
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
    delayed_allocator allocation={0};
    nx_window *window=NULL; nx_window_desc desc={"Settings IME test",640,360,NX_WINDOW_HIDDEN,{0}};
    if(slow_allocator)desc.allocator=(nx_allocator){&allocation,delayed_allocate,delayed_deallocate};
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
    if(once || keyboard_once || stall || keyboard_stall || keyboard_focus || keyboard_model || keyboard_selection || keyboard_timeout || keyboard_command || keyboard_abandoned || keyboard_ime || keyboard_revert || keyboard_repeat || keyboard_window_focus || ax_async || slow_allocator) {
        CHECK(nx_focus_set(ctx,3)==NX_OK && settings_frame(scene,640,360));
        atomic_store(&server.offer_once,!keyboard_repeat);atomic_store(&server.offer_stall,stall || keyboard_stall || keyboard_focus || keyboard_model || keyboard_selection || keyboard_timeout || keyboard_abandoned || keyboard_ime || keyboard_revert || keyboard_window_focus || (ax_async && !ax_once));
        atomic_store(&server.send_offer,1);
        wl_surface_commit(nx_window_wayland_surface(window));
        CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
        paste_ticket ticket={0};if(ax_short)ticket.timeout=100;
        nx_text_value_guard guard={&ticket,1,paste_retain,paste_release,paste_current,0};
        if(ax_async)guard.defer_paste=paste_defer;
        if(once || stall || slow_allocator || ax_async)CHECK(nx_semantics_request_text_paste_with_completion(ctx,3,0,&guard,paste_complete)==NX_OK);
        else {nx_event event={0};event.type=keyboard_command?NX_EVENT_TEXT_COMMAND:NX_EVENT_KEY_DOWN;
              if(keyboard_command)event.text_command=NX_TEXT_COMMAND_PASTE;
              else {event.key=NX_KEY_V;event.modifiers=NX_MOD_CONTROL;}
              CHECK(nx_context_queue_event(ctx,&event)==NX_OK);
              if(keyboard_repeat)CHECK(nx_context_queue_event(ctx,&event)==NX_OK);}
        struct timespec start,finish; CHECK(clock_gettime(CLOCK_MONOTONIC,&start)==0);
        allocation.armed=slow_allocator;
        CHECK(settings_frame(scene,640,360));
        CHECK(clock_gettime(CLOCK_MONOTONIC,&finish)==0);
        if(ax_async) {
            double elapsed=(double)(finish.tv_sec-start.tv_sec)+(finish.tv_nsec-start.tv_nsec)/1e9;
            CHECK(elapsed<1.0 && ticket.deferred && ticket.refs==1 && !ticket.calls);
            if(!ax_once) {
                CHECK(wait_stalled_offer(&server,window));
                CHECK(atomic_load(&server.stalled_fd) && value_is(ctx,"Aethericaf\xc3\xa9"));
                if(ax_input) {
                    nx_event event={0};event.type=NX_EVENT_TEXT;event.scalar='X';
                    CHECK(nx_context_queue_event(ctx,&event)==NX_OK && settings_frame(scene,640,360));
                } else if(ax_model) {
                    CHECK(settings_set_draft_name(scene,"Changed",7) && settings_frame(scene,640,360));
                } else if(ax_detach) { ticket.revoked=1;CHECK(settings_frame(scene,640,360)); }
                else if(ax_omit) {
                    CHECK(nx_begin_frame(ctx,640,360,0)==NX_OK && nx_end_frame(ctx)==NX_OK);
                    CHECK(ticket.calls==1 && !ticket.applied && ticket.refs==0);
                    CHECK(nx_linux_window_clipboard_async_cancel_active(window)==NX_ERROR_STATE);
                    CHECK(settings_frame(scene,640,360));
                }
                else {
                    if(ax_short && getenv("NEXAUI_TEST_DELAY_SHORT_FOCUS")) {
                        struct timespec delay={0,150000000};
                        while(nanosleep(&delay,&delay)!=0) {}
                        CHECK(fixture_poll(window,0)==NX_OK);
                        CHECK(nx_window_route_events(window,ctx,&routed)==NX_OK);
                    }
                    CHECK(nx_focus_set(ctx,4)==NX_OK && settings_frame(scene,640,360));
                    if(ax_short && getenv("NEXAUI_TEST_DELAY_SHORT_FOCUS"))
                        CHECK(ticket.calls==1 && !ticket.applied && ticket.refs==0);
                    /* The short deadline may expire while the event loop is
                       waiting for the stalled offer or before this frame. */
                    if(ax_short && ticket.calls)
                        CHECK(ticket.calls==1 && !ticket.applied && ticket.refs==0);
                    else
                        CHECK(!ticket.calls && ticket.refs==1);
                }
            }
            for(unsigned i=0;i<40 && !ticket.calls;++i) {
                CHECK(fixture_poll(window,100)==NX_OK && nx_window_route_events(window,ctx,&routed)==NX_OK);
                if(routed)CHECK(settings_frame(scene,640,360));
            }
            CHECK(ticket.calls==1 && ticket.refs==0 && ticket.applied==ax_once);
            if(ax_short) {
                CHECK(clock_gettime(CLOCK_MONOTONIC,&finish)==0);
                CHECK((double)(finish.tv_sec-start.tv_sec)+(finish.tv_nsec-start.tv_nsec)/1e9<1.5);
                CHECK(nx_linux_window_clipboard_async_cancel_active(window)==NX_ERROR_STATE);
            }
            CHECK(value_is(ctx,ax_once?"OnceAethericaf\xc3\xa9":ax_input?"Aethericaf\xc3\xa9X":ax_model?"Changed":"Aethericaf\xc3\xa9"));
            if(!ax_once) {int fd=atomic_exchange(&server.stalled_fd,0);CHECK(fd>0 && close(fd-1)==0);}
        } else if(keyboard_stall || keyboard_focus || keyboard_model || keyboard_selection || keyboard_timeout || keyboard_abandoned || keyboard_ime || keyboard_revert || keyboard_window_focus) {
            double elapsed=(double)(finish.tv_sec-start.tv_sec)+(finish.tv_nsec-start.tv_nsec)/1e9;
            CHECK(elapsed<1.0 && value_is(ctx,"Aethericaf\xc3\xa9"));
            CHECK(wait_stalled_offer(&server,window));
            CHECK(atomic_load(&server.stalled_fd) && atomic_load(&server.offer_reads)==1);
            if(keyboard_abandoned) {
                settings_destroy(scene); scene=NULL;
                int fd=atomic_exchange(&server.stalled_fd,0); CHECK(fd>0 && close(fd-1)==0);
                int woke=0;
                for(unsigned i=0;i<30 && !woke;++i) {
                    CHECK(fixture_poll(window,100)==NX_OK);
                    nx_event event; int available=0;
                    do { CHECK(nx_window_next_event(window,&event,&available)==NX_OK);
                         if(available && event.type==NX_EVENT_WAKE) woke=1; } while(available);
                }
                CHECK(woke);
                struct timespec before,after; CHECK(clock_gettime(CLOCK_MONOTONIC,&before)==0);
                CHECK(fixture_poll(window,100)==NX_OK && clock_gettime(CLOCK_MONOTONIC,&after)==0);
                CHECK((double)(after.tv_sec-before.tv_sec)+(after.tv_nsec-before.tv_nsec)/1e9 >= 0.05);
                uint64_t new_token=0; nx_result availability=NX_ERROR_LIMIT;
                for(unsigned i=0;i<60 && availability==NX_ERROR_LIMIT;++i) {
                    CHECK(fixture_poll(window,100)==NX_OK);
                    availability=nx_linux_window_clipboard_async_start(window,1024,1000,0,&new_token);
                }
                CHECK(availability==NX_OK && new_token && nx_linux_window_clipboard_async_cancel(window,new_token)==NX_OK);
                goto cleanup;
            }
            if(keyboard_timeout) {
                int woke=0;
                for(unsigned i=0;i<80 && !woke;++i) {
                    CHECK(fixture_poll(window,100)==NX_OK);
                    nx_event event; int available=0; size_t count=0;
                    do {
                        CHECK(nx_window_next_event(window,&event,&available)==NX_OK);
                        if(available) { if(event.type==NX_EVENT_WAKE) woke=1; CHECK(nx_context_queue_event(ctx,&event)==NX_OK); ++count; }
                    } while(available);
                    if(count) CHECK(settings_frame(scene,640,360));
                }
                CHECK(woke && value_is(ctx,"Aethericaf\xc3\xa9"));
            } else if(keyboard_focus) {
                CHECK(nx_focus_set(ctx,4)==NX_OK && nx_focus_set(ctx,3)==NX_OK && settings_frame(scene,640,360));
            } else if(keyboard_window_focus) {
                nx_event event={0};event.type=NX_EVENT_FOCUS;event.focused=0;
                CHECK(nx_context_queue_event(ctx,&event)==NX_OK); event.focused=1;
                CHECK(nx_context_queue_event(ctx,&event)==NX_OK && settings_frame(scene,640,360));
            } else if(keyboard_model) {
                CHECK(settings_set_draft_name(scene,"Changed",7) && settings_frame(scene,640,360));
            } else if(keyboard_ime) {
                nx_event event={0};event.type=NX_EVENT_IME_PREEDIT_CHANGED;
                CHECK(nx_context_queue_event(ctx,&event)==NX_OK && settings_frame(scene,640,360));
            } else if(keyboard_selection) {
                nx_event event={0};event.type=NX_EVENT_KEY_DOWN;event.key=NX_KEY_LEFT;
                CHECK(nx_context_queue_event(ctx,&event)==NX_OK && settings_frame(scene,640,360));
            } else if(keyboard_revert) {
                nx_event event={0};event.type=NX_EVENT_TEXT;event.scalar='X';
                CHECK(nx_context_queue_event(ctx,&event)==NX_OK);
                event=(nx_event){0};event.type=NX_EVENT_KEY_DOWN;event.key=NX_KEY_BACKSPACE;
                CHECK(nx_context_queue_event(ctx,&event)==NX_OK && settings_frame(scene,640,360));
            } else {
                nx_event event={0};event.type=NX_EVENT_TEXT;event.scalar='X';
                CHECK(nx_context_queue_event(ctx,&event)==NX_OK && settings_frame(scene,640,360));
            }
            int fd=atomic_exchange(&server.stalled_fd,0);CHECK(fd>0);CHECK(close(fd-1)==0);
            for(unsigned i=0;i<20;++i) {
                CHECK(fixture_poll(window,20)==NX_OK && nx_window_route_events(window,ctx,&routed)==NX_OK);
                if(routed)CHECK(settings_frame(scene,640,360));
                if(!keyboard_ime || value_is(ctx,"Aethericaf\xc3\xa9Once"))break;
            }
            CHECK(settings_frame(scene,640,360));
            CHECK(value_is(ctx,keyboard_ime?"Aethericaf\xc3\xa9Once":(keyboard_focus || keyboard_window_focus || keyboard_selection || keyboard_timeout || keyboard_revert)?"Aethericaf\xc3\xa9":keyboard_model?"Changed":"Aethericaf\xc3\xa9X"));
        } else if(stall || slow_allocator) {
            double elapsed=(double)(finish.tv_sec-start.tv_sec)+(finish.tv_nsec-start.tv_nsec)/1e9;
            CHECK(elapsed>=4.0 && elapsed<9.0 && atomic_load(&server.offer_reads)==1);
            CHECK(ticket.calls==1 && !ticket.applied && ticket.refs==0);
            CHECK(value_is(ctx,"Aethericaf\xc3\xa9"));
            if(slow_allocator) CHECK(allocation.delayed);
            else {int fd=atomic_exchange(&server.stalled_fd,0);CHECK(fd>0);CHECK(close(fd-1)==0);}
            nx_event event={0};event.type=NX_EVENT_TEXT;event.scalar='X';
            CHECK(nx_context_queue_event(ctx,&event)==NX_OK && settings_frame(scene,640,360));
            CHECK(value_is(ctx,"Aethericaf\xc3\xa9X"));
        } else {
        if(keyboard_once || keyboard_command || keyboard_repeat) {
            for(unsigned i=0;i<40 && !value_is(ctx,keyboard_repeat?"Aethericaf\xc3\xa9OnceOnce":"Aethericaf\xc3\xa9Once");++i) {
                CHECK(fixture_poll(window,100)==NX_OK && nx_window_route_events(window,ctx,&routed)==NX_OK);
                if(routed) CHECK(settings_frame(scene,640,360));
            }
        }
        if(once)CHECK(ticket.calls==1 && ticket.applied==1 && ticket.refs==0);
        fprintf(stderr,"one-shot clipboard: reads=%d expected-value=%d\n",
            atomic_load(&server.offer_reads),value_is(ctx,once?"OnceAethericaf\xc3\xa9":"Aethericaf\xc3\xa9Once"));
        CHECK(value_is(ctx,once?"OnceAethericaf\xc3\xa9":keyboard_repeat?"Aethericaf\xc3\xa9OnceOnce":"Aethericaf\xc3\xa9Once") && atomic_load(&server.offer_reads)==(keyboard_repeat?2:1));
        }
    }
cleanup:
    settings_destroy(scene); nx_window_destroy(window); free(font);
    wl_display_terminate(server.display); CHECK(pthread_join(thread,NULL)==0);
    wl_display_destroy(server.display);
    CHECK(unlink(socket_path)==0 || access(socket_path,F_OK)!=0); CHECK(rmdir(runtime)==0);
    puts("Settings text-input-v3 commit reached the focused scene exactly once."); return 0;
}
