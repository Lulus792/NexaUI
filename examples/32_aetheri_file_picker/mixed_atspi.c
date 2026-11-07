#include "peer_helpers.h"
int main(int argc,char **argv) {
    int okay=0,initialized=0,maximum=0;AtspiAccessible *desktop=NULL,*app=NULL,*row=NULL,*old_row=NULL,*node=NULL,*field=NULL;
    CHECK(argc>=2&&argc<=4);for(int i=2;i<argc;++i){if(!strcmp(argv[i],"--german"))german=1;else if(!strcmp(argv[i],"--maximum"))maximum=1;else CHECK(0);}
    char *end;unsigned long pid=strtoul(argv[1],&end,10);CHECK(pid&&!*end&&pid<=2147483647UL);
    setvbuf(stdout,NULL,_IONBF,0);CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    row=wait_for(desktop,pid,&app,"snapshot-0014.txt",ATSPI_ROLE_LIST_ITEM);CHECK(row&&action(row,"select")&&focus(row));
    CHECK(step(0));node=lookup(app,"Vertical scrollbar",ATSPI_ROLE_SLIDER);CHECK(node&&state(node,ATSPI_STATE_FOCUSED)&&state(row,ATSPI_STATE_SELECTED));g_object_unref(node);node=NULL;puts("ACK 0");
    old_row=row;row=NULL;CHECK(step(1));
    row=wait_for(desktop,pid,&app,maximum?"snapshot-4094.txt":"snapshot-0039.txt",ATSPI_ROLE_LIST_ITEM);
    CHECK(row&&state(row,ATSPI_STATE_FOCUSED)&&state(row,ATSPI_STATE_SELECTED)&&row_visible(app,row)&&retired(old_row)&&!action(old_row,"select"));puts("ACK 1");
    node=lookup(app,name("Choose a file","Datei auswählen"),ATSPI_ROLE_PANEL);CHECK(node);
    AtspiComponent *component=atspi_accessible_get_component_iface(node);GError *error=NULL;
    AtspiRect *bounds=component?atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,&error):NULL;
    CHECK(!error&&bounds&&bounds->width>4&&bounds->height>4);printf("POINTER %d %d\n",bounds->x+2,bounds->y+2);
    g_boxed_free(ATSPI_TYPE_RECT,bounds);g_object_unref(component);g_object_unref(node);node=NULL;
    CHECK(step(2));CHECK(state(row,ATSPI_STATE_SELECTED)&&!state(row,ATSPI_STATE_FOCUSED)&&row_visible(app,row));puts("ACK 2");
    field=lookup(app,name("Name filter","Namensfilter"),ATSPI_ROLE_ENTRY);CHECK(field&&focus(field)&&edit(field,"unapplied mixed draft"));
    double until=now()+3;while(now()<until&&!text_is(field,"unapplied mixed draft"))pump();CHECK(text_is(field,"unapplied mixed draft"));
    CHECK(step(3)&&state(field,ATSPI_STATE_FOCUSED)&&text_is(field,"unapplied mixed draft"));puts("ACK 3");
    CHECK(focus(row));CHECK(step(4));g_object_unref(row);row=wait_for(desktop,pid,&app,maximum?"snapshot-4094.txt":"snapshot-0039.txt",ATSPI_ROLE_LIST_ITEM);
    CHECK(row&&state(row,ATSPI_STATE_FOCUSED)&&state(row,ATSPI_STATE_SELECTED)&&row_visible(app,row));
    CHECK(retired(field));g_object_unref(field);field=lookup(app,name("Name filter","Namensfilter"),ATSPI_ROLE_ENTRY);CHECK(field&&text_is(field,"unapplied mixed draft"));puts("ACK 4");
    puts("PICKER_NATIVE_QUEUED_TAB_POINTER_TEXT_AND_ROW_COMMAND_ORDER_VERIFIED");okay=1;
cleanup:
    if(field)g_object_unref(field);
    if(node)g_object_unref(node);
    if(row)g_object_unref(row);
    if(old_row)g_object_unref(old_row);
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    if(initialized)atspi_exit();
    return okay?0:1;
}
