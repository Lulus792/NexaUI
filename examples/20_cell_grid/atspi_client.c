#define _POSIX_C_SOURCE 200809L
/* Independent libatspi consumer: no NexaUI headers or linkage. */
#include <atspi/atspi.h>
#include <poll.h>
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
static int key_action(const char *tool,const char *action,const char *arg) {
    pid_t child=fork(); if(child<0)return 0;
    if(!child){execl(tool,tool,action,arg,(char *)NULL);_exit(127);}
    int status=0; return waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status);
}
static int await_selection(AtspiText *text,int caret,int start,int end) {
    for(unsigned i=0;i<200;++i) {
        while(g_main_context_iteration(NULL,FALSE)){}
        GError *error=NULL; int actual=atspi_text_get_caret_offset(text,&error);
        int count=!error?atspi_text_get_n_selections(text,&error):-1;
        AtspiRange *range=!error && count==1?atspi_text_get_selection(text,0,&error):NULL;
        int correct=!error && actual==caret && ((start==end && count==0) ||
            (count==1 && range && range->start_offset==start && range->end_offset==end));
        if(range)g_boxed_free(ATSPI_TYPE_RANGE,range);
        if(error)g_error_free(error);
        if(correct)return 1;
        pause_ms();
    }
    return 0;
}
static int clipboard_equals(const char *tool,const char *expected) {
    int fds[2]; if(pipe(fds))return 0;
    pid_t child=fork();
    if(child<0){close(fds[0]);close(fds[1]);return 0;}
    if(!child){dup2(fds[1],STDOUT_FILENO);close(fds[0]);close(fds[1]);
        execl(tool,tool,"--no-newline",(char *)NULL);_exit(127);}
    close(fds[1]); char bytes[128]; size_t count=0; int finished=0;
    struct pollfd fd={fds[0],POLLIN|POLLHUP,0};
    while(count<sizeof(bytes) && poll(&fd,1,2000)>0) {
        ssize_t n=read(fds[0],bytes+count,sizeof(bytes)-count);
        if(n==0){finished=1;break;} if(n<0)break; count+=(size_t)n;
    }
    close(fds[0]);if(!finished)kill(child,SIGTERM);
    int status=0; if(waitpid(child,&status,0)!=child)return 0;
    return finished && WIFEXITED(status) && !WEXITSTATUS(status) &&
        count==strlen(expected) && !memcmp(bytes,expected,count);
}
static int await_clipboard(const char *tool,const char *expected) {
    for(unsigned i=0;i<8;++i) { if(clipboard_equals(tool,expected))return 1; pause_ms(); }
    return 0;
}
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
    if(getenv("NEXAUI_CELL_SELECTION_TEST")) {
        const char *tool=getenv("NEXAUI_CELL_GRID_XDOTOOL"),*logpath=getenv("NEXAUI_TEST_WESTON_LOG");
        CHECK(tool && logpath);
        FILE *log=fopen(logpath,"r"); CHECK(log);
        char line[1024],window[32]={0};
        while(fgets(line,sizeof(line),log)) {
            char *id=strstr(line,"window id ");
            if(id)snprintf(window,sizeof(window),"%lu",strtoul(id+10,NULL,10));
        }
        fclose(log); CHECK(window[0] && strcmp(window,"0"));
        CHECK(key_action(tool,"windowfocus",window));
        CHECK(key_action(tool,"key","shift+Right") && await_selection(text,1,0,1));
        CHECK(key_action(tool,"key","shift+Right") && await_selection(text,2,0,2));
        value=atspi_text_get_text(text,0,2,&error);
        CHECK(!error && value && !strcmp(value,"A\xe4\xb8\xad")); g_free(value);
        const char *paste=getenv("NEXAUI_CELL_GRID_WL_PASTE");
        if(paste)CHECK(key_action(tool,"key","ctrl+shift+c") && await_clipboard(paste,"A\xe4\xb8\xad"));
        CHECK(key_action(tool,"key","Left") && await_selection(text,1,1,1));
        if(paste)CHECK(key_action(tool,"key","ctrl+shift+c") && await_clipboard(paste,"A\xe4\xb8\xad"));
        CHECK(key_action(tool,"key","Right") && await_selection(text,2,2,2));
        CHECK(key_action(tool,"key","shift+Left") && await_selection(text,1,1,2));
        value=atspi_text_get_text(text,1,2,&error);
        CHECK(!error && value && !strcmp(value,"\xe4\xb8\xad")); g_free(value);
        if(paste)CHECK(key_action(tool,"key","ctrl+shift+c") && await_clipboard(paste,"\xe4\xb8\xad"));
        CHECK(!atspi_text_set_selection(text,0,0,1,&error) && !error);
        CHECK(await_selection(text,1,1,2));
        /* Move from the wide leading cell to row 1. Cell positions and AT-SPI
           scalar offsets differ: the wide continuation has no text, the line
           break has one scalar, and the marked cell has two scalars. */
        for(unsigned i=0;i<14;++i)CHECK(key_action(tool,"key","Right"));
        CHECK(await_selection(text,16,16,16));
        CHECK(key_action(tool,"key","shift+Right") && await_selection(text,18,16,18));
        value=atspi_text_get_text(text,16,18,&error);
        CHECK(!error && value && !strcmp(value,"e\xcc\x81")); g_free(value);
        if(paste) {
            CHECK(key_action(tool,"key","ctrl+c") && await_clipboard(paste,"\xe4\xb8\xad"));
            CHECK(await_selection(text,18,16,18));
            CHECK(key_action(tool,"key","ctrl+shift+c") && await_clipboard(paste,"e\xcc\x81"));
        }
        CHECK(key_action(tool,"key","shift+Left") && await_selection(text,16,16,16));
        CHECK(key_action(tool,"key","Right") && await_selection(text,18,18,18));
        CHECK(key_action(tool,"key","shift+Left") && await_selection(text,16,16,18));
        CHECK(key_action(tool,"key","Left") && await_selection(text,14,14,14));
        CHECK(key_action(tool,"key","shift+Right") && await_selection(text,16,14,16));
        value=atspi_text_get_text(text,14,16,&error);
        CHECK(!error && value && !strcmp(value," \n")); g_free(value);
        if(paste)CHECK(key_action(tool,"key","ctrl+shift+c") && await_clipboard(paste," \n"));
    }
    g_object_unref(text);g_object_unref(target);g_object_unref(desktop);
    atspi_exit();cleanup();
    puts("Independent AT-SPI snapshot text, scalar offsets, selection and rejected mutations passed.");
    return 0;
}
