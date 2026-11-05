#include "wayland_v3_fixture.h"
#include "nexaui/software.h"
#include "nexaui/wayland_outputs.h"
#include "fractional-scale-v1-server-protocol.h"
#include "viewporter-server-protocol.h"
#include "window_internal.h"
#include "accessibility.h"
#include <math.h>

typedef struct scaling_server {
    test_server base;
    struct wl_resource *fractional;
    struct wl_resource *output;
    struct wl_global *output_global;
    uint32_t initial_scale; int initial_output_pending;
    atomic_int viewport_width,viewport_height,viewport_calls;
    atomic_int scale_request,resize_request,enter_output,output_action;
} scaling_server;

static void get_fractional(struct wl_client *client,struct wl_resource *manager,
                           uint32_t id,struct wl_resource *surface) {
    scaling_server *s=wl_resource_get_user_data(manager);CHECK(surface==s->base.surface);
    s->fractional=wl_resource_create(client,&wp_fractional_scale_v1_interface,1,id);CHECK(s->fractional);
    static const struct wp_fractional_scale_v1_interface impl={.destroy=resource_destroy};
    wl_resource_set_implementation(s->fractional,&impl,s,NULL);
    if(s->initial_scale)wp_fractional_scale_v1_send_preferred_scale(s->fractional,s->initial_scale);
}
static void bind_fractional(struct wl_client *client,void *data,uint32_t version,uint32_t id) {
    struct wl_resource *r=wl_resource_create(client,&wp_fractional_scale_manager_v1_interface,version,id);CHECK(r);
    static const struct wp_fractional_scale_manager_v1_interface impl={.destroy=resource_destroy,.get_fractional_scale=get_fractional};
    wl_resource_set_implementation(r,&impl,data,NULL);
}
static void viewport_destination(struct wl_client *client,struct wl_resource *r,int32_t width,int32_t height) {
    (void)client;scaling_server *s=wl_resource_get_user_data(r);
    atomic_store(&s->viewport_width,width);atomic_store(&s->viewport_height,height);
    atomic_fetch_add(&s->viewport_calls,1);
}
static void get_viewport(struct wl_client *client,struct wl_resource *manager,uint32_t id,struct wl_resource *surface) {
    scaling_server *s=wl_resource_get_user_data(manager);CHECK(surface==s->base.surface);
    struct wl_resource *r=wl_resource_create(client,&wp_viewport_interface,1,id);CHECK(r);
    static const struct wp_viewport_interface impl={.destroy=resource_destroy,.set_destination=viewport_destination};
    wl_resource_set_implementation(r,&impl,s,NULL);
}
static void bind_viewporter(struct wl_client *client,void *data,uint32_t version,uint32_t id) {
    struct wl_resource *r=wl_resource_create(client,&wp_viewporter_interface,version,id);CHECK(r);
    static const struct wp_viewporter_interface impl={.destroy=resource_destroy,.get_viewport=get_viewport};
    wl_resource_set_implementation(r,&impl,data,NULL);
}
static void bind_output(struct wl_client *client,void *data,uint32_t version,uint32_t id) {
    scaling_server *s=data;
    s->output=wl_resource_create(client,&wl_output_interface,version,id);CHECK(s->output);
    static const struct wl_output_interface impl={.release=resource_destroy};
    wl_resource_set_implementation(s->output,&impl,s,NULL);
    wl_output_send_geometry(s->output,0,0,500,300,WL_OUTPUT_SUBPIXEL_UNKNOWN,"NexaUI","Test",WL_OUTPUT_TRANSFORM_NORMAL);
    wl_output_send_mode(s->output,WL_OUTPUT_MODE_CURRENT|WL_OUTPUT_MODE_PREFERRED,800,600,60000);
#ifdef WL_OUTPUT_NAME_SINCE_VERSION
    if(version>=4) wl_output_send_name(s->output,"TEST-OUTPUT");
#endif
    wl_output_send_scale(s->output,2);
    if(!s->initial_output_pending) wl_output_send_done(s->output);
}
static void committed(test_server *base,struct wl_client *client) {
    (void)client;scaling_server *s=(scaling_server *)base;
    int scale=atomic_exchange(&s->scale_request,0);
    if(scale) {CHECK(s->fractional);wp_fractional_scale_v1_send_preferred_scale(s->fractional,scale<0?0:(uint32_t)scale);}
    if(atomic_exchange(&s->enter_output,0)) {CHECK(s->output);wl_surface_send_enter(base->surface,s->output);}
    int output_action=atomic_exchange(&s->output_action,0);
    if(output_action==1) wl_output_send_mode(s->output,WL_OUTPUT_MODE_CURRENT,1024,768,0);
    else if(output_action==2) wl_output_send_done(s->output);
    else if(output_action==3) {wl_global_destroy(s->output_global);s->output_global=NULL;}
    else if(output_action==4) {
        wl_output_send_mode(s->output,WL_OUTPUT_MODE_CURRENT,1100,768,60000);wl_output_send_done(s->output);
        wl_output_send_mode(s->output,WL_OUTPUT_MODE_CURRENT,1200,768,60000);wl_output_send_done(s->output);
    }
    if(atomic_exchange(&s->resize_request,0)) {
        base->width=103;base->height=53;
        struct wl_array states;wl_array_init(&states);
        xdg_toplevel_send_configure(base->toplevel,103,53,&states);wl_array_release(&states);
        xdg_surface_send_configure(base->xdg_surface,1);
    }
}
static void roundtrip(nx_window *w) {
    CHECK(wl_display_roundtrip(nx_window_wayland_display(w))>=0);
}
static void *wrong_output_thread(void *arg) {
    size_t count=1;uint64_t revision=1;
    return (void *)(uintptr_t)(nx_window_get_wayland_outputs(arg,NULL,0,&count,&revision)!=NX_ERROR_STATE || count || revision);
}
static void check_size(nx_window *w,int width,int height,int numerator,int pw,int ph) {
    nx_window_info info;nx_pixel_size pixels;
    CHECK(nx_window_get_info(w,&info)==NX_OK && info.width==width && info.height==height);
    CHECK(fabs((double)info.scale-(double)numerator/120.0)<0.000001);
    CHECK(nx_window_get_pixel_size(w,&pixels)==NX_OK && pixels.width==(uint32_t)pw && pixels.height==(uint32_t)ph);
}
static void check_resize_event(nx_window *w,int width,int height,int numerator) {
    nx_event event;int available=0,found=0;
    do {CHECK(nx_window_next_event(w,&event,&available)==NX_OK);
        if(available && event.type==NX_EVENT_RESIZE && event.width==width && event.height==height &&
           fabs((double)event.scale-(double)numerator/120.0)<0.000001)found=1;
    } while(available);
    CHECK(found);
}

int main(int argc,char **argv) {
    CHECK(argc==2);
    int initial_integer=!strcmp(argv[1],"both-integer");
    int delayed=!strcmp(argv[1],"delayed");
    int invalid=!strcmp(argv[1],"invalid");
    int pending=!strcmp(argv[1],"output-pending");
    int both=!strcmp(argv[1],"both") || initial_integer || invalid || delayed,fractional_only=!strcmp(argv[1],"fractional-only");
    int viewport_only=!strcmp(argv[1],"viewport-only"),neither=!strcmp(argv[1],"neither") || pending;
    CHECK(both || fractional_only || viewport_only || neither);
    char runtime[]="/tmp/nexaui-fractional-XXXXXX";
    CHECK(mkdtemp(runtime) && setenv("XDG_RUNTIME_DIR",runtime,1)==0 && setenv("NEXAUI_PLATFORM","wayland",1)==0);
    unsetenv("WAYLAND_SOCKET");
    scaling_server s={0};s.initial_scale=delayed?0:initial_integer?120:150;s.initial_output_pending=pending;s.base.width=101;s.base.height=51;s.base.pointer_enabled=1;s.base.surface_committed=committed;
    s.base.display=wl_display_create();CHECK(s.base.display);
    CHECK(wl_global_create(s.base.display,&wl_compositor_interface,4,&s.base,bind_compositor));
    CHECK(wl_display_init_shm(s.base.display)==0);
    CHECK(wl_global_create(s.base.display,&wl_seat_interface,5,&s.base,bind_seat));
    CHECK(wl_global_create(s.base.display,&wl_data_device_manager_interface,3,&s.base,bind_data_manager));
    CHECK(wl_global_create(s.base.display,&xdg_wm_base_interface,1,&s.base,bind_wm));
    CHECK(wl_global_create(s.base.display,&zwp_text_input_manager_v3_interface,1,&s.base,bind_manager));
#ifdef WL_OUTPUT_NAME_SINCE_VERSION
    s.output_global=wl_global_create(s.base.display,&wl_output_interface,viewport_only?3:4,&s,bind_output);
#else
    s.output_global=wl_global_create(s.base.display,&wl_output_interface,3,&s,bind_output);
#endif
    CHECK(s.output_global);
    if(both || fractional_only)CHECK(wl_global_create(s.base.display,&wp_fractional_scale_manager_v1_interface,1,&s,bind_fractional));
    if(both || viewport_only)CHECK(wl_global_create(s.base.display,&wp_viewporter_interface,1,&s,bind_viewporter));
    const char *socket=wl_display_add_socket_auto(s.base.display);CHECK(socket);
    char path[256];CHECK(snprintf(path,sizeof(path),"%s/%s",runtime,socket)>0 && setenv("WAYLAND_DISPLAY",socket,1)==0);
    pthread_t thread;CHECK(pthread_create(&thread,NULL,run_server,&s.base)==0);
    window_heap heap={0};nx_window_desc desc={"fractional",101,51,0,{&heap,window_allocate,window_free}};
    nx_window *w=NULL;CHECK(nx_window_create(&desc,&w)==NX_OK);roundtrip(w);
    if(pending) {
        size_t count=123; uint64_t revision=123;
        CHECK(nx_window_get_wayland_outputs(w,NULL,0,&count,&revision)==NX_ERROR_STATE && !count && !revision);
        nx_event event; int available=0,wakes=0;
        do {CHECK(nx_window_next_event(w,&event,&available)==NX_OK);} while(available);
        atomic_store(&s.output_action,2);wl_surface_commit(nx_window_wayland_surface(w));roundtrip(w);
        do {CHECK(nx_window_next_event(w,&event,&available)==NX_OK);if(available && event.type==NX_EVENT_WAKE)++wakes;} while(available);
        CHECK(wakes==1 && nx_window_get_wayland_outputs(w,NULL,0,&count,&revision)==NX_OK && count==1);
    }
    CHECK(atomic_load(&s.base.geometry_before_configure)==0);
    CHECK(atomic_load(&s.base.geometry_after_configure)>0);
    check_size(w,101,51,both && !delayed?(int)s.initial_scale:120,both && !initial_integer && !delayed?126:101,both && !initial_integer && !delayed?64:51);
    CHECK(atomic_load(&s.base.attached_width)==(both && !initial_integer && !delayed?126:101));
    CHECK(atomic_load(&s.base.attached_height)==(both && !initial_integer && !delayed?64:51));
    CHECK(atomic_load(&s.base.buffer_scale)==1);
    CHECK((atomic_load(&s.viewport_calls)>0)==(both && !delayed));
    if(both && !delayed)check_resize_event(w,101,51,(int)s.initial_scale);
    atomic_store(&s.enter_output,1);wl_surface_commit(nx_window_wayland_surface(w));roundtrip(w);roundtrip(w);
    if(delayed) {
        check_size(w,101,51,240,202,102);check_resize_event(w,101,51,240);
        CHECK(atomic_load(&s.base.buffer_scale)==2);
        atomic_store(&s.scale_request,150);wl_surface_commit(nx_window_wayland_surface(w));roundtrip(w);roundtrip(w);
        check_size(w,101,51,150,126,64);check_resize_event(w,101,51,150);
        CHECK(atomic_load(&s.base.buffer_scale)==1 && atomic_load(&s.viewport_calls)>0);
    } else if(both)check_size(w,101,51,(int)s.initial_scale,initial_integer?101:126,initial_integer?51:64);
    else {
        check_size(w,101,51,240,202,102);check_resize_event(w,101,51,240);
        CHECK(atomic_load(&s.base.attached_width)==202 && atomic_load(&s.base.attached_height)==102);
        CHECK(atomic_load(&s.base.buffer_scale)==2);
    }
    if(both) {
        CHECK(atomic_load(&s.viewport_width)==101 && atomic_load(&s.viewport_height)==51);
        if(invalid) {
            atomic_store(&s.scale_request,-1);wl_surface_commit(nx_window_wayland_surface(w));roundtrip(w);
            CHECK(nx_window_poll(w,0)==NX_ERROR_PLATFORM);
            nx_window_destroy(w);CHECK(heap.live==0);
            wl_display_terminate(s.base.display);CHECK(pthread_join(thread,NULL)==0);wl_display_destroy(s.base.display);
            CHECK(unlink(path)==0 || access(path,F_OK)!=0);CHECK(rmdir(runtime)==0);
            puts("Invalid preferred scale is rejected");return 0;
        }
        atomic_store(&s.scale_request,180);wl_surface_commit(nx_window_wayland_surface(w));roundtrip(w);roundtrip(w);
        check_size(w,101,51,180,152,77);
        check_resize_event(w,101,51,180);
        CHECK(atomic_load(&s.base.attached_width)==152 && atomic_load(&s.base.attached_height)==77);
        atomic_store(&s.scale_request,240);wl_surface_commit(nx_window_wayland_surface(w));roundtrip(w);roundtrip(w);
        check_size(w,101,51,240,202,102);check_resize_event(w,101,51,240);
        CHECK(atomic_load(&s.base.attached_width)==202 && atomic_load(&s.base.attached_height)==102 && atomic_load(&s.base.buffer_scale)==1);
        atomic_store(&s.scale_request,123);wl_surface_commit(nx_window_wayland_surface(w));roundtrip(w);roundtrip(w);
        check_size(w,101,51,123,104,52);
        check_resize_event(w,101,51,123);
        atomic_store(&s.resize_request,1);wl_surface_commit(nx_window_wayland_surface(w));roundtrip(w);roundtrip(w);
        check_size(w,103,53,123,106,54);
        check_resize_event(w,103,53,123);
        CHECK(atomic_load(&s.viewport_width)==103 && atomic_load(&s.viewport_height)==53);
        CHECK(atomic_load(&s.base.attached_width)==106 && atomic_load(&s.base.attached_height)==54);
        CHECK(atomic_load(&s.base.buffer_scale)==1);
        nx_linux_ax_geometry geometry;nx_linux_window_ax_geometry(w,&geometry);
        CHECK(geometry.visible && geometry.width==103 && geometry.height==53);
        CHECK(nx_window_set_ime_rect(w,(nx_rect){12,34,2,18})==NX_OK);
        CHECK(nx_window_set_ime_enabled(w,1)==NX_OK);roundtrip(w);
        CHECK(atomic_load(&s.base.caret_ok)>0 && atomic_load(&s.base.enable_commits)>0);
        nx_event event;int available=0;
        do {CHECK(nx_window_next_event(w,&event,&available)==NX_OK);} while(available);
        atomic_store(&s.base.pointer_press_requested,1);wl_surface_commit(nx_window_wayland_surface(w));roundtrip(w);
        int saw=0;
        do {CHECK(nx_window_next_event(w,&event,&available)==NX_OK);
            if(available && event.type==NX_EVENT_POINTER_MOVE && event.x==12 && event.y==15)saw=1;
        } while(available);
        CHECK(saw);roundtrip(w);
        CHECK(nx_window_renderer_acquire(w,&s)==NX_OK);
        int old_width=atomic_load(&s.base.attached_width),old_height=atomic_load(&s.base.attached_height);
        atomic_store(&s.scale_request,150);wl_surface_commit(nx_window_wayland_surface(w));roundtrip(w);roundtrip(w);
        check_size(w,103,53,150,129,66);check_resize_event(w,103,53,150);
        CHECK(atomic_load(&s.base.attached_width)==old_width && atomic_load(&s.base.attached_height)==old_height);
        CHECK(atomic_load(&s.viewport_width)==103 && atomic_load(&s.viewport_height)==53 && atomic_load(&s.base.buffer_scale)==1);
        CHECK(nx_window_renderer_release(w,&s)==NX_OK);roundtrip(w);
        CHECK(atomic_load(&s.base.attached_width)==129 && atomic_load(&s.base.attached_height)==66);
        nx_software_view *software=NULL;
        nx_software_view_outcome outcome;
        CHECK(nx_software_view_open(w,&software)==NX_OK);
        CHECK(nx_software_view_render(software,NULL,0,(nx_color){1,0,0,1},&outcome)==NX_OK &&
              outcome==NX_SOFTWARE_VIEW_READY);
        roundtrip(w);
        CHECK(atomic_load(&s.base.attached_width)==129 && atomic_load(&s.base.attached_height)==66 &&
              atomic_load(&s.base.buffer_scale)==1);
        atomic_store(&s.scale_request,180);wl_surface_commit(nx_window_wayland_surface(w));roundtrip(w);roundtrip(w);
        check_size(w,103,53,180,155,80);
        CHECK(nx_software_view_render(software,NULL,0,(nx_color){0,1,0,1},&outcome)==NX_OK &&
              outcome==NX_SOFTWARE_VIEW_READY);
        roundtrip(w);
        CHECK(atomic_load(&s.base.attached_width)==155 && atomic_load(&s.base.attached_height)==80 &&
              atomic_load(&s.viewport_width)==103 && atomic_load(&s.viewport_height)==53);
        CHECK(nx_software_view_close(&software)==NX_OK && !software);
    }
    nx_wayland_output_info output[1], sentinel; memset(&sentinel,0x5a,sizeof(sentinel)); output[0]=sentinel;
    size_t count=99; uint64_t revision=99, next=0;
    CHECK(nx_window_get_wayland_outputs(NULL,output,1,&count,&revision)==NX_ERROR_ARGUMENT && !count && !revision);
    CHECK(nx_window_get_wayland_outputs(w,NULL,1,&count,&revision)==NX_ERROR_ARGUMENT && !count && !revision);
    pthread_t wrong;void *wrong_result;
    CHECK(!pthread_create(&wrong,NULL,wrong_output_thread,w) && !pthread_join(wrong,&wrong_result) && !wrong_result);
    CHECK(nx_window_get_wayland_outputs(w,NULL,0,&count,&revision)==NX_OK && count==1 && revision>0);
    CHECK(nx_window_get_wayland_outputs(w,output,0,&count,&next)==NX_ERROR_LIMIT && count==1 && next==revision && !memcmp(output,&sentinel,sizeof(sentinel)));
    CHECK(nx_window_get_wayland_outputs(w,output,1,&count,&next)==NX_OK && count==1 && next==revision);
    CHECK(output[0].id && output[0].mode_known && output[0].width==800 && output[0].height==600 &&
          output[0].refresh_millihz==60000 && output[0].scale==2);
#ifdef WL_OUTPUT_NAME_SINCE_VERSION
    CHECK(viewport_only ? !output[0].name[0] : !strcmp(output[0].name,"TEST-OUTPUT"));
#else
    CHECK(!output[0].name[0]);
#endif
    nx_event out_event; int available=0, wakes=0;
    do {CHECK(nx_window_next_event(w,&out_event,&available)==NX_OK);} while(available);
    atomic_store(&s.output_action,1);wl_surface_commit(nx_window_wayland_surface(w));roundtrip(w);
    CHECK(nx_window_get_wayland_outputs(w,output,1,&count,&next)==NX_OK && next==revision && output[0].width==800);
    do {CHECK(nx_window_next_event(w,&out_event,&available)==NX_OK);if(available && out_event.type==NX_EVENT_WAKE)++wakes;} while(available);
    CHECK(!wakes);
    atomic_store(&s.output_action,2);wl_surface_commit(nx_window_wayland_surface(w));roundtrip(w);
    do {CHECK(nx_window_next_event(w,&out_event,&available)==NX_OK);if(available && out_event.type==NX_EVENT_WAKE)++wakes;} while(available);
    CHECK(wakes==1 && nx_window_get_wayland_outputs(w,output,1,&count,&next)==NX_OK && next>revision &&
          output[0].width==1024 && output[0].height==768 && output[0].refresh_millihz==0);
    revision=next;wakes=0;atomic_store(&s.output_action,4);wl_surface_commit(nx_window_wayland_surface(w));roundtrip(w);
    do {CHECK(nx_window_next_event(w,&out_event,&available)==NX_OK);if(available && out_event.type==NX_EVENT_WAKE)++wakes;} while(available);
    CHECK(wakes==1 && nx_window_get_wayland_outputs(w,output,1,&count,&next)==NX_OK && next==revision+2 && output[0].width==1200);
    revision=next; atomic_store(&s.output_action,3);wl_surface_commit(nx_window_wayland_surface(w));roundtrip(w);
    CHECK(nx_window_get_wayland_outputs(w,output,1,&count,&next)==NX_OK && !count && next>revision);
    nx_window_destroy(w);CHECK(heap.live==0);
    wl_display_terminate(s.base.display);CHECK(pthread_join(thread,NULL)==0);wl_display_destroy(s.base.display);
    CHECK(unlink(path)==0 || access(path,F_OK)!=0);CHECK(rmdir(runtime)==0);
    puts("Wayland fractional scale negotiation and integer fallback checked");return 0;
}
