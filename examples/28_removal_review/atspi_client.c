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
    atspi_accessible_clear_cache(root);
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
static int readable_text(AtspiAccessible *node,const char *expected){
    GError *error=NULL;atspi_accessible_clear_cache(node);
    AtspiText *text=atspi_accessible_get_text_iface(node);
    if(!text){fprintf(stderr,"Missing AT-SPI Text for a read-only value\n");return 0;}
    char *actual=atspi_text_get_text(text,0,-1,&error);
    int result=!error && actual && !strcmp(actual,expected);g_free(actual);
    if(!error)result=result && atspi_text_get_character_count(text,&error)==g_utf8_strlen(expected,-1);
    if(!error)result=result && atspi_text_get_caret_offset(text,&error)==-1;
    if(!error)result=result && atspi_text_get_n_selections(text,&error)==0;
    if(error){g_error_free(error);error=NULL;result=0;}
    /* Read-only data cannot acquire a caret or an app-owned selection. */
    int moved=atspi_text_set_caret_offset(text,0,&error);
    result=result && !moved;if(error){g_error_free(error);error=NULL;}
    int selected=atspi_text_add_selection(text,0,1,&error);
    result=result && !selected;if(error){g_error_free(error);error=NULL;}
    actual=atspi_text_get_text(text,0,-1,&error);
    result=result && !error && actual && !strcmp(actual,expected);g_free(actual);
    if(!error)result=result && atspi_text_get_caret_offset(text,&error)==-1;
    if(!error)result=result && atspi_text_get_n_selections(text,&error)==0;
    if(error){g_error_free(error);result=0;}
    AtspiEditableText *editable=atspi_accessible_get_editable_text_iface(node);
    result=result && !editable;if(editable)g_object_unref(editable);
    g_object_unref(text);return result;
}
static int retired_text(AtspiText *text){
    GError *error=NULL;char *value=atspi_text_get_text(text,0,-1,&error);
    int result=error && (!value || !value[0]);g_free(value);
    if(error)g_error_free(error);
    return result;
}
static int value(AtspiAccessible *node,const char *expected){
    GError *error=NULL;atspi_accessible_clear_cache(node);GHashTable *attrs=atspi_accessible_get_attributes(node,&error);
    int result=!error && attrs && !g_strcmp0(g_hash_table_lookup(attrs,"nexaui-value"),expected);
    if(attrs)g_hash_table_unref(attrs);
    if(error)g_error_free(error);
    return result && (!expected || readable_text(node,expected));
}
static int activate(AtspiAccessible *node){
    GError *error=NULL;AtspiAction *action=atspi_accessible_get_action_iface(node);
    int result=action && atspi_action_do_action(action,0,&error) && !error;
    if(action)g_object_unref(action);
    if(error)g_error_free(error);
    return result;
}
static int invoke(AtspiAccessible *node,const char *wanted){
    GError *error=NULL;AtspiAction *action=atspi_accessible_get_action_iface(node);int okay=0;
    int count=action?atspi_action_get_n_actions(action,&error):0;
    for(int i=0;i<count && !error;++i){
        char *name=atspi_action_get_name(action,i,&error);int match=name && !strcmp(name,wanted);g_free(name);
        if(match && !error){okay=atspi_action_do_action(action,i,&error) && !error;break;}
    }
    if(action)g_object_unref(action);
    if(error)g_error_free(error);
    return okay;
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
    AtspiText *held_text=NULL;
    const char *font=getenv("NEXAUI_REMOVAL_FONT");int incomplete=getenv("NEXAUI_REMOVAL_INCOMPLETE")!=NULL;
    int keyboard=getenv("NEXAUI_REMOVAL_KEYBOARD")!=NULL;setvbuf(stdout,NULL,_IONBF,0);
    int large=getenv("NEXAUI_REMOVAL_LARGE")!=NULL;
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
    held_text=atspi_accessible_get_text_iface(old_plan);CHECK(held_text);
    AtspiAccessible *installation=find(app,"Installation",0),*scope=find(app,"Scope",0);
    CHECK(installation && scope && value(installation,"00000000000000000000000000000002") &&
        value(scope,"Selected user's managed installation (00000000000000000000000000000003)"));
    g_object_unref(installation);g_object_unref(scope);
    CHECK(!state(request,ATSPI_STATE_ENABLED) && !activate(request));
    const char *groups[]={"Remove in this scope","Retained: shared dependency","Retained: explicit choice",
        "Independent documents","Backups or remote copies","Unknown or unmanaged"};
    for(unsigned i=0;i<6;++i){AtspiAccessible *group=find(app,groups[i],0);CHECK(group);
        const char *expected=large?(i==5?(incomplete?"1 object":"0 objects"):i==4 && incomplete?"19999 objects":"20000 objects"):
            i==0?"4 objects":i==5?"0 objects":"1 object";
        CHECK(value(group,expected));g_object_unref(group);}
    const char *objects[]={"Application code","Private settings and cache","Credential registration","Owned background service",
        "Shared support library","Profile deliberately kept","Independent project document","Backup outside this action"};
    const char *reasons[]={"Owned by this installation","Managed private state in the selected user scope",
        "Revoke the app-owned credential registration","Stop and unregister only this app's background work",
        "Another installed application still requires this component","The user explicitly chose to retain this profile",
        "User-created document outside automatic app-state removal","This request does not remove backup or remote copies"};
    if(!large)for(unsigned i=0;i<8;++i){
        AtspiAccessible *object=find(app,objects[i],0);char expected[512];CHECK(object);
        snprintf(expected,sizeof(expected),"%s. %s\nObject: %032x\nLogical bytes: %u",
            groups[i<4?0:i-3],reasons[i],10+i,4096*(i+1));
        CHECK(value(object,expected));g_object_unref(object);
    }
    if(large && !incomplete){
        AtspiAccessible *scroll=find(app,"Vertical scrollbar",0),*object=NULL;CHECK(scroll);
        if(keyboard){
            CHECK(state(reviewed,ATSPI_STATE_FOCUSED));puts("REMOVAL_KEY_LARGE_FOCUS_SCROLL");
            for(unsigned i=0;i<100 && !state(scroll,ATSPI_STATE_FOCUSED);++i)pause_briefly();
            CHECK(state(scroll,ATSPI_STATE_FOCUSED));puts("REMOVAL_KEY_LARGE_END");
            for(unsigned i=0;i<100 && !object;++i){object=find(app,"managed-object-099999",0);if(!object)pause_briefly();}
            CHECK(object && value(object,"Backups or remote copies. This request does not remove backup or remote copies\nObject: 000000000000000000000000000186a0\nLogical bytes: 32768"));
            puts("REMOVAL_KEY_LARGE_FOCUS_ACK");
            for(unsigned i=0;i<100 && !state(reviewed,ATSPI_STATE_FOCUSED);++i)pause_briefly();
            CHECK(state(reviewed,ATSPI_STATE_FOCUSED));
        } else {
            CHECK(invoke(scroll,"increment"));
            for(unsigned i=0;i<100 && !object;++i){object=find(app,"managed-object-000000",0);if(!object)pause_briefly();}
            CHECK(object);
        }
        AtspiAccessible *preview=find(app,"Complete removal preview",0);GError *error=NULL;CHECK(preview);
        int children=atspi_accessible_get_child_count(preview,&error);CHECK(!error && children>0 && children<300);
        g_object_unref(preview);g_object_unref(object);g_object_unref(scroll);
    }
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
        CHECK(request && !state(request,ATSPI_STATE_ENABLED) && state(old_plan,ATSPI_STATE_DEFUNCT) && retired_text(held_text));
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
    if(held_text)g_object_unref(held_text);
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
