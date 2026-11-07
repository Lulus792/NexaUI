#ifndef FP_PICKER_OS_PEER_HELPERS_H
#define FP_PICKER_OS_PEER_HELPERS_H
/* Independent OS transport helpers; no NexaUI headers or linkage. */
#define main hierarchy_peer_main
#include "../30_async_hierarchy/atspi_client.c"
#undef main
static const char *name(const char *en,const char *de){return german?de:en;}
static int text_is(AtspiAccessible *node,const char *expected){
    AtspiText *t=node?atspi_accessible_get_text_iface(node):NULL;GError *error=NULL;
    char *s=t?atspi_text_get_text(t,0,-1,&error):NULL;int yes=!error&&s&&!strcmp(s,expected);
    g_free(s);
    if(error)g_error_free(error);
    if(t)g_object_unref(t);
    return yes;
}
static int edit(AtspiAccessible *node,const char *value){
    AtspiEditableText *t=node?atspi_accessible_get_editable_text_iface(node):NULL;GError *error=NULL;
    int yes=t&&atspi_editable_text_set_text_contents(t,value,&error)&&!error;
    if(error)g_error_free(error);
    if(t)g_object_unref(t);
    return yes;
}
static AtspiAccessible *wait_for(AtspiAccessible *desktop,unsigned long pid,AtspiAccessible **app,const char *text,AtspiRole role){
    double until=now()+4;AtspiAccessible *node=NULL;
    while(!node&&now()<until){
        AtspiAccessible *fresh=target(desktop,pid);
        if(fresh){if(*app)g_object_unref(*app);*app=fresh;node=lookup(*app,text,role);}
        if(!node)pump();
    }
    return node;
}
static int row_visible(AtspiAccessible *app,AtspiAccessible *row){
    AtspiAccessible *area=lookup(app,name("Folder contents","Ordnerinhalt"),ATSPI_ROLE_SCROLL_PANE);
    AtspiComponent *a=row?atspi_accessible_get_component_iface(row):NULL,*b=area?atspi_accessible_get_component_iface(area):NULL;GError *error=NULL;
    AtspiRect *inner=a?atspi_component_get_extents(a,ATSPI_COORD_TYPE_WINDOW,&error):NULL;
    AtspiRect *outer=b&&!error?atspi_component_get_extents(b,ATSPI_COORD_TYPE_WINDOW,&error):NULL;
    int yes=!error&&inner&&outer&&inner->width>0&&inner->height>0&&inner->x>=outer->x&&inner->y>=outer->y&&
        (long long)inner->x+inner->width<=(long long)outer->x+outer->width&&(long long)inner->y+inner->height<=(long long)outer->y+outer->height;
    if(error)g_error_free(error);
    if(inner)g_boxed_free(ATSPI_TYPE_RECT,inner);
    if(outer)g_boxed_free(ATSPI_TYPE_RECT,outer);
    if(a)g_object_unref(a);
    if(b)g_object_unref(b);
    if(area)g_object_unref(area);
    return yes;
}
static int step(unsigned number){
    printf("STEP %u\n",number);char s[64];unsigned actual=999;
    return fgets(s,sizeof s,stdin)&&sscanf(s,"CHECK %u",&actual)==1&&actual==number;
}
#endif
