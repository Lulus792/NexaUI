#include "peer_helpers.h"
static int rectangle(AtspiAccessible *node,AtspiRect *out){
    AtspiComponent *c=node?atspi_accessible_get_component_iface(node):NULL;GError *error=NULL;
    AtspiRect *r=c?atspi_component_get_extents(c,ATSPI_COORD_TYPE_WINDOW,&error):NULL;int yes=!error&&r;
    if(yes)*out=*r;
    if(r)g_boxed_free(ATSPI_TYPE_RECT,r);
    if(error)g_error_free(error);
    if(c)g_object_unref(c);
    return yes;
}
static AtspiAccessible *wait_focused(AtspiAccessible *desktop,unsigned long pid,AtspiAccessible **app,const char *text,AtspiRole role){
    double until=now()+4;
    while(now()<until){AtspiAccessible *node=wait_for(desktop,pid,app,text,role);if(node&&state(node,ATSPI_STATE_FOCUSED))return node;if(node)g_object_unref(node);pump();}
    return NULL;
}
static int read_review(AtspiAccessible *app,AtspiAccessible *label,AtspiAccessible *back){
    int okay=0;AtspiAccessible *area=lookup(app,name("Existing file review","Prüfung der vorhandenen Datei"),ATSPI_ROLE_SCROLL_PANE);
    AtspiRect first,outer,current;char message[64];CHECK(rectangle(label,&first)&&rectangle(area,&outer)&&outer.height>0&&first.height>0&&state(back,ATSPI_STATE_FOCUSED));
    long long overflow=(long long)first.y+first.height-outer.y-outer.height;
    unsigned pages=overflow>0?(unsigned)((2*overflow+outer.height-1)/outer.height):0;
    CHECK(pages<256);
    if(!pages)CHECK(first.x>=outer.x&&first.y>=outer.y&&(long long)first.x+first.width<=(long long)outer.x+outer.width&&(long long)first.y+first.height<=(long long)outer.y+outer.height);
    printf("SAVE_READ %u\n",pages);current=first;
    for(unsigned direction=0;direction<2;++direction)for(unsigned i=0;i<pages;++i){
        CHECK(fgets(message,sizeof message,stdin));char wanted[64];snprintf(wanted,sizeof wanted,"SAVE_%s %u\n",direction?"UP":"DOWN",i);CHECK(!strcmp(message,wanted));
        AtspiRect next;CHECK(rectangle(label,&next)&&state(back,ATSPI_STATE_FOCUSED));
        CHECK(direction?next.y>=current.y&&(long long)next.y-current.y<=outer.height/2+1:next.y<=current.y&&(long long)current.y-next.y<=outer.height/2+1);
        current=next;
        if(!direction&&i+1==pages)CHECK((long long)current.y+current.height<=(long long)outer.y+outer.height&&(long long)current.y+current.height>outer.y+outer.height/2);
        printf("SAVE_%s_ACK %u\n",direction?"UP":"DOWN",i);
    }
    CHECK(current.y==first.y);puts("SAVE_REVIEW_COMPLETE_RAW_LABEL_READING_AND_SAFE_FOCUS_VERIFIED");okay=1;
cleanup:
    if(area)g_object_unref(area);
    return okay;
}
int main(int argc,char **argv){
    int okay=0,initialized=0,ime=0;AtspiAccessible *desktop=NULL,*app=NULL,*trigger=NULL,*field=NULL,*back=NULL,*label=NULL,*row=NULL,*confirm=NULL,*note=NULL;
    CHECK(argc>=2&&argc<=5);for(int i=2;i<argc;++i){if(!strcmp(argv[i],"--german"))german=1;else if(!strcmp(argv[i],"--ime"))ime=1;else CHECK(!strcmp(argv[i],"--maximum"));}
    char *end;unsigned long pid=strtoul(argv[1],&end,10);CHECK(pid&&!*end&&pid<=2147483647UL);
    char raw_label[1021];for(size_t i=0;i<255;++i)memcpy(raw_label+i*4,"\\xff",4);raw_label[1020]=0;
    setvbuf(stdout,NULL,_IONBF,0);CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    trigger=wait_for(desktop,pid,&app,name("Save as…","Speichern unter…"),ATSPI_ROLE_PUSH_BUTTON);
    note=lookup(app,name("Owner note","Notiz"),ATSPI_ROLE_ENTRY);CHECK(trigger&&note&&edit(note,"save draft"));
    double until=now()+3;while(now()<until&&!text_is(note,"save draft"))pump();CHECK(text_is(note,"save draft")&&focus(trigger));
    CHECK(step(0));field=wait_for(desktop,pid,&app,name("File name","Dateiname"),ATSPI_ROLE_ENTRY);CHECK(field&&retired(trigger)&&retired(note));
    g_object_unref(trigger);trigger=NULL;g_object_unref(note);note=NULL;
    CHECK(focus(field));until=now()+3;while(now()<until&&!row_visible(app,field))pump();CHECK(row_visible(app,field)&&text_is(field,""));
    CHECK(edit(field,"snapshot-0000.txt"));until=now()+3;while(now()<until&&!text_is(field,"snapshot-0000.txt"))pump();CHECK(text_is(field,"snapshot-0000.txt"));puts("ACK 0");
    CHECK(step(1));back=wait_focused(desktop,pid,&app,name("Back","Zurück"),ATSPI_ROLE_PUSH_BUTTON);CHECK(back&&retired(field)&&!edit(field,"late edit"));g_object_unref(field);field=NULL;
    label=lookup(app,"snapshot-0000.txt",ATSPI_ROLE_LABEL);CHECK(label);g_object_unref(label);label=NULL;
    confirm=lookup(app,name("Replace this file","Diese Datei ersetzen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(confirm);
    AtspiAccessible *review=lookup(app,name("Replace existing file?","Vorhandene Datei ersetzen?"),ATSPI_ROLE_DIALOG);CHECK(review);
    AtspiAccessible *thumb=lookup(review,"Vertical scrollbar",ATSPI_ROLE_SLIDER);g_object_unref(review);
    unsigned tabs=thumb?2u:1u;printf("SAVE_TAB_READY %u\n",tabs);
    for(unsigned direction=0;direction<2;++direction)for(unsigned i=0;i<tabs;++i){
        char message[64],wanted[64];snprintf(wanted,sizeof wanted,"SAVE_%s_CHECK %u\n",direction?"SHIFT_TAB":"TAB",i);
        CHECK(fgets(message,sizeof message,stdin)&&!strcmp(message,wanted));
        AtspiAccessible *expected=i+1==tabs?(direction?back:confirm):thumb;
        until=now()+3;while(now()<until&&!state(expected,ATSPI_STATE_FOCUSED))pump();CHECK(state(expected,ATSPI_STATE_FOCUSED));
        printf("SAVE_%s_ACK %u\n",direction?"SHIFT_TAB":"TAB",i);
    }
    if(thumb)g_object_unref(thumb);
    g_object_unref(confirm);confirm=NULL;puts("ACK 1");
    CHECK(step(2));field=wait_focused(desktop,pid,&app,name("File name","Dateiname"),ATSPI_ROLE_ENTRY);CHECK(field&&retired(back)&&!action(back,"click")&&text_is(field,"snapshot-0000.txt"));g_object_unref(back);back=NULL;
    CHECK(edit(field,"fresh"));until=now()+3;while(now()<until&&!text_is(field,"fresh"))pump();CHECK(text_is(field,"fresh"));puts("ACK 2");
    CHECK(step(3));trigger=wait_focused(desktop,pid,&app,name("Save as…","Speichern unter…"),ATSPI_ROLE_PUSH_BUTTON);CHECK(trigger&&retired(field));g_object_unref(field);field=NULL;
    label=lookup(app,ime?"freshxé":"freshx",ATSPI_ROLE_LABEL);CHECK(label);g_object_unref(label);label=NULL;puts("ACK 3");
    CHECK(step(4));field=wait_for(desktop,pid,&app,name("File name","Dateiname"),ATSPI_ROLE_ENTRY);CHECK(field&&text_is(field,""));g_object_unref(field);field=NULL;g_object_unref(trigger);trigger=NULL;
    row=lookup(app,"folder",ATSPI_ROLE_LIST_ITEM);CHECK(row&&focus(row));puts("ACK 4");
    CHECK(step(5));back=wait_focused(desktop,pid,&app,name("Back","Zurück"),ATSPI_ROLE_PUSH_BUTTON);CHECK(back&&retired(row));g_object_unref(row);row=NULL;
    label=lookup(app,raw_label,ATSPI_ROLE_LABEL);CHECK(label&&read_review(app,label,back));g_object_unref(label);label=NULL;
    confirm=lookup(app,name("Replace this file","Diese Datei ersetzen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(confirm&&focus(confirm));puts("ACK 5");
    CHECK(step(6));trigger=wait_focused(desktop,pid,&app,name("Save as…","Speichern unter…"),ATSPI_ROLE_PUSH_BUTTON);CHECK(trigger&&retired(confirm)&&!action(confirm,"click")&&retired(back));g_object_unref(confirm);confirm=NULL;g_object_unref(back);back=NULL;
    label=lookup(app,raw_label,ATSPI_ROLE_LABEL);CHECK(label);g_object_unref(label);label=NULL;puts("ACK 6");
    CHECK(step(7));field=wait_for(desktop,pid,&app,name("File name","Dateiname"),ATSPI_ROLE_ENTRY);CHECK(field&&text_is(field,"")&&focus(field)&&edit(field,"snapshot-0000.txt"));
    until=now()+3;while(now()<until&&!text_is(field,"snapshot-0000.txt"))pump();CHECK(text_is(field,"snapshot-0000.txt"));g_object_unref(trigger);trigger=NULL;puts("ACK 7");
    CHECK(step(8));back=wait_focused(desktop,pid,&app,name("Back","Zurück"),ATSPI_ROLE_PUSH_BUTTON);CHECK(back&&retired(field));g_object_unref(field);field=NULL;puts("ACK 8");
    CHECK(step(9));field=wait_focused(desktop,pid,&app,name("File name","Dateiname"),ATSPI_ROLE_ENTRY);CHECK(field&&retired(back)&&text_is(field,"snapshot-0000.txt"));g_object_unref(back);back=NULL;puts("ACK 9");
    CHECK(step(10));trigger=wait_focused(desktop,pid,&app,name("Save as…","Speichern unter…"),ATSPI_ROLE_PUSH_BUTTON);CHECK(trigger&&retired(field));g_object_unref(field);field=NULL;
    note=lookup(app,name("Owner note","Notiz"),ATSPI_ROLE_ENTRY);CHECK(note&&text_is(note,"save draft"));label=lookup(app,raw_label,ATSPI_ROLE_LABEL);CHECK(label);g_object_unref(label);label=NULL;
    label=lookup(app,name("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(label&&focus(label));puts("ACK 10");
    puts("PICKER_NATIVE_SAVE_NAME_IME_RAW_OVERWRITE_REVIEW_BACK_CANCEL_AND_OWNER_LIFETIME_VERIFIED");okay=1;
cleanup:
    if(note)g_object_unref(note);
    if(confirm)g_object_unref(confirm);
    if(row)g_object_unref(row);
    if(label)g_object_unref(label);
    if(back)g_object_unref(back);
    if(field)g_object_unref(field);
    if(trigger)g_object_unref(trigger);
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    if(initialized)atspi_exit();
    return okay?0:1;
}
