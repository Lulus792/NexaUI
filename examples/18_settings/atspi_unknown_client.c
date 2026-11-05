#define _POSIX_C_SOURCE 200809L
/* Independent native consumer: no NexaUI headers or linkage. */
#include <atspi/atspi.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static pid_t host=-1;
static void cleanup(void) { if(host>0){kill(host,SIGTERM);waitpid(host,NULL,0);host=-1;} }
#define CHECK(x) do { if(!(x)){fprintf(stderr,"settings-unknown:%d: %s\n",__LINE__,#x);cleanup();return 1;} } while(0)
static void pump(unsigned ms) {
    for(unsigned i=0;i<ms/10;++i) {
        while(g_main_context_iteration(NULL,FALSE)){}
        struct timespec wait={0,10000000};nanosleep(&wait,NULL);
    }
}
static AtspiAccessible *find(AtspiAccessible *node,const char *wanted,unsigned depth) {
    if(!node || depth>16)return NULL;
    GError *error=NULL;char *name=atspi_accessible_get_name(node,&error);
    int match=!error && name && !strcmp(name,wanted);
    if(error)g_error_free(error);g_free(name);
    if(match)return g_object_ref(node);
    error=NULL;int count=atspi_accessible_get_child_count(node,&error);
    if(error){g_error_free(error);return NULL;}
    for(int i=0;i<count;++i) {
        error=NULL;AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,&error);
        if(error){g_error_free(error);continue;}
        AtspiAccessible *found=find(child,wanted,depth+1);
        if(child)g_object_unref(child);if(found)return found;
    }
    return NULL;
}
static int status_is(AtspiAccessible *node,const char *value) {
    GError *error=NULL;atspi_accessible_clear_cache(node);
    GHashTable *attrs=atspi_accessible_get_attributes(node,&error);
    int match=!error && attrs && !g_strcmp0(g_hash_table_lookup(attrs,"nexaui-value"),value);
    if(attrs)g_hash_table_unref(attrs);if(error)g_error_free(error);return match;
}
static int enabled(AtspiAccessible *node) {
    atspi_accessible_clear_cache(node);AtspiStateSet *set=atspi_accessible_get_state_set(node);
    int result=set && atspi_state_set_contains(set,ATSPI_STATE_ENABLED);
    if(set)g_object_unref(set);return result;
}
static int marker(int fd,const char *expected) {
    char line[128];size_t used=0;struct pollfd wait={fd,POLLIN|POLLHUP,0};
    while(used+1<sizeof(line) && poll(&wait,1,5000)>0) {
        ssize_t n=read(fd,line+used,1);if(n!=1)return 0;
        if(line[used++]=='\n'){line[used-1]=0;return !strcmp(line,expected);}
    }
    return 0;
}
int main(int argc,char **argv) {
    const char *font=getenv("NEXAUI_SETTINGS_FONT");CHECK(argc==2 && font);
    int reopen=getenv("NEXAUI_SETTINGS_REOPEN_TEST")!=NULL;int markers[2]={-1,-1};
    if(reopen)CHECK(pipe(markers)==0);
    host=fork();CHECK(host>=0);
    if(!host){if(reopen){dup2(markers[1],STDOUT_FILENO);close(markers[0]);close(markers[1]);}
        execl(argv[1],argv[1],font,reopen?"--verify-reopen":"--simulate-unknown",(char *)NULL);_exit(127);}
    if(reopen)close(markers[1]);
    CHECK(atspi_init()==0);AtspiAccessible *desktop=atspi_get_desktop(0),*root=NULL;
    CHECK(desktop);
    for(unsigned i=0;i<500 && !root;++i){pump(10);atspi_accessible_clear_cache(desktop);root=find(desktop,"Settings example",0);}
    CHECK(root);
    AtspiAccessible *apply=find(root,"Apply settings",0),*status=find(root,"Settings status",0),*name=find(root,"Display name",0);
    CHECK(apply && status && name && enabled(apply));
    GError *error=NULL;AtspiAction *action=atspi_accessible_get_action_iface(apply);CHECK(action);
    if(reopen) {
        AtspiEditableText *editor=atspi_accessible_get_editable_text_iface(name);CHECK(editor);
        CHECK(atspi_editable_text_set_text_contents(editor,"Reopened draft",&error) && !error);
        g_object_unref(editor);
        AtspiAccessible *motion=find(root,"Reduce motion",0);CHECK(motion);
        AtspiComponent *component=atspi_accessible_get_component_iface(motion);CHECK(component);
        CHECK(atspi_component_grab_focus(component,&error) && !error);
        g_object_unref(component);g_object_unref(motion);
    }
    CHECK(atspi_action_do_action(action,0,&error) && !error);
    if(reopen) {
        CHECK(marker(markers[0],"SETTINGS_VIEW_DETACHED") && marker(markers[0],"SETTINGS_VIEW_REOPENED"));
        close(markers[0]);g_object_unref(apply);g_object_unref(status);g_object_unref(name);g_object_unref(root);
        root=NULL;status=NULL;name=NULL;apply=NULL;
        for(unsigned i=0;i<300 && !status;++i) {
            pump(10);atspi_accessible_clear_cache(desktop);
            root=find(desktop,"Settings example",0);
            if(root) {status=find(root,"Settings status",0);if(!status){g_object_unref(root);root=NULL;}}
        }
        CHECK(root && status);
        int observed=0;
        for(unsigned i=0;i<200 && !observed;++i){pump(10);observed=status_is(status,"Applied to this example");}
        CHECK(observed);
        name=find(root,"Display name",0);apply=find(root,"Apply settings",0);CHECK(name && apply && enabled(apply));
        AtspiAccessible *motion=find(root,"Reduce motion",0);CHECK(motion);
        AtspiStateSet *states=atspi_accessible_get_state_set(motion);
        CHECK(states && atspi_state_set_contains(states,ATSPI_STATE_FOCUSED));
        g_object_unref(states);g_object_unref(motion);
        AtspiText *text=atspi_accessible_get_text_iface(name);CHECK(text);
        char *draft=atspi_text_get_text(text,0,-1,&error);
        CHECK(!error && draft && !strcmp(draft,"Reopened draft"));g_free(draft);g_object_unref(text);
        pump(600);CHECK(status_is(status,"Applied to this example"));
        g_object_unref(action);g_object_unref(apply);g_object_unref(status);g_object_unref(name);g_object_unref(root);g_object_unref(desktop);
        atspi_exit();cleanup();puts("Backend completion survived view replacement without redispatch.");return 0;
    }
    const char *unknown="Outcome unknown. Check the service.";int observed=0;
    for(unsigned i=0;i<300 && !observed;++i){pump(10);observed=status_is(status,unknown);}
    CHECK(observed && !enabled(apply));
    CHECK(!atspi_action_do_action(action,0,&error) && !error);
    AtspiEditableText *editable=atspi_accessible_get_editable_text_iface(name);CHECK(editable);
    CHECK(atspi_editable_text_set_text_contents(editable,"Later draft",&error) && !error);
    pump(1500);CHECK(status_is(status,unknown) && !enabled(apply));
    AtspiText *text=atspi_accessible_get_text_iface(name);CHECK(text);
    char *draft=atspi_text_get_text(text,0,-1,&error);
    CHECK(!error && draft && !strcmp(draft,"Later draft"));g_free(draft);
    g_object_unref(text);g_object_unref(editable);g_object_unref(action);
    g_object_unref(apply);g_object_unref(status);g_object_unref(name);g_object_unref(root);g_object_unref(desktop);
    atspi_exit();cleanup();
    puts("Unknown outcome stays pending, blocks redispatch, and preserves later draft.");
    return 0;
}
