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
typedef struct replay {const char *path;GString *text;unsigned inserted,deleted;int bad;} replay;
static void replay_event(AtspiEvent *event,void *user) {
    replay *r=user;
    if(event->source && !strcmp(ATSPI_OBJECT(event->source)->path,r->path) &&
       (!strcmp(event->type,"object:text-changed:insert") || !strcmp(event->type,"object:text-changed:delete"))) {
        int deleting=strstr(event->type,":delete")!=NULL;
        const char *bytes=G_VALUE_HOLDS_STRING(&event->any_data)?g_value_get_string(&event->any_data):NULL;
        glong count=g_utf8_strlen(r->text->str,(gssize)r->text->len);
        if(!bytes || !g_utf8_validate(bytes,-1,NULL) || strlen(bytes)>65536u || event->detail1<0 ||
           event->detail2<=0 || g_utf8_strlen(bytes,-1)!=event->detail2 || event->detail1>count ||
           (deleting && event->detail2>count-event->detail1)) r->bad=1;
        else {
            const char *start=g_utf8_offset_to_pointer(r->text->str,event->detail1);
            size_t at=(size_t)(start-r->text->str),length=strlen(bytes);
            if(deleting) {
                const char *end=g_utf8_offset_to_pointer(start,event->detail2);
                if((size_t)(end-start)!=length || memcmp(start,bytes,length))r->bad=1;
                else {g_string_erase(r->text,(gssize)at,(gssize)length);++r->deleted;}
            } else {g_string_insert_len(r->text,(gssize)at,bytes,(gssize)length);++r->inserted;}
        }
    }
    g_boxed_free(ATSPI_TYPE_EVENT,event);
}
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
    if(getenv("NEXAUI_TERMINAL_AX_LARGE")) {
        CHECK(text);GError *error=NULL;int count=atspi_text_get_character_count(text,&error);
        CHECK(!error && count==24099);
        char *value=atspi_text_get_text(text,0,-1,&error);
        CHECK(!error && value && strlen(value)==96099 && g_utf8_validate(value,-1,NULL) &&
              g_utf8_strlen(value,-1)==count && !memcmp(value,"\xf0\x9f\x98\x80",4));
        size_t offset=0;
        for(unsigned row=0;row<100;++row) {
            for(unsigned column=0;column<240;++column) {
                CHECK(!memcmp(value+offset,"\xf0\x9f\x98\x80",4));offset+=4;
            }
            if(row<99)CHECK(value[offset++]=='\n');
        }
        CHECK(offset==96099);
        if(getenv("NEXAUI_TERMINAL_AX_LARGE_UPDATES")) {
            replay r={.path=ATSPI_OBJECT(target)->path,.text=g_string_new(value)};
            CHECK(r.text);AtspiEventListener *listener=atspi_event_listener_new(replay_event,&r,NULL);CHECK(listener);
            CHECK(atspi_event_listener_register(listener,"object:text-changed",&error) && !error);
            for(unsigned pass=0;pass<2;++pass) {
                unsigned before_insert=r.inserted,before_delete=r.deleted;
                CHECK(!kill(child,SIGUSR2));int matched=0;
                for(unsigned attempt=0;attempt<500 && !matched;++attempt) {
                    while(g_main_context_iteration(NULL,FALSE)){}
                    atspi_accessible_clear_cache(target);
                    char *current=atspi_text_get_text(text,0,-1,&error);CHECK(!error && current);
                    size_t expected=pass?96099u:24099u;
                    matched=strlen(current)==expected && r.text->len==expected &&
                        !memcmp(current,r.text->str,expected) && r.inserted>before_insert && r.deleted>before_delete;
                    g_free(current);CHECK(!r.bad);if(!matched)pause_ms();
                }
                CHECK(matched);
                for(unsigned row=0;row<100;++row) {
                    for(unsigned col=0;col<240;++col) {
                        size_t at=pass?(size_t)row*961u+col*4u:(size_t)row*241u+col;
                        CHECK(pass?!memcmp(r.text->str+at,"\xf0\x9f\x98\x80",4):r.text->str[at]=='X');
                    }
                    if(row<99)CHECK(r.text->str[pass?(size_t)row*961u+960u:(size_t)row*241u+240u]=='\n');
                }
            }
            printf("Replayed %u insert and %u delete events across two large updates\n",r.inserted,r.deleted);
            CHECK(atspi_event_listener_deregister(listener,"object:text-changed",&error) && !error);
            g_object_unref(listener);g_string_free(r.text,TRUE);
        }
        g_free(value);g_object_unref(text);g_object_unref(target);g_object_unref(desktop);cleanup();
        puts("Independent AT-SPI read the full large Unicode terminal snapshot");return 0;
    }
    if(getenv("NEXAUI_TERMINAL_AX_COMPOSITION")) {
        CHECK(text);GError *error=NULL;
        AtspiAccessible *parent=atspi_accessible_get_parent(target,&error);CHECK(!error && parent);
        AtspiAccessible *draft=find(parent,"Uncommitted composition",0);CHECK(draft);
        AtspiText *draft_text=atspi_accessible_get_text_iface(draft);CHECK(draft_text);
        CHECK(atspi_text_get_character_count(draft_text,&error)==3 && !error);
        char *value=atspi_text_get_text(draft_text,0,-1,&error);
        CHECK(!error && value && !strcmp(value,"na\xc3\xaf"));g_free(value);
        int hidden=getenv("NEXAUI_TERMINAL_AX_HIDDEN")!=NULL;
        CHECK(atspi_text_get_caret_offset(draft_text,&error)==(hidden?-1:1) && !error);
        CHECK(atspi_text_get_n_selections(draft_text,&error)==(hidden?0:1) && !error);
        if(!hidden) {
            AtspiRange *range=atspi_text_get_selection(draft_text,0,&error);
            CHECK(!error && range && range->start_offset==1 && range->end_offset==3);g_free(range);
        }
        CHECK(!atspi_text_set_caret_offset(draft_text,0,&error) && !error);
        CHECK(!atspi_text_set_selection(draft_text,0,0,1,&error) && !error);
        CHECK(!kill(child,SIGUSR2));int updated=0;
        for(unsigned attempt=0;attempt<300;++attempt) {
            while(g_main_context_iteration(NULL,FALSE)){}
            atspi_accessible_clear_cache(draft);
            value=atspi_text_get_text(draft_text,0,-1,&error);CHECK(!error);
            if(value && !strcmp(value,"n\xc3\xa4"))updated=1;
            g_free(value);if(updated)break;pause_ms();
        }
        CHECK(updated && atspi_text_get_caret_offset(draft_text,&error)==2 && !error);
        CHECK(atspi_text_get_n_selections(draft_text,&error)==0 && !error);
        CHECK(!kill(child,SIGUSR1));int removed=0;
        for(unsigned attempt=0;attempt<300;++attempt) {
            while(g_main_context_iteration(NULL,FALSE)){}
            atspi_accessible_clear_cache(parent);
            AtspiAccessible *current=find(parent,"Uncommitted composition",0);
            if(!current){removed=1;break;}g_object_unref(current);pause_ms();
        }
        CHECK(removed);
        value=atspi_text_get_text(text,0,4,&error);
        CHECK(!error && value && !strcmp(value,"A\xe7\x95\x8c" "e\xcc\x81"));g_free(value);
        g_object_unref(draft_text);g_object_unref(draft);g_object_unref(parent);
        g_object_unref(text);g_object_unref(target);g_object_unref(desktop);cleanup();
        puts("Independent AT-SPI uncommitted composition and removal passed");return 0;
    }
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
