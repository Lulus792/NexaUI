/* Independent OS AT-SPI peer; no NexaUI headers or linkage. Reuse transport
   discovery/state/focus helpers from the existing hierarchy peer. */
#define main hierarchy_peer_main
#include "../30_async_hierarchy/atspi_client.c"
#undef main
static const char *label(const char *en,const char *de){return german?de:en;}
static AtspiAccessible *wait_node(AtspiAccessible *desktop,unsigned long pid,AtspiAccessible **app,const char *name,AtspiRole role){
    double until=now()+5;AtspiAccessible *node=NULL;
    while(!node&&now()<until){AtspiAccessible *fresh=target(desktop,pid);if(fresh){if(*app)g_object_unref(*app);*app=fresh;node=lookup(*app,name,role);}if(!node)pump();}return node;
}
static unsigned sequence;
static int command(const char *kind,unsigned a,unsigned b){printf("HISTORY %u %s %u %u\n",sequence,kind,a,b);char line[64],expected[64];snprintf(expected,sizeof expected,"HISTORY_ACK %u\n",sequence++);return fgets(line,sizeof line,stdin)&&!strcmp(line,expected);}
static int rectangle(AtspiAccessible *node,AtspiRect *out){
    AtspiComponent *c=node?atspi_accessible_get_component_iface(node):NULL;GError *error=NULL;AtspiRect *r=c?atspi_component_get_extents(c,ATSPI_COORD_TYPE_WINDOW,&error):NULL;
    int yes=!error&&r;if(yes)*out=*r;if(error)g_error_free(error);if(r)g_boxed_free(ATSPI_TYPE_RECT,r);if(c)g_object_unref(c);return yes;
}
static int readonly_text(AtspiAccessible *node,char **out){
    AtspiText *t=node?atspi_accessible_get_text_iface(node):NULL;GError *error=NULL;*out=t?atspi_text_get_text(t,0,-1,&error):NULL;
    int yes=t&&!error&&*out&&g_utf8_validate(*out,-1,NULL)&&atspi_text_get_caret_offset(t,&error)==-1&&!error&&atspi_text_get_n_selections(t,&error)==0&&!error;
    if(error){g_error_free(error);error=NULL;}if(t)g_object_unref(t);
    AtspiEditableText *edit=node?atspi_accessible_get_editable_text_iface(node):NULL;
    if(edit){if(atspi_editable_text_set_text_contents(edit,"changed",&error))yes=0;g_object_unref(edit);}if(error)g_error_free(error);return yes;
}
int main(int argc,char **argv){
    int okay=0,initialized=0;AtspiAccessible *desktop=NULL,*app=NULL,*row=NULL,*button=NULL,*detail=NULL,*area=NULL;char *text=NULL;FILE *expected=NULL;
    CHECK(argc==7);german=!strcmp(argv[6],"de");char *end;unsigned long pid=strtoul(argv[1],&end,10);CHECK(pid&&!*end&&pid<=2147483647UL);
    int unavailable=!strcmp(argv[5],"corrupt"),none=!strcmp(argv[5],"none"),unsupported=!strcmp(argv[5],"unsupported");
    setvbuf(stdout,NULL,_IONBF,0);CHECK(!atspi_init());initialized=1;atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    row=wait_node(desktop,pid,&app,argv[2],ATSPI_ROLE_LIST_ITEM);CHECK(row&&action(row,"select")&&command("SYNC",2,43));
    button=wait_node(desktop,pid,&app,label("History","Historie"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&state(button,ATSPI_STATE_ENABLED)==!(unavailable||none));
    detail=lookup(app,label("Selected installation details","Details der ausgewählten Installation"),ATSPI_ROLE_LABEL);CHECK(detail&&readonly_text(detail,&text)&&strstr(text,label("unknown","unbekannt"))&&retired(row)&&!action(row,"select"));
    if(unavailable||none){CHECK(strstr(text,unavailable?label("Historical process observations unavailable","Historische Prozessbeobachtungen nicht verfügbar"):label("not an empty process inventory","kein leeres Prozessinventar"))&&!action(button,"click"));}
    else{
        CHECK(action(button,"click")&&command("SYNC",5,600));g_object_unref(row);row=wait_node(desktop,pid,&app,argv[3],ATSPI_ROLE_TREE_ITEM);CHECK(row&&retired(detail)&&!action(detail,"click")&&focus(row));
        double until=now()+3;while(now()<until&&!state(row,ATSPI_STATE_FOCUSED))pump();CHECK(state(row,ATSPI_STATE_FOCUSED));
        area=lookup(app,label("Historical profile entries","Historische Profileinträge"),ATSPI_ROLE_SCROLL_PANE);AtspiRect tree_first,tree_outer,tree_current;CHECK(area&&rectangle(row,&tree_first)&&rectangle(area,&tree_outer)&&tree_outer.height>2);tree_current=tree_first;
        long long tree_overflow=(long long)tree_first.y+tree_first.height-tree_outer.y-tree_outer.height;unsigned tree_pages=tree_overflow>0?(unsigned)((2*(tree_overflow+2)+tree_outer.height-3)/(tree_outer.height-2)):0;CHECK(tree_pages<512);
        for(unsigned direction=0;direction<2;++direction)for(unsigned i=0;i<tree_pages;++i){CHECK(command("KEY",direction?104:109,0)&&rectangle(row,&tree_current)&&state(row,ATSPI_STATE_FOCUSED));if(!direction&&i+1==tree_pages)CHECK((long long)tree_current.y+tree_current.height<=tree_outer.y+tree_outer.height);}
        CHECK(tree_current.y==tree_first.y);g_object_unref(area);area=NULL;CHECK(command("KEY",28,0)&&command("SYNC",6,43));
        g_object_unref(detail);detail=wait_node(desktop,pid,&app,label("Historical enrollment details","Historische Registrierungsdetails"),ATSPI_ROLE_LABEL);g_free(text);text=NULL;
        CHECK(detail&&readonly_text(detail,&text)&&strstr(text,label("Historical enrollment metadata only","Nur historische Registrierungsmetadaten"))&&strstr(text,label("unknown","unbekannt"))&&retired(row)&&!action(row,"select"));
        expected=fopen(argv[4],"rb");CHECK(expected);char line[128];unsigned fields=0;while(fgets(line,sizeof line,expected)){line[strcspn(line,"\r\n")]=0;CHECK(*line&&strstr(text,line));++fields;}CHECK(!ferror(expected)&&!fclose(expected)&&fields==19);expected=NULL;
        if(unsupported)CHECK(strstr(text,"-93")&&!strstr(text,"4242"));
        else CHECK(strstr(text,"4242")&&strstr(text,"18446744073709551615")&&strstr(text,"9223372036854775839")&&strstr(text,"124000")&&strstr(text,"124002"));
        g_object_unref(button);button=lookup(app,label("Back","Zurück"),ATSPI_ROLE_PUSH_BUTTON);area=lookup(app,label("App observation details","App-Beobachtungsdetails"),ATSPI_ROLE_SCROLL_PANE);AtspiRect first,outer,current;
        CHECK(button&&area&&rectangle(detail,&first)&&rectangle(area,&outer)&&outer.height>2&&state(button,ATSPI_STATE_FOCUSED));current=first;
        long long overflow=(long long)first.y+first.height-outer.y-outer.height;unsigned pages=overflow>0?(unsigned)((2*(overflow+2)+outer.height-3)/(outer.height-2)):0;CHECK(pages<512);
        for(unsigned direction=0;direction<2;++direction)for(unsigned i=0;i<pages;++i){CHECK(command("KEY",direction?104:109,0));AtspiRect next;CHECK(rectangle(detail,&next)&&state(button,ATSPI_STATE_FOCUSED));CHECK(direction?next.y>=current.y:next.y<=current.y);current=next;
            if(!direction&&i+1==pages)CHECK((long long)current.y+current.height<=outer.y+outer.height&&(long long)current.y+current.height>outer.y+outer.height/2);}
        CHECK(current.y==first.y&&command("KEY",1,0)&&command("SYNC",5,600));g_object_unref(row);row=wait_node(desktop,pid,&app,argv[3],ATSPI_ROLE_TREE_ITEM);CHECK(row&&state(row,ATSPI_STATE_FOCUSED)&&retired(detail));
        CHECK(command("KEY",1,0)&&command("SYNC",2,49));g_object_unref(button);button=wait_node(desktop,pid,&app,label("History","Historie"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&state(button,ATSPI_STATE_FOCUSED));
    }
    CHECK(command("KEY",1,0)&&command("SYNC",0,1000));g_object_unref(row);row=wait_node(desktop,pid,&app,argv[2],ATSPI_ROLE_LIST_ITEM);CHECK(row&&state(row,ATSPI_STATE_FOCUSED));
    g_object_unref(button);button=lookup(app,label("Read","Lesen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&action(button,"click")&&command("REFRESH",2,0)&&retired(row)&&!action(row,"select"));
    g_object_unref(button);button=wait_node(desktop,pid,&app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));double until=now()+3;while(now()<until&&!state(button,ATSPI_STATE_FOCUSED))pump();CHECK(state(button,ATSPI_STATE_FOCUSED)&&command("DONE",0,0));okay=1;
cleanup:
    if(expected)fclose(expected);
    g_free(text);if(area)g_object_unref(area);if(detail)g_object_unref(detail);if(button)g_object_unref(button);if(row)g_object_unref(row);if(app)g_object_unref(app);if(desktop)g_object_unref(desktop);if(initialized)atspi_exit();
    if(okay)puts("HISTORY_NATIVE_ORIGINAL_READONLY_FULL_IDENTITIES_RETIRED_FOCUS_PAGES_REFRESH_UNKNOWN_CURRENT_OK");
    return okay?0:1;
}
