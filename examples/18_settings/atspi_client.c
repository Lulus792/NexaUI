#define _POSIX_C_SOURCE 200809L
/* Independent OS client: no NexaUI headers or linkage. */
#include <atspi/atspi.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
static pid_t host=-1;
static pid_t clipboard_peer=-1;
typedef struct event_log { const char *path; unsigned deletions,insertions,caret,selection,phase; int bad; } event_log;
static void text_event(AtspiEvent *event,void *user) {
    event_log *log=user;
    if (event->source && !strcmp(ATSPI_OBJECT(event->source)->path,log->path)) {
        const char *value=G_VALUE_HOLDS_STRING(&event->any_data)?g_value_get_string(&event->any_data):NULL;
        if (!strcmp(event->type,"object:text-changed:delete")) {
            ++log->deletions;
            const char *expected=log->phase==1?"\xc3\xa9":"\xf0\x9f\x8c\x8d";
            if (event->detail1!=(log->phase==1?1:2) || event->detail2!=1 || !value || strcmp(value,expected)) log->bad=1;
        } else if (!strcmp(event->type,"object:text-changed:insert")) {
            ++log->insertions;
            const char *expected=log->phase==1?"\xc3\xb6":"Z";
            if (event->detail1!=(log->phase==1?1:2) || event->detail2!=1 || !value || strcmp(value,expected)) log->bad=1;
        } else if (!strcmp(event->type,"object:text-caret-moved")) {
            ++log->caret; if (event->detail1!=3) log->bad=1;
        } else if (!strcmp(event->type,"object:text-selection-changed")) ++log->selection;
    }
    g_boxed_free(ATSPI_TYPE_EVENT,event);
}
typedef struct movement_log { const char *path; unsigned carets,selections; int bad; } movement_log;
static void movement_event(AtspiEvent *event,void *user) {
    movement_log *log=user;
    if(event->source && !strcmp(ATSPI_OBJECT(event->source)->path,log->path)) {
        if(!strcmp(event->type,"object:text-caret-moved")) {
            static const int expected[]={1,2,3};
            if(log->carets>=3 || event->detail1!=expected[log->carets])log->bad=1;
            ++log->carets;
        } else if(!strcmp(event->type,"object:text-selection-changed"))++log->selections;
    }
    g_boxed_free(ATSPI_TYPE_EVENT,event);
}
static void cleanup(void) {
    if(clipboard_peer>0) {kill(clipboard_peer,SIGTERM);waitpid(clipboard_peer,NULL,0);clipboard_peer=-1;}
    if(host>0) { kill(host,SIGTERM); waitpid(host,NULL,0); host=-1; }
}
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"settings-atspi:%d: %s\n",__LINE__,#x); cleanup(); return 1; } } while(0)
static void pump(unsigned ms) { for(unsigned i=0;i<ms;++i) { while(g_main_context_iteration(NULL,FALSE)) {} struct timespec t={0,1000000}; nanosleep(&t,NULL); } }
static AtspiAccessible *find(AtspiAccessible *node,const char *wanted,unsigned depth) {
    if (!node || depth>16) return NULL;
    GError *error=NULL; char *name=atspi_accessible_get_name(node,&error);
    int match=!error && name && !strcmp(name,wanted);
    if(error) g_error_free(error); g_free(name);
    if(match) return g_object_ref(node);
    error=NULL; int count=atspi_accessible_get_child_count(node,&error);
    if(error) { g_error_free(error); return NULL; }
    for(int i=0;i<count;++i) {
        error=NULL; AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,&error);
        if(error) {g_error_free(error); continue;}
        AtspiAccessible *found=find(child,wanted,depth+1);
        if(child) g_object_unref(child); if(found) return found;
    }
    return NULL;
}
static AtspiAccessible *app_for_pid(AtspiAccessible *desktop) {
    for(unsigned n=0;n<300;++n) {
        pump(10); atspi_accessible_clear_cache(desktop); GError *error=NULL;
        int count=atspi_accessible_get_child_count(desktop,&error);
        if(error) {g_error_free(error); continue;}
        for(int i=0;i<count;++i) {
            error=NULL; AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,&error);
            if(error) {g_error_free(error); continue;} if(!app) continue;
            atspi_accessible_clear_cache(app); guint pid=atspi_accessible_get_process_id(app,&error);
            if(!error && pid==(guint)host && atspi_accessible_get_child_count(app,&error)>0 && !error) return app;
            if(error)g_error_free(error); g_object_unref(app);
        }
    }
    return NULL;
}
static int checked(AtspiAccessible *a) {
    AtspiStateSet *s=atspi_accessible_get_state_set(a);
    int yes=s && atspi_state_set_contains(s,ATSPI_STATE_CHECKED);
    if(s)g_object_unref(s); return yes;
}
static int activate(AtspiAccessible *a) {
    AtspiAction *action=atspi_accessible_get_action_iface(a); if(!action)return 0;
    GError *error=NULL; int okay=atspi_action_do_action(action,0,&error) && !error;
    if(error){fprintf(stderr,"action: %s\n",error->message);g_error_free(error);}
    g_object_unref(action); return okay;
}
static int virtual_serial(void) {
    const char *path=getenv("NEXAUI_TEST_WESTON_LOG"),*tool=getenv("NEXAUI_SETTINGS_XDOTOOL");
    if(!path || !tool)return 0;
    FILE *log=fopen(path,"r");if(!log)return 0;
    char line[1024],id[32]={0};
    while(fgets(line,sizeof(line),log)) {
        char *match=strstr(line,"window id ");
        if(match)snprintf(id,sizeof(id),"%lu",strtoul(match+10,NULL,10));
    }
    fclose(log);if(!id[0] || !strcmp(id,"0"))return 0;
    const char *actions[][2]={{"windowfocus",id},{"key","Shift_L"}};
    for(unsigned i=0;i<2;++i) {
        pid_t p=fork();if(p<0)return 0;
        if(!p){execl(tool,tool,actions[i][0],actions[i][1],(char *)NULL);_exit(127);}
        int status=0;if(waitpid(p,&status,0)!=p || !WIFEXITED(status) || WEXITSTATUS(status))return 0;
        pump(300);
    }
    return 1;
}
static int restore_text_focus(AtspiAccessible *text) {
    if(!virtual_serial())return 0;
    const char *tool=getenv("NEXAUI_SETTINGS_XDOTOOL");if(!tool)return 0;
    for(unsigned attempt=0;attempt<4;++attempt) {
        for(unsigned i=0;i<30;++i) {
            atspi_accessible_clear_cache(text);
            AtspiStateSet *states=atspi_accessible_get_state_set(text);
            int focused=states && atspi_state_set_contains(states,ATSPI_STATE_FOCUSED);
            if(states)g_object_unref(states);
            if(focused)return 1;
            pump(10);
        }
        pid_t child=fork();if(child<0)return 0;
        if(!child){execl(tool,tool,"key","Alt+Tab",(char *)NULL);_exit(127);}
        int status=0;if(waitpid(child,&status,0)!=child || !WIFEXITED(status) || WEXITSTATUS(status))return 0;
    }
    return 0;
}
static int peer_read(const char *expected) {
    const char *tool=getenv("NEXAUI_TEST_WL_PASTE");if(!tool)return 0;
    int pipes[2];if(pipe(pipes))return 0;
    pid_t p=fork();if(p<0){close(pipes[0]);close(pipes[1]);return 0;}
    if(!p){dup2(pipes[1],STDOUT_FILENO);close(pipes[0]);close(pipes[1]);
        execl(tool,tool,"--no-newline",(char *)NULL);_exit(127);}
    close(pipes[1]);struct pollfd fd={pipes[0],POLLIN|POLLHUP,0};char buffer[128];size_t used=0;int ready=0;
    while(used<sizeof(buffer) && poll(&fd,1,2000)>0) {
        ssize_t n=read(pipes[0],buffer+used,sizeof(buffer)-used);
        if(n==0){ready=1;break;}if(n<0)break;used+=(size_t)n;
    }
    close(pipes[0]);if(!ready)kill(p,SIGTERM);
    int status=0;if(waitpid(p,&status,0)!=p)return 0;
    return ready && WIFEXITED(status) && WEXITSTATUS(status)==0 &&
        used==strlen(expected) && !memcmp(buffer,expected,used);
}
static int peer_await(const char *expected) {
    for(unsigned i=0;i<8;++i){pump(50);if(peer_read(expected))return 1;}
    return 0;
}
static int peer_own(const char *value,int once) {
    const char *tool=getenv("NEXAUI_TEST_WL_COPY");if(!tool || clipboard_peer>0)return 0;
    int pipes[2];if(pipe(pipes))return 0;
    clipboard_peer=fork();if(clipboard_peer<0){clipboard_peer=-1;close(pipes[0]);close(pipes[1]);return 0;}
    if(!clipboard_peer){dup2(pipes[0],STDIN_FILENO);close(pipes[0]);close(pipes[1]);
        if(once)execl(tool,tool,"--foreground","--paste-once",(char *)NULL);
        else execl(tool,tool,"--foreground",(char *)NULL);_exit(127);}
    close(pipes[0]);size_t length=strlen(value),written=0;
    while(written<length){ssize_t n=write(pipes[1],value+written,length-written);if(n<=0)break;written+=(size_t)n;}
    close(pipes[1]);if(written!=length)return 0;
    if(once){pump(300);return kill(clipboard_peer,0)==0;}
    return peer_await(value);
}
int main(int argc,char **argv) {
    const char *font=getenv("NEXAUI_SETTINGS_FONT"); CHECK(argc==2 && font && *font);
    host=fork(); CHECK(host>=0);
    if(!host) {execl(argv[1],argv[1],font,"--verify-accessibility",(char *)NULL);_exit(127);}
    CHECK(atspi_init()==0); atspi_set_timeout(1000,1000);
    AtspiAccessible *desktop=atspi_get_desktop(0); CHECK(desktop);
    AtspiAccessible *app=app_for_pid(desktop); CHECK(app);
    int german=getenv("NEXAUI_SETTINGS_TEST_GERMAN")!=NULL;
    AtspiAccessible *name=find(app,german?"Anzeigename":"Display name",0),
        *motion=find(app,german?"Bewegung reduzieren":"Reduce motion",0),
        *apply=find(app,german?"Einstellungen anwenden":"Apply settings",0);
    CHECK(name && motion && apply);
    AtspiAccessible *secret=find(app,"Secret probe",0), *custom=find(app,"Custom text probe",0);
    CHECK(secret && custom);
    GError *error=NULL;
    CHECK(atspi_accessible_get_role(secret,&error)==ATSPI_ROLE_PASSWORD_TEXT && !error);
    CHECK(!atspi_accessible_get_text_iface(secret) && !atspi_accessible_get_editable_text_iface(secret));
    CHECK(!atspi_accessible_get_text_iface(custom) && !atspi_accessible_get_editable_text_iface(custom));
    GHashTable *secret_attrs=atspi_accessible_get_attributes(secret,&error);
    CHECK(secret_attrs && !error && !g_hash_table_lookup(secret_attrs,"nexaui-value"));
    g_hash_table_unref(secret_attrs);
    GHashTable *attributes=atspi_accessible_get_attributes(name,&error);
    CHECK(attributes && !error && g_strcmp0(g_hash_table_lookup(attributes,"nexaui-value"),"A\xc3\xa9\xf0\x9f\x8c\x8d")==0);
    g_hash_table_unref(attributes);
    AtspiText *text=atspi_accessible_get_text_iface(name);
    AtspiEditableText *editable=atspi_accessible_get_editable_text_iface(name);
    CHECK(text && editable);
    CHECK(atspi_text_get_character_count(text,&error)==3 && !error);
    CHECK(atspi_text_get_caret_offset(text,&error)==1 && !error);
    CHECK(atspi_text_get_n_selections(text,&error)==1 && !error);
    AtspiRange *selection=atspi_text_get_selection(text,0,&error);
    CHECK(!error && selection && selection->start_offset==1 && selection->end_offset==3);
    g_boxed_free(ATSPI_TYPE_RANGE,selection);
    char *all=atspi_text_get_text(text,0,-1,&error);
    CHECK(!error && all && !strcmp(all,"A\xc3\xa9\xf0\x9f\x8c\x8d")); g_free(all);
    char *part=atspi_text_get_text(text,1,3,&error);
    CHECK(!error && part && !strcmp(part,"\xc3\xa9\xf0\x9f\x8c\x8d")); g_free(part);
    char *empty=atspi_text_get_text(text,1,1,&error);
    CHECK(!error && empty && !*empty); g_free(empty);
    CHECK(atspi_text_get_character_at_offset(text,2,&error)==0x1f30d && !error);
    AtspiTextRange *character=atspi_text_get_string_at_offset(text,1,ATSPI_TEXT_GRANULARITY_CHAR,&error);
    CHECK(!error && character && character->start_offset==1 && character->end_offset==2 &&
          character->content && !strcmp(character->content,"\xc3\xa9"));
    g_boxed_free(ATSPI_TYPE_TEXT_RANGE,character);
    AtspiTextRange *unsupported=atspi_text_get_string_at_offset(text,1,ATSPI_TEXT_GRANULARITY_WORD,&error);
    CHECK(error); if (unsupported) g_boxed_free(ATSPI_TYPE_TEXT_RANGE,unsupported); g_clear_error(&error);
    char *invalid=atspi_text_get_text(text,0,4,&error);
    CHECK(error); g_free(invalid); g_clear_error(&error);
    invalid=atspi_text_get_text(text,2,1,&error);
    CHECK(error); g_free(invalid); g_clear_error(&error);
    AtspiRange *missing=atspi_text_get_selection(text,1,&error);
    CHECK(error); if (missing) g_boxed_free(ATSPI_TYPE_RANGE,missing); g_clear_error(&error);
    CHECK(!atspi_text_set_caret_offset(text,99,&error) && !error);
    CHECK(atspi_text_get_caret_offset(text,&error)==1 && !error);
    event_log events={0}; events.path=ATSPI_OBJECT(name)->path; events.phase=1;
    AtspiEventListener *listener=atspi_event_listener_new(text_event,&events,NULL); CHECK(listener);
    CHECK(atspi_event_listener_register(listener,"object:text-changed",&error) && !error);
    CHECK(atspi_event_listener_register(listener,"object:text-caret-moved",&error) && !error);
    CHECK(atspi_event_listener_register(listener,"object:text-selection-changed",&error) && !error);
    AtspiAccessible *change=find(app,"Change text probe",0); CHECK(change && activate(change));
    for(unsigned i=0;i<200 && (!events.deletions || !events.insertions || !events.caret || !events.selection);++i) pump(10);
    CHECK(events.deletions==1 && events.insertions==1 && events.caret==1 && events.selection==1 && !events.bad);
    all=atspi_text_get_text(text,0,-1,&error); CHECK(!error && all && !strcmp(all,"A\xc3\xb6\xf0\x9f\x8c\x8d")); g_free(all);
    events.phase=2; CHECK(activate(change));
    for(unsigned i=0;i<200 && (events.deletions<2 || events.insertions<2);++i) pump(10);
    CHECK(events.deletions==2 && events.insertions==2 && events.caret==1 && events.selection==1 && !events.bad);
    all=atspi_text_get_text(text,0,-1,&error); CHECK(!error && all && !strcmp(all,"A\xc3\xb6Z")); g_free(all);
    CHECK(atspi_text_get_caret_offset(text,&error)==3 && !error);
    CHECK(atspi_text_get_n_selections(text,&error)==0 && !error);
    CHECK(atspi_event_listener_deregister(listener,"object:text-changed",&error) && !error);
    CHECK(atspi_event_listener_deregister(listener,"object:text-caret-moved",&error) && !error);
    CHECK(atspi_event_listener_deregister(listener,"object:text-selection-changed",&error) && !error);
    g_object_unref(change); g_object_unref(listener);
    movement_log movement={0};movement.path=ATSPI_OBJECT(name)->path;
    listener=atspi_event_listener_new(movement_event,&movement,NULL);CHECK(listener);
    CHECK(atspi_event_listener_register(listener,"object:text-caret-moved",&error) && !error);
    CHECK(atspi_event_listener_register(listener,"object:text-selection-changed",&error) && !error);
    CHECK(atspi_text_set_caret_offset(text,1,&error) && !error);
    CHECK(atspi_text_get_caret_offset(text,&error)==1 && !error);
    CHECK(atspi_text_add_selection(text,0,2,&error) && !error);
    CHECK(!atspi_text_add_selection(text,1,2,&error) && !error);
    CHECK(!atspi_text_remove_selection(text,1,&error) && !error);
    CHECK(atspi_text_set_selection(text,0,1,3,&error) && !error);
    selection=atspi_text_get_selection(text,0,&error);
    CHECK(!error && selection && selection->start_offset==1 && selection->end_offset==3);
    g_boxed_free(ATSPI_TYPE_RANGE,selection);
    CHECK(atspi_text_remove_selection(text,0,&error) && !error);
    CHECK(atspi_text_get_n_selections(text,&error)==0 && !error);
    for(unsigned i=0;i<200 && (movement.carets<3 || movement.selections<3);++i)pump(10);
    CHECK(!movement.bad && movement.carets==3 && movement.selections==3);
    CHECK(atspi_event_listener_deregister(listener,"object:text-caret-moved",&error) && !error);
    CHECK(atspi_event_listener_deregister(listener,"object:text-selection-changed",&error) && !error);
    g_object_unref(listener);
    if(getenv("NEXAUI_SETTINGS_CLIPBOARD_SERIAL")) {
        CHECK(restore_text_focus(name));
        CHECK(atspi_editable_text_copy_text(editable,1,2,&error) && !error);
        if(getenv("NEXAUI_SETTINGS_CLIPBOARD_PEER"))CHECK(peer_await("\xc3\xb6") && restore_text_focus(name));
        CHECK(atspi_editable_text_cut_text(editable,1,2,&error) && !error);
        all=atspi_text_get_text(text,0,-1,&error);CHECK(!error && all && !strcmp(all,"AZ"));g_free(all);
        CHECK(atspi_editable_text_paste_text(editable,1,&error) && !error);
        if(getenv("NEXAUI_SETTINGS_CLIPBOARD_PEER")) {
            CHECK(peer_own("Peer \xf0\x9f\x8c\x8d",getenv("NEXAUI_SETTINGS_CLIPBOARD_ONCE")!=NULL));
            CHECK(restore_text_focus(name));
            CHECK(atspi_editable_text_paste_text(editable,0,&error) && !error);
            all=atspi_text_get_text(text,0,-1,&error);
            if(error || !all || strcmp(all,"Peer \xf0\x9f\x8c\x8d" "A\xc3\xb6Z"))
                fprintf(stderr,"external paste result: %s; error: %s\n",all?all:"(null)",error?error->message:"none");
            int peer_matches=!error && all && !strcmp(all,"Peer \xf0\x9f\x8c\x8d" "A\xc3\xb6Z");
            g_free(all);CHECK(peer_matches);
            if(getenv("NEXAUI_SETTINGS_CLIPBOARD_ONCE")) {
                int status=0,exited=0;
                for(unsigned i=0;i<60 && !exited;++i){pid_t p=waitpid(clipboard_peer,&status,WNOHANG);
                    if(p==clipboard_peer){exited=1;clipboard_peer=-1;}else if(p<0)break;else pump(50);}
                CHECK(exited && WIFEXITED(status) && WEXITSTATUS(status)==0);
            }
            CHECK(atspi_editable_text_set_text_contents(editable,"A\xc3\xb6Z",&error) && !error);
        }
    } else {
        /* Without an input serial Wayland must not claim clipboard ownership. */
        (void)atspi_editable_text_copy_text(editable,0,1,&error);
        CHECK(error);g_clear_error(&error);
        CHECK(!atspi_editable_text_cut_text(editable,0,1,&error) && !error);
    }
    all=atspi_text_get_text(text,0,-1,&error);CHECK(!error && all && !strcmp(all,"A\xc3\xb6Z"));g_free(all);
    CHECK(atspi_editable_text_set_text_contents(editable,"AT-SPI Toky",&error) && !error);
    CHECK(atspi_editable_text_insert_text(editable,11,"o",1,&error) && !error);
    CHECK(atspi_editable_text_delete_text(editable,11,12,&error) && !error);
    CHECK(atspi_editable_text_insert_text(editable,11,"o",1,&error) && !error);
    all=atspi_text_get_text(text,0,-1,&error);CHECK(!error && all && !strcmp(all,"AT-SPI Tokyo"));g_free(all);
    g_object_unref(editable);
    g_object_unref(text);
    CHECK(atspi_accessible_get_role(motion,&error)==ATSPI_ROLE_CHECK_BOX && !error);
    /* Allow the host to become idle before sending a native action. */
    pump(1200);
    CHECK(!checked(motion) && activate(motion));
    int changed=0;
    for(unsigned i=0;i<200 && !changed;++i) {pump(10);atspi_accessible_clear_cache(motion);changed=checked(motion);}
    CHECK(changed && activate(apply));
    if (getenv("NEXAUI_SETTINGS_TEST_APPLY_ERROR")) {
        AtspiAccessible *message=find(app,german?"Einstellungsstatus":"Settings status",0); CHECK(message);
        int failed=0;
        for(unsigned i=0;i<200 && !failed;++i) {
            pump(10); atspi_accessible_clear_cache(message);
            GHashTable *attrs=atspi_accessible_get_attributes(message,&error);
            CHECK(attrs && !error);
            failed=g_strcmp0(g_hash_table_lookup(attrs,"nexaui-value"),german?
                "Anwenden fehlgeschlagen. Erneut versuchen.":"Example apply failed. Retry.")==0;
            g_hash_table_unref(attrs);
        }
        CHECK(failed && activate(apply));
        g_object_unref(message);
    }
    int status=0,finished=0;
    for(unsigned i=0;i<300 && !finished;++i) {
        pump(10); pid_t p=waitpid(host,&status,WNOHANG);
        if(p==host) { finished=1; host=-1; } else if(p<0) break;
    }
    CHECK(finished && WIFEXITED(status) && WEXITSTATUS(status)==0);
    g_object_unref(secret);g_object_unref(custom);g_object_unref(name);g_object_unref(motion);g_object_unref(apply);g_object_unref(app);g_object_unref(desktop);
    atspi_exit();cleanup(); puts("Independent AT-SPI text, checkbox and Apply updated the real model; host closed cleanly."); return 0;
}
