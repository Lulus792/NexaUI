#define _POSIX_C_SOURCE 200809L
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
static void cleanup(void) {if(host>0){kill(host,SIGTERM);waitpid(host,NULL,0);host=-1;}}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"atspi-async:%d: %s\n",__LINE__,#x);cleanup();return 1;}} while(0)
static double now(void) {struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;return t.tv_sec+t.tv_nsec/1e9;}
static void pump(void) {while(g_main_context_iteration(NULL,FALSE)){}struct timespec t={0,10000000};nanosleep(&t,NULL);}
static AtspiAccessible *find(AtspiAccessible *node,const char *wanted,unsigned depth) {
    if(!node || depth>16)return NULL;
    GError *error=NULL;char *name=atspi_accessible_get_name(node,&error);
    int match=!error && name && !strcmp(name,wanted);if(error)g_error_free(error);g_free(name);
    if(match)return g_object_ref(node);
    error=NULL;int count=atspi_accessible_get_child_count(node,&error);
    if(error){g_error_free(error);return NULL;}
    for(int i=0;i<count;++i) {
        error=NULL;AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,&error);
        if(error){g_error_free(error);continue;}
        AtspiAccessible *found=find(child,wanted,depth+1);if(child)g_object_unref(child);if(found)return found;
    }
    return NULL;
}
int main(int argc,char **argv) {
    const char *font=getenv("NEXAUI_SETTINGS_FONT"),*mode=getenv("NEXAUI_AX_ASYNC_MODE");
    CHECK(argc==2 && font && mode && (!strcmp(mode,"input") || !strcmp(mode,"timeout") || !strcmp(mode,"complete")));
    int control[2],progress[2];CHECK(pipe(control)==0 && pipe(progress)==0);
    host=fork();CHECK(host>=0);
    if(!host) {
        dup2(control[0],STDIN_FILENO);dup2(progress[1],STDOUT_FILENO);
        close(control[0]);close(control[1]);close(progress[0]);close(progress[1]);
        execl(argv[1],argv[1],font,mode,(char *)NULL);_exit(127);
    }
    close(control[0]);close(progress[1]);CHECK(atspi_init()==0);atspi_set_timeout(3500,3500);
    AtspiAccessible *desktop=atspi_get_desktop(0),*app=NULL;CHECK(desktop);
    for(unsigned attempt=0;attempt<300 && !app;++attempt) {
        pump();atspi_accessible_clear_cache(desktop);GError *error=NULL;
        int count=atspi_accessible_get_child_count(desktop,&error);
        if(error){g_error_free(error);continue;}
        for(int i=0;i<count && !app;++i) {
            error=NULL;AtspiAccessible *candidate=atspi_accessible_get_child_at_index(desktop,i,&error);
            if(error){g_error_free(error);continue;}if(!candidate)continue;
            atspi_accessible_clear_cache(candidate);guint pid=atspi_accessible_get_process_id(candidate,&error);
            if(!error && pid==(guint)host && atspi_accessible_get_child_count(candidate,&error)>0 && !error) app=candidate;
            else {if(error)g_error_free(error);g_object_unref(candidate);}
        }
    }
    CHECK(app);AtspiAccessible *name=find(app,"Display name",0);CHECK(name);
    AtspiEditableText *editable=atspi_accessible_get_editable_text_iface(name);CHECK(editable);
    AtspiText *text=atspi_accessible_get_text_iface(name);CHECK(text);
    GError *error=NULL;double started=now();CHECK(started>0);
    int applied=atspi_editable_text_paste_text(editable,0,&error);double elapsed=now()-started;
    CHECK(applied==(!strcmp(mode,"complete")) && !error && elapsed>=0 && elapsed<2.9);
    if(!strcmp(mode,"timeout"))CHECK(elapsed>=1.4);else CHECK(elapsed<1.5);
    char dots[512];struct pollfd fd={progress[0],POLLIN,0};CHECK(poll(&fd,1,1000)>0);
    ssize_t count=read(progress[0],dots,sizeof(dots));CHECK(count>0);
    if(strcmp(mode,"input"))CHECK(count>=5);
    char *value=atspi_text_get_text(text,0,-1,&error);
    const char *expected=!strcmp(mode,"input")?"Aethericaf\xc3\xa9X":!strcmp(mode,"complete")?"OnceAethericaf\xc3\xa9":"Aethericaf\xc3\xa9";
    if(error || !value || strcmp(value,expected))
        fprintf(stderr,"observed text: %s; error: %s\n",value?value:"(null)",error?error->message:"none");
    CHECK(!error && value && !strcmp(value,expected));g_free(value);
    CHECK(write(control[1],"Q",1)==1);close(control[1]);
    int status=0;CHECK(waitpid(host,&status,0)==host && WIFEXITED(status) && WEXITSTATUS(status)==0);host=-1;close(progress[0]);
    g_object_unref(text);g_object_unref(editable);g_object_unref(name);g_object_unref(app);g_object_unref(desktop);
    atspi_exit();printf("AT-SPI %s pending clipboard remained responsive in %.3f s\n",mode,elapsed);return 0;
}
