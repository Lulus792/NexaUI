#define _POSIX_C_SOURCE 200809L
/* Independent native client: no NexaUI or Aetheri headers/linkage. */
#include <atspi/atspi.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void pump(void){while(g_main_context_iteration(NULL,FALSE)){}struct timespec t={0,10000000};nanosleep(&t,NULL);}
static AtspiAccessible *find(AtspiAccessible *node,const char *name,unsigned depth){
    if(!node || depth>20)return NULL;
    GError *error=NULL;char *text=atspi_accessible_get_name(node,&error);
    int matches=!error && text && !strcmp(text,name);g_free(text);
    if(error){g_error_free(error);error=NULL;}
    if(matches)return g_object_ref(node);
    int count=atspi_accessible_get_child_count(node,&error);
    if(error){g_error_free(error);return NULL;}
    for(int i=0;i<count;++i){AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,&error);
        if(error){g_error_free(error);error=NULL;continue;}
        AtspiAccessible *result=find(child,name,depth+1);if(child)g_object_unref(child);if(result)return result;}
    return NULL;
}
static AtspiAccessible *by_pid(AtspiAccessible *desktop,pid_t pid){
    GError *error=NULL;atspi_accessible_clear_cache(desktop);int count=atspi_accessible_get_child_count(desktop,&error);
    if(error){g_error_free(error);return NULL;}
    for(int i=0;i<count;++i){AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,&error);
        if(error){g_error_free(error);error=NULL;continue;}if(!app)continue;
        guint actual=atspi_accessible_get_process_id(app,&error);
        if(!error && actual==(guint)pid)return app;
        if(error){g_error_free(error);error=NULL;}g_object_unref(app);}
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
    GError *error=NULL;atspi_accessible_clear_cache(node);
    GHashTable *attrs=atspi_accessible_get_attributes(node,&error);
    int result=!error && attrs && !g_strcmp0(g_hash_table_lookup(attrs,"nexaui-value"),expected);
    if(attrs)g_hash_table_unref(attrs);
    if(error)g_error_free(error);
    return result && (!expected || readable_text(node,expected));
}
static int response(FILE *responses,const char *expected){
    struct pollfd fd={fileno(responses),POLLIN,0};char line[160];
    if(poll(&fd,1,10000)<=0 || !fgets(line,sizeof(line),responses))return 0;
    printf("HOST %s",line);fflush(stdout);return !strcmp(line,expected);
}
static int activate(AtspiAccessible *node){
    GError *error=NULL;AtspiAction *action=atspi_accessible_get_action_iface(node);
    int result=action && atspi_action_do_action(action,0,&error) && !error;
    if(action)g_object_unref(action);
    if(error)g_error_free(error);
    return result;
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"operation-atspi:%d: %s\n",__LINE__,#x);goto cleanup;}}while(0)
int main(int argc,char **argv){
    int ok=0,in[2]={-1,-1},out[2]={-1,-1};pid_t child=-1;FILE *commands=NULL,*responses=NULL;
    AtspiAccessible *desktop=NULL,*app=NULL,*status=NULL,*operation=NULL,*component=NULL,*refresh=NULL,*close_view=NULL,*old_operation=NULL;
    AtspiText *held_text=NULL;
    int keyboard=getenv("NEXAUI_OPERATION_KEYBOARD")!=NULL;
    const char *manifest=getenv("NEXAUI_OPERATION_MANIFEST"),*expected=getenv("NEXAUI_OPERATION_EXPECTED"),
        *catalogue=getenv("NEXAUI_OPERATION_CATALOGUE"),*font=getenv("NEXAUI_OPERATION_FONT");
    CHECK(argc==2 && manifest && expected && catalogue && font && !pipe(in) && !pipe(out));
    child=fork();CHECK(child>=0);
    if(!child){
        if(dup2(in[0],STDIN_FILENO)<0 || dup2(out[1],STDOUT_FILENO)<0)_exit(126);
        close(in[0]);close(in[1]);close(out[0]);close(out[1]);
        execl(argv[1],argv[1],manifest,expected,catalogue,font,(char *)NULL);_exit(127);
    }
    close(in[0]);in[0]=-1;close(out[1]);out[1]=-1;
    commands=fdopen(in[1],"w");responses=fdopen(out[0],"r");CHECK(commands && responses);in[1]=out[0]=-1;
    setvbuf(responses,NULL,_IONBF,0);CHECK(response(responses,"OBS_READY\n"));
    CHECK(atspi_init()==0);atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    for(unsigned i=0;i<500 && !status;++i){
        pump();if(!app)app=by_pid(desktop,child);
        if(app){atspi_accessible_clear_cache(app);status=find(app,"Staging in progress; no completion estimate",0);}
    }
    CHECK(status);GError *error=NULL;
    CHECK(atspi_accessible_get_role(status,&error)==ATSPI_ROLE_STATUS_BAR && !error &&
        state(status,ATSPI_STATE_BUSY) && state(status,ATSPI_STATE_READ_ONLY));
    CHECK(!atspi_accessible_get_value_iface(status) && !atspi_accessible_get_action_iface(status) && value(status,NULL));
    operation=find(app,"Operation",0);component=find(app,"Component",0);refresh=find(app,"Refresh status",0);
    CHECK(operation && component && refresh && value(operation,"0000000000004000800000000000000b") && value(component,"aetheri.p0.probe"));
    held_text=atspi_accessible_get_text_iface(operation);CHECK(held_text);
    CHECK(state(refresh,ATSPI_STATE_FOCUSED));
    if(keyboard){puts("OPERATION_KEYBOARD_REFRESH");fflush(stdout);}else CHECK(activate(refresh));
    CHECK(response(responses,"OBS_REFRESH\n"));
    CHECK(fprintf(commands,"uncertain\n")>0 && !fflush(commands));
    CHECK(response(responses,"OBS_REOPENED\n"));
    old_operation=operation;operation=NULL;
    g_object_unref(status);status=NULL;g_object_unref(component);component=NULL;g_object_unref(refresh);refresh=NULL;
    g_object_unref(app);app=NULL;
    for(unsigned i=0;i<500 && !status;++i){
        pump();if(app){g_object_unref(app);app=NULL;}app=by_pid(desktop,child);
        if(app){atspi_accessible_clear_cache(app);status=find(app,"Outcome uncertain; retain the original operation",0);}
    }
    CHECK(status && !state(status,ATSPI_STATE_BUSY) && state(status,ATSPI_STATE_READ_ONLY) && value(status,NULL));
    operation=find(app,"Operation",0);component=find(app,"Component",0);refresh=find(app,"Refresh status",0);close_view=find(app,"Close view",0);
    CHECK(operation && component && refresh && close_view && value(operation,"0000000000004000800000000000000b") && value(component,"aetheri.p0.probe"));
    CHECK((strcmp(ATSPI_OBJECT(operation)->path,ATSPI_OBJECT(old_operation)->path) ||
        strcmp(ATSPI_OBJECT(operation)->app->bus_name,ATSPI_OBJECT(old_operation)->app->bus_name)) && state(old_operation,ATSPI_STATE_DEFUNCT) && retired_text(held_text));
    CHECK(state(refresh,ATSPI_STATE_FOCUSED));
    if(keyboard){
        puts("OPERATION_KEYBOARD_FOCUS_CLOSE");fflush(stdout);
        int focused=0;for(unsigned i=0;i<500 && !focused;++i){pump();focused=state(close_view,ATSPI_STATE_FOCUSED);}CHECK(focused);
        puts("OPERATION_KEYBOARD_ACTIVATE_CLOSE");fflush(stdout);
    }else CHECK(activate(close_view));
    CHECK(response(responses,"OBS_CLOSED accepts=1 refreshes=1 reopened=1\n"));
    int child_status;CHECK(waitpid(child,&child_status,0)==child && WIFEXITED(child_status) && WEXITSTATUS(child_status)==0);child=-1;
    puts("Independent operation AT-SPI identity, busy/unknown, retired reference, refresh and clean close passed");ok=1;
cleanup:
    if(commands)fclose(commands);
    if(responses)fclose(responses);
    for(unsigned i=0;i<2;++i){if(in[i]>=0)close(in[i]);if(out[i]>=0)close(out[i]);}
    if(child>0){kill(child,SIGTERM);waitpid(child,NULL,0);}
    if(held_text)g_object_unref(held_text);
    if(old_operation)g_object_unref(old_operation);
    if(close_view)g_object_unref(close_view);
    if(refresh)g_object_unref(refresh);
    if(component)g_object_unref(component);
    if(operation)g_object_unref(operation);
    if(status)g_object_unref(status);
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    atspi_exit();return ok?0:1;
}
