#include "peer_helpers.h"
static AtspiAccessible *wait_owner(AtspiAccessible *desktop,unsigned long pid,AtspiAccessible **app,const char *text){
    double until=now()+4;AtspiAccessible *node=NULL;
    while(now()<until){node=wait_for(desktop,pid,app,text,ATSPI_ROLE_PUSH_BUTTON);if(node&&state(node,ATSPI_STATE_FOCUSED))return node;if(node)g_object_unref(node);pump();}
    return NULL;
}
int main(int argc,char **argv){
    int okay=0,initialized=0,maximum=0,held=0,ime=0;AtspiAccessible *desktop=NULL,*app=NULL,*node=NULL,*note=NULL,*trigger=NULL,*row=NULL,*open=NULL,*field=NULL;
    CHECK(argc>=2&&argc<=6);for(int i=2;i<argc;++i){if(!strcmp(argv[i],"--german"))german=1;else if(!strcmp(argv[i],"--maximum"))maximum=1;else if(!strcmp(argv[i],"--pending-cancel"))held=1;else if(!strcmp(argv[i],"--ime"))ime=1;else CHECK(0);}
    const char *draft=ime?"owner drafté":"owner draft";
    char *end;unsigned long pid=strtoul(argv[1],&end,10);CHECK(pid&&!*end&&pid<=2147483647UL);
    setvbuf(stdout,NULL,_IONBF,0);CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    trigger=wait_for(desktop,pid,&app,name("Choose file…","Datei auswählen…"),ATSPI_ROLE_PUSH_BUTTON);CHECK(trigger);
    note=lookup(app,name("Owner note","Notiz"),ATSPI_ROLE_ENTRY);CHECK(note&&edit(note,"owner draft"));double until=now()+3;while(now()<until&&!text_is(note,"owner draft"))pump();CHECK(text_is(note,"owner draft")&&focus(ime?note:trigger));
    CHECK(step(0));node=wait_for(desktop,pid,&app,name("Choose a file","Datei auswählen"),ATSPI_ROLE_DIALOG);CHECK(node);g_object_unref(node);node=NULL;
    CHECK(retired(trigger)&&!action(trigger,"click")&&retired(note)&&!edit(note,"retired edit"));g_object_unref(trigger);trigger=NULL;g_object_unref(note);note=NULL;
    trigger=lookup(app,name("Choose folder…","Ordner auswählen…"),ATSPI_ROLE_PUSH_BUTTON);note=lookup(app,name("Owner note","Notiz"),ATSPI_ROLE_ENTRY);
    CHECK(trigger&&note&&!action(trigger,"click")&&!edit(note,"blocked edit")&&text_is(note,draft));g_object_unref(trigger);trigger=NULL;g_object_unref(note);note=NULL;
    row=wait_for(desktop,pid,&app,"folder",ATSPI_ROLE_LIST_ITEM);CHECK(row&&focus(row)&&row_visible(app,row));puts("ACK 0");
    CHECK(step(1));g_object_unref(row);row=wait_for(desktop,pid,&app,maximum?"snapshot-4094.txt":"snapshot-0039.txt",ATSPI_ROLE_LIST_ITEM);CHECK(row&&state(row,ATSPI_STATE_SELECTED)&&state(row,ATSPI_STATE_FOCUSED)&&row_visible(app,row));
    field=lookup(app,name("Name filter","Namensfilter"),ATSPI_ROLE_ENTRY);open=lookup(app,name("Open","Öffnen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(field&&open&&focus(open));puts("ACK 1");
    CHECK(step(2));trigger=wait_owner(desktop,pid,&app,name("Choose file…","Datei auswählen…"));CHECK(trigger&&state(trigger,ATSPI_STATE_FOCUSED));
    CHECK(retired(row)&&!action(row,"select")&&retired(open)&&!action(open,"click")&&retired(field)&&!edit(field,"late picker edit"));g_object_unref(row);row=NULL;g_object_unref(open);open=NULL;g_object_unref(field);field=NULL;
    node=lookup(app,maximum?"snapshot-4094.txt":"snapshot-0039.txt",ATSPI_ROLE_LABEL);CHECK(node);g_object_unref(node);node=NULL;
    note=lookup(app,name("Owner note","Notiz"),ATSPI_ROLE_ENTRY);CHECK(note&&text_is(note,draft));g_object_unref(note);note=NULL;g_object_unref(trigger);trigger=lookup(app,name("Choose folder…","Ordner auswählen…"),ATSPI_ROLE_PUSH_BUTTON);CHECK(trigger&&focus(trigger));puts("ACK 2");
    CHECK(step(3));node=wait_for(desktop,pid,&app,name("Choose a folder","Ordner auswählen"),ATSPI_ROLE_DIALOG);CHECK(node&&retired(trigger));g_object_unref(node);node=NULL;g_object_unref(trigger);trigger=NULL;
    row=wait_for(desktop,pid,&app,"folder",ATSPI_ROLE_LIST_ITEM);CHECK(row&&focus(row));puts("ACK 3");
    CHECK(step(4));node=wait_for(desktop,pid,&app,"é.txt",ATSPI_ROLE_LIST_ITEM);CHECK(node&&retired(row));g_object_unref(node);node=NULL;g_object_unref(row);row=NULL;
    open=lookup(app,name("Choose this folder","Diesen Ordner auswählen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(open&&focus(open));puts("ACK 4");
    CHECK(step(5));trigger=wait_owner(desktop,pid,&app,name("Choose folder…","Ordner auswählen…"));CHECK(trigger&&state(trigger,ATSPI_STATE_FOCUSED)&&retired(open)&&!action(open,"click"));g_object_unref(open);open=NULL;
    node=lookup(app,"folder",ATSPI_ROLE_LABEL);CHECK(node);g_object_unref(node);node=NULL;
    note=lookup(app,name("Owner note","Notiz"),ATSPI_ROLE_ENTRY);CHECK(note&&text_is(note,draft));g_object_unref(note);note=NULL;g_object_unref(trigger);trigger=lookup(app,name("Choose file…","Datei auswählen…"),ATSPI_ROLE_PUSH_BUTTON);CHECK(trigger&&focus(trigger));puts("ACK 5");
    CHECK(step(6));node=wait_for(desktop,pid,&app,name("Choose a file","Datei auswählen"),ATSPI_ROLE_DIALOG);CHECK(node&&retired(trigger));g_object_unref(node);node=NULL;g_object_unref(trigger);trigger=NULL;
    node=wait_for(desktop,pid,&app,"é.txt",ATSPI_ROLE_LIST_ITEM);CHECK(node);g_object_unref(node);node=NULL;
    if(held){
        node=lookup(app,name("Refresh","Aktualisieren"),ATSPI_ROLE_PUSH_BUTTON);CHECK(node&&focus(node));g_object_unref(node);node=NULL;puts("ACK 6");
        CHECK(step(7));node=wait_for(desktop,pid,&app,name("Reading folder…","Ordner wird gelesen…"),ATSPI_ROLE_STATUS_BAR);CHECK(node&&state(node,ATSPI_STATE_BUSY));g_object_unref(node);node=NULL;
        puts("ACK 7");CHECK(step(8));
    }else{
        node=lookup(app,name("Cancel","Abbrechen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(node&&focus(node));g_object_unref(node);node=NULL;puts("ACK 6");CHECK(step(7));
    }
    trigger=wait_owner(desktop,pid,&app,name("Choose file…","Datei auswählen…"));CHECK(trigger&&state(trigger,ATSPI_STATE_FOCUSED));
    node=lookup(app,"folder",ATSPI_ROLE_LABEL);CHECK(node);g_object_unref(node);node=NULL;note=lookup(app,name("Owner note","Notiz"),ATSPI_ROLE_ENTRY);CHECK(note&&text_is(note,draft));
    node=lookup(app,name("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(node&&focus(node));printf("ACK %u\n",held?8u:7u);
    puts("PICKER_NATIVE_OWNER_FILE_DIRECTORY_MODAL_RETIREMENT_CANCEL_AND_FOCUS_VERIFIED");okay=1;
cleanup:
    if(field)g_object_unref(field);
    if(open)g_object_unref(open);
    if(row)g_object_unref(row);
    if(trigger)g_object_unref(trigger);
    if(note)g_object_unref(note);
    if(node)g_object_unref(node);
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    if(initialized)atspi_exit();
    return okay?0:1;
}
