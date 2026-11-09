#define _POSIX_C_SOURCE 200809L
/* Independent native peer; no NexaUI or Aetheri linkage. The inherited private
 * test channel controls observations only, never production service admission. */
#include <atspi/atspi.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"controlled audio peer:%d: %s\n",__LINE__,#x);goto done;}} while(0)
static unsigned visits;
static void pump(void) {while(g_main_context_iteration(NULL,FALSE)){}struct timespec t={0,10000000};nanosleep(&t,NULL);}
static AtspiAccessible *find(AtspiAccessible *node,const char *name,AtspiRole role,unsigned depth) {
    if(!node || depth>16 || ++visits>256)return NULL;
    GError *error=NULL;atspi_accessible_clear_cache(node);char *text=atspi_accessible_get_name(node,&error);
    bool same=!error && text && !strcmp(text,name);g_free(text);
    if(error){g_error_free(error);return NULL;}
    if(same && atspi_accessible_get_role(node,&error)==role && !error)return g_object_ref(node);
    if(error){g_error_free(error);return NULL;}
    int count=atspi_accessible_get_child_count(node,&error);
    if(error){g_error_free(error);return NULL;}
    if(count<0 || count>256)return NULL;
    for(int i=0;i<count;++i) {
        AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,&error);
        if(error){g_error_free(error);return NULL;}
        AtspiAccessible *result=find(child,name,role,depth+1);if(child)g_object_unref(child);
        if(result)return result;
    }
    return NULL;
}
static AtspiAccessible *lookup(AtspiAccessible *app,const char *name,AtspiRole role) {visits=0;return find(app,name,role,0);}
static AtspiAccessible *application(AtspiAccessible *desktop,pid_t pid) {
    for(unsigned attempt=0;attempt<300;++attempt) {
        GError *error=NULL;atspi_accessible_clear_cache(desktop);int count=atspi_accessible_get_child_count(desktop,&error);
        if(error){g_error_free(error);return NULL;}
        if(count<0 || count>16)return NULL;
        for(int i=0;i<count;++i) {
            AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,&error);
            if(error){g_error_free(error);return NULL;}
            if(!app)continue;
            guint current=atspi_accessible_get_process_id(app,&error);
            if(!error && current==(guint)pid)return app;
            if(error){g_error_free(error);error=NULL;}g_object_unref(app);
        }
        pump();
    }
    return NULL;
}
static bool state(AtspiAccessible *node,AtspiStateType value) {
    atspi_accessible_clear_cache(node);AtspiStateSet *states=atspi_accessible_get_state_set(node);
    bool result=states && atspi_state_set_contains(states,value);if(states)g_object_unref(states);return result;
}
static int action_index(AtspiAccessible *node,const char *name) {
    AtspiAction *actions=atspi_accessible_get_action_iface(node);if(!actions)return -1;
    GError *error=NULL;int count=atspi_action_get_n_actions(actions,&error),index=-1;
    if(!error && count>=0 && count<=32)for(int i=0;i<count;++i) {
        char *current=atspi_action_get_action_name(actions,i,&error);
        bool same=!error && current && !strcmp(current,name);g_free(current);
        if(same){index=i;break;}
        if(error)break;
    }
    if(error)g_error_free(error);g_object_unref(actions);return index;
}
static bool invoke(AtspiAccessible *node,int index) {
    AtspiAction *actions=atspi_accessible_get_action_iface(node);if(!actions)return false;
    GError *error=NULL;bool result=atspi_action_do_action(actions,index,&error) && !error;
    if(error)g_error_free(error);g_object_unref(actions);return result;
}
static bool press(AtspiAccessible *app,const char *name,AtspiRole role) {
    for(unsigned i=0;i<300;++i) {
        AtspiAccessible *node=lookup(app,name,role);
        if(node) {
            int index=action_index(node,"click");bool enabled=state(node,ATSPI_STATE_ENABLED);
            bool result=enabled && index>=0 && invoke(node,index);g_object_unref(node);
            if(result)return true;
        }
        pump();
    }
    return false;
}
static bool slider(AtspiAccessible *node,const char *expected,bool enabled) {
    for(unsigned attempt=0;attempt<300;++attempt) {
        GError *error=NULL;atspi_accessible_clear_cache(node);GHashTable *attributes=atspi_accessible_get_attributes(node,&error);
        const char *value=attributes?g_hash_table_lookup(attributes,"nexaui-value"):NULL;
        bool same=!error && value && !strcmp(value,expected) && state(node,ATSPI_STATE_ENABLED)==enabled;
        if(error)g_error_free(error);if(attributes)g_hash_table_unref(attributes);
        if(same)return true;pump();
    }
    return false;
}
static bool command(int fd,const char *text,char *reply,size_t capacity) {
    if(send(fd,text,strlen(text),MSG_NOSIGNAL)!=(ssize_t)strlen(text))return false;
    struct pollfd p={fd,POLLIN,0};int ready;
    do {ready=poll(&p,1,2000);}while(ready<0 && errno==EINTR);
    if(ready!=1 || !(p.revents&POLLIN))return false;
    ssize_t n=recv(fd,reply,capacity-1,MSG_DONTWAIT);if(n<=0)return false;reply[n]=0;return true;
}
static bool observe(int fd,const char *expected) {
    char reply[256];
    for(unsigned attempt=0;attempt<300;++attempt) {
        if(!command(fd,"STATE",reply,sizeof(reply)))return false;
        if(!strcmp(reply,expected)){printf("BACKEND %s\n",reply);fflush(stdout);return true;}
        pump();
    }
    fprintf(stderr,"last backend observation: %s; expected: %s\n",reply,expected);return false;
}
static bool update(int fd,const char *text) {char reply[32];return command(fd,text,reply,sizeof(reply)) && !strcmp(reply,"OK");}
int main(int argc,char **argv) {
    int result=1,initialized=0,pair[2]={-1,-1};pid_t child=-1;FILE *log=NULL;
    AtspiAccessible *desktop=NULL,*app=NULL,*volume=NULL;
    const char *font=getenv("NEXAUI_AETHERI_SETTINGS_FONT");
    CHECK(argc==2 && font && (log=tmpfile()) && !socketpair(AF_UNIX,SOCK_SEQPACKET,0,pair));
    child=fork();CHECK(child>=0);
    if(!child) {
        close(pair[0]);char descriptor[32];(void)snprintf(descriptor,sizeof(descriptor),"%d",pair[1]);
        if(dup2(fileno(log),STDOUT_FILENO)<0 || dup2(fileno(log),STDERR_FILENO)<0 ||
                setenv("NEXAUI_SETTINGS_AUDIO_CONTROL_FD",descriptor,1))_exit(126);
        execl(argv[1],argv[1],"--font",font,"--fixture-displays","--diagnostics",(char *)NULL);_exit(127);
    }
    close(pair[1]);pair[1]=-1;
    CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);
    desktop=atspi_get_desktop(0);CHECK(desktop);app=application(desktop,child);CHECK(app);
    CHECK(press(app,"Sound",ATSPI_ROLE_PUSH_BUTTON));
    for(unsigned i=0;i<300 && !volume;++i){volume=lookup(app,"Volume",ATSPI_ROLE_SLIDER);if(!volume)pump();}
    CHECK(volume && slider(volume,"60",true));
    int increment=action_index(volume,"increment");CHECK(increment>=0 && invoke(volume,increment));
    CHECK(slider(volume,"60",false) && observe(pair[0],"STATE 1 1 0 0.600000000 0 4"));
    CHECK(!invoke(volume,increment));
    CHECK(observe(pair[0],"STATE 1 1 0 0.600000000 0 4"));
    if(!getenv("NEXAUI_AUDIO_CLOSE_PENDING")) {
        CHECK(update(pair[0],"ACK") && slider(volume,"61",true));
        CHECK(press(app,"Mute output",ATSPI_ROLE_SWITCH));
        CHECK(observe(pair[0],"STATE 2 1 0 0.610000000 0 4"));
        CHECK(update(pair[0],"ACK"));
        CHECK(press(app,"Headphones",ATSPI_ROLE_RADIO_BUTTON));
        CHECK(observe(pair[0],"STATE 3 1 0 0.610000000 1 5"));
        CHECK(update(pair[0],"LOSS"));
        AtspiAccessible *lost=NULL;
        for(unsigned i=0;i<300 && !lost;++i){lost=lookup(app,"Audio connection lost. Changes are disabled.",ATSPI_ROLE_LABEL);if(!lost)pump();}
        CHECK(lost);g_object_unref(lost);
        CHECK(state(volume,ATSPI_STATE_DEFUNCT) && !invoke(volume,increment));
        CHECK(observe(pair[0],"STATE 3 3 8 0.610000000 1 5"));
        CHECK(update(pair[0],"RECOVER"));
        CHECK(observe(pair[0],"STATE 3 3 0 0.610000000 1 5"));
        CHECK(!invoke(volume,increment));
    }
    CHECK(press(app,"Close",ATSPI_ROLE_PUSH_BUTTON));
    int status=0;pid_t ended=0;
    for(unsigned i=0;i<300 && !ended;++i){ended=waitpid(child,&status,WNOHANG);if(!ended)pump();}
    CHECK(ended==child && WIFEXITED(status) && !WEXITSTATUS(status));child=-1;
    result=0;
done:
    if(child>0){kill(child,SIGKILL);waitpid(child,NULL,0);}
    if(log) {
        if(!fseek(log,0,SEEK_SET)) {
            char *transcript=malloc(128*1024+1);size_t total=0;
            if(!transcript)result=1;
            else {
                total=fread(transcript,1,128*1024,log);transcript[total]=0;
                if(total==128*1024 || ferror(log)){fputs("excessive/unreadable host log\n",stderr);result=1;}
                unsigned frames=0;const char *p=transcript;
                while((p=strstr(p,"FRAME "))!=NULL){++frames;++p;}
                if(!frames || frames>100){fputs("host frame bound failed\n",stderr);result=1;}
                const char *retired=getenv("NEXAUI_AUDIO_CLOSE_PENDING")?"CONTROLLED_RETIRED 1 0":"CONTROLLED_RETIRED 3 0";
                if(!strstr(transcript,retired)){fputs("original host did not retire cleanly\n",stderr);result=1;}
                const char *retained=getenv("NEXAUI_AUDIO_CLOSE_PENDING")?"CONTROLLED_OWNER_AFTER_WINDOW 1 1":"CONTROLLED_OWNER_AFTER_WINDOW 3 3";
                if(!strstr(transcript,retained)){fputs("owner uncertainty did not survive window return\n",stderr);result=1;}
                if(!getenv("NEXAUI_AUDIO_CLOSE_PENDING")) {
                    const char *recover=strstr(transcript,"CONTROLLED_OBSERVE RECOVER\n");
                    const char *audio=recover?strstr(recover,"AUDIO state="):NULL;
                    const char *fence=audio?strstr(audio,"fenced=1"):NULL;
                    const char *endline=audio?strchr(audio,'\n'):NULL;
                    if(!fence || !endline || fence>endline){fputs("original model loss fence was not retained after recovery input\n",stderr);result=1;}
                }
                puts("CONTROLLED_HOST_LOG_BEGIN");(void)fwrite(transcript,1,total,stdout);
                puts("CONTROLLED_HOST_LOG_END");free(transcript);
            }
        }
        fclose(log);
    }
    if(pair[0]>=0)close(pair[0]);if(pair[1]>=0)close(pair[1]);
    if(volume)g_object_unref(volume);if(app)g_object_unref(app);if(desktop)g_object_unref(desktop);
    if(initialized)atspi_exit();
    if(!result)puts("Original audio entry: native intent, once-only dispatch, observed acknowledgement/loss or pending-close passed");
    return result;
}
