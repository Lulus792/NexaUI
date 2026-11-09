/* Independent AT-SPI/wire peer. Reuse discovery/pacing from the scroll peer. */
#define main original_scroll_client_main
#include "atspi_scroll_client.c"
#undef main
#include <dbus/dbus.h>
#include <poll.h>
#include <sys/socket.h>

typedef struct endpoint { char *bus,*path; } endpoint;
static void endpoint_clear(endpoint *e){g_free(e->bus);g_free(e->path);*e=(endpoint){0};}
static int endpoint_set(endpoint *e,AtspiAccessible *node) {
    AtspiObject *o=ATSPI_OBJECT(node);if(!o->app || !o->path)return 0;
    endpoint_clear(e);e->bus=g_strdup(o->app->bus_name);e->path=g_strdup(o->path);return e->bus && e->path;
}
static int control(int fd,const char *command,const char *expected) {
    if(send(fd,command,strlen(command),MSG_NOSIGNAL)!=(ssize_t)strlen(command))return 0;
    struct pollfd p={fd,POLLIN,0};if(poll(&p,1,1000)!=1 || !(p.revents&POLLIN))return 0;
    char text[128];ssize_t n=recv(fd,text,sizeof(text)-1,MSG_TRUNC);
    if(n<=0 || (size_t)n>=sizeof(text))return 0;text[n]=0;
    if(strcmp(text,expected)){fprintf(stderr,"control %s: expected=%s actual=%s\n",command,expected,text);return 0;}return 1;
}
static AtspiAccessible *fresh(AtspiAccessible *app) {
    for(unsigned i=0;i<300;++i) {
        while(g_main_context_iteration(NULL,FALSE)){}
        atspi_accessible_clear_cache(app);AtspiAccessible *node=find(app,"Numeric",0);
        if(node)return node;pause_ms();
    }
    return NULL;
}
static int metadata(AtspiAccessible *node,double value,double maximum,int editable) {
    atspi_accessible_clear_cache(node);AtspiValue *v=atspi_accessible_get_value_iface(node);if(!v)return 0;
    GError *error=NULL;int okay=atspi_value_get_minimum_value(v,&error)==0 && !error &&
        atspi_value_get_maximum_value(v,&error)==maximum && !error &&
        atspi_value_get_minimum_increment(v,&error)==1 && !error && atspi_value_get_current_value(v,&error)==value && !error;
    AtspiStateSet *states=atspi_accessible_get_state_set(node);
    okay=okay && states && !!atspi_state_set_contains(states,ATSPI_STATE_EDITABLE)==!!editable;
    if(states)g_object_unref(states);if(error)g_error_free(error);g_object_unref(v);return okay;
}
static DBusMessage *call(DBusConnection *bus,const endpoint *e,const char *interface,const char *method,const char *argument) {
    DBusMessage *message=dbus_message_new_method_call(e->bus,e->path,interface,method);if(!message)return NULL;
    if(argument && !dbus_message_append_args(message,DBUS_TYPE_STRING,&argument,DBUS_TYPE_INVALID)){dbus_message_unref(message);return NULL;}
    DBusError error;dbus_error_init(&error);DBusMessage *reply=dbus_connection_send_with_reply_and_block(bus,message,1000,&error);
    if(dbus_error_is_set(&error))fprintf(stderr,"wire %s: %s\n",method,error.name);
    dbus_error_free(&error);dbus_message_unref(message);return reply;
}
static int get_current(DBusConnection *bus,const endpoint *e,double expected,const char *expected_error) {
    const char *interface="org.a11y.atspi.Value",*property="CurrentValue";
    DBusMessage *message=dbus_message_new_method_call(e->bus,e->path,DBUS_INTERFACE_PROPERTIES,"Get");if(!message)return 0;
    if(!dbus_message_append_args(message,DBUS_TYPE_STRING,&interface,DBUS_TYPE_STRING,&property,DBUS_TYPE_INVALID)){dbus_message_unref(message);return 0;}
    DBusError error;dbus_error_init(&error);DBusMessage *reply=dbus_connection_send_with_reply_and_block(bus,message,1000,&error);int okay=0;
    if(expected_error)okay=!reply && dbus_error_has_name(&error,expected_error);
    else if(reply && !dbus_error_is_set(&error) && dbus_message_has_signature(reply,"v")) {
        DBusMessageIter args,v;double value=0;dbus_message_iter_init(reply,&args);dbus_message_iter_recurse(&args,&v);
        if(dbus_message_iter_get_arg_type(&v)==DBUS_TYPE_DOUBLE){dbus_message_iter_get_basic(&v,&value);okay=value==expected;}
    }
    if(!okay)fprintf(stderr,"CurrentValue expected=%.17g error=%s actual-error=%s\n",expected,expected_error?expected_error:"none",error.name?error.name:"none");
    if(reply)dbus_message_unref(reply);dbus_error_free(&error);dbus_message_unref(message);return okay;
}
static DBusPendingCall *set_later(DBusConnection *bus,const endpoint *e,double number) {
    const char *interface="org.a11y.atspi.Value",*property="CurrentValue";DBusPendingCall *pending=NULL;
    DBusMessage *message=dbus_message_new_method_call(e->bus,e->path,DBUS_INTERFACE_PROPERTIES,"Set");if(!message)return NULL;
    DBusMessageIter args,v;int okay=!!dbus_message_append_args(message,DBUS_TYPE_STRING,&interface,DBUS_TYPE_STRING,&property,DBUS_TYPE_INVALID);
    dbus_message_iter_init_append(message,&args);
    okay=okay && dbus_message_iter_open_container(&args,DBUS_TYPE_VARIANT,"d",&v) &&
        dbus_message_iter_append_basic(&v,DBUS_TYPE_DOUBLE,&number) && dbus_message_iter_close_container(&args,&v);
    if(okay)okay=!!dbus_connection_send_with_reply(bus,message,&pending,1000);
    dbus_message_unref(message);if(!okay)return NULL;dbus_connection_flush(bus);return pending;
}
static int refused(DBusPendingCall **pending) {
    dbus_pending_call_block(*pending);DBusMessage *reply=dbus_pending_call_steal_reply(*pending);int okay=reply &&
        dbus_message_get_type(reply)==DBUS_MESSAGE_TYPE_ERROR && !strcmp(dbus_message_get_error_name(reply),DBUS_ERROR_FAILED);
    if(!okay)fprintf(stderr,"deferred Set reply: %s\n",reply?(dbus_message_get_error_name(reply)?dbus_message_get_error_name(reply):"success"):"none");
    if(reply)dbus_message_unref(reply);dbus_pending_call_unref(*pending);*pending=NULL;return okay;
}
static int schema(DBusConnection *bus,const endpoint *e) {
    DBusMessage *reply=call(bus,e,"org.freedesktop.DBus.Introspectable","Introspect",NULL);if(!reply)return 0;
    const char *xml=NULL;int okay=dbus_message_get_args(reply,NULL,DBUS_TYPE_STRING,&xml,DBUS_TYPE_INVALID) &&
        strstr(xml,"org.a11y.atspi.Value") && strstr(xml,"MinimumIncrement") && strstr(xml,"CurrentValue\" type=\"d\" access=\"readwrite\"");
    dbus_message_unref(reply);if(!okay)return 0;
    reply=call(bus,e,DBUS_INTERFACE_PROPERTIES,"GetAll","org.a11y.atspi.Value");if(!reply)return 0;
    okay=!!dbus_message_has_signature(reply,"a{sv}");unsigned fields=0,count=0;
    if(okay) {
        DBusMessageIter args,array;dbus_message_iter_init(reply,&args);dbus_message_iter_recurse(&args,&array);
        while(okay && dbus_message_iter_get_arg_type(&array)!=DBUS_TYPE_INVALID) {
            DBusMessageIter entry,v;const char *name;dbus_message_iter_recurse(&array,&entry);
            dbus_message_iter_get_basic(&entry,&name);dbus_message_iter_next(&entry);dbus_message_iter_recurse(&entry,&v);
            int type=dbus_message_iter_get_arg_type(&v);unsigned bit=0;
            if(!strcmp(name,"version") && type==DBUS_TYPE_UINT32){dbus_uint32_t version;dbus_message_iter_get_basic(&v,&version);okay=version==1;bit=1;}
            else if(!strcmp(name,"Text") && type==DBUS_TYPE_STRING){const char *text;dbus_message_iter_get_basic(&v,&text);okay=!strcmp(text,"5");bit=2;}
            else if(type==DBUS_TYPE_DOUBLE) {
                double value;dbus_message_iter_get_basic(&v,&value);
                if(!strcmp(name,"MinimumValue")){okay=value==0;bit=4;}
                else if(!strcmp(name,"MaximumValue")){okay=value==10;bit=8;}
                else if(!strcmp(name,"MinimumIncrement")){okay=value==1;bit=16;}
                else if(!strcmp(name,"CurrentValue")){okay=value==5;bit=32;}
            }
            okay=okay && bit && !(fields&bit);fields|=bit;++count;dbus_message_iter_next(&array);
        }
    }
    dbus_message_unref(reply);return okay && fields==63 && count==6;
}
int main(int argc,char **argv) {
    int okay=0,channel[2]={-1,-1},output[2]={-1,-1},initialized=0;pid_t child=-1;
    AtspiAccessible *desktop=NULL,*app=NULL,*node=NULL;DBusConnection *bus=NULL;DBusPendingCall *pending=NULL;endpoint target={0};
    CHECK(argc==2 && !socketpair(AF_UNIX,SOCK_SEQPACKET,0,channel) && !pipe(output));child=fork();CHECK(child>=0);
    if(!child) {
        close(channel[0]);close(output[0]);char fd[32];(void)snprintf(fd,sizeof(fd),"%d",channel[1]);
        if(setenv("NEXAUI_NUMERIC_TEST_FD",fd,1) || dup2(output[1],STDOUT_FILENO)<0)_exit(126);
        close(output[1]);execl(argv[1],argv[1],(char *)NULL);_exit(127);
    }
    close(channel[1]);channel[1]=-1;close(output[1]);output[1]=-1;
    CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    for(unsigned i=0;i<300 && !app;++i){app=by_pid(desktop,child);if(!app)pause_ms();}CHECK(app);
    node=fresh(app);CHECK(node && metadata(node,5,10,1) && endpoint_set(&target,node));
    DBusError error;dbus_error_init(&error);bus=dbus_connection_open_private(getenv("AT_SPI_BUS_ADDRESS"),&error);
    CHECK(bus && !dbus_error_is_set(&error));dbus_connection_set_exit_on_disconnect(bus,FALSE);CHECK(dbus_bus_register(bus,&error));dbus_error_free(&error);
    CHECK(schema(bus,&target));
    AtspiValue *value=atspi_accessible_get_value_iface(node);GError *ge=NULL;CHECK(value);
    int assigned=atspi_value_set_current_value(value,6,&ge) && !ge;g_object_unref(value);g_clear_error(&ge);
    CHECK(assigned && metadata(node,6,10,1) && control(channel[0],"STATE","STATE 6 10 0 0"));
    const char *changes[]={"DISABLE","RANGE","OBSERVE","PARENT","KIND"};
    const char *states[]={"STATE 5 10 1 0","STATE 5 8 0 0","STATE 7 10 0 0","STATE 5 10 0 0","STATE 5 10 0 1"};
    for(unsigned i=0;i<5;++i) {
        CHECK(control(channel[0],"RESET","OK"));g_object_unref(node);node=fresh(app);CHECK(node && endpoint_set(&target,node) && metadata(node,5,10,1));
        CHECK(control(channel[0],"HOLD","OK"));pending=set_later(bus,&target,9);CHECK(pending && get_current(bus,&target,5,NULL));
        CHECK(control(channel[0],changes[i],"OK") && refused(&pending) && control(channel[0],"STATE",states[i]));
        if(i>=3)CHECK(get_current(bus,&target,0,"org.a11y.atspi.Error.Defunct"));
        printf("NUMERIC_CANCEL %s\n",changes[i]);
    }
    CHECK(control(channel[0],"RESET","OK"));g_object_unref(node);node=fresh(app);CHECK(node && endpoint_set(&target,node));
    CHECK(control(channel[0],"HOLD","OK") && control(channel[0],"UNPUBLISHED","OK"));
    pending=set_later(bus,&target,9);CHECK(pending && get_current(bus,&target,5,NULL));
    CHECK(control(channel[0],"PUBLISH","OK") && refused(&pending) && get_current(bus,&target,7,NULL) && control(channel[0],"STATE","STATE 7 10 0 0"));
    puts("NUMERIC_CANCEL unpublished-frame");
    CHECK(control(channel[0],"RESET","OK"));g_object_unref(node);node=fresh(app);CHECK(node && endpoint_set(&target,node));
    CHECK(control(channel[0],"HOLD","OK"));pending=set_later(bus,&target,9);CHECK(pending && get_current(bus,&target,5,NULL));
    CHECK(control(channel[0],"FORWARD","ROUTED 1") && control(channel[0],"RETIRE","OK") && refused(&pending));
    CHECK(get_current(bus,&target,0,"org.a11y.atspi.Error.Defunct") && control(channel[0],"STATE","STATE 5 10 0 0"));
    CHECK(control(channel[0],"RESUME","OK"));g_object_unref(node);node=fresh(app);CHECK(node && metadata(node,5,10,1));
    puts("NUMERIC_CANCEL retired-forwarded-no-replay");
    CHECK(control(channel[0],"CLOSE","OK"));int status=0;pid_t ended=0;
    for(unsigned i=0;i<300 && !ended;++i){ended=waitpid(child,&status,WNOHANG);if(!ended)pause_ms();}
    CHECK(ended==child && WIFEXITED(status) && !WEXITSTATUS(status));child=-1;
    char text[128]={0};CHECK(read(output[0],text,sizeof(text)-1)>0 && strstr(text,"NUMERIC_LEASE_CLOSED 5 "));fputs(text,stdout);okay=1;
cleanup:
    if(pending){dbus_pending_call_cancel(pending);dbus_pending_call_unref(pending);}
    if(child>0){kill(child,SIGKILL);waitpid(child,NULL,0);}
    if(bus){dbus_connection_close(bus);dbus_connection_unref(bus);}endpoint_clear(&target);
    if(node)g_object_unref(node);if(app)g_object_unref(app);if(desktop)g_object_unref(desktop);if(initialized)atspi_exit();
    for(unsigned i=0;i<2;++i){if(channel[i]>=0)close(channel[i]);if(output[i]>=0)close(output[i]);}
    if(okay)puts("Native numeric Value: schema, assignment, seven cancellations and clean two-window retirement passed");return okay?0:1;
}
