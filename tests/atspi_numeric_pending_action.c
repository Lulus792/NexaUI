#define _POSIX_C_SOURCE 200809L
#include <atspi/atspi.h>
#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void pump(void) {
    while(g_main_context_iteration(NULL,FALSE)){}
    struct timespec delay={0,10000000};nanosleep(&delay,NULL);
}
static AtspiAccessible *find(AtspiAccessible *node,unsigned depth) {
    if(!node || depth>20)return NULL;
    GError *error=NULL;char *name=atspi_accessible_get_name(node,&error);
    int found=!error && name && !strcmp(name,"Apply");g_clear_error(&error);g_free(name);
    if(found) {
        AtspiRole role=atspi_accessible_get_role(node,&error);
        found=!error && role==ATSPI_ROLE_PUSH_BUTTON;g_clear_error(&error);
        if(found)return g_object_ref(node);
    }
    int count=atspi_accessible_get_child_count(node,&error);
    if(error){g_clear_error(&error);return NULL;}
    for(int i=0;i<count;++i) {
        AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,&error);
        if(error){g_clear_error(&error);continue;}
        AtspiAccessible *result=find(child,depth+1);if(child)g_object_unref(child);
        if(result)return result;
    }
    return NULL;
}
static AtspiAccessible *find_app(AtspiAccessible *desktop,unsigned long wanted) {
    GError *error=NULL;atspi_accessible_clear_cache(desktop);
    int count=atspi_accessible_get_child_count(desktop,&error);
    if(error){g_clear_error(&error);return NULL;}
    for(int i=0;i<count;++i) {
        AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,&error);
        if(error){g_clear_error(&error);continue;}if(!app)continue;
        guint pid=atspi_accessible_get_process_id(app,&error);
        if(!error && pid==wanted)return app;
        g_clear_error(&error);g_object_unref(app);
    }
    return NULL;
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    char *end=NULL;unsigned long pid=strtoul(argv[1],&end,10);
    const char *address=getenv("NEXAUI_TEST_ATSPI_ADDRESS");
    if(!pid || !end || *end || !address || atspi_init()!=0)return 2;
    atspi_set_timeout(1000,1000);AtspiAccessible *desktop=atspi_get_desktop(0),*button=NULL;
    for(unsigned i=0;desktop && i<300 && !button;++i) {
        AtspiAccessible *app=find_app(desktop,pid);
        if(app){button=find(app,0);g_object_unref(app);}if(!button)pump();
    }
    if(!button){fputs("Apply not found\n",stderr);return 1;}
    AtspiObject *object=ATSPI_OBJECT(button);
    DBusError error;dbus_error_init(&error);
    DBusConnection *bus=dbus_connection_open_private(address,&error);
    if(!bus || !dbus_bus_register(bus,&error))return 1;
    dbus_connection_set_exit_on_disconnect(bus,FALSE);
    setvbuf(stdout,NULL,_IOLBF,0);puts("READY");
    char input[16];if(!fgets(input,sizeof(input),stdin) || strcmp(input,"GO\n"))return 1;
    DBusMessage *request=dbus_message_new_method_call(object->app->bus_name,object->path,
        "org.a11y.atspi.Action","DoAction");
    DBusPendingCall *pending=NULL;dbus_int32_t index=0;
    if(!request || !dbus_message_append_args(request,DBUS_TYPE_INT32,&index,DBUS_TYPE_INVALID) ||
       !dbus_connection_send_with_reply(bus,request,&pending,8000) || !pending)return 1;
    dbus_message_unref(request);dbus_connection_flush(bus);
    /* A round trip to the bus on the same connection establishes that the
       preceding action was routed before we tell the parent to resume host. */
    char *id=dbus_bus_get_id(bus,&error);if(!id)return 1;dbus_free(id);
    puts("SENT");dbus_pending_call_block(pending);
    DBusMessage *reply=dbus_pending_call_steal_reply(pending);dbus_bool_t accepted=FALSE;
    int okay=reply && dbus_message_get_args(reply,&error,DBUS_TYPE_BOOLEAN,&accepted,DBUS_TYPE_INVALID);
    if(okay)printf("RESULT %d\n",accepted?1:0);
    if(reply)dbus_message_unref(reply);
    dbus_pending_call_unref(pending);
    int disabled=0;
    for(unsigned i=0;okay && i<200 && !disabled;++i) {
        atspi_accessible_clear_cache(button);
        AtspiStateSet *states=atspi_accessible_get_state_set(button);
        if(states){disabled=!atspi_state_set_contains(states,ATSPI_STATE_ENABLED);g_object_unref(states);}
        if(!disabled)pump();
    }
    AtspiAction *action=atspi_accessible_get_action_iface(button);GError *action_error=NULL;
    if(!disabled || !action || atspi_action_do_action(action,0,&action_error) || action_error)okay=0;
    if(action_error)g_error_free(action_error);
    if(action)g_object_unref(action);
    if(okay)puts("DISABLED");
    dbus_connection_close(bus);dbus_connection_unref(bus);
    g_object_unref(button);if(desktop)g_object_unref(desktop);atspi_exit();
    return okay?0:1;
}
