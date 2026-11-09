/* Reuse the independent peer's process/discovery/async-D-Bus helpers. This
   executable links only the OS AT-SPI stack, never NexaUI or a document model. */
#define main original_large_text_client_main
#include "atspi_large_text_client.c"
#undef main
#undef CHECK
#define CHECK(x) do {if(!(x)){fprintf(stderr,"document-atspi:%d: %s\n",__LINE__,#x);cleanup();return 1;}} while(0)
static int equals(AtspiText *text,const char *expected) {
    GError *e=NULL;char *actual=atspi_text_get_text(text,0,-1,&e);
    int okay=!e && actual && !strcmp(actual,expected);g_clear_error(&e);g_free(actual);return okay;
}
static AtspiAccessible *fresh(AtspiAccessible *app) {
    pump(30);atspi_accessible_clear_cache(app);return find(app,"External document",0);
}
int main(int argc,char **argv) {
    CHECK(argc==2);int in[2],out[2];CHECK(!pipe(in) && !pipe(out));child=fork();CHECK(child>=0);
    if(!child){dup2(in[0],STDIN_FILENO);dup2(out[1],STDOUT_FILENO);close(in[0]);close(in[1]);close(out[0]);close(out[1]);execl(argv[1],argv[1],(char *)NULL);_exit(127);}
    close(in[0]);close(out[1]);commands=fdopen(in[1],"w");responses=fdopen(out[0],"r");CHECK(commands && responses);
    setvbuf(responses,NULL,_IONBF,0);CHECK(response("READY\n") && !atspi_init());atspi_set_timeout(1000,1000);
    AtspiAccessible *desktop=atspi_get_desktop(0),*app=NULL;CHECK(desktop);
    for(unsigned tries=0;tries<300 && !app;++tries) {
        pump(10);atspi_accessible_clear_cache(desktop);GError *e=NULL;int count=atspi_accessible_get_child_count(desktop,&e);
        if(e){g_clear_error(&e);continue;}
        for(int i=0;i<count;++i){AtspiAccessible *a=atspi_accessible_get_child_at_index(desktop,i,&e);if(e){g_clear_error(&e);continue;}if(!a)continue;
            guint pid=atspi_accessible_get_process_id(a,&e);if(!e && pid==(guint)child){app=a;break;}g_clear_error(&e);g_object_unref(a);}
    }
    CHECK(app);AtspiAccessible *field=fresh(app);CHECK(field);AtspiText *text=atspi_accessible_get_text_iface(field);AtspiEditableText *edit=atspi_accessible_get_editable_text_iface(field);CHECK(text && edit);
    GError *e=NULL;CHECK(atspi_text_get_character_count(text,&e)==4 && !e && equals(text,"A\xc3\xa9\xf0\x9f\x8c\x8dZ"));
    CHECK(atspi_editable_text_insert_text(edit,2,"\xce\xa9",2,&e) && !e && equals(text,"A\xc3\xa9\xce\xa9\xf0\x9f\x8c\x8dZ"));
    CHECK(atspi_text_get_caret_offset(text,&e)==3 && !e && atspi_editable_text_delete_text(edit,1,3,&e) && !e && equals(text,"A\xf0\x9f\x8c\x8dZ"));
    CHECK(atspi_editable_text_set_text_contents(edit,"a\xcc\x81" "b",&e) && !e);
    CHECK(atspi_text_add_selection(text,0,2,&e) && !e);AtspiRange *range=atspi_text_get_selection(text,0,&e);
    CHECK(!e && range && range->start_offset==0 && range->end_offset==2);g_free(range);
    CHECK(atspi_text_remove_selection(text,0,&e) && !e);
    CHECK(!atspi_editable_text_insert_text(edit,1,"\xc3\xa9",1,&e));g_clear_error(&e);CHECK(equals(text,"a\xcc\x81" "b"));
    /* libatspi CopyText returns TRUE even on a D-Bus error (void wire reply).
       Assert the actual native rejection instead of that wrapper's boolean. */
    (void)atspi_editable_text_copy_text(edit,0,1,&e);CHECK(e);g_clear_error(&e);CHECK(equals(text,"a\xcc\x81" "b"));
    CHECK(command("fail") && !atspi_editable_text_set_text_contents(edit,"rejected",&e));g_clear_error(&e);CHECK(equals(text,"a\xcc\x81" "b"));
    DBusError de;dbus_error_init(&de);DBusConnection *bus=dbus_connection_open_private(getenv("AT_SPI_BUS_ADDRESS"),&de);
    CHECK(bus && dbus_bus_register(bus,&de) && command("pause"));dbus_connection_set_exit_on_disconnect(bus,FALSE);
    DBusPendingCall *pending=write_async(bus,field,"stale");CHECK(pending);
    for(unsigned i=0;i<100;++i)dbus_connection_read_write_dispatch(bus,1);
    CHECK(!dbus_pending_call_get_completed(pending) && command("local") && command("resume") && pending_result(bus,pending,0,1000));
    CHECK(equals(text,"local"));
    CHECK(command("disable") && !atspi_editable_text_set_text_contents(edit,"disabled",&e));g_clear_error(&e);CHECK(command("enable") && equals(text,"local"));
    CHECK(command("readonly"));g_object_unref(text);g_object_unref(edit);g_object_unref(field);field=fresh(app);CHECK(field);
    text=atspi_accessible_get_text_iface(field);CHECK(text && !atspi_accessible_get_editable_text_iface(field) && equals(text,"local"));
    CHECK(atspi_text_set_caret_offset(text,1,&e) && !e);
    CHECK(command("large"));g_object_unref(text);g_object_unref(field);field=fresh(app);CHECK(field);text=atspi_accessible_get_text_iface(field);CHECK(text);
    const int maximum=8*1024*1024;CHECK(atspi_text_get_character_count(text,&e)==maximum && !e && atspi_text_get_caret_offset(text,&e)==maximum && !e);
    for(int start=0;start<maximum;start+=512*1024) {
        int end=start+512*1024;char *part=atspi_text_get_text(text,start,end,&e);CHECK(!e && part && strlen(part)==(size_t)(end-start));
        for(int i=0;i<end-start;++i)CHECK(part[i]==(start+i==0?'A':start+i==maximum-1?'Z':'x'));
        g_free(part);
    }
    char *too_large=atspi_text_get_text(text,0,-1,&e);CHECK(e);g_clear_error(&e);g_free(too_large);
    CHECK(command("writable"));g_object_unref(text);g_object_unref(field);field=fresh(app);CHECK(field);
    text=atspi_accessible_get_text_iface(field);edit=atspi_accessible_get_editable_text_iface(field);CHECK(text && edit);
    CHECK(atspi_text_set_caret_offset(text,4,&e) && !e && atspi_text_get_caret_offset(text,&e)==4 && !e);
    CHECK(atspi_editable_text_delete_text(edit,0,maximum,&e) && !e && equals(text,""));
    CHECK(atspi_editable_text_set_text_contents(edit,"retained",&e) && !e && command("pause"));pending=write_async(bus,field,"detached");CHECK(pending);
    for(unsigned i=0;i<100;++i)dbus_connection_read_write_dispatch(bus,1);
    CHECK(!dbus_pending_call_get_completed(pending) && command("detach") && pending_result(bus,pending,0,1000));
    dbus_connection_close(bus);dbus_connection_unref(bus);dbus_error_free(&de);
    CHECK(fprintf(commands,"quit\n")>0 && !fflush(commands));int status=0;
    CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));child=-1;
    g_object_unref(edit);g_object_unref(text);g_object_unref(field);g_object_unref(app);g_object_unref(desktop);atspi_exit();cleanup();
    puts("AT-SPI application document: scalar edits/selection, actual completion and stale/read-only/disabled/lease rejection; full8MiB chunked reads and delete; clean close passed.");return 0;
}
