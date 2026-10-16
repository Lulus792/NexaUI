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
    return response(line,sizeof(line)) && !strncmp(line,expected,strlen(expected));
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
    g_boxed_free(ATSPI_TYPE_EVENT,(void *)event);
}
typedef struct native_ref { char *bus,*path; } native_ref;
static native_ref copy_ref(AtspiAccessible *object) {
    AtspiObject *identity=ATSPI_OBJECT(object);
    return (native_ref){g_strdup(identity->app->bus_name),g_strdup(identity->path)};
}
static int raw_error(DBusConnection *connection,native_ref identity,const char *method,const char *expected,int index) {
    DBusMessage *call=dbus_message_new_method_call(identity.bus,identity.path,
        "org.a11y.atspi.Accessible",method);
    if (!call) return 0;
    if (index>=0) { dbus_int32_t arg=index; dbus_message_append_args(call,DBUS_TYPE_INT32,&arg,DBUS_TYPE_INVALID); }
    DBusError error; dbus_error_init(&error);
    DBusMessage *reply=dbus_connection_send_with_reply_and_block(connection,call,1000,&error);
    int result=!reply && error.name && !strcmp(error.name,expected);
    if (!result) fprintf(stderr,"raw %s: expected %s, got %s\n",method,expected,error.name?error.name:"success");
    if (reply) dbus_message_unref(reply);
    dbus_error_free(&error); dbus_message_unref(call); return result;
}
static int screen_geometry(DBusConnection *connection,AtspiAccessible *object,int wayland) {
    native_ref identity=copy_ref(object);
    DBusMessage *call=dbus_message_new_method_call(identity.bus,identity.path,"org.a11y.atspi.Component","GetExtents");
    dbus_uint32_t screen=ATSPI_COORD_TYPE_SCREEN;
    dbus_message_append_args(call,DBUS_TYPE_UINT32,&screen,DBUS_TYPE_INVALID);
    DBusError error; dbus_error_init(&error);
    DBusMessage *reply=dbus_connection_send_with_reply_and_block(connection,call,1000,&error);
    int good=wayland?!reply && error.name && !strcmp(error.name,DBUS_ERROR_NOT_SUPPORTED):
        reply && !error.name && dbus_message_has_signature(reply,"(iiii)");
    if (reply && !wayland) {
        DBusMessageIter it,rect; dbus_int32_t x,y;
        dbus_message_iter_init(reply,&it); dbus_message_iter_recurse(&it,&rect);
        dbus_message_iter_get_basic(&rect,&x); dbus_message_iter_next(&rect); dbus_message_iter_get_basic(&rect,&y);
        good=good && x==90 && y==110;
    }
    if (reply) dbus_message_unref(reply);
    dbus_message_unref(call); dbus_error_free(&error); g_free(identity.bus); g_free(identity.path); return good;
}
int main(int argc,char **argv) {
    CHECK(argc==2);
    int input[2],output[2]; CHECK(!pipe(input) && !pipe(output));
    host_pid=fork(); CHECK(host_pid>=0);
    if (!host_pid) {
        dup2(input[0],STDIN_FILENO); dup2(output[1],STDOUT_FILENO);
        close(input[0]); close(input[1]); close(output[0]); close(output[1]);
        execl(argv[1],argv[1],(char *)NULL); _exit(127);
    }
    close(input[0]); close(output[1]);
    commands=fdopen(input[1],"w"); responses=fdopen(output[0],"r");
    CHECK(commands && responses); setvbuf(responses,NULL,_IONBF,0);
    char line[256]; CHECK(response(line,sizeof(line)) && !strncmp(line,"READY ",6));
    CHECK(atspi_init()==0); atspi_set_timeout(1000,1000);
    AtspiAccessible *desktop=atspi_get_desktop(0),*app=NULL; CHECK(desktop);
    for (unsigned attempt=0;attempt<100u && !app;++attempt) {
        pump(10); GError *error=NULL;
        int count=atspi_accessible_get_child_count(desktop,&error); CHECK(!error);
        for (int i=0;i<count;++i) {
            AtspiAccessible *candidate=atspi_accessible_get_child_at_index(desktop,i,&error); CHECK(candidate && !error);
            guint pid=atspi_accessible_get_process_id(candidate,&error); CHECK(!error);
            if (pid==(guint)host_pid) app=candidate;
            else g_object_unref(candidate);
        }
    }
    CHECK(app && named(app,"NexaCloud AT-SPI"));
    GError *error=NULL;
    CHECK(atspi_accessible_get_role(app,&error)==ATSPI_ROLE_APPLICATION && !error);
    char *toolkit=atspi_accessible_get_toolkit_name(app,&error); CHECK(toolkit && !strcmp(toolkit,"NexaUI") && !error); g_free(toolkit);
    AtspiAccessible *button=find(app,"\xc3\x96" "ffnen \xf0\x9f\x93\x81",0),*sync=find(app,"Sync",0);
    AtspiAccessible *disabled=find(app,"Disabled",0),*secret=find(app,"Password",0),*counter=find(app,"Button count 0",0);
    AtspiAccessible *required=find(app,"Required",0),*optional=find(app,"Optional",0),*dynamic=find(app,"Dynamic",0);
    CHECK(button && sync && disabled && secret && counter && required && optional && dynamic);
    CHECK(atspi_accessible_get_role(button,&error)==ATSPI_ROLE_PUSH_BUTTON && !error);
    CHECK(atspi_accessible_get_role(secret,&error)==ATSPI_ROLE_PASSWORD_TEXT && !error);
    GHashTable *attrs=atspi_accessible_get_attributes(secret,&error); CHECK(attrs && !error && !g_hash_table_lookup(attrs,"nexaui-value")); g_hash_table_unref(attrs);
    CHECK(!state(disabled,ATSPI_STATE_ENABLED));
    AtspiComponent *component=atspi_accessible_get_component_iface(button); CHECK(component);
    AtspiRect *rect=atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,&error);
    CHECK(rect && !error && rect->x==10 && rect->y==20 && rect->width==100 && rect->height==30); g_free(rect);
    CHECK(atspi_component_contains(component,15,25,ATSPI_COORD_TYPE_WINDOW,&error) && !error);
    CHECK(!atspi_component_contains(component,115,25,ATSPI_COORD_TYPE_WINDOW,&error) && !error);
    AtspiComponent *root_component=NULL;
    AtspiAccessible *native=atspi_accessible_get_child_at_index(app,0,&error); CHECK(native && !error);
    root_component=atspi_accessible_get_component_iface(native); CHECK(root_component);
    AtspiAccessible *hit=atspi_component_get_accessible_at_point(root_component,15,25,ATSPI_COORD_TYPE_WINDOW,&error);
    CHECK(hit && !error && named(hit,"\xc3\x96" "ffnen \xf0\x9f\x93\x81")); g_object_unref(hit);
    AtspiEventListener *listener=atspi_event_listener_new(event,NULL,NULL); CHECK(listener);
    CHECK(atspi_event_listener_register(listener,"object:",&error) && !error);
    AtspiAction *action=atspi_accessible_get_action_iface(button),*toggle=atspi_accessible_get_action_iface(sync);
    AtspiAction *disabled_action=atspi_accessible_get_action_iface(disabled); CHECK(action && toggle && disabled_action);
    CHECK(atspi_action_get_n_actions(action,&error)==1 && !error);
    CHECK(!atspi_action_do_action(disabled_action,0,&error) && !error);
    CHECK(atspi_action_do_action(action,0,&error) && !error); pump(100);
    CHECK(named(counter,"Button count 1") && name_events>0);
    CHECK(atspi_action_do_action(toggle,0,&error) && !error); pump(100); CHECK(state(sync,ATSPI_STATE_CHECKED));
    AtspiSelection *req=atspi_accessible_get_selection_iface(required),*opt=atspi_accessible_get_selection_iface(optional); CHECK(req && opt);
    CHECK(atspi_selection_get_n_selected_children(req,&error)==1 && !error);
    CHECK(atspi_selection_get_n_selected_children(opt,&error)==0 && !error);
    CHECK(command("pause"));
    for (unsigned i=0;i<64u;++i) CHECK(atspi_action_do_action(action,0,&error) && !error);
    CHECK(!atspi_action_do_action(action,0,&error) && !error);
    CHECK(named(counter,"Button count 1"));
    CHECK(command("resume"));
    for (unsigned i=0;i<200u && !named(counter,"Button count 65");++i) pump(10);
    CHECK(named(counter,"Button count 65"));
    CHECK(command("wait")); pump(200);
    /* Its native display poll now waits two seconds. This uncached method must
       finish within the client's one-second RPC deadline via the bus fd wake. */
    atspi_accessible_clear_cache(button);
    CHECK(named(button,"\xc3\x96" "ffnen \xf0\x9f\x93\x81"));
    CHECK(command("normal"));
    CHECK(atspi_accessible_get_child_count(optional,&error)==3 && !error);
    AtspiAccessible *layout=atspi_accessible_get_child_at_index(optional,0,&error); CHECK(layout && !error && named(layout,"Layout"));
    CHECK(!atspi_selection_select_child(opt,0,&error) && !error); /* direct child 0 is a panel */
    CHECK(command("pause"));
    CHECK(atspi_selection_select_child(req,1,&error) && !error);
    CHECK(!atspi_selection_select_child(req,0,&error) && !error); /* pending group conflict */
    CHECK(atspi_selection_is_child_selected(req,0,&error) && !error); /* no speculative model mutation */
    CHECK(command("resume")); pump(100);
    CHECK(atspi_selection_is_child_selected(req,1,&error) && !error && selection_events>0);
    CHECK(!atspi_selection_clear_selection(req,&error) && !error);
    CHECK(atspi_selection_select_child(opt,2,&error) && !error); pump(100);
    AtspiAccessible *selected=atspi_selection_get_selected_child(opt,0,&error); CHECK(selected && !error && named(selected,"Second"));
    AtspiAccessible *parent=atspi_accessible_get_parent(selected,&error); CHECK(parent && !error && named(parent,"Optional"));
    AtspiComponent *selected_component=atspi_accessible_get_component_iface(selected);
    AtspiComponent *parent_component=atspi_accessible_get_component_iface(parent); CHECK(selected_component && parent_component);
    AtspiRect *in_window=atspi_component_get_extents(selected_component,ATSPI_COORD_TYPE_WINDOW,&error); CHECK(in_window && !error);
    AtspiRect *in_parent=atspi_component_get_extents(selected_component,ATSPI_COORD_TYPE_PARENT,&error); CHECK(in_parent && !error);
    AtspiRect *parent_window=atspi_component_get_extents(parent_component,ATSPI_COORD_TYPE_WINDOW,&error); CHECK(parent_window && !error);
    CHECK(in_window->x==in_parent->x+parent_window->x && in_window->y==in_parent->y+parent_window->y &&
        in_window->width==in_parent->width && in_window->height==in_parent->height);
    g_free(in_window); g_free(in_parent); g_free(parent_window); g_object_unref(selected_component); g_object_unref(parent_component);
    CHECK(atspi_selection_clear_selection(opt,&error) && !error); pump(100);
    CHECK(atspi_selection_get_n_selected_children(opt,&error)==0 && !error);
    DBusError dbus_error; dbus_error_init(&dbus_error);
    const char *address=getenv("NEXAUI_TEST_ATSPI_ADDRESS");
    if (!address) address=getenv("AT_SPI_BUS_ADDRESS");
    DBusConnection *raw=dbus_connection_open_private(address,&dbus_error); CHECK(raw && !dbus_error.name);
    dbus_connection_set_exit_on_disconnect(raw,0); CHECK(dbus_bus_register(raw,&dbus_error));
    int wayland=getenv("NEXAUI_PLATFORM") && !strcmp(getenv("NEXAUI_PLATFORM"),"wayland");
    if (!wayland) { CHECK(command("move")); pump(100); }
    CHECK(screen_geometry(raw,button,wayland));
    gboolean focused=atspi_component_grab_focus(component,&error); CHECK(!error); pump(100);
    CHECK(wayland?(!focused || state(button,ATSPI_STATE_FOCUSED)):(focused && state(button,ATSPI_STATE_FOCUSED)));
    native_ref button_ref=copy_ref(button), dynamic_ref=copy_ref(dynamic);
    CHECK(raw_error(raw,button_ref,"GetChildAtIndex",DBUS_ERROR_INVALID_ARGS,99));
    CHECK(command("other")); pump(100);
    CHECK(atspi_accessible_get_child_count(app,&error)==2 && !error);
    AtspiAccessible *peer_window=atspi_accessible_get_child_at_index(app,0,&error); CHECK(peer_window && !error);
    AtspiAccessible *peer_button=find(peer_window,"\xc3\x96" "ffnen \xf0\x9f\x93\x81",0),*peer_counter=find(peer_window,"Button count 0",0);
    CHECK(peer_button && peer_counter && strcmp(button_ref.path,ATSPI_OBJECT(peer_button)->path));
    native_ref peer_ref=copy_ref(peer_button);
    AtspiAction *peer_action=atspi_accessible_get_action_iface(peer_button); CHECK(peer_action);
    CHECK(atspi_action_do_action(peer_action,0,&error) && !error); pump(100);
    CHECK(named(peer_counter,"Button count 1") && named(counter,"Button count 65"));
    CHECK(command("closeother")); pump(100);
    CHECK(atspi_accessible_get_child_count(app,&error)==1 && !error);
    CHECK(raw_error(raw,peer_ref,"GetRole","org.a11y.atspi.Error.Defunct",-1));
    g_object_unref(peer_action); g_object_unref(peer_window); g_object_unref(peer_button); g_object_unref(peer_counter);
    g_free(peer_ref.bus); g_free(peer_ref.path);
    char *old_path=g_strdup(ATSPI_OBJECT(dynamic)->path);
    CHECK(command("remove")); pump(100);
    CHECK(raw_error(raw,dynamic_ref,"GetRole","org.a11y.atspi.Error.Defunct",-1));
    CHECK(command("restore")); pump(100);
    AtspiAccessible *replacement=find(app,"Dynamic",0); CHECK(replacement && strcmp(old_path,ATSPI_OBJECT(replacement)->path));
    native_ref replacement_ref=copy_ref(replacement);
    CHECK(command("retype")); pump(100);
    CHECK(raw_error(raw,replacement_ref,"GetRole","org.a11y.atspi.Error.Defunct",-1));
    CHECK(command("rename")); pump(100); CHECK(named(button,"Changed"));
    CHECK(command("failure")); pump(100);
    atspi_accessible_clear_cache(button); /* Require a real round trip for rollback, not its old cached name. */
    CHECK(named(button,"Changed"));
    CHECK(command("republish")); pump(100);
    char *long_name=atspi_accessible_get_name(button,&error); CHECK(long_name && !error && strlen(long_name)==32767u && long_name[0]=='Z');
    CHECK(command("resume")); CHECK(command("replace")); pump(100);
    CHECK(raw_error(raw,button_ref,"GetRole","org.a11y.atspi.Error.Defunct",-1));
    CHECK(state_events>0 && children_events>0);
    CHECK(command("disconnect"));
    AtspiAccessible *reconnected=NULL;
    for (unsigned attempt=0;attempt<300u && !reconnected;++attempt) {
        pump(10);
        int count=atspi_accessible_get_child_count(desktop,&error); CHECK(!error);
        for (int i=0;i<count;++i) {
            AtspiAccessible *candidate=atspi_accessible_get_child_at_index(desktop,i,&error);
            CHECK(!error);
            if (!candidate) continue;
            AtspiObject *identity=ATSPI_OBJECT(candidate);
            guint pid=atspi_accessible_get_process_id(candidate,&error);
            if (!error && pid==(guint)host_pid && identity->app && strcmp(identity->app->bus_name,button_ref.bus))
                reconnected=candidate;
            else g_object_unref(candidate);
            if (error) { g_error_free(error); error=NULL; }
        }
    }
    CHECK(reconnected);
    AtspiAccessible *new_button=find(reconnected,long_name,0),*new_counter=find(reconnected,"Button count 65",0);
    CHECK(new_button && new_counter && strcmp(button_ref.path,ATSPI_OBJECT(new_button)->path));
    AtspiAction *new_action=atspi_accessible_get_action_iface(new_button); CHECK(new_action);
    CHECK(atspi_action_do_action(new_action,0,&error) && !error); pump(100);
    CHECK(named(new_counter,"Button count 66") && strlen(long_name)==32767u && long_name[0]=='Z');
    CHECK(raw_error(raw,button_ref,"GetRole",DBUS_ERROR_SERVICE_UNKNOWN,-1));
    g_object_unref(new_action); g_object_unref(new_button); g_object_unref(new_counter); g_object_unref(reconnected);
    g_free(long_name);
    CHECK(atspi_event_listener_deregister(listener,"object:",&error) && !error);
    CHECK(command("quit"));
    fclose(commands); commands=NULL; fclose(responses); responses=NULL;
    int status; CHECK(waitpid(host_pid,&status,0)==host_pid && WIFEXITED(status) && WEXITSTATUS(status)==0); host_pid=-1;
    printf("AT-SPI registry/client: native queries, committed actions, selection/cache/events and retired references passed\n");
    g_free(old_path);
    g_free(button_ref.bus); g_free(button_ref.path);
    g_free(dynamic_ref.bus); g_free(dynamic_ref.path);
    g_free(replacement_ref.bus); g_free(replacement_ref.path);
    g_object_unref(replacement); g_object_unref(selected); g_object_unref(parent); g_object_unref(layout);
    g_object_unref(req); g_object_unref(opt); g_object_unref(action); g_object_unref(toggle); g_object_unref(disabled_action);
    g_object_unref(component); g_object_unref(root_component); g_object_unref(native); g_object_unref(listener);
    g_object_unref(button); g_object_unref(sync); g_object_unref(disabled); g_object_unref(secret); g_object_unref(counter);
    g_object_unref(required); g_object_unref(optional); g_object_unref(dynamic); g_object_unref(app); g_object_unref(desktop);
    dbus_connection_close(raw); dbus_connection_unref(raw); dbus_error_free(&dbus_error);
    atspi_exit(); return 0;
}
