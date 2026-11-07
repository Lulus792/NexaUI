#include "peer_helpers.h"
/* Search chrome without recursively enumerating a virtual list. Membership and
   selected children are queried separately through the actual native APIs. */
static AtspiAccessible *chrome_find(AtspiAccessible *node,const char *text,AtspiRole role,unsigned depth){
    if(!node||depth>16||++visits>512||now()>=scan_deadline){scan_failed=1;return NULL;}
    atspi_accessible_clear_cache(node);GError *error=NULL;char *s=atspi_accessible_get_name(node,&error);int match=!error&&s&&!strcmp(s,text);g_free(s);
    AtspiRole actual=!error?atspi_accessible_get_role(node,&error):ATSPI_ROLE_INVALID;
    if(error){g_error_free(error);scan_failed=1;return NULL;}if(match&&actual==role)return g_object_ref(node);
    if(actual==ATSPI_ROLE_LIST)return NULL;
    int count=atspi_accessible_get_child_count(node,&error);if(error||count<0||count>128){if(error)g_error_free(error);scan_failed=1;return NULL;}
    for(int i=0;i<count;++i){AtspiAccessible *child=atspi_accessible_get_child_at_index(node,i,&error);
        if(error){g_error_free(error);scan_failed=1;return NULL;}AtspiAccessible *found=chrome_find(child,text,role,depth+1);if(child)g_object_unref(child);if(found)return found;}
    return NULL;
}
static AtspiAccessible *chrome(AtspiAccessible *app,const char *text,AtspiRole role){visits=0;scan_failed=0;scan_deadline=now()+1;return chrome_find(app,text,role,0);}
static AtspiAccessible *await_chrome(AtspiAccessible *desktop,unsigned long pid,AtspiAccessible **app,const char *text,AtspiRole role){
    double until=now()+4;AtspiAccessible *node=NULL;while(!node&&now()<until){AtspiAccessible *fresh=target(desktop,pid);if(fresh){if(*app)g_object_unref(*app);*app=fresh;node=chrome(*app,text,role);}if(!node)pump();}return node;
}
static int await_retired(AtspiAccessible *node){double until=now()+3;while(now()<until){if(retired(node))return 1;pump();}return 0;}
static AtspiAccessible *await_group(AtspiAccessible *desktop,unsigned long pid,AtspiAccessible **app,const char *text,int count){
    double until=now()+4;while(now()<until){AtspiAccessible *node=await_chrome(desktop,pid,app,text,ATSPI_ROLE_LIST);GError *error=NULL;
        int actual=node?atspi_accessible_get_child_count(node,&error):-1;int yes=!error&&actual==count;if(error)g_error_free(error);if(yes)return node;if(node)g_object_unref(node);pump();}return NULL;
}
static AtspiAccessible *await_owner(AtspiAccessible *desktop,unsigned long pid,AtspiAccessible **app,const char *text){
    double until=now()+4;while(now()<until){AtspiAccessible *node=await_chrome(desktop,pid,app,text,ATSPI_ROLE_PUSH_BUTTON);if(node&&state(node,ATSPI_STATE_FOCUSED))return node;if(node)g_object_unref(node);pump();}return NULL;
}
static AtspiAccessible *child(AtspiAccessible *group,int index,const char *expected){
    GError *error=NULL;AtspiAccessible *node=atspi_accessible_get_child_at_index(group,index,&error);char *s=node&&!error?atspi_accessible_get_name(node,&error):NULL;
    int yes=!error&&s&&(!expected||!strcmp(s,expected));g_free(s);if(error)g_error_free(error);if(!yes&&node){g_object_unref(node);node=NULL;}return node;
}
static int count_is(AtspiSelection *s,int count){GError *e=NULL;int n=atspi_selection_get_n_selected_children(s,&e);int yes=!e&&n==count;if(e)g_error_free(e);return yes;}
static int await_count(AtspiSelection *s,int count){double until=now()+3;while(now()<until){if(count_is(s,count))return 1;pump();}return 0;}
static int selected_name(AtspiSelection *s,int index,const char *text){GError *e=NULL;AtspiAccessible *node=atspi_selection_get_selected_child(s,index,&e);char *value=node&&!e?atspi_accessible_get_name(node,&e):NULL;
    int yes=!e&&value&&!strcmp(value,text);g_free(value);if(node)g_object_unref(node);if(e)g_error_free(e);return yes;}
static int note_is(AtspiAccessible *app){AtspiAccessible *n=chrome(app,name("Owner note","Notiz"),ATSPI_ROLE_ENTRY);int yes=n&&text_is(n,"multi draft");if(n)g_object_unref(n);return yes;}
static int focused(AtspiAccessible *app,const char *text){AtspiAccessible *n=chrome(app,text,ATSPI_ROLE_PUSH_BUTTON);int yes=n&&state(n,ATSPI_STATE_FOCUSED);if(n)g_object_unref(n);return yes;}
static int focus_named(AtspiAccessible *app,const char *text){AtspiAccessible *n=chrome(app,text,ATSPI_ROLE_PUSH_BUTTON);int yes=n&&focus(n);
    if(yes){double until=now()+3;while(now()<until&&!state(n,ATSPI_STATE_FOCUSED))pump();yes=state(n,ATSPI_STATE_FOCUSED);}if(n)g_object_unref(n);return yes;}
static int rect_of(AtspiAccessible *node,AtspiRect *out){AtspiComponent *c=node?atspi_accessible_get_component_iface(node):NULL;GError *e=NULL;
    AtspiRect *r=c?atspi_component_get_extents(c,ATSPI_COORD_TYPE_WINDOW,&e):NULL;int yes=!e&&r;if(yes)*out=*r;
    if(r)g_boxed_free(ATSPI_TYPE_RECT,r);
    if(c)g_object_unref(c);
    if(e)g_error_free(e);
    return yes;
}
static int read_results(AtspiAccessible *app,char **names,unsigned phase){
    int okay=0;AtspiAccessible *area=NULL,*last=NULL,*close=NULL;AtspiRect outer,first={0},current;char line[80],expected_line[80];
    area=chrome(app,name("Selection workspace","Auswahlbereich"),ATSPI_ROLE_SCROLL_PANE);CHECK(rect_of(area,&outer)&&outer.height>0);
    for(int i=0;i<17;++i){AtspiAccessible *n=chrome(app,names[i],ATSPI_ROLE_LABEL);AtspiRect r;
        if(n&&rect_of(n,&r)&&(!last||(long long)r.y+r.height>(long long)first.y+first.height)){if(last)g_object_unref(last);last=n;first=r;}else if(n)g_object_unref(n);}
    CHECK(last&&first.height>0&&first.height<=outer.height&&focus_named(app,name("Close","Schließen")));
    close=chrome(app,name("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(close);current=first;
    long long delta=(long long)first.y+first.height-outer.y-outer.height;unsigned pages=delta>0?(unsigned)((2*delta+outer.height-1)/outer.height)+2:0;
    CHECK(pages<128);if(!pages)CHECK(first.y>=outer.y&&(long long)first.y+first.height<=(long long)outer.y+outer.height&&state(close,ATSPI_STATE_FOCUSED));
    printf("MULTI_READ %u %u\n",phase,pages);
    for(unsigned direction=0;direction<2;++direction)for(unsigned i=0;i<pages;++i){
        snprintf(expected_line,sizeof expected_line,"MULTI_READ_%s %u %u\n",direction?"UP":"DOWN",phase,i);
        CHECK(fgets(line,sizeof line,stdin)&&!strcmp(line,expected_line));AtspiRect next;CHECK(rect_of(last,&next)&&state(close,ATSPI_STATE_FOCUSED));
        CHECK(direction?next.y>=current.y&&(long long)next.y-current.y<=outer.height/2+1:next.y<=current.y&&(long long)current.y-next.y<=outer.height/2+1);
        current=next;if(!direction&&i+1==pages)CHECK(current.y>=outer.y&&(long long)current.y+current.height<=(long long)outer.y+outer.height);
        printf("MULTI_READ_%u_%s_%u_ACK\n",phase,direction?"UP":"DOWN",i);
    }
    CHECK(current.y>=first.y);printf("MULTIPLE_RESULT_PAGE_%u_READ_WITH_FOCUS_VERIFIED\n",phase);okay=1;
cleanup:
    if(area)g_object_unref(area);
    if(last)g_object_unref(last);
    if(close)g_object_unref(close);
    return okay;
}
int main(int argc,char **argv){
    int okay=0,initialized=0,maximum=0,held=0;AtspiAccessible *desktop=NULL,*app=NULL,*group=NULL,*old_group=NULL,*node=NULL,*first=NULL,*last=NULL,*old_first=NULL,*result=NULL,*page_ref=NULL;
    AtspiSelection *selection=NULL,*old_selection=NULL;char *chosen[17]={0};GError *error=NULL;
    CHECK(argc>=2&&argc<=5);for(int i=2;i<argc;++i){if(!strcmp(argv[i],"--german"))german=1;else if(!strcmp(argv[i],"--maximum"))maximum=1;else if(!strcmp(argv[i],"--pending-cancel"))held=1;else CHECK(0);}
    char *end;unsigned long pid=strtoul(argv[1],&end,10);CHECK(pid&&!*end&&pid<=2147483647UL);setvbuf(stdout,NULL,_IONBF,0);
    CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    node=wait_for(desktop,pid,&app,name("Owner note","Notiz"),ATSPI_ROLE_ENTRY);CHECK(node&&edit(node,"multi draft"));
    double until=now()+3;while(now()<until&&!text_is(node,"multi draft"))pump();CHECK(text_is(node,"multi draft"));g_object_unref(node);node=NULL;
    CHECK(focus_named(app,name("Choose files…","Dateien auswählen…"))&&step(0));
    node=await_chrome(desktop,pid,&app,name("Choose files","Dateien auswählen"),ATSPI_ROLE_DIALOG);CHECK(node);g_object_unref(node);node=NULL;
    group=await_group(desktop,pid,&app,name("Files in this folder","Dateien dieses Ordners"),maximum?4096:41);CHECK(group);selection=atspi_accessible_get_selection_iface(group);CHECK(selection&&count_is(selection,0));
    node=child(group,0,"folder");CHECK(node&&action(node,"click"));g_object_unref(node);node=NULL;
    CHECK(await_retired(group));g_object_unref(group);group=await_group(desktop,pid,&app,name("Files in this folder","Dateien dieses Ordners"),2);CHECK(group);
    node=chrome(app,name("Up","Übergeordneter Ordner"),ATSPI_ROLE_PUSH_BUTTON);CHECK(node&&action(node,"click"));g_object_unref(node);node=NULL;
    CHECK(await_retired(group));g_object_unref(group);group=await_group(desktop,pid,&app,name("Files in this folder","Dateien dieses Ordners"),maximum?4096:41);CHECK(group);
    g_object_unref(selection);selection=atspi_accessible_get_selection_iface(group);CHECK(selection&&count_is(selection,0));puts("MULTIPLE_DIRECTORY_BUTTON_AND_UP_VERIFIED");
    node=chrome(app,name("Choose file…","Datei auswählen…"),ATSPI_ROLE_PUSH_BUTTON);CHECK(node&&!action(node,"click"));g_object_unref(node);node=NULL;
    first=child(group,1,"snapshot-0000.txt");CHECK(first&&focus(first));until=now()+3;while(now()<until&&!state(first,ATSPI_STATE_FOCUSED))pump();CHECK(state(first,ATSPI_STATE_FOCUSED));puts("ACK 0");
    CHECK(step(1)&&await_count(selection,1)&&selected_name(selection,0,"snapshot-0000.txt"));old_first=first;first=NULL;old_group=group;group=NULL;old_selection=selection;selection=NULL;puts("ACK 1");
    CHECK(step(2)&&await_retired(old_group));group=await_group(desktop,pid,&app,name("Files in this folder","Dateien dieses Ordners"),maximum?4095:40);CHECK(group&&retired(old_first)&&!action(old_first,"add to selection"));
    selection=atspi_accessible_get_selection_iface(group);CHECK(selection&&await_count(selection,1)&&selected_name(selection,0,"snapshot-0000.txt"));
    last=child(group,maximum?15:8,maximum?"snapshot-4094.txt":"snapshot-0039.txt");CHECK(last&&state(last,ATSPI_STATE_FOCUSED)&&!state(last,ATSPI_STATE_SELECTED));
    CHECK(!atspi_selection_select_all(old_selection,&error));if(error){g_error_free(error);error=NULL;}puts("ACK 2");
    CHECK(step(3)&&await_count(selection,2)&&state(last,ATSPI_STATE_SELECTED));puts("ACK 3");
    CHECK(step(4));AtspiComponent *component=atspi_accessible_get_component_iface(last);CHECK(component);
    AtspiRect *rect=atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,&error);g_object_unref(component);CHECK(rect&&!error&&rect->width>0&&rect->height>0);
    printf("MULTI_POINTER %d %d\n",rect->x+rect->width/2,rect->y+rect->height/2);g_boxed_free(ATSPI_TYPE_RECT,rect);
    char pointer_ack[32];CHECK(fgets(pointer_ack,sizeof pointer_ack,stdin)&&!strcmp(pointer_ack,"POINTER_ONE\n")&&await_count(selection,1)&&!state(last,ATSPI_STATE_SELECTED));puts("POINTER_ONE_ACK");
    CHECK(fgets(pointer_ack,sizeof pointer_ack,stdin)&&!strcmp(pointer_ack,"POINTER_TWO\n")&&await_count(selection,2)&&state(last,ATSPI_STATE_SELECTED));puts("POINTER_TWO_ACK");
    CHECK(atspi_selection_select_all(selection,&error)&&!error&&await_count(selection,maximum?4095:40));
    CHECK(atspi_selection_clear_selection(selection,&error)&&!error&&await_count(selection,0));puts("ACK 4");
    CHECK(step(5));
    for(int i=0;i<17;++i){node=child(group,i,NULL);CHECK(node&&atspi_accessible_get_role(node,&error)==ATSPI_ROLE_LIST_ITEM&&!error);
        chosen[i]=atspi_accessible_get_name(node,&error);CHECK(chosen[i]&&!error&&atspi_selection_select_child(selection,i,&error)&&!error&&await_count(selection,i+1));
        printf("CHOSEN %s\n",chosen[i]);g_object_unref(node);node=NULL;}
    CHECK(focus_named(app,name("Open selected files","Ausgewählte Dateien öffnen")));puts("ACK 5");
    CHECK(step(6));node=await_chrome(desktop,pid,&app,chosen[0],ATSPI_ROLE_LABEL);CHECK(node&&retired(group)&&retired(last)&&note_is(app)&&focused(app,name("Choose files…","Dateien auswählen…")));result=node;node=NULL;
    int seen=0;for(int i=0;i<17;++i){node=chrome(app,chosen[i],ATSPI_ROLE_LABEL);if(node){++seen;g_object_unref(node);node=NULL;}}
    CHECK(seen==16&&read_results(app,chosen,6)&&focus_named(app,name("Next results","Nächste Ergebnisse")));puts("ACK 6");
    CHECK(step(7)&&await_retired(result));page_ref=await_chrome(desktop,pid,&app,name("Previous results","Vorherige Ergebnisse"),ATSPI_ROLE_PUSH_BUTTON);CHECK(page_ref);
    seen=0;for(int i=0;i<17;++i){node=chrome(app,chosen[i],ATSPI_ROLE_LABEL);if(node){++seen;g_object_unref(node);node=NULL;}}CHECK(seen==1&&note_is(app)&&read_results(app,chosen,7)&&focus_named(app,name("Previous results","Vorherige Ergebnisse")));puts("ACK 7");
    CHECK(step(8)&&await_retired(page_ref));node=await_chrome(desktop,pid,&app,name("Next results","Nächste Ergebnisse"),ATSPI_ROLE_PUSH_BUTTON);CHECK(node);g_object_unref(node);node=NULL;
    seen=0;for(int i=0;i<17;++i){node=chrome(app,chosen[i],ATSPI_ROLE_LABEL);if(node){++seen;g_object_unref(node);node=NULL;}}CHECK(seen==16&&focus_named(app,name("Choose files…","Dateien auswählen…")));puts("ACK 8");
    CHECK(step(9));node=await_chrome(desktop,pid,&app,name("Choose files","Dateien auswählen"),ATSPI_ROLE_DIALOG);CHECK(node);g_object_unref(node);node=NULL;
    g_object_unref(group);group=chrome(app,name("Files in this folder","Dateien dieses Ordners"),ATSPI_ROLE_LIST);CHECK(group);g_object_unref(selection);selection=atspi_accessible_get_selection_iface(group);
    CHECK(selection&&count_is(selection,0)&&focus_named(app,name("Cancel","Abbrechen")));puts("ACK 9");
    CHECK(step(10));node=await_owner(desktop,pid,&app,name("Choose files…","Dateien auswählen…"));CHECK(node&&note_is(app));g_object_unref(node);node=NULL;
    CHECK(focus_named(app,name("Choose file…","Datei auswählen…")));puts("ACK 10");
    CHECK(step(11));node=await_chrome(desktop,pid,&app,name("Choose a file","Datei auswählen"),ATSPI_ROLE_DIALOG);CHECK(node);g_object_unref(node);node=NULL;
    g_object_unref(group);group=await_group(desktop,pid,&app,name("Entries on this page","Einträge dieser Seite"),16);CHECK(group);node=child(group,11,"snapshot-0010.txt");CHECK(node&&action(node,"select")&&focus(node));
    until=now()+3;while(now()<until&&(!state(node,ATSPI_STATE_SELECTED)||!row_visible(app,node)))pump();CHECK(state(node,ATSPI_STATE_SELECTED)&&row_visible(app,node));g_object_unref(node);node=NULL;
    CHECK(focus_named(app,name("Open","Öffnen")));puts("ACK 11");
    CHECK(step(12));node=await_chrome(desktop,pid,&app,"snapshot-0010.txt",ATSPI_ROLE_LABEL);CHECK(node&&note_is(app)&&focused(app,name("Choose file…","Datei auswählen…")));g_object_unref(node);node=NULL;
    CHECK(focus_named(app,name("Choose files…","Dateien auswählen…")));puts("ACK 12");
    CHECK(step(13));node=await_chrome(desktop,pid,&app,name("Choose files","Dateien auswählen"),ATSPI_ROLE_DIALOG);CHECK(node);g_object_unref(node);node=NULL;
    g_object_unref(group);group=await_group(desktop,pid,&app,name("Files in this folder","Dateien dieses Ordners"),maximum?4096:41);CHECK(group);
    CHECK(focus_named(app,held?name("Refresh","Aktualisieren"):name("Cancel","Abbrechen")));puts("ACK 13");
    CHECK(step(14));
    if(held){node=await_chrome(desktop,pid,&app,name("Reading folder…","Ordner wird gelesen…"),ATSPI_ROLE_STATUS_BAR);CHECK(node&&state(node,ATSPI_STATE_BUSY));g_object_unref(node);node=NULL;puts("ACK 14");CHECK(step(15));}
    node=await_owner(desktop,pid,&app,name("Choose files…","Dateien auswählen…"));CHECK(node&&note_is(app));g_object_unref(node);node=NULL;
    CHECK(focus_named(app,name("Close","Schließen")));printf("ACK %u\n",held?15u:14u);
    puts("PICKER_NATIVE_MULTIPLE_WHOLE_SET_OFFSCREEN_BULK_OWNER_PAGES_CANCEL_REPLACEMENT_AND_FOCUS_VERIFIED");okay=1;
cleanup:
    if(error)g_error_free(error);
    for(int i=0;i<17;++i)g_free(chosen[i]);
    if(selection)g_object_unref(selection);
    if(old_selection)g_object_unref(old_selection);
    if(result)g_object_unref(result);
    if(page_ref)g_object_unref(page_ref);
    if(node)g_object_unref(node);
    if(first)g_object_unref(first);
    if(last)g_object_unref(last);
    if(old_first)g_object_unref(old_first);
    if(group)g_object_unref(group);
    if(old_group)g_object_unref(old_group);
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    if(initialized)atspi_exit();
    return okay?0:1;
}
