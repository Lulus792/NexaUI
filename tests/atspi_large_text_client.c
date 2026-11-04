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

static pid_t child=-1; static FILE *commands,*responses;
static void cleanup(void) {
    if (commands) { fclose(commands); commands=NULL; }
    if (responses) { fclose(responses); responses=NULL; }
    if (child>0) { kill(child,SIGTERM); waitpid(child,NULL,0); child=-1; }
}
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"large-atspi:%d: %s\n",__LINE__,#x); cleanup(); return 1; } } while (0)
static void pump(unsigned ms) {
    for (unsigned i=0;i<ms;++i) { while(g_main_context_iteration(NULL,FALSE)) {} struct timespec t={0,1000000}; nanosleep(&t,NULL); }
}
static int response(const char *expected) {
    struct pollfd fd={fileno(responses),POLLIN,0}; char line[80];
    return poll(&fd,1,30000)>0 && fgets(line,sizeof(line),responses) && !strcmp(line,expected);
}
static int command(const char *s) {
    char expected[80]; snprintf(expected,sizeof(expected),"ACK %s\n",s);
    return fprintf(commands,"%s\n",s)>0 && fflush(commands)==0 && response(expected);
}
static AtspiAccessible *find(AtspiAccessible *root,const char *wanted,unsigned depth) {
    if (!root || depth>12) return NULL;
    GError *error=NULL; char *name=atspi_accessible_get_name(root,&error);
    int match=!error && name && !strcmp(name,wanted); g_clear_error(&error); g_free(name);
    if (match) return g_object_ref(root);
    int count=atspi_accessible_get_child_count(root,&error); if (error) {g_clear_error(&error);return NULL;}
    for (int i=0;i<count;++i) {
        AtspiAccessible *c=atspi_accessible_get_child_at_index(root,i,&error);
        if(error) {g_clear_error(&error);continue;}
        AtspiAccessible *found=find(c,wanted,depth+1); if(c) g_object_unref(c); if(found)return found;
    }
    return NULL;
}
typedef struct replay { const char *path; GString *model; unsigned phase, inserts, deletes; int bad; } replay;
static void event(AtspiEvent *e,void *user) {
    replay *r=user;
    if (e->source && !strcmp(ATSPI_OBJECT(e->source)->path,r->path) &&
        (!strcmp(e->type,"object:text-changed:insert") || !strcmp(e->type,"object:text-changed:delete"))) {
        int deleting=strstr(e->type,":delete")!=NULL;
        const char *value=G_VALUE_HOLDS_STRING(&e->any_data)?g_value_get_string(&e->any_data):NULL;
        if (!value || !g_utf8_validate(value,-1,NULL) || strlen(value)>65536u || e->detail1<0 || e->detail2<=0 ||
            g_utf8_strlen(value,-1)!=e->detail2 || deleting!=(r->phase==2)) r->bad=1;
        else {
            static const int positions[2][3]={{1,65535,131068},{65536,3,1}};
            static const int characters[2][3]={{65534,65533,2},{65534,65533,2}};
            static const size_t lengths[2][3]={{65534,65536,2},{65534,65536,2}};
            unsigned which=deleting?r->deletes:r->inserts, phase=deleting?1u:0u;
            if (which>=3 || e->detail1!=positions[phase][which] ||
                e->detail2!=characters[phase][which] || strlen(value)!=lengths[phase][which]) r->bad=1;
            glong count=g_utf8_strlen(r->model->str,(gssize)r->model->len);
            if (e->detail1>count || (deleting && e->detail2>count-e->detail1)) r->bad=1;
            else {
                const char *start=g_utf8_offset_to_pointer(r->model->str,e->detail1);
                size_t at=(size_t)(start-r->model->str), bytes=strlen(value);
                if (deleting) {
                    const char *end=g_utf8_offset_to_pointer(start,e->detail2);
                    if ((size_t)(end-start)!=bytes || memcmp(start,value,bytes)) r->bad=1;
                    else {g_string_erase(r->model,(gssize)at,(gssize)bytes); ++r->deletes;}
                } else {g_string_insert_len(r->model,(gssize)at,value,(gssize)bytes); ++r->inserts;}
            }
        }
    }
    g_boxed_free(ATSPI_TYPE_EVENT,e);
}
int main(int argc,char **argv) {
    CHECK(argc==2); int in[2],out[2]; CHECK(!pipe(in) && !pipe(out));
    child=fork(); CHECK(child>=0);
    if (!child) {dup2(in[0],STDIN_FILENO);dup2(out[1],STDOUT_FILENO);close(in[0]);close(in[1]);close(out[0]);close(out[1]);execl(argv[1],argv[1],(char *)NULL);_exit(127);}
    close(in[0]); close(out[1]); commands=fdopen(in[1],"w"); responses=fdopen(out[0],"r");
    CHECK(commands && responses); setvbuf(responses,NULL,_IONBF,0); CHECK(response("READY\n"));
    CHECK(atspi_init()==0); atspi_set_timeout(1000,1000); AtspiAccessible *desktop=atspi_get_desktop(0),*app=NULL;
    CHECK(desktop);
    for(unsigned tries=0;tries<300 && !app;++tries) {
        pump(10); atspi_accessible_clear_cache(desktop); GError *e=NULL;
        int n=atspi_accessible_get_child_count(desktop,&e); if(e){g_clear_error(&e);continue;}
        for(int i=0;i<n;++i) {
            AtspiAccessible *a=atspi_accessible_get_child_at_index(desktop,i,&e);
            if(e){g_clear_error(&e);continue;} if(!a)continue;
            guint pid=atspi_accessible_get_process_id(a,&e);
            if(!e && pid==(guint)child){app=a;break;} g_clear_error(&e);g_object_unref(a);
        }
    }
    CHECK(app); AtspiAccessible *field=find(app,"Large text field",0); CHECK(field);
    AtspiText *text=atspi_accessible_get_text_iface(field); CHECK(text);
    replay r={0}; r.path=ATSPI_OBJECT(field)->path; r.model=g_string_new("AZ"); r.phase=1;
    AtspiEventListener *listener=atspi_event_listener_new(event,&r,NULL); CHECK(listener);
    GError *error=NULL; CHECK(atspi_event_listener_register(listener,"object:text-changed",&error) && !error);
    CHECK(command("large"));
    for(unsigned i=0;i<300 && r.inserts<3 && !r.bad;++i) pump(10);
    CHECK(!r.bad && r.inserts==3 && r.deletes==0 && r.model->len==131074u);
    CHECK(r.model->str[0]=='A' && r.model->str[131073]=='Z' &&
          !memcmp(r.model->str+65535,"\xf0\x9f\x8c\x8d",4));
    for(size_t i=1;i<65535u;++i) CHECK(r.model->str[i]=='x');
    for(size_t i=65539u;i<131073u;++i) CHECK(r.model->str[i]=='x');
    char *actual=atspi_text_get_text(text,0,-1,&error);
    CHECK(!error && actual && strlen(actual)==r.model->len && !memcmp(actual,r.model->str,r.model->len)); g_free(actual);
    r.phase=2; CHECK(command("short"));
    for(unsigned i=0;i<300 && r.deletes<3 && !r.bad;++i) pump(10);
    CHECK(!r.bad && r.inserts==3 && r.deletes==3 && r.model->len==2 && !memcmp(r.model->str,"AZ",2));
    actual=atspi_text_get_text(text,0,-1,&error); CHECK(!error && actual && !strcmp(actual,"AZ")); g_free(actual);
    CHECK(fprintf(commands,"quit\n")>0 && fflush(commands)==0);
    int status=0; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0); child=-1;
    g_string_free(r.model,TRUE); g_object_unref(listener);g_object_unref(text);g_object_unref(field);g_object_unref(app);g_object_unref(desktop);
    atspi_exit(); cleanup(); puts("AT-SPI replay: 131074 UTF-8 bytes, 3 insert chunks, 3 delete chunks; host closed cleanly."); return 0;
}
