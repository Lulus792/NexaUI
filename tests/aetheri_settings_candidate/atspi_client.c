#define _POSIX_C_SOURCE 200809L
/* Independent test process; no Aetheri or NexaUI linkage. */
#include <atspi/atspi.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"settings-candidate:%d: %s\n",__LINE__,#x); goto done; } } while (0)
static void pump(void) { while(g_main_context_iteration(NULL,FALSE)){} struct timespec t={0,10000000};nanosleep(&t,NULL); }
static AtspiAccessible *find(AtspiAccessible *node,const char *name,unsigned depth) {
    if(!node || depth>20)return NULL;
    GError *error=NULL;atspi_accessible_clear_cache(node);char *got=atspi_accessible_get_name(node,&error);
    int found=!error && got && !strcmp(got,name);if(error)g_error_free(error);g_free(got);
    if(found)return g_object_ref(node);
    error=NULL;int n=atspi_accessible_get_child_count(node,&error);
    if(error){g_error_free(error);return NULL;}
    for(int i=0;i<n;++i) {
        AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,&error);
        if(error){g_error_free(error);error=NULL;continue;}
        AtspiAccessible *match=find(child,name,depth+1);if(child)g_object_unref(child);if(match)return match;
    }
    return NULL;
}
static AtspiAccessible *app_for_pid(AtspiAccessible *desktop,pid_t pid) {
    GError *error=NULL;atspi_accessible_clear_cache(desktop);
    int n=atspi_accessible_get_child_count(desktop,&error);if(error){g_error_free(error);return NULL;}
    for(int i=0;i<n;++i) {
        AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,&error);
        if(error){g_error_free(error);error=NULL;continue;}if(!app)continue;
        guint actual=atspi_accessible_get_process_id(app,&error);
        if(!error && actual==(guint)pid)return app;
        if(error){g_error_free(error);error=NULL;}g_object_unref(app);
    }
    return NULL;
}
static int press(AtspiAccessible *app,const char *name,int radio) {
    AtspiAccessible *node=NULL;
    for(unsigned i=0;i<300 && !node;++i){node=find(app,name,0);if(!node)pump();}
    if(!node)return 0;
    GError *error=NULL;AtspiRole role=atspi_accessible_get_role(node,&error);
    AtspiAction *action=!error?atspi_accessible_get_action_iface(node):NULL;
    int okay=!error && role==(radio?ATSPI_ROLE_RADIO_BUTTON:ATSPI_ROLE_PUSH_BUTTON) && action &&
        atspi_action_do_action(action,0,&error) && !error;
    if(okay && radio) {
        okay=0;
        for(unsigned i=0;i<300 && !okay;++i) {
            pump();atspi_accessible_clear_cache(node);AtspiStateSet *states=atspi_accessible_get_state_set(node);
            if(states){okay=atspi_state_set_contains(states,ATSPI_STATE_CHECKED);g_object_unref(states);}
        }
    }
    if(error)g_error_free(error);
    if(action)g_object_unref(action);
    g_object_unref(node);return okay;
}
int main(int argc,char **argv) {
    int okay=0,initialized=0;pid_t child=-1;AtspiAccessible *desktop=NULL,*app=NULL;FILE *log=NULL;
    const char *font=getenv("NEXAUI_AETHERI_SETTINGS_FONT");
    CHECK(argc==2 && font && *font && (log=tmpfile()));child=fork();CHECK(child>=0);
    if(!child){if(dup2(fileno(log),STDOUT_FILENO)<0)_exit(126);
        if(getenv("NEXAUI_SETTINGS_DISABLE_AX") && setenv("NO_AT_BRIDGE","1",1))_exit(126);
        execl(argv[1],argv[1],"--font",font,"--fixture-displays","--diagnostics",(char *)NULL);_exit(127);}
    CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);
    desktop=atspi_get_desktop(0);CHECK(desktop);
    for(unsigned i=0;i<500 && !app;++i){app=app_for_pid(desktop,child);if(!app)pump();}
    if(getenv("NEXAUI_SETTINGS_DISABLE_AX")) {
        CHECK(!app && !fseek(log,0,SEEK_SET));
        char disabled_output[32768];size_t count=fread(disabled_output,1,sizeof(disabled_output)-1,log);
        disabled_output[count]=0;
        CHECK(strstr(disabled_output,"FRAME ") && strstr(disabled_output,"MODEL theme=0 german=0 text=100"));
        CHECK(kill(child,SIGTERM)==0);int disabled_status=0;
        CHECK(waitpid(child,&disabled_status,0)==child && WIFEXITED(disabled_status) && WEXITSTATUS(disabled_status)==0);
        child=-1;okay=1;puts("Disabled AT-SPI: rendered unchanged model remained undiscoverable");goto done;
    }
    CHECK(app && press(app,"Appearance",0) && press(app,"High contrast",1) && press(app,"200 %",1));
    CHECK(press(app,"Language",0) && press(app,"German",1));
    if(getenv("NEXAUI_SETTINGS_SOUND_PAGE")) {
        CHECK(press(app,"Ton",0));
        AtspiAccessible *unavailable=NULL;
        for(unsigned i=0;i<300 && !unavailable;++i) {
            unavailable=find(app,"Audio ist in dieser Sitzung nicht verfügbar.",0);
            if(!unavailable)pump();
        }
        CHECK(unavailable);
        GError *error=NULL;
        AtspiRole role=atspi_accessible_get_role(unavailable,&error);
        int named_text=!error && role==ATSPI_ROLE_LABEL;
        if(error)g_error_free(error);
        g_object_unref(unavailable);CHECK(named_text);
    }
    CHECK(press(app,"Schließen",0));
    int status=0;pid_t waited=0;
    for(unsigned i=0;i<500 && !waited;++i){waited=waitpid(child,&status,WNOHANG);if(!waited)pump();}
    CHECK(waited==child && WIFEXITED(status) && WEXITSTATUS(status)==0);child=-1;
    char output[65536];CHECK(!fseek(log,0,SEEK_SET));size_t used=fread(output,1,sizeof(output)-1,log);
    output[used]=0;CHECK(strstr(output,"MODEL theme=2 german=1 text=200") && strstr(output,"FRAME "));
    if(getenv("NEXAUI_SETTINGS_SOUND_PAGE"))
        CHECK(strstr(output,"MODEL theme=2 german=1 text=200 page=3"));
    okay=1;
done:
    if(child>0){kill(child,SIGKILL);waitpid(child,NULL,0);}
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    if(log)fclose(log);
    if(initialized)atspi_exit();
    if(okay && !getenv("NEXAUI_SETTINGS_DISABLE_AX"))
        puts("Aetheri-owned Settings: independent AT-SPI model changes and close passed");
    return okay?0:1;
}
