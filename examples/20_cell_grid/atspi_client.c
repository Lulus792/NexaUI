#define _POSIX_C_SOURCE 200809L
/* Independent libatspi consumer: no NexaUI headers or linkage. */
#include <atspi/atspi.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static pid_t host=-1;
static void cleanup(void) { if(host>0){kill(host,SIGTERM);waitpid(host,NULL,0);host=-1;} }
#define CHECK(x) do { if(!(x)){fprintf(stderr,"cell-atspi:%d: %s\n",__LINE__,#x);cleanup();return 1;} } while(0)
static void pause_ms(void) { struct timespec t={0,10000000}; nanosleep(&t,NULL); }
static AtspiAccessible *find(AtspiAccessible *node,const char *name,unsigned depth) {
    if(!node || depth>16)return NULL;
    GError *error=NULL; char *actual=atspi_accessible_get_name(node,&error);
    int found=!error && actual && !strcmp(actual,name);
    if(error)g_error_free(error); g_free(actual);
    if(found)return g_object_ref(node);
    error=NULL; int count=atspi_accessible_get_child_count(node,&error);
    if(error){g_error_free(error);return NULL;}
    for(int i=0;i<count;++i) {
        error=NULL; AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,&error);
        if(error){g_error_free(error);continue;}
        AtspiAccessible *result=find(child,name,depth+1);
        if(child)g_object_unref(child); if(result)return result;
    }
    return NULL;
}
int main(int argc,char **argv) {
    const char *font=getenv("NEXAUI_CELL_GRID_FONT");
    CHECK(argc==2 && font);
    host=fork(); CHECK(host>=0);
    if(!host){execl(argv[1],argv[1],font,(char *)NULL);_exit(127);}
    CHECK(atspi_init()==0);
    AtspiAccessible *desktop=atspi_get_desktop(0),*target=NULL;
    CHECK(desktop);
    for(unsigned i=0;i<500 && !target;++i) {
        while(g_main_context_iteration(NULL,FALSE)){}
        atspi_accessible_clear_cache(desktop);
        target=find(desktop,"Visible terminal cells",0);
        if(!target)pause_ms();
    }
    CHECK(target);
    AtspiText *text=atspi_accessible_get_text_iface(target); CHECK(text);
    GError *error=NULL;
    int count=atspi_text_get_character_count(text,&error);
    CHECK(!error && count==50); /* 48 cells, one wide continuation, one mark, two line breaks. */
    char *value=atspi_text_get_text(text,0,count,&error);
    CHECK(!error && value && !strncmp(value,"A\xe4\xb8\xad" "B",5));
    CHECK(strstr(value,"\ne\xcc\x81 ")!=NULL);
    g_free(value);
    CHECK(atspi_text_get_caret_offset(text,&error)==0 && !error);
    CHECK(!atspi_text_set_caret_offset(text,1,&error) && !error);
    CHECK(atspi_text_get_caret_offset(text,&error)==0 && !error);
    CHECK(!atspi_text_add_selection(text,0,1,&error) && !error);
    CHECK(atspi_text_get_n_selections(text,&error)==0 && !error);
    g_object_unref(text);g_object_unref(target);g_object_unref(desktop);
    atspi_exit();cleanup();
    puts("Independent AT-SPI snapshot text, scalar offsets and rejected mutations passed.");
    return 0;
}
