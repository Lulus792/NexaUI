#define _POSIX_C_SOURCE 200809L
/* Independent native client: no NexaUI headers or linkage. */
#include <atspi/atspi.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static pid_t child=-1;
static void cleanup(void) {if(child>0){kill(child,SIGTERM);waitpid(child,NULL,0);child=-1;}}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"desktop-atspi:%d: %s\n",__LINE__,#x);cleanup();return 1;}} while(0)
static void pump(unsigned ms) {
    for(unsigned i=0;i<ms;++i){while(g_main_context_iteration(NULL,FALSE)){}struct timespec t={0,1000000};nanosleep(&t,NULL);}
}
static AtspiAccessible *find(AtspiAccessible *node,const char *name,AtspiRole role,unsigned depth) {
    if(!node || depth>20)return NULL;
    GError *error=NULL;char *got=atspi_accessible_get_name(node,&error);
    int named=!error && got && !strcmp(got,name);if(error)g_error_free(error);g_free(got);
    error=NULL;AtspiRole actual=atspi_accessible_get_role(node,&error);
    if(!error && named && actual==role)return g_object_ref(node);
    if(error)g_error_free(error);
    error=NULL;
    int n=atspi_accessible_get_child_count(node,&error);if(error){g_error_free(error);return NULL;}
    for(int i=0;i<n;++i){
        AtspiAccessible *next=atspi_accessible_get_child_at_index(node,i,&error);
        if(error){g_error_free(error);error=NULL;continue;}
        AtspiAccessible *found=find(next,name,role,depth+1);if(next)g_object_unref(next);if(found)return found;
    }
    return NULL;
}
static AtspiAccessible *app_for_pid(AtspiAccessible *desktop) {
    for(unsigned n=0;n<300;++n){
        pump(10);atspi_accessible_clear_cache(desktop);GError *error=NULL;
        int count=atspi_accessible_get_child_count(desktop,&error);
        if(error){g_error_free(error);continue;}
        for(int i=0;i<count;++i){
            AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,&error);
            if(error){g_error_free(error);error=NULL;continue;}if(!app)continue;
            guint pid=atspi_accessible_get_process_id(app,&error);
            if(!error && pid==(guint)child && atspi_accessible_get_child_count(app,&error)>0 && !error)return app;
            if(error){g_error_free(error);error=NULL;}g_object_unref(app);
        }
    }
    return NULL;
}
static int wait_output(int fd,char *buffer,size_t capacity,size_t *used,const char *needle) {
    for(unsigned n=0;n<100;++n) {
        if(strstr(buffer,needle))return 1;
        struct pollfd p={fd,POLLIN,0};
        if(poll(&p,1,30)>0) {
            if(*used>=capacity-1)return 0;
            ssize_t bytes=read(fd,buffer+*used,capacity-*used-1);
            if(bytes<=0)return 0;
            *used+=(size_t)bytes;buffer[*used]=0;
        }
    }
    return strstr(buffer,needle)!=NULL;
}
int main(int argc,char **argv) {
    CHECK(argc==2);const char *font=getenv("NEXAUI_DESKTOP_FONT");CHECK(font && *font);
    int pipes[2];CHECK(!pipe(pipes));child=fork();CHECK(child>=0);
    if(!child){dup2(pipes[1],STDOUT_FILENO);close(pipes[0]);close(pipes[1]);execl(argv[1],argv[1],"--diagnostics",font,(char *)NULL);_exit(127);}
    close(pipes[1]);CHECK(atspi_init()==0);atspi_set_timeout(1000,1000);
    AtspiAccessible *desktop=atspi_get_desktop(0);CHECK(desktop);
    AtspiAccessible *app=app_for_pid(desktop);CHECK(app);
    AtspiAccessible *field=NULL;
    for(unsigned n=0;n<200 && !field;++n){atspi_accessible_clear_cache(app);field=find(app,"Search",ATSPI_ROLE_ENTRY,0);if(!field)field=find(app,"Suche",ATSPI_ROLE_ENTRY,0);if(!field)pump(10);}
    CHECK(field);AtspiEditableText *editable=atspi_accessible_get_editable_text_iface(field);CHECK(editable);
    GError *scope_error=NULL;AtspiAccessible *scope=atspi_accessible_get_parent(field,&scope_error);
    CHECK(scope && !scope_error);
    if(getenv("NEXAUI_AX_FILES_DOCK")) {
        AtspiAccessible *button=find(scope,"Files",ATSPI_ROLE_PUSH_BUTTON,0);CHECK(button);
        AtspiAction *button_action=atspi_accessible_get_action_iface(button);CHECK(button_action);
        GError *button_error=NULL;CHECK(atspi_action_do_action(button_action,0,&button_error) && !button_error);
        char output[1024]={0};size_t used=0;
        CHECK(wait_output(pipes[0],output,sizeof(output),&used,getenv("NEXAUI_AX_LAYER")?
                          "INTENT 0 2 3 0\n":"INTENT kind=2 app=3 window=0\n"));
        g_object_unref(button_action);g_object_unref(button);
    }
    if(getenv("NEXAUI_AX_SELECTION")) {
        AtspiAccessible *list=find(scope,"Search results",ATSPI_ROLE_LIST,0);
        AtspiAccessible *row=find(scope,"Terminal",ATSPI_ROLE_LIST_ITEM,0);
        CHECK(list && row);GError *selection_error=NULL;
        AtspiSelection *selection=atspi_accessible_get_selection_iface(list);CHECK(selection);
        if(getenv("NEXAUI_AX_CURRENT_FILES")) {
            AtspiAccessible *files=find(scope,"Files",ATSPI_ROLE_LIST_ITEM,0);CHECK(files);
            int files_index=atspi_accessible_get_index_in_parent(files,&selection_error);CHECK(files_index>=0 && !selection_error);
            CHECK(atspi_selection_select_child(selection,files_index,&selection_error) && !selection_error);
            char files_output[1024]={0};size_t files_used=0;
            CHECK(wait_output(pipes[0],files_output,sizeof(files_output),&files_used,getenv("NEXAUI_AX_LAYER")?
                              "SELECT 0 3 0\n":"SELECT 3 0\n"));
            CHECK(!strstr(files_output,"INTENT"));g_object_unref(files);
        }
        int index=atspi_accessible_get_index_in_parent(row,&selection_error);CHECK(index>=0 && !selection_error);
        CHECK(atspi_selection_select_child(selection,index,&selection_error) && !selection_error);
        char selected_output[1024]={0};size_t selected_used=0;
        CHECK(wait_output(pipes[0],selected_output,sizeof(selected_output),&selected_used,getenv("NEXAUI_AX_LAYER")?
                          "SELECT 0 2 0\n":"SELECT 2 0\n"));
        int selected=0;
        for(unsigned i=0;i<300 && !selected;++i) {
            pump(10);atspi_accessible_clear_cache(row);AtspiStateSet *states=atspi_accessible_get_state_set(row);
            if(states){selected=atspi_state_set_contains(states,ATSPI_STATE_SELECTED);g_object_unref(states);}
        }
        CHECK(selected && !strstr(selected_output,"INTENT"));
        g_object_unref(selection);g_object_unref(row);g_object_unref(list);
    }
    if(getenv("NEXAUI_AX_LAYER_MULTI")) {
        GError *multi_error=NULL;CHECK(atspi_accessible_get_child_count(app,&multi_error)==3 && !multi_error);
        AtspiAccessible *other=find(app,"Suche",ATSPI_ROLE_ENTRY,0);CHECK(other);
        AtspiStateSet *first_states=atspi_accessible_get_state_set(field),*other_states=atspi_accessible_get_state_set(other);
        CHECK(first_states && other_states && atspi_state_set_contains(first_states,ATSPI_STATE_FOCUSED) &&
              !atspi_state_set_contains(other_states,ATSPI_STATE_FOCUSED));
        g_object_unref(first_states);g_object_unref(other_states);
        CHECK(!kill(child,SIGUSR1));char removed[1024]={0};size_t removed_size=0;
        CHECK(wait_output(pipes[0],removed,sizeof(removed),&removed_size,"REMOVED 2\n"));
        atspi_accessible_clear_cache(other);multi_error=NULL;
        char *old_name=atspi_accessible_get_name(other,&multi_error);
        CHECK(multi_error!=NULL);g_error_free(multi_error);g_free(old_name);g_object_unref(other);
    }
    GError *error=NULL;CHECK(atspi_editable_text_set_text_contents(editable,"Terminal",&error) && !error);
    char buffer[4096]={0};size_t used=0;
    CHECK(wait_output(pipes[0],buffer,sizeof(buffer),&used,getenv("NEXAUI_AX_LAYER")?"QUERY 0 Terminal\n":"QUERY Terminal\n"));
    if(getenv("NEXAUI_AX_REENTER"))CHECK(wait_output(pipes[0],buffer,sizeof(buffer),&used,"REENTER_BLOCKED\n"));
    int writable=0;
    for(unsigned n=0;n<300 && !writable;++n) {
        pump(10);atspi_accessible_clear_cache(app);
        AtspiAccessible *current=find(app,"Search",ATSPI_ROLE_ENTRY,0);
        if(!current)current=find(app,"Suche",ATSPI_ROLE_ENTRY,0);
        if(!current)continue;
        g_object_unref(field);field=current;atspi_accessible_clear_cache(field);
        AtspiStateSet *states=atspi_accessible_get_state_set(field);
        AtspiText *text=atspi_accessible_get_text_iface(field);
        GError *read_error=NULL;
        char *value=text?atspi_text_get_text(text,0,-1,&read_error):NULL;
        if(states && value && !read_error)
            writable=!strcmp(value,"Terminal") && !atspi_state_set_contains(states,ATSPI_STATE_READ_ONLY);
        if(read_error)g_error_free(read_error);
        g_free(value);if(text)g_object_unref(text);if(states)g_object_unref(states);
    }
    CHECK(writable);
    g_object_unref(editable);editable=atspi_accessible_get_editable_text_iface(field);CHECK(editable);
    CHECK(atspi_editable_text_set_text_contents(editable,"Termina",&error) && !error);
    CHECK(wait_output(pipes[0],buffer,sizeof(buffer),&used,getenv("NEXAUI_AX_LAYER")?"QUERY 0 Termina\n":"QUERY Termina\n"));
    AtspiAccessible *terminal=NULL;
    for(unsigned n=0;n<300 && !terminal;++n){pump(10);atspi_accessible_clear_cache(app);terminal=find(app,"Terminal",ATSPI_ROLE_LIST_ITEM,0);}
    CHECK(terminal);AtspiAction *action=atspi_accessible_get_action_iface(terminal);CHECK(action);
    CHECK(atspi_action_do_action(action,0,&error) && !error);
    CHECK(wait_output(pipes[0],buffer,sizeof(buffer),&used,getenv("NEXAUI_AX_LAYER")?"INTENT 0 2 2 0":"INTENT kind=2 app=2 window=0"));
    if(getenv("NEXAUI_AX_REBIND")) {
        CHECK(!kill(child,SIGUSR1));CHECK(wait_output(pipes[0],buffer,sizeof(buffer),&used,"REBOUND\n"));
        atspi_accessible_clear_cache(field);error=NULL;
        char *old_name=atspi_accessible_get_name(field,&error);
        CHECK(error!=NULL);g_error_free(error);g_free(old_name);
        AtspiAccessible *replacement=app_for_pid(desktop);CHECK(replacement);
        AtspiAccessible *new_field=NULL;
        for(unsigned n=0;n<200 && !new_field;++n){atspi_accessible_clear_cache(replacement);new_field=find(replacement,"Search",ATSPI_ROLE_ENTRY,0);if(!new_field)pump(10);}
        CHECK(new_field);g_object_unref(new_field);g_object_unref(replacement);
    }
    g_object_unref(action);g_object_unref(terminal);g_object_unref(editable);g_object_unref(field);g_object_unref(scope);g_object_unref(app);g_object_unref(desktop);
    CHECK(kill(child,SIGTERM)==0);int status=0;CHECK(waitpid(child,&status,0)==child);child=-1;
    CHECK(WIFEXITED(status) && WEXITSTATUS(status)==0);close(pipes[0]);atspi_exit();
    puts("Independent AT-SPI desktop query and intent passed");return 0;
}
