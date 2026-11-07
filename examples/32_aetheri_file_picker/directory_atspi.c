#include "peer_helpers.h"
int main(int argc,char **argv) {
    int okay=0,initialized=0,cancel=0;AtspiAccessible *desktop=NULL,*app=NULL,*row=NULL,*field=NULL,*node=NULL;
    CHECK(argc>=2&&argc<=4);for(int i=2;i<argc;++i){if(!strcmp(argv[i],"--german"))german=1;else if(!strcmp(argv[i],"--cancel"))cancel=1;else CHECK(0);}
    char *end;unsigned long pid=strtoul(argv[1],&end,10);CHECK(pid&&!*end&&pid<=2147483647UL);
    setvbuf(stdout,NULL,_IONBF,0);CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    row=wait_for(desktop,pid,&app,"folder",ATSPI_ROLE_LIST_ITEM);CHECK(row&&action(row,"select")&&focus(row)&&row_visible(app,row));
    node=lookup(app,name("Choose a folder","Ordner auswählen"),ATSPI_ROLE_PANEL);CHECK(node);g_object_unref(node);node=NULL;
    CHECK(step(0));node=wait_for(desktop,pid,&app,"é.txt",ATSPI_ROLE_LIST_ITEM);CHECK(node&&retired(row)&&!action(row,"select"));g_object_unref(node);node=NULL;
    field=lookup(app,name("Name filter","Namensfilter"),ATSPI_ROLE_ENTRY);CHECK(field&&focus(field)&&edit(field,"no-such-entry"));
    double until=now()+3;while(now()<until&&!text_is(field,"no-such-entry"))pump();CHECK(text_is(field,"no-such-entry"));puts("ACK 0");
    CHECK(step(1));node=wait_for(desktop,pid,&app,name("No matching entries","Keine passenden Einträge"),ATSPI_ROLE_STATUS_BAR);CHECK(node);g_object_unref(node);node=NULL;
    node=lookup(app,german?"0–0 von 0 Einträgen (2 insgesamt)":"0–0 of 0 entries (2 total)",ATSPI_ROLE_LABEL);CHECK(node);g_object_unref(node);node=NULL;
    CHECK(retired(field)&&!edit(field,"retired directory draft"));g_object_unref(field);field=NULL;
    node=lookup(app,name("Choose this folder","Diesen Ordner auswählen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(node&&state(node,ATSPI_STATE_ENABLED));g_object_unref(node);node=NULL;puts("ACK 1");
    CHECK(step(2));node=lookup(app,"folder",ATSPI_ROLE_LABEL);CHECK(node);g_object_unref(node);node=NULL;
    node=lookup(app,cancel?name("Cancel","Abbrechen"):name("Choose this folder","Diesen Ordner auswählen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(node&&state(node,ATSPI_STATE_ENABLED)&&focus(node));puts("ACK 2");
    puts("PICKER_NATIVE_DIRECTORY_FILTERED_EMPTY_AND_HELD_PATH_REPLACEMENT_VERIFIED");okay=1;
cleanup:
    if(node)g_object_unref(node);
    if(field)g_object_unref(field);
    if(row)g_object_unref(row);
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    if(initialized)atspi_exit();
    return okay?0:1;
}
