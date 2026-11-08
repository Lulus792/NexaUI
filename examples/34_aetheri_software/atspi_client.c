/* Independent OS transport peer. No NexaUI headers, types or linkage. */
#define main hierarchy_peer_main
#include "../30_async_hierarchy/atspi_client.c"
#undef main
static const char *expected_dependencies;
static int expect_manifest_failure;
static const char *expected_contents;
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
static int absent(AtspiAccessible *app,const char *name,AtspiRole role){
    AtspiAccessible *node=lookup(app,name,role);int yes=!node&&!scan_failed;if(node)g_object_unref(node);return yes;
}
static int rectangle(AtspiAccessible *node,AtspiRect *out){
    AtspiComponent *c=node?atspi_accessible_get_component_iface(node):NULL;GError *error=NULL;AtspiRect *r=c?atspi_component_get_extents(c,ATSPI_COORD_TYPE_WINDOW,&error):NULL;
    int yes=!error&&r;if(yes)*out=*r;if(error)g_error_free(error);if(r)g_boxed_free(ATSPI_TYPE_RECT,r);if(c)g_object_unref(c);return yes;
}
static int read_details(AtspiAccessible *app,AtspiAccessible *node,AtspiAccessible *back,const char *id){
    int okay=0;AtspiAccessible *area=lookup(app,label("App observation details","App-Beobachtungsdetails"),ATSPI_ROLE_SCROLL_PANE);AtspiText *t=atspi_accessible_get_text_iface(node);GError *error=NULL;
    char *text=t?atspi_text_get_text(t,0,-1,&error):NULL;char expected[80];snprintf(expected,sizeof expected,label("Selected installation: %s","Ausgewählte Installation: %s"),id);
    CHECK(t&&!error&&text&&strstr(text,expected)&&strstr(text,"Runtime:")&&strstr(text,label("Catalogue revision:","Katalogrevision:"))&&g_utf8_validate(text,-1,NULL));
    if(expected_dependencies){
        CHECK(strstr(text,label("Declared files: 32","Deklarierte Dateien: 32"))&&strstr(text,label("Declared actions: 8","Deklarierte Aktionen: 8"))&&strstr(text,label("Declared dependencies: 8","Deklarierte Abhängigkeiten: 8"))&&strstr(text,label("ownership state: unknown","Besitzstatus: unbekannt")));
        for(unsigned i=0;i<8;++i){char expected_dependency[65];memcpy(expected_dependency,expected_dependencies+i*65,64);expected_dependency[64]=0;CHECK(strstr(text,expected_dependency));}
    }
    if(expect_manifest_failure)CHECK(strstr(text,label("Manifest unavailable","Manifest nicht verfügbar"))&&!strstr(text,label("Declared dependencies:","Deklarierte Abhängigkeiten:")));
    {
        AtspiAccessible *contents=lookup(app,label("Contents","Inhalt"),ATSPI_ROLE_PUSH_BUTTON);
        int enabled=contents&&state(contents,ATSPI_STATE_ENABLED)==(expected_dependencies!=NULL);
        if(contents&&!expected_dependencies)enabled=enabled&&!action(contents,"click");
        if(contents)g_object_unref(contents);
        CHECK(enabled);
    }
    CHECK(atspi_text_get_caret_offset(t,&error)==-1&&!error&&atspi_text_get_n_selections(t,&error)==0&&!error&&!replace_text(node,"changed"));
    AtspiRect first,outer,current;CHECK(rectangle(node,&first)&&rectangle(area,&outer)&&first.height>0&&outer.height>0&&state(back,ATSPI_STATE_FOCUSED));
    /* OS extents enclose fractional logical bounds: reported height can exceed
       the true viewport by less than two pixels. Use its conservative lower
       bound to reach the end, then verify actual extents rather than assuming. */
    long long overflow=(long long)first.y+first.height-outer.y-outer.height,viewport_min=outer.height>2?outer.height-2:1;
    unsigned pages=overflow>0?(unsigned)((2*(overflow+2)+viewport_min-1)/viewport_min):0;CHECK(pages<256);
    if(!pages)CHECK(first.y>=outer.y&&(long long)first.y+first.height<=outer.y+outer.height);
    printf("READ_PAGES %u\n",pages);current=first;
    for(unsigned direction=0;direction<2;++direction)for(unsigned i=0;i<pages;++i){
        char line[64],wanted[64];snprintf(wanted,sizeof wanted,"PAGE %c %u\n",direction?'U':'D',i);CHECK(fgets(line,sizeof line,stdin)&&!strcmp(line,wanted));
        AtspiRect next;CHECK(rectangle(node,&next)&&state(back,ATSPI_STATE_FOCUSED));
        CHECK(direction?next.y>=current.y&&(long long)next.y-current.y<=outer.height/2+1:next.y<=current.y&&(long long)current.y-next.y<=outer.height/2+1);current=next;
        if(!direction&&i+1==pages){printf("DETAIL_END y=%d height=%d viewport_y=%d viewport_height=%d\n",current.y,current.height,outer.y,outer.height);CHECK((long long)current.y+current.height<=outer.y+outer.height&&(long long)current.y+current.height>outer.y+outer.height/2);}
        printf("PAGE_ACK %c %u\n",direction?'U':'D',i);
    }
    CHECK(current.y==first.y);okay=1;
cleanup:
    g_free(text);if(error)g_error_free(error);if(t)g_object_unref(t);if(area)g_object_unref(area);return okay;
}
#include "contents_peer.h"
int main(int argc,char **argv){
    int okay=0,initialized=0,ime=0,resizing=0,outputs=0,hotplug=0,clipboard=0,large=0;AtspiAccessible *desktop=NULL,*app=NULL,*old_first=NULL,*last=NULL,*detail=NULL,*field=NULL,*button=NULL;
    CHECK(argc>=4&&argc<=13);for(int i=4;i<argc;++i){if(!strcmp(argv[i],"--german"))german=1;else if(!strcmp(argv[i],"--ime"))ime=1;else if(!strcmp(argv[i],"--large"))large=1;
        else if(!strcmp(argv[i],"--components")){CHECK(++i<argc&&strlen(argv[i])==519&&strspn(argv[i],"0123456789abcdef,")==519);expected_dependencies=argv[i];}
        else if(!strcmp(argv[i],"--component-failure"))expect_manifest_failure=1;
        else if(!strcmp(argv[i],"--contents")){CHECK(++i<argc);expected_contents=argv[i];}
        else if(!strcmp(argv[i],"--resize"))resizing=1;
        else if(!strcmp(argv[i],"--outputs"))outputs=1;
        else if(!strcmp(argv[i],"--hotplug"))hotplug=1;
        else if(!strcmp(argv[i],"--clipboard-normal"))clipboard=1;
        else if(!strcmp(argv[i],"--clipboard-close"))clipboard=2;else CHECK(0);}
    CHECK(resizing+outputs+(clipboard!=0)<=1&&(!(resizing||outputs||clipboard)||(!expected_dependencies&&!expect_manifest_failure&&!expected_contents)));
    CHECK(!hotplug||outputs);
    char *end;unsigned long pid=strtoul(argv[1],&end,10);CHECK(pid&&!*end&&pid<=2147483647UL);const char *id=strrchr(argv[3],'\n');CHECK(id&&strlen(++id)==32);
    setvbuf(stdout,NULL,_IONBF,0);CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    old_first=wait_node(desktop,pid,&app,argv[2],ATSPI_ROLE_LIST_ITEM);CHECK(old_first);
    button=lookup(app,label("Filter","Filter"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));g_object_unref(button);button=NULL;
    CHECK(checkpoint(0));CHECK(!retired(old_first));puts("ACK 0");
    CHECK(checkpoint(1));last=wait_node(desktop,pid,&app,argv[3],ATSPI_ROLE_LIST_ITEM);CHECK(last&&state(last,ATSPI_STATE_FOCUSED)&&action(last,"select"));puts("ACK 1");
    CHECK(checkpoint(2));button=wait_node(desktop,pid,&app,label("Back","Zurück"),ATSPI_ROLE_PUSH_BUTTON);detail=lookup(app,label("Selected installation details","Details der ausgewählten Installation"),ATSPI_ROLE_LABEL);
    CHECK(button&&detail&&retired(old_first)&&retired(last)&&!action(last,"select")&&read_details(app,detail,button,id));g_object_unref(button);button=NULL;
    if(expected_contents){CHECK(inspect_contents(desktop,pid,&app,expected_contents));}
    puts("ACK 2");
    CHECK(checkpoint(3));CHECK(retired(detail));g_object_unref(detail);detail=NULL;g_object_unref(last);last=wait_node(desktop,pid,&app,argv[3],ATSPI_ROLE_LIST_ITEM);CHECK(last&&state(last,ATSPI_STATE_FOCUSED));
    button=lookup(app,label("Read","Lesen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&action(button,"click"));g_object_unref(button);button=NULL;puts("ACK 3");
    CHECK(checkpoint(4));CHECK(retired(last)&&!action(last,"select"));g_object_unref(last);last=NULL;
    button=wait_node(desktop,pid,&app,label("Filter","Filter"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&action(button,"click"));g_object_unref(button);button=NULL;puts("ACK 4");
    CHECK(checkpoint(5));field=wait_node(desktop,pid,&app,label("App filter","App-Filter"),ATSPI_ROLE_ENTRY);CHECK(field&&text_equal(field,"x")&&state(field,ATSPI_STATE_FOCUSED));puts("ACK 5");
    CHECK(checkpoint(6));CHECK(text_equal(field,"x"));button=lookup(app,label("Apply","Anwenden"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button);
    if(ime)CHECK(!state(button,ATSPI_STATE_ENABLED)&&!action(button,"click"));
    g_object_unref(button);button=NULL;puts("ACK 6");
    CHECK(checkpoint(7));CHECK(text_equal(field,ime?"xé":"x"));button=lookup(app,label("Apply","Anwenden"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&state(button,ATSPI_STATE_ENABLED));g_object_unref(button);button=NULL;puts("ACK 7");
    CHECK(checkpoint(8));CHECK(retired(field));g_object_unref(field);field=NULL;button=wait_node(desktop,pid,&app,label("No matching selected targets","Keine passenden ausgewählten Ziele"),ATSPI_ROLE_LABEL);CHECK(button);g_object_unref(button);
    button=lookup(app,label("Filter","Filter"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&action(button,"click"));g_object_unref(button);button=NULL;
    field=wait_node(desktop,pid,&app,label("App filter","App-Filter"),ATSPI_ROLE_ENTRY);CHECK(field&&text_equal(field,ime?"xé":"x")&&replace_text(field,"")&&focus(field));
    double until=now()+3;while(now()<until&&!text_equal(field,""))pump();CHECK(text_equal(field,""));puts("ACK 8");
    CHECK(checkpoint(9));CHECK(retired(field));g_object_unref(field);field=NULL;last=wait_node(desktop,pid,&app,argv[3],ATSPI_ROLE_LIST_ITEM);CHECK(last&&state(last,ATSPI_STATE_FOCUSED)&&state(last,ATSPI_STATE_SELECTED));
    button=lookup(app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));puts("ACK 9");
    if(outputs){
        static const char draft[]="output draft \xc3\xa9";
        g_object_unref(button);button=lookup(app,"Filter",ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));g_object_unref(button);button=NULL;
        CHECK(checkpoint(10));field=wait_node(desktop,pid,&app,label("App filter","App-Filter"),ATSPI_ROLE_ENTRY);CHECK(field&&text_equal(field,"")&&state(field,ATSPI_STATE_FOCUSED)&&replace_text(field,draft));
        until=now()+3;while(now()<until&&!text_equal(field,draft))pump();CHECK(text_equal(field,draft));
        AtspiText *t=atspi_accessible_get_text_iface(field);GError *error=NULL;
        int positioned=t&&atspi_text_set_caret_offset(t,5,&error)&&!error;
        if(error)g_error_free(error);
        if(t)g_object_unref(t);
        CHECK(positioned);AtspiRect original_bounds;CHECK(rectangle(field,&original_bounds));puts("ACK 10");
        for(unsigned step=11;step<=(hotplug?33u:21u);++step){
            CHECK(checkpoint(step));CHECK(!retired(field)&&text_equal(field,draft)&&state(field,ATSPI_STATE_FOCUSED));
            t=atspi_accessible_get_text_iface(field);error=NULL;
            int caret=t?atspi_text_get_caret_offset(t,&error):-1;
            int retained=t&&!error&&caret==5;
            if(error)g_error_free(error);
            if(t)g_object_unref(t);
            CHECK(retained);
            AtspiRect bounds;CHECK(rectangle(field,&bounds)&&bounds.x>=0&&bounds.y>=0&&bounds.width>16&&bounds.height>16);
            int width=step==14||step==15?1920:step==16?1600:original_bounds.width+24;
            if(hotplug&&step==23)width=1536;
            if(hotplug&&(step==24||step==25))width=2560;
            CHECK(bounds.x==12&&bounds.width==width-24&&bounds.height==original_bounds.height);
            const char *names[]={label("Apply","Anwenden"),label("Back","Zurück"),label("Close","Schließen")};
            for(unsigned i=0;i<3;++i){
                AtspiAccessible *header=lookup(app,names[i],ATSPI_ROLE_PUSH_BUTTON);AtspiRect b;
                int fits=header&&rectangle(header,&b)&&b.x>=0&&b.y>=0&&b.width>0&&b.height>0&&b.x+b.width<=width&&b.y+b.height<=bounds.y;
                if(header)g_object_unref(header);
                CHECK(fits);
            }
            if(step==21||step==33)CHECK(bounds.x==original_bounds.x&&bounds.y==original_bounds.y&&bounds.width==original_bounds.width&&bounds.height==original_bounds.height);
            if(step==21||step==33)printf("OUTPUT_FIELD %d %d %d %d\n",bounds.x,bounds.y,bounds.width,bounds.height);
            printf("OUTPUT_FIELD_RETAINED step=%u caret=%d x=%d y=%d width=%d height=%d\n",step,caret,bounds.x,bounds.y,bounds.width,bounds.height);
            printf("ACK %u\n",step);
        }
        unsigned edit=hotplug?34u:22u,cancel=edit+1;
        CHECK(checkpoint(edit));until=now()+3;while(now()<until&&!text_equal(field,"output draft \xc3\xa9x"))pump();CHECK(text_equal(field,"output draft \xc3\xa9x")&&state(field,ATSPI_STATE_FOCUSED));printf("ACK %u\n",edit);
        CHECK(checkpoint(cancel));CHECK(retired(field)&&!replace_text(field,"late output edit"));
        button=wait_node(desktop,pid,&app,"Filter",ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&state(button,ATSPI_STATE_FOCUSED));
        g_object_unref(button);button=lookup(app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));printf("ACK %u\n",cancel);
        if(hotplug)puts("SOFTWARE_NATIVE_OUTPUT_REMOVAL_RECREATION_MAXIMIZED_DRAFT_CARET_FOCUS_OK");
        puts("SOFTWARE_NATIVE_OUTPUT_SCALE_WORKAREA_DRAFT_CARET_AND_HIT_OK");
    }
    if(resizing||clipboard){g_object_unref(button);button=lookup(app,"Filter",ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));g_object_unref(button);button=NULL;}
    if(resizing){
        static const char draft[]="kept draft \xc3\xa9";
        CHECK(checkpoint(10));field=wait_node(desktop,pid,&app,label("App filter","App-Filter"),ATSPI_ROLE_ENTRY);CHECK(field&&text_equal(field,"")&&state(field,ATSPI_STATE_FOCUSED)&&replace_text(field,draft));
        until=now()+3;while(now()<until&&!text_equal(field,draft))pump();CHECK(text_equal(field,draft));
        if(german&&large){
            AtspiAccessible *apply=lookup(app,"Anwenden",ATSPI_ROLE_PUSH_BUTTON),*back=lookup(app,"Zurück",ATSPI_ROLE_PUSH_BUTTON),*close=lookup(app,"Schließen",ATSPI_ROLE_PUSH_BUTTON);AtspiRect a,b,c,f;
            int fits=apply&&back&&close&&rectangle(apply,&a)&&rectangle(back,&b)&&rectangle(close,&c)&&rectangle(field,&f)&&a.y==b.y&&c.y>a.y&&a.x+a.width<=b.x&&c.y+c.height<=f.y;
            if(apply)g_object_unref(apply);
            if(back)g_object_unref(back);
            if(close)g_object_unref(close);
            CHECK(fits);puts("SOFTWARE_NATIVE_LARGE_GERMAN_ACTION_ROWS_ABOVE_FILTER_OK");
        }
        button=lookup(app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button);puts("ACK 10");
        for(unsigned step=11;step<=12;++step){
            CHECK(checkpoint(step));CHECK(retired(field)&&!replace_text(field,"late compact edit")&&state(button,ATSPI_STATE_FOCUSED));
            CHECK(absent(app,label("App filter","App-Filter"),ATSPI_ROLE_ENTRY));AtspiRect bounds;CHECK(rectangle(button,&bounds));
            int width=step==11?64:240,height=step==11?64:96;
            CHECK(bounds.x>=0&&bounds.y>=0&&bounds.width>0&&bounds.height>0&&bounds.x+bounds.width<=width&&bounds.y+bounds.height<=height);printf("ACK %u\n",step);
        }
        CHECK(checkpoint(13));CHECK(retired(field)&&!replace_text(field,"late restored edit"));g_object_unref(button);button=NULL;
        button=wait_node(desktop,pid,&app,label("App filter","App-Filter"),ATSPI_ROLE_ENTRY);CHECK(button&&state(button,ATSPI_STATE_FOCUSED)&&text_equal(button,draft)&&!retired(button));
        g_object_unref(field);field=button;button=NULL;puts("ACK 13");
        CHECK(checkpoint(14));CHECK(retired(field)&&!replace_text(field,"late cancel edit"));g_object_unref(field);field=NULL;
        button=wait_node(desktop,pid,&app,"Filter",ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&state(button,ATSPI_STATE_FOCUSED));g_object_unref(button);button=NULL;
        g_object_unref(last);last=lookup(app,argv[3],ATSPI_ROLE_LIST_ITEM);CHECK(last&&state(last,ATSPI_STATE_SELECTED));
        button=lookup(app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));puts("ACK 14");
        g_object_unref(button);button=lookup(app,"Filter",ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));g_object_unref(button);button=NULL;
        CHECK(checkpoint(15));field=wait_node(desktop,pid,&app,label("App filter","App-Filter"),ATSPI_ROLE_ENTRY);CHECK(field&&state(field,ATSPI_STATE_FOCUSED)&&text_equal(field,draft));
        button=lookup(app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button);puts("ACK 15");
        CHECK(checkpoint(16));CHECK(retired(field)&&!replace_text(field,"late compact Escape edit")&&state(button,ATSPI_STATE_FOCUSED));puts("ACK 16");
        CHECK(checkpoint(17));CHECK(retired(button)&&!action(button,"click"));g_object_unref(button);button=wait_node(desktop,pid,&app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&state(button,ATSPI_STATE_FOCUSED));puts("ACK 17");
        CHECK(checkpoint(18));CHECK(retired(field)&&!replace_text(field,"late list edit"));g_object_unref(button);button=wait_node(desktop,pid,&app,"Filter",ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&state(button,ATSPI_STATE_FOCUSED));g_object_unref(button);button=NULL;puts("ACK 18");
        CHECK(checkpoint(19));CHECK(retired(field));g_object_unref(field);field=wait_node(desktop,pid,&app,label("App filter","App-Filter"),ATSPI_ROLE_ENTRY);CHECK(field&&text_equal(field,draft)&&state(field,ATSPI_STATE_FOCUSED));
        button=lookup(app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));puts("ACK 19");
        puts("SOFTWARE_NATIVE_COMPACT_ESCAPE_RETIRED_REFERENCES_FILTER_FOCUS_AND_DRAFT_REOPEN_OK");
        puts("SOFTWARE_NATIVE_POINTER_RESIZE_COMPACT_RETIRED_FIELD_DRAFT_AND_FOCUS_RESTORED_OK");
    }
    if(clipboard){
        static const char sample[]="Gr\xc3\xbc\xc3\x9f" "e \xf0\x9f\x8c\x8d e\xcc\x81";
        CHECK(checkpoint(10));field=wait_node(desktop,pid,&app,label("App filter","App-Filter"),ATSPI_ROLE_ENTRY);CHECK(field&&text_equal(field,"")&&state(field,ATSPI_STATE_FOCUSED));
        AtspiRect bounds;CHECK(rectangle(field,&bounds));printf("FIELD_BOUNDS %d %d %d %d\n",bounds.x,bounds.y,bounds.width,bounds.height);puts("ACK 10");
        CHECK(checkpoint(11));until=now()+3;while(now()<until&&!text_equal(field,sample))pump();CHECK(text_equal(field,sample)&&state(field,ATSPI_STATE_FOCUSED));puts("ACK 11");
        CHECK(checkpoint(12));CHECK(text_equal(field,sample)&&state(field,ATSPI_STATE_FOCUSED));puts("ACK 12");
        CHECK(checkpoint(13));CHECK(text_equal(field,sample)&&state(field,ATSPI_STATE_FOCUSED));
        if(clipboard==2){button=lookup(app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));}puts("ACK 13");
        CHECK(checkpoint(14));
        if(clipboard==2){CHECK(retired(field)&&retired(button)&&!replace_text(field,"late closed clipboard")&&!action(button,"click"));puts("ACK 14");}
        else{
            CHECK(!state(field,ATSPI_STATE_FOCUSED)&&text_equal(field,sample));puts("ACK 14");
            CHECK(checkpoint(15));CHECK(retired(field)&&!replace_text(field,"late cancelled clipboard"));g_object_unref(field);field=NULL;
            button=wait_node(desktop,pid,&app,"Filter",ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&state(button,ATSPI_STATE_FOCUSED));g_object_unref(button);button=NULL;
            g_object_unref(last);last=lookup(app,argv[3],ATSPI_ROLE_LIST_ITEM);CHECK(last&&state(last,ATSPI_STATE_SELECTED));
            button=lookup(app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));puts("ACK 15");
        }
        puts(clipboard==2?"SOFTWARE_NATIVE_PENDING_FOREIGN_CLIPBOARD_CLOSE_RETIRED_REFERENCES_OK":"SOFTWARE_NATIVE_FOREIGN_UNICODE_PASTE_COPY_AND_LATE_FOCUS_REPLY_REFUSED_OK");
    }
    puts("SOFTWARE_NATIVE_SELECTED_ROWS_READONLY_DETAILS_RETIRED_REFERENCES_KEYBOARD_FILTER_SELECTION_OK");okay=1;
cleanup:
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
