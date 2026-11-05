/* Controlled Wayland wire test of the private host adapter. */
#define main aetheri_desktop_layer_program_main
#include "main_layer_test.c"
#undef main
#include "text-input-v3-server.h"
#include <pthread.h>
#include <stdatomic.h>
#include <wayland-server.h>

struct wire_server {struct wl_display *display;struct wl_resource *surface;uint32_t serial;int enabled;atomic_int caret,surrounding;};
static void wire_destroy(struct wl_client *client,struct wl_resource *resource){(void)client;wl_resource_destroy(resource);}
static void wire_create_surface(struct wl_client *client,struct wl_resource *resource,uint32_t id) {
    struct wire_server *s=wl_resource_get_user_data(resource);
    s->surface=wl_resource_create(client,&wl_surface_interface,1,id);CHECK(s->surface);
    static const struct wl_surface_interface impl={.destroy=wire_destroy};
    wl_resource_set_implementation(s->surface,&impl,s,NULL);
}
static void wire_bind_compositor(struct wl_client *client,void *data,uint32_t version,uint32_t id) {
    (void)version;struct wl_resource *r=wl_resource_create(client,&wl_compositor_interface,1,id);CHECK(r);
    static const struct wl_compositor_interface impl={.create_surface=wire_create_surface};
    wl_resource_set_implementation(r,&impl,data,NULL);
}
static void wire_bind_extra_output(struct wl_client *client,void *data,uint32_t version,uint32_t id) {
    (void)client;(void)data;(void)version;(void)id;CHECK(false); /* never bind an unmapped output */
}
static void wire_get_pointer(struct wl_client *c,struct wl_resource *seat,uint32_t id) {
    struct wl_resource *r=wl_resource_create(c,&wl_pointer_interface,5,id);CHECK(r);
    static const struct wl_pointer_interface impl={.release=wire_destroy};
    wl_resource_set_implementation(r,&impl,wl_resource_get_user_data(seat),NULL);
}
static void wire_bind_seat(struct wl_client *client,void *data,uint32_t version,uint32_t id) {
    (void)version;struct wl_resource *r=wl_resource_create(client,&wl_seat_interface,5,id);CHECK(r);
    static const struct wl_seat_interface impl={.get_pointer=wire_get_pointer,.release=wire_destroy};
    wl_resource_set_implementation(r,&impl,data,NULL);
}
static void wire_enable(struct wl_client *c,struct wl_resource *r){(void)c;((struct wire_server *)wl_resource_get_user_data(r))->enabled=1;}
static void wire_disable(struct wl_client *c,struct wl_resource *r){(void)c;((struct wire_server *)wl_resource_get_user_data(r))->enabled=0;}
static void wire_content(struct wl_client *c,struct wl_resource *r,uint32_t hint,uint32_t purpose) {
    (void)c;(void)r;CHECK(hint==0 && purpose==0);
}
static void wire_caret(struct wl_client *c,struct wl_resource *r,int32_t x,int32_t y,int32_t w,int32_t h) {
    (void)c;(void)x;(void)y;struct wire_server *s=wl_resource_get_user_data(r);CHECK(w>0 && h>0);atomic_fetch_add(&s->caret,1);
}
static void wire_surrounding(struct wl_client *c,struct wl_resource *r,const char *text,int32_t cursor,int32_t anchor) {
    (void)c;struct wire_server *s=wl_resource_get_user_data(r);
    CHECK(cursor>=0 && anchor>=0 && (size_t)cursor<=strlen(text) && (size_t)anchor<=strlen(text));
    if(s->serial==0)CHECK(!strcmp(text,"A") && cursor==1 && anchor==1);
    if(s->serial==2)CHECK(!strcmp(text,"A\xc3\xa9") && cursor==3 && anchor==3);
    if(s->serial==3)CHECK(!strcmp(text,"AB") && cursor==2 && anchor==2);
    if(s->serial==4)CHECK(!strcmp(text,"ABD") && cursor==3 && anchor==3);
    atomic_fetch_add(&s->surrounding,1);
}
static void wire_commit(struct wl_client *c,struct wl_resource *r) {
    (void)c;struct wire_server *s=wl_resource_get_user_data(r);++s->serial;
    if(!s->enabled)return;
    if(s->serial==1)zwp_text_input_v3_send_preedit_string(r,"x",1,1);
    else if(s->serial==2) {zwp_text_input_v3_send_commit_string(r,"\xc3\xa9");zwp_text_input_v3_send_preedit_string(r,"",0,0);}
    else if(s->serial==3) {zwp_text_input_v3_send_delete_surrounding_text(r,2,0);zwp_text_input_v3_send_commit_string(r,"B");}
    else if(s->serial==4) {zwp_text_input_v3_send_commit_string(r,"X");return;}
    else if(s->serial==5) {
        zwp_text_input_v3_send_done(r,4);
        zwp_text_input_v3_send_commit_string(r,"C");
    } else if(s->serial==6)zwp_text_input_v3_send_preedit_string(r,"x",1,1);
    else if(s->serial==7) {
        zwp_text_input_v3_send_commit_string(r,"X");
        zwp_text_input_v3_send_leave(r,s->surface);
    }
    zwp_text_input_v3_send_done(r,s->serial);
}
static void wire_get_input(struct wl_client *c,struct wl_resource *manager,uint32_t id,struct wl_resource *seat) {
    (void)seat;struct wire_server *s=wl_resource_get_user_data(manager);CHECK(s->surface);
    struct wl_resource *r=wl_resource_create(c,&zwp_text_input_v3_interface,1,id);CHECK(r);
    static const struct zwp_text_input_v3_interface impl={.destroy=wire_destroy,.enable=wire_enable,.disable=wire_disable,
        .set_surrounding_text=wire_surrounding,.set_content_type=wire_content,.set_cursor_rectangle=wire_caret,.commit=wire_commit};
    wl_resource_set_implementation(r,&impl,s,NULL);zwp_text_input_v3_send_enter(r,s->surface);
}
static void wire_bind_manager(struct wl_client *c,void *data,uint32_t version,uint32_t id) {
    (void)version;struct wl_resource *r=wl_resource_create(c,&zwp_text_input_manager_v3_interface,1,id);CHECK(r);
    static const struct zwp_text_input_manager_v3_interface impl={.destroy=wire_destroy,.get_text_input=wire_get_input};
    wl_resource_set_implementation(r,&impl,data,NULL);
}
static void wire_global(void *data,struct wl_registry *registry,uint32_t name,const char *interface,uint32_t version) {
    (void)version;struct app *a=data;
    if(!strcmp(interface,"wl_compositor"))a->compositor=wl_registry_bind(registry,name,&wl_compositor_interface,1);
    else if(!strcmp(interface,"wl_seat"))a->seat=wl_registry_bind(registry,name,&wl_seat_interface,5);
    else if(!strcmp(interface,"zwp_text_input_manager_v3"))a->ime_manager=wl_registry_bind(registry,name,&zwp_text_input_manager_v3_interface,1);
    else if(!strcmp(interface,"wl_output"))global(a,registry,name,interface,version);
}
static const struct wl_registry_listener wire_registry_listener={.global=wire_global};
static void *wire_run(void *data){wl_display_run(((struct wire_server *)data)->display);return NULL;}
int main(int argc,char **argv) {
    CHECK(argc==2);struct wire_server server={0};server.display=wl_display_create();CHECK(server.display);
    CHECK(wl_global_create(server.display,&wl_compositor_interface,1,&server,wire_bind_compositor));
    CHECK(wl_global_create(server.display,&wl_seat_interface,5,&server,wire_bind_seat));
    CHECK(wl_global_create(server.display,&zwp_text_input_manager_v3_interface,1,&server,wire_bind_manager));
    CHECK(wl_global_create(server.display,&wl_output_interface,1,&server,wire_bind_extra_output));
    int sockets[2];CHECK(socketpair(AF_UNIX,SOCK_STREAM,0,sockets)==0);CHECK(wl_client_create(server.display,sockets[0]));
    pthread_t thread;CHECK(!pthread_create(&thread,NULL,wire_run,&server));
    struct app a={.launcher=-1,.toggle_output=-1,.ime_requested=true,.output_count=3};
    a.display=wl_display_connect_to_fd(sockets[1]);CHECK(a.display);a.registry=wl_display_get_registry(a.display);CHECK(a.registry);
    CHECK(!wl_registry_add_listener(a.registry,&wire_registry_listener,&a));CHECK(wl_display_roundtrip(a.display)>=0);
    CHECK(a.compositor && a.seat && a.ime_manager && a.unmapped_outputs==1);
    a.font=load_font(argv[1],&a.font_size);CHECK(a.font);
    struct view *v=&a.views[0];v->app=&a;v->width=640;v->height=480;v->expanded=v->configured=true;
    v->surface=wl_compositor_create_surface(a.compositor);CHECK(v->surface);CHECK(wl_display_roundtrip(a.display)>=0);
    ae_desktop_model_init(&v->model);ae_desktop_model_search(&v->model,0,true);
    CHECK(ae_desktop_model_text(&v->model,"A"));v->revision=1;
    CHECK(aetheri_desktop_view_create(&v->model,a.font,a.font_size,&v->view));
    CHECK(aetheri_desktop_view_set_host_ime(v->view,1)==NX_OK);advance(v);
    struct wl_array keys={0};enter(&a,NULL,0,v->surface,&keys);
    a.ime_input=zwp_text_input_manager_v3_get_text_input(a.ime_manager,a.seat);CHECK(a.ime_input);
    CHECK(!zwp_text_input_v3_add_listener(a.ime_input,&ime_listener,&a));CHECK(wl_display_roundtrip(a.display)>=0);
    CHECK(a.ime_surface==v->surface);ime_sync(&a);CHECK(wl_display_roundtrip(a.display)>=0);
    CHECK(!strcmp(v->model.query,"A"));ime_sync(&a);CHECK(wl_display_roundtrip(a.display)>=0);
    CHECK(!strcmp(v->model.query,"A\xc3\xa9"));ime_sync(&a);CHECK(wl_display_roundtrip(a.display)>=0);
    CHECK(!strcmp(v->model.query,"AB") && atomic_load(&server.caret)>=3 && atomic_load(&server.surrounding)>=3);
    ime_sync(&a);CHECK(wl_display_roundtrip(a.display)>=0);CHECK(!strcmp(v->model.query,"AB"));
    nx_event typed={0};typed.type=NX_EVENT_TEXT;typed.scalar='D';
    CHECK(aetheri_desktop_view_queue_event(v->view,&typed)==NX_OK);advance(v);advance(v);
    CHECK(!strcmp(v->model.query,"ABD"));ime_sync(&a);CHECK(wl_display_roundtrip(a.display)>=0);
    CHECK(!strcmp(v->model.query,"ABDC"));
    uint64_t revision=0;nx_rect plain={0},composing={0};
    CHECK(aetheri_desktop_view_host_ime_revision(v->view,&revision)==NX_OK);
    CHECK(aetheri_desktop_view_host_ime_caret(v->view,&plain)==NX_OK);
    ime_sync(&a);CHECK(wl_display_roundtrip(a.display)>=0);
    CHECK(aetheri_desktop_view_host_ime_caret(v->view,&composing)==NX_OK && composing.x>plain.x);
    ime_sync(&a);CHECK(wl_display_roundtrip(a.display)>=0);
    CHECK(!a.ime_enabled && !a.ime_surface && !strcmp(v->model.query,"ABDC"));
    advance(v);CHECK(aetheri_desktop_view_host_ime_caret(v->view,&composing)==NX_OK && composing.x==plain.x);
    ae_desktop_model_search(&v->model,0,true);
    CHECK(ae_desktop_model_window(&v->model,42,"Editor","org.example.Editor",false));++v->revision;
    advance(v);advance(v);
    uint32_t row_index=0;nx_semantic_view row;
    CHECK(nx_semantics_find(aetheri_desktop_view_context(v->view),UINT64_C(0x100000000)+42,&row_index)==NX_OK);
    CHECK(nx_semantics_get(aetheri_desktop_view_context(v->view),row_index,&row)==NX_OK);
    a.pointer=wl_seat_get_pointer(a.seat);CHECK(a.pointer);
    CHECK(!wl_pointer_add_listener(a.pointer,&pointer_listener,&a));
    pointer_enter(&a,a.pointer,0,v->surface,wl_fixed_from_double(row.bounds.x+8),wl_fixed_from_double(row.bounds.y+8));
    a.activation_enabled=true;
    if(!getenv("NEXAUI_TEST_POINTER_REMOVAL_ONLY")) {
        pointer_button(&a,a.pointer,0,0,BTN_LEFT,WL_POINTER_BUTTON_STATE_PRESSED);
        pointer_button(&a,a.pointer,0,0,BTN_LEFT,WL_POINTER_BUTTON_STATE_RELEASED);
        CHECK(!a.activation_phase && a.pointer_release_barrier);
        CHECK(wl_display_roundtrip(a.display)>=0);CHECK(a.activation_phase==1 && a.activation_id==42);
        a.activation_phase=0;a.activation_enabled=true;
        pointer_button(&a,a.pointer,0,0,BTN_LEFT,WL_POINTER_BUTTON_STATE_PRESSED);
        pointer_button(&a,a.pointer,0,0,BTN_LEFT,WL_POINTER_BUTTON_STATE_RELEASED);
        pointer_motion(&a,a.pointer,0,wl_fixed_from_double(row.bounds.x+9),wl_fixed_from_double(row.bounds.y+8));
        CHECK(!a.pointer_release_barrier && !a.activation_phase);
        CHECK(wl_display_roundtrip(a.display)>=0);CHECK(!a.activation_phase);
        pointer_button(&a,a.pointer,0,0,BTN_LEFT,WL_POINTER_BUTTON_STATE_PRESSED);
        pointer_button(&a,a.pointer,0,0,BTN_LEFT,WL_POINTER_BUTTON_STATE_RELEASED);
        pointer_axis(&a,a.pointer,0,WL_POINTER_AXIS_VERTICAL_SCROLL,wl_fixed_from_double(10));
        pointer_frame(&a,a.pointer);CHECK(!a.pointer_release_barrier && !a.activation_phase);
        CHECK(wl_display_roundtrip(a.display)>=0);CHECK(!a.activation_phase);
        pointer_button(&a,a.pointer,0,0,BTN_LEFT,WL_POINTER_BUTTON_STATE_PRESSED);
        pointer_button(&a,a.pointer,0,0,BTN_LEFT,WL_POINTER_BUTTON_STATE_RELEASED);
        CHECK(ae_desktop_model_window(&v->model,42,"Changed","org.example.Editor",false));++v->revision;
        CHECK(wl_display_roundtrip(a.display)>=0);CHECK(!a.activation_phase && !a.pointer_release_barrier);
        advance(v);
    }
    pointer_button(&a,a.pointer,0,0,BTN_LEFT,WL_POINTER_BUTTON_STATE_PRESSED);
    pointer_button(&a,a.pointer,0,0,BTN_LEFT,WL_POINTER_BUTTON_STATE_RELEASED);
    CHECK(a.pointer_release_barrier && !a.activation_phase);
    capabilities(&a,a.seat,0);
    CHECK(!a.pointer && !a.pointer_release_barrier && !a.activation_phase);
    CHECK(wl_display_roundtrip(a.display)>=0);CHECK(!a.activation_phase);
    /* An output removal observed before the sync reply retires a real surface
       and cancels its outstanding captured release before any activation. */
    a.pointer=wl_seat_get_pointer(a.seat);CHECK(a.pointer);
    CHECK(!wl_pointer_add_listener(a.pointer,&pointer_listener,&a));
    pointer_enter(&a,a.pointer,0,v->surface,wl_fixed_from_double(row.bounds.x+8),wl_fixed_from_double(row.bounds.y+8));
    pointer_button(&a,a.pointer,0,0,BTN_LEFT,WL_POINTER_BUTTON_STATE_PRESSED);
    pointer_button(&a,a.pointer,0,0,BTN_LEFT,WL_POINTER_BUTTON_STATE_RELEASED);
    CHECK(a.pointer_release_barrier && !a.activation_phase);
    a.output_names[0]=77;global_remove(&a,NULL,77);
    CHECK(v->remove_pending && v->surface && v->view);
    retire_output(&a,0);
    CHECK(!v->surface && !v->view && !a.pointer_release_barrier && !a.pointer_view && !a.focused);
    CHECK(wl_display_roundtrip(a.display)>=0 && !a.activation_phase);
    /* A new output after the initial three cannot be assigned an Aetheri
       desktop index from registry order; it must not recreate the old view. */
    a.output_count=3;global(&a,NULL,88,"wl_output",4);
    CHECK(a.unmapped_outputs==2 && !a.outputs[0] && !a.output_names[0] && !v->surface && !v->view && !a.activation_phase);
    capabilities(&a,a.seat,0);
    ime_stop(&a);zwp_text_input_v3_destroy(a.ime_input);zwp_text_input_manager_v3_destroy(a.ime_manager);
    wl_seat_destroy(a.seat);
    wl_compositor_destroy(a.compositor);wl_registry_destroy(a.registry);free(a.font);
    wl_display_terminate(server.display);wl_display_disconnect(a.display);CHECK(!pthread_join(thread,NULL));
    wl_display_destroy_clients(server.display);wl_display_destroy(server.display);return 0;
}
