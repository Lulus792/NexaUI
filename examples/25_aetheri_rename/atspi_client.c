#define _POSIX_C_SOURCE 200809L
/* Independent libatspi test; no NexaUI headers or linkage. */
#include <atspi/atspi.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"rename-atspi:%d: %s\n",__LINE__,#x);goto cleanup;}}while(0)
static void pause_ms(void){struct timespec t={0,10000000};(void)nanosleep(&t,NULL);}
static AtspiAccessible *find(AtspiAccessible *node,const char *name,unsigned depth){
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
        AtspiAccessible *found=find(child,name,depth+1);if(child)g_object_unref(child);if(found)return found;}
    return NULL;
}
static AtspiAccessible *by_pid(AtspiAccessible *desktop,pid_t pid){
    GError *error=NULL;atspi_accessible_clear_cache(desktop);int count=atspi_accessible_get_child_count(desktop,&error);
    if(error){g_error_free(error);return NULL;}
    for(int i=0;i<count;++i){AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,&error);
        if(error){g_error_free(error);error=NULL;continue;}if(!app)continue;
        guint actual=atspi_accessible_get_process_id(app,&error);if(!error && actual==(guint)pid)return app;
        if(error){g_error_free(error);error=NULL;}g_object_unref(app);}
    return NULL;
}
static int enabled(AtspiAccessible *node){atspi_accessible_clear_cache(node);AtspiStateSet *s=atspi_accessible_get_state_set(node);
    int result=s && atspi_state_set_contains(s,ATSPI_STATE_ENABLED);if(s)g_object_unref(s);return result;}
static int value(AtspiAccessible *node,const char *expected){GError *error=NULL;atspi_accessible_clear_cache(node);
    GHashTable *a=atspi_accessible_get_attributes(node,&error);int result=!error && a && !g_strcmp0(g_hash_table_lookup(a,"nexaui-value"),expected);
    if(a)g_hash_table_unref(a);
    if(error)g_error_free(error);
    return result;}
static int percent(AtspiAccessible *node){GError *error=NULL;atspi_accessible_clear_cache(node);
    GHashTable *a=atspi_accessible_get_attributes(node,&error);const char *text=a?g_hash_table_lookup(a,"nexaui-value"):NULL;
    int result=-1;if(!error && text){char *end=NULL;long n=strtol(text,&end,10);if(end && !strcmp(end,"%") && n>=0 && n<=100)result=(int)n;}
    if(a)g_hash_table_unref(a);
    if(error)g_error_free(error);
    return result;}
int main(int argc,char **argv){
    int ok=0,dir=-1,pipes[2]={-1,-1},created=0;pid_t child=-1;char path[]="/tmp/nexaui-rename-atspi-XXXXXX";
    AtspiAccessible *desktop=NULL,*app=NULL,*dialog=NULL,*background=NULL,*confirm=NULL,*status=NULL,*cancel=NULL,*new_name=NULL,*scrollbar=NULL,*next_page=NULL,*previous_page=NULL;
    AtspiAction *confirm_action=NULL,*cancel_action=NULL,*background_action=NULL,*next_action=NULL,*previous_action=NULL;const char *font=getenv("NEXAUI_RENAME_FONT");
    int long_mode=getenv("NEXAUI_RENAME_LONG_TEST")!=NULL;char long_name[256];memset(long_name,'x',255);long_name[255]=0;
    CHECK(argc==2 && font && mkdtemp(path));created=1;dir=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(dir>=0);
    int fd=openat(dir,"old.txt",O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600);CHECK(fd>=0 && write(fd,"data",4)==4 && !close(fd));
    CHECK(!pipe(pipes));child=fork();CHECK(child>=0);
    if(!child){close(pipes[0]);if(dup2(pipes[1],STDOUT_FILENO)<0)_exit(126);close(pipes[1]);execl(argv[1],argv[1],path,font,(char *)NULL);_exit(127);}
    close(pipes[1]);pipes[1]=-1;CHECK(atspi_init()==0);atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    for(unsigned i=0;i<500 && (!dialog || !background || !confirm || !status);++i){
        while(g_main_context_iteration(NULL,FALSE)){}if(!app)app=by_pid(desktop,child);
        if(app){atspi_accessible_clear_cache(app);if(!dialog)dialog=find(app,"Rename review",0);
            if(!background)background=find(app,"Files",0);
            if(!confirm)confirm=find(app,"Confirm",0);
            if(!status)status=find(app,"Rename status",0);}
        if(!dialog || !background || !confirm || !status)pause_ms();}
    CHECK(dialog && background && confirm && status);GError *error=NULL;
    CHECK(atspi_accessible_get_role(dialog,&error)==ATSPI_ROLE_DIALOG && !error);
    CHECK(!enabled(background));
    if(long_mode){
        CHECK(!enabled(confirm) && value(status,"Scroll to review both names"));
        new_name=find(app,"New name",0);scrollbar=find(app,"Vertical scrollbar",0);
        CHECK(new_name && value(new_name,long_name) && scrollbar &&
              atspi_accessible_get_role(scrollbar,&error)==ATSPI_ROLE_SLIDER && !error);
        next_page=find(app,"Next page",0);previous_page=find(app,"Previous page",0);
        CHECK(next_page && previous_page && enabled(next_page) && !enabled(previous_page));
    }else CHECK(enabled(confirm) && value(status,"Ready for confirmation"));
    background_action=atspi_accessible_get_action_iface(background);
    if(background_action && atspi_action_get_n_actions(background_action,&error)>0 && !error){
        CHECK(!atspi_action_do_action(background_action,0,&error));if(error){g_error_free(error);error=NULL;}}
    CHECK(!error);
    if(long_mode){
        next_action=atspi_accessible_get_action_iface(next_page);CHECK(next_action);
        CHECK(atspi_action_get_n_actions(next_action,&error)>0 && !error);
        for(unsigned step=0;step<16 && !enabled(confirm);++step){
            int before=percent(scrollbar);CHECK(before>=0 && enabled(next_page));
            CHECK(atspi_action_do_action(next_action,0,&error) && !error);
            int moved=0;for(unsigned i=0;i<200 && !moved;++i){while(g_main_context_iteration(NULL,FALSE)){}
                moved=percent(scrollbar)>before || enabled(confirm);if(!moved)pause_ms();}
            CHECK(moved);
        }
        CHECK(enabled(confirm) && value(status,"Ready for confirmation") && enabled(previous_page));
        int before=percent(scrollbar);previous_action=atspi_accessible_get_action_iface(previous_page);CHECK(previous_action);
        CHECK(atspi_action_get_n_actions(previous_action,&error)>0 && !error);
        CHECK(atspi_action_do_action(previous_action,0,&error) && !error);
        int moved=0;for(unsigned i=0;i<200 && !moved;++i){while(g_main_context_iteration(NULL,FALSE)){}
            int after=percent(scrollbar);moved=after>=0 && after<before;if(!moved)pause_ms();}
        CHECK(moved && enabled(confirm));
    }
    confirm_action=atspi_accessible_get_action_iface(confirm);CHECK(confirm_action);
    CHECK(atspi_action_get_n_actions(confirm_action,&error)>0 && !error);
    CHECK(atspi_action_do_action(confirm_action,0,&error) && !error);
    g_object_unref(dialog);dialog=NULL;g_object_unref(background);background=NULL;g_object_unref(confirm);confirm=NULL;
    g_object_unref(status);status=NULL;g_object_unref(app);app=NULL;
    gint64 deadline=g_get_monotonic_time()+8*G_TIME_SPAN_SECOND;
    for(unsigned i=0;i<500 && g_get_monotonic_time()<deadline;++i){
        while(g_main_context_iteration(NULL,FALSE)){}
        if(status){g_object_unref(status);status=NULL;}if(confirm){g_object_unref(confirm);confirm=NULL;}
        if(cancel){g_object_unref(cancel);cancel=NULL;}if(app){g_object_unref(app);app=NULL;}
        app=by_pid(desktop,child);
        if(app){atspi_accessible_clear_cache(app);status=find(app,"Rename status",0);
            confirm=find(app,"Confirm",0);cancel=find(app,"Cancel",0);}
        if(status && confirm && cancel && value(status,"Result unknown; check the original operation") && !enabled(confirm))break;
        pause_ms();}
    CHECK(status && confirm && cancel && value(status,"Result unknown; check the original operation") && !enabled(confirm));
    struct stat st;CHECK(fstatat(dir,"old.txt",&st,AT_SYMLINK_NOFOLLOW)<0 && errno==ENOENT &&
                         !fstatat(dir,long_mode?long_name:"new.txt",&st,AT_SYMLINK_NOFOLLOW));
    cancel_action=atspi_accessible_get_action_iface(cancel);CHECK(cancel_action);
    CHECK(atspi_action_get_n_actions(cancel_action,&error)>0 && !error);
    CHECK(atspi_action_do_action(cancel_action,0,&error) && !error);
    int status_code=0;pid_t waited=0;for(unsigned i=0;i<500 && !waited;++i){waited=waitpid(child,&status_code,WNOHANG);if(!waited)pause_ms();}
    CHECK(waited==child && WIFEXITED(status_code) && WEXITSTATUS(status_code)==0);child=-1;
    char output[256]={0};ssize_t n=read(pipes[0],output,sizeof(output)-1);CHECK(n>0);
    CHECK(strstr(output,"RENAME_NATIVE_REOPENED_UNKNOWN\n") && strstr(output,"RENAME_NATIVE_CLOSED 1 1\n"));
    ok=1;
cleanup:
    if(child>0){(void)kill(child,SIGKILL);(void)waitpid(child,NULL,0);}
    if(confirm_action)g_object_unref(confirm_action);
    if(cancel_action)g_object_unref(cancel_action);
    if(background_action)g_object_unref(background_action);
    if(next_action)g_object_unref(next_action);
    if(previous_action)g_object_unref(previous_action);
    if(dialog)g_object_unref(dialog);
    if(background)g_object_unref(background);
    if(new_name)g_object_unref(new_name);
    if(scrollbar)g_object_unref(scrollbar);
    if(next_page)g_object_unref(next_page);
    if(previous_page)g_object_unref(previous_page);
    if(confirm)g_object_unref(confirm);
    if(status)g_object_unref(status);
    if(cancel)g_object_unref(cancel);
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    if(pipes[0]>=0)close(pipes[0]);
    if(pipes[1]>=0)close(pipes[1]);
    if(dir>=0){(void)unlinkat(dir,"old.txt",0);(void)unlinkat(dir,"new.txt",0);(void)unlinkat(dir,long_name,0);close(dir);}if(created)(void)rmdir(path);
    if(ok)puts("Native AT-SPI rename: modal blocking, unknown reopen and no replay passed");
    return ok?0:1;
}
