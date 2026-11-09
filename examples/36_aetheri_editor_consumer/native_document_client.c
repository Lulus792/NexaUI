/* Independent OS reader. Reuse the SDK fixture's discovery/query helpers;
   this executable links no NexaUI or original Editor model code. */
#define main nexaui_large_text_fixture_main
#include "../../tests/atspi_large_text_client.c"
#undef main
#undef CHECK
#include "../../tests/atspi_cache_probe.h"
#include <stdint.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"original-editor-atspi:%d: %s\n",__LINE__,#x);goto done;}} while(0)
static int expected_text(AtspiText *text,const char *expected) {
    GError *e=NULL;char *actual=atspi_text_get_text(text,0,-1,&e);
    int okay=!e && actual && !strcmp(actual,expected);
    if(!okay)fprintf(stderr,"native text mismatch: actual_bytes=%zu expected_bytes=%zu error=%s\n",actual?strlen(actual):0u,strlen(expected),e?e->message:"none");
    g_clear_error(&e);g_free(actual);return okay;
}
static int expected_file(AtspiText *text,const char *name) {
    FILE *file=fopen(name,"rb");if(!file)return 0;
    int okay=0;char *bytes=NULL;GError *e=NULL;long length;
    if(fseek(file,0,SEEK_END) || (length=ftell(file))<0 || length>8*1024*1024 || fseek(file,0,SEEK_SET))goto done;
    bytes=malloc((size_t)length+1u);if(!bytes || fread(bytes,1,(size_t)length,file)!=(size_t)length)goto done;
    bytes[length]=0;if(!g_utf8_validate(bytes,length,NULL))goto done;
    glong count=g_utf8_strlen(bytes,length);
    if(atspi_text_get_character_count(text,&e)!=count || e)goto done;
    if(length<=1024*1024) {okay=expected_text(text,bytes);goto done;}
    if(count!=length)goto done; /* The explicit maximum-size corpus is ASCII. */
    for(int start=0;start<length;start+=512*1024) {
        int end=start+512*1024;if(end>length)end=(int)length;
        char *part=atspi_text_get_text(text,start,end,&e);
        int same=!e && part && strlen(part)==(size_t)(end-start) && !memcmp(part,bytes+start,(size_t)(end-start));
        g_free(part);if(!same)goto done;
    }
    okay=1;
done:
    g_clear_error(&e);free(bytes);fclose(file);return okay;
}
static int geometry(AtspiAccessible *field,AtspiText *text,int combining) {
    int okay=0;GError *error=NULL;AtspiComponent *component=atspi_accessible_get_component_iface(field);
    AtspiRect *parent=NULL,*a=NULL,*b=NULL,*caret=NULL,*ending=NULL,*next=NULL,*cr=NULL,*lf=NULL;
    if(!component)goto done;
    parent=atspi_component_get_extents(component,ATSPI_COORD_TYPE_WINDOW,&error);if(error || !parent)goto done;
    a=atspi_text_get_character_extents(text,0,ATSPI_COORD_TYPE_WINDOW,&error);if(error || !a)goto done;
    b=atspi_text_get_character_extents(text,1,ATSPI_COORD_TYPE_WINDOW,&error);if(error || !b)goto done;
    caret=atspi_text_get_range_extents(text,0,0,ATSPI_COORD_TYPE_WINDOW,&error);if(error || !caret)goto done;
    if(a->width<=0 || a->height<=0 || a->x<parent->x || a->y<parent->y || a->x+a->width>parent->x+parent->width ||
       a->y+a->height>parent->y+parent->height || caret->x!=a->x || caret->y!=a->y || caret->width<1 || caret->width>3)goto done;
    gint hit=atspi_text_get_offset_at_point(text,a->x+a->width/2,a->y+a->height/2,ATSPI_COORD_TYPE_WINDOW,&error);
    if(error || hit!=0)goto done;
    if(combining) {
        if(a->x!=b->x || a->y!=b->y || a->width!=b->width || a->height!=b->height)goto done;
        cr=atspi_text_get_character_extents(text,3,ATSPI_COORD_TYPE_WINDOW,&error);if(error || !cr)goto done;
        lf=atspi_text_get_character_extents(text,4,ATSPI_COORD_TYPE_WINDOW,&error);if(error || !lf)goto done;
        ending=atspi_text_get_range_extents(text,3,5,ATSPI_COORD_TYPE_WINDOW,&error);if(error || !ending)goto done;
        next=atspi_text_get_character_extents(text,5,ATSPI_COORD_TYPE_WINDOW,&error);if(error || !next)goto done;
        if(cr->x!=lf->x || cr->y!=lf->y || cr->width!=lf->width || cr->width>1 || cr->height<=0 ||
           ending->x!=cr->x || ending->y!=cr->y || ending->width!=cr->width || next->y<=a->y)goto done;
    } else if(b->x<=a->x || b->y!=a->y || b->width<=0)goto done;
    fprintf(stderr,"Original Editor geometry: combining=%d first=%d,%d,%d,%d caret=%d,%d,%d,%d hit=%d\n",
        combining,a->x,a->y,a->width,a->height,caret->x,caret->y,caret->width,caret->height,(int)hit);okay=1;
done:
    if(!okay)fprintf(stderr,"Original Editor geometry failed: %s\n",error?error->message:"rectangle/point relation");
    g_clear_error(&error);g_free(parent);g_free(a);g_free(b);g_free(caret);g_free(ending);g_free(next);g_free(cr);g_free(lf);
    if(component)g_object_unref(component);
    return okay;
}
static int attr(GHashTable *table,const char *name,const char *expected) {
    const char *s=table?g_hash_table_lookup(table,name):NULL;return s && !strcmp(s,expected);
}
static int attributes(AtspiText *text,int variant,int mode) {
    static const char *const sizes[]={"10.5","21","15.75"};
    static const char *const fg[]={"59624,60138,61680","65535,65535,65535","8224,8995,11308"};
    static const char *const bg[]={"8224,8995,11308","0,0,0","62965,63222,63736"};
    static const char *const selected[]={"15163,19532,26214","65535,58082,16448","51400,54998,60138"};
    GError *error=NULL;GHashTable *table=atspi_text_get_default_attributes(text,&error);int okay=0;
    if(error || !table || g_hash_table_size(table)!=8 || !attr(table,"family-name","Liberation Mono") ||
       !attr(table,"weight","400") || !attr(table,"style","normal") || !attr(table,"stretch","normal") ||
       !attr(table,"size",sizes[variant]) || !attr(table,"fg-color",fg[variant]) || !attr(table,"bg-color",bg[variant]) ||
       !attr(table,"editable",mode==2?"false":"true"))goto done;
    g_hash_table_unref(table);table=NULL;int first=-1,last=-1;
    int count=atspi_text_get_character_count(text,&error);if(error || count<1)goto done;
    table=atspi_text_get_attribute_run(text,0,TRUE,&first,&last,&error);
    if(error || !table || g_hash_table_size(table)!=8 || first!=0 || last!=count || !attr(table,"size",sizes[variant]) ||
       !attr(table,"fg-color",mode==1 && variant==1?"0,0,0":fg[variant]) || !attr(table,"bg-color",mode==1?selected[variant]:bg[variant]))goto done;
    g_hash_table_unref(table);table=NULL;
    table=atspi_text_get_attribute_run(text,0,FALSE,&first,&last,&error);
    if(error || !table || g_hash_table_size(table)!=(mode==1?2u:0u) || first!=0 || last!=count)goto done;
    fprintf(stderr,"Original Editor attributes: variant=%d selected=%d editable=%d size=%s family=Liberation Mono weight=400 style=normal defaults=8\n",variant,mode==1,mode!=2,sizes[variant]);okay=1;
done:
    if(!okay)fprintf(stderr,"Original Editor attributes failed: variant=%d mode=%d count=%u error=%s\n",variant,mode,table?g_hash_table_size(table):0u,error?error->message:"default/run values");
    if(table)g_hash_table_unref(table);
    g_clear_error(&error);return okay;
}
struct attribute_events {const char *path;unsigned count;};
static void attribute_event(AtspiEvent *event,void *user) {
    struct attribute_events *events=user;
    if(event->source && !strcmp(ATSPI_OBJECT(event->source)->path,events->path) && !strcmp(event->type,"object:text-attributes-changed"))++events->count;
    g_boxed_free(ATSPI_TYPE_EVENT,event);
}
static int events_at(struct attribute_events *events,unsigned count) {
    for(unsigned i=0;i<100 && events->count<count;++i)pump(10);
    pump(30);return events->count==count;
}
static int size_at(AtspiText *text,const char *size) {
    for(unsigned i=0;i<100;++i) {
        GError *e=NULL;GHashTable *table=atspi_text_get_default_attributes(text,&e);int same=!e && attr(table,"size",size);
        if(table)g_hash_table_unref(table);
        g_clear_error(&e);if(same)return 1;pump(10);
    }
    return 0;
}
static int attribute_changes(AtspiAccessible *app,AtspiAccessible *field,AtspiText *text) {
    int okay=0;GError *error=NULL;AtspiAccessible *plus=NULL,*minus=NULL;
    AtspiAction *up=NULL,*down=NULL;AtspiComponent *component=NULL;AtspiRect *before=NULL,*larger=NULL,*after=NULL;
    struct attribute_events events={ATSPI_OBJECT(field)->path,0};pump(30);
    AtspiEventListener *listener=atspi_event_listener_new(attribute_event,&events,NULL);if(!listener)goto done;
    if(!atspi_event_listener_register(listener,"object:text-attributes-changed",&error) || error)goto done;
    before=atspi_text_get_character_extents(text,0,ATSPI_COORD_TYPE_WINDOW,&error);if(error || !before)goto done;
    if(!atspi_text_add_selection(text,1,3,&error) || error || !events_at(&events,1))goto done;
    int first=-1,last=-1;GHashTable *run=atspi_text_get_attribute_run(text,2,TRUE,&first,&last,&error);
    int selected=!error && run && first==1 && last==3 && attr(run,"bg-color","15163,19532,26214");if(run)g_hash_table_unref(run);if(!selected)goto done;
    if(!atspi_text_set_caret_offset(text,5,&error) || error || !events_at(&events,2))goto done;
    if(!atspi_text_set_caret_offset(text,5,&error) || error)goto done;
    pump(30);if(events.count!=2)goto done;
    plus=find(app,"A+",0);minus=find(app,"A\xe2\x88\x92",0);if(!plus || !minus)goto done;
    up=atspi_accessible_get_action_iface(plus);down=atspi_accessible_get_action_iface(minus);if(!up || !down)goto done;
    if(!atspi_action_do_action(up,0,&error) || error || !size_at(text,"15.75") || !events_at(&events,3))goto done;
    larger=atspi_text_get_character_extents(text,0,ATSPI_COORD_TYPE_WINDOW,&error);if(error || !larger || larger->height<=before->height || larger->width<=before->width)goto done;
    if(!atspi_action_do_action(down,0,&error) || error || !size_at(text,"10.5") || !events_at(&events,4))goto done;
    after=atspi_text_get_character_extents(text,0,ATSPI_COORD_TYPE_WINDOW,&error);if(error || !after || after->height!=before->height || after->width!=before->width)goto done;
    component=atspi_accessible_get_component_iface(field);if(!component || !atspi_component_grab_focus(component,&error) || error)goto done;
    pump(30);if(events.count!=4 || !attributes(text,0,0))goto done;
    fprintf(stderr,"Original Editor attribute events: selection=1 clear=1 unchanged=0 font150=1 font100=1 total=%u geometry=%dx%d->%dx%d->%dx%d\n",events.count,before->width,before->height,larger->width,larger->height,after->width,after->height);okay=1;
done:
    if(!okay)fprintf(stderr,"Original Editor attribute events failed: count=%u error=%s\n",events.count,error?error->message:"selection/font/focus relation");
    g_clear_error(&error);if(listener) {atspi_event_listener_deregister(listener,"object:text-attributes-changed",&error);g_clear_error(&error);g_object_unref(listener);}
    if(plus)g_object_unref(plus);
    if(minus)g_object_unref(minus);
    if(up)g_object_unref(up);
    if(down)g_object_unref(down);
    if(component)g_object_unref(component);
    g_free(before);g_free(larger);g_free(after);return okay;
}
int main(int argc,char **argv) {
    int initialized=0,status=1;GError *e=NULL;AtspiRange *range=NULL;char *reply_text=NULL;
    AtspiAccessible *desktop=NULL,*app=NULL,*field=NULL;AtspiText *text=NULL;AtspiEditableText *edit=NULL;
    CHECK(argc==3 || argc==4);char *end=NULL;unsigned long wanted=strtoul(argv[1],&end,10);
    CHECK(end && !*end && wanted && wanted<=UINT32_MAX && !atspi_init());initialized=1;atspi_set_timeout(1000,1000);
    desktop=atspi_get_desktop(0);CHECK(desktop);
    for(unsigned tries=0;tries<300 && !app;++tries) {
        pump(10);atspi_accessible_clear_cache(desktop);int count=atspi_accessible_get_child_count(desktop,&e);
        if(e){g_clear_error(&e);continue;}
        for(int i=0;i<count;++i) {
            AtspiAccessible *a=atspi_accessible_get_child_at_index(desktop,i,&e);if(e){g_clear_error(&e);continue;}if(!a)continue;
            guint pid=atspi_accessible_get_process_id(a,&e);if(!e && pid==(guint)wanted){app=a;break;}g_clear_error(&e);g_object_unref(a);
        }
    }
    CHECK(app);field=find(app,"Edit document",0);if(!field)field=find(app,"Dokument bearbeiten",0);CHECK(field);
    text=atspi_accessible_get_text_iface(field);edit=atspi_accessible_get_editable_text_iface(field);CHECK(text);
    CHECK(cache_probe(field,0,64u*1024u));
    if(!strcmp(argv[2],"attributes")) {
        CHECK(expected_text(text,"abcde") && attributes(text,0,0) && attribute_changes(app,field,text));
    } else if(!strcmp(argv[2],"attributes-readonly")) {
        CHECK(attributes(text,0,2));
    } else if(!strncmp(argv[2],"attributes-selected-",20) || !strncmp(argv[2],"attributes-visible-",19)) {
        int selected=!strncmp(argv[2],"attributes-selected-",20);const char *name=argv[2]+(selected?20:19);
        int variant=!strcmp(name,"contrast-200")?1:!strcmp(name,"light-150")?2:-1;CHECK(variant>=0 && attributes(text,variant,selected));
    } else if(!strcmp(argv[2],"geometry")) {
        CHECK(edit && expected_text(text,"abcde") && geometry(field,text,0));
        CHECK(atspi_editable_text_set_text_contents(edit,"a\xcc\x81" "b\r\nend",&e) && !e && expected_text(text,"a\xcc\x81" "b\r\nend"));
        CHECK(geometry(field,text,1));
        CHECK(atspi_editable_text_set_text_contents(edit,"abcde",&e) && !e && expected_text(text,"abcde"));
    } else if(!strcmp(argv[2],"geometry-visible")) {
        CHECK(expected_text(text,"AX\xc3\xa9") && geometry(field,text,0));
    } else if(!strcmp(argv[2],"clipboard-seed")) {
        CHECK(edit && atspi_editable_text_set_text_contents(edit,"A\xc3\xa9\xf0\x9f\x8c\x8d\r\nline",&e) && !e);
        CHECK(atspi_text_set_caret_offset(text,0,&e) && !e);
    } else if(!strcmp(argv[2],"clipboard-copy")) {
        CHECK(edit);(void)atspi_editable_text_copy_text(edit,1,3,&e);CHECK(!e);
        CHECK(expected_text(text,"A\xc3\xa9\xf0\x9f\x8c\x8d\r\nline"));
    } else if(!strcmp(argv[2],"clipboard-cut")) {
        CHECK(edit && atspi_editable_text_cut_text(edit,1,3,&e) && !e);
        CHECK(expected_text(text,"A\r\nline"));
    } else if(!strcmp(argv[2],"clipboard-paste") || !strcmp(argv[2],"clipboard-paste-refused") || !strcmp(argv[2],"clipboard-paste-cancelled")) {
        CHECK(argc==4 && edit);puts("READY");fflush(stdout);
        gint64 started=g_get_monotonic_time();
        fprintf(stderr,"native paste begin: phase=%s monotonic_us=%lld\n",argv[2],(long long)started);
        gboolean accepted=atspi_editable_text_paste_text(edit,1,&e);
        fprintf(stderr,"native paste reply: phase=%s accepted=%d elapsed_us=%lld error=%s\n",
            argv[2],accepted,(long long)(g_get_monotonic_time()-started),e?e->message:"none");
        if(!strcmp(argv[2],"clipboard-paste"))CHECK(accepted && !e);
        else {CHECK(!accepted);g_clear_error(&e);}
        if(strcmp(argv[2],"clipboard-paste-cancelled"))CHECK(expected_file(text,argv[3]));
        else {
            reply_text=atspi_text_get_text(text,0,-1,&e);CHECK(e);
            g_clear_error(&e);g_free(reply_text);reply_text=NULL;
        }
    } else if(!strcmp(argv[2],"expect")) {CHECK(argc==4 && expected_file(text,argv[3]));}
    else if(!strcmp(argv[2],"readonly")) {
        CHECK(argc==4 && !edit && expected_file(text,argv[3]));
        CHECK(!atspi_text_set_caret_offset(text,0,&e));g_clear_error(&e);
    } else if(!strcmp(argv[2],"edits")) {
        CHECK(edit && expected_text(text,"abcde"));
        CHECK(atspi_editable_text_set_text_contents(edit,"A\xc3\xa9\xf0\x9f\x8c\x8d\nline",&e) && !e);
        gboolean inserted=atspi_editable_text_insert_text(edit,2,"\xce\xa9",2,&e);
        if(!inserted || e)fprintf(stderr,"native insert: accepted=%d error=%s\n",inserted,e?e->message:"none");
        CHECK(inserted && !e);CHECK(expected_text(text,"A\xc3\xa9\xce\xa9\xf0\x9f\x8c\x8d\nline"));
        CHECK(atspi_text_add_selection(text,0,2,&e) && !e);range=atspi_text_get_selection(text,0,&e);
        CHECK(range && !e && range->start_offset==0 && range->end_offset==2);g_free(range);range=NULL;
        gboolean caret_set=atspi_text_set_caret_offset(text,3,&e);
        if(!caret_set || e)fprintf(stderr,"native caret set: accepted=%d error=%s\n",caret_set,e?e->message:"none");
        CHECK(caret_set && !e);int caret=atspi_text_get_caret_offset(text,&e);
        if(caret!=3 || e)fprintf(stderr,"native caret query: actual=%d expected=3 error=%s\n",caret,e?e->message:"none");
        CHECK(caret==3 && !e);
        CHECK(atspi_editable_text_delete_text(edit,1,3,&e) && !e && expected_text(text,"A\xf0\x9f\x8c\x8d\nline"));
    } else if(!strcmp(argv[2],"grapheme")) {
        CHECK(edit && atspi_editable_text_set_text_contents(edit,"a\xcc\x81" "b\r\nend",&e) && !e);
        CHECK(!atspi_text_set_caret_offset(text,1,&e));g_clear_error(&e);
        CHECK(!atspi_editable_text_delete_text(edit,3,4,&e));g_clear_error(&e);
        CHECK(expected_text(text,"a\xcc\x81" "b\r\nend"));
    } else if(!strcmp(argv[2],"final")) {
        CHECK(edit && atspi_editable_text_set_text_contents(edit,"assistive\nsaved",&e) && !e && expected_text(text,"assistive\nsaved"));
    } else if(!strcmp(argv[2],"style")) {
        CHECK(edit && expected_text(text,"changed"));range=atspi_text_get_selection(text,0,&e);
        CHECK(range && !e && range->start_offset==0 && range->end_offset==7);g_free(range);range=NULL;
        CHECK(atspi_editable_text_set_text_contents(edit,"AX\xc3\xa9",&e) && !e);
    } else if(!strcmp(argv[2],"large")) {
        CHECK(argc==4 && edit && expected_file(text,argv[3]));int count=atspi_text_get_character_count(text,&e);
        CHECK(!e && count==8*1024*1024);
        reply_text=atspi_text_get_text(text,0,-1,&e);CHECK(e);g_clear_error(&e);g_free(reply_text);reply_text=NULL;
        CHECK(!atspi_editable_text_insert_text(edit,0,"x",1,&e));g_clear_error(&e);
        CHECK(expected_file(text,argv[3]));
        gint64 started=g_get_monotonic_time();gboolean deleted=atspi_editable_text_delete_text(edit,0,1,&e);
        fprintf(stderr,"native large delete: accepted=%d elapsed_us=%lld error=%s\n",deleted,(long long)(g_get_monotonic_time()-started),e?e->message:"none");
        CHECK(deleted && !e);started=g_get_monotonic_time();gboolean restored=atspi_editable_text_insert_text(edit,0,"A",1,&e);
        fprintf(stderr,"native large restore: accepted=%d elapsed_us=%lld error=%s\n",restored,(long long)(g_get_monotonic_time()-started),e?e->message:"none");
        CHECK(restored && !e);
        CHECK(expected_file(text,argv[3]));
    } else if(!strcmp(argv[2],"hold")) {
        CHECK(edit);puts("READY");fflush(stdout);char command[32];int quit=0;
        while(fgets(command,sizeof(command),stdin)) {
            if(!strcmp(command,"quit\n")){quit=1;break;}
            CHECK(!strcmp(command,"reject\n") && !atspi_editable_text_set_text_contents(edit,"stale",&e));g_clear_error(&e);
            reply_text=atspi_text_get_text(text,0,-1,&e);CHECK(e);g_clear_error(&e);g_free(reply_text);reply_text=NULL;
            puts("REJECTED");fflush(stdout);
        }
        CHECK(quit);
    } else CHECK(0);
    status=0;
done:
    g_clear_error(&e);g_free(range);g_free(reply_text);
    if(edit)g_object_unref(edit);
    if(text)g_object_unref(text);
    if(field)g_object_unref(field);
    if(app)g_object_unref(app);
    if(desktop)g_object_unref(desktop);
    if(initialized)atspi_exit();
    if(!status)puts("Original Editor independent AT-SPI phase passed.");
    return status;
}
