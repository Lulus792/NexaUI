/* Independent OS peer; reuse bounded native transport helpers, no NexaUI. */
#define main hierarchy_peer_main
#include "../30_async_hierarchy/atspi_client.c"
#undef main
static const char *label(const char *en,const char *de){return german?de:en;}
static AtspiAccessible *wait_node(AtspiAccessible *desktop,unsigned long pid,AtspiAccessible **app,const char *name,AtspiRole role){
    double until=now()+4;AtspiAccessible *node=NULL;
    while(!node&&now()<until){AtspiAccessible *fresh=target(desktop,pid);if(fresh){if(*app)g_object_unref(*app);*app=fresh;node=lookup(*app,name,role);}if(!node)pump();}return node;
}
static int checkpoint(unsigned n){printf("STEP %u\n",n);char line[64];unsigned actual=999;return fgets(line,sizeof line,stdin)&&sscanf(line,"CHECK %u",&actual)==1&&actual==n;}
static int replace_text(AtspiAccessible *node,const char *value){
    AtspiEditableText *iface=node?atspi_accessible_get_editable_text_iface(node):NULL;GError *error=NULL;
    int yes=iface&&atspi_editable_text_set_text_contents(iface,value,&error)&&!error;
    if(error)g_error_free(error);
    if(iface)g_object_unref(iface);
    return yes;
}
static int text_equal(AtspiAccessible *node,const char *value){
    AtspiText *iface=node?atspi_accessible_get_text_iface(node):NULL;GError *error=NULL;char *text=iface?atspi_text_get_text(iface,0,-1,&error):NULL;
    int yes=!error&&text&&!strcmp(text,value);g_free(text);if(error)g_error_free(error);if(iface)g_object_unref(iface);return yes;
}
int main(int argc,char **argv){
    int okay=0,initialized=0,closing=0,issuer=0;AtspiAccessible *desktop=NULL,*app=NULL,*row=NULL,*button=NULL,*field=NULL;
    CHECK(argc>=6&&argc<=8);CHECK(!strcmp(argv[4],"--held"));closing=!strcmp(argv[5],"close");CHECK(closing||!strcmp(argv[5],"wake"));
    for(int i=6;i<argc;++i){if(!strcmp(argv[i],"--german")){CHECK(!german);german=1;}else if(!strcmp(argv[i],"--issuer")){CHECK(!issuer);issuer=1;}else CHECK(0);}
    const char *row_name=issuer?argv[3]:argv[2];
    if(issuer){const char *id=strrchr(row_name,'\n');CHECK(id&&!strcmp(id+1,"0000000000004000800000000000005a")&&strstr(row_name,label("Unit issuer","Unit-Aussteller")));}
    char *end;unsigned long pid=strtoul(argv[1],&end,10);CHECK(pid&&!*end&&pid<=2147483647UL);
    setvbuf(stdout,NULL,_IONBF,0);CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    row=wait_node(desktop,pid,&app,row_name,ATSPI_ROLE_LIST_ITEM);CHECK(row);
    CHECK(checkpoint(0));button=lookup(app,label("Read","Lesen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&action(button,"click"));g_object_unref(button);button=NULL;puts("ACK 0");
    CHECK(checkpoint(1));button=wait_node(desktop,pid,&app,label("Read","Lesen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&!state(button,ATSPI_STATE_ENABLED)&&!action(button,"click")&&!retired(row));g_object_unref(button);button=NULL;
    if(closing){
        button=lookup(app,"Filter",ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&action(button,"click"));g_object_unref(button);button=NULL;
        field=wait_node(desktop,pid,&app,label("Record filter","Eintragsfilter"),ATSPI_ROLE_ENTRY);CHECK(field&&replace_text(field,"kept draft"));
        double until=now()+3;while(now()<until&&!text_equal(field,"kept draft"))pump();CHECK(text_equal(field,"kept draft")&&!retired(field));
    }
    button=lookup(app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&state(button,ATSPI_STATE_ENABLED)&&focus(button));puts("ACK 1");
    CHECK(checkpoint(2));
    if(closing){CHECK(retired(field)&&!replace_text(field,"late mutation")&&retired(button)&&!action(button,"click"));puts("RECOVERY_NATIVE_WHOLE_WINDOW_RETIRED_TEXT_AND_ACTIONS_REFUSED_OK");}
    else{
        CHECK(retired(row)&&!action(row,"select"));g_object_unref(row);row=wait_node(desktop,pid,&app,row_name,ATSPI_ROLE_LIST_ITEM);CHECK(row&&!retired(row));
        g_object_unref(button);button=lookup(app,label("Read","Lesen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&state(button,ATSPI_STATE_ENABLED));g_object_unref(button);
        button=lookup(app,label("Close","Schließen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(button&&focus(button));puts("RECOVERY_NATIVE_COMPLETION_REPLACED_ROWS_AND_REENABLED_READ_OK");
    }
    if(issuer){if(closing)CHECK(retired(row)&&!action(row,"select"));puts("RECOVERY_NATIVE_HELD_ROOT11_ISSUER_ROW_LIFETIME_OK");}
    puts("ACK 2");okay=1;
cleanup:
    if(field)g_object_unref(field);
    if(button)g_object_unref(button);
    if(row)g_object_unref(row);
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    if(initialized)atspi_exit();
    return okay?0:1;
}
