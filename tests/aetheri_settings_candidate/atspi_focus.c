#define _POSIX_C_SOURCE 200809L
/* Read-only independent AT-SPI observer of the test-owned process. */
#include <atspi/atspi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void pump(void) {while(g_main_context_iteration(NULL,FALSE)){}struct timespec t={0,10000000};nanosleep(&t,NULL);}
static AtspiAccessible *find(AtspiAccessible *node,const char *name,AtspiRole wanted,unsigned depth) {
    if(!node || depth>20)return NULL;
    GError *error=NULL;atspi_accessible_clear_cache(node);char *text=atspi_accessible_get_name(node,&error);
    int match=!error && text && !strcmp(text,name);if(error)g_error_free(error);g_free(text);
    if(match){error=NULL;AtspiRole role=atspi_accessible_get_role(node,&error);match=!error && role==wanted;if(error)g_error_free(error);}
    if(match)return g_object_ref(node);
    error=NULL;int count=atspi_accessible_get_child_count(node,&error);if(error){g_error_free(error);return NULL;}
    for(int i=0;i<count;++i){AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,&error);
        if(error){g_error_free(error);error=NULL;continue;}
        AtspiAccessible *result=find(child,name,wanted,depth+1);if(child)g_object_unref(child);if(result)return result;}
    return NULL;
}
static AtspiAccessible *by_pid(AtspiAccessible *desktop,unsigned long pid) {
    GError *error=NULL;atspi_accessible_clear_cache(desktop);int count=atspi_accessible_get_child_count(desktop,&error);
    if(error){g_error_free(error);return NULL;}
    for(int i=0;i<count;++i){AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,&error);
        if(error){g_error_free(error);error=NULL;continue;}if(!app)continue;
        guint actual=atspi_accessible_get_process_id(app,&error);if(!error && actual==pid)return app;
        if(error){g_error_free(error);error=NULL;}g_object_unref(app);}
    return NULL;
}
int main(int argc,char **argv) {
    if(argc!=5)return 2;
    char *end=NULL;unsigned long pid=strtoul(argv[1],&end,10);
    if(!pid || !end || *end || (strcmp(argv[3],"button") && strcmp(argv[3],"radio")) ||
       (strcmp(argv[4],"any") && strcmp(argv[4],"checked") && strcmp(argv[4],"unchecked")))return 2;
    if(atspi_init()!=0)return 1;
    atspi_set_timeout(1000,1000);
    AtspiAccessible *desktop=atspi_get_desktop(0);int okay=0;
    for(unsigned attempt=0;desktop && attempt<200 && !okay;++attempt) {
        AtspiAccessible *app=by_pid(desktop,pid);
        AtspiAccessible *node=app?find(app,argv[2],!strcmp(argv[3],"radio")?ATSPI_ROLE_RADIO_BUTTON:ATSPI_ROLE_PUSH_BUTTON,0):NULL;
        if(node) {
            atspi_accessible_clear_cache(node);AtspiStateSet *states=atspi_accessible_get_state_set(node);
            if(states) {
                int checked=atspi_state_set_contains(states,ATSPI_STATE_CHECKED);
                okay=atspi_state_set_contains(states,ATSPI_STATE_FOCUSED) &&
                    atspi_state_set_contains(states,ATSPI_STATE_ENABLED) &&
                    (!strcmp(argv[4],"any") || checked==!strcmp(argv[4],"checked"));
                g_object_unref(states);
            }
            g_object_unref(node);
        }
        if(app)g_object_unref(app);
        if(!okay)pump();
    }
    if(desktop)g_object_unref(desktop);
    atspi_exit();
    if(!okay)fprintf(stderr,"AT-SPI focus/state not observed: %s (%s, %s)\n",argv[2],argv[3],argv[4]);
    return okay?0:1;
}
