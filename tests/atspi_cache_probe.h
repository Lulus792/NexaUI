#ifndef NEXAUI_TEST_ATSPI_CACHE_PROBE_H
#define NEXAUI_TEST_ATSPI_CACHE_PROBE_H
#include <atspi/atspi.h>
#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cache_ref(DBusMessageIter *item,const char **bus,const char **path) {
    if(dbus_message_iter_get_arg_type(item)!=DBUS_TYPE_STRUCT)return 0;
    DBusMessageIter ref;dbus_message_iter_recurse(item,&ref);
    if(dbus_message_iter_get_arg_type(&ref)!=DBUS_TYPE_STRING)return 0;
    dbus_message_iter_get_basic(&ref,bus);
    if(!dbus_message_iter_next(&ref) || dbus_message_iter_get_arg_type(&ref)!=DBUS_TYPE_OBJECT_PATH)return 0;
    dbus_message_iter_get_basic(&ref,path);return !dbus_message_iter_next(&ref);
}
/* Raw independent wire query: libatspi's lazy fallback must not hide a failed
   bulk query. The existing native one-second call deadline is retained. */
static int cache_probe(AtspiAccessible *field,int refusal,size_t maximum_bytes) {
    DBusError error;dbus_error_init(&error);DBusConnection *bus=NULL;DBusMessage *call=NULL,*reply=NULL;
    char *wire=NULL;int wire_bytes=0,okay=0,items=0,found=0;GError *ge=NULL;gchar *name=NULL;
    AtspiObject *object=ATSPI_OBJECT(field);const char *address=getenv("AT_SPI_BUS_ADDRESS");
    if(!address || !object || !object->app)goto done;
    bus=dbus_connection_open_private(address,&error);if(!bus)goto done;
    dbus_connection_set_exit_on_disconnect(bus,FALSE);if(!dbus_bus_register(bus,&error))goto done;
    call=dbus_message_new_method_call(object->app->bus_name,"/org/a11y/atspi/cache","org.a11y.atspi.Cache","GetItems");
    if(!call)goto done;
    reply=dbus_connection_send_with_reply_and_block(bus,call,1000,&error);
    if(refusal) {okay=!reply && dbus_error_has_name(&error,DBUS_ERROR_LIMITS_EXCEEDED);goto done;}
    if(!reply || dbus_error_is_set(&error) || !dbus_message_has_signature(reply,"a((so)(so)(so)iiassusau)") ||
       !dbus_message_marshal(reply,&wire,&wire_bytes) || wire_bytes<=0 || (size_t)wire_bytes>maximum_bytes)goto done;
    name=atspi_accessible_get_name(field,&ge);if(ge || !name)goto done;
    DBusMessageIter array,iterator;if(!dbus_message_iter_init(reply,&iterator))goto done;
    dbus_message_iter_recurse(&iterator,&array);
    while(dbus_message_iter_get_arg_type(&array)!=DBUS_TYPE_INVALID) {
        DBusMessageIter item,interfaces;const char *ref_bus,*path,*application_bus,*application_path,*parent_bus,*parent_path;
        dbus_message_iter_recurse(&array,&item);
        if(!cache_ref(&item,&ref_bus,&path) || !dbus_message_iter_next(&item) ||
           !cache_ref(&item,&application_bus,&application_path) || !dbus_message_iter_next(&item) ||
           !cache_ref(&item,&parent_bus,&parent_path) || !dbus_message_iter_next(&item))goto done;
        (void)parent_bus;(void)parent_path;
        if(!dbus_message_iter_next(&item) || !dbus_message_iter_next(&item))goto done;
        dbus_message_iter_recurse(&item,&interfaces);int text=0;
        while(dbus_message_iter_get_arg_type(&interfaces)!=DBUS_TYPE_INVALID) {
            const char *interface;dbus_message_iter_get_basic(&interfaces,&interface);
            if(!strcmp(interface,"org.a11y.atspi.Text"))text=1;
            dbus_message_iter_next(&interfaces);
        }
        if(!dbus_message_iter_next(&item))goto done;
        const char *cached_name;dbus_message_iter_get_basic(&item,&cached_name);
        if(!strcmp(ref_bus,object->app->bus_name) && !strcmp(path,object->path)) {
            if(found || !text || strcmp(cached_name,name) || strcmp(application_bus,ref_bus) ||
               strcmp(application_path,"/org/a11y/atspi/accessible/root"))goto done;
            found=1;
        }
        ++items;dbus_message_iter_next(&array);
    }
    okay=found;
done:
    if(!okay)fprintf(stderr,"native cache: expected_refusal=%d bytes=%d items=%d error=%s\n",refusal,wire_bytes,items,error.name?error.name:"none");
    else fprintf(stderr,"native cache passed: refused=%d bytes=%d items=%d\n",refusal,wire_bytes,items);
    g_clear_error(&ge);g_free(name);dbus_free(wire);
    if(reply)dbus_message_unref(reply);
    if(call)dbus_message_unref(call);
    if(bus){dbus_connection_close(bus);dbus_connection_unref(bus);}
    dbus_error_free(&error);return okay;
}
#endif
