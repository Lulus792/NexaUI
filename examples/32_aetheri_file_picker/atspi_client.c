#include "peer_helpers.h"
int main(int argc,char **argv){
    int okay=0,initialized=0,cancel=0,maximum=0,pending=0,no_ime=0;AtspiAccessible *desktop=NULL,*app=NULL,*folder=NULL,*field=NULL,*old_field=NULL,*entry=NULL,*button=NULL,*old_last=NULL;
    CHECK(argc>=2&&argc<=7);for(int i=2;i<argc;++i){if(!strcmp(argv[i],"--german"))german=1;else if(!strcmp(argv[i],"--cancel"))cancel=1;else if(!strcmp(argv[i],"--maximum"))maximum=1;else if(!strcmp(argv[i],"--pending-cancel"))pending=cancel=1;else if(!strcmp(argv[i],"--no-ime"))no_ime=1;else CHECK(0);}
    char *end;unsigned long pid=strtoul(argv[1],&end,10);CHECK(pid&&!*end&&pid<=2147483647UL);
    setvbuf(stdout,NULL,_IONBF,0);CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    folder=wait_for(desktop,pid,&app,"folder",ATSPI_ROLE_LIST_ITEM);CHECK(folder&&action(folder,"select")&&focus(folder));
    CHECK(step(0));entry=wait_for(desktop,pid,&app,"é.txt",ATSPI_ROLE_LIST_ITEM);CHECK(entry&&retired(folder)&&!action(folder,"select"));puts("ACK 0");
    field=lookup(app,name("Name filter","Namensfilter"),ATSPI_ROLE_ENTRY);CHECK(field&&focus(field)&&text_is(field,""));
    CHECK(step(1)&&text_is(field,""));puts("ACK 1");
    CHECK(step(2)&&text_is(field,""));button=lookup(app,name("Apply filter","Filter anwenden"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&state(button,ATSPI_STATE_FOCUSED));
    g_object_unref(button);button=NULL;CHECK(focus(field));
    if(no_ime)CHECK(edit(field,"é"));
    puts("ACK 2");
    CHECK(step(3));double until=now()+3;while(now()<until&&!text_is(field,"é"))pump();CHECK(text_is(field,"é"));puts("ACK 3");
    CHECK(step(4));old_field=field;field=NULL;g_object_unref(entry);entry=NULL;
    button=wait_for(desktop,pid,&app,german?"1–1 von 1 Einträgen (2 insgesamt)":"1–1 of 1 entries (2 total)",ATSPI_ROLE_LABEL);CHECK(button);g_object_unref(button);button=NULL;
    entry=lookup(app,"é.txt",ATSPI_ROLE_LIST_ITEM);CHECK(entry&&retired(old_field)&&!edit(old_field,"late retired edit"));
    field=lookup(app,name("Name filter","Namensfilter"),ATSPI_ROLE_ENTRY);CHECK(field&&text_is(field,"é")&&focus(field)&&edit(field,""));
    until=now()+3;while(now()<until&&!text_is(field,""))pump();CHECK(text_is(field,""));puts("ACK 4");
    CHECK(step(5));button=wait_for(desktop,pid,&app,"other.txt",ATSPI_ROLE_LIST_ITEM);CHECK(button);g_object_unref(button);button=NULL;
    button=lookup(app,name("Folder path","Ordnerpfad"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&action(button,"expand"));g_object_unref(button);button=NULL;
    button=wait_for(desktop,pid,&app,name("Ancestor: Root","Vorfahre: Root"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));g_object_unref(button);button=NULL;puts("ACK 5");
    CHECK(step(6));g_object_unref(folder);folder=wait_for(desktop,pid,&app,"folder",ATSPI_ROLE_LIST_ITEM);CHECK(folder&&retired(entry)&&focus(folder));puts("ACK 6");
    CHECK(step(7));button=wait_for(desktop,pid,&app,maximum?"snapshot-4094.txt":"snapshot-0039.txt",ATSPI_ROLE_LIST_ITEM);
    CHECK(button&&state(button,ATSPI_STATE_FOCUSED)&&state(button,ATSPI_STATE_SELECTED)&&row_visible(app,button)&&retired(folder)&&!action(folder,"select"));g_object_unref(button);button=NULL;puts("ACK 7");
    CHECK(step(8));button=lookup(app,name("Next page","Nächste Seite"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&!state(button,ATSPI_STATE_ENABLED)&&!action(button,"click"));g_object_unref(button);button=NULL;
    old_last=lookup(app,maximum?"snapshot-4094.txt":"snapshot-0039.txt",ATSPI_ROLE_LIST_ITEM);CHECK(old_last);
    button=lookup(app,name("Previous page","Vorherige Seite"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&action(button,"click"));g_object_unref(button);button=NULL;
    const char *counts=maximum?name("4065–4080 of 4096 entries (4096 total)","4065–4080 von 4096 Einträgen (4096 insgesamt)"):
        name("17–32 of 41 entries (41 total)","17–32 von 41 Einträgen (41 insgesamt)");
    button=wait_for(desktop,pid,&app,counts,ATSPI_ROLE_LABEL);CHECK(button&&retired(old_last));g_object_unref(button);button=NULL;
    button=lookup(app,name("Open","Öffnen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&!state(button,ATSPI_STATE_ENABLED));g_object_unref(button);button=NULL;
    button=lookup(app,name("Next page","Nächste Seite"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&action(button,"click"));g_object_unref(button);button=NULL;
    button=wait_for(desktop,pid,&app,maximum?"snapshot-4094.txt":"snapshot-0039.txt",ATSPI_ROLE_LIST_ITEM);
    CHECK(button&&retired(old_last)&&!action(old_last,"select")&&action(button,"select")&&focus(button)&&row_visible(app,button));g_object_unref(button);button=NULL;
    if(pending){
        button=lookup(app,name("Refresh","Aktualisieren"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&action(button,"click"));g_object_unref(button);button=NULL;
        button=wait_for(desktop,pid,&app,name("Reading folder…","Ordner wird gelesen…"),ATSPI_ROLE_STATUS_BAR);CHECK(button&&state(button,ATSPI_STATE_BUSY));g_object_unref(button);button=NULL;
    }
    if(cancel){button=lookup(app,name("Cancel","Abbrechen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&state(button,ATSPI_STATE_ENABLED)&&focus(button));}
    puts("ACK 8");puts("PICKER_NATIVE_KEYS_IME_FILTER_PAGING_AND_RETIREMENT_VERIFIED");okay=1;
cleanup:
    if(old_last)g_object_unref(old_last);
    if(button)g_object_unref(button);
    if(entry)g_object_unref(entry);
    if(old_field)g_object_unref(old_field);
    if(field)g_object_unref(field);
    if(folder)g_object_unref(folder);
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    if(initialized)atspi_exit();
    return okay?0:1;
}
