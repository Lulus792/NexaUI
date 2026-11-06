#define _POSIX_C_SOURCE 200809L
#include <atspi/atspi.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static void pause_briefly(void){struct timespec delay={0,20000000};nanosleep(&delay,NULL);}
static AtspiAccessible *find(AtspiAccessible *root,const char *name,unsigned depth){
    if(!root || depth>12)return NULL;
    GError *error=NULL;char *text=atspi_accessible_get_name(root,&error);
    int match=!error && text && !strcmp(text,name);g_free(text);
    if(error){g_error_free(error);error=NULL;}if(match)return g_object_ref(root);
    int count=atspi_accessible_get_child_count(root,&error);if(error){g_error_free(error);return NULL;}
    for(int i=0;i<count;++i){AtspiAccessible *child=atspi_accessible_get_child_at_index(root,i,&error);
        if(error){g_error_free(error);error=NULL;continue;}
        AtspiAccessible *result=find(child,name,depth+1);if(child)g_object_unref(child);if(result)return result;
    }
    return NULL;
}
static AtspiAccessible *by_pid(AtspiAccessible *desktop,pid_t pid){
    GError *error=NULL;atspi_accessible_clear_cache(desktop);int count=atspi_accessible_get_child_count(desktop,&error);
    if(error){g_error_free(error);return NULL;}
    for(int i=0;i<count;++i){AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,&error);
        if(error){g_error_free(error);error=NULL;continue;}if(!app)continue;
        guint actual=atspi_accessible_get_process_id(app,&error);
        if(!error && actual==(guint)pid)return app;
        if(error){g_error_free(error);error=NULL;}g_object_unref(app);
    }
    return NULL;
}
static int state(AtspiAccessible *node,AtspiStateType wanted){
    atspi_accessible_clear_cache(node);AtspiStateSet *states=atspi_accessible_get_state_set(node);
    int result=states && atspi_state_set_contains(states,wanted);if(states)g_object_unref(states);return result;
}
static int value(AtspiAccessible *node,const char *expected){
    GError *error=NULL;atspi_accessible_clear_cache(node);GHashTable *attrs=atspi_accessible_get_attributes(node,&error);
    int result=!error && attrs && !g_strcmp0(g_hash_table_lookup(attrs,"nexaui-value"),expected);
    if(attrs)g_hash_table_unref(attrs);
    if(error)g_error_free(error);
    return result;
}
static int activate(AtspiAccessible *node){
    GError *error=NULL;AtspiAction *action=atspi_accessible_get_action_iface(node);
    int result=action && atspi_action_do_action(action,0,&error) && !error;
    if(action)g_object_unref(action);
    if(error)g_error_free(error);
    return result;
}
static int response(FILE *stream,const char *expected){
    struct pollfd fd={fileno(stream),POLLIN,0};char line[160];
    if(poll(&fd,1,10000)<=0 || !fgets(line,sizeof(line),stream))return 0;
    printf("HOST %s",line);return !strcmp(line,expected);
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"removal-atspi:%d: %s\n",__LINE__,#x);goto cleanup;}}while(0)
int main(int argc,char **argv){
    int okay=0,input[2]={-1,-1},output[2]={-1,-1};pid_t child=-1;FILE *commands=NULL,*responses=NULL;
    AtspiAccessible *desktop=NULL,*app=NULL,*reviewed=NULL,*request=NULL,*close_view=NULL,*old_plan=NULL,*status=NULL,*old_request=NULL;
    const char *font=getenv("NEXAUI_REMOVAL_FONT");int incomplete=getenv("NEXAUI_REMOVAL_INCOMPLETE")!=NULL;
    int keyboard=getenv("NEXAUI_REMOVAL_KEYBOARD")!=NULL;setvbuf(stdout,NULL,_IONBF,0);
    signal(SIGPIPE,SIG_IGN);
    CHECK(argc==2 && font && !pipe(input) && !pipe(output));child=fork();CHECK(child>=0);
    if(!child){
        if(dup2(input[0],STDIN_FILENO)<0 || dup2(output[1],STDOUT_FILENO)<0)_exit(125);
        close(input[0]);close(input[1]);close(output[0]);close(output[1]);
        execl(argv[1],argv[1],font,(char *)NULL);_exit(126);
    }
    close(input[0]);input[0]=-1;close(output[1]);output[1]=-1;
    commands=fdopen(input[1],"w");CHECK(commands);input[1]=-1;
    responses=fdopen(output[0],"r");CHECK(responses);output[0]=-1;setvbuf(responses,NULL,_IONBF,0);
    CHECK(response(responses,"REMOVAL_READY\n") && atspi_init()==0);desktop=atspi_get_desktop(0);CHECK(desktop);
    for(unsigned i=0;i<150 && !reviewed;++i){
        if(app){g_object_unref(app);app=NULL;}app=by_pid(desktop,child);
        if(app)reviewed=find(app,"Scope reviewed",0);
        if(!reviewed)pause_briefly();
    }
    CHECK(reviewed);request=find(app,"Request removal",0);close_view=find(app,"Close view",0);old_plan=find(app,"Plan",0);
    CHECK(request && close_view && old_plan && value(old_plan,"00000000000000000000000000000001 / revision 42"));
    AtspiAccessible *installation=find(app,"Installation",0),*scope=find(app,"Scope",0);
    CHECK(installation && scope && value(installation,"00000000000000000000000000000002") &&
        value(scope,"Selected user's managed installation (00000000000000000000000000000003)"));
    g_object_unref(installation);g_object_unref(scope);
    CHECK(!state(request,ATSPI_STATE_ENABLED) && !activate(request));
    const char *groups[]={"Remove in this scope","Retained: shared dependency","Retained: explicit choice",
        "Independent documents","Backups or remote copies","Unknown or unmanaged"};
    for(unsigned i=0;i<6;++i){AtspiAccessible *group=find(app,groups[i],0);CHECK(group);
        CHECK(value(group,i==0?"4 objects":i==5?"0 objects":"1 object"));g_object_unref(group);}
    const char *objects[]={"Application code","Private settings and cache","Credential registration","Owned background service",
        "Shared support library","Profile deliberately kept","Independent project document","Backup outside this action"};
    for(unsigned i=0;i<8;++i){AtspiAccessible *object=find(app,objects[i],0);CHECK(object);g_object_unref(object);}
    if(incomplete){
        status=find(app,"Inventory incomplete; request unavailable",0);CHECK(status && !state(reviewed,ATSPI_STATE_ENABLED));
        CHECK(activate(close_view) && response(responses,"REMOVAL_CLOSED requests=0 reopened=0\n"));
    } else {
        if(keyboard){CHECK(state(reviewed,ATSPI_STATE_FOCUSED));puts("REMOVAL_KEY_CHECK");}
        else CHECK(activate(reviewed));
        for(unsigned i=0;i<100 && !state(reviewed,ATSPI_STATE_CHECKED);++i)pause_briefly();
        CHECK(state(reviewed,ATSPI_STATE_CHECKED));
        for(unsigned i=0;i<100 && !state(request,ATSPI_STATE_ENABLED);++i)pause_briefly();
        CHECK(state(request,ATSPI_STATE_ENABLED));
        if(keyboard){
            puts("REMOVAL_KEY_FOCUS_CLOSE");for(unsigned i=0;i<100 && !state(close_view,ATSPI_STATE_FOCUSED);++i)pause_briefly();
            CHECK(state(close_view,ATSPI_STATE_FOCUSED));puts("REMOVAL_KEY_FOCUS_REQUEST");
            for(unsigned i=0;i<100 && !state(request,ATSPI_STATE_FOCUSED);++i)pause_briefly();
            CHECK(state(request,ATSPI_STATE_FOCUSED));puts("REMOVAL_KEY_REQUEST");
        } else CHECK(activate(request));
        CHECK(response(responses,"REMOVAL_REQUESTED revision=42\n"));
        CHECK(fputs("reopen\n",commands)>=0 && !fflush(commands) && response(responses,"REMOVAL_REOPENED\n"));
        g_object_unref(app);app=NULL;old_request=request;request=NULL;g_object_unref(close_view);close_view=NULL;
        for(unsigned i=0;i<150 && !request;++i){
            if(app){g_object_unref(app);app=NULL;}app=by_pid(desktop,child);
            if(app)request=find(app,"Request removal",0);
            if(!request)pause_briefly();
        }
        CHECK(request && !state(request,ATSPI_STATE_ENABLED) && state(old_plan,ATSPI_STATE_DEFUNCT));
        CHECK(!activate(request) && state(old_request,ATSPI_STATE_DEFUNCT) && !activate(old_request));
        status=find(app,"Request handed to owner; outcome unknown",0);CHECK(status && !state(status,ATSPI_STATE_BUSY));
        close_view=find(app,"Close view",0);CHECK(close_view);
        if(keyboard){CHECK(state(close_view,ATSPI_STATE_FOCUSED));puts("REMOVAL_KEY_CLOSE");}
        else CHECK(activate(close_view));
        CHECK(response(responses,"REMOVAL_CLOSED requests=1 reopened=1\n"));
    }
    {int status_code=0;CHECK(waitpid(child,&status_code,0)==child && WIFEXITED(status_code) && WEXITSTATUS(status_code)==0);child=-1;}
    okay=1;puts("Independent removal preview, exact plan, disabled action, view replacement and clean close passed");
cleanup:
    if(child>0){
        if(commands){fputs("close\n",commands);fflush(commands);}
        int status_code=0;pid_t ended=0;
        for(unsigned i=0;i<100 && !(ended=waitpid(child,&status_code,WNOHANG));++i)pause_briefly();
        if(!ended){kill(child,SIGKILL);waitpid(child,&status_code,0);}
    }
    if(status)g_object_unref(status);
    if(old_plan)g_object_unref(old_plan);
    if(old_request)g_object_unref(old_request);
    if(close_view)g_object_unref(close_view);
    if(request)g_object_unref(request);
    if(reviewed)g_object_unref(reviewed);
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    atspi_exit();
    if(commands)fclose(commands);
    if(responses)fclose(responses);
    for(unsigned i=0;i<2;++i){if(input[i]>=0)close(input[i]);if(output[i]>=0)close(output[i]);}
    return okay?0:1;
}
