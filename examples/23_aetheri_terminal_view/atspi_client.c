#define _POSIX_C_SOURCE 200809L
/* Independent libatspi client: no NexaUI headers or linkage. */
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
#define CHECK(x) do {if(!(x)){fprintf(stderr,"terminal-atspi:%d: %s\n",__LINE__,#x);cleanup();return 1;}} while(0)
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
    AtspiAccessible *target=NULL;
    for(unsigned attempt=0;attempt<300 && !target;++attempt) {
        while(g_main_context_iteration(NULL,FALSE)){}atspi_accessible_clear_cache(desktop);
        GError *error=NULL;int count=atspi_accessible_get_child_count(desktop,&error);
        if(error){g_error_free(error);pause_ms();continue;}
        for(int i=0;i<count && !target;++i){AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,&error);
            if(error){g_error_free(error);error=NULL;continue;}if(!app)continue;
            guint pid=atspi_accessible_get_process_id(app,&error);
            if(!error && pid==(guint)child)target=find(app,"Visible terminal text",0);
            if(error){g_error_free(error);error=NULL;}g_object_unref(app);}
        if(!target)pause_ms();
    }
    CHECK(target);AtspiText *text=atspi_accessible_get_text_iface(target);
    if(getenv("NEXAUI_TERMINAL_AX_SCROLL")) {
        CHECK(text);GError *error=NULL;
        CHECK(atspi_text_get_character_count(text,&error)>0 && !error);
        char *value=atspi_text_get_text(text,0,-1,&error);
        CHECK(!error && value && value[0]=='1');g_free(value);
        CHECK(atspi_text_get_caret_offset(text,&error)==-1 && !error);
        CHECK(atspi_text_get_n_selections(text,&error)==0 && !error);
        CHECK(!atspi_text_set_caret_offset(text,0,&error) && !error);
        g_object_unref(text);g_object_unref(target);g_object_unref(desktop);cleanup();
        puts("Independent AT-SPI scrollback is readable without an invented caret");return 0;
    }
    CHECK(text);GError *error=NULL;
    int count=atspi_text_get_character_count(text,&error);CHECK(!error && count==26);
    char *value=atspi_text_get_text(text,0,4,&error);
    CHECK(!error && value && !strcmp(value,"A\xe7\x95\x8c" "e\xcc\x81"));g_free(value);
    CHECK(atspi_text_get_caret_offset(text,&error)==1 && !error);
    CHECK(atspi_text_get_n_selections(text,&error)==1 && !error);
    AtspiRange *range=atspi_text_get_selection(text,0,&error);
    CHECK(!error && range && range->start_offset==1 && range->end_offset==4);g_free(range);
    CHECK(!atspi_text_set_caret_offset(text,2,&error) && !error);
    CHECK(!atspi_text_set_selection(text,0,0,1,&error) && !error);
    if(getenv("NEXAUI_TERMINAL_AX_TRANSITION")) {
        CHECK(!kill(child,SIGUSR1));int caret=0;
        for(unsigned attempt=0;attempt<300;++attempt) {
            while(g_main_context_iteration(NULL,FALSE)){}
            atspi_accessible_clear_cache(target);
            caret=atspi_text_get_caret_offset(text,&error);
            CHECK(!error);if(caret==-1)break;pause_ms();
        }
        CHECK(caret==-1 && atspi_text_get_n_selections(text,&error)==0 && !error);
        value=atspi_text_get_text(text,0,-1,&error);CHECK(!error && value && value[0]=='1');g_free(value);
    }
    g_object_unref(text);g_object_unref(target);g_object_unref(desktop);cleanup();
    puts("Independent AT-SPI terminal model text and selection passed");return 0;
}
