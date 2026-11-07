#include "peer_helpers.h"
static int signal_size(const char *phase) {
    char line[64],expected[64];printf("RESIZE_%s\n",phase);snprintf(expected,sizeof expected,"SIZE_%s\n",phase);
    return fgets(line,sizeof line,stdin)&&!strcmp(line,expected);
}
static int inside(AtspiAccessible *app,AtspiAccessible *node) {
    AtspiAccessible *root=lookup(app,name("Choose a file","Datei auswählen"),ATSPI_ROLE_PANEL);
    AtspiComponent *a=node?atspi_accessible_get_component_iface(node):NULL,*b=root?atspi_accessible_get_component_iface(root):NULL;GError *error=NULL;
    AtspiRect *inner=a?atspi_component_get_extents(a,ATSPI_COORD_TYPE_WINDOW,&error):NULL;
    AtspiRect *outer=b&&!error?atspi_component_get_extents(b,ATSPI_COORD_TYPE_WINDOW,&error):NULL;
    int yes=!error&&inner&&outer&&inner->width>0&&inner->height>0&&inner->x>=outer->x&&inner->y>=outer->y&&
        (long long)inner->x+inner->width<=(long long)outer->x+outer->width&&(long long)inner->y+inner->height<=(long long)outer->y+outer->height;
    if(error)g_error_free(error);
    if(inner)g_boxed_free(ATSPI_TYPE_RECT,inner);
    if(outer)g_boxed_free(ATSPI_TYPE_RECT,outer);
    if(a)g_object_unref(a);
    if(b)g_object_unref(b);
    if(root)g_object_unref(root);
    return yes;
}
static int error_top_visible(AtspiAccessible *app,AtspiAccessible *node,int *pages) {
    AtspiAccessible *area=lookup(app,name("Folder contents","Ordnerinhalt"),ATSPI_ROLE_SCROLL_PANE);
    AtspiComponent *a=node?atspi_accessible_get_component_iface(node):NULL,*b=area?atspi_accessible_get_component_iface(area):NULL;GError *error=NULL;
    AtspiRect *inner=a?atspi_component_get_extents(a,ATSPI_COORD_TYPE_WINDOW,&error):NULL;
    AtspiRect *outer=b&&!error?atspi_component_get_extents(b,ATSPI_COORD_TYPE_WINDOW,&error):NULL;
    int readable=inner&&inner->height<60?inner->height:60;
    if(pages&&inner&&outer&&outer->height>0) {
        long long delta=(long long)outer->y-inner->y;
        *pages=delta>0?(int)((2*(delta+1)+outer->height-1)/outer->height):0;
    }
    int yes=!error&&inner&&outer&&inner->x>=outer->x&&inner->y>=outer->y&&
        (long long)inner->x+inner->width<=(long long)outer->x+outer->width&&(long long)inner->y+readable<=(long long)outer->y+outer->height;
    if(error)g_error_free(error);
    if(inner)g_boxed_free(ATSPI_TYPE_RECT,inner);
    if(outer)g_boxed_free(ATSPI_TYPE_RECT,outer);
    if(a)g_object_unref(a);
    if(b)g_object_unref(b);
    if(area)g_object_unref(area);
    return yes;
}
static const char *initial_failure(void){return name("Too many entries in this folder (maximum 4,096). No folder was loaded.","Dieser Ordner enthält zu viele Einträge (höchstens 4.096). Kein Ordner wurde geladen.");}
static const char *retained_failure(void){return name("Too many entries in this folder (maximum 4,096). Previous folder retained.","Dieser Ordner enthält zu viele Einträge (höchstens 4.096). Vorheriger Ordner bleibt geöffnet.");}
static const char *changed_failure(void){return name("Folder or entry changed. Refresh before retrying.","Ordner oder Eintrag geändert. Vor erneutem Versuch aktualisieren.");}
static const char *compact_help(void){return name("Enlarge this window to continue choosing a file. Your folder and draft are retained.","Vergrößern Sie dieses Fenster, um eine Datei auszuwählen. Ordner und Entwurf bleiben erhalten.");}
int main(int argc,char **argv) {
    int okay=0,initialized=0;AtspiAccessible *desktop=NULL,*app=NULL,*node=NULL,*retry=NULL,*old_retry=NULL,*old_row=NULL,*field=NULL,*old_field=NULL,*old_open=NULL;
    CHECK(argc==2||(argc==3&&!strcmp(argv[2],"--german")));german=argc==3;
    char *end;unsigned long pid=strtoul(argv[1],&end,10);CHECK(pid&&!*end&&pid<=2147483647UL);
    setvbuf(stdout,NULL,_IONBF,0);CHECK(atspi_init()==0);initialized=1;atspi_set_timeout(1000,1000);desktop=atspi_get_desktop(0);CHECK(desktop);
    node=wait_for(desktop,pid,&app,initial_failure(),ATSPI_ROLE_STATUS_BAR);CHECK(node&&state(node,ATSPI_STATE_INVALID_ENTRY));g_object_unref(node);node=NULL;
    node=lookup(app,name("No matching entries","Keine passenden Einträge"),ATSPI_ROLE_STATUS_BAR);CHECK(!node&&!scan_failed);
    retry=lookup(app,name("Retry","Erneut versuchen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(retry&&focus(retry));
    CHECK(step(0));node=wait_for(desktop,pid,&app,"large",ATSPI_ROLE_LIST_ITEM);CHECK(node&&retired(retry)&&!action(retry,"click")&&focus(node));g_object_unref(node);node=NULL;puts("ACK 0");
    CHECK(step(1));node=wait_for(desktop,pid,&app,retained_failure(),ATSPI_ROLE_STATUS_BAR);CHECK(node&&state(node,ATSPI_STATE_INVALID_ENTRY));g_object_unref(node);node=NULL;
    node=lookup(app,german?"1–16 von 4096 Einträgen (4096 insgesamt)":"1–16 of 4096 entries (4096 total)",ATSPI_ROLE_LABEL);CHECK(node);g_object_unref(node);node=NULL;
    old_retry=retry;retry=lookup(app,name("Retry","Erneut versuchen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(retry&&focus(retry)&&retired(old_retry));
    int pages=0;node=lookup(app,retained_failure(),ATSPI_ROLE_STATUS_BAR);CHECK(node);(void)error_top_visible(app,node,&pages);g_object_unref(node);node=NULL;CHECK(pages>=0&&pages<=64);
    printf("ERROR_READ %d\n",pages);char read_line[64];CHECK(fgets(read_line,sizeof read_line,stdin)&&!strcmp(read_line,"READ_ERROR\n"));
    node=lookup(app,retained_failure(),ATSPI_ROLE_STATUS_BAR);CHECK(node&&error_top_visible(app,node,NULL)&&state(retry,ATSPI_STATE_FOCUSED));g_object_unref(node);node=NULL;puts("ACK 1");
    CHECK(step(2));node=wait_for(desktop,pid,&app,"item-0000",ATSPI_ROLE_LIST_ITEM);CHECK(node&&retired(retry)&&!action(retry,"click")&&focus(node));g_object_unref(node);node=NULL;puts("ACK 2");
    CHECK(step(3));old_row=wait_for(desktop,pid,&app,"item-4095",ATSPI_ROLE_LIST_ITEM);CHECK(old_row&&state(old_row,ATSPI_STATE_FOCUSED)&&state(old_row,ATSPI_STATE_SELECTED)&&row_visible(app,old_row));
    old_open=lookup(app,name("Open","Öffnen"),ATSPI_ROLE_PUSH_BUTTON);old_field=lookup(app,name("Name filter","Namensfilter"),ATSPI_ROLE_ENTRY);
    CHECK(old_open&&old_field&&edit(old_field,"unapplied draft"));double until=now()+3;while(now()<until&&!text_is(old_field,"unapplied draft"))pump();CHECK(text_is(old_field,"unapplied draft")&&focus(old_row));puts("ACK 3");
    const char *phases[]={"TINY","SMALL","RESTORE"};
    for(unsigned i=0;i<3;++i) {
        CHECK(signal_size(phases[i]));
        if(i<2) {
            node=wait_for(desktop,pid,&app,compact_help(),ATSPI_ROLE_STATUS_BAR);CHECK(node);g_object_unref(node);node=NULL;
            node=lookup(app,name("Cancel","Abbrechen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(node&&state(node,ATSPI_STATE_ENABLED)&&state(node,ATSPI_STATE_FOCUSED)&&inside(app,node));g_object_unref(node);node=NULL;
            CHECK(retired(old_row)&&!action(old_row,"select")&&retired(old_open)&&!action(old_open,"click")&&retired(old_field)&&!edit(old_field,"late geometry edit"));
            node=lookup(app,name("Name filter","Namensfilter"),ATSPI_ROLE_ENTRY);CHECK(!node&&!scan_failed);
        } else {
            node=wait_for(desktop,pid,&app,"item-4095",ATSPI_ROLE_LIST_ITEM);CHECK(node&&state(node,ATSPI_STATE_FOCUSED)&&state(node,ATSPI_STATE_SELECTED)&&row_visible(app,node));g_object_unref(node);node=NULL;
            field=lookup(app,name("Name filter","Namensfilter"),ATSPI_ROLE_ENTRY);CHECK(field&&text_is(field,"unapplied draft")&&retired(old_row)&&!action(old_row,"select"));
        }
        printf("SIZE_ACK_%s\n",phases[i]);
    }
    CHECK(step(4));node=wait_for(desktop,pid,&app,changed_failure(),ATSPI_ROLE_STATUS_BAR);CHECK(node&&state(node,ATSPI_STATE_INVALID_ENTRY));g_object_unref(node);node=NULL;
    node=lookup(app,name("Retry","Erneut versuchen"),ATSPI_ROLE_PUSH_BUTTON);CHECK(!node&&!scan_failed);
    node=lookup(app,name("Refresh folder","Ordner aktualisieren"),ATSPI_ROLE_PUSH_BUTTON);CHECK(node&&state(node,ATSPI_STATE_FOCUSED)&&focus(node));g_object_unref(node);node=NULL;puts("ACK 4");
    CHECK(step(5));node=wait_for(desktop,pid,&app,"item-0000",ATSPI_ROLE_LIST_ITEM);CHECK(node&&retired(field)&&!edit(field,"late pre-refresh edit")&&focus(node));g_object_unref(node);node=NULL;puts("ACK 5");
    CHECK(step(6));node=wait_for(desktop,pid,&app,"item-4095",ATSPI_ROLE_LIST_ITEM);CHECK(node&&state(node,ATSPI_STATE_FOCUSED)&&state(node,ATSPI_STATE_SELECTED)&&row_visible(app,node));puts("ACK 6");
    puts("PICKER_NATIVE_INITIAL_RETRY_RETAINED_ENTER_RETRY_CHANGED_REFRESH_AND_RESIZE_VERIFIED");okay=1;
cleanup:
    if(node)g_object_unref(node);
    if(field)g_object_unref(field);
    if(old_field)g_object_unref(old_field);
    if(old_open)g_object_unref(old_open);
    if(old_row)g_object_unref(old_row);
    if(old_retry)g_object_unref(old_retry);
    if(retry)g_object_unref(retry);
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    if(initialized)atspi_exit();
    return okay?0:1;
}
