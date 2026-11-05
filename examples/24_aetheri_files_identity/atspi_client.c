#define _POSIX_C_SOURCE 200809L
/* Independent libatspi client: no NexaUI linkage or private APIs. */
#include <atspi/atspi.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"files-atspi:%d: %s\n",__LINE__,#x);goto cleanup;}} while(0)
static void pause_ms(void){struct timespec t={0,10000000};(void)nanosleep(&t,NULL);}
static AtspiAccessible *find(AtspiAccessible *node,const char *name,unsigned depth) {
    if(!node || depth>20)return NULL;
    GError *error=NULL;char *text=atspi_accessible_get_name(node,&error);
    int match=!error && text && !strcmp(text,name);if(error)g_error_free(error);g_free(text);
    if(match){error=NULL;AtspiRole role=atspi_accessible_get_role(node,&error);
        match=!error && role!=ATSPI_ROLE_APPLICATION && role!=ATSPI_ROLE_FRAME;
        if(error)g_error_free(error);}
    if(match)return g_object_ref(node);
    error=NULL;int count=atspi_accessible_get_child_count(node,&error);
    if(error){g_error_free(error);return NULL;}
    for(int i=0;i<count;++i){AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,&error);
        if(error){g_error_free(error);error=NULL;continue;}
        AtspiAccessible *result=find(child,name,depth+1);if(child)g_object_unref(child);if(result)return result;}
    return NULL;
}
static int selected(AtspiAccessible *node) {
    atspi_accessible_clear_cache(node);AtspiStateSet *states=atspi_accessible_get_state_set(node);
    if(!states)return 0;
    int result=atspi_state_set_contains(states,ATSPI_STATE_SELECTED);g_object_unref(states);return result;
}
static int invalid(AtspiAccessible *node) {
    atspi_accessible_clear_cache(node);AtspiStateSet *states=atspi_accessible_get_state_set(node);
    if(!states)return 0;
    int result=atspi_state_set_contains(states,ATSPI_STATE_INVALID_ENTRY);g_object_unref(states);return result;
}
int main(int argc,char **argv) {
    int okay=0,root=-1,pipes[2]={-1,-1},created=0;pid_t child=-1;
    char path[]="/tmp/nexaui-files-atspi-XXXXXX",output[8192]={0};size_t used=0;
    AtspiAccessible *desktop=NULL,*first=NULL,*second=NULL,*filter=NULL,*close_button=NULL,*tree=NULL,*status_node=NULL;AtspiAction *action=NULL,*close_action=NULL;
    AtspiEditableText *edit=NULL;
    const char *font=getenv("NEXAUI_FILES_FONT");
    CHECK(argc==2 && font && *font && mkdtemp(path));created=1;
    root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(root>=0);
    for(int i=0;i<2;++i){const char *name=i?"item01":"item00";
        int fd=openat(root,name,O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600);CHECK(fd>=0 && !close(fd));}
    CHECK(!pipe(pipes));child=fork();CHECK(child>=0);
    if(!child){close(pipes[0]);if(dup2(pipes[1],STDOUT_FILENO)<0)_exit(126);close(pipes[1]);
        if(setenv("NEXAUI_FILES_NATIVE_TRACE","1",1))_exit(126);
        execl(argv[1],argv[1],path,font,(char *)NULL);_exit(127);}
    close(pipes[1]);pipes[1]=-1;CHECK(fcntl(pipes[0],F_SETFL,fcntl(pipes[0],F_GETFL)|O_NONBLOCK)==0);
    CHECK(atspi_init()==0);atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    for(unsigned attempt=0;attempt<500 && (!first || !second || !filter || !close_button);++attempt) {
        while(g_main_context_iteration(NULL,FALSE)){}atspi_accessible_clear_cache(desktop);
        GError *error=NULL;int count=atspi_accessible_get_child_count(desktop,&error);
        if(error){g_error_free(error);pause_ms();continue;}
        for(int i=0;i<count;++i){AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,&error);
            if(error){g_error_free(error);error=NULL;continue;}if(!app)continue;
            guint pid=atspi_accessible_get_process_id(app,&error);
            if(!error && pid==(guint)child){if(!first)first=find(app,"item00",0);if(!second)second=find(app,"item01",0);
                if(!filter)filter=find(app,"Filter files",0);
                if(!close_button)close_button=find(app,"Close",0);}
            if(error){g_error_free(error);error=NULL;}g_object_unref(app);}
        if(!first || !second || !filter || !close_button)pause_ms();
    }
    CHECK(first && second && filter && close_button);GError *error=NULL;
    CHECK(atspi_accessible_get_role(first,&error)==ATSPI_ROLE_TREE_ITEM && !error);
    tree=atspi_accessible_get_parent(first,&error);CHECK(tree && !error);
    CHECK(selected(first) && !selected(second));action=atspi_accessible_get_action_iface(second);CHECK(action);
    int count=atspi_action_get_n_actions(action,&error);CHECK(!error && count>0);int index=-1;
    for(int i=0;i<count;++i){char *name=atspi_action_get_name(action,i,&error);CHECK(!error);
        if(name && !strcmp(name,"select"))index=i;
        g_free(name);}
    CHECK(index>=0 && atspi_action_do_action(action,index,&error) && !error);
    int observed=0,model_observed=0;
    for(unsigned attempt=0;attempt<500 && (!observed || !model_observed);++attempt) {
        while(g_main_context_iteration(NULL,FALSE)){}
        observed=selected(second) && !selected(first);
        ssize_t n=read(pipes[0],output+used,sizeof(output)-1-used);
        if(n>0){used+=(size_t)n;output[used]=0;if(strstr(output,"FILES_NATIVE_SELECTED item01\n"))model_observed=1;}
        if(!observed || !model_observed)pause_ms();
    }
    CHECK(observed && model_observed);
    atspi_accessible_clear_cache(filter);
    edit=atspi_accessible_get_editable_text_iface(filter);
    CHECK(edit);
    CHECK(atspi_editable_text_set_text_contents(edit,"item01",&error) && !error);
    int filtered=0;
    for(unsigned attempt=0;attempt<500 && !filtered;++attempt) {
        ssize_t n=read(pipes[0],output+used,sizeof(output)-1-used);
        if(n>0){used+=(size_t)n;output[used]=0;filtered=strstr(output,"FILES_NATIVE_FILTER 6 1 1\n")!=NULL;}
        if(!filtered)pause_ms();
    }
    CHECK(filtered && !invalid(filter));
    CHECK(atspi_editable_text_set_text_contents(edit,"no-such-file",&error) && !error);
    for(unsigned attempt=0;attempt<500 && !status_node;++attempt){
        while(g_main_context_iteration(NULL,FALSE)){}atspi_accessible_clear_cache(tree);
        status_node=find(tree,"No matching files.",0);if(!status_node)pause_ms();}
    CHECK(status_node && atspi_accessible_get_role(status_node,&error)==ATSPI_ROLE_STATUS_BAR && !error);
    g_object_unref(status_node);status_node=NULL;
    char oversized[257];memset(oversized,'x',256);oversized[256]=0;
    CHECK(atspi_editable_text_set_text_contents(edit,oversized,&error) && !error);
    int rejected=0;
    for(unsigned attempt=0;attempt<500 && !rejected;++attempt) {
        ssize_t n=read(pipes[0],output+used,sizeof(output)-1-used);
        if(n>0){used+=(size_t)n;output[used]=0;rejected=strstr(output,"FILES_NATIVE_FILTER 12 0 0\n")!=NULL;}
        if(!rejected)pause_ms();
    }
    CHECK(rejected && invalid(filter));
    for(unsigned attempt=0;attempt<500 && !status_node;++attempt){
        while(g_main_context_iteration(NULL,FALSE)){}atspi_accessible_clear_cache(tree);
        status_node=find(tree,"Invalid filter. Clear it to continue.",0);if(!status_node)pause_ms();}
    CHECK(status_node && atspi_accessible_get_role(status_node,&error)==ATSPI_ROLE_STATUS_BAR && !error);
    CHECK(atspi_editable_text_set_text_contents(edit,"item01",&error) && !error && invalid(filter));
    CHECK(atspi_editable_text_set_text_contents(edit,"",&error) && !error && !invalid(filter));
    CHECK(atspi_accessible_get_role(close_button,&error)==ATSPI_ROLE_PUSH_BUTTON && !error);
    close_action=atspi_accessible_get_action_iface(close_button);CHECK(close_action);
    CHECK(atspi_action_get_n_actions(close_action,&error)>0 && !error);
    CHECK(atspi_action_do_action(close_action,0,&error) && !error);
    int status=0;pid_t waited=0;
    for(unsigned i=0;i<300 && !waited;++i){waited=waitpid(child,&status,WNOHANG);if(!waited)pause_ms();}
    CHECK(waited==child && WIFEXITED(status) && WEXITSTATUS(status)==0);child=-1;okay=1;
cleanup:
    if(child>0){(void)kill(child,SIGKILL);(void)waitpid(child,NULL,0);}
    if(action)g_object_unref(action);
    if(close_action)g_object_unref(close_action);
    if(edit)g_object_unref(edit);
    if(first)g_object_unref(first);
    if(second)g_object_unref(second);
    if(filter)g_object_unref(filter);
    if(close_button)g_object_unref(close_button);
    if(tree)g_object_unref(tree);
    if(status_node)g_object_unref(status_node);
    if(desktop)g_object_unref(desktop);
    if(pipes[0]>=0)close(pipes[0]);
    if(pipes[1]>=0)close(pipes[1]);
    if(root>=0){(void)unlinkat(root,"item00",0);(void)unlinkat(root,"item01",0);close(root);}
    if(created)(void)rmdir(path);
    if(okay)puts("Independent AT-SPI Files selection, filtering and close passed");
    return okay?0:1;
}
