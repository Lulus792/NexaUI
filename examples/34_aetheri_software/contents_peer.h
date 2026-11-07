/* Included by the independent OS peer. Uses only libatspi and transport helpers. */
static unsigned content_sequence;
static int content_focused(AtspiAccessible *node){
    double until=now()+3;
    while(node&&now()<until){if(state(node,ATSPI_STATE_FOCUSED))return 1;pump();}
    AtspiRect bounds={0};(void)rectangle(node,&bounds);
    fprintf(stderr,"CONTENTS_FOCUS_MISSING showing=%d visible=%d bounds=%d,%d,%d,%d\n",state(node,ATSPI_STATE_SHOWING),state(node,ATSPI_STATE_VISIBLE),bounds.x,bounds.y,bounds.width,bounds.height);
    return 0;
}
static int content_command(const char *kind,unsigned a,unsigned b){
    unsigned n=content_sequence++;printf("CONTENTS %u %s %u %u\n",n,kind,a,b);
    char line[80],expected[80];snprintf(expected,sizeof expected,"CONTENTS_ACK %u\n",n);
    return fgets(line,sizeof line,stdin)&&!strcmp(line,expected);
}
static int content_read_end(AtspiAccessible *app,AtspiAccessible *node){
    int okay=0;AtspiAccessible *area=lookup(app,label("App observation details","App-Beobachtungsdetails"),ATSPI_ROLE_SCROLL_PANE);
    AtspiRect inner,outer;CHECK(rectangle(node,&inner)&&rectangle(area,&outer)&&inner.y>=outer.y&&outer.height>2);
    unsigned pages=0;
    while((long long)inner.y+inner.height>outer.y+outer.height){
        CHECK(++pages<256&&content_command("KEY",109,0));AtspiRect next;CHECK(rectangle(node,&next)&&next.y<inner.y);inner=next;
    }
    CHECK((long long)inner.y+inner.height<=outer.y+outer.height);okay=1;
cleanup:
    if(area)g_object_unref(area);
    return okay;
}
static int content_text(AtspiAccessible *node,const char *a,const char *b,const char *c){
    AtspiText *iface=node?atspi_accessible_get_text_iface(node):NULL;GError *error=NULL;
    char *text=iface?atspi_text_get_text(iface,0,-1,&error):NULL;
    int yes=!error&&text&&strstr(text,a)&&strstr(text,b)&&strstr(text,c)&&g_utf8_validate(text,-1,NULL)&&!replace_text(node,"changed");
    if(iface)g_object_unref(iface);
    g_free(text);if(error)g_error_free(error);return yes;
}
static int inspect_contents(AtspiAccessible *desktop,unsigned long pid,AtspiAccessible **app,const char *expected_file){
    int okay=0;FILE *f=fopen(expected_file,"rb");char expected[4][512];
    AtspiAccessible *button=NULL,*entry=NULL,*old_entry=NULL,*detail=NULL,*tree=NULL;
    CHECK(f);for(unsigned i=0;i<4;++i){CHECK(fgets(expected[i],sizeof expected[i],f));size_t n=strlen(expected[i]);CHECK(n&&expected[i][n-1]=='\n');expected[i][n-1]=0;}
    CHECK(!fclose(f));f=NULL;CHECK(strlen(expected[0])==256&&strlen(expected[1])==64&&strlen(expected[3])==256);
    button=lookup(*app,label("Contents","Inhalt"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&action(button,"click")&&content_command("SYNC",3,201));g_object_unref(button);button=NULL;
    tree=wait_node(desktop,pid,app,label("Declared manifest contents","Deklarierter Manifestinhalt"),ATSPI_ROLE_TREE);CHECK(tree);
    for(unsigned i=0;i<32;++i){char name[257];if(i<31)snprintf(name,sizeof name,"entry-%02u",i);else memcpy(name,expected[0],257);
        entry=lookup(*app,name,ATSPI_ROLE_TREE_ITEM);CHECK(entry);g_object_unref(entry);entry=NULL;}
    for(unsigned i=0;i<8;++i){char name[32];snprintf(name,sizeof name,"run-%u",i);entry=lookup(*app,name,ATSPI_ROLE_TREE_ITEM);CHECK(entry);g_object_unref(entry);entry=NULL;}
    CHECK(content_command("KEY",107,0)&&content_command("SYNC",3,407));old_entry=lookup(*app,"run-7",ATSPI_ROLE_TREE_ITEM);CHECK(old_entry&&content_focused(old_entry));
    CHECK(content_command("KEY",28,0)&&content_command("SYNC",4,43));detail=wait_node(desktop,pid,app,label("Declared entry details","Details des deklarierten Eintrags"),ATSPI_ROLE_LABEL);
    CHECK(detail&&retired(tree)&&retired(old_entry)&&!action(old_entry,"select")&&content_text(detail,label("Declared action: run-7","Deklarierte Aktion: run-7"),"argv[0]): entry-00",expected[3]));
    for(unsigned i=1;i<=8;++i){char argument[80];snprintf(argument,sizeof argument,label("Argument %u (256 UTF-8 bytes):","Argument %u (256 UTF-8-Bytes):"),i);CHECK(content_text(detail,argument,"YZ",label("no action is executed","keine Aktion ausgeführt")));}
    CHECK(content_read_end(*app,detail)&&content_command("KEY",1,0)&&content_command("SYNC",3,407)&&retired(detail));g_object_unref(detail);detail=NULL;
    for(unsigned i=0;i<9;++i){CHECK(content_command("KEY",103,0));}
    CHECK(content_command("SYNC",3,331));entry=wait_node(desktop,pid,app,expected[0],ATSPI_ROLE_TREE_ITEM);CHECK(entry&&content_focused(entry));
    CHECK(content_command("KEY",28,0)&&content_command("SYNC",4,43));detail=wait_node(desktop,pid,app,label("Declared entry details","Details des deklarierten Eintrags"),ATSPI_ROLE_LABEL);
    CHECK(detail&&retired(entry)&&content_text(detail,expected[0],expected[1],expected[2])&&content_text(detail,"0444",label("unknown","unbekannt"),label("Declared file:","Deklarierte Datei:"))&&content_read_end(*app,detail));
    CHECK(content_command("KEY",1,0)&&content_command("SYNC",3,331)&&retired(detail));g_object_unref(detail);detail=NULL;
    CHECK(content_command("KEY",1,0)&&content_command("SYNC",2,48));
    CHECK(content_command("DONE",0,0));puts("CONTENTS_ALL_32_FILES_8_ACTIONS_LITERAL_ARGUMENTS_DECLARED_SIZE_NATIVE_FOCUS_RETIREMENT_OK");okay=1;
cleanup:
    if(f)fclose(f);
    if(button)g_object_unref(button);
    if(entry)g_object_unref(entry);
    if(old_entry)g_object_unref(old_entry);
    if(detail)g_object_unref(detail);
    if(tree)g_object_unref(tree);
    return okay;
}
