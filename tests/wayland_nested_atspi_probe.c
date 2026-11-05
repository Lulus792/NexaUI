#define _POSIX_C_SOURCE 200809L
#include <atspi/atspi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static AtspiAccessible *find(AtspiAccessible *node,const char *name,unsigned depth) {
    if(!node || depth>16)return NULL;
    GError *error=NULL;char *current=atspi_accessible_get_name(node,&error);
    int match=!error && current && !strcmp(current,name);g_free(current);g_clear_error(&error);
    if(match)return g_object_ref(node);
    int count=atspi_accessible_get_child_count(node,&error);
    if(error){g_clear_error(&error);return NULL;}
    for(int i=0;i<count;++i) {
        AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,&error);
        if(error){g_clear_error(&error);continue;}
        AtspiAccessible *found=find(child,name,depth+1);
        if(child)g_object_unref(child);
        if(found)return found;
    }
    return NULL;
}
static int has(AtspiAccessible *node,AtspiStateType state) {
    AtspiStateSet *states=atspi_accessible_get_state_set(node);
    int result=states && atspi_state_set_contains(states,state);
    if(states)g_object_unref(states);
    return result;
}
int main(int argc,char **argv) {
    if(argc!=3 || (strcmp(argv[2],"dialog") && strcmp(argv[2],"menu")))return 2;
    long pid=strtol(argv[1],NULL,10);if(pid<=0 || atspi_init()!=0)return 1;
    atspi_set_timeout(1000,1000);AtspiAccessible *desktop=atspi_get_desktop(0),*app=NULL;
    for(unsigned attempt=0;desktop && !app && attempt<150;++attempt) {
        atspi_accessible_clear_cache(desktop);GError *error=NULL;
        int count=atspi_accessible_get_child_count(desktop,&error);g_clear_error(&error);
        for(int i=0;i<count;++i) {
            AtspiAccessible *child=atspi_accessible_get_child_at_index(desktop,i,&error);
            if(error){g_clear_error(&error);continue;}
            guint actual=child?atspi_accessible_get_process_id(child,&error):0;
            if(!error && actual==(guint)pid){app=child;break;}
            g_clear_error(&error);if(child)g_object_unref(child);
        }
        if(!app){struct timespec delay={0,20000000};nanosleep(&delay,NULL);}
    }
    AtspiAccessible *trigger=app?find(app,"Open menu",0):NULL;
    AtspiAccessible *item=app && !strcmp(argv[2],"menu")?find(app,"First action",0):NULL;
    int okay=trigger && (!strcmp(argv[2],"dialog") ?
        has(trigger,ATSPI_STATE_ENABLED) && has(trigger,ATSPI_STATE_FOCUSED) :
        item && !has(trigger,ATSPI_STATE_ENABLED) && has(item,ATSPI_STATE_ENABLED) && has(item,ATSPI_STATE_FOCUSED));
    if(!okay)fprintf(stderr,"AT-SPI state mismatch in %s\n",argv[2]);
    if(item)g_object_unref(item);
    if(trigger)g_object_unref(trigger);
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    atspi_exit();
    return okay?0:1;
}
