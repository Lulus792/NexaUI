#include "wayland_v3_fixture.h"
#include "text_value.h"
#include "nexaui/wayland_clipboard.h"
#include <time.h>

typedef struct document {
    char bytes[128];size_t length;
    uint64_t revision;uint32_t operations;
    int multiline,reject;unsigned applied,validated;
} document;
typedef struct document_lease {unsigned refs,calls;int live,applied;uint64_t deadline;} document_lease;
static uint64_t milliseconds(void) {
    struct timespec t;CHECK(!clock_gettime(CLOCK_MONOTONIC,&t));
    return (uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u;
}
static void retain(void *user){++((document_lease *)user)->refs;}
static void release(void *user){document_lease *l=user;CHECK(l->refs);--l->refs;}
static int current(void *user,uint64_t token,uint64_t identity,uint64_t id,nx_text_value_target target) {
    (void)token;(void)identity;(void)id;(void)target;document_lease *l=user;
    return l->live && milliseconds()<l->deadline;
}
static int defer(void *user,uint32_t *remaining) {
    document_lease *l=user;uint64_t now=milliseconds();
    if(!l->live || now>=l->deadline)return 0;
    *remaining=(uint32_t)(l->deadline-now);return 1;
}
static void complete(void *user,int applied){document_lease *l=user;++l->calls;l->applied=applied;}
static nx_text_document_state state(document *d) {
    return (nx_text_document_state){{d->bytes,d->length},19,d->revision,0,0,d->operations,d->multiline};
}
static nx_result apply(void *user,const nx_text_document_request *r,nx_text_document_state *next) {
    document *d=user;
    if(r->identity!=19 || r->revision!=d->revision || r->operation!=NX_TEXT_DOCUMENT_REPLACE_RANGE ||
       r->start_byte!=2 || r->end_byte!=2 || d->length+r->value.length>sizeof(d->bytes))return NX_ERROR_STATE;
    memmove(d->bytes+2+r->value.length,d->bytes+2,d->length-2);
    memcpy(d->bytes+2,r->value.data,r->value.length);d->length+=r->value.length;
    ++d->revision;++d->applied;*next=state(d);return NX_OK;
}
static int validate(nx_utf8_span before,size_t start,size_t end,nx_utf8_span inserted,void *user) {
    document *d=user;++d->validated;
    CHECK(before.length==3 && !memcmp(before.data,"abc",3) && start==2 && end==2);
    const char *expected=d->multiline?"first\r\nsecond\tline":"first second\tline";
    CHECK(inserted.length==strlen(expected) && !memcmp(inserted.data,expected,inserted.length));
    return !d->reject;
}
static void frame(nx_context *ctx,nx_window *window,document *d,int omit) {
    CHECK(nx_begin_frame(ctx,240,120,0)==NX_OK);
    if(!omit) {
        nx_widget_desc w={0};w.semantic.id=81;w.semantic.role=NX_ROLE_TEXT_FIELD;
        w.semantic.bounds=(nx_rect){0,0,220,80};w.semantic.name=(nx_utf8_span){"Document",8};
        w.semantic.actions=NX_ACTION_SET_VALUE|NX_ACTION_FOCUS;w.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE;
        nx_text_document_state s=state(d);nx_text_document_clipboard clipboard={0};
        CHECK(nx_text_clipboard_for_window(window,&clipboard.clipboard)==NX_OK);
        clipboard.validate=validate;clipboard.validate_user=d;nx_text_document_result result;
        CHECK(nx_text_document_register_with_clipboard(ctx,&w,&s,apply,d,&clipboard,&result)==NX_OK);
    }
    CHECK(nx_end_frame(ctx)==NX_OK);
}
static void pump(nx_context *ctx,nx_window *window,document *d) {
    size_t routed=0;CHECK(nx_window_poll(window,20)==NX_OK && nx_window_route_events(window,ctx,&routed)==NX_OK);
    frame(ctx,window,d,0);
}
static void offer(test_server *server,nx_window *window,int enter) {
    atomic_store(&server->send_offer,1);
    if(enter)atomic_store(&server->keyboard_enter_requested,1);
    wl_surface_commit(nx_window_wayland_surface(window));CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);
}
static void scenario(test_server *server,nx_window *window,unsigned mode) {
    document d={0};memcpy(d.bytes,"abc",3);d.length=3;d.revision=1;
    d.operations=NX_TEXT_DOCUMENT_REPLACE|NX_TEXT_DOCUMENT_PASTE;d.multiline=mode==1;
    document_lease l={0};l.live=1;l.deadline=milliseconds()+1000u;
    nx_context *ctx=NULL;CHECK(nx_context_create(NULL,&ctx)==NX_OK);
    frame(ctx,window,&d,0);atomic_store(&server->offer_stall,1);offer(server,window,mode>0);
    nx_text_value_guard guard={0};guard.user=&l;guard.token=1;guard.retain=retain;guard.release=release;
    guard.current=current;guard.defer_paste=defer;
    CHECK(nx_semantics_request_text_paste_with_completion(ctx,81,2,&guard,complete)==NX_OK);
    frame(ctx,window,&d,0);CHECK(!d.applied && !d.validated && l.refs==1 && !l.calls);
    for(unsigned i=0;i<30 && !atomic_load(&server->stalled_fd);++i)pump(ctx,window,&d);
    CHECK(atomic_load(&server->stalled_fd) && !d.applied && !d.validated && !l.calls);
    /* Several completed frames and partial provider bytes are not a completion. */
    for(unsigned i=0;i<3;++i)pump(ctx,window,&d);
    CHECK(!d.applied && !l.calls && l.refs==1);
    switch(mode) {
    case 2:d.reject=1;break;
    case 3:++d.revision;break;
    case 4:d.operations=NX_TEXT_DOCUMENT_REPLACE;frame(ctx,window,&d,0);d.operations|=NX_TEXT_DOCUMENT_PASTE;break;
    case 5:frame(ctx,window,&d,1);break;
    case 6:l.live=0;break;
    case 7:
        atomic_store(&server->keyboard_leave_requested,1);wl_surface_commit(nx_window_wayland_surface(window));
        CHECK(wl_display_roundtrip(nx_window_wayland_display(window))>=0);break;
    case 8:
        l.deadline=milliseconds();CHECK(nx_window_poll(window,20)==NX_OK);break;
    case 9:nx_context_destroy(ctx);ctx=NULL;break;
    default:break;
    }
    if(ctx)frame(ctx,window,&d,0);
    int fd=atomic_exchange(&server->stalled_fd,0);CHECK(fd>0 && !close(fd-1));
    if(ctx) {
        for(unsigned i=0;i<30 && !l.calls;++i)pump(ctx,window,&d);
        CHECK(l.calls==1 && !l.refs);
        if(mode<2) {
            const char *expected=d.multiline?"abfirst\r\nsecond\tlinec":"abfirst second\tlinec";
            CHECK(l.applied && d.applied==1 && d.validated==1 && d.revision==2 && d.length==strlen(expected) && !memcmp(d.bytes,expected,d.length));
        } else CHECK(!l.applied && !d.applied && d.length==3 && !memcmp(d.bytes,"abc",3));
        if(mode==7) {
            offer(server,window,1);pump(ctx,window,&d);CHECK(l.calls==1 && !d.applied);
        }
        nx_context_destroy(ctx);
    } else CHECK(l.calls==1 && !l.applied && !l.refs && !d.applied);
    atomic_store(&server->offer_stall,0);
    printf("Document Wayland paste mode%u passed\n",mode);
}
int main(void) {
    test_server server={0};server.clipboard_auto_focus=1;atomic_store(&server.offer_multiline,1);
    char runtime[]="/tmp/nexaui-document-clip-XXXXXX";CHECK(mkdtemp(runtime) && !setenv("XDG_RUNTIME_DIR",runtime,1));
    CHECK(!setenv("NEXAUI_PLATFORM","wayland",1));unsetenv("DISPLAY");unsetenv("WAYLAND_SOCKET");
    server.display=wl_display_create();CHECK(server.display);
    CHECK(wl_global_create(server.display,&wl_compositor_interface,1,&server,bind_compositor));
    CHECK(wl_global_create(server.display,&wl_shm_interface,1,&server,bind_shm));
    server.seat_global=wl_global_create(server.display,&wl_seat_interface,5,&server,bind_seat);CHECK(server.seat_global);
    CHECK(wl_global_create(server.display,&wl_data_device_manager_interface,3,&server,bind_data_manager));
    CHECK(wl_global_create(server.display,&xdg_wm_base_interface,1,&server,bind_wm));
    CHECK(wl_global_create(server.display,&zwp_text_input_manager_v3_interface,1,&server,bind_manager));
    const char *name=wl_display_add_socket_auto(server.display);CHECK(name);char path[256];CHECK(snprintf(path,sizeof(path),"%s/%s",runtime,name)>0);
    CHECK(!setenv("WAYLAND_DISPLAY",name,1));pthread_t thread;CHECK(!pthread_create(&thread,NULL,run_server,&server));
    window_heap heap={0};nx_window_desc desc={"document clipboard",240,120,NX_WINDOW_HIDDEN,{&heap,window_allocate,window_free}};
    nx_window *window=NULL;CHECK(nx_window_create(&desc,&window)==NX_OK);
    for(unsigned mode=0;mode<10;++mode)scenario(&server,window,mode);
    nx_window_destroy(window);CHECK(!heap.live);
    wl_display_terminate(server.display);CHECK(!pthread_join(thread,NULL));wl_display_destroy(server.display);
    CHECK(unlink(path)==0 || access(path,F_OK)!=0);CHECK(!rmdir(runtime));
    puts("Application document Wayland paste: delayed atomic application, full validation, revision/policy/omission/lease/focus/deadline/close cancellation passed");return 0;
}
