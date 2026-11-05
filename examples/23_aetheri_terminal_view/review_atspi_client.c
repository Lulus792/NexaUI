#define _POSIX_C_SOURCE 200809L
/* Independent AT-SPI client; it does not link to NexaUI. */
#include <atspi/atspi.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static pid_t child=-1;
static void cleanup(void){if(child>0){kill(child,SIGTERM);waitpid(child,NULL,0);child=-1;}}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"review-atspi:%d: %s\n",__LINE__,#x);cleanup();return 1;}} while(0)
static void pause_ms(void){struct timespec delay={0,10000000};nanosleep(&delay,NULL);}
static AtspiAccessible *find(AtspiAccessible *node,const char *name,unsigned depth) {
    if(!node || depth>20)return NULL;
    GError *error=NULL;char *s=atspi_accessible_get_name(node,&error);
    int match=!error && s && !strcmp(s,name);if(error)g_error_free(error);g_free(s);
    if(match)return g_object_ref(node);
    error=NULL;int count=atspi_accessible_get_child_count(node,&error);
    if(error){g_error_free(error);return NULL;}
    for(int i=0;i<count;++i){AtspiAccessible *next=atspi_accessible_get_child_at_index(node,i,&error);
        if(error){g_error_free(error);error=NULL;continue;}AtspiAccessible *found=find(next,name,depth+1);
        if(next)g_object_unref(next);
        if(found)return found;}
    return NULL;
}
int main(int argc,char **argv) {
    const char *font=getenv("NEXAUI_TERMINAL_FONT");CHECK(argc==2 && font && *font);
    child=fork();CHECK(child>=0);if(!child){execl(argv[1],argv[1],font,(char *)NULL);_exit(127);}
    CHECK(atspi_init()==0);atspi_set_timeout(1000,1000);AtspiAccessible *desktop=atspi_get_desktop(0);CHECK(desktop);
    AtspiAccessible *app=NULL;
    for(unsigned attempt=0;attempt<300 && !app;++attempt){
        while(g_main_context_iteration(NULL,FALSE)){}atspi_accessible_clear_cache(desktop);GError *error=NULL;
        int count=atspi_accessible_get_child_count(desktop,&error);if(error){g_error_free(error);pause_ms();continue;}
        for(int i=0;i<count && !app;++i){AtspiAccessible *candidate=atspi_accessible_get_child_at_index(desktop,i,&error);
            if(error){g_error_free(error);error=NULL;continue;}if(!candidate)continue;
            guint pid=atspi_accessible_get_process_id(candidate,&error);
            if(!error && pid==(guint)child)app=g_object_ref(candidate);
            if(error){g_error_free(error);error=NULL;}g_object_unref(candidate);}
        if(!app)pause_ms();
    }
    CHECK(app);AtspiAccessible *row=find(app,"Preview row",0),*next=find(app,"Next",0),*cancel=find(app,"Cancel",0);
    CHECK(row && next && cancel);AtspiStateSet *states=atspi_accessible_get_state_set(cancel);
    CHECK(states && atspi_state_set_contains(states,ATSPI_STATE_FOCUSED));g_object_unref(states);
    AtspiText *text=atspi_accessible_get_text_iface(row);CHECK(text);GError *error=NULL;
    char *value=atspi_text_get_text(text,0,-1,&error);CHECK(!error && value && !strcmp(value,"AAAAAAAAAAAAAAAA"));g_free(value);
    CHECK(atspi_text_get_caret_offset(text,&error)==-1 && !error);
    AtspiAction *action=atspi_accessible_get_action_iface(next);CHECK(action);
    CHECK(atspi_action_do_action(action,0,&error) && !error);g_object_unref(action);
    int changed=0;
    for(unsigned i=0;i<300 && !changed;++i){while(g_main_context_iteration(NULL,FALSE)){}
        atspi_accessible_clear_cache(row);value=atspi_text_get_text(text,0,-1,&error);CHECK(!error && value);
        changed=!strcmp(value,"BBBBBBBBBBBBBBBB");g_free(value);if(!changed)pause_ms();}
    CHECK(changed);
    AtspiAccessible *decision=cancel;
    if(getenv("NEXAUI_REVIEW_ACCEPT")){decision=find(app,"Paste",0);CHECK(decision);}
    action=atspi_accessible_get_action_iface(decision);CHECK(action);
    CHECK(atspi_action_do_action(action,0,&error) && !error);g_object_unref(action);
    if(decision!=cancel)g_object_unref(decision);
    g_object_unref(text);g_object_unref(row);g_object_unref(next);g_object_unref(cancel);g_object_unref(app);g_object_unref(desktop);
    int status=0,exited=0;for(unsigned i=0;i<300 && !exited;++i){pid_t p=waitpid(child,&status,WNOHANG);CHECK(p>=0);if(p==child)exited=1;else pause_ms();}
    CHECK(exited && WIFEXITED(status) && WEXITSTATUS(status)==0);child=-1;
    puts(getenv("NEXAUI_REVIEW_ACCEPT")?"Independent AT-SPI review navigation and paste passed":
        "Independent AT-SPI review navigation and cancel passed");return 0;
}
