#include "wayland_v3_fixture.h"
#include "nexaui/wayland_host.h"
#include <wayland-client.h>
#include <sys/socket.h>

/* A private test role: it is created and acknowledged by the host, not NexaUI. */
static const struct wl_interface test_role_interface;
static const struct wl_interface *get_role_types[]={&test_role_interface,&wl_surface_interface};
static const struct wl_message manager_requests[]={{"get_role","no",get_role_types}};
static const struct wl_interface test_manager_interface={"nexaui_test_role_manager",1,1,manager_requests,0,NULL};
static const struct wl_message role_requests[]={{"destroy","",NULL},{"ack","u",NULL},{"release","",NULL}};
static const struct wl_message role_events[]={{"configure","u",NULL}};
static const struct wl_interface test_role_interface={"nexaui_test_role",1,3,role_requests,1,role_events};

typedef struct host_server {
    test_server base;
    atomic_int acknowledged, captures, role_creations, capture_enabled, disconnect;
    atomic_uint pixels[3];
    atomic_uint outside[3];
    struct wl_resource *held[3];
} host_server;
static void committed(test_server *base,struct wl_client *client) {
    host_server *server=(host_server *)base;
    if (atomic_exchange(&server->disconnect,0))
        CHECK(shutdown(wl_client_get_fd(client),SHUT_RDWR)==0);
}
static void role_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client; wl_resource_destroy(resource);
}
static void role_ack(struct wl_client *client, struct wl_resource *resource, uint32_t serial) {
    (void)client; host_server *server=wl_resource_get_user_data(resource);
    CHECK(serial==42); atomic_store(&server->acknowledged,1);
}
static void role_release(struct wl_client *client, struct wl_resource *resource) {
    (void)client; host_server *server=wl_resource_get_user_data(resource);
    for (int i=0;i<3;++i) if (server->held[i]) {
        wl_buffer_send_release(server->held[i]); server->held[i]=NULL;
    }
}
static const struct { void (*destroy)(struct wl_client *,struct wl_resource *);
    void (*ack)(struct wl_client *,struct wl_resource *,uint32_t);
    void (*release)(struct wl_client *,struct wl_resource *); } role_impl={role_destroy,role_ack,role_release};
static void manager_get_role(struct wl_client *client, struct wl_resource *manager,
                             uint32_t id, struct wl_resource *surface) {
    host_server *server=wl_resource_get_user_data(manager);
    CHECK(surface==server->base.surface && atomic_fetch_add(&server->role_creations,1)==0);
    struct wl_resource *role=wl_resource_create(client,&test_role_interface,1,id); CHECK(role);
    wl_resource_set_implementation(role,&role_impl,server,NULL);
    wl_resource_post_event(role,0,42u);
}
static const struct { void (*get_role)(struct wl_client *,struct wl_resource *,uint32_t,struct wl_resource *); }
    manager_impl={manager_get_role};
static void bind_test_manager(struct wl_client *client,void *data,uint32_t version,uint32_t id) {
    struct wl_resource *resource=wl_resource_create(client,&test_manager_interface,version,id); CHECK(resource);
    wl_resource_set_implementation(resource,&manager_impl,data,NULL);
}
static void capture(test_server *base,struct wl_resource *buffer) {
    host_server *server=(host_server *)base;
    if (!atomic_load(&server->capture_enabled)) return;
    int index=atomic_load(&server->captures);
    CHECK(atomic_load(&server->acknowledged) && index<3);
    struct wl_shm_buffer *shm=wl_shm_buffer_get(buffer); CHECK(shm);
    CHECK(wl_shm_buffer_get_format(shm)==WL_SHM_FORMAT_ARGB8888);
    CHECK(wl_shm_buffer_get_width(shm)==40 && wl_shm_buffer_get_height(shm)==20);
    wl_shm_buffer_begin_access(shm);
    uint32_t pixel; memcpy(&pixel,wl_shm_buffer_get_data(shm),sizeof(pixel));
    uint32_t outside; memcpy(&outside,(const char *)wl_shm_buffer_get_data(shm)+5*4,sizeof(outside));
    wl_shm_buffer_end_access(shm);
    atomic_store(&server->pixels[index],pixel);
    atomic_store(&server->outside[index],outside);
    server->held[index]=buffer; atomic_store(&server->captures,index+1);
}
typedef struct client_globals {
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_proxy *manager;
} client_globals;
static void global(void *data,struct wl_registry *registry,uint32_t name,const char *interface,uint32_t version) {
    client_globals *g=data; (void)version;
    if (!strcmp(interface,"wl_compositor")) g->compositor=wl_registry_bind(registry,name,&wl_compositor_interface,4);
    else if (!strcmp(interface,"wl_shm")) g->shm=wl_registry_bind(registry,name,&wl_shm_interface,1);
    else if (!strcmp(interface,test_manager_interface.name)) g->manager=wl_registry_bind(registry,name,&test_manager_interface,1);
}
static void global_remove(void *data,struct wl_registry *registry,uint32_t name) {(void)data;(void)registry;(void)name;}
static const struct wl_registry_listener registry_listener={global,global_remove};
static void configured(void *data,struct wl_proxy *role,uint32_t serial) {(void)role; *(uint32_t *)data=serial;}
static void (*role_listener[])(void)={(void (*)(void))configured};
static void *wrong_thread(void *data) {
    nx_wayland_shm_host *host=data,*copy=host; nx_wayland_shm_outcome outcome;
    CHECK(nx_wayland_shm_host_present(host,(nx_pixel_size){1,1},1,NULL,0,(nx_color){0},&outcome)==NX_ERROR_STATE);
    CHECK(nx_wayland_shm_host_destroy(&copy)==NX_ERROR_STATE && copy==host);
    return NULL;
}
int main(void) {
    char runtime[]="/tmp/nexaui-host-test-XXXXXX"; CHECK(mkdtemp(runtime));
    CHECK(setenv("XDG_RUNTIME_DIR",runtime,1)==0); unsetenv("WAYLAND_SOCKET");
    host_server server={0}; server.base.buffer_attached=capture; server.base.surface_committed=committed;
    server.base.display=wl_display_create(); CHECK(server.base.display);
    CHECK(wl_global_create(server.base.display,&wl_compositor_interface,4,&server.base,bind_compositor));
    CHECK(wl_display_init_shm(server.base.display)==0);
    CHECK(wl_global_create(server.base.display,&test_manager_interface,1,&server,bind_test_manager));
    const char *socket=wl_display_add_socket_auto(server.base.display); CHECK(socket);
    char path[256]; CHECK(snprintf(path,sizeof(path),"%s/%s",runtime,socket)>0);
    CHECK(setenv("WAYLAND_DISPLAY",socket,1)==0);
    pthread_t server_thread, thread; CHECK(pthread_create(&server_thread,NULL,run_server,&server.base)==0);
    struct wl_display *display=wl_display_connect(NULL); CHECK(display);
    struct wl_event_queue *queue=wl_display_create_queue(display); CHECK(queue);
    struct wl_registry *registry=wl_display_get_registry(display); CHECK(registry);
    wl_proxy_set_queue((struct wl_proxy *)registry,queue);
    client_globals g={0}; CHECK(wl_registry_add_listener(registry,&registry_listener,&g)==0);
    CHECK(wl_display_roundtrip_queue(display,queue)>=0 && g.compositor && g.shm && g.manager);
    struct wl_surface *surface=wl_compositor_create_surface(g.compositor); CHECK(surface);
    struct wl_proxy *role=wl_proxy_marshal_flags(g.manager,0,&test_role_interface,1,0,NULL,surface); CHECK(role);
    uint32_t serial=0; CHECK(wl_proxy_add_listener(role,role_listener,&serial)==0);
    CHECK(wl_display_roundtrip_queue(display,queue)>=0 && serial==42);
    wl_proxy_marshal_flags(role,1,NULL,1,0,serial);
    CHECK(wl_display_roundtrip_queue(display,queue)>=0 && atomic_load(&server.acknowledged));
    nx_wayland_shm_host *host=NULL; CHECK(nx_wayland_shm_host_create(display,g.shm,surface,queue,&host)==NX_OK);
    CHECK(pthread_create(&thread,NULL,wrong_thread,host)==0 && pthread_join(thread,NULL)==0);
    nx_wayland_shm_outcome outcome;
    CHECK(nx_wayland_shm_host_present(host,(nx_pixel_size){0,0},1,NULL,0,(nx_color){0},&outcome)==NX_OK && outcome==NX_WAYLAND_SHM_SUSPENDED);
    CHECK(nx_wayland_shm_host_present(host,(nx_pixel_size){1,0},1,NULL,0,(nx_color){0},&outcome)==NX_ERROR_ARGUMENT);
    CHECK(nx_wayland_shm_host_present(host,(nx_pixel_size){40,20},0,NULL,0,(nx_color){0},&outcome)==NX_ERROR_ARGUMENT);
    CHECK(nx_wayland_shm_host_present(host,(nx_pixel_size){UINT32_MAX,2},1,NULL,0,(nx_color){0},&outcome)==NX_ERROR_LIMIT);
    CHECK(nx_wayland_shm_host_present(host,(nx_pixel_size){40,20},1,(nx_command *)1,SIZE_MAX,(nx_color){0},&outcome)==NX_ERROR_LIMIT);
    nx_command bad={.type=NX_COMMAND_POP_CLIP};
    CHECK(nx_wayland_shm_host_present(host,(nx_pixel_size){40,20},1,&bad,1,(nx_color){0},&outcome)==NX_ERROR_COMMAND);
    atomic_store(&server.capture_enabled,1);
    const nx_color clears[]={{0,0,0,0},{1,.5f,.25f,.5f},{0,0,0,0}};
    nx_command square={.type=NX_COMMAND_FILL_RECT,.rect={0,0,2,2},.color={0,0,1,1}};
    for (int i=0;i<3;++i)
        CHECK(nx_wayland_shm_host_present(host,(nx_pixel_size){40,20},1,i==2?&square:NULL,i==2?1u:0u,clears[i],&outcome)==NX_OK && outcome==NX_WAYLAND_SHM_READY);
    CHECK(nx_wayland_shm_host_present(host,(nx_pixel_size){40,20},1,NULL,0,clears[0],&outcome)==NX_OK && outcome==NX_WAYLAND_SHM_BUSY);
    CHECK(wl_display_roundtrip_queue(display,queue)>=0 && atomic_load(&server.captures)==3);
    CHECK(atomic_load(&server.pixels[0])==0 && atomic_load(&server.pixels[1])==0x80804020u && atomic_load(&server.pixels[2])==0xff0000ffu && atomic_load(&server.outside[2])==0);
    atomic_store(&server.capture_enabled,0);
    wl_proxy_marshal_flags(role,2,NULL,1,0);
    CHECK(wl_display_roundtrip(display)>=0);
    CHECK(nx_wayland_shm_host_present(host,(nx_pixel_size){40,20},1,NULL,0,clears[0],&outcome)==NX_OK && outcome==NX_WAYLAND_SHM_BUSY);
    CHECK(wl_display_dispatch_queue_pending(display,queue)>=0);
    CHECK(nx_wayland_shm_host_present(host,(nx_pixel_size){40,20},1,NULL,0,clears[0],&outcome)==NX_OK && outcome==NX_WAYLAND_SHM_READY);
    CHECK(wl_display_roundtrip_queue(display,queue)>=0);
    CHECK(nx_wayland_shm_host_destroy(&host)==NX_OK && !host);
    wl_surface_damage(surface,0,0,40,20); wl_surface_commit(surface);
    CHECK(wl_display_roundtrip_queue(display,queue)>=0);
    CHECK(nx_wayland_shm_host_create(display,g.shm,surface,queue,&host)==NX_OK);
    CHECK(nx_wayland_shm_host_present(host,(nx_pixel_size){40,20},1,NULL,0,clears[0],&outcome)==NX_OK && outcome==NX_WAYLAND_SHM_READY);
    CHECK(wl_display_roundtrip_queue(display,queue)>=0);
    atomic_store(&server.disconnect,1);
    wl_surface_commit(surface);
    CHECK(wl_display_roundtrip_queue(display,queue)<0);
    CHECK(nx_wayland_shm_host_present(host,(nx_pixel_size){40,20},1,NULL,0,clears[0],&outcome)==NX_ERROR_PLATFORM);
    CHECK(nx_wayland_shm_host_destroy(&host)==NX_OK && !host);
    wl_proxy_destroy(role);
    wl_proxy_destroy((struct wl_proxy *)surface); wl_proxy_destroy((struct wl_proxy *)g.shm);
    wl_proxy_destroy((struct wl_proxy *)g.compositor);
    wl_proxy_destroy(g.manager); wl_registry_destroy(registry); wl_event_queue_destroy(queue);
    wl_display_disconnect(display);
    wl_display_terminate(server.base.display); CHECK(pthread_join(server_thread,NULL)==0);
    wl_display_destroy_clients(server.base.display); wl_display_destroy(server.base.display);
    CHECK(unlink(path)==0 || access(path,F_OK)!=0); CHECK(rmdir(runtime)==0);
    puts("Host-owned role, queue, pixels, backpressure and retirement passed.");
    return 0;
}
