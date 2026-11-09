/* Independent OS peer. The host uses explicit in-memory clipboard callbacks;
   real delayed Wayland transfers have their own separate protocol fixture. */
#define main original_large_text_client_main
#include "atspi_large_text_client.c"
#undef main
#undef CHECK
#include "atspi_cache_probe.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"document-clipboard-atspi:%d: %s\n",__LINE__,#x);cleanup();return 1;}} while(0)
static int equals(AtspiText *text,const char *expected) {
    GError *e=NULL;char *actual=atspi_text_get_text(text,0,-1,&e);
    int okay=!e && actual && !strcmp(actual,expected);g_clear_error(&e);g_free(actual);return okay;
}
static AtspiAccessible *fresh(AtspiAccessible *app) {
    pump(30);atspi_accessible_clear_cache(app);return find(app,"External document",0);
}
int main(int argc,char **argv) {
    CHECK(argc==2);int in[2],out[2];CHECK(!pipe(in) && !pipe(out));child=fork();CHECK(child>=0);
    if(!child){dup2(in[0],STDIN_FILENO);dup2(out[1],STDOUT_FILENO);close(in[0]);close(in[1]);close(out[0]);close(out[1]);execl(argv[1],argv[1],"--clipboard",(char *)NULL);_exit(127);}
    close(in[0]);close(out[1]);commands=fdopen(in[1],"w");responses=fdopen(out[0],"r");CHECK(commands && responses);
    setvbuf(responses,NULL,_IONBF,0);CHECK(response("READY\n") && !atspi_init());atspi_set_timeout(1000,1000);
    AtspiAccessible *desktop=atspi_get_desktop(0),*app=NULL;CHECK(desktop);
    for(unsigned tries=0;tries<300 && !app;++tries) {
        pump(10);atspi_accessible_clear_cache(desktop);GError *e=NULL;int count=atspi_accessible_get_child_count(desktop,&e);
        if(e){g_clear_error(&e);continue;}
        for(int i=0;i<count;++i) {AtspiAccessible *candidate=atspi_accessible_get_child_at_index(desktop,i,&e);if(e){g_clear_error(&e);continue;}if(!candidate)continue;
            guint pid=atspi_accessible_get_process_id(candidate,&e);if(!e && pid==(guint)child){app=candidate;break;}g_clear_error(&e);g_object_unref(candidate);}
    }
    CHECK(app);AtspiAccessible *field=fresh(app);CHECK(field);GError *e=NULL;
    AtspiText *text=atspi_accessible_get_text_iface(field);AtspiEditableText *edit=atspi_accessible_get_editable_text_iface(field);CHECK(text && edit && cache_probe(field,0,65536));
    CHECK(atspi_editable_text_copy_text(edit,1,3,&e) && !e && command("clip-check") && equals(text,"A\xc3\xa9\xf0\x9f\x8c\x8dZ"));
    CHECK(atspi_editable_text_cut_text(edit,1,3,&e) && !e && command("clip-check") && equals(text,"AZ"));
    CHECK(atspi_editable_text_paste_text(edit,1,&e) && !e && equals(text,"A\xc3\xa9\xf0\x9f\x8c\x8dZ"));
    CHECK(command("clipfail"));(void)atspi_editable_text_copy_text(edit,1,3,&e);CHECK(e);g_clear_error(&e);
    CHECK(!atspi_editable_text_cut_text(edit,1,3,&e) && !e && !atspi_editable_text_paste_text(edit,1,&e) && !e && equals(text,"A\xc3\xa9\xf0\x9f\x8c\x8dZ"));
    CHECK(command("clipok") && command("fail") && !atspi_editable_text_cut_text(edit,1,3,&e) && !e && command("clip-check") && equals(text,"A\xc3\xa9\xf0\x9f\x8c\x8dZ"));
    CHECK(command("readonly"));g_object_unref(edit);g_object_unref(text);g_object_unref(field);field=fresh(app);CHECK(field);
    text=atspi_accessible_get_text_iface(field);edit=atspi_accessible_get_editable_text_iface(field);CHECK(text && edit);
    CHECK(atspi_editable_text_copy_text(edit,1,3,&e) && !e && command("clip-check"));
    CHECK(!atspi_editable_text_cut_text(edit,1,3,&e) && !e && !atspi_editable_text_paste_text(edit,1,&e) && !e && equals(text,"A\xc3\xa9\xf0\x9f\x8c\x8dZ"));
    CHECK(command("large"));g_object_unref(edit);g_object_unref(text);g_object_unref(field);field=fresh(app);CHECK(field);
    text=atspi_accessible_get_text_iface(field);edit=atspi_accessible_get_editable_text_iface(field);CHECK(text && edit && atspi_text_get_character_count(text,&e)==8388608 && !e);
    (void)atspi_editable_text_copy_text(edit,0,8388608,&e);CHECK(e);g_clear_error(&e);
    CHECK(atspi_editable_text_copy_text(edit,0,1,&e) && !e && command("writable"));
    g_object_unref(edit);g_object_unref(text);g_object_unref(field);field=fresh(app);CHECK(field);
    text=atspi_accessible_get_text_iface(field);edit=atspi_accessible_get_editable_text_iface(field);CHECK(text && edit);
    CHECK(!atspi_editable_text_paste_text(edit,0,&e) && !e && atspi_text_get_character_count(text,&e)==8388608 && !e);
    CHECK(atspi_editable_text_cut_text(edit,0,1,&e) && !e && atspi_text_get_character_count(text,&e)==8388607 && !e);
    CHECK(atspi_editable_text_paste_text(edit,0,&e) && !e && atspi_text_get_character_count(text,&e)==8388608 && !e && cache_probe(field,0,65536));
    char *prefix=atspi_text_get_text(text,0,2,&e);CHECK(prefix && !e && !strcmp(prefix,"Ax"));g_free(prefix);
    CHECK(fprintf(commands,"quit\n")>0 && !fflush(commands));int status=0;
    CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));child=-1;
    g_object_unref(edit);g_object_unref(text);g_object_unref(field);g_object_unref(app);g_object_unref(desktop);atspi_exit();cleanup();
    puts("AT-SPI application document clipboard: scalar Unicode copy/cut/paste, read-only copy, actual callback failures, full8MiB bound and clean close passed; clipboard callbacks are an in-memory fixture.");return 0;
}
