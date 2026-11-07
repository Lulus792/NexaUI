/* Independent OS transport peer. No NexaUI headers, types or linkage. */
#define main hierarchy_peer_main
#include "../30_async_hierarchy/atspi_client.c"
#undef main
static const char *label(const char *en,const char *de){return german?de:en;}
static AtspiAccessible *wait_node(AtspiAccessible *desktop,unsigned long pid,AtspiAccessible **app,const char *name,AtspiRole role){
    double until=now()+4;AtspiAccessible *node=NULL;
    while(!node&&now()<until){AtspiAccessible *fresh=target(desktop,pid);if(fresh){if(*app)g_object_unref(*app);*app=fresh;node=lookup(*app,name,role);}if(!node)pump();}return node;
}
static int checkpoint(unsigned n){printf("STEP %u\n",n);char line[64];unsigned actual=999;return fgets(line,sizeof line,stdin)&&sscanf(line,"CHECK %u",&actual)==1&&actual==n;}
static int text_equal(AtspiAccessible *node,const char *expected){
    AtspiText *t=node?atspi_accessible_get_text_iface(node):NULL;GError *error=NULL;char *s=t?atspi_text_get_text(t,0,-1,&error):NULL;int yes=!error&&s&&!strcmp(s,expected);
    g_free(s);if(error)g_error_free(error);if(t)g_object_unref(t);return yes;
}
static int replace_text(AtspiAccessible *node,const char *text){
    AtspiEditableText *t=node?atspi_accessible_get_editable_text_iface(node):NULL;GError *error=NULL;
    int yes=t&&atspi_editable_text_set_text_contents(t,text,&error)&&!error;if(error)g_error_free(error);if(t)g_object_unref(t);return yes;
}
static int has_focus(AtspiAccessible *app,const char *name,AtspiRole role){
    AtspiAccessible *node=lookup(app,name,role);int yes=node&&state(node,ATSPI_STATE_FOCUSED);if(node)g_object_unref(node);return yes;
}
static int absent(AtspiAccessible *app,const char *name,AtspiRole role){
    AtspiAccessible *node=lookup(app,name,role);int yes=!node&&!scan_failed;if(node)g_object_unref(node);return yes;
}
static int rectangle(AtspiAccessible *node,AtspiRect *out){
    AtspiComponent *c=node?atspi_accessible_get_component_iface(node):NULL;GError *error=NULL;AtspiRect *r=c?atspi_component_get_extents(c,ATSPI_COORD_TYPE_WINDOW,&error):NULL;
    int yes=!error&&r;if(yes)*out=*r;if(error)g_error_free(error);if(r)g_boxed_free(ATSPI_TYPE_RECT,r);if(c)g_object_unref(c);return yes;
}
static int read_details(AtspiAccessible *app,AtspiAccessible *node,AtspiAccessible *back,const char *id){
    int okay=0;AtspiAccessible *area=lookup(app,label("Record details","Eintragsdetails"),ATSPI_ROLE_SCROLL_PANE);AtspiText *t=atspi_accessible_get_text_iface(node);GError *error=NULL;
    char *text=t?atspi_text_get_text(t,0,-1,&error):NULL;char expected[40];snprintf(expected,sizeof expected,"ID: %s",id);
    CHECK(t&&!error&&text&&strstr(text,expected)&&strstr(text,"Runtime:")&&strstr(text,label("Catalogue revision:","Katalogrevision:"))&&g_utf8_validate(text,-1,NULL));
    CHECK(atspi_text_get_caret_offset(t,&error)==-1&&!error&&atspi_text_get_n_selections(t,&error)==0&&!error&&!replace_text(node,"changed"));
    AtspiRect first,outer,current;CHECK(rectangle(node,&first)&&rectangle(area,&outer)&&first.height>0&&outer.height>0&&state(back,ATSPI_STATE_FOCUSED));
    long long overflow=(long long)first.y+first.height-outer.y-outer.height;unsigned pages=overflow>0?(unsigned)((2*overflow+outer.height-1)/outer.height):0;CHECK(pages<256);
    if(!pages)CHECK(first.y>=outer.y&&(long long)first.y+first.height<=outer.y+outer.height);
    printf("READ_PAGES %u\n",pages);current=first;
    for(unsigned direction=0;direction<2;++direction)for(unsigned i=0;i<pages;++i){
        char line[64],wanted[64];snprintf(wanted,sizeof wanted,"PAGE %c %u\n",direction?'U':'D',i);CHECK(fgets(line,sizeof line,stdin)&&!strcmp(line,wanted));
        AtspiRect next;CHECK(rectangle(node,&next)&&state(back,ATSPI_STATE_FOCUSED));
        CHECK(direction?next.y>=current.y&&(long long)next.y-current.y<=outer.height/2+1:next.y<=current.y&&(long long)current.y-next.y<=outer.height/2+1);current=next;
        if(!direction&&i+1==pages)CHECK((long long)current.y+current.height<=outer.y+outer.height&&(long long)current.y+current.height>outer.y+outer.height/2);
        printf("PAGE_ACK %c %u\n",direction?'U':'D',i);
    }
    CHECK(current.y==first.y);okay=1;
cleanup:
    g_free(text);if(error)g_error_free(error);if(t)g_object_unref(t);if(area)g_object_unref(area);return okay;
}
int main(int argc,char **argv){
    int okay=0,initialized=0,ime=0,navigation=0,resizing=0;AtspiAccessible *desktop=NULL,*app=NULL,*old_first=NULL,*last=NULL,*detail=NULL,*field=NULL,*button=NULL,*scrollbar=NULL;
    CHECK(argc>=4&&argc<=8);for(int i=4;i<argc;++i){if(!strcmp(argv[i],"--german"))german=1;else if(!strcmp(argv[i],"--ime"))ime=1;else if(!strcmp(argv[i],"--navigation"))navigation=1;else if(!strcmp(argv[i],"--resize"))resizing=1;else CHECK(0);}
    CHECK(!resizing||navigation);
    char *end;unsigned long pid=strtoul(argv[1],&end,10);CHECK(pid&&!*end&&pid<=2147483647UL);const char *id=strrchr(argv[3],'\n');CHECK(id&&strlen(++id)==32);
    setvbuf(stdout,NULL,_IONBF,0);CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    old_first=wait_node(desktop,pid,&app,argv[2],ATSPI_ROLE_LIST_ITEM);CHECK(old_first);
    button=lookup(app,label("Filter","Filter"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));g_object_unref(button);button=NULL;
    CHECK(checkpoint(0));CHECK(!retired(old_first));puts("ACK 0");
    CHECK(checkpoint(1));last=wait_node(desktop,pid,&app,argv[3],ATSPI_ROLE_LIST_ITEM);CHECK(last&&state(last,ATSPI_STATE_FOCUSED)&&action(last,"select"));puts("ACK 1");
    CHECK(checkpoint(2));button=wait_node(desktop,pid,&app,label("Back","Zurück"),ATSPI_ROLE_PUSH_BUTTON);detail=lookup(app,label("Observed record details","Beobachtete Eintragsdetails"),ATSPI_ROLE_LABEL);
    CHECK(button&&detail&&retired(old_first)&&retired(last)&&!action(last,"select")&&read_details(app,detail,button,id));g_object_unref(button);button=NULL;puts("ACK 2");
    CHECK(checkpoint(3));CHECK(retired(detail));g_object_unref(detail);detail=NULL;g_object_unref(last);last=wait_node(desktop,pid,&app,argv[3],ATSPI_ROLE_LIST_ITEM);CHECK(last&&state(last,ATSPI_STATE_FOCUSED));
    button=lookup(app,label("Read","Lesen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&action(button,"click"));g_object_unref(button);button=NULL;puts("ACK 3");
    CHECK(checkpoint(4));CHECK(retired(last)&&!action(last,"select"));g_object_unref(last);last=NULL;
    button=wait_node(desktop,pid,&app,label("Filter","Filter"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&action(button,"click"));g_object_unref(button);button=NULL;puts("ACK 4");
    CHECK(checkpoint(5));field=wait_node(desktop,pid,&app,label("Record filter","Eintragsfilter"),ATSPI_ROLE_ENTRY);CHECK(field&&text_equal(field,"x")&&state(field,ATSPI_STATE_FOCUSED));puts("ACK 5");
    CHECK(checkpoint(6));CHECK(text_equal(field,"x"));button=lookup(app,label("Apply","Anwenden"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button);
    if(ime)CHECK(!state(button,ATSPI_STATE_ENABLED)&&!action(button,"click"));
    g_object_unref(button);button=NULL;puts("ACK 6");
    CHECK(checkpoint(7));CHECK(text_equal(field,ime?"xé":"x"));button=lookup(app,label("Apply","Anwenden"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&state(button,ATSPI_STATE_ENABLED));g_object_unref(button);button=NULL;puts("ACK 7");
    CHECK(checkpoint(8));CHECK(retired(field));g_object_unref(field);field=NULL;button=wait_node(desktop,pid,&app,label("No matching observed records","Keine passenden beobachteten Einträge"),ATSPI_ROLE_LABEL);CHECK(button);g_object_unref(button);
    button=lookup(app,label("Filter","Filter"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&action(button,"click"));g_object_unref(button);button=NULL;
    field=wait_node(desktop,pid,&app,label("Record filter","Eintragsfilter"),ATSPI_ROLE_ENTRY);CHECK(field&&text_equal(field,ime?"xé":"x")&&replace_text(field,"")&&focus(field));
    double until=now()+3;while(now()<until&&!text_equal(field,""))pump();CHECK(text_equal(field,""));puts("ACK 8");
    CHECK(checkpoint(9));CHECK(retired(field));g_object_unref(field);field=NULL;last=wait_node(desktop,pid,&app,argv[3],ATSPI_ROLE_LIST_ITEM);CHECK(last&&state(last,ATSPI_STATE_FOCUSED)&&state(last,ATSPI_STATE_SELECTED));
    button=lookup(app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));puts("ACK 9");
    if(navigation){
        CHECK(checkpoint(10));CHECK(has_focus(app,label("Read","Lesen"),ATSPI_ROLE_PUSH_BUTTON));puts("ACK 10");
        CHECK(checkpoint(11));CHECK(retired(last)&&!action(last,"select")&&retired(button));g_object_unref(last);last=NULL;g_object_unref(button);button=NULL;
        last=wait_node(desktop,pid,&app,argv[3],ATSPI_ROLE_LIST_ITEM);CHECK(last&&state(last,ATSPI_STATE_FOCUSED)&&state(last,ATSPI_STATE_SELECTED));
        button=lookup(app,"Filter",ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));g_object_unref(button);button=NULL;puts("ACK 11");
        CHECK(checkpoint(12));CHECK(has_focus(app,label("Read","Lesen"),ATSPI_ROLE_PUSH_BUTTON));puts("ACK 12");
        CHECK(checkpoint(13));CHECK(has_focus(app,"Filter",ATSPI_ROLE_PUSH_BUTTON));puts("ACK 13");
        CHECK(checkpoint(14));field=wait_node(desktop,pid,&app,label("Record filter","Eintragsfilter"),ATSPI_ROLE_ENTRY);CHECK(field&&text_equal(field,"")&&state(field,ATSPI_STATE_FOCUSED)&&retired(last));puts("ACK 14");
        CHECK(checkpoint(15));CHECK(text_equal(field,"x")&&state(field,ATSPI_STATE_FOCUSED));scrollbar=lookup(app,"Vertical scrollbar",ATSPI_ROLE_SLIDER);CHECK(!scan_failed);
        if(scrollbar)CHECK(state(scrollbar,ATSPI_STATE_FOCUSABLE)&&!retired(scrollbar));
        printf("FILTER_SCROLLBAR %d\n",scrollbar!=NULL);puts("ACK 15");
        CHECK(checkpoint(16));
        if(scrollbar){CHECK(state(scrollbar,ATSPI_STATE_FOCUSED));puts("TAB_SCROLL_FORWARD");char line[32];CHECK(fgets(line,sizeof line,stdin)&&!strcmp(line,"NEXT_TAB\n"));}
        CHECK(has_focus(app,label("Apply","Anwenden"),ATSPI_ROLE_PUSH_BUTTON));puts("ACK 16");
        CHECK(checkpoint(17));
        if(scrollbar){CHECK(state(scrollbar,ATSPI_STATE_FOCUSED));puts("TAB_SCROLL_REVERSE");char line[32];CHECK(fgets(line,sizeof line,stdin)&&!strcmp(line,"NEXT_TAB\n"));}
        CHECK(state(field,ATSPI_STATE_FOCUSED)&&text_equal(field,"x"));puts("ACK 17");
        CHECK(checkpoint(18));CHECK(has_focus(app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON));puts("ACK 18");
        CHECK(checkpoint(19));CHECK(has_focus(app,label("Back","Zurück"),ATSPI_ROLE_PUSH_BUTTON));puts("ACK 19");
        for(unsigned cycle=0;cycle<3;++cycle){
            unsigned back=20+2*cycle;CHECK(checkpoint(back));CHECK(retired(field)&&!replace_text(field,"late edit"));g_object_unref(field);field=NULL;
            if(scrollbar){CHECK(retired(scrollbar));g_object_unref(scrollbar);scrollbar=NULL;}
            button=wait_node(desktop,pid,&app,"Filter",ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&state(button,ATSPI_STATE_FOCUSED));g_object_unref(button);button=NULL;
            g_object_unref(last);last=lookup(app,argv[3],ATSPI_ROLE_LIST_ITEM);CHECK(last&&state(last,ATSPI_STATE_SELECTED));printf("ACK %u\n",back);
            if(cycle==2)break;
            CHECK(checkpoint(back+1));field=wait_node(desktop,pid,&app,label("Record filter","Eintragsfilter"),ATSPI_ROLE_ENTRY);CHECK(field&&text_equal(field,"x")&&state(field,ATSPI_STATE_FOCUSED));printf("ACK %u\n",back+1);
        }
        button=lookup(app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&(resizing||focus(button)));
        puts("RECOVERY_NATIVE_TAB_REVERSE_TAB_READ_BACK_ESCAPE_RETAINED_DRAFT_AND_SELECTION_OK");
    }
    if(resizing){
        CHECK(checkpoint(25));field=wait_node(desktop,pid,&app,label("Record filter","Eintragsfilter"),ATSPI_ROLE_ENTRY);CHECK(field&&text_equal(field,"x")&&state(field,ATSPI_STATE_FOCUSED));
        CHECK(retired(last)&&retired(button));g_object_unref(button);button=lookup(app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&!retired(button));puts("ACK 25");
        for(unsigned step=26;step<=27;++step){
            CHECK(checkpoint(step));CHECK(retired(field)&&!replace_text(field,"late compact edit")&&state(button,ATSPI_STATE_FOCUSED));
            CHECK(absent(app,label("Record filter","Eintragsfilter"),ATSPI_ROLE_ENTRY));AtspiRect bounds;CHECK(rectangle(button,&bounds));
            int width=step==26?64:240,height=step==26?64:96;
            CHECK(bounds.x>=0&&bounds.y>=0&&bounds.width>0&&bounds.height>0&&bounds.x+bounds.width<=width&&bounds.y+bounds.height<=height);
            printf("ACK %u\n",step);
        }
        CHECK(checkpoint(28));CHECK(retired(field)&&!replace_text(field,"late restored edit"));g_object_unref(button);button=NULL;
        button=wait_node(desktop,pid,&app,label("Record filter","Eintragsfilter"),ATSPI_ROLE_ENTRY);CHECK(button&&state(button,ATSPI_STATE_FOCUSED)&&text_equal(button,"x")&&!retired(button));
        g_object_unref(field);field=button;button=NULL;puts("ACK 28");
        CHECK(checkpoint(29));CHECK(retired(field)&&!replace_text(field,"late cancel edit"));g_object_unref(field);field=NULL;
        button=wait_node(desktop,pid,&app,"Filter",ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&state(button,ATSPI_STATE_FOCUSED));g_object_unref(button);button=NULL;
        g_object_unref(last);last=lookup(app,argv[3],ATSPI_ROLE_LIST_ITEM);CHECK(last&&state(last,ATSPI_STATE_SELECTED));
        button=lookup(app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));puts("ACK 29");
        puts("RECOVERY_NATIVE_POINTER_RESIZE_COMPACT_RETIRED_FIELD_DRAFT_SELECTION_AND_FOCUS_RESTORED_OK");
    }
    puts("RECOVERY_NATIVE_ORIGINAL_ROWS_FULL_READONLY_TEXT_RETIRED_REFERENCES_KEYBOARD_FILTER_AND_SELECTION_OK");okay=1;
cleanup:
    if(scrollbar)g_object_unref(scrollbar);
    if(button)g_object_unref(button);
    if(field)g_object_unref(field);
    if(detail)g_object_unref(detail);
    if(last)g_object_unref(last);
    if(old_first)g_object_unref(old_first);
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    if(initialized)atspi_exit();
    return okay?0:1;
}
