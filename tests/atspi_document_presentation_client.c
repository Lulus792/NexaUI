/* Independent OS peer: no NexaUI or model linkage. */
#define main original_large_text_client_main
#include "atspi_large_text_client.c"
#undef main

static AtspiAccessible *fresh(AtspiAccessible *app) {
    pump(30);atspi_accessible_clear_cache(app);
    return find(app,"External document",0);
}

static int rectangle(AtspiText *text,int offset,AtspiCoordType coordinates,int x,int y,int w,int h) {
    GError *e=NULL;AtspiRect *r=atspi_text_get_character_extents(text,offset,coordinates,&e);
    int okay=!e && r && r->x==x && r->y==y && r->width==w && r->height==h;
    if(!okay)fprintf(stderr,"character geometry: offset=%d actual=%d,%d,%d,%d error=%s\n",offset,r?r->x:0,r?r->y:0,r?r->width:0,r?r->height:0,e?e->message:"none");
    g_free(r);g_clear_error(&e);return okay;
}
static int weight(AtspiText *text,int offset,const char *expected) {
    GError *e=NULL;char *s=atspi_text_get_text_attribute_value(text,offset,"weight",&e);
    int okay=!e && s && !strcmp(s,expected);g_free(s);g_clear_error(&e);return okay;
}
static int default_attributes_rejected(AtspiAccessible *field) {
    DBusError error;dbus_error_init(&error);
    DBusConnection *bus=dbus_connection_open_private(getenv("AT_SPI_BUS_ADDRESS"),&error);
    if(!bus){dbus_error_free(&error);return 0;}
    dbus_connection_set_exit_on_disconnect(bus,FALSE);int okay=0;
    if(dbus_bus_register(bus,&error)) {
        AtspiObject *object=ATSPI_OBJECT(field);
        DBusMessage *call=dbus_message_new_method_call(object->app->bus_name,object->path,"org.a11y.atspi.Text","GetDefaultAttributes");
        if(call) {
            DBusMessage *reply=dbus_connection_send_with_reply_and_block(bus,call,1000,&error);
            okay=dbus_error_has_name(&error,DBUS_ERROR_NOT_SUPPORTED);if(reply)dbus_message_unref(reply);dbus_message_unref(call);
        }
    }
    dbus_error_free(&error);dbus_connection_close(bus);dbus_connection_unref(bus);return okay;
}
typedef struct presentation_events {const char *path;unsigned attributes,geometry;} presentation_events;
static void presentation_event(AtspiEvent *event,void *user) {
    presentation_events *events=user;
    if(event->source && !strcmp(ATSPI_OBJECT(event->source)->path,events->path)) {
        if(!strcmp(event->type,"object:text-attributes-changed"))++events->attributes;
        if(!strcmp(event->type,"object:visible-data-changed"))++events->geometry;
    }
    g_boxed_free(ATSPI_TYPE_EVENT,event);
}
int main(int argc,char **argv) {
    CHECK(argc==2);int in[2],out[2];CHECK(!pipe(in) && !pipe(out));child=fork();CHECK(child>=0);
    if(!child){dup2(in[0],STDIN_FILENO);dup2(out[1],STDOUT_FILENO);close(in[0]);close(in[1]);close(out[0]);close(out[1]);execl(argv[1],argv[1],"--presentation",(char *)NULL);_exit(127);}
    close(in[0]);close(out[1]);commands=fdopen(in[1],"w");responses=fdopen(out[0],"r");CHECK(commands && responses && response("READY\n") && !atspi_init());atspi_set_timeout(1000,1000);
    AtspiAccessible *desktop=atspi_get_desktop(0),*app=NULL;CHECK(desktop);
    for(unsigned tries=0;tries<300 && !app;++tries) {
        pump(10);atspi_accessible_clear_cache(desktop);GError *e=NULL;int count=atspi_accessible_get_child_count(desktop,&e);
        if(e){g_clear_error(&e);continue;}
        for(int i=0;i<count;++i){AtspiAccessible *a=atspi_accessible_get_child_at_index(desktop,i,&e);if(e){g_clear_error(&e);continue;}if(!a)continue;
            guint pid=atspi_accessible_get_process_id(a,&e);if(!e && pid==(guint)child){app=a;break;}g_clear_error(&e);g_object_unref(a);}
    }
    CHECK(app);AtspiAccessible *field=fresh(app);CHECK(field);AtspiText *text=atspi_accessible_get_text_iface(field);CHECK(text);
    GError *e=NULL;CHECK(atspi_text_get_character_count(text,&e)==4 && !e);
    CHECK(rectangle(text,1,ATSPI_COORD_TYPE_WINDOW,28,30,9,16));
    CHECK(rectangle(text,2,ATSPI_COORD_TYPE_WINDOW,36,30,17,16));
    CHECK(rectangle(text,1,ATSPI_COORD_TYPE_PARENT,23,24,9,16));
    AtspiRect *r=atspi_text_get_range_extents(text,1,3,ATSPI_COORD_TYPE_WINDOW,&e);
    CHECK(!e && r && r->x==28 && r->y==30 && r->width==25 && r->height==16);g_free(r);
    r=atspi_text_get_range_extents(text,4,4,ATSPI_COORD_TYPE_WINDOW,&e);CHECK(!e && r && r->x==60 && r->width==1);g_free(r);
    CHECK(atspi_text_get_offset_at_point(text,41,35,ATSPI_COORD_TYPE_WINDOW,&e)==2 && !e);
    CHECK(atspi_text_get_offset_at_point(text,36,29,ATSPI_COORD_TYPE_PARENT,&e)==2 && !e);
    CHECK(atspi_text_get_offset_at_point(text,310,155,ATSPI_COORD_TYPE_WINDOW,&e)==-1 && !e);
    r=atspi_text_get_character_extents(text,4,ATSPI_COORD_TYPE_WINDOW,&e);CHECK(e);g_clear_error(&e);g_free(r);
    r=atspi_text_get_character_extents(text,0,ATSPI_COORD_TYPE_SCREEN,&e);CHECK(e);g_clear_error(&e);g_free(r);
    GHashTable *attrs=atspi_text_get_default_attributes(text,&e);
    CHECK(!e && attrs && g_hash_table_size(attrs)==2 && !strcmp(g_hash_table_lookup(attrs,"family-name"),"Fixture mono"));g_hash_table_unref(attrs);
    int first=-1,last=-1;attrs=atspi_text_get_attribute_run(text,1,FALSE,&first,&last,&e);
    CHECK(!e && attrs && first==1 && last==2 && g_hash_table_size(attrs)==1 && !strcmp(g_hash_table_lookup(attrs,"weight"),"700"));g_hash_table_unref(attrs);
    attrs=atspi_text_get_attribute_run(text,1,TRUE,&first,&last,&e);
    CHECK(!e && attrs && first==1 && last==2 && g_hash_table_size(attrs)==2 && !strcmp(g_hash_table_lookup(attrs,"weight"),"700"));g_hash_table_unref(attrs);
    attrs=atspi_text_get_text_attributes(text,0,&first,&last,&e);CHECK(!e && attrs && first==0 && last==1 && !g_hash_table_size(attrs));g_hash_table_unref(attrs);
    CHECK(weight(text,0,"400") && weight(text,1,"700"));
    presentation_events events={ATSPI_OBJECT(field)->path,0,0};
    AtspiEventListener *listener=atspi_event_listener_new(presentation_event,&events,NULL);CHECK(listener);
    CHECK(atspi_event_listener_register(listener,"object:text-attributes-changed",&e) && !e);
    CHECK(atspi_event_listener_register(listener,"object:visible-data-changed",&e) && !e);
    CHECK(command("presentation-change") && rectangle(text,1,ATSPI_COORD_TYPE_WINDOW,38,30,9,16) && weight(text,1,"600"));
    for(unsigned i=0;i<100 && (!events.attributes || !events.geometry);++i)pump(10);
    CHECK(events.attributes==1 && events.geometry==1 && command("presentation-same"));pump(30);
    CHECK(events.attributes==1 && events.geometry==1);
    CHECK(command("presentation-off"));r=atspi_text_get_character_extents(text,0,ATSPI_COORD_TYPE_WINDOW,&e);CHECK(e);g_clear_error(&e);g_free(r);
    CHECK(default_attributes_rejected(field));
    CHECK(command("presentation-on") && rectangle(text,1,ATSPI_COORD_TYPE_WINDOW,38,30,9,16));
    CHECK(atspi_event_listener_deregister(listener,"object:text-attributes-changed",&e) && !e);
    CHECK(atspi_event_listener_deregister(listener,"object:visible-data-changed",&e) && !e);g_object_unref(listener);
    CHECK(command("large"));r=atspi_text_get_character_extents(text,0,ATSPI_COORD_TYPE_WINDOW,&e);CHECK(e);g_clear_error(&e);g_free(r);
    g_object_unref(text);g_object_unref(field);field=fresh(app);CHECK(field);text=atspi_accessible_get_text_iface(field);CHECK(text);
    CHECK(atspi_text_get_character_count(text,&e)==8*1024*1024 && !e && rectangle(text,0,ATSPI_COORD_TYPE_WINDOW,30,30,9,16));
    r=atspi_text_get_character_extents(text,8*1024*1024-1,ATSPI_COORD_TYPE_WINDOW,&e);CHECK(e);g_clear_error(&e);g_free(r);
    CHECK(weight(text,8*1024*1024-1,"400"));
    CHECK(command("detach"));r=atspi_text_get_character_extents(text,0,ATSPI_COORD_TYPE_WINDOW,&e);CHECK(e);g_clear_error(&e);g_free(r);
    CHECK(fprintf(commands,"quit\n")>0 && !fflush(commands));int status=0;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));child=-1;
    g_object_unref(text);g_object_unref(field);g_object_unref(app);g_object_unref(desktop);atspi_exit();cleanup();
    puts("AT-SPI document presentation: scalar/window/parent geometry, range/caret, hit test, attributes/default override, actual replacement/omission,8MiB sparse bounds, stale/unknown-screen/detach and clean exit passed");return 0;
}
