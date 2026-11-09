#define main nexaui_large_text_fixture_main
#include "atspi_large_text_client.c"
#undef main
#undef CHECK
#include "atspi_cache_probe.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"cache-budget:%d: %s\n",__LINE__,#x);cleanup();return 1;}} while(0)

int main(int argc,char **argv) {
    CHECK(argc==2);int in[2],out[2];CHECK(!pipe(in) && !pipe(out));child=fork();CHECK(child>=0);
    if(!child){dup2(in[0],STDIN_FILENO);dup2(out[1],STDOUT_FILENO);close(in[0]);close(in[1]);close(out[0]);close(out[1]);execl(argv[1],argv[1],"--cache-metadata",(char *)NULL);_exit(127);}
    close(in[0]);close(out[1]);commands=fdopen(in[1],"w");responses=fdopen(out[0],"r");CHECK(commands && responses);
    setvbuf(responses,NULL,_IONBF,0);CHECK(response("READY\n") && !atspi_init());atspi_set_timeout(1000,1000);
    AtspiAccessible *desktop=atspi_get_desktop(0),*app=NULL;CHECK(desktop);
    for(unsigned tries=0;tries<300 && !app;++tries) {
        pump(10);atspi_accessible_clear_cache(desktop);GError *e=NULL;int count=atspi_accessible_get_child_count(desktop,&e);
        if(e){g_clear_error(&e);continue;}
        for(int i=0;i<count;++i) {AtspiAccessible *candidate=atspi_accessible_get_child_at_index(desktop,i,&e);if(e){g_clear_error(&e);continue;}if(!candidate)continue;
            guint pid=atspi_accessible_get_process_id(candidate,&e);if(!e && pid==(guint)child){app=candidate;break;}g_clear_error(&e);g_object_unref(candidate);}
    }
    CHECK(app);AtspiAccessible *field=find(app,"External document",0);CHECK(field);
    CHECK(cache_probe(field,1,0));GError *e=NULL;
    AtspiAccessible *root=atspi_accessible_get_parent(field,&e);CHECK(root && !e && atspi_accessible_get_child_count(root,&e)==3073 && !e);
    AtspiAccessible *last=atspi_accessible_get_child_at_index(root,3072,&e);CHECK(last && !e);
    char *name=atspi_accessible_get_name(last,&e);CHECK(name && !e && strlen(name)==1536);
    for(unsigned i=0;i<1536;++i)CHECK(name[i]=='c');
    g_free(name);AtspiText *text=atspi_accessible_get_text_iface(field);CHECK(text && atspi_text_get_character_count(text,&e)==4 && !e);
    CHECK(fprintf(commands,"quit\n")>0 && !fflush(commands));int status=0;
    CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));child=-1;
    g_object_unref(text);g_object_unref(last);g_object_unref(root);g_object_unref(field);g_object_unref(app);g_object_unref(desktop);atspi_exit();cleanup();
    puts("AT-SPI cache: genuine metadata over4MiB rejected; complete tree and individual native text/name queries remain available; clean close passed.");return 0;
}
