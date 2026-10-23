#define _POSIX_C_SOURCE 200809L
/* Deliberately not linked to NexaUI: this process uses the OS's libatspi and
   D-Bus client API to discover a separate native application through registryd. */
#include <atspi/atspi.h>
#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

static pid_t host_pid=-1;
static FILE *commands,*responses;
static unsigned name_events,selection_events,state_events,children_events;
static unsigned focus_events;
static char *last_focus_path;
static void cleanup(void) {
    if (commands) { fclose(commands); commands=NULL; }
    if (responses) { fclose(responses); responses=NULL; }
    if (host_pid>0) { kill(host_pid,SIGTERM); waitpid(host_pid,NULL,0); host_pid=-1; }
}
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"client:%d: %s\n",__LINE__,#x); cleanup(); return 1; } } while (0)
static void pump(unsigned milliseconds) {
    for (unsigned i=0;i<milliseconds;++i) {
        while (g_main_context_iteration(NULL,FALSE)) {}
        struct timespec delay={0,1000000}; nanosleep(&delay,NULL);
    }
}
static int response(char *buffer,size_t capacity) {
    struct pollfd fd={fileno(responses),POLLIN,0};
    return poll(&fd,1,3000)>0 && fgets(buffer,(int)capacity,responses)!=NULL;
}
static int command(const char *value) {
    fprintf(commands,"%s\n",value); fflush(commands);
    char line[256],expected[256]; snprintf(expected,sizeof(expected),"ACK %s ",value);
    if(!response(line,sizeof(line))){fprintf(stderr,"command %s: missing acknowledgement\n",value);return 0;}
    if(strncmp(line,expected,strlen(expected))){fprintf(stderr,"command %s: got %s",value,line);return 0;}
    return 1;
}
static int named(AtspiAccessible *object,const char *name) {
    GError *error=NULL; char *value=atspi_accessible_get_name(object,&error);
    int same=!error && value && !strcmp(value,name);
    if (error) { fprintf(stderr,"get_name: %s\n",error->message); g_error_free(error); }
    g_free(value); return same;
}
static AtspiAccessible *find(AtspiAccessible *root,const char *name,unsigned depth) {
    if (named(root,name)) return g_object_ref(root);
    if (depth>16) return NULL;
    GError *error=NULL; int count=atspi_accessible_get_child_count(root,&error);
    if (error) { g_error_free(error); return NULL; }
    for (int i=0;i<count;++i) {
        AtspiAccessible *child=atspi_accessible_get_child_at_index(root,i,&error);
        if (error) { g_error_free(error); error=NULL; continue; }
        AtspiAccessible *result=child?find(child,name,depth+1u):NULL;
        if (child) g_object_unref(child);
        if (result) return result;
    }
    return NULL;
}
static int state(AtspiAccessible *object,AtspiStateType wanted) {
    AtspiStateSet *set=atspi_accessible_get_state_set(object);
    int result=set && atspi_state_set_contains(set,wanted);
    if (set) g_object_unref(set);
    return result;
}
static void event(AtspiEvent *event,void *user) {
    (void)user;
    if (strstr(event->type,"property-change:accessible-name")) ++name_events;
    if (strstr(event->type,"selection-changed")) ++selection_events;
    if (strstr(event->type,"state-changed")) ++state_events;
    if (strstr(event->type,"children-changed")) ++children_events;
    if (!strcmp(event->type,"object:state-changed:focused") && event->detail1 && event->source) {
        ++focus_events; g_free(last_focus_path);
        last_focus_path=g_strdup(ATSPI_OBJECT(event->source)->path);
    }
    g_boxed_free(ATSPI_TYPE_EVENT,(void *)event);
}
typedef struct native_ref { char *bus,*path; } native_ref;
static native_ref copy_ref(AtspiAccessible *object) {
    AtspiObject *identity=ATSPI_OBJECT(object);
    return (native_ref){g_strdup(identity->app->bus_name),g_strdup(identity->path)};
}
static int wait_count(AtspiSelection *selection,int wanted) {
    for(unsigned i=0;i<250;++i){GError *error=NULL;int count=atspi_selection_get_n_selected_children(selection,&error);if(error){g_error_free(error);return 0;}if(count==wanted)return 1;pump(10);}return 0;
}
static int model_count(size_t expected) {
    fprintf(commands,"verify\n");fflush(commands);char line[256];size_t actual=0;unsigned long long active=0;
    if(!response(line,sizeof(line)) || sscanf(line,"ACK verify %zu %llu",&actual,&active)!=2)return 0;
    if(actual!=expected || active!=1066){fprintf(stderr,"model actual=%zu active=%llu expected=%zu active=1066\n",actual,active,expected);return 0;}
    return 1;
}
static int null_selected(DBusConnection *connection,native_ref identity,int index) {
    DBusMessage *call=dbus_message_new_method_call(identity.bus,identity.path,"org.a11y.atspi.Selection","GetSelectedChild");if(!call)return 0;
    dbus_int32_t arg=index;dbus_message_append_args(call,DBUS_TYPE_INT32,&arg,DBUS_TYPE_INVALID);
    DBusError error;dbus_error_init(&error);DBusMessage *reply=dbus_connection_send_with_reply_and_block(connection,call,1000,&error);int good=0;
    if(reply && !error.name && dbus_message_has_signature(reply,"(so)")) {
        DBusMessageIter it,ref;const char *path=NULL;dbus_message_iter_init(reply,&it);dbus_message_iter_recurse(&it,&ref);dbus_message_iter_next(&ref);dbus_message_iter_get_basic(&ref,&path);
        good=path && !strcmp(path,"/org/a11y/atspi/null");
    }
    if(reply)dbus_message_unref(reply);dbus_message_unref(call);dbus_error_free(&error);return good;
}
static AtspiAccessible *application(AtspiAccessible *desktop) {
    for(unsigned attempt=0;attempt<200;++attempt) {
        pump(10);atspi_accessible_clear_cache(desktop);GError *error=NULL;
        int count=atspi_accessible_get_child_count(desktop,&error);if(error){g_error_free(error);continue;}
        for(int i=0;i<count;++i) {
            AtspiAccessible *candidate=atspi_accessible_get_child_at_index(desktop,i,&error);
            if(error){g_error_free(error);error=NULL;continue;}
            if(!candidate)continue;
            atspi_accessible_clear_cache(candidate);guint pid=atspi_accessible_get_process_id(candidate,&error);
            if(!error && pid==(guint)host_pid && atspi_accessible_get_child_count(candidate,&error)>0 && !error)return candidate;
            if(error){g_error_free(error);error=NULL;}g_object_unref(candidate);
        }
    }
    return NULL;
}
static AtspiSelection *current_selection(AtspiAccessible *desktop,AtspiAccessible **owner_out) {
    AtspiAccessible *app=application(desktop);if(!app)return NULL;
    AtspiAccessible *owner=find(app,"Multi",0);g_object_unref(app);if(!owner)return NULL;
    AtspiSelection *selection=atspi_accessible_get_selection_iface(owner);
    if(!selection){g_object_unref(owner);return NULL;}
    *owner_out=owner;return selection;
}
static int retired(DBusConnection *connection,native_ref identity) {
    DBusMessage *call=dbus_message_new_method_call(identity.bus,identity.path,"org.a11y.atspi.Accessible","GetRole");if(!call)return 0;
    DBusError error;dbus_error_init(&error);DBusMessage *reply=dbus_connection_send_with_reply_and_block(connection,call,1000,&error);
    int yes=!reply && error.name && (!strcmp(error.name,"org.a11y.atspi.Error.Defunct") || !strcmp(error.name,DBUS_ERROR_SERVICE_UNKNOWN) || !strcmp(error.name,DBUS_ERROR_NAME_HAS_NO_OWNER));
    if(!yes)fprintf(stderr,"expected retired reference %s, got %s\n",identity.path,error.name?error.name:"success");
    if(reply)dbus_message_unref(reply);dbus_message_unref(call);dbus_error_free(&error);return yes;
}
static int stale_matrix(AtspiAccessible *desktop,DBusConnection *raw) {
    const char *changes[]={"ancestor","member-role","owner-role","detach","context","revision","target","disabled-cycle","demote","disconnect"};
    for(int whole=0;whole<2;++whole)for(int forwarded=0;forwarded<2;++forwarded)for(unsigned kind=0;kind<sizeof(changes)/sizeof(changes[0]);++kind) {
        if(kind==9)fprintf(stderr,"disconnect case whole=%d forwarded=%d\n",whole,forwarded);
        CHECK(command("pause") && command("reset"));AtspiAccessible *owner=NULL;AtspiSelection *selection=current_selection(desktop,&owner);CHECK(selection);
        GError *error=NULL;AtspiAccessible *item=atspi_accessible_get_child_at_index(owner,2,&error);CHECK(item && !error);
        native_ref old_item=copy_ref(item);
        CHECK((whole?atspi_selection_select_all(selection,&error):atspi_selection_select_child(selection,2,&error)) && !error && model_count(0));
        if(forwarded)CHECK(command("route") && command("expect-forward") && model_count(0));
        CHECK(command(changes[kind]) && command("route") && command("step") && model_count(0) && command("expect-single"));
        if(kind!=5 && kind!=7)CHECK(retired(raw,old_item));
        CHECK(command("reset") && command("step") && model_count(0));
        g_free(old_item.bus);g_free(old_item.path);g_object_unref(item);g_object_unref(selection);g_object_unref(owner);
    }
    puts("AT-SPI native-pending and forwarded item/bulk: ten stale transitions including reconnect, actual unchanged models passed");return 0;
}
static int policies_and_oom(AtspiAccessible *desktop) {
    CHECK(command("pause") && command("reset") && command("required"));
    AtspiAccessible *owner=NULL;AtspiSelection *selection=current_selection(desktop,&owner);CHECK(selection);GError *error=NULL;
    CHECK(wait_count(selection,1) && !atspi_selection_clear_selection(selection,&error) && !error && model_count(1));
    g_object_unref(selection);g_object_unref(owner);
    CHECK(command("reset") && command("disabled"));selection=current_selection(desktop,&owner);CHECK(selection);
    CHECK(!atspi_selection_select_all(selection,&error) && !error && model_count(0));
    CHECK(!atspi_selection_select_child(selection,3,&error) && !error);
    g_object_unref(selection);g_object_unref(owner);
    CHECK(command("reset") && command("disabled-selected"));selection=current_selection(desktop,&owner);CHECK(selection);
    CHECK(wait_count(selection,1) && !atspi_selection_clear_selection(selection,&error) && !error && model_count(1));
    CHECK(atspi_selection_select_all(selection,&error) && !error && command("route") && command("step") && wait_count(selection,67) && model_count(67));
    g_object_unref(selection);g_object_unref(owner);
    CHECK(command("reset"));selection=current_selection(desktop,&owner);CHECK(selection);
    CHECK(atspi_selection_select_all(selection,&error) && !error && command("oom") && command("route") && command("expect-memory") && model_count(0));
    CHECK(command("route") && command("expect-forward") && model_count(0) && command("step") && wait_count(selection,67) && model_count(67));
    g_object_unref(selection);g_object_unref(owner);
    CHECK(command("reset"));AtspiAccessible *app=application(desktop);CHECK(app);AtspiAccessible *table=find(app,"Table",0);CHECK(table);g_object_unref(app);
    selection=atspi_accessible_get_selection_iface(table);CHECK(selection && atspi_accessible_get_child_count(table,&error)==4 && !error);
    CHECK(!atspi_selection_select_child(selection,3,&error) && !error);
    CHECK(atspi_selection_select_all(selection,&error) && !error && command("route") && command("step") && wait_count(selection,3));
    AtspiAccessible *custom=atspi_selection_get_selected_child(selection,2,&error);CHECK(custom && !error && named(custom,"Table 2") && command("verify-table"));g_object_unref(custom);
    g_object_unref(selection);g_object_unref(table);
    puts("AT-SPI required/disabled/bulk OOM retry/table custom and nonmember sorting header passed");return 0;
}
static int callbacks(AtspiAccessible *desktop) {
    const char *core_hooks[]={"hook-detach","hook-destroy","hook-ancestor"};
    for(unsigned i=0;i<3;++i) {
        CHECK(command("pause") && command("reset"));AtspiAccessible *owner=NULL;AtspiSelection *selection=current_selection(desktop,&owner);CHECK(selection);GError *error=NULL;
        CHECK(atspi_selection_select_all(selection,&error) && !error && command(core_hooks[i]) && command("route") && command("check-hook") && model_count(0));
        CHECK(command("reset") && command("step") && model_count(0));g_object_unref(selection);g_object_unref(owner);
    }
    const char *native_hooks[]={"native-detach","native-destroy","native-republish","free-detach","free-destroy","free-republish","bridge-detach","bridge-destroy","bridge-republish"};
    for(unsigned i=0;i<9;++i) {
        CHECK(command("pause") && command("reset") && command("fresh"));
        if(i>=6)CHECK(command("drop"));
        CHECK(command(native_hooks[i]) && command("grow") && command("check-hook") && model_count(0));
        CHECK(command("reset") && command("step") && model_count(0));
    }
    puts("AT-SPI real core/native allocator/deallocator detach/destroy/republication lifetime and retry passed");return 0;
}
static int inflight_slots(AtspiAccessible *desktop) {
    CHECK(command("pause") && command("reset"));AtspiAccessible *app=application(desktop);CHECK(app);
    AtspiAction *actions[64];GError *error=NULL;
    for(unsigned i=0;i<64;++i){char name[32];snprintf(name,sizeof(name),"Queued %u",i);AtspiAccessible *item=find(app,name,0);CHECK(item);actions[i]=atspi_accessible_get_action_iface(item);CHECK(actions[i]);g_object_unref(item);}
    g_object_unref(app);AtspiAccessible *owner=NULL;AtspiSelection *selection=current_selection(desktop,&owner);CHECK(selection);
    CHECK(atspi_selection_select_all(selection,&error) && !error && command("hook-slots"));
    fprintf(commands,"route\n");fflush(commands);char line[256];CHECK(response(line,sizeof(line)) && !strcmp(line,"HOOK slots\n"));
    for(unsigned i=0;i<63;++i)CHECK(atspi_action_do_action(actions[i],0,&error) && !error);
    CHECK(!atspi_action_do_action(actions[63],0,&error) && !error);
    fprintf(commands,"finish\n");fflush(commands);CHECK(response(line,sizeof(line)) && !strncmp(line,"ACK route ",10));
    CHECK(command("check-hook") && model_count(0) && command("step") && model_count(67));
    CHECK(command("route") && command("step") && command("check-slots") && model_count(67));
    for(unsigned i=0;i<64;++i)g_object_unref(actions[i]);g_object_unref(selection);g_object_unref(owner);
    puts("AT-SPI true in-flight64-slot cap:63 OS button requests accepted,64th rejected,63 actual clicks delivered");return 0;
}
static int idempotence(AtspiAccessible *desktop) {
    CHECK(command("pause") && command("reset"));AtspiAccessible *owner=NULL;AtspiSelection *selection=current_selection(desktop,&owner);CHECK(selection);
    GError *error=NULL;pump(20);unsigned before=selection_events;
    CHECK(atspi_selection_select_all(selection,&error) && !error && atspi_selection_select_all(selection,&error) && !error);
    CHECK(!atspi_selection_clear_selection(selection,&error) && !error && !atspi_selection_select_child(selection,2,&error) && !error);
    CHECK(wait_count(selection,0) && model_count(0) && command("route") && model_count(0) && command("step") && wait_count(selection,67) && model_count(67));
    pump(20);CHECK(selection_events==before+1);before=selection_events;
    CHECK(atspi_selection_select_all(selection,&error) && !error && command("route") && command("step") && model_count(67));
    pump(20);CHECK(selection_events==before);
    g_object_unref(selection);g_object_unref(owner);puts("AT-SPI equivalent native whole-set idempotence/owner serialization and no duplicate event passed");return 0;
}
int main(int argc,char **argv) {
    CHECK(argc==2);int input[2],output[2];CHECK(!pipe(input) && !pipe(output));host_pid=fork();CHECK(host_pid>=0);
    if(!host_pid) {
        dup2(input[0],STDIN_FILENO);dup2(output[1],STDOUT_FILENO);close(input[0]);close(input[1]);close(output[0]);close(output[1]);execl(argv[1],argv[1],(char *)NULL);_exit(127);
    }
    close(input[0]);close(output[1]);commands=fdopen(input[1],"w");responses=fdopen(output[0],"r");CHECK(commands && responses);setvbuf(responses,NULL,_IONBF,0);
    char line[256];CHECK(response(line,sizeof(line)) && !strncmp(line,"READY ",6));
    CHECK(atspi_init()==0);atspi_set_timeout(1000,1000);AtspiAccessible *desktop=atspi_get_desktop(0),*app=NULL;CHECK(desktop);
    GError *error=NULL;
    for(unsigned attempt=0;attempt<200 && !app;++attempt) {
        pump(10);int count=atspi_accessible_get_child_count(desktop,&error);CHECK(!error);
        for(int i=0;i<count;++i) {
            AtspiAccessible *candidate=atspi_accessible_get_child_at_index(desktop,i,&error);CHECK(candidate && !error);
            guint pid=atspi_accessible_get_process_id(candidate,&error);CHECK(!error);
            if(pid==(guint)host_pid)app=candidate;else g_object_unref(candidate);
        }
    }
    CHECK(app);AtspiAccessible *owner=find(app,"Multi",0);CHECK(owner);
    AtspiSelection *selection=atspi_accessible_get_selection_iface(owner);
    CHECK(selection && "bound MULTIPLE owner must expose AT-SPI Selection");
    CHECK(state(owner,ATSPI_STATE_MULTISELECTABLE));
    CHECK(atspi_accessible_get_child_count(owner,&error)==68 && !error);
    AtspiAccessible *layout=atspi_accessible_get_child_at_index(owner,0,&error);CHECK(layout && !error && named(layout,"Layout"));
    CHECK(wait_count(selection,0) && !atspi_selection_select_child(selection,0,&error) && !error);
    AtspiEventListener *listener=atspi_event_listener_new(event,NULL,NULL);CHECK(listener && atspi_event_listener_register(listener,"object:",&error) && !error);
    CHECK(atspi_selection_select_child(selection,2,&error) && !error && wait_count(selection,1));
    CHECK(atspi_selection_select_child(selection,4,&error) && !error && wait_count(selection,2));
    AtspiAccessible *chosen=atspi_selection_get_selected_child(selection,1,&error);CHECK(chosen && !error && named(chosen,"Item 3"));g_object_unref(chosen);
    CHECK(atspi_selection_deselect_selected_child(selection,0,&error) && !error && wait_count(selection,1));
    chosen=atspi_selection_get_selected_child(selection,0,&error);CHECK(chosen && !error && named(chosen,"Item 3"));g_object_unref(chosen);
    CHECK(atspi_selection_deselect_child(selection,4,&error) && !error && wait_count(selection,0) && model_count(0));
    CHECK(atspi_selection_select_all(selection,&error) && !error && wait_count(selection,67) && model_count(67));
    chosen=atspi_selection_get_selected_child(selection,66,&error);CHECK(chosen && !error && named(chosen,"Item 66") && state(chosen,ATSPI_STATE_SELECTED));g_object_unref(chosen);
    const char *address=getenv("NEXAUI_TEST_ATSPI_ADDRESS");if(!address)address=getenv("AT_SPI_BUS_ADDRESS");DBusError dbus_error;dbus_error_init(&dbus_error);
    DBusConnection *raw=dbus_connection_open_private(address,&dbus_error);CHECK(raw && !dbus_error.name);dbus_connection_set_exit_on_disconnect(raw,0);CHECK(dbus_bus_register(raw,&dbus_error));
    native_ref identity=copy_ref(owner);CHECK(null_selected(raw,identity,67) && null_selected(raw,identity,-1));
    CHECK(atspi_selection_clear_selection(selection,&error) && !error && wait_count(selection,0) && model_count(0));
    pump(50);CHECK(selection_events>=6 && state_events>=67);
    CHECK(stale_matrix(desktop,raw)==0 && policies_and_oom(desktop)==0 && callbacks(desktop)==0 && inflight_slots(desktop)==0 && idempotence(desktop)==0);
    CHECK(command("quit"));fclose(commands);commands=NULL;fclose(responses);responses=NULL;int status=0;CHECK(waitpid(host_pid,&status,0)==host_pid && WIFEXITED(status) && WEXITSTATUS(status)==0);host_pid=-1;
    printf("AT-SPI multi: complete67, direct/selected ordinal asymmetry, raw null references, model acknowledgement and events passed\n");
    dbus_connection_close(raw);dbus_connection_unref(raw);dbus_error_free(&dbus_error);g_free(identity.bus);g_free(identity.path);
    atspi_event_listener_deregister(listener,"object:",&error);CHECK(!error);g_object_unref(listener);g_object_unref(layout);g_object_unref(selection);g_object_unref(owner);g_object_unref(app);g_object_unref(desktop);atspi_exit();return 0;
}
