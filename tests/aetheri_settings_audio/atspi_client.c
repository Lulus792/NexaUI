#define _POSIX_C_SOURCE 200809L
/* Independent native peer; no NexaUI or Aetheri linkage. The inherited private
 * test channel controls observations only, never production service admission. */
#include <atspi/atspi.h>
#include <dbus/dbus.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"controlled audio peer:%d: %s\n",__LINE__,#x);goto done;}} while(0)
static unsigned visits;
static int numeric_event_seen,numeric_event_bad;
static char *numeric_target_bus,*numeric_target_path;
static void numeric_event(AtspiEvent *event,void *user) {
    (void)user;
    if(event->source && numeric_target_path && !strcmp(ATSPI_OBJECT(event->source)->path,numeric_target_path) && event->type &&
       !strcmp(event->type,"object:property-change:accessible-value")) {
        fprintf(stderr,"NUMERIC_EVENT type=%s payload=%s\n",event->type,G_VALUE_TYPE_NAME(&event->any_data));
        /* 2.60.5 dispatches double variants as its gint0 fallback. The raw
         * independent listener below must still verify the actual double. */
        if(G_VALUE_HOLDS_DOUBLE(&event->any_data)) {
            double value=g_value_get_double(&event->any_data);
            if(!isfinite(value) || value<0 || value>100)numeric_event_bad=1;
        } else if(!G_VALUE_HOLDS_INT(&event->any_data) || g_value_get_int(&event->any_data)!=0)
            numeric_event_bad=1;
        numeric_event_seen=1;
    }
    g_boxed_free(ATSPI_TYPE_EVENT,event);
}
static void pump(void) {while(g_main_context_iteration(NULL,FALSE)){}struct timespec t={0,10000000};nanosleep(&t,NULL);}
static AtspiAccessible *find(AtspiAccessible *node,const char *name,AtspiRole role,unsigned depth) {
    if(!node || depth>16 || ++visits>256)return NULL;
    GError *error=NULL;atspi_accessible_clear_cache(node);char *text=atspi_accessible_get_name(node,&error);
    bool same=!error && text && !strcmp(text,name);g_free(text);
    if(error){g_error_free(error);return NULL;}
    if(same && atspi_accessible_get_role(node,&error)==role && !error)return g_object_ref(node);
    if(error){g_error_free(error);return NULL;}
    int count=atspi_accessible_get_child_count(node,&error);
    if(error){g_error_free(error);return NULL;}
    if(count<0 || count>256)return NULL;
    for(int i=0;i<count;++i) {
        AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,&error);
        if(error){g_error_free(error);return NULL;}
        AtspiAccessible *result=find(child,name,role,depth+1);if(child)g_object_unref(child);
        if(result)return result;
    }
    return NULL;
}
static AtspiAccessible *lookup(AtspiAccessible *app,const char *name,AtspiRole role) {visits=0;return find(app,name,role,0);}
static AtspiAccessible *application(AtspiAccessible *desktop,pid_t pid) {
    for(unsigned attempt=0;attempt<300;++attempt) {
        GError *error=NULL;atspi_accessible_clear_cache(desktop);int count=atspi_accessible_get_child_count(desktop,&error);
        if(error){g_error_free(error);return NULL;}
        if(count<0 || count>16)return NULL;
        for(int i=0;i<count;++i) {
            AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,&error);
            if(error){g_error_free(error);return NULL;}
            if(!app)continue;
            guint current=atspi_accessible_get_process_id(app,&error);
            if(!error && current==(guint)pid)return app;
            if(error){g_error_free(error);error=NULL;}g_object_unref(app);
        }
        pump();
    }
    return NULL;
}
static bool state(AtspiAccessible *node,AtspiStateType value) {
    atspi_accessible_clear_cache(node);AtspiStateSet *states=atspi_accessible_get_state_set(node);
    bool result=states && atspi_state_set_contains(states,value);if(states)g_object_unref(states);return result;
}
static int action_index(AtspiAccessible *node,const char *name) {
    AtspiAction *actions=atspi_accessible_get_action_iface(node);if(!actions)return -1;
    GError *error=NULL;int count=atspi_action_get_n_actions(actions,&error),index=-1;
    if(!error && count>=0 && count<=32)for(int i=0;i<count;++i) {
        char *current=atspi_action_get_action_name(actions,i,&error);
        bool same=!error && current && !strcmp(current,name);g_free(current);
        if(same){index=i;break;}
        if(error)break;
    }
    if(error)g_error_free(error);g_object_unref(actions);return index;
}
static bool invoke(AtspiAccessible *node,int index) {
    AtspiAction *actions=atspi_accessible_get_action_iface(node);if(!actions)return false;
    GError *error=NULL;bool result=atspi_action_do_action(actions,index,&error) && !error;
    if(error)g_error_free(error);g_object_unref(actions);return result;
}
static bool press(AtspiAccessible *app,const char *name,AtspiRole role) {
    for(unsigned i=0;i<300;++i) {
        AtspiAccessible *node=lookup(app,name,role);
        if(node) {
            int index=action_index(node,"click");bool enabled=state(node,ATSPI_STATE_ENABLED);
            bool result=enabled && index>=0 && invoke(node,index);g_object_unref(node);
            if(result)return true;
        }
        pump();
    }
    return false;
}
static bool slider(AtspiAccessible *node,const char *expected,bool enabled) {
    for(unsigned attempt=0;attempt<300;++attempt) {
        GError *error=NULL;atspi_accessible_clear_cache(node);GHashTable *attributes=atspi_accessible_get_attributes(node,&error);
        const char *value=attributes?g_hash_table_lookup(attributes,"nexaui-value"):NULL;
        bool same=!error && value && !strcmp(value,expected) && state(node,ATSPI_STATE_ENABLED)==enabled;
        if(error)g_error_free(error);if(attributes)g_hash_table_unref(attributes);
        if(same)return true;pump();
    }
    return false;
}
static bool label(AtspiAccessible *app,const char *name) {
    for(unsigned attempt=0;attempt<300;++attempt) {
        AtspiAccessible *node=lookup(app,name,ATSPI_ROLE_LABEL);
        if(node){g_object_unref(node);return true;}pump();
    }
    return false;
}
static bool disabled(AtspiAccessible *app,const char *name,AtspiRole role) {
    AtspiAccessible *node=lookup(app,name,role);if(!node)return false;
    int index=action_index(node,"click");
    bool ok=!state(node,ATSPI_STATE_ENABLED) && index>=0 && !invoke(node,index);
    g_object_unref(node);return ok;
}
static bool command(int fd,const char *text,char *reply,size_t capacity) {
    if(send(fd,text,strlen(text),MSG_NOSIGNAL)!=(ssize_t)strlen(text))return false;
    struct pollfd p={fd,POLLIN,0};int ready;
    do {ready=poll(&p,1,2000);}while(ready<0 && errno==EINTR);
    if(ready!=1 || !(p.revents&POLLIN))return false;
    ssize_t n=recv(fd,reply,capacity-1,MSG_DONTWAIT);if(n<=0)return false;reply[n]=0;return true;
}
static bool observe(int fd,const char *expected) {
    char reply[256];
    for(unsigned attempt=0;attempt<300;++attempt) {
        if(!command(fd,"STATE",reply,sizeof(reply)))return false;
        if(!strcmp(reply,expected)){printf("BACKEND %s\n",reply);fflush(stdout);return true;}
        pump();
    }
    fprintf(stderr,"last backend observation: %s; expected: %s\n",reply,expected);return false;
}
static bool update(int fd,const char *text) {char reply[32];return command(fd,text,reply,sizeof(reply)) && !strcmp(reply,"OK");}
static bool numeric_values(AtspiAccessible *control,double expected,bool enabled) {
    AtspiValue *value=atspi_accessible_get_value_iface(control);if(!value)return false;
    GError *error=NULL;
    bool ok=atspi_value_get_minimum_value(value,&error)==0 && !error &&
        atspi_value_get_maximum_value(value,&error)==100 && !error &&
        atspi_value_get_minimum_increment(value,&error)==1 && !error &&
        atspi_value_get_current_value(value,&error)==expected && !error;
    char *text=!error?atspi_value_get_text(value,&error):NULL;
    char wanted[16];(void)snprintf(wanted,sizeof(wanted),"%.0f",expected);
    ok=ok && !error && text && !strcmp(text,wanted) && state(control,ATSPI_STATE_EDITABLE)==enabled;
    if(error)g_error_free(error);g_free(text);g_object_unref(value);return ok;
}
static bool numeric_write(AtspiAccessible *control,double number) {
    AtspiValue *value=atspi_accessible_get_value_iface(control);if(!value)return false;
    GError *error=NULL;bool ok=atspi_value_set_current_value(value,number,&error) && !error;
    if(error)g_error_free(error);g_object_unref(value);return ok;
}
/* libatspi2.60.5 unrefs a NULL reply after a rejected Value.Set and aborts.
 * Retain its real successful setter; inspect expected errors on the same public
 * wire protocol independently, with the existing one-second call deadline. */
static bool numeric_rejected(double number,const char *expected) {
    DBusError error;dbus_error_init(&error);DBusConnection *bus=NULL;
    DBusMessage *call=NULL,*reply=NULL;bool ok=false;const char *address=getenv("AT_SPI_BUS_ADDRESS");
    if(!address || !numeric_target_bus || !numeric_target_path)goto done;
    bus=dbus_connection_open_private(address,&error);if(!bus)goto done;
    dbus_connection_set_exit_on_disconnect(bus,FALSE);if(!dbus_bus_register(bus,&error))goto done;
    call=dbus_message_new_method_call(numeric_target_bus,numeric_target_path,DBUS_INTERFACE_PROPERTIES,"Set");
    if(!call)goto done;
    const char *interface="org.a11y.atspi.Value",*property="CurrentValue";
    if(!dbus_message_append_args(call,DBUS_TYPE_STRING,&interface,DBUS_TYPE_STRING,&property,DBUS_TYPE_INVALID))goto done;
    DBusMessageIter args,v;dbus_message_iter_init_append(call,&args);
    if(!dbus_message_iter_open_container(&args,DBUS_TYPE_VARIANT,"d",&v) ||
       !dbus_message_iter_append_basic(&v,DBUS_TYPE_DOUBLE,&number) ||
       !dbus_message_iter_close_container(&args,&v))goto done;
    reply=dbus_connection_send_with_reply_and_block(bus,call,1000,&error);
    ok=!reply && dbus_error_has_name(&error,expected);
    if(ok)printf("RAW_VALUE_REJECTED %s %.17g\n",expected,number);
    else fprintf(stderr,"numeric error expected=%s actual=%s reply=%d\n",expected,error.name?error.name:"none",reply!=NULL);
done:
    if(reply)dbus_message_unref(reply);if(call)dbus_message_unref(call);
    if(bus){dbus_connection_close(bus);dbus_connection_unref(bus);}dbus_error_free(&error);return ok;
}
static DBusConnection *numeric_wire_open(AtspiAccessible *control) {
    DBusError error;dbus_error_init(&error);DBusConnection *bus=NULL;
    AtspiObject *object=ATSPI_OBJECT(control);const char *address=getenv("AT_SPI_BUS_ADDRESS");
    if(!address || !object->app)goto failed;
    bus=dbus_connection_open_private(address,&error);if(!bus)goto failed;
    dbus_connection_set_exit_on_disconnect(bus,FALSE);if(!dbus_bus_register(bus,&error))goto failed;
    char match[512];int length=snprintf(match,sizeof(match),"type='signal',sender='%s',interface='org.a11y.atspi.Event.Object',member='PropertyChange',path='%s',arg0='accessible-value'",object->app->bus_name,object->path);
    if(length<0 || (size_t)length>=sizeof(match))goto failed;
    dbus_bus_add_match(bus,match,&error);if(dbus_error_is_set(&error))goto failed;
    dbus_connection_flush(bus);dbus_error_free(&error);return bus;
failed:
    if(bus){dbus_connection_close(bus);dbus_connection_unref(bus);}dbus_error_free(&error);return NULL;
}
static bool numeric_wire_events(DBusConnection *bus,int *seen) {
    if(!dbus_connection_read_write(bus,0))return false;
    for(unsigned i=0;i<64;++i) {
        DBusMessage *message=dbus_connection_pop_message(bus);if(!message)break;
        bool ok=true;
        if(dbus_message_is_signal(message,"org.a11y.atspi.Event.Object","PropertyChange")) {
            ok=dbus_message_has_signature(message,"siiva{sv}");
            if(ok) {
                DBusMessageIter args,v;const char *detail;dbus_int32_t first,second;double value=0;
                dbus_message_iter_init(message,&args);dbus_message_iter_get_basic(&args,&detail);
                dbus_message_iter_next(&args);dbus_message_iter_get_basic(&args,&first);
                dbus_message_iter_next(&args);dbus_message_iter_get_basic(&args,&second);
                dbus_message_iter_next(&args);dbus_message_iter_recurse(&args,&v);
                ok=!strcmp(detail,"accessible-value") && first==0 && second==0 && dbus_message_iter_get_arg_type(&v)==DBUS_TYPE_DOUBLE;
                if(ok) {
                    dbus_message_iter_get_basic(&v,&value);ok=isfinite(value) && value>=0 && value<=100;
                    if(ok){printf("RAW_VALUE_EVENT %.17g\n",value);if(value==61)*seen=1;}
                }
            }
        }
        dbus_message_unref(message);if(!ok)return false;
    }
    return true;
}
int main(int argc,char **argv) {
    int result=1,initialized=0,pair[2]={-1,-1};pid_t child=-1;FILE *log=NULL;
    AtspiAccessible *desktop=NULL,*app=NULL,*volume=NULL;
    AtspiEventListener *listener=NULL;
    DBusConnection *numeric_wire=NULL;int wire_seen=0;
    bool numeric=getenv("NEXAUI_AUDIO_NUMERIC_VALUE")!=NULL;
    const char *font=getenv("NEXAUI_AETHERI_SETTINGS_FONT");
    CHECK(argc==2 && font && (log=tmpfile()) && !socketpair(AF_UNIX,SOCK_SEQPACKET,0,pair));
    child=fork();CHECK(child>=0);
    if(!child) {
        close(pair[0]);char descriptor[32];(void)snprintf(descriptor,sizeof(descriptor),"%d",pair[1]);
        if(dup2(fileno(log),STDOUT_FILENO)<0 || dup2(fileno(log),STDERR_FILENO)<0 ||
                setenv("NEXAUI_SETTINGS_AUDIO_CONTROL_FD",descriptor,1))_exit(126);
        execl(argv[1],argv[1],"--font",font,"--fixture-displays","--diagnostics",(char *)NULL);_exit(127);
    }
    close(pair[1]);pair[1]=-1;
    CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);
    desktop=atspi_get_desktop(0);CHECK(desktop);app=application(desktop,child);CHECK(app);
    CHECK(press(app,"Sound",ATSPI_ROLE_PUSH_BUTTON));
    for(unsigned i=0;i<300 && !volume;++i){volume=lookup(app,"Volume",ATSPI_ROLE_SLIDER);if(!volume)pump();}
    CHECK(volume && slider(volume,"60",true));
    int increment=action_index(volume,"increment");CHECK(increment>=0);
    if(numeric) {
        AtspiObject *target=ATSPI_OBJECT(volume);CHECK(target->app && target->path);
        /* Keep the actual old endpoint after libatspi disposes a Defunct proxy. */
        numeric_target_bus=g_strdup(target->app->bus_name);numeric_target_path=g_strdup(target->path);
        CHECK(numeric_values(volume,60,true));
        CHECK(numeric_rejected(-1,DBUS_ERROR_INVALID_ARGS) && numeric_rejected(101,DBUS_ERROR_INVALID_ARGS) &&
            numeric_rejected(NAN,DBUS_ERROR_INVALID_ARGS) && numeric_rejected(INFINITY,DBUS_ERROR_INVALID_ARGS));
        CHECK(observe(pair[0],"STATE 0 0 0 0.600000000 0 0"));
        GError *error=NULL;listener=atspi_event_listener_new(numeric_event,volume,NULL);CHECK(listener);
        CHECK(atspi_event_listener_register(listener,"object:property-change:accessible-value",&error) && !error);
        numeric_wire=numeric_wire_open(volume);CHECK(numeric_wire);
        CHECK(numeric_write(volume,61));
    } else CHECK(invoke(volume,increment));
    CHECK(slider(volume,"60",false) && observe(pair[0],"STATE 1 1 0 0.600000000 0 4"));
    CHECK(!invoke(volume,increment));
    if(numeric)CHECK(numeric_values(volume,60,false) && numeric_rejected(62,DBUS_ERROR_PROPERTY_READ_ONLY));
    CHECK(observe(pair[0],"STATE 1 1 0 0.600000000 0 4"));
    if(getenv("NEXAUI_AUDIO_OBSERVATION_FAILURE")) {
        CHECK(update(pair[0],"OBS_FAIL"));
        CHECK(label(app,"Audio information could not be refreshed."));
        CHECK(!state(volume,ATSPI_STATE_DEFUNCT) && slider(volume,"60",false));
        CHECK(!invoke(volume,increment));
        CHECK(disabled(app,"Mute output",ATSPI_ROLE_SWITCH));
        CHECK(disabled(app,"Headphones",ATSPI_ROLE_RADIO_BUTTON));
        if(numeric)CHECK(numeric_values(volume,60,false) && numeric_rejected(62,DBUS_ERROR_PROPERTY_READ_ONLY));
        CHECK(observe(pair[0],"STATE 1 1 0 0.600000000 0 4"));
        puts("OBSERVATION_FAILURE_PENDING_VERIFIED");
        if(!getenv("NEXAUI_AUDIO_CLOSE_PENDING")) {
            CHECK(update(pair[0],"OBS_OK") && label(app,"Applying change…"));
            CHECK(slider(volume,"60",false) && !invoke(volume,increment));
            if(numeric)CHECK(numeric_values(volume,60,false) && numeric_rejected(62,DBUS_ERROR_PROPERTY_READ_ONLY));
            CHECK(observe(pair[0],"STATE 1 1 0 0.600000000 0 4"));
            puts("OBSERVATION_RECOVERY_PENDING_VERIFIED");
        }
    }
    if(!getenv("NEXAUI_AUDIO_CLOSE_PENDING")) {
        CHECK(update(pair[0],"ACK") && slider(volume,"61",true));
        if(numeric) {
            CHECK(numeric_values(volume,61,true));
            for(unsigned i=0;i<300 && (!numeric_event_seen || !wire_seen) && !numeric_event_bad;++i) {
                pump();CHECK(numeric_wire_events(numeric_wire,&wire_seen));
            }
            CHECK(numeric_event_seen && wire_seen && !numeric_event_bad);
        }
        CHECK(press(app,"Mute output",ATSPI_ROLE_SWITCH));
        CHECK(observe(pair[0],"STATE 2 1 0 0.610000000 0 4"));
        CHECK(update(pair[0],"ACK"));
        CHECK(press(app,"Headphones",ATSPI_ROLE_RADIO_BUTTON));
        CHECK(observe(pair[0],"STATE 3 1 0 0.610000000 1 5"));
        CHECK(update(pair[0],"LOSS"));
        AtspiAccessible *lost=NULL;
        for(unsigned i=0;i<300 && !lost;++i){lost=lookup(app,"Audio connection lost. Changes are disabled.",ATSPI_ROLE_LABEL);if(!lost)pump();}
        CHECK(lost);g_object_unref(lost);
        CHECK(state(volume,ATSPI_STATE_DEFUNCT) && !invoke(volume,increment));
        if(numeric)CHECK(numeric_rejected(62,"org.a11y.atspi.Error.Defunct"));
        CHECK(observe(pair[0],"STATE 3 3 8 0.610000000 1 5"));
        CHECK(update(pair[0],"RECOVER"));
        CHECK(observe(pair[0],"STATE 3 3 0 0.610000000 1 5"));
        CHECK(!invoke(volume,increment));
        if(numeric)CHECK(numeric_rejected(62,"org.a11y.atspi.Error.Defunct"));
    }
    CHECK(press(app,"Close",ATSPI_ROLE_PUSH_BUTTON));
    int status=0;pid_t ended=0;
    for(unsigned i=0;i<300 && !ended;++i){ended=waitpid(child,&status,WNOHANG);if(!ended)pump();}
    CHECK(ended==child && WIFEXITED(status) && !WEXITSTATUS(status));child=-1;
    result=0;
done:
    if(child>0){kill(child,SIGKILL);waitpid(child,NULL,0);}
    if(log) {
        if(!fseek(log,0,SEEK_SET)) {
            char *transcript=malloc(128*1024+1);size_t total=0;
            if(!transcript)result=1;
            else {
                total=fread(transcript,1,128*1024,log);transcript[total]=0;
                if(total==128*1024 || ferror(log)){fputs("excessive/unreadable host log\n",stderr);result=1;}
                unsigned frames=0;const char *p=transcript;
                while((p=strstr(p,"FRAME "))!=NULL){++frames;++p;}
                if(!frames || frames>100){fputs("host frame bound failed\n",stderr);result=1;}
                const char *retired=getenv("NEXAUI_AUDIO_CLOSE_PENDING")?"CONTROLLED_RETIRED 1 0":"CONTROLLED_RETIRED 3 0";
                if(!strstr(transcript,retired)){fputs("original host did not retire cleanly\n",stderr);result=1;}
                const char *retained=getenv("NEXAUI_AUDIO_CLOSE_PENDING")?"CONTROLLED_OWNER_AFTER_WINDOW 1 1":"CONTROLLED_OWNER_AFTER_WINDOW 3 3";
                if(!strstr(transcript,retained)){fputs("owner uncertainty did not survive window return\n",stderr);result=1;}
                if(getenv("NEXAUI_AUDIO_OBSERVATION_FAILURE")) {
                    if(!strstr(transcript,"CONTROLLED_OBSERVE OBS_FAIL\n") ||
                       !strstr(transcript,"CONTROLLED_SNAPSHOT_UNAVAILABLE\n") ||
                       !strstr(transcript,"CONTROLLED_SNAPSHOT_READS ")) {
                        fputs("original host did not consume an unavailable snapshot\n",stderr);result=1;
                    }
                }
                if(!getenv("NEXAUI_AUDIO_CLOSE_PENDING")) {
                    const char *recover=strstr(transcript,"CONTROLLED_OBSERVE RECOVER\n");
                    const char *audio=recover?strstr(recover,"AUDIO state="):NULL;
                    const char *fence=audio?strstr(audio,"fenced=1"):NULL;
                    const char *endline=audio?strchr(audio,'\n'):NULL;
                    if(!fence || !endline || fence>endline){fputs("original model loss fence was not retained after recovery input\n",stderr);result=1;}
                }
                puts("CONTROLLED_HOST_LOG_BEGIN");(void)fwrite(transcript,1,total,stdout);
                puts("CONTROLLED_HOST_LOG_END");free(transcript);
            }
        }
        fclose(log);
    }
    if(pair[0]>=0)close(pair[0]);if(pair[1]>=0)close(pair[1]);
    if(listener) {
        GError *error=NULL;
        (void)atspi_event_listener_deregister(listener,"object:property-change:accessible-value",&error);
        if(error)g_error_free(error);g_object_unref(listener);
    }
    if(numeric_wire){dbus_connection_close(numeric_wire);dbus_connection_unref(numeric_wire);}
    g_free(numeric_target_bus);g_free(numeric_target_path);
    if(volume)g_object_unref(volume);if(app)g_object_unref(app);if(desktop)g_object_unref(desktop);
    if(initialized)atspi_exit();
    if(!result)puts("Original audio entry: native intent, once-only dispatch, observed acknowledgement/loss or pending-close passed");
    return result;
}
